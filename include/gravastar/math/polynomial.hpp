// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <compare>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gravastar::math {

// Arbitrary-precision signed integer in base 10^9 for overflow-free exact
// rational polynomial Sturm sequences and Bernstein subdivision.
class BigInt {
public:
    static constexpr std::uint32_t kBase = 1000000000U;

    BigInt() = default;
    BigInt(std::int64_t v) {
        if (v < 0) {
            neg_ = true;
            std::uint64_t u = static_cast<std::uint64_t>(-(v + 1)) + 1ULL;
            while (u > 0) {
                limbs_.push_back(static_cast<std::uint32_t>(u % kBase));
                u /= kBase;
            }
        } else if (v > 0) {
            std::uint64_t u = static_cast<std::uint64_t>(v);
            while (u > 0) {
                limbs_.push_back(static_cast<std::uint32_t>(u % kBase));
                u /= kBase;
            }
        }
    }

    static BigInt from_string(std::string_view s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '+')) s.remove_prefix(1);
        while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
        if (s.empty()) throw std::invalid_argument("empty BigInt string");
        bool neg = false;
        if (s.front() == '-') {
            neg = true;
            s.remove_prefix(1);
        }
        if (s.empty()) throw std::invalid_argument("invalid BigInt string");
        BigInt out;
        for (char ch : s) {
            if (ch < '0' || ch > '9') throw std::invalid_argument("non-digit in BigInt");
            out = out * BigInt(10) + BigInt(ch - '0');
        }
        if (neg && !out.is_zero()) out.neg_ = true;
        return out;
    }

    bool is_zero() const { return limbs_.empty(); }
    bool is_negative() const { return neg_ && !limbs_.empty(); }
    int sign() const { return limbs_.empty() ? 0 : (neg_ ? -1 : 1); }

    BigInt abs() const {
        BigInt out = *this;
        out.neg_ = false;
        return out;
    }

    BigInt operator-() const {
        BigInt out = *this;
        if (!out.limbs_.empty()) out.neg_ = !out.neg_;
        return out;
    }

    static int compare_abs(const BigInt& a, const BigInt& b) {
        if (a.limbs_.size() != b.limbs_.size())
            return a.limbs_.size() < b.limbs_.size() ? -1 : 1;
        for (std::size_t i = a.limbs_.size(); i > 0; --i) {
            if (a.limbs_[i - 1] != b.limbs_[i - 1])
                return a.limbs_[i - 1] < b.limbs_[i - 1] ? -1 : 1;
        }
        return 0;
    }

    friend std::strong_ordering operator<=>(const BigInt& a, const BigInt& b) {
        if (a.sign() != b.sign()) return a.sign() <=> b.sign();
        if (a.is_zero()) return std::strong_ordering::equal;
        int cmp = compare_abs(a, b);
        if (a.neg_) cmp = -cmp;
        return cmp < 0 ? std::strong_ordering::less
                       : (cmp > 0 ? std::strong_ordering::greater
                                  : std::strong_ordering::equal);
    }

    friend bool operator==(const BigInt& a, const BigInt& b) {
        return a.neg_ == b.neg_ && a.limbs_ == b.limbs_;
    }

    friend BigInt operator+(const BigInt& a, const BigInt& b) {
        if (a.is_zero()) return b;
        if (b.is_zero()) return a;
        if (a.neg_ == b.neg_) {
            BigInt out = add_abs(a, b);
            out.neg_ = a.neg_;
            return out;
        }
        int cmp = compare_abs(a, b);
        if (cmp == 0) return BigInt(0);
        if (cmp > 0) {
            BigInt out = sub_abs(a, b);
            out.neg_ = a.neg_;
            return out;
        }
        BigInt out = sub_abs(b, a);
        out.neg_ = b.neg_;
        return out;
    }

    friend BigInt operator-(const BigInt& a, const BigInt& b) { return a + (-b); }

    friend BigInt operator*(const BigInt& a, const BigInt& b) {
        if (a.is_zero() || b.is_zero()) return BigInt(0);
        BigInt out;
        out.limbs_.assign(a.limbs_.size() + b.limbs_.size(), 0U);
        for (std::size_t i = 0; i < a.limbs_.size(); ++i) {
            std::uint64_t carry = 0;
            for (std::size_t j = 0; j < b.limbs_.size() || carry > 0; ++j) {
                std::uint64_t cur = out.limbs_[i + j] + carry;
                if (j < b.limbs_.size())
                    cur += static_cast<std::uint64_t>(a.limbs_[i]) * b.limbs_[j];
                out.limbs_[i + j] = static_cast<std::uint32_t>(cur % kBase);
                carry = cur / kBase;
            }
        }
        out.neg_ = (a.neg_ != b.neg_);
        out.normalize();
        return out;
    }

    static std::pair<BigInt, BigInt> divmod(const BigInt& a, const BigInt& b) {
        if (b.is_zero()) throw std::domain_error("BigInt division by zero");
        BigInt ua = a.abs(), ub = b.abs();
        if (compare_abs(ua, ub) < 0) return {BigInt(0), a};
        if (ub.limbs_.size() == 1) {
            std::uint32_t d = ub.limbs_[0];
            BigInt q;
            q.limbs_.resize(ua.limbs_.size(), 0U);
            std::uint64_t rem = 0;
            for (std::size_t i = ua.limbs_.size(); i > 0; --i) {
                std::uint64_t cur = ua.limbs_[i - 1] + rem * kBase;
                q.limbs_[i - 1] = static_cast<std::uint32_t>(cur / d);
                rem = cur % d;
            }
            q.neg_ = (a.neg_ != b.neg_);
            q.normalize();
            BigInt r(static_cast<std::int64_t>(rem));
            if (a.neg_ && !r.is_zero()) r.neg_ = true;
            return {q, r};
        }
        BigInt q, r;
        q.limbs_.resize(ua.limbs_.size(), 0U);
        for (std::size_t i = ua.limbs_.size(); i > 0; --i) {
            if (!r.is_zero()) {
                r.limbs_.insert(r.limbs_.begin(), ua.limbs_[i - 1]);
            } else if (ua.limbs_[i - 1] != 0) {
                r.limbs_.push_back(ua.limbs_[i - 1]);
            }
            std::uint32_t lo = 0, hi = kBase - 1, best = 0;
            while (lo <= hi) {
                std::uint32_t mid = lo + (hi - lo) / 2;
                BigInt prod = ub * BigInt(static_cast<std::int64_t>(mid));
                if (compare_abs(prod, r) <= 0) {
                    best = mid;
                    lo = mid + 1;
                } else {
                    if (mid == 0) break;
                    hi = mid - 1;
                }
            }
            q.limbs_[i - 1] = best;
            if (best > 0) r = r - ub * BigInt(static_cast<std::int64_t>(best));
        }
        q.neg_ = (a.neg_ != b.neg_);
        q.normalize();
        if (a.neg_ && !r.is_zero()) r.neg_ = true;
        return {q, r};
    }

    friend BigInt operator/(const BigInt& a, const BigInt& b) { return divmod(a, b).first; }
    friend BigInt operator%(const BigInt& a, const BigInt& b) { return divmod(a, b).second; }

    static BigInt gcd(BigInt a, BigInt b) {
        a = a.abs();
        b = b.abs();
        while (!b.is_zero()) {
            BigInt r = a % b;
            a = std::move(b);
            b = std::move(r);
        }
        return a;
    }

    long double to_long_double() const {
        long double v = 0.0L;
        for (std::size_t i = limbs_.size(); i > 0; --i)
            v = v * static_cast<long double>(kBase) + static_cast<long double>(limbs_[i - 1]);
        return neg_ ? -v : v;
    }

    std::string to_string() const {
        if (is_zero()) return "0";
        std::string s = neg_ ? "-" : "";
        s += std::to_string(limbs_.back());
        for (std::size_t i = limbs_.size() - 1; i > 0; --i) {
            std::string part = std::to_string(limbs_[i - 1]);
            s.append(9 - part.size(), '0');
            s += part;
        }
        return s;
    }

private:
    std::vector<std::uint32_t> limbs_;
    bool neg_{false};

    void normalize() {
        while (!limbs_.empty() && limbs_.back() == 0U) limbs_.pop_back();
        if (limbs_.empty()) neg_ = false;
    }

    static BigInt add_abs(const BigInt& a, const BigInt& b) {
        BigInt out;
        std::size_t n = std::max(a.limbs_.size(), b.limbs_.size());
        out.limbs_.resize(n, 0U);
        std::uint64_t carry = 0;
        for (std::size_t i = 0; i < n; ++i) {
            std::uint64_t sum = carry;
            if (i < a.limbs_.size()) sum += a.limbs_[i];
            if (i < b.limbs_.size()) sum += b.limbs_[i];
            out.limbs_[i] = static_cast<std::uint32_t>(sum % kBase);
            carry = sum / kBase;
        }
        if (carry > 0) out.limbs_.push_back(static_cast<std::uint32_t>(carry));
        return out;
    }

    static BigInt sub_abs(const BigInt& a, const BigInt& b) {
        BigInt out;
        out.limbs_.resize(a.limbs_.size(), 0U);
        std::int64_t borrow = 0;
        for (std::size_t i = 0; i < a.limbs_.size(); ++i) {
            std::int64_t diff = static_cast<std::int64_t>(a.limbs_[i]) - borrow;
            if (i < b.limbs_.size()) diff -= static_cast<std::int64_t>(b.limbs_[i]);
            if (diff < 0) {
                diff += kBase;
                borrow = 1;
            } else {
                borrow = 0;
            }
            out.limbs_[i] = static_cast<std::uint32_t>(diff);
        }
        out.normalize();
        return out;
    }
};

// Exact reduced rational number p/q with q > 0.
class Rational {
public:
    Rational() : num_(0), den_(1) {}
    Rational(std::int64_t n) : num_(n), den_(1) {}
    Rational(BigInt n, BigInt d = BigInt(1)) : num_(std::move(n)), den_(std::move(d)) {
        reduce();
    }

    static Rational from_string(std::string_view text) {
        while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
        while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
        if (text.empty()) throw std::invalid_argument("empty Rational string");
        auto slash = text.find('/');
        if (slash != std::string_view::npos) {
            return Rational(BigInt::from_string(text.substr(0, slash)),
                            BigInt::from_string(text.substr(slash + 1)));
        }
        bool neg = false;
        if (text.front() == '-' || text.front() == '+') {
            neg = (text.front() == '-');
            text.remove_prefix(1);
        }
        auto exp_pos = text.find_first_of("eE");
        std::int64_t exp10 = 0;
        if (exp_pos != std::string_view::npos) {
            exp10 = std::stoll(std::string(text.substr(exp_pos + 1)));
            text = text.substr(0, exp_pos);
        }
        auto dot = text.find('.');
        std::string digits;
        std::int64_t frac_digits = 0;
        if (dot == std::string_view::npos) {
            digits = std::string(text);
        } else {
            digits = std::string(text.substr(0, dot)) + std::string(text.substr(dot + 1));
            frac_digits = static_cast<std::int64_t>(text.size() - dot - 1);
        }
        BigInt num = BigInt::from_string(digits.empty() ? "0" : digits);
        if (neg) num = -num;
        BigInt den(1);
        std::int64_t total_exp = exp10 - frac_digits;
        if (total_exp > 0) {
            for (std::int64_t i = 0; i < total_exp; ++i) num = num * BigInt(10);
        } else if (total_exp < 0) {
            for (std::int64_t i = 0; i < -total_exp; ++i) den = den * BigInt(10);
        }
        return Rational(std::move(num), std::move(den));
    }

    const BigInt& num() const { return num_; }
    const BigInt& den() const { return den_; }
    bool is_zero() const { return num_.is_zero(); }
    int sign() const { return num_.sign(); }
    Rational abs() const { return Rational(num_.abs(), den_); }
    Rational operator-() const { return Rational(-num_, den_); }

    friend Rational operator+(const Rational& a, const Rational& b) {
        return Rational(a.num_ * b.den_ + b.num_ * a.den_, a.den_ * b.den_);
    }
    friend Rational operator-(const Rational& a, const Rational& b) {
        return Rational(a.num_ * b.den_ - b.num_ * a.den_, a.den_ * b.den_);
    }
    friend Rational operator*(const Rational& a, const Rational& b) {
        return Rational(a.num_ * b.num_, a.den_ * b.den_);
    }
    friend Rational operator/(const Rational& a, const Rational& b) {
        if (b.is_zero()) throw std::domain_error("Rational division by zero");
        return Rational(a.num_ * b.den_, a.den_ * b.num_);
    }

    friend std::strong_ordering operator<=>(const Rational& a, const Rational& b) {
        return (a.num_ * b.den_) <=> (b.num_ * a.den_);
    }
    friend bool operator==(const Rational& a, const Rational& b) {
        return a.num_ == b.num_ && a.den_ == b.den_;
    }

    double to_double() const {
        auto dm = BigInt::divmod(num_.abs(), den_);
        long double whole = dm.first.to_long_double();
        BigInt scaled_rem = dm.second * BigInt(1000000000LL) * BigInt(1000000000LL);
        BigInt frac_int = scaled_rem / den_;
        long double frac = frac_int.to_long_double() / 1e18L;
        long double total = whole + frac;
        return static_cast<double>(num_.is_negative() ? -total : total);
    }

    std::string to_string() const {
        if (den_ == BigInt(1)) return num_.to_string();
        return num_.to_string() + "/" + den_.to_string();
    }

private:
    BigInt num_;
    BigInt den_;

    void reduce() {
        if (den_.is_zero()) throw std::domain_error("zero denominator in Rational");
        if (den_.is_negative()) {
            num_ = -num_;
            den_ = -den_;
        }
        if (num_.is_zero()) {
            den_ = BigInt(1);
            return;
        }
        BigInt g = BigInt::gcd(num_, den_);
        if (g > BigInt(1)) {
            num_ = num_ / g;
            den_ = den_ / g;
        }
    }
};

using Poly = std::vector<Rational>;

inline Poly trim(Poly p) {
    while (p.size() > 1 && p.back().is_zero()) p.pop_back();
    if (p.empty()) p.emplace_back(0);
    return p;
}

inline bool any_nonzero(const Poly& p) {
    for (const auto& c : p)
        if (!c.is_zero()) return true;
    return false;
}


}  // namespace gravastar::math
