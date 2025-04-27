set(AUDIO_STREAM_SOURCES audio_stream.c)

list(TRANSFORM AUDIO_STREAM_SOURCES PREPEND src/)

idf_component_register(SRCS ${AUDIO_STREAM_SOURCES}
                       INCLUDE_DIRS include
                       REQUIRES esp_driver_i2s)

