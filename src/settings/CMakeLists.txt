add_library(MDNS_Settings)

target_sources(MDNS_Settings
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Settings.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/Settings.cpp
)

target_include_directories(MDNS_Settings
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private
)

set_target_properties(MDNS_Settings
    PROPERTIES
        CXX_VISIBILITY_PRESET     hidden
        VISIBILITY_INLINES_HIDDEN YES
)

target_link_libraries(MDNS_Settings
    PRIVATE
        MDNS::Logger
        imgui
)

add_library(MDNS::Settings ALIAS MDNS_Settings)
