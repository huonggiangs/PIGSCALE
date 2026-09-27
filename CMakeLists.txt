cmake_minimum_required(VERSION 3.16)

# ESP32-P4 Excavator Scale V3
set(EXTRA_COMPONENT_DIRS
    "$ENV{IDF_PATH}/components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(mayxuc_v3)
