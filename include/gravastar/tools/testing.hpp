// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <iostream>
#include <cstdlib>
#include <string_view>

namespace gravastar::tools {

inline void report_failure(std::string_view expr, std::string_view msg, const char* file, int line) {
    std::cerr << "\n=================================================================\n"
              << "[TEST FAILURE] " << msg << "\n"
              << "  Expression: " << expr << "\n"
              << "  Location:   " << file << ":" << line << "\n"
              << "=================================================================\n";
    std::exit(1);
}

} // namespace gravastar::tools

#define GRAVASTAR_REQUIRE(condition, message) \
    do { \
        if (!(condition)) { \
            ::gravastar::tools::report_failure(#condition, message, __FILE__, __LINE__); \
        } \
    } while (false)
