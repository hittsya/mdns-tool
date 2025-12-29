add_library(MDNS_Engine)

target_sources(MDNS_Engine
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include/Application.h
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/private/Application.cpp
)

target_include_directories(MDNS_Engine
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/private
)

set_target_properties(MDNS_Engine
        PROPERTIES
            CXX_VISIBILITY_PRESET     hidden
            VISIBILITY_INLINES_HIDDEN YES
)

target_link_libraries(MDNS_Engine
        PUBLIC
            imgui
            glfw
            OpenGL::GL
            MDNS::Logger
            MDNS::Helper
)

add_library(MDNS::Engine ALIAS MDNS_Engine)
