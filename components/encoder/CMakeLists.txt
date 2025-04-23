set(ENCODER_SOURCES encoder.c)

list(TRANSFORM ENCODER_SOURCES PREPEND src/)

idf_component_register(SRCS ${ENCODER_SOURCES}
                       INCLUDE_DIRS include
                       REQUIRES esp_driver_gpio)

