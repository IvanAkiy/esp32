set(SOURCES ./Src/main.cpp)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS . Inc
                    REQUIRES esp_common
                             esp_event
                             driver
                    PRIV_REQUIRES mqtt)
