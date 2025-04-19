set(BLUETOOTH_LIB_SOURCES bt_lib.c
                          utils.c)
list(TRANSFORM BLUETOOTH_LIB_SOURCES PREPEND src/)

idf_component_register(SRCS ${BLUETOOTH_LIB_SOURCES}
                       INCLUDE_DIRS include
                       REQUIRES bt dispatcher
                       PRIV_REQUIRES nvs_flash)

