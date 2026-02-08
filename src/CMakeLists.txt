add_executable(
    mdns_listener
    main.cpp
)

add_subdirectory(logger)
add_subdirectory(settings)
add_subdirectory(mdns)
add_subdirectory(stb)
add_subdirectory(engine)

target_link_libraries  (mdns_listener PRIVATE MDNS::Engine)
target_compile_features(mdns_listener PRIVATE cxx_std_20)

if (WIN32)
    target_link_libraries(mdns_listener PRIVATE ws2_32 iphlpapi)
    add_compile_options(/EHsc)
endif()

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_SHORT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND ${GIT_EXECUTABLE} diff --quiet
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE GIT_DIRTY_RESULT
    )

    if(GIT_DIRTY_RESULT EQUAL 0)
        set(GIT_DIRTY 0)
    else()
        set(GIT_DIRTY 1)    
    endif()
else()
    set(GIT_COMMIT_HASH "unknown")
    set(GIT_COMMIT_SHORT "unknown")
    set(GIT_BRANCH "unknown")
    set(GIT_DIRTY 0)
endif()

add_compile_definitions(
    GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
    GIT_COMMIT_SHORT="${GIT_COMMIT_SHORT}"
    GIT_BRANCH="${GIT_BRANCH}"
    GIT_DIRTY=${GIT_DIRTY}
)
