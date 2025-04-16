set(DISPATCHER_SOURCES dispatcher.c)

list(TRANSFORM DISPATCHER_SOURCES PREPEND src/)

idf_component_register(SRCS ${DISPATCHER_SOURCES}
                       INCLUDE_DIRS include)

