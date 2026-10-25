// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <compare>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace gravastar::reference {
// Signed base-10^9 integer. All intermediates fit standard uint64_t.
// Division truncates toward zero, with remainder carrying the dividend sign.
class Integer {
  static constexpr std::uint64_t base = 1000000000;
  std::vector<std::uint32_t> digits_;
  bool negative_ = false;
  void trim() {
    while (!digits_.empty() && digits_.back() == 0)
      digits_.pop_back();
    if (digits_.empty())
      negative_ = false;
  }
  static int compare_abs(const Integer &a, const Integer &b) {
    if (a.digits_.size() != b.digits_.size())
      return a.digits_.size() < b.digits_.size() ? -1 : 1;
    for (std::size_t i = a.digits_.size(); i-- > 0;)
      if (a.digits_[i] != b.digits_[i])
        return a.digits_[i] < b.digits_[i] ? -1 : 1;
    return 0;
  }
  Integer small(std::uint64_t n) const {
    Integer r;
    r.negative_ = negative_;
    std::uint64_t carry = 0;
    for (auto d : digits_) {
      auto v = d * n + carry;
      r.digits_.push_back(v % base);
      carry = v / base;
    }
    if (carry)
      r.digits_.push_back(static_cast<std::uint32_t>(carry));
    r.trim();
    return r;
  }
  Integer divide_small(std::uint64_t n) const {
    Integer r = *this;
    std::uint64_t carry = 0;
    for (std::size_t i = digits_.size(); i-- > 0;) {
      auto v = carry * base + digits_[i];
      r.digits_[i] = v / n;
      carry = v % n;
    }
    r.trim();
    return r;
  }

public:
  Integer() = default;
  Integer(long long n) {
    negative_ = n < 0;
    std::uint64_t u = n < 0 ? std::uint64_t(-(n + 1)) + 1 : std::uint64_t(n);
    while (u) {
      digits_.push_back(u % base);
      u /= base;
    }
  }
  explicit Integer(const std::string &s) {
    if (s.empty())
      throw std::invalid_argument("empty integer");
    std::size_t begin = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    negative_ = s[0] == '-';
    if (begin == s.size())
      throw std::invalid_argument("missing integer digits");
    for (std::size_t i = begin; i < s.size(); ++i)
      if (s[i] < '0' || s[i] > '9')
        throw std::invalid_argument("invalid integer digit");
    for (std::size_t end = s.size(); end > begin;) {
      auto start = std::max(begin, end > 9 ? end - 9 : std::size_t(0));
      digits_.push_back(
          static_cast<std::uint32_t>(std::stoul(s.substr(start, end - start))));
      end = start;
    }
    trim();
  }
  bool zero() const { return digits_.empty(); }
  bool negative() const { return negative_; }
  std::string str() const {
    if (zero())
      return "0";
    std::ostringstream o;
    if (negative_)
      o << '-';
    o << digits_.back();
    for (std::size_t i = digits_.size() - 1; i-- > 0;)
      o << std::setw(9) << std::setfill('0') << digits_[i];
    return o.str();
  }
  friend Integer abs(Integer x) {
    x.negative_ = false;
    return x;
  }
  friend Integer operator-(Integer x) {
    if (!x.zero())
      x.negative_ = !x.negative_;
    return x;
  }
  friend bool operator==(const Integer &, const Integer &) = default;
  friend std::strong_ordering operator<=>(const Integer &a, const Integer &b) {
    int c = a.negative_ != b.negative_
                ? (a.negative_ ? -1 : 1)
                : compare_abs(a, b) * (a.negative_ ? -1 : 1);
    return c < 0   ? std::strong_ordering::less
           : c > 0 ? std::strong_ordering::greater
                   : std::strong_ordering::equal;
  }
  friend Integer operator+(const Integer &a, const Integer &b) {
    Integer r;
    if (a.negative_ == b.negative_) {
      std::uint64_t carry = 0;
      r.negative_ = a.negative_;
      for (std::size_t i = 0;
           i < std::max(a.digits_.size(), b.digits_.size()) || carry; ++i) {
        auto v = carry + (i < a.digits_.size() ? a.digits_[i] : 0ULL) +
                 (i < b.digits_.size() ? b.digits_[i] : 0ULL);
        r.digits_.push_back(v % base);
        carry = v / base;
      }
    } else {
      const Integer *big = &a, *little = &b;
      if (compare_abs(a, b) < 0)
        std::swap(big, little);
      r.negative_ = big->negative_;
      std::int64_t borrow = 0;
      for (std::size_t i = 0; i < big->digits_.size(); ++i) {
        std::int64_t v = std::int64_t(big->digits_[i]) - borrow -
                         (i < little->digits_.size() ? little->digits_[i] : 0);
        borrow = v < 0;
        if (borrow)
          v += base;
        r.digits_.push_back(static_cast<std::uint32_t>(v));
      }
    }
    r.trim();
    return r;
  }
  friend Integer operator-(const Integer &a, const Integer &b) {
    return a + (-b);
  }
  friend Integer operator*(const Integer &a, const Integer &b) {
    Integer r;
    r.digits_.resize(a.digits_.size() + b.digits_.size());
    r.negative_ = a.negative_ != b.negative_;
    for (std::size_t i = 0; i < a.digits_.size(); ++i) {
      std::uint64_t carry = 0;
      for (std::size_t j = 0; j < b.digits_.size() || carry; ++j) {
        auto v = r.digits_[i + j] + carry +
                 std::uint64_t(a.digits_[i]) *
                     (j < b.digits_.size() ? b.digits_[j] : 0);
        r.digits_[i + j] = v % base;
        carry = v / base;
      }
    }
    r.trim();
    return r;
  }
  friend std::pair<Integer, Integer> divmod(const Integer &aa,
                                            const Integer &bb) {
    if (bb.zero())
      throw std::domain_error("integer division by zero");
    if (compare_abs(aa, bb) < 0)
      return {Integer(0), aa};
    const auto normalizer = base / (std::uint64_t(bb.digits_.back()) + 1);
    Integer a = abs(aa).small(normalizer), b = abs(bb).small(normalizer), q, r;
    q.digits_.resize(a.digits_.size());
    for (std::size_t i = a.digits_.size(); i-- > 0;) {
      r.digits_.insert(r.digits_.begin(), a.digits_[i]);
      r.trim();
      const std::size_t n = b.digits_.size();
      const std::uint64_t hi = r.digits_.size() > n ? r.digits_[n] : 0;
      const std::uint64_t lo = r.digits_.size() >= n ? r.digits_[n - 1] : 0;
      auto estimate = std::min(base - 1, (hi * base + lo) / b.digits_.back());
      r = r - b.small(estimate);
      while (r.negative_) {
        r = r + b;
        --estimate;
      }
      q.digits_[i] = static_cast<std::uint32_t>(estimate);
    }
    q.negative_ = aa.negative_ != bb.negative_;
    q.trim();
    r = r.divide_small(normalizer);
    r.negative_ = aa.negative_ && !r.zero();
    return {q, r};
  }
  friend Integer operator/(const Integer &a, const Integer &b) {
    return divmod(a, b).first;
  }
  friend Integer operator%(const Integer &a, const Integer &b) {
    return divmod(a, b).second;
  }
};
inline Integer gcd(Integer a, Integer b) {
  a = abs(a);
  b = abs(b);
  while (!b.zero()) {
    auto r = a % b;
    a = b;
    b = r;
  }
  return a;
}
inline Integer power10(unsigned n) {
  return Integer("1" + std::string(n, '0'));
}

class Rational {
  Integer n_{0}, d_{1};

public:
  Rational() = default;
  Rational(long long n) : n_(n) {}
  Rational(Integer n, Integer d) : n_(std::move(n)), d_(std::move(d)) {
    if (d_.zero())
      throw std::domain_error("rational zero denominator");
    if (d_.negative()) {
      n_ = -n_;
      d_ = -d_;
    }
    auto g = gcd(n_, d_);
    n_ = n_ / g;
    d_ = d_ / g;
  }
  static Rational decimal(std::string s) {
    auto dot = s.find('.');
    if (dot == std::string::npos)
      return {Integer(s), Integer(1)};
    auto places = s.size() - dot - 1;
    s.erase(dot, 1);
    return {Integer(s), power10(static_cast<unsigned>(places))};
  }
  std::string str() const { return n_.str() + "/" + d_.str(); }
  double number() const { return std::stod(n_.str()) / std::stod(d_.str()); }
  friend bool operator==(const Rational &, const Rational &) = default;
  friend auto operator<=>(const Rational &a, const Rational &b) {
    return a.n_ * b.d_ <=> b.n_ * a.d_;
  }
  friend Rational operator-(const Rational &a) { return {-a.n_, a.d_}; }
  friend Rational abs(const Rational &a) { return {abs(a.n_), a.d_}; }
  friend Rational operator+(const Rational &a, const Rational &b) {
    auto g = gcd(a.d_, b.d_);
    auto ad = a.d_ / g, bd = b.d_ / g;
    return {a.n_ * bd + b.n_ * ad, ad * b.d_};
  }
  friend Rational operator-(const Rational &a, const Rational &b) {
    return a + (-b);
  }
  friend Rational operator*(const Rational &a, const Rational &b) {
    auto g = gcd(a.n_, b.d_), h = gcd(b.n_, a.d_);
    return {(a.n_ / g) * (b.n_ / h), (a.d_ / h) * (b.d_ / g)};
  }
  friend Rational operator/(const Rational &a, const Rational &b) {
    return a * Rational(b.d_, b.n_);
  }
};

// Fixed decimal places, arbitrary integer range. Operations truncate toward
// zero. Guard places and 90->110 place refinement quantify rounding effects.
template <unsigned Places = 90> class Decimal {
  Integer raw_{};
  static const Integer &scale() {
    static const Integer s = power10(Places);
    return s;
  }
  static Decimal raw(Integer x) {
    Decimal d;
    d.raw_ = std::move(x);
    return d;
  }

public:
  Decimal() = default;
  Decimal(int n) : raw_(Integer(n) * scale()) {}
  explicit Decimal(std::string s) {
    if (s.size() > 10000)
      throw std::out_of_range("decimal literal too long");
    auto exp = s.find_first_of("eE");
    long long exponent = 0;
    if (exp != std::string::npos) {
      std::size_t used = 0;
      auto suffix = s.substr(exp + 1);
      exponent = std::stoll(suffix, &used);
      if (used != suffix.size() || exponent < -10000 || exponent > 10000)
        throw std::out_of_range("invalid decimal exponent");
      s.resize(exp);
    }
    auto dot = s.find('.');
    int fractional = 0;
    if (dot != std::string::npos) {
      fractional = static_cast<int>(s.size() - dot - 1);
      s.erase(dot, 1);
    }
    long long shift = static_cast<long long>(Places) + exponent - fractional;
    if (std::abs(shift) > 10000)
      throw std::out_of_range("decimal exponent too large");
    raw_ =
        shift >= 0 ? Integer(s) * power10(shift) : Integer(s) / power10(-shift);
  }
  std::string str() const {
    std::string s = abs(raw_).str();
    if (s.size() <= Places)
      s.insert(0, Places + 1 - s.size(), '0');
    s.insert(s.size() - Places, 1, '.');
    if (raw_.negative())
      s.insert(0, 1, '-');
    return s;
  }
  double number() const { return std::stod(str()); }
  friend bool operator==(const Decimal &, const Decimal &) = default;
  friend auto operator<=>(const Decimal &a, const Decimal &b) {
    return a.raw_ <=> b.raw_;
  }
  friend Decimal operator+(const Decimal &a, const Decimal &b) {
    return raw(a.raw_ + b.raw_);
  }
  friend Decimal operator-(const Decimal &a, const Decimal &b) {
    return raw(a.raw_ - b.raw_);
  }
  friend Decimal operator-(const Decimal &a) { return raw(-a.raw_); }
  friend Decimal operator*(const Decimal &a, const Decimal &b) {
    return raw(a.raw_ * b.raw_ / scale());
  }
  friend Decimal operator/(const Decimal &a, const Decimal &b) {
    return raw(a.raw_ * scale() / b.raw_);
  }
  friend Decimal abs(const Decimal &a) { return raw(abs(a.raw_)); }
  Decimal &operator+=(const Decimal &b) { return *this = *this + b; }
  Decimal &operator-=(const Decimal &b) { return *this = *this - b; }
};
template <class T> T power(T x, unsigned n) {
  T r(1);
  while (n) {
    if (n & 1U)
      r = r * x;
    x = x * x;
    n >>= 1;
  }
  return r;
}
template <class T> T pi() {
  return T("3."
           "1415926535897932384626433832795028841971693993751058209749445923078"
           "164062862089986280348253421170679821480865132823066470938446");
}
template <class T> std::pair<T, T> sincos(T x) {
  if (abs(x) > T(100))
    throw std::out_of_range("reference trigonometry argument");
  const T p = pi<T>();
  while (x > p)
    x = x - T(2) * p;
  while (x < -p)
    x = x + T(2) * p;
  T s = x, c(1), st = x, ct(1);
  for (int n = 1; n < 500; ++n) {
    st = -st * x * x / T(2 * n * (2 * n + 1));
    ct = -ct * x * x / T((2 * n - 1) * 2 * n);
    T ns = s + st, nc = c + ct;
    if (ns == s && nc == c)
      return {ns, nc};
    s = ns;
    c = nc;
  }
  throw std::runtime_error("reference trig did not converge");
}
template <class T> T sqrt(const T &x) {
  if (x < T(0))
    throw std::domain_error("negative square root");
  if (x == T(0))
    return T(0);
  // The initial estimate is not used as a final scientific value.
  std::ostringstream o;
  o << std::setprecision(17) << std::sqrt(x.number());
  T y(o.str()), previous;
  for (int i = 0; i < 100; ++i) {
    T next = (y + x / y) / T(2);
    if (next == y || next == previous)
      return std::min(next, y);
    previous = y;
    y = next;
  }
  throw std::runtime_error("reference square root did not converge");
}
} // namespace gravastar::reference
