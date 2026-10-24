cmake_minimum_required(VERSION 3.16)

project(prisma_batch_manager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

function(prisma_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(batch_core
    src/batch.cpp
    src/batch_repository.cpp
    src/command_parser.cpp
    src/console_log.cpp
    src/worker.cpp
)

target_include_directories(batch_core PUBLIC include)
target_link_libraries(batch_core PUBLIC Threads::Threads)
prisma_set_warnings(batch_core)

add_executable(batch_manager
    src/main.cpp
)
target_link_libraries(batch_manager PRIVATE batch_core)
prisma_set_warnings(batch_manager)

option(PRISMA_BUILD_TESTS "Build the unit tests" ON)

if(PRISMA_BUILD_TESTS)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    )
    # Use the same C runtime as the rest of the project on MSVC
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(batch_tests
        tests/batch_test.cpp
    )
    target_link_libraries(batch_tests PRIVATE batch_core GTest::gtest_main)
    prisma_set_warnings(batch_tests)

    include(GoogleTest)
    gtest_discover_tests(batch_tests)
endif()