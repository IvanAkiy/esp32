set(SOURCES ./Src/main.c
            ./Src/pn532.c)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS ./Inc
                    REQUIRES esp_common
                             esp_event
                             driver
                    PRIV_REQUIRES mqtt nvs_flash esp_wifi)
