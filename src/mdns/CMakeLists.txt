add_library(MDNS_Helper)

target_sources(MDNS_Helper
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/MdnsHelper.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/MdnsHelper.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/private/MdnsLinuxImpl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/private/MdnsImpl.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/private/MdnsWindowsImpl.cpp
)

if (WIN32)
    target_link_libraries(mdns_listener PRIVATE ws2_32 iphlpapi dwmapi)
    add_compile_options(/EHsc)
endif()

target_include_directories(MDNS_Helper
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private
)

set_target_properties(MDNS_Helper
    PROPERTIES
        CXX_VISIBILITY_PRESET     hidden
        VISIBILITY_INLINES_HIDDEN YES
)

target_link_libraries(MDNS_Helper
    PRIVATE
        MDNS::Logger
)

add_library(MDNS::Helper ALIAS MDNS_Helper)
