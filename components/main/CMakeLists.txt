set(MAIN_SOURCES main.c 
                 menu.c)

list(TRANSFORM MAIN_SOURCES PREPEND src/)

idf_component_register(SRCS ${MAIN_SOURCES}
                       INCLUDE_DIRS include
                       PRIV_REQUIRES bluetooth-lib oled-display encoder esp_adc audio-stream)

