add_executable(
    mdns_listener
        main.cpp
)

add_subdirectory(logger)
add_subdirectory(mdns)
add_subdirectory(engine)

target_link_libraries(mdns_listener PRIVATE MDNS::Engine)
target_compile_features(mdns_listener PRIVATE cxx_std_20)
