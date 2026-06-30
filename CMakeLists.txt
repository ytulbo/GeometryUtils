cmake_minimum_required(VERSION 3.10.0)
project(geometry VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)

set(HAPPLY_LOCAL_DIR "${CMAKE_CURRENT_LIST_DIR}/../happly")
if(EXISTS "${HAPPLY_LOCAL_DIR}/happly.h")
    message(STATUS "Using local happly checkout: ${HAPPLY_LOCAL_DIR}")
    set(HAPPLY_SOURCE_DIR "${HAPPLY_LOCAL_DIR}")
else()
    message(STATUS "Local happly checkout not found; fetching happly")
    FetchContent_Declare(
        happly_fetch
        GIT_REPOSITORY https://github.com/nmwsharp/happly.git
        GIT_TAG 4b1f2d73b39c172fa8f1e451377f1d3151d9c51c
    )
    FetchContent_MakeAvailable(happly_fetch)
    set(HAPPLY_SOURCE_DIR "${happly_fetch_SOURCE_DIR}")
endif()

add_library(happly INTERFACE)
target_include_directories(happly INTERFACE "${HAPPLY_SOURCE_DIR}")
target_compile_features(happly INTERFACE cxx_std_14)

add_library(geometry_lib geometry.cpp)
target_include_directories(geometry_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(geometry_lib PUBLIC happly)
target_compile_features(geometry_lib PUBLIC cxx_std_20)

add_executable(geometry main.cpp)
target_link_libraries(geometry PRIVATE geometry_lib)

target_compile_features(geometry PRIVATE cxx_std_20)
include(CTest)
enable_testing()

add_executable(triangle_plane_tests tests/triangle_plane_tests.cpp)
target_link_libraries(triangle_plane_tests PRIVATE geometry_lib)
add_test(NAME triangle_plane_tests COMMAND triangle_plane_tests)
