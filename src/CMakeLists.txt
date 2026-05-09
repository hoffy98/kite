# Makefile - Kite library

zephyr_library_sources_ifdef(CONFIG_KITE_BYTE_STREAM ByteStream.cpp)

add_subdirectory_ifdef(CONFIG_KITE_HTTP HTTP)
add_subdirectory_ifdef(CONFIG_KITE_MQTT MQTT)
add_subdirectory_ifdef(CONFIG_KITE_SERIAL Serial)