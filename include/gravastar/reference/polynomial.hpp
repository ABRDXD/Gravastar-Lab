// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gravastar/reference/number.hpp"
#include <optional>
#include <tuple>
namespace gravastar::reference {
using Polynomial = std::vector<Rational>;
inline Polynomial trim(Polynomial p) {
  while (p.size() > 1 && p.back() == Rational(0))
    p.pop_back();
  return p;
}
inline Polynomial add(const Polynomial &a, const Polynomial &b) {
  Polynomial r(std::max(a.size(), b.size()));
  for (std::size_t i = 0; i < r.size(); ++i)
    r[i] = (i < a.size() ? a[i] : Rational(0)) +
           (i < b.size() ? b[i] : Rational(0));
  return trim(r);
}
inline Polynomial scale(Polynomial p, const Rational &k) {
  for (auto &x : p)
    x = x * k;
  return trim(p);
}
inline Polynomial multiply(const Polynomial &a, const Polynomial &b) {
  Polynomial r(a.size() + b.size() - 1);
  for (std::size_t i = 0; i < a.size(); ++i)
    for (std::size_t j = 0; j < b.size(); ++j)
      r[i + j] = r[i + j] + a[i] * b[j];
  return trim(r);
}
inline Polynomial derivative(const Polynomial &p) {
  Polynomial r;
  for (std::size_t i = 1; i < p.size(); ++i)
    r.push_back(p[i] * Rational(static_cast<long long>(i)));
  if (r.empty())
    r.push_back(0);
  return trim(r);
}
inline Rational value(const Polynomial &p, const Rational &x) {
  Rational r;
  for (auto i = p.rbegin(); i != p.rend(); ++i)
    r = r * x + *i;
  return r;
}
inline Polynomial remainder(Polynomial a, const Polynomial &b) {
  if (b.empty() || (b.size() == 1 && b[0] == Rational(0)))
    throw std::domain_error("zero polynomial divisor");
  a = trim(a);
  while (a.size() >= b.size() && !(a.size() == 1 && a[0] == Rational(0))) {
    auto k = a.size() - b.size();
    auto f = a.back() / b.back();
    for (std::size_t j = 0; j < b.size(); ++j)
      a[j + k] = a[j + k] - f * b[j];
    a = trim(a);
  }
  return a;
}
inline std::vector<Polynomial> sturm(Polynomial p) {
  p = trim(p);
  if (p.empty() || (p.size() == 1 && p[0] == Rational(0)))
    throw std::invalid_argument("zero polynomial has no finite root count");
  std::vector<Polynomial> s{p};
  if (p.size() == 1)
    return s;
  s.push_back(derivative(p));
  while (true) {
    auto r = scale(remainder(s[s.size() - 2], s.back()), -1);
    if (r.size() == 1 && r[0] == Rational(0))
      return s;
    r = scale(r, Rational(1) / abs(r.back()));
    s.push_back(std::move(r));
  }
}
inline int variations(const std::vector<Polynomial> &s, const Rational &x) {
  int previous = 0, count = 0;
  for (const auto &p : s) {
    auto v = value(p, x);
    int sign = v > Rational(0) ? 1 : v < Rational(0) ? -1 : 0;
    if (sign) {
      if (previous && sign != previous)
        ++count;
      previous = sign;
    }
  }
  return count;
}
inline void interval_check(const Polynomial &p, const Rational &lo,
                           const Rational &hi) {
  if (!(lo < hi))
    throw std::invalid_argument("root interval must be ordered");
  if (value(p, lo) == Rational(0) || value(p, hi) == Rational(0))
    throw std::invalid_argument("root at interval endpoint");
}
inline int count_roots(const Polynomial &p, Rational lo = 0, Rational hi = 1) {
  interval_check(p, lo, hi);
  auto s = sturm(p);
  return variations(s, lo) - variations(s, hi);
}
inline std::vector<std::pair<Rational, Rational>>
isolate_roots(const Polynomial &p, Rational lo = 0, Rational hi = 1,
              Rational width = Rational(Integer(1), Integer("1000000000000"))) {
  interval_check(p, lo, hi);
  if (width <= Rational(0))
    throw std::invalid_argument("root width must be positive");
  auto s = sturm(p);
  std::vector<std::tuple<Rational, Rational, int>> pending{
      {lo, hi, variations(s, lo) - variations(s, hi)}};
  std::vector<std::pair<Rational, Rational>> out;
  std::size_t budget = 100000;
  while (!pending.empty()) {
    if (--budget == 0)
      throw std::runtime_error("root isolation budget exhausted");
    auto [l, h, n] = pending.back();
    pending.pop_back();
    if (n == 0)
      continue;
    if (n == 1 && h - l <= width) {
      out.emplace_back(l, h);
      continue;
    }
    Rational mid = (l + h) / Rational(2);
    int denominator = 3;
    while (value(p, mid) == Rational(0))
      mid = l + (h - l) / Rational(denominator++);
    int left = variations(s, l) - variations(s, mid);
    pending.emplace_back(l, mid, left);
    pending.emplace_back(mid, h, n - left);
  }
  std::sort(out.begin(), out.end());
  return out;
}
inline Polynomial shell_delta(Rational m, Rational a, Rational L, Rational ri,
                              Rational ro) {
  Polynomial r{ri, ro - ri}, r2 = multiply(r, r), b = add(r2, {a * a}),
                             mu{0, 0, 0, 0, 35, -84, 70, -20},
                             lam = add({1}, scale(mu, -1));
  return add(multiply(b, add({1}, scale(multiply(r2, lam), -L / Rational(3)))),
             scale(multiply(r, mu), Rational(-2) * m));
}
inline long long binomial(int n, int k) {
  long long v = 1;
  for (int i = 1; i <= k; ++i)
    v = v * (n - i + 1) / i;
  return v;
}
struct Bounds {
  Rational lower, upper;
  int leaves{}, depth{};
};
inline std::optional<Bounds> positive_bounds(const Polynomial &p,
                                             int maximum_depth = 24) {
  if (p.empty())
    throw std::invalid_argument("Bernstein polynomial must contain a coefficient");
  if (p.size() > 32)
    throw std::invalid_argument(
        "Bernstein degree exceeds tested combinatorial range");
  int n = static_cast<int>(p.size()) - 1;
  Polynomial b(p.size());
  for (int k = 0; k <= n; ++k)
    for (int j = 0; j <= k; ++j)
      b[k] = b[k] +
             p[j] * Rational(Integer(binomial(k, j)), Integer(binomial(n, j)));
  std::vector<std::pair<Polynomial, int>> pending{{b, 0}};
  Bounds out{};
  while (!pending.empty()) {
    auto [v, d] = pending.back();
    pending.pop_back();
    auto [lo, hi] = std::minmax_element(v.begin(), v.end());
    if (*lo > Rational(0)) {
      if (out.leaves == 0) {
        out.lower = *lo;
        out.upper = *hi;
      } else {
        out.lower = std::min(out.lower, *lo);
        out.upper = std::max(out.upper, *hi);
      }
      ++out.leaves;
      out.depth = std::max(out.depth, d);
      continue;
    }
    if (d >= maximum_depth || *hi <= Rational(0))
      return std::nullopt;
    Polynomial left{v.front()}, right{v.back()};
    while (v.size() > 1) {
      Polynomial next;
      for (std::size_t j = 1; j < v.size(); ++j)
        next.push_back((v[j - 1] + v[j]) / Rational(2));
      v = std::move(next);
      left.push_back(v.front());
      right.push_back(v.back());
    }
    std::reverse(right.begin(), right.end());
    pending.emplace_back(left, d + 1);
    pending.emplace_back(right, d + 1);
  }
  return out;
}
} // namespace gravastar::reference
