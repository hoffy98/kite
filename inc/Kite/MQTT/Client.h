/**
 * @file Client.h
 * @author hoffy98
 * @brief Simplified C++ MQTT 3.1.1 client API wrapping Zephyr's mqtt library.
 *        All configuration must be applied before calling Init().
 *        Thread-safe publishing and subscription are handled internally.
 * @date 2026-05-07
 */
#pragma once
#include <Kite/ByteStream.h>
#include <zephyr/net/mqtt.h>

#include <cerrno>
#include <cstdint>

namespace Kite::MQTT::Client
{

/**
 * @brief Callback invoked on the client thread when a message arrives on a
 *        subscribed topic.
 *
 * @param topic   Null-terminated topic string.
 * @param payload Read-only view of the received payload bytes.
 */
using MessageCallback = void (*)(const char* topic, const Kite::ByteStream& payload);

// ---------------------------------------------------------------------------
// Configuration – must be called before Init()
// ---------------------------------------------------------------------------

/**
 * @brief Set the MQTT broker address and port.
 *
 * @param host  Broker hostname or IPv4/IPv6 address string.
 * @param port  TCP port (typically 1883).
 * @return 0 on success, negative errno on failure (e.g. -EINVAL).
 */
int SetBroker(const char* host, uint16_t port);

/**
 * @brief Set the MQTT client identifier.
 *
 * @param clientId Null-terminated client ID string.
 * @return 0 on success, negative errno on failure.
 */
int SetClientId(const char* clientId);

/**
 * @brief Set optional broker credentials.
 *        Pass nullptr for both parameters to connect without authentication.
 *
 * @param username Null-terminated username, or nullptr.
 * @param password Null-terminated password, or nullptr.
 * @return 0 on success, negative errno on failure.
 */
int SetCredentials(const char* username, const char* password);

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

/**
 * @brief Initialise and start the internal client thread.
 *        The thread establishes the broker connection and drives the MQTT
 *        event loop (keep-alive, inbound message dispatch, QoS handshakes).
 *
 * @return 0 on success, negative errno on failure.
 */
int Init();

/**
 * @brief Send a DISCONNECT packet and stop the client thread.
 *
 * @return 0 on success, negative errno on failure.
 */
int Disconnect();

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

/**
 * @brief Query whether the client currently has an active broker connection.
 *
 * @return true if connected, false otherwise.
 */
bool IsConnected();

// ---------------------------------------------------------------------------
// Publish
// ---------------------------------------------------------------------------

/**
 * @brief Publish a message to a topic.
 *
 * @param topic   Null-terminated topic string.
 * @param payload Payload bytes to publish.
 * @param qos     QoS level (default: MQTT_QOS_0_AT_MOST_ONCE).
 * @return 0 on success, negative errno on failure (e.g. -ENOTCONN if not connected).
 */
int Publish(const char* topic, const Kite::ByteStream& payload, mqtt_qos qos = MQTT_QOS_0_AT_MOST_ONCE);

/**
 * @brief Publish a null-terminated string message to a topic.
 *
 * @param topic   Null-terminated topic string.
 * @param payload Null-terminated string payload.
 * @param qos     QoS level (default: MQTT_QOS_0_AT_MOST_ONCE).
 * @return 0 on success, negative errno on failure (e.g. -ENOTCONN if not connected).
 */
inline int Publish(const char* topic, const char* payload, mqtt_qos qos = MQTT_QOS_0_AT_MOST_ONCE)
{
    Kite::ByteStream stream(reinterpret_cast<uint8_t*>(const_cast<char*>(payload)),
                            strlen(payload), strlen(payload));
    return Publish(topic, stream, qos);
}

// ---------------------------------------------------------------------------
// Subscribe / Unsubscribe
// ---------------------------------------------------------------------------

/**
 * @brief Subscribe to a topic and register a callback for inbound messages.
 *        Subscribing to the same topic a second time replaces the callback.
 *
 * @param topic    Null-terminated topic filter (wildcards + / supported).
 * @param callback Function invoked on the client thread for each matching message.
 * @param qos      Maximum QoS level requested from the broker
 *                 (default: MQTT_QOS_0_AT_MOST_ONCE).
 * @return 0 on success, negative errno on failure.
 */
int Subscribe(const char* topic, MessageCallback callback, mqtt_qos qos = MQTT_QOS_0_AT_MOST_ONCE);

/**
 * @brief Unsubscribe from a topic and remove the associated callback.
 *
 * @param topic Null-terminated topic filter to unsubscribe from.
 * @return 0 on success, negative errno on failure (e.g. -ENOENT if not subscribed).
 */
int Unsubscribe(const char* topic);

} // namespace Kite::MQTT::Client