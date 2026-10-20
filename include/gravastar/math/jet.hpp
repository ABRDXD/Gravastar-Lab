// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace gravastar::math {

// Bivariate Taylor coefficients, c[i][j] = d_r^i d_theta^j f / (i! j!).
// Terms of total order > 3 are identically zero. No numerical differences.
struct Jet {
    std::array<std::array<double, 4>, 4> c{};
    Jet() = default;
    Jet(double value) { c[0][0] = value; }  // NOLINT: scalar algebra
    static Jet variable(double value, std::size_t axis) {
        Jet result(value);
        result.c[axis == 0 ? 1 : 0][axis == 0 ? 0 : 1] = 1.0;
        return result;
    }
    double value() const { return c[0][0]; }
    double derivative(std::size_t r_order, std::size_t theta_order) const {
        constexpr std::array<double, 4> factorial{1, 1, 2, 6};
        return c[r_order][theta_order] * factorial[r_order] * factorial[theta_order];
    }
};

inline Jet operator+(const Jet& a, const Jet& b) {
    Jet result;
    for (std::size_t i = 0; i <= 3; ++i)
        for (std::size_t j = 0; i + j <= 3; ++j)
            result.c[i][j] = a.c[i][j] + b.c[i][j];
    return result;
}
inline Jet operator-(const Jet& a, const Jet& b) {
    Jet result;
    for (std::size_t i = 0; i <= 3; ++i)
        for (std::size_t j = 0; i + j <= 3; ++j)
            result.c[i][j] = a.c[i][j] - b.c[i][j];
    return result;
}
inline Jet operator-(const Jet& a) { return Jet(0) - a; }
inline Jet operator*(const Jet& a, const Jet& b) {
    Jet result;
    for (std::size_t i = 0; i <= 3; ++i)
        for (std::size_t j = 0; i + j <= 3; ++j)
            for (std::size_t k = 0; k <= i; ++k)
                for (std::size_t l = 0; l <= j; ++l)
                    result.c[i][j] += a.c[k][l] * b.c[i-k][j-l];
    return result;
}
inline Jet reciprocal(const Jet& x) {
    const double inv = 1.0 / x.value();
    const Jet normalized = (x - Jet(x.value())) * Jet(inv);
    const Jet squared = normalized * normalized;
    return Jet(inv) * (Jet(1) - normalized + squared - squared * normalized);
}
inline Jet operator/(const Jet& a, const Jet& b) { return a * reciprocal(b); }
inline Jet sin(const Jet& x) {
    const Jet h = x - Jet(x.value());
    const Jet h2 = h * h;
    return Jet(std::sin(x.value())) * (Jet(1) - h2 * Jet(0.5))
         + Jet(std::cos(x.value())) * (h - h2 * h * Jet(1.0 / 6.0));
}
inline Jet cos(const Jet& x) {
    const Jet h = x - Jet(x.value());
    const Jet h2 = h * h;
    return Jet(std::cos(x.value())) * (Jet(1) - h2 * Jet(0.5))
         - Jet(std::sin(x.value())) * (h - h2 * h * Jet(1.0 / 6.0));
}
}  // namespace gravastar::math
