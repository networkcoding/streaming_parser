cmake_minimum_required(VERSION 3.16)
project(streaming_parser CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(TARGET_OS "${CMAKE_SYSTEM_NAME}")

message(
  STATUS "============== ${PROJECT_NAME} Project Configuration ============== ")
message(STATUS "System      : ${CMAKE_SYSTEM_NAME}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Compiler ABI: ${CMAKE_CXX_COMPILER_ABI}")
message(STATUS "Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler:   ${CMAKE_C_COMPILER}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "============================================================ ")

set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckCXXCompilerFlag)
if(NOT MSVC)
  check_cxx_compiler_flag("-std=c++17" COMPILER_SUPPORTS_CXX17)
else()
  check_cxx_compiler_flag("/std=c++17" COMPILER_SUPPORTS_CXX17)
endif()
if(COMPILER_SUPPORTS_CXX17)
  message(STATUS "c++17 is supported by the compiler ${CMAKE_CXX_COMPILER}")
else()
  message(FATAL_ERROR "
            FATAL: the compiler ${CMAKE_CXX_COMPILER} does not support c++17")
endif()

message(STATUS "Building googletest from source ${CMAKE_SOURCE_DIR}/googletest")
include(FetchContent)
FetchContent_Declare(googletest SOURCE SOURCE_DIR
                                ${CMAKE_SOURCE_DIR}/googletest)
FetchContent_MakeAvailable(googletest)
include_directories(${googletest_SOURCE_DIR}/googletest/include)

include(GoogleTest)
enable_testing()
add_custom_target(all_test ${CMAKE_CTEST_COMMAND} -V)

set(CTEST_OUTPUT_ON_FAILURE true)
set(GTEST_COLOR true)

# ring_buffer_test
add_executable(ring_buffer_test src/ring_buffer_test.cc src/ring_buffer.cc)
target_link_libraries(ring_buffer_test gtest_main)
gtest_discover_tests(ring_buffer_test)

# streaming_parser_test
add_executable(streaming_parser_test src/streaming_parser_test.cc
                                     src/ring_buffer.cc)
target_link_libraries(streaming_parser_test gtest_main)
gtest_discover_tests(streaming_parser_test)
