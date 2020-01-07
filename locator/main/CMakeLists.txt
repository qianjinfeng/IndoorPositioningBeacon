idf_component_register(SRCS "esp_eddystone_api.c" "station_example_main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES root_cert.pem)
