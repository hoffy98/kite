/**
 * @file Client.cpp
 * @author hoffy98
 * @brief
 * @date 2026-05-07
 */
#include <Kite/MQTT/Client.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#include <cerrno>
#include <cstring>

LOG_MODULE_REGISTER(kite_mqtt_client, CONFIG_KITE_MQTT_LOG_LEVEL);

namespace Kite::MQTT::Client
{

// ---------------------------------------------------------------------------
// Internal types
// ---------------------------------------------------------------------------

struct Subscription
{
    char topic[CONFIG_KITE_MQTT_MAX_TOPIC_LEN + 1];
    MessageCallback callback;
    mqtt_qos qos;
    bool active;
};

// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------

// -- Configuration --
static char s_brokerHost[CONFIG_KITE_MQTT_MAX_BROKER_HOST_LEN + 1];
static uint16_t s_brokerPort;
static char s_clientId[CONFIG_KITE_MQTT_MAX_CLIENT_ID_LEN + 1];
static char s_username[CONFIG_KITE_MQTT_MAX_CREDENTIAL_LEN + 1];
static char s_password[CONFIG_KITE_MQTT_MAX_CREDENTIAL_LEN + 1];
static bool s_hasCredentials;

// -- Last Will --
static char s_willTopic[CONFIG_KITE_MQTT_MAX_TOPIC_LEN + 1];
static uint8_t s_willPayload[CONFIG_KITE_MQTT_MAX_WILL_PAYLOAD_LEN];
static uint32_t s_willPayloadLen;
static mqtt_qos s_willQos;
static bool s_willRetain;
static bool s_hasWill;

// Persistent will structs handed to the MQTT library – must remain valid
// for the lifetime of the connection.
static struct mqtt_topic s_willTopicStruct;
static struct mqtt_utf8 s_willMessageStruct;

// -- Zephyr MQTT objects --
static struct mqtt_client s_mqttClient;
static struct sockaddr_storage s_brokerAddr;
static uint8_t s_rxBuf[CONFIG_KITE_MQTT_RX_BUFFER_SIZE];
static uint8_t s_txBuf[CONFIG_KITE_MQTT_TX_BUFFER_SIZE];
static uint8_t s_payloadBuf[CONFIG_KITE_MQTT_PAYLOAD_BUFFER_SIZE];

// Persistent UTF-8 structs whose pointers are handed to the MQTT library.
// Must remain valid for the lifetime of a connection.
static struct mqtt_utf8 s_usernameUtf8;
static struct mqtt_utf8 s_passwordUtf8;

// -- Socket polling --
static struct zsock_pollfd s_pollFd;

// -- Runtime state --
static volatile bool s_connected;
static volatile bool s_running;
static uint16_t s_messageId;

// -- Subscriptions --
static Subscription s_subscriptions[CONFIG_KITE_MQTT_MAX_SUBSCRIPTIONS];
static struct k_mutex s_subscriptionMutex;

// Serialises all MQTT write operations (publish, subscribe, unsubscribe)
// so concurrent callers cannot interleave frames on the socket.
static struct k_mutex s_writeMutex;

// -- Thread --
K_THREAD_STACK_DEFINE(s_threadStack, CONFIG_KITE_MQTT_THREAD_STACK_SIZE);
static struct k_thread s_thread;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static uint16_t NextMessageId()
{
    // 0 is reserved; skip it.
    if (++s_messageId == 0)
    {
        s_messageId = 1;
    }
    return s_messageId;
}

/** MQTT wildcard topic matching.
 *  Supports '+' (single level) and '#' (multi-level, must be last segment). */
static bool TopicMatches(const char* filter, const char* topic)
{
    while (*filter && *topic)
    {
        if (*filter == '#')
        {
            return true;
        }
        if (*filter == '+')
        {
            // Consume one level in the incoming topic.
            while (*topic && *topic != '/')
            {
                ++topic;
            }
            ++filter;
        }
        else
        {
            if (*filter != *topic)
            {
                return false;
            }
            ++filter;
            ++topic;
        }
    }
    // A trailing '#' matches an empty remainder.
    if (*filter == '#')
    {
        return true;
    }
    return (*filter == '\0' && *topic == '\0');
}

/** Collect matching callbacks under mutex, then invoke them without the mutex
 *  so that callbacks are free to call Subscribe / Unsubscribe. */
static void DispatchMessage(const char* topic, const uint8_t* data, uint32_t len)
{
    MessageCallback callbacks[CONFIG_KITE_MQTT_MAX_SUBSCRIPTIONS];
    int count = 0;

    k_mutex_lock(&s_subscriptionMutex, K_FOREVER);
    for (int i = 0; i < CONFIG_KITE_MQTT_MAX_SUBSCRIPTIONS; ++i)
    {
        if (s_subscriptions[i].active && s_subscriptions[i].callback != nullptr &&
            TopicMatches(s_subscriptions[i].topic, topic))
        {
            callbacks[count++] = s_subscriptions[i].callback;
        }
    }
    k_mutex_unlock(&s_subscriptionMutex);

    // Wrap the scratch buffer as a non-owning, read-only ByteStream.
    // cast away const: ByteStream accepts uint8_t* but we expose it as const&.
    Kite::ByteStream payload(const_cast<uint8_t*>(data), len, len);

    for (int i = 0; i < count; ++i)
    {
        callbacks[i](topic, payload);
    }
}

static int ResolveBrokerAddress()
{
    struct zsock_addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct zsock_addrinfo* res = nullptr;
    int rc = zsock_getaddrinfo(s_brokerHost, nullptr, &hints, &res);
    if (rc != 0)
    {
        LOG_ERR("DNS lookup failed for '%s': %d", s_brokerHost, rc);
        return -ENOENT;
    }

    auto* addr4       = reinterpret_cast<struct sockaddr_in*>(&s_brokerAddr);
    addr4->sin_family = AF_INET;
    addr4->sin_port   = htons(s_brokerPort);
    addr4->sin_addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr)->sin_addr;

    zsock_freeaddrinfo(res);
    LOG_DBG("Resolved '%s' -> port %u", s_brokerHost, s_brokerPort);
    return 0;
}

/** Send MQTT SUBSCRIBE for every active slot. Called after a fresh CONNACK. */
static void ResubscribeAll()
{
    k_mutex_lock(&s_subscriptionMutex, K_FOREVER);

    for (int i = 0; i < CONFIG_KITE_MQTT_MAX_SUBSCRIPTIONS; ++i)
    {
        if (!s_subscriptions[i].active)
        {
            continue;
        }

        struct mqtt_topic mqttTopic{};
        mqttTopic.topic.utf8 =
            reinterpret_cast<const uint8_t*>(s_subscriptions[i].topic);
        mqttTopic.topic.size = strlen(s_subscriptions[i].topic);
        mqttTopic.qos        = s_subscriptions[i].qos;

        struct mqtt_subscription_list list{};
        list.list       = &mqttTopic;
        list.list_count = 1;
        list.message_id = NextMessageId();

        // s_writeMutex is NOT held here: ResubscribeAll is called from the
        // event handler which itself runs inside mqtt_input on the client
        // thread. Acquiring s_writeMutex here is safe because no other
        // mqtt_input call can be in flight simultaneously.
        k_mutex_lock(&s_writeMutex, K_FOREVER);
        int rc = mqtt_subscribe(&s_mqttClient, &list);
        k_mutex_unlock(&s_writeMutex);

        if (rc != 0)
        {
            LOG_WRN("Re-subscribe to '%s' failed: %d", s_subscriptions[i].topic, rc);
        }
        else
        {
            LOG_DBG("Re-subscribed to '%s'", s_subscriptions[i].topic);
        }
    }

    k_mutex_unlock(&s_subscriptionMutex);
}

// ---------------------------------------------------------------------------
// MQTT event handler (called on the client thread via mqtt_input)
// ---------------------------------------------------------------------------

static void MqttEventHandler(struct mqtt_client* client, const struct mqtt_evt* evt)
{
    int rc;

    switch (evt->type)
    {
    case MQTT_EVT_CONNACK:
        if (evt->result != 0)
        {
            LOG_ERR("CONNACK rejected, result: %d", evt->result);
            break;
        }
        LOG_INF("Connected to broker %s:%u", s_brokerHost, s_brokerPort);
        s_connected = true;
        ResubscribeAll();
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_INF("Disconnected from broker (result: %d)", evt->result);
        s_connected = false;
        break;

    case MQTT_EVT_PUBLISH:
    {
        const struct mqtt_publish_param& pub = evt->param.publish;
        uint32_t payloadLen                  = pub.message.payload.len;

        // topic.utf8 is NOT null-terminated; copy it to a local buffer.
        char topicBuf[CONFIG_KITE_MQTT_MAX_TOPIC_LEN + 1]{};
        uint32_t topicLen = pub.message.topic.topic.size;
        if (topicLen > CONFIG_KITE_MQTT_MAX_TOPIC_LEN)
        {
            topicLen = CONFIG_KITE_MQTT_MAX_TOPIC_LEN;
        }
        memcpy(topicBuf, pub.message.topic.topic.utf8, topicLen);

        LOG_DBG("PUBLISH '%s' payload=%u B QoS=%d", topicBuf, payloadLen,
                pub.message.topic.qos);

        uint32_t readLen = payloadLen;
        if (readLen > CONFIG_KITE_MQTT_PAYLOAD_BUFFER_SIZE)
        {
            LOG_WRN("Payload truncated %u -> %d bytes", payloadLen,
                    CONFIG_KITE_MQTT_PAYLOAD_BUFFER_SIZE);
            readLen = CONFIG_KITE_MQTT_PAYLOAD_BUFFER_SIZE;
        }

        rc = mqtt_readall_publish_payload(client, s_payloadBuf, readLen);
        if (rc != 0)
        {
            LOG_ERR("mqtt_readall_publish_payload failed: %d", rc);
        }
        else
        {
            DispatchMessage(topicBuf, s_payloadBuf, readLen);
        }

        // QoS acknowledgements
        if (pub.message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE)
        {
            struct mqtt_puback_param ack{ .message_id = pub.message_id };
            rc = mqtt_publish_qos1_ack(client, &ack);
            if (rc != 0)
            {
                LOG_ERR("PUBACK failed: %d", rc);
            }
        }
        else if (pub.message.topic.qos == MQTT_QOS_2_EXACTLY_ONCE)
        {
            struct mqtt_pubrec_param rec{ .message_id = pub.message_id };
            rc = mqtt_publish_qos2_receive(client, &rec);
            if (rc != 0)
            {
                LOG_ERR("PUBREC failed: %d", rc);
            }
        }
        break;
    }

    case MQTT_EVT_PUBACK:
        if (evt->result != 0)
        {
            LOG_ERR("PUBACK error: %d", evt->result);
        }
        else
        {
            LOG_DBG("PUBACK msg_id=%u", evt->param.puback.message_id);
        }
        break;

    case MQTT_EVT_PUBREC:
        if (evt->result != 0)
        {
            LOG_ERR("PUBREC error: %d", evt->result);
        }
        else
        {
            LOG_DBG("PUBREC msg_id=%u, sending PUBREL", evt->param.pubrec.message_id);
            struct mqtt_pubrel_param rel{ .message_id = evt->param.pubrec.message_id };
            rc = mqtt_publish_qos2_release(client, &rel);
            if (rc != 0)
            {
                LOG_ERR("PUBREL failed: %d", rc);
            }
        }
        break;

    case MQTT_EVT_PUBREL:
        if (evt->result != 0)
        {
            LOG_ERR("PUBREL error: %d", evt->result);
        }
        else
        {
            LOG_DBG("PUBREL msg_id=%u, sending PUBCOMP", evt->param.pubrel.message_id);
            struct mqtt_pubcomp_param comp{ .message_id = evt->param.pubrel.message_id };
            rc = mqtt_publish_qos2_complete(client, &comp);
            if (rc != 0)
            {
                LOG_ERR("PUBCOMP failed: %d", rc);
            }
        }
        break;

    case MQTT_EVT_PUBCOMP:
        LOG_DBG("PUBCOMP msg_id=%u", evt->param.pubcomp.message_id);
        break;

    case MQTT_EVT_SUBACK:
        LOG_DBG("SUBACK msg_id=%u", evt->param.suback.message_id);
        break;

    case MQTT_EVT_UNSUBACK:
        LOG_DBG("UNSUBACK msg_id=%u", evt->param.unsuback.message_id);
        break;

    case MQTT_EVT_PINGRESP:
        LOG_DBG("PINGRESP received");
        break;

    default:
        LOG_WRN("Unhandled MQTT event type: %d", evt->type);
        break;
    }
}

// ---------------------------------------------------------------------------
// Connection helper
// ---------------------------------------------------------------------------

static int ConnectToBroker()
{
    int rc = ResolveBrokerAddress();
    if (rc != 0)
    {
        return rc;
    }

    mqtt_client_init(&s_mqttClient);

    s_mqttClient.broker         = &s_brokerAddr;
    s_mqttClient.evt_cb         = MqttEventHandler;
    s_mqttClient.client_id.utf8 = reinterpret_cast<const uint8_t*>(s_clientId);
    s_mqttClient.client_id.size = strlen(s_clientId);
    s_mqttClient.protocol_version = MQTT_VERSION_3_1_1;
    s_mqttClient.clean_session    = 1;
    s_mqttClient.rx_buf           = s_rxBuf;
    s_mqttClient.rx_buf_size      = sizeof(s_rxBuf);
    s_mqttClient.tx_buf           = s_txBuf;
    s_mqttClient.tx_buf_size      = sizeof(s_txBuf);
    s_mqttClient.transport.type   = MQTT_TRANSPORT_NON_SECURE;

    if (s_hasWill)
    {
        s_willTopicStruct.topic.utf8 = reinterpret_cast<const uint8_t*>(s_willTopic);
        s_willTopicStruct.topic.size = strlen(s_willTopic);
        s_willTopicStruct.qos        = s_willQos;
        s_willMessageStruct.utf8     = s_willPayload;
        s_willMessageStruct.size     = s_willPayloadLen;
        s_mqttClient.will_topic      = &s_willTopicStruct;
        s_mqttClient.will_message    = &s_willMessageStruct;
        s_mqttClient.will_retain     = s_willRetain ? 1 : 0;
    }
    else
    {
        s_mqttClient.will_topic   = nullptr;
        s_mqttClient.will_message = nullptr;
        s_mqttClient.will_retain  = 0;
    }

    if (s_hasCredentials)
    {
        s_usernameUtf8 = { reinterpret_cast<const uint8_t*>(s_username), strlen(s_username) };
        s_passwordUtf8 = { reinterpret_cast<const uint8_t*>(s_password), strlen(s_password) };
        s_mqttClient.user_name = &s_usernameUtf8;
        s_mqttClient.password  = &s_passwordUtf8;
    }
    else
    {
        s_mqttClient.user_name = nullptr;
        s_mqttClient.password  = nullptr;
    }

    rc = mqtt_connect(&s_mqttClient);
    if (rc != 0)
    {
        LOG_ERR("mqtt_connect failed: %d", rc);
        return rc;
    }

    s_pollFd.fd     = s_mqttClient.transport.tcp.sock;
    s_pollFd.events = ZSOCK_POLLIN;

    // Wait for CONNACK.
    int ready = zsock_poll(&s_pollFd, 1, CONFIG_KITE_MQTT_CONNECT_TIMEOUT_MS);
    if (ready <= 0)
    {
        LOG_ERR("Timed out waiting for CONNACK");
        mqtt_abort(&s_mqttClient);
        return -ETIMEDOUT;
    }

    rc = mqtt_input(&s_mqttClient);
    if (rc != 0)
    {
        LOG_ERR("mqtt_input after connect failed: %d", rc);
        mqtt_abort(&s_mqttClient);
        return rc;
    }

    if (!s_connected)
    {
        LOG_ERR("Broker refused connection");
        mqtt_abort(&s_mqttClient);
        return -ECONNREFUSED;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Client thread
// ---------------------------------------------------------------------------

static void ClientThread(void*, void*, void*)
{
    LOG_INF("MQTT client thread started");

    while (s_running)
    {
        if (!s_connected)
        {
            int retries = 0;
            while (s_running && !s_connected)
            {
                if (retries >= CONFIG_KITE_MQTT_CONNECT_MAX_RETRIES)
                {
                    LOG_ERR("Exhausted %d connect attempts, "
                            "waiting %d ms before retrying",
                            CONFIG_KITE_MQTT_CONNECT_MAX_RETRIES,
                            CONFIG_KITE_MQTT_RECONNECT_DELAY_MS);
                    k_msleep(CONFIG_KITE_MQTT_RECONNECT_DELAY_MS);
                    retries = 0;
                }

                LOG_INF("Connecting to %s:%u (attempt %d/%d)", s_brokerHost, s_brokerPort,
                        retries + 1, CONFIG_KITE_MQTT_CONNECT_MAX_RETRIES);

                int rc = ConnectToBroker();
                if (rc == 0)
                {
                    break;
                }

                ++retries;
                LOG_WRN("Connection attempt %d failed (%d), "
                        "retrying in %d ms",
                        retries, rc, CONFIG_KITE_MQTT_RECONNECT_DELAY_MS);
                k_msleep(CONFIG_KITE_MQTT_RECONNECT_DELAY_MS);
            }

            if (!s_connected)
            {
                // s_running became false during the connect loop.
                break;
            }
        }

        // Event loop: poll the socket and drive the MQTT state machine.
        int keepaliveMs = mqtt_keepalive_time_left(&s_mqttClient);
        int pollTimeout =
            (keepaliveMs < 0 || keepaliveMs > CONFIG_KITE_MQTT_POLL_TIMEOUT_MS) ?
            CONFIG_KITE_MQTT_POLL_TIMEOUT_MS :
            keepaliveMs;

        int ready = zsock_poll(&s_pollFd, 1, pollTimeout);
        if (ready < 0)
        {
            LOG_ERR("poll error: %d", errno);
            s_connected = false;
            continue;
        }

        if (ready > 0 && (s_pollFd.revents & ZSOCK_POLLIN))
        {
            int rc = mqtt_input(&s_mqttClient);
            if (rc != 0)
            {
                LOG_ERR("mqtt_input error: %d", rc);
                s_connected = false;
                continue;
            }
        }

        int rc = mqtt_live(&s_mqttClient);
        if (rc != 0 && rc != -EAGAIN)
        {
            LOG_ERR("mqtt_live error: %d", rc);
            s_connected = false;
        }
    }

    if (s_connected)
    {
        mqtt_disconnect(&s_mqttClient, nullptr);
        s_connected = false;
    }

    LOG_INF("MQTT client thread stopped");
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int SetBroker(const char* host, uint16_t port)
{
    if (host == nullptr || host[0] == '\0' || port == 0)
    {
        return -EINVAL;
    }
    if (strlen(host) > CONFIG_KITE_MQTT_MAX_BROKER_HOST_LEN)
    {
        LOG_ERR("Broker host too long (max %d chars)", CONFIG_KITE_MQTT_MAX_BROKER_HOST_LEN);
        return -EINVAL;
    }

    strncpy(s_brokerHost, host, CONFIG_KITE_MQTT_MAX_BROKER_HOST_LEN);
    s_brokerHost[CONFIG_KITE_MQTT_MAX_BROKER_HOST_LEN] = '\0';
    s_brokerPort                                       = port;

    LOG_DBG("Broker set to %s:%u", s_brokerHost, s_brokerPort);
    return 0;
}

int SetClientId(const char* clientId)
{
    if (clientId == nullptr || clientId[0] == '\0')
    {
        return -EINVAL;
    }
    if (strlen(clientId) > CONFIG_KITE_MQTT_MAX_CLIENT_ID_LEN)
    {
        LOG_ERR("Client ID too long (max %d chars)", CONFIG_KITE_MQTT_MAX_CLIENT_ID_LEN);
        return -EINVAL;
    }

    strncpy(s_clientId, clientId, CONFIG_KITE_MQTT_MAX_CLIENT_ID_LEN);
    s_clientId[CONFIG_KITE_MQTT_MAX_CLIENT_ID_LEN] = '\0';

    LOG_DBG("Client ID set to '%s'", s_clientId);
    return 0;
}

int SetCredentials(const char* username, const char* password)
{
    if (username == nullptr && password == nullptr)
    {
        s_hasCredentials = false;
        memset(s_username, 0, sizeof(s_username));
        memset(s_password, 0, sizeof(s_password));
        LOG_DBG("Credentials cleared");
        return 0;
    }

    if (username == nullptr || password == nullptr)
    {
        return -EINVAL;
    }
    if (strlen(username) > CONFIG_KITE_MQTT_MAX_CREDENTIAL_LEN ||
        strlen(password) > CONFIG_KITE_MQTT_MAX_CREDENTIAL_LEN)
    {
        LOG_ERR("Credentials too long (max %d chars)", CONFIG_KITE_MQTT_MAX_CREDENTIAL_LEN);
        return -EINVAL;
    }

    strncpy(s_username, username, CONFIG_KITE_MQTT_MAX_CREDENTIAL_LEN);
    s_username[CONFIG_KITE_MQTT_MAX_CREDENTIAL_LEN] = '\0';
    strncpy(s_password, password, CONFIG_KITE_MQTT_MAX_CREDENTIAL_LEN);
    s_password[CONFIG_KITE_MQTT_MAX_CREDENTIAL_LEN] = '\0';
    s_hasCredentials                                = true;

    LOG_DBG("Credentials set for user '%s'", s_username);
    return 0;
}

int SetLastWill(const char* topic, const uint8_t* payload, size_t payloadLen, mqtt_qos qos, bool retain)
{
    if (topic == nullptr || topic[0] == '\0')
    {
        return -EINVAL;
    }
    if (strlen(topic) > CONFIG_KITE_MQTT_MAX_TOPIC_LEN)
    {
        LOG_ERR("Will topic too long (max %d chars)", CONFIG_KITE_MQTT_MAX_TOPIC_LEN);
        return -EINVAL;
    }
    if (payload == nullptr && payloadLen > 0)
    {
        return -EINVAL;
    }
    if (payloadLen > CONFIG_KITE_MQTT_MAX_WILL_PAYLOAD_LEN)
    {
        LOG_ERR("Will payload too long (max %d bytes)", CONFIG_KITE_MQTT_MAX_WILL_PAYLOAD_LEN);
        return -EINVAL;
    }

    strncpy(s_willTopic, topic, CONFIG_KITE_MQTT_MAX_TOPIC_LEN);
    s_willTopic[CONFIG_KITE_MQTT_MAX_TOPIC_LEN] = '\0';

    if (payload != nullptr && payloadLen > 0)
    {
        memcpy(s_willPayload, payload, payloadLen);
    }
    s_willPayloadLen = static_cast<uint32_t>(payloadLen);
    s_willQos        = qos;
    s_willRetain     = retain;
    s_hasWill        = true;

    LOG_DBG("Last will set: topic='%s' payload=%u B QoS=%d retain=%d",
            s_willTopic, s_willPayloadLen, qos, retain ? 1 : 0);
    return 0;
}

int Init()
{
    if (s_running)
    {
        LOG_WRN("Init called but client is already running");
        return -EALREADY;
    }
    if (s_brokerHost[0] == '\0')
    {
        LOG_ERR("Broker not configured; call SetBroker() first");
        return -EINVAL;
    }
    if (s_clientId[0] == '\0')
    {
        LOG_ERR("Client ID not configured; call SetClientId() first");
        return -EINVAL;
    }

    k_mutex_init(&s_writeMutex);
    k_mutex_init(&s_subscriptionMutex);

    s_connected = false;
    s_running   = true;
    s_messageId = 0;

    k_tid_t tid = k_thread_create(&s_thread, s_threadStack,
                                  K_THREAD_STACK_SIZEOF(s_threadStack),
                                  ClientThread, nullptr, nullptr, nullptr,
                                  CONFIG_KITE_MQTT_THREAD_PRIORITY, 0, K_NO_WAIT);

    if (tid == nullptr)
    {
        LOG_ERR("Failed to create client thread");
        s_running = false;
        return -ENOMEM;
    }

    k_thread_name_set(tid, "mqtt_client");

    LOG_INF("MQTT client initialised (broker: %s:%u)", s_brokerHost, s_brokerPort);
    return 0;
}

int Disconnect()
{
    if (!s_running)
    {
        return -EALREADY;
    }

    LOG_INF("Disconnecting MQTT client");

    s_running = false;

    // Abort the socket to unblock any blocking poll() / read inside the thread.
    if (s_connected)
    {
        mqtt_abort(&s_mqttClient);
    }

    k_thread_join(&s_thread, K_MSEC(CONFIG_KITE_MQTT_CONNECT_TIMEOUT_MS));

    LOG_INF("MQTT client stopped");
    return 0;
}

bool IsConnected()
{
    return s_connected;
}

int Publish(const char* topic, const Kite::ByteStream& payload, mqtt_qos qos, bool retain)
{
    if (topic == nullptr || topic[0] == '\0')
    {
        return -EINVAL;
    }
    if (!s_connected)
    {
        LOG_WRN("Publish skipped: not connected");
        return -ENOTCONN;
    }

    struct mqtt_publish_param param{};
    param.message.topic.topic.utf8 = reinterpret_cast<const uint8_t*>(topic);
    param.message.topic.topic.size = strlen(topic);
    param.message.topic.qos        = qos;
    param.message.payload.data     = const_cast<uint8_t*>(payload.GetData());
    param.message.payload.len      = static_cast<uint32_t>(payload.GetSize());
    // QoS 0 does not use message IDs; an ID of 0 is fine for the library.
    param.message_id  = (qos != MQTT_QOS_0_AT_MOST_ONCE) ? NextMessageId() : 0;
    param.dup_flag    = 0;
    param.retain_flag = retain ? 1 : 0;

    k_mutex_lock(&s_writeMutex, K_FOREVER);
    int rc = mqtt_publish(&s_mqttClient, &param);
    k_mutex_unlock(&s_writeMutex);

    if (rc != 0)
    {
        LOG_ERR("Publish to '%s' failed: %d", topic, rc);
    }
    else
    {
        LOG_DBG("Published %zu B to '%s' (QoS %d)", payload.GetSize(), topic, qos);
    }

    return rc;
}

int Subscribe(const char* topic, MessageCallback callback, mqtt_qos qos)
{
    if (topic == nullptr || topic[0] == '\0' || callback == nullptr)
    {
        return -EINVAL;
    }
    if (strlen(topic) > CONFIG_KITE_MQTT_MAX_TOPIC_LEN)
    {
        LOG_ERR("Topic too long (max %d chars)", CONFIG_KITE_MQTT_MAX_TOPIC_LEN);
        return -EINVAL;
    }

    k_mutex_lock(&s_subscriptionMutex, K_FOREVER);

    // Look for an existing slot for this topic (to replace), or a free slot.
    int slot     = -1;
    bool replace = false;
    for (int i = 0; i < CONFIG_KITE_MQTT_MAX_SUBSCRIPTIONS; ++i)
    {
        if (s_subscriptions[i].active && strcmp(s_subscriptions[i].topic, topic) == 0)
        {
            slot    = i;
            replace = true;
            break;
        }
        if (slot < 0 && !s_subscriptions[i].active)
        {
            slot = i;
        }
    }

    if (slot < 0)
    {
        k_mutex_unlock(&s_subscriptionMutex);
        LOG_ERR("Subscription table full (max %d entries)", CONFIG_KITE_MQTT_MAX_SUBSCRIPTIONS);
        return -ENOMEM;
    }

    strncpy(s_subscriptions[slot].topic, topic, CONFIG_KITE_MQTT_MAX_TOPIC_LEN);
    s_subscriptions[slot].topic[CONFIG_KITE_MQTT_MAX_TOPIC_LEN] = '\0';
    s_subscriptions[slot].callback                              = callback;
    s_subscriptions[slot].qos                                   = qos;
    s_subscriptions[slot].active                                = true;

    k_mutex_unlock(&s_subscriptionMutex);

    LOG_DBG("%s subscription for '%s' (QoS %d)", replace ? "Updated" : "Added", topic, qos);

    if (s_connected)
    {
        struct mqtt_topic mqttTopic{};
        mqttTopic.topic.utf8 = reinterpret_cast<const uint8_t*>(topic);
        mqttTopic.topic.size = strlen(topic);
        mqttTopic.qos        = qos;

        struct mqtt_subscription_list list{};
        list.list       = &mqttTopic;
        list.list_count = 1;
        list.message_id = NextMessageId();

        k_mutex_lock(&s_writeMutex, K_FOREVER);
        int rc = mqtt_subscribe(&s_mqttClient, &list);
        k_mutex_unlock(&s_writeMutex);

        if (rc != 0)
        {
            LOG_ERR("SUBSCRIBE to '%s' failed: %d", topic, rc);
            return rc;
        }
        LOG_INF("Subscribed to '%s'", topic);
    }
    else
    {
        LOG_DBG("Subscription '%s' queued for next connection", topic);
    }

    return 0;
}

int Unsubscribe(const char* topic)
{
    if (topic == nullptr || topic[0] == '\0')
    {
        return -EINVAL;
    }

    k_mutex_lock(&s_subscriptionMutex, K_FOREVER);

    int slot = -1;
    for (int i = 0; i < CONFIG_KITE_MQTT_MAX_SUBSCRIPTIONS; ++i)
    {
        if (s_subscriptions[i].active && strcmp(s_subscriptions[i].topic, topic) == 0)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        k_mutex_unlock(&s_subscriptionMutex);
        LOG_WRN("Unsubscribe: topic '%s' not found", topic);
        return -ENOENT;
    }

    s_subscriptions[slot].active   = false;
    s_subscriptions[slot].callback = nullptr;
    memset(s_subscriptions[slot].topic, 0, sizeof(s_subscriptions[slot].topic));

    k_mutex_unlock(&s_subscriptionMutex);

    LOG_DBG("Removed local subscription for '%s'", topic);

    if (s_connected)
    {
        struct mqtt_topic mqttTopic{};
        mqttTopic.topic.utf8 = reinterpret_cast<const uint8_t*>(topic);
        mqttTopic.topic.size = strlen(topic);

        struct mqtt_subscription_list list{};
        list.list       = &mqttTopic;
        list.list_count = 1;
        list.message_id = NextMessageId();

        k_mutex_lock(&s_writeMutex, K_FOREVER);
        int rc = mqtt_unsubscribe(&s_mqttClient, &list);
        k_mutex_unlock(&s_writeMutex);

        if (rc != 0)
        {
            LOG_ERR("UNSUBSCRIBE from '%s' failed: %d", topic, rc);
            return rc;
        }
        LOG_INF("Unsubscribed from '%s'", topic);
    }

    return 0;
}

} // namespace Kite::MQTT::Client