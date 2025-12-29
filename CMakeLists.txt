cmake_minimum_required(VERSION 3.20.0)
project(mdns_listener)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD   11)

include(cmake/FetchCPM.cmake)
include(cmake/FetchSPDLOG.cmake)
include(cmake/FetchGLFW.cmake)
include(cmake/FetchImGUI.cmake)

find_package(OpenGL REQUIRED)

add_subdirectory(src)
