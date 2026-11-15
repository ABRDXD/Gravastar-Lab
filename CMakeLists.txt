cmake_minimum_required(VERSION 3.25)
project(GravastarLab VERSION 0.1.0 LANGUAGES CXX)

option(GRAVASTAR_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(gravastar_geometry src/geometry/geometry.cpp)
target_include_directories(gravastar_geometry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(gravastar_geometry PUBLIC cxx_std_20)

add_library(gravastar_integration
    src/integration/integrator.cpp
    src/integration/critical_impact.cpp
)
target_link_libraries(gravastar_integration PUBLIC gravastar_geometry)
target_include_directories(gravastar_integration PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(gravastar_integration PUBLIC cxx_std_20)

add_library(gravastar_optics src/optics/camera.cpp src/optics/renderer.cpp)
target_link_libraries(gravastar_optics PUBLIC gravastar_integration gravastar_geometry)
target_include_directories(gravastar_optics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(gravastar_optics PUBLIC cxx_std_20)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(gravastar_optics PUBLIC OpenMP::OpenMP_CXX)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gravastar_geometry PUBLIC
        -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    target_compile_options(gravastar_integration PUBLIC
        -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    target_compile_options(gravastar_optics PUBLIC
        -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    if(GRAVASTAR_SANITIZERS)
        target_compile_options(gravastar_geometry PUBLIC
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(gravastar_geometry PUBLIC -fsanitize=address,undefined)
        target_compile_options(gravastar_integration PUBLIC
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(gravastar_integration PUBLIC -fsanitize=address,undefined)
        target_compile_options(gravastar_optics PUBLIC
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(gravastar_optics PUBLIC -fsanitize=address,undefined)
    endif()
elseif(GRAVASTAR_SANITIZERS)
    message(FATAL_ERROR "GRAVASTAR_SANITIZERS requires GCC or Clang")
endif()

add_executable(gravastar_benchmark src/experiments/benchmark.cpp)
target_link_libraries(gravastar_benchmark PRIVATE gravastar_geometry)

add_executable(gravastar_validate src/experiments/validate.cpp)
target_link_libraries(gravastar_validate PRIVATE gravastar_geometry)

add_executable(gravastar_render_test tools/render_test.cpp)
target_link_libraries(gravastar_render_test PRIVATE gravastar_optics)

include(CTest)
if(BUILD_TESTING)
    add_executable(gravastar_geometry_tests tests/cpp_geometry.cpp)
    target_link_libraries(gravastar_geometry_tests PRIVATE gravastar_geometry)
    add_test(NAME geometry_unit COMMAND gravastar_geometry_tests)

    add_executable(gravastar_integration_tests tests/integration_tests.cpp)
    target_link_libraries(gravastar_integration_tests PRIVATE gravastar_integration)
    add_test(NAME integration_unit COMMAND gravastar_integration_tests)

    add_executable(gravastar_conserved_tests tests/conserved_quantities_test.cpp)
    target_link_libraries(gravastar_conserved_tests PRIVATE gravastar_integration)
    add_test(NAME conserved_quantities_unit COMMAND gravastar_conserved_tests)

    add_executable(gravastar_critical_impact_tests tests/critical_impact_test.cpp)
    target_link_libraries(gravastar_critical_impact_tests PRIVATE gravastar_integration)
    add_test(NAME critical_impact_unit COMMAND gravastar_critical_impact_tests)

    add_test(NAME optical_simulation_test COMMAND gravastar_render_test)

    add_test(NAME benchmark_help COMMAND gravastar_benchmark --help)
    add_test(NAME benchmark_reject_axis COMMAND gravastar_benchmark
        evaluate toy 1 0.875 0.5 1 2 1.5 0)
    set_tests_properties(benchmark_reject_axis PROPERTIES WILL_FAIL TRUE)
    add_test(NAME p1_scientific_validation
        COMMAND gravastar_validate --root ${CMAKE_CURRENT_SOURCE_DIR})
endif()

add_executable(gravastar_turning_point_tests tests/equatorial_turning_points_test.cpp)
target_link_libraries(gravastar_turning_point_tests PRIVATE gravastar_integration)
add_test(NAME equatorial_turning_points_unit COMMAND gravastar_turning_point_tests)

add_executable(gravastar_geodesic_cross_val tests/geodesic_cross_validation.cpp)
add_test(NAME geodesic_cross_validation_unit COMMAND gravastar_geodesic_cross_val)

# --- Vulkan GPU Compute Pipeline (Stage P3) ---
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

set(GRAVASTAR_SPV_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${GRAVASTAR_SPV_DIR}")

set(GRAVASTAR_METRIC_SPV "${GRAVASTAR_SPV_DIR}/gravastar_metric.spv")
add_custom_command(
    OUTPUT "${GRAVASTAR_METRIC_SPV}"
    COMMAND "${GLSLC_EXECUTABLE}" -O "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gravastar_metric.comp" -o "${GRAVASTAR_METRIC_SPV}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gravastar_metric.comp"
    VERBATIM
)

set(GRAVASTAR_GEODESIC_SPV "${GRAVASTAR_SPV_DIR}/gravastar_geodesic.spv")
add_custom_command(
    OUTPUT "${GRAVASTAR_GEODESIC_SPV}"
    COMMAND "${GLSLC_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders" -O "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gravastar_geodesic.comp" -o "${GRAVASTAR_GEODESIC_SPV}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gravastar_geodesic.comp" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gravastar_physics.glsl"
    VERBATIM
)

add_custom_target(gravastar_shaders ALL DEPENDS "${GRAVASTAR_METRIC_SPV}" "${GRAVASTAR_GEODESIC_SPV}")

add_library(gravastar_vulkan src/rendering/vulkan/ComputeContext.cpp)
target_include_directories(gravastar_vulkan PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/include>
)
target_compile_features(gravastar_vulkan PUBLIC cxx_std_20)
target_link_libraries(gravastar_vulkan PRIVATE ${CMAKE_DL_LIBS})

add_executable(gravastar_vulkan_audit tests/vulkan_device_audit.cpp)
target_link_libraries(gravastar_vulkan_audit PRIVATE gravastar_vulkan)
add_test(NAME vulkan_device_audit COMMAND gravastar_vulkan_audit)

add_executable(gravastar_vulkan_metric_test tests/vulkan_metric_comparison.cpp)
target_link_libraries(gravastar_vulkan_metric_test PRIVATE gravastar_vulkan gravastar_geometry)
target_compile_definitions(gravastar_vulkan_metric_test PRIVATE GRAVASTAR_METRIC_SPV="${GRAVASTAR_METRIC_SPV}")
add_dependencies(gravastar_vulkan_metric_test gravastar_shaders)
add_test(NAME vulkan_metric_unit COMMAND gravastar_vulkan_metric_test)

add_executable(gravastar_vulkan_ray_test tests/vulkan_ray_comparison.cpp)
target_link_libraries(gravastar_vulkan_ray_test PRIVATE gravastar_vulkan gravastar_geometry gravastar_integration)
target_compile_definitions(gravastar_vulkan_ray_test PRIVATE GRAVASTAR_GEODESIC_SPV="${GRAVASTAR_GEODESIC_SPV}")
add_dependencies(gravastar_vulkan_ray_test gravastar_shaders)
add_test(NAME vulkan_ray_unit COMMAND gravastar_vulkan_ray_test)
