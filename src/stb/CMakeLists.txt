add_library(STB INTERFACE)

target_sources(STB
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/stb_image.h
)
