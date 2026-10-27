# SPDX-License-Identifier: MIT
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The initial GYOTO CMake reference build is verified only on Linux")
endif()
include(FetchSources.cmake)
set(GYOTO_REVISION d7dfa50402c40a2f2bf1aef33fc7e98283f35b3a)
set(EIGEN_REVISION 3147391d946bb4b6c68edd901f2add6ac1f31f8c)
reference_repository(gyoto https://github.com/gyoto/Gyoto.git 2.0.2 "${GYOTO_REVISION}")
reference_repository(eigen https://gitlab.com/libeigen/eigen.git 3.4.0 "${EIGEN_REVISION}")
set(gyoto_source "${CMAKE_CURRENT_BINARY_DIR}/upstream/gyoto")
set(eigen_source "${CMAKE_CURRENT_BINARY_DIR}/upstream/eigen3")
reference_export_matching("${gyoto_repository}" "${GYOTO_REVISION}"
    "^include/[^/]+\\.h$|^COPYING$|^AUTHORS$" "${gyoto_source}")
reference_export_matching("${eigen_repository}" "${EIGEN_REVISION}"
    "^Eigen/.*\\.h$|^Eigen/[A-Za-z0-9_]+$|^COPYING\\.[A-Za-z0-9]+$"
    "${eigen_source}")
set(core_names Value Property Object Astrobj Factory Register SmartPointer Utils
    Metric WIP Worldline Photon Scenery WorldlineIntegState Error Screen Spectrum
    Spectrometer ComplexSpectrometer UniformSpectrometer StandardAstrobj ThinDisk
    Converters Functors Hooks GridData2D FitsRW KerrBL Minkowski)
set(core_sources)
foreach(name IN LISTS core_names)
    set(path "${gyoto_source}/lib/${name}.C")
    reference_export("${gyoto_repository}" "${GYOTO_REVISION}" "lib/${name}.C" "${path}")
    list(APPEND core_sources "${path}")
endforeach()
# Upstream 2.0.2 leaves two XML-dependent textual-vector cases unguarded.
# Reject those unsupported input paths; typed setters and numerical code remain intact.
file(READ "${gyoto_source}/lib/Object.C" object_source)
foreach(parser parseArray parseArrayULong)
    set(original "val = FactoryMessenger::${parser}(content);")
    string(FIND "${object_source}" "${original}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pinned GYOTO XML-free compatibility patch no longer matches")
    endif()
    string(REPLACE "${original}"
        "GYOTO_ERROR(\"XML-free reference: textual vector properties unsupported; use typed setters\");"
        object_source "${object_source}")
endforeach()
file(WRITE "${gyoto_source}/lib/Object.C" "${object_source}")
configure_file(config.hpp "${gyoto_source}/include/GyotoConfig.h" COPYONLY)
add_library(gravastar_gyoto_upstream STATIC ${core_sources})
target_compile_features(gravastar_gyoto_upstream PUBLIC cxx_std_20)
set_target_properties(gravastar_gyoto_upstream PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(gravastar_gyoto_upstream SYSTEM PUBLIC
    "${gyoto_source}/include" "${CMAKE_CURRENT_BINARY_DIR}/upstream")
target_link_libraries(gravastar_gyoto_upstream PUBLIC ${CMAKE_DL_LIBS})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gravastar_gyoto_upstream PRIVATE -fno-fast-math -ffp-contract=off)
endif()
message(STATUS "Optional reference: GYOTO 2.0.2 GPL-3.0-or-later; Eigen 3.4.0 notices preserved")
