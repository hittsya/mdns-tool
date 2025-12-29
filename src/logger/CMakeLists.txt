add_library(MDNS_Logger)

target_sources(MDNS_Logger
        PUBLIC
            include/Logger.h
        PRIVATE
            private/Logger.cpp
)

target_include_directories(MDNS_Logger
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/private
)

set_target_properties(MDNS_Logger
        PROPERTIES
        CXX_VISIBILITY_PRESET     hidden
        VISIBILITY_INLINES_HIDDEN YES
)

target_link_libraries(MDNS_Logger
        PUBLIC
        spdlog::spdlog
)

add_library(MDNS::Logger ALIAS MDNS_Logger)
