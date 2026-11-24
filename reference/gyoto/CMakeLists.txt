# SPDX-License-Identifier: GPL-3.0-or-later
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The initial GYOTO CMake reference build is verified only on Linux")
endif()
include(FetchSources.cmake)
set(GYOTO_REVISION d7dfa50402c40a2f2bf1aef33fc7e98283f35b3a)
set(EIGEN_REVISION 3147391d946bb4b6c68edd901f2add6ac1f31f8c)
reference_repository(gyoto https://github.com/gyoto/Gyoto.git 2.0.2 "${GYOTO_REVISION}")
reference_repository(eigen https://gitlab.com/libeigen/eigen.git 3.4.0 "${EIGEN_REVISION}")
set(boost_archive "${GRAVASTAR_REFERENCE_GIT_CACHE}/boost_1_84_0.tar.gz")
set(boost_sha256 a5800f405508f5df8114558ca9855d2640a2de8f0445f051fa1c7c3383045724)
file(DOWNLOAD https://archives.boost.io/release/1.84.0/source/boost_1_84_0.tar.gz
    "${boost_archive}" EXPECTED_HASH "SHA256=${boost_sha256}" TLS_VERIFY ON)
set(boost_source "${CMAKE_CURRENT_BINARY_DIR}/upstream/boost_1_84_0")
if(NOT EXISTS "${boost_source}/.headers-extracted")
    file(ARCHIVE_EXTRACT INPUT "${boost_archive}"
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/upstream" PATTERNS
        "boost_1_84_0/boost/*.hpp" "boost_1_84_0/boost/*.h"
        "boost_1_84_0/boost/*.ipp" "boost_1_84_0/LICENSE_1_0.txt")
    file(WRITE "${boost_source}/.headers-extracted" "${boost_sha256}\n")
endif()
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
    Converters Functors Hooks KerrBL Minkowski)
# GridData2D and FitsRW are left out: only FITS-backed astrobj plugins outside
# this build use them, and without CFITSIO they do not compile.
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
# Upstream 2.0.2 declares thread_safe only under HAVE_PTHREAD but uses it in a
# log message after that block. The thread-free build declares it false.
file(READ "${gyoto_source}/lib/Scenery.C" scenery_source)
set(original "  // Call worker on the parent thread")
string(FIND "${scenery_source}" "${original}" found)
if(found EQUAL -1)
    message(FATAL_ERROR "Pinned GYOTO thread-free compatibility patch no longer matches")
endif()
string(REPLACE "${original}"
    "#ifndef HAVE_PTHREAD\n  const bool thread_safe = false; // thread-free reference build\n#endif\n${original}"
    scenery_source "${scenery_source}")
file(WRITE "${gyoto_source}/lib/Scenery.C" "${scenery_source}")
# Upstream 2.0.2 leaves Spectrometer::Uniform::fillProperty (XML output)
# unguarded. Without XML it raises an error instead; nothing else changes.
file(READ "${gyoto_source}/lib/UniformSpectrometer.C" uniform_source)
set(head "void Uniform::fillProperty(Gyoto::FactoryMessenger *fmp,")
set(tail "#ifdef GYOTO_USE_XERCES\n\nvoid Gyoto::Spectrometer::Uniform::setParameters")
string(FIND "${uniform_source}" "${head}" found_head)
string(FIND "${uniform_source}" "#ifdef GYOTO_USE_XERCES\n\nvoid Gyoto::Spectrometer::Uniform::setParameters" found_tail)
if(found_head EQUAL -1 OR found_tail EQUAL -1)
    message(FATAL_ERROR "Pinned GYOTO XML-free spectrometer patch no longer matches")
endif()
string(REPLACE "${head}" "#ifdef GYOTO_USE_XERCES\n${head}" uniform_source "${uniform_source}")
string(REPLACE "#ifdef GYOTO_USE_XERCES\n\nvoid Gyoto::Spectrometer::Uniform::setParameters"
    "#else\nvoid Uniform::fillProperty(Gyoto::FactoryMessenger *, Property const &) const {\n  GYOTO_ERROR(\"XML-free reference: Spectrometer::Uniform XML output unsupported\");\n}\n#endif\n#ifdef GYOTO_USE_XERCES\n\nvoid Gyoto::Spectrometer::Uniform::setParameters"
    uniform_source "${uniform_source}")
file(WRITE "${gyoto_source}/lib/UniformSpectrometer.C" "${uniform_source}")
configure_file(config.hpp "${gyoto_source}/include/GyotoConfig.h" COPYONLY)
add_library(gravastar_gyoto_upstream STATIC ${core_sources})
target_compile_features(gravastar_gyoto_upstream PUBLIC cxx_std_20)
set_target_properties(gravastar_gyoto_upstream PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(gravastar_gyoto_upstream SYSTEM PUBLIC
    "${gyoto_source}/include" "${CMAKE_CURRENT_BINARY_DIR}/upstream" "${boost_source}")
target_link_libraries(gravastar_gyoto_upstream PUBLIC ${CMAKE_DL_LIBS})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gravastar_gyoto_upstream PRIVATE -fno-fast-math -ffp-contract=off)
endif()
message(STATUS "Optional reference: GYOTO 2.0.2 GPL-3.0-or-later; Eigen 3.4.0 and Boost 1.84.0 notices preserved")
add_executable(gravastar_gyoto_compare compare.cpp)
target_link_libraries(gravastar_gyoto_compare PRIVATE gravastar_gyoto_upstream
    gravastar_optics nlohmann_json::nlohmann_json)
target_compile_definitions(gravastar_gyoto_compare PRIVATE
    GRAVASTAR_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
    GRAVASTAR_GYOTO_REVISION="${GYOTO_REVISION}"
    GRAVASTAR_EIGEN_REVISION="${EIGEN_REVISION}"
    GRAVASTAR_BOOST_SHA256="${boost_sha256}")
if(BUILD_TESTING)
    add_test(NAME gyoto_reference COMMAND gravastar_gyoto_compare)
    set_tests_properties(gyoto_reference PROPERTIES TIMEOUT 180 LABELS "optics;optional_reference")
endif()
