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

find_package(nlohmann_json 3.12.0 EXACT REQUIRED)
add_executable(gravastar_validate src/experiments/validate.cpp)
target_link_libraries(gravastar_validate PRIVATE gravastar_geometry nlohmann_json::nlohmann_json)
target_compile_definitions(gravastar_validate PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(gravastar_tools tools/project.cpp)
target_link_libraries(gravastar_tools PRIVATE gravastar_geometry nlohmann_json::nlohmann_json)
target_compile_definitions(gravastar_tools PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

include(CTest)
if(BUILD_TESTING)
    add_executable(gravastar_validation_failures tests/validation_failures.cpp)
    target_link_libraries(gravastar_validation_failures PRIVATE gravastar_geometry nlohmann_json::nlohmann_json)
    add_test(NAME validation_failure_isolation COMMAND gravastar_validation_failures
        $<TARGET_FILE:gravastar_validate> ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(validation_failure_isolation PROPERTIES TIMEOUT 300)
    # Preserve the earlier API and its coverage while its owner reviews consolidation.
    add_library(gravastar_legacy_geometry STATIC src/geometry/metric.cpp src/geometry/curvature.cpp)
    target_link_libraries(gravastar_legacy_geometry PUBLIC gravastar_geometry)
    add_executable(gravastar_legacy_tests tests/geometry_tests.cpp)
    target_link_libraries(gravastar_legacy_tests PRIVATE gravastar_legacy_geometry)
    add_test(NAME legacy_geometry_coverage COMMAND gravastar_legacy_tests)
    add_test(NAME tools_migration COMMAND gravastar_tools self-test)
    add_test(NAME rotating_geometry COMMAND gravastar_validate
        --output results/local/ctest_rotating.json)
    set_tests_properties(rotating_geometry PROPERTIES TIMEOUT 300 LABELS scientific)
    add_executable(gravastar_reference_tests tests/reference_tests.cpp)
    target_link_libraries(gravastar_reference_tests PRIVATE gravastar_geometry)
    add_test(NAME reference_migration COMMAND gravastar_reference_tests)
    set_tests_properties(reference_migration PROPERTIES TIMEOUT 180)
    add_executable(gravastar_geometry_tests tests/cpp_geometry.cpp)
    target_link_libraries(gravastar_geometry_tests PRIVATE gravastar_geometry)
    add_test(NAME geometry_unit COMMAND gravastar_geometry_tests)
    add_test(NAME benchmark_help COMMAND gravastar_benchmark --help)
    add_test(NAME benchmark_reject_axis COMMAND gravastar_benchmark
        evaluate toy 1 0.875 0.5 1 2 1.5 0)
    set_tests_properties(benchmark_reject_axis PROPERTIES WILL_FAIL TRUE)
endif()
