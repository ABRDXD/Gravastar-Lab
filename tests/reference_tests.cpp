// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/reference/geometry.hpp"
#include "gravastar/reference/polynomial.hpp"
#include <iostream>
#include <random>
using namespace gravastar::reference;
void require(bool b, const char *message) {
  if (!b)
    throw std::runtime_error(message);
}
int main() {
  try {
    using T = Decimal<90>;
    std::mt19937_64 random(0x947ac15);
    for (int i = 0; i < 10000; ++i) {
      long long a = static_cast<long long>(random() % 2000000000) - 1000000000;
      long long b = static_cast<long long>(random() % 2000000000) - 1000000000;
      if (b == 0)
        b = 1;
      require((Integer(a) + Integer(b)).str() == std::to_string(a + b),
              "integer addition");
      require((Integer(a) * Integer(b)).str() == std::to_string(a * b),
              "integer multiplication");
      auto [q, r] = divmod(Integer(a), Integer(b));
      require(q.str() == std::to_string(a / b) &&
                  r.str() == std::to_string(a % b),
              "integer divmod");
    }
    for (int i = 0; i < 100; ++i) {
      std::string a, b;
      for (int j = 0; j < 200; ++j)
        a += char('0' + random() % 10);
      for (int j = 0; j < 70; ++j)
        b += char('0' + random() % 10);
      Integer aa(a), bb(b);
      auto [q, r] = divmod(aa, bb);
      require(q * bb + r == aa && r >= Integer(0) && r < bb,
              "large integer division identity and bound");
    }
    require((Integer("12345678901234567890") * Integer("98765432109876543210"))
                    .str() == "1219326311370217952237463801111263526900",
            "known large product");
    require(gcd(Integer("12345678901234567890"),
                Integer("98765432109876543210")) == Integer("900000000090"),
            "known large gcd");
    require(Rational::decimal("0.875") == Rational(Integer(7), Integer(8)),
            "exact decimal rational");
    require(Rational(1) / Rational(3) + Rational(1) / Rational(6) ==
                Rational(1) / Rational(2),
            "rational normalization");
    require(abs(T(1) / T(7) * T(7) - T(1)) < T("1e-88"),
            "fixed decimal division");
    require(abs(sqrt(T(2)) * sqrt(T(2)) - T(2)) < T("1e-88"), "square root");
    require(abs(sincos(pi<T>() / T(6)).first - T(1) / T(2)) < T("1e-88"),
            "sin pi/6");
    auto [sn, cs] = sincos(T("1.1"));
    require(abs(sn * sn + cs * cs - T(1)) < T("1e-87"), "trig identity");
    require(std::abs(sn.number() - std::sin(1.1)) < 3e-16,
            "sine vs platform libm");
    require(abs(sincos(pi<T>() / T(4)).first - sqrt(T(2)) / T(2)) < T("1e-87"),
            "sin pi/4");
    auto atan_series = [](T x) {
      T term = x, sum = x;
      for (int i = 1; i < 500; ++i) {
        term = -term * x * x;
        T next = sum + term / T(2 * i + 1);
        if (next == sum)
          return sum;
        sum = next;
      }
      throw std::runtime_error("atan convergence");
    };
    require(abs(T(16) * atan_series(T(1) / T(5)) -
                T(4) * atan_series(T(1) / T(239)) - pi<T>()) < T("1e-86"),
            "Machin pi identity");
    bool rejected = false;
    try {
      (void)(Integer(1) / Integer(0));
    } catch (const std::domain_error &) {
      rejected = true;
    }
    require(rejected, "zero division rejected");
    Polynomial tangent{Rational(1) / Rational(4), -1, 1};
    require(count_roots(tangent) == 1, "even multiplicity root");
    auto intervals = isolate_roots(tangent);
    require(intervals.size() == 1 &&
                intervals[0].first < Rational(1) / Rational(2) &&
                intervals[0].second > Rational(1) / Rational(2),
            "tangent isolated");
    require(count_roots(
                {Rational(1) / Rational(4) - Rational::decimal(".000000000001"),
                 -1, 1}) == 2,
            "close roots");
    require(count_roots({1}) == 0, "constant polynomial");
    rejected = false;
    try {
      (void)isolate_roots(tangent, 0, 1, 0);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    require(rejected, "zero isolation width rejected");
    rejected = false;
    try {
      (void)count_roots({0, 1});
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    require(rejected, "endpoint roots explicit");
    auto delta =
        shell_delta(1, Rational::decimal(".875"), Rational::decimal(".5"),
                    Rational::decimal("1.5"), Rational::decimal("1.8"));
    require(count_roots(delta) == 0, "shell no radial roots");
    auto bound = positive_bounds(delta);
    require(bound && bound->lower == Rational::decimal(".279625"),
            "exact shell bound migration fixture");
    auto old =
        shell_delta(1, Rational::decimal(".875"), Rational::decimal(".5"),
                    Rational::decimal("1.8"), Rational::decimal("2.2"));
    require(positive_bounds(old).has_value(),
            "alternate radial_bounds.py configuration retained");
    require(!positive_bounds(tangent),
            "nonpositive Bernstein bound cannot pass");
    rejected = false;
    try {
      (void)positive_bounds({});
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    require(rejected, "empty Bernstein polynomial rejected before dereference");
    Parameters<T> p;
    auto d = derivatives(T(1), T(".4"), p, Family::de_sitter);
    auto c = curvature(d);
    require(abs(c.scalar - T(2)) < T("1e-30"), "independent de Sitter scalar");
    require(abs(c.kretschmann - T(2) / T(3)) < T("1e-30"),
            "independent de Sitter invariant");
    T full_error;
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b)
        for (int e = 0; e < 4; ++e)
          for (int f = 0; f < 4; ++f)
            full_error = std::max(full_error, abs(c.lower[a][b][e][f] -
                                                  p.L / T(3) *
                                                      (d.g[a][e] * d.g[b][f] -
                                                       d.g[a][f] * d.g[b][e])));
    require(full_error < T("1e-30"), "full constant curvature Riemann tensor");
    auto k = curvature(derivatives(T(3), T("1.1"), p, Family::kerr));
    require(abs(k.scalar) < T("1e-30"), "independent Kerr vacuum");
    auto flat = curvature(derivatives(T(1), T("1.1"), p, Family::flat));
    require(abs(flat.scalar) < T("1e-30"), "independent flat space");
    std::cout << "C++ migration arithmetic, polynomial, high precision "
                 "reference checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
