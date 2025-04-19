
set(OLED_SCREEN_LIB_SOURCES display.c
                            display_construction.c)

list(TRANSFORM OLED_SCREEN_LIB_SOURCES PREPEND src/)

idf_component_register(SRCS ${OLED_SCREEN_LIB_SOURCES}
                       INCLUDE_DIRS include
                       REQUIRES esp_driver_i2c)

