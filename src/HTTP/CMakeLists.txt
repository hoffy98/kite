zephyr_library_sources(
    Client.cpp
    Server.cpp
    Server/Endpoint.cpp
)

## Linker settings for Kite::HTTP::Server
zephyr_linker_sources(SECTIONS sections-rom.ld)

zephyr_linker_section(NAME http_resource_desc_http_service_descriptor
                      KVMA RAM_REGION GROUP RODATA_REGION
                      SUBALIGN ${CONFIG_LINKER_ITERABLE_SUBALIGN})