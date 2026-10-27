// SPDX-License-Identifier: MIT
// Configuration for the bounded C++20 reference build, not a general GYOTO ABI.
#pragma once
#include <cstddef>
#include <cmath>
#include <type_traits>
static_assert(std::is_same_v<std::size_t, unsigned long>,
              "The initial GYOTO reference configuration requires an LP64 ABI");
#define GYOTO_SIZE__T_IS_UNSIGNED_LONG 1
#define GYOTO_SOVERS "9"
#define GYOTO_PLUGIN_SFX "so"
#define GYOTO_PREFIX ""
#define GYOTO_PKGLIBDIR ""
#define GYOTO_DEFAULT_PLUGINS ""
#define HAVE_SINCOS 0
#define GYOTO_NO_DEPRECATED 1
// Leave optional backends undefined: Legacy integrator, no XML/FITS/MPI/Boost.
