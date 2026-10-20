cmake_minimum_required(VERSION 3.25)
project(GravastarLab VERSION 0.1.0 LANGUAGES CXX)

option(GRAVASTAR_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(gravastar_geometry src/geometry/geometry.cpp)
target_include_directories(gravastar_geometry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(gravastar_geometry PUBLIC cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gravastar_geometry PUBLIC
        -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    if(GRAVASTAR_SANITIZERS)
        target_compile_options(gravastar_geometry PUBLIC
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(gravastar_geometry PUBLIC -fsanitize=address,undefined)
    endif()
elseif(GRAVASTAR_SANITIZERS)
    message(FATAL_ERROR "GRAVASTAR_SANITIZERS requires GCC or Clang")
endif()

add_executable(gravastar_benchmark src/experiments/benchmark.cpp)
target_link_libraries(gravastar_benchmark PRIVATE gravastar_geometry)

include(CTest)
if(BUILD_TESTING)
    add_executable(gravastar_geometry_tests tests/cpp_geometry.cpp)
    target_link_libraries(gravastar_geometry_tests PRIVATE gravastar_geometry)
    add_test(NAME geometry_unit COMMAND gravastar_geometry_tests)
    add_test(NAME benchmark_help COMMAND gravastar_benchmark --help)
    add_test(NAME benchmark_reject_axis COMMAND gravastar_benchmark
        evaluate toy 1 0.875 0.5 1 2 1.5 0)
    set_tests_properties(benchmark_reject_axis PROPERTIES WILL_FAIL TRUE)
endif()
