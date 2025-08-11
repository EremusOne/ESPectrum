idf_component_register(SRC_DIRS src
                                src/comdrivers
                                src/devdrivers
                       INCLUDE_DIRS src
                                src/comdrivers
                                src/devdrivers
                       REQUIRES fatfs
                                spiffs
                                esp_adc_cal
                                ulp
                                nvs_flash
                                soc)