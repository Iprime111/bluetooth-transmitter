cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bluetooth-transmitter)

# Generate clangd configuration file with respect to xtensa c/c++ standard libs

execute_process(
    COMMAND bash -c "which ${CMAKE_CXX_COMPILER}"
    OUTPUT_VARIABLE BL_TRANSMITTER_XTENSA_PATH_
)

get_filename_component(BL_TRANSMITTER_XTENSA_DIR_ ${BL_TRANSMITTER_XTENSA_PATH_} DIRECTORY)
set(BL_TRANSMITTER_XTENSA_LIB_PATH_ ${BL_TRANSMITTER_XTENSA_DIR_}/..)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.clangd.in ${CMAKE_CURRENT_SOURCE_DIR}/.clangd)
