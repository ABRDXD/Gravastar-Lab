cmake_minimum_required(VERSION 3.25)
project(GravastarLab VERSION 0.1.0 LANGUAGES CXX)

option(GRAVASTAR_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
option(GRAVASTAR_GYOTO_REFERENCE "Build optional GPL GYOTO reference executable" OFF)
option(GRAVASTAR_DESKTOP "Build Vulkan compute, GLFW window and desktop studio targets" OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(gravastar_geometry src/geometry/geometry.cpp src/geometry/angular.cpp src/geometry/core.cpp)
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

if(GRAVASTAR_GYOTO_REFERENCE)
    add_subdirectory(reference/gyoto)
endif()

add_executable(gravastar_benchmark src/experiments/benchmark.cpp)
target_link_libraries(gravastar_benchmark PRIVATE gravastar_geometry)

add_library(gravastar_optics src/optics/ray.cpp src/optics/chart.cpp src/optics/core_chart.cpp)
target_link_libraries(gravastar_optics PUBLIC gravastar_geometry)

find_package(nlohmann_json 3.12.0 EXACT REQUIRED)
add_executable(gravastar_ray_benchmark src/experiments/ray_benchmark.cpp)
target_link_libraries(gravastar_ray_benchmark PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
target_compile_definitions(gravastar_ray_benchmark PRIVATE
    GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    GRAVASTAR_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
add_executable(gravastar_angular_benchmark src/experiments/angular_benchmark.cpp)
target_link_libraries(gravastar_angular_benchmark PRIVATE gravastar_geometry nlohmann_json::nlohmann_json)
target_compile_definitions(gravastar_angular_benchmark PRIVATE
    GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    GRAVASTAR_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
add_executable(gravastar_trace src/experiments/trace.cpp)
target_link_libraries(gravastar_trace PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
target_compile_definitions(gravastar_trace PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(gravastar_sampling src/experiments/sampling.cpp)
target_link_libraries(gravastar_sampling PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
target_compile_definitions(gravastar_sampling PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(gravastar_validate src/experiments/validate.cpp)
target_link_libraries(gravastar_validate PRIVATE gravastar_geometry nlohmann_json::nlohmann_json)
target_compile_definitions(gravastar_validate PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(gravastar_tools tools/project.cpp)
target_link_libraries(gravastar_tools PRIVATE gravastar_geometry nlohmann_json::nlohmann_json)
target_compile_definitions(gravastar_tools PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

include(CTest)
if(BUILD_TESTING)
    add_executable(gravastar_event_optics_tests tests/event_optics_tests.cpp)
    target_link_libraries(gravastar_event_optics_tests PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_event_optics_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME event_optics COMMAND gravastar_event_optics_tests)
    set_tests_properties(event_optics PROPERTIES TIMEOUT 300 LABELS optics)
    add_executable(gravastar_critical_optics_tests tests/critical_optics_tests.cpp)
    target_link_libraries(gravastar_critical_optics_tests PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_critical_optics_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME critical_optics COMMAND gravastar_critical_optics_tests)
    set_tests_properties(critical_optics PROPERTIES TIMEOUT 300 LABELS optics)
    add_executable(gravastar_shell_turning_tests tests/shell_turning_tests.cpp)
    target_link_libraries(gravastar_shell_turning_tests PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_shell_turning_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME shell_turning COMMAND gravastar_shell_turning_tests)
    set_tests_properties(shell_turning PROPERTIES TIMEOUT 300 LABELS optics)
    add_executable(gravastar_core_cli_tests tests/core_cli_tests.cpp)
    target_link_libraries(gravastar_core_cli_tests PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_core_cli_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME core_cli COMMAND gravastar_core_cli_tests $<TARGET_FILE:gravastar_trace>)
    set_tests_properties(core_cli PROPERTIES TIMEOUT 300 LABELS optics)
    add_executable(gravastar_core_chart_tests tests/core_chart_tests.cpp)
    target_link_libraries(gravastar_core_chart_tests PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_core_chart_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME core_chart COMMAND gravastar_core_chart_tests)
    set_tests_properties(core_chart PROPERTIES TIMEOUT 300 LABELS optics)
    add_executable(gravastar_core_optics_tests tests/core_optics_tests.cpp)
    target_link_libraries(gravastar_core_optics_tests PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_core_optics_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME core_optics COMMAND gravastar_core_optics_tests)
    set_tests_properties(core_optics PROPERTIES TIMEOUT 300 LABELS optics)
    add_executable(gravastar_core_geometry_tests tests/core_geometry_tests.cpp)
    target_link_libraries(gravastar_core_geometry_tests PRIVATE gravastar_geometry nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_core_geometry_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME core_geometry COMMAND gravastar_core_geometry_tests)
    set_tests_properties(core_geometry PROPERTIES TIMEOUT 300 LABELS scientific)
    add_executable(gravastar_angular_optics_tests tests/angular_optics_tests.cpp)
    target_link_libraries(gravastar_angular_optics_tests PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_angular_optics_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME angular_optics COMMAND gravastar_angular_optics_tests)
    set_tests_properties(angular_optics PROPERTIES TIMEOUT 300 LABELS optics)
    add_executable(gravastar_angular_reference_tests tests/angular_reference_tests.cpp)
    target_link_libraries(gravastar_angular_reference_tests PRIVATE gravastar_geometry nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_angular_reference_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME angular_reference COMMAND gravastar_angular_reference_tests)
    set_tests_properties(angular_reference PROPERTIES TIMEOUT 300 LABELS scientific)
    add_executable(gravastar_angular_geometry_tests tests/angular_geometry_tests.cpp)
    target_link_libraries(gravastar_angular_geometry_tests PRIVATE gravastar_geometry nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_angular_geometry_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME angular_geometry COMMAND gravastar_angular_geometry_tests)
    set_tests_properties(angular_geometry PROPERTIES TIMEOUT 300 LABELS scientific)
    add_executable(gravastar_axis_chart_tests tests/axis_chart_tests.cpp)
    target_link_libraries(gravastar_axis_chart_tests PRIVATE gravastar_geometry nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_axis_chart_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME axis_chart_preflight COMMAND gravastar_axis_chart_tests)
    set_tests_properties(axis_chart_preflight PROPERTIES TIMEOUT 180 LABELS scientific)
    add_executable(gravastar_sampling_tests tests/sampling_tests.cpp)
    target_link_libraries(gravastar_sampling_tests PRIVATE gravastar_optics)
    add_test(NAME optics_sampling COMMAND gravastar_sampling_tests)
    set_tests_properties(optics_sampling PROPERTIES TIMEOUT 30 LABELS optics)
    add_executable(gravastar_sampling_cli_tests tests/sampling_cli_tests.cpp)
    target_link_libraries(gravastar_sampling_cli_tests PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_sampling_cli_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME optics_sampling_cli COMMAND gravastar_sampling_cli_tests
        $<TARGET_FILE:gravastar_sampling> $<TARGET_FILE:gravastar_trace>)
    set_tests_properties(optics_sampling_cli PROPERTIES TIMEOUT 180 LABELS optics)
    add_executable(gravastar_optics_tests tests/optics_tests.cpp)
    target_link_libraries(gravastar_optics_tests PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_optics_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME optics_reference COMMAND gravastar_optics_tests)
    set_tests_properties(optics_reference PROPERTIES TIMEOUT 300 LABELS optics)
    add_test(NAME optics_help COMMAND gravastar_trace --help)
    add_executable(gravastar_optics_cli_tests tests/optics_cli_tests.cpp)
    target_link_libraries(gravastar_optics_cli_tests PRIVATE gravastar_optics nlohmann_json::nlohmann_json)
    target_compile_definitions(gravastar_optics_cli_tests PRIVATE GRAVASTAR_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME optics_cli COMMAND gravastar_optics_cli_tests $<TARGET_FILE:gravastar_trace>)
    set_tests_properties(optics_cli PROPERTIES TIMEOUT 600 LABELS optics)
    add_executable(gravastar_validation_failures tests/validation_failures.cpp)
    target_link_libraries(gravastar_validation_failures PRIVATE gravastar_geometry nlohmann_json::nlohmann_json)
    add_test(NAME validation_failure_isolation COMMAND gravastar_validation_failures
        $<TARGET_FILE:gravastar_validate> ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(validation_failure_isolation PROPERTIES TIMEOUT 300)
    add_test(NAME tools_migration COMMAND gravastar_tools self-test)
    add_test(NAME cpp20_language_policy COMMAND gravastar_tools language-policy)
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
    add_test(NAME benchmark_shell_endpoint COMMAND gravastar_benchmark
        evaluate toy 1 0.875 0.5 1.5 1.8 1.5 1.2 shell)
    add_test(NAME benchmark_reject_axis COMMAND gravastar_benchmark
        evaluate toy 1 0.875 0.5 1 2 1.5 0)
    set_tests_properties(benchmark_reject_axis PROPERTIES WILL_FAIL TRUE)
endif()

# --- Optional Vulkan compute and desktop presentation ---
# Off by default so headless scientific builds need no graphics stack.
# Vulkan headers 1.4.321 and GLFW 3.4 are vendored; see THIRD_PARTY.md.
if(GRAVASTAR_DESKTOP)
    find_program(GLSLC_EXECUTABLE glslc REQUIRED)
    set(GRAVASTAR_SPV_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${GRAVASTAR_SPV_DIR}")

    set(GRAVASTAR_PRESENTATION_SPV "${GRAVASTAR_SPV_DIR}/gravastar_presentation.spv")
    add_custom_command(
        OUTPUT "${GRAVASTAR_PRESENTATION_SPV}"
        COMMAND "${GLSLC_EXECUTABLE}" -O
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gravastar_presentation.comp"
            -o "${GRAVASTAR_PRESENTATION_SPV}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gravastar_presentation.comp"
        VERBATIM)
    add_custom_target(gravastar_presentation_shader DEPENDS "${GRAVASTAR_PRESENTATION_SPV}")

    # FP32 GLSL metric kernel; compared with the CPU double-precision evaluator.
    set(GRAVASTAR_METRIC_SPV "${GRAVASTAR_SPV_DIR}/gravastar_metric.spv")
    add_custom_command(
        OUTPUT "${GRAVASTAR_METRIC_SPV}"
        COMMAND "${GLSLC_EXECUTABLE}" -O
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gravastar_metric.comp"
            -o "${GRAVASTAR_METRIC_SPV}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gravastar_metric.comp"
        VERBATIM)
    add_custom_target(gravastar_metric_shader DEPENDS "${GRAVASTAR_METRIC_SPV}")

    # FP32 fixed-step RK4 geodesic kernel (includes gravastar_physics.glsl).
    set(GRAVASTAR_GEODESIC_SPV "${GRAVASTAR_SPV_DIR}/gravastar_geodesic.spv")
    add_custom_command(
        OUTPUT "${GRAVASTAR_GEODESIC_SPV}"
        COMMAND "${GLSLC_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders" -O
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gravastar_geodesic.comp"
            -o "${GRAVASTAR_GEODESIC_SPV}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gravastar_geodesic.comp"
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gravastar_physics.glsl"
        VERBATIM)
    add_custom_target(gravastar_geodesic_shader DEPENDS "${GRAVASTAR_GEODESIC_SPV}")

    add_library(gravastar_vulkan
        src/rendering/vulkan/ComputeContext.cpp
        src/rendering/vulkan/WindowRenderer.cpp)
    target_include_directories(gravastar_vulkan PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/include>)
    target_compile_features(gravastar_vulkan PUBLIC cxx_std_20)
    target_link_libraries(gravastar_vulkan PRIVATE ${CMAKE_DL_LIBS})

    enable_language(C)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
    add_subdirectory(third_party/glfw "${CMAKE_CURRENT_BINARY_DIR}/third_party/glfw" EXCLUDE_FROM_ALL)

    add_library(gravastar_platform src/platform/Window.cpp)
    target_include_directories(gravastar_platform PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/include>)
    target_compile_features(gravastar_platform PUBLIC cxx_std_20)
    target_link_libraries(gravastar_platform PUBLIC glfw)

    add_executable(gravastar_studio tools/gravastar_studio.cpp)
    target_link_libraries(gravastar_studio PRIVATE gravastar_vulkan gravastar_platform)
    target_compile_definitions(gravastar_studio PRIVATE
        GRAVASTAR_PRESENTATION_SHADER="${GRAVASTAR_PRESENTATION_SPV}")
    add_dependencies(gravastar_studio gravastar_presentation_shader)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        foreach(target gravastar_vulkan gravastar_platform gravastar_studio)
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
            # PUBLIC so executables linking these libraries get the runtime too.
            if(GRAVASTAR_SANITIZERS)
                target_compile_options(${target} PUBLIC
                    -fsanitize=address,undefined -fno-omit-frame-pointer)
                target_link_options(${target} PUBLIC -fsanitize=address,undefined)
            endif()
        endforeach()
    endif()

    if(BUILD_TESTING)
        add_executable(gravastar_vulkan_audit tests/vulkan_device_audit.cpp)
        target_link_libraries(gravastar_vulkan_audit PRIVATE gravastar_vulkan)
        add_test(NAME vulkan_device_audit COMMAND gravastar_vulkan_audit)
        add_executable(gravastar_vulkan_metric_test tests/vulkan_metric_comparison.cpp)
        target_link_libraries(gravastar_vulkan_metric_test PRIVATE gravastar_vulkan gravastar_geometry)
        target_compile_definitions(gravastar_vulkan_metric_test PRIVATE
            GRAVASTAR_METRIC_SPV="${GRAVASTAR_METRIC_SPV}")
        add_dependencies(gravastar_vulkan_metric_test gravastar_metric_shader)
        add_test(NAME vulkan_metric_fp32 COMMAND gravastar_vulkan_metric_test)
        add_executable(gravastar_vulkan_ray_test tests/vulkan_ray_comparison.cpp)
        target_link_libraries(gravastar_vulkan_ray_test PRIVATE gravastar_vulkan gravastar_geometry)
        target_compile_definitions(gravastar_vulkan_ray_test PRIVATE
            GRAVASTAR_GEODESIC_SPV="${GRAVASTAR_GEODESIC_SPV}"
            # Measured 2026-11-23, RTX 3050: worst 5.31e-5 over the four rays
            # (identical across runs). Ceiling is about twice that.
            GRAVASTAR_GPU_TURNING_TOLERANCE=1e-4)
        add_dependencies(gravastar_vulkan_ray_test gravastar_geodesic_shader)
        add_test(NAME vulkan_ray_fp32 COMMAND gravastar_vulkan_ray_test)
        # Needs a Wayland or X11 session.
        add_test(NAME vulkan_studio_smoke COMMAND gravastar_studio --smoke)
        set_tests_properties(vulkan_device_audit vulkan_studio_smoke vulkan_metric_fp32 vulkan_ray_fp32
            PROPERTIES LABELS desktop)
        if(GRAVASTAR_SANITIZERS)
            # Measured 2026-11-23: every LeakSanitizer report in these tests is
            # allocated inside the run-time-loaded GPU driver or libdbus, not in
            # project code. Address and undefined-behavior checks stay on.
            set_tests_properties(vulkan_device_audit vulkan_studio_smoke vulkan_metric_fp32 vulkan_ray_fp32
                PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
        endif()
    endif()
endif()

# --- Standalone quad-precision reference suites (from backup/astra-work) ---
# Each carries its own copy of the toy metric and does not call production
# geometry or optics code, so these are independent references, not tests of
# the tracer. They need the GCC/Clang __float128 extension and libquadmath.
if(BUILD_TESTING)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LIBRARIES quadmath)
    check_cxx_source_compiles("
        #include <quadmath.h>
        int main() { __float128 x = 2; return sqrtq(x) > 1 ? 0 : 1; }"
        GRAVASTAR_HAVE_QUADMATH)
    unset(CMAKE_REQUIRED_LIBRARIES)
    if(GRAVASTAR_HAVE_QUADMATH)
        add_executable(gravastar_geodesic_reference tests/geodesic_cross_validation.cpp)
        target_include_directories(gravastar_geodesic_reference PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_compile_features(gravastar_geodesic_reference PRIVATE cxx_std_20)
        target_link_libraries(gravastar_geodesic_reference PRIVATE quadmath)
        add_test(NAME standalone_geodesic_reference COMMAND gravastar_geodesic_reference)

        add_executable(gravastar_zamo_shadow_reference tests/zamo_shadow_validation.cpp)
        target_compile_features(gravastar_zamo_shadow_reference PRIVATE cxx_std_20)
        target_link_libraries(gravastar_zamo_shadow_reference PRIVATE quadmath)
        add_test(NAME standalone_zamo_shadow_reference COMMAND gravastar_zamo_shadow_reference)

        set_tests_properties(standalone_geodesic_reference standalone_zamo_shadow_reference
            PROPERTIES LABELS standalone-reference TIMEOUT 300)
    else()
        message(STATUS "libquadmath not available: standalone quad references skipped")
    endif()
endif()
