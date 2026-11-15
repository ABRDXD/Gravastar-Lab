// SPDX-License-Identifier: GPL-3.0-or-later
// Native C++20 replacement for python/validate.py, tools/validate.py, and tools/source_manifest.py.
#include "gravastar/geometry/geometry.hpp"
#include "gravastar/geometry/reference.hpp"
#include "gravastar/math/polynomial.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace fs = std::filesystem;
using namespace gravastar::geometry;
using namespace gravastar::geometry::reference;
using namespace gravastar::math;

// Pure C++20 FIPS 180-4 SHA-256 implementation for source-tree manifests.
class Sha256 {
public:
    Sha256() { reset(); }

    void reset() {
        state_ = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                  0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
        bit_len_ = 0;
        buf_len_ = 0;
    }

    void update(const std::uint8_t* data, std::size_t len) {
        for (std::size_t i = 0; i < len; ++i) {
            buf_[buf_len_++] = data[i];
            if (buf_len_ == 64) {
                transform(buf_.data());
                bit_len_ += 512;
                buf_len_ = 0;
            }
        }
    }

    void update(std::string_view sv) {
        update(reinterpret_cast<const std::uint8_t*>(sv.data()), sv.size());
    }

    std::string hex_digest() {
        std::uint64_t total_bits = bit_len_ + buf_len_ * 8ULL;
        buf_[buf_len_++] = 0x80U;
        if (buf_len_ > 56) {
            while (buf_len_ < 64) buf_[buf_len_++] = 0U;
            transform(buf_.data());
            buf_len_ = 0;
        }
        while (buf_len_ < 56) buf_[buf_len_++] = 0U;
        for (int i = 7; i >= 0; --i)
            buf_[buf_len_++] = static_cast<std::uint8_t>((total_bits >> (i * 8)) & 0xffU);
        transform(buf_.data());

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (std::uint32_t w : state_) oss << std::setw(8) << w;
        return oss.str();
    }

    static std::string hash_bytes(std::string_view sv) {
        Sha256 hasher;
        hasher.update(sv);
        return hasher.hex_digest();
    }

    static std::string hash_file(const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        if (!in) throw std::runtime_error("cannot read file for SHA-256: " + p.string());
        Sha256 hasher;
        std::array<char, 8192> chunk{};
        while (in.read(chunk.data(), static_cast<std::streamsize>(chunk.size())) || in.gcount() > 0) {
            hasher.update(reinterpret_cast<const std::uint8_t*>(chunk.data()),
                          static_cast<std::size_t>(in.gcount()));
        }
        return hasher.hex_digest();
    }

private:
    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buf_{};
    std::uint64_t bit_len_{0};
    std::size_t buf_len_{0};

    static constexpr std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
        return (x >> n) | (x << (32U - n));
    }

    void transform(const std::uint8_t* block) {
        static constexpr std::array<std::uint32_t, 64> k{
            0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
            0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
            0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
            0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
            0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
            0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
            0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
            0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
        };
        std::array<std::uint32_t, 64> w{};
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24U)
                 | (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16U)
                 | (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8U)
                 | static_cast<std::uint32_t>(block[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            std::uint32_t s0 = rotr(w[i - 15], 7U) ^ rotr(w[i - 15], 18U) ^ (w[i - 15] >> 3U);
            std::uint32_t s1 = rotr(w[i - 2], 17U) ^ rotr(w[i - 2], 19U) ^ (w[i - 2] >> 10U);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        auto [a, b, c, d, e, f, g, h] = state_;
        for (int i = 0; i < 64; ++i) {
            std::uint32_t S1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
            std::uint32_t ch = (e & f) ^ ((~e) & g);
            std::uint32_t temp1 = h + S1 + ch + k[i] + w[i];
            std::uint32_t S0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
            std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            std::uint32_t temp2 = S0 + maj;
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }
};

struct ManifestRecord {
    std::string path;
    std::string sha256;
    std::uintmax_t size{0};
    std::string role;
};

struct SourceManifest {
    std::string computational_sha256;
    std::string documentation_sha256;
    std::vector<ManifestRecord> files;
};

bool is_excluded_dir(std::string_view name) {
    static constexpr std::array<std::string_view, 8> excluded{
        ".git", ".venv", ".cache", "__pycache__", ".pytest_cache",
        "build", "results", "node_modules"
    };
    return std::find(excluded.begin(), excluded.end(), name) != excluded.end();
}

bool is_excluded_ext(std::string_view ext) {
    static constexpr std::array<std::string_view, 6> excluded{
        ".pyc", ".pyo", ".o", ".a", ".so", ".exe"
    };
    return std::find(excluded.begin(), excluded.end(), ext) != excluded.end();
}

void collect_dir_files(const fs::path& root, const fs::path& dir,
                       std::vector<std::pair<fs::path, std::string>>& out) {
    if (!fs::exists(dir)) return;
    if (fs::is_symlink(dir))
        throw std::runtime_error("symlinked source directory is not independent: " + dir.string());
    for (auto it = fs::recursive_directory_iterator(dir); it != fs::recursive_directory_iterator(); ++it) {
        const fs::path p = it->path();
        if (fs::is_symlink(p))
            throw std::runtime_error("symlinked source entry: " + p.string());
        if (it->is_directory()) {
            if (is_excluded_dir(p.filename().string())) it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file()) {
            if (is_excluded_ext(p.extension().string())) continue;
            out.emplace_back(fs::relative(p, root), "computational");
        }
    }
}

std::string digest_entries(const std::vector<ManifestRecord>& entries) {
    std::ostringstream canonical;
    canonical << '[';
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) canonical << ',';
        canonical << "{\"path\":\"" << entries[i].path << "\",\"sha256\":\""
                  << entries[i].sha256 << "\",\"size\":" << entries[i].size << '}';
    }
    canonical << ']';
    return Sha256::hash_bytes(canonical.str());
}

SourceManifest build_manifest(const fs::path& project_root) {
    const fs::path root = fs::canonical(project_root);
    std::vector<std::pair<fs::path, std::string>> selected;

    for (std::string_view name : {"CMakeLists.txt", "CMakePresets.json"}) {
        fs::path p = root / name;
        if (fs::is_symlink(p)) throw std::runtime_error("symlinked source file: " + p.string());
        if (fs::is_regular_file(p)) selected.emplace_back(fs::path(name), "computational");
    }
    for (std::string_view dir_name : {"include", "src", "tests", "benchmarks", "tools",
                                      "schemas", "scenes", "shaders", "plugins", "third_party"}) {
        collect_dir_files(root, root / dir_name, selected);
    }
    for (std::string_view doc_name : {"README.md", "PLAN.md", "LICENSE", "NOTICE", "THIRD_PARTY.md"}) {
        fs::path p = root / doc_name;
        if (fs::is_symlink(p)) throw std::runtime_error("symlinked doc file: " + p.string());
        if (fs::is_regular_file(p)) selected.emplace_back(fs::path(doc_name), "documentation");
    }
    std::sort(selected.begin(), selected.end(),
              [](const auto& a, const auto& b) { return a.first.generic_string() < b.first.generic_string(); });

    SourceManifest m;
    std::vector<ManifestRecord> comp, docs;
    for (const auto& [rel, role] : selected) {
        fs::path full = root / rel;
        ManifestRecord rec{
            rel.generic_string(),
            Sha256::hash_file(full),
            fs::file_size(full),
            role
        };
        m.files.push_back(rec);
        if (role == "computational") comp.push_back(rec);
        else docs.push_back(rec);
    }
    m.computational_sha256 = digest_entries(comp);
    m.documentation_sha256 = digest_entries(docs);
    return m;
}

void write_atomic(const fs::path& target, std::string_view content) {
    if (target.has_parent_path()) fs::create_directories(target.parent_path());
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot open temporary file: " + tmp.string());
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out) throw std::runtime_error("failed writing temporary file: " + tmp.string());
    }
    fs::rename(tmp, target);
}

struct CheckRow {
    std::string name;
    bool passed{false};
    bool has_bound{false};
    double observed_error{0.0};
    double threshold{0.0};
    std::string reason;
};

class Checks {
public:
    bool truth(std::string name, bool condition, std::string reason = "") {
        rows_.push_back({std::move(name), condition, false, 0.0, 0.0, std::move(reason)});
        return condition;
    }

    bool upper(std::string name, double error, double threshold) {
        bool ok = std::isfinite(error) && error <= threshold;
        rows_.push_back({std::move(name), ok, true, error, threshold, ""});
        return ok;
    }

    bool lower(std::string name, double value, double min_threshold) {
        bool ok = std::isfinite(value) && value >= min_threshold;
        rows_.push_back({std::move(name), ok, true, value, min_threshold, ""});
        return ok;
    }

    const std::vector<CheckRow>& rows() const { return rows_; }

    bool all_passed() const {
        if (rows_.empty()) return false;
        for (const auto& r : rows_)
            if (!r.passed) return false;
        return true;
    }

private:
    std::vector<CheckRow> rows_;
};

struct BenchmarkThresholds {
    double metric{2e-12};
    double inverse{2e-11};
    double dg{2e-10};
    double ddg{2e-9};
    double drrr{2e-8};
    double curvature{2e-8};
    double analytic_curvature{2e-9};
    double identity{2e-11};
    double matching{1e-13};
    double extrinsic_matching{1e-13};
    double reference_refinement{1e-18};
    double convergence_ratio_min{3.0};
};

}  // namespace

int main(int argc, char** argv) {
    fs::path project_root = fs::current_path();
    fs::path report_path = project_root / "results" / "rotating_v1.json";
    fs::path manifest_path = project_root / "results" / "source_manifest.json";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) project_root = argv[++i];
        else if (arg == "--output" && i + 1 < argc) report_path = argv[++i];
        else if (arg == "--manifest" && i + 1 < argc) manifest_path = argv[++i];
    }

    const auto start_time = std::chrono::steady_clock::now();
    Checks checks;

    // 1. Exact rational radial certificate on [1.5, 1.8].
    const RationalParameters rp{
        Rational::from_string("1"),
        Rational::from_string("0.875"),
        Rational::from_string("0.5"),
        Rational::from_string("1.5"),
        Rational::from_string("1.8")
    };
    const auto radial_cert = certify_radial_delta(rp);
    const Poly delta_poly = shell_delta(rp);
    const Poly ddelta_poly = derivative(delta_poly);
    // Since d(Delta)/du = 0 at critical points, isolating critical points to width 1e-6
    // determines min Delta to O(1e-12) accuracy while keeping rational size compact.
    const auto droots = isolate_roots(ddelta_poly, Rational(0), Rational(1), Rational(1, 1000000));
    Rational min_delta = std::min(value(delta_poly, Rational(0)), value(delta_poly, Rational(1)));
    for (const auto& [lo, hi] : droots) {
        min_delta = std::min(min_delta, value(delta_poly, (lo + hi) / Rational(2)));
    }

    checks.truth("shell Delta positive inside interval", min_delta > Rational(0),
                 "min Delta = " + std::to_string(min_delta.to_double()));
    checks.truth("shell Delta root-free inside interval", radial_cert.isolated_roots == 0);
    checks.truth("shell Delta Sturm count zero", radial_cert.sturm_roots == 0);
    checks.truth("shell Delta Bernstein lower bound positive", radial_cert.certified,
                 "Bernstein lower bound = " + radial_cert.shell_lower_bound.to_string());
    checks.truth("core 1 - Lambda r^2 / 3 positive", radial_cert.core_factor_lower_bound > Rational(0));
    checks.truth("exterior Delta minimum positive", radial_cert.exterior_delta_minimum > Rational(0));

    // 2. Domain, signature, observer timelikeness, and error rejection checks.
    const Parameters p{Family::toy, 1.0, 0.875, 0.5, 1.5, 1.8};
    const ParametersQ pq{
        Family::toy,
        static_cast<Real>(1),
        static_cast<Real>(7) / static_cast<Real>(8),
        static_cast<Real>(1) / static_cast<Real>(2),
        static_cast<Real>(3) / static_cast<Real>(2),
        static_cast<Real>(9) / static_cast<Real>(5)
    };
    const BenchmarkThresholds thr{};

    double max_inv_id = 0.0;
    bool all_det_neg = true;
    bool all_zamo_timelike = true;
    bool all_gphiphi_pos = true;

    for (double r : {0.25, 0.8, 1.5, 1.65, 1.8, 3.0, 10.0}) {
        for (double th : {0.05, 0.7, 1.2, std::numbers::pi / 2.0, 2.5, std::numbers::pi - 0.05}) {
            const auto res = evaluate(p, r, th);
            if (!std::holds_alternative<Evaluation>(res)) {
                checks.truth("valid domain evaluation succeeds", false, "unexpected domain error");
                continue;
            }
            const auto& e = std::get<Evaluation>(res);
            if (!(e.determinant < 0.0)) all_det_neg = false;
            if (!(e.metric[3][3] > 0.0)) all_gphiphi_pos = false;
            const double zamo_disc = e.metric[0][3] * e.metric[0][3]
                                   - e.metric[0][0] * e.metric[3][3];
            if (!(zamo_disc > 0.0)) all_zamo_timelike = false;
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    double prod = 0.0;
                    for (int k = 0; k < 4; ++k) prod += e.metric[i][k] * e.inverse[k][j];
                    max_inv_id = std::max(max_inv_id, std::abs(prod - (i == j ? 1.0 : 0.0)));
                }
            }
        }
    }
    checks.truth("metric determinant negative across validation box", all_det_neg);
    checks.truth("azimuthal g_phiphi positive off-axis (no closed timelike phi loops)", all_gphiphi_pos);
    checks.truth("ZAMO observer discriminant g_tphi^2 - g_tt g_phiphi > 0", all_zamo_timelike);
    checks.upper("C++ metric inverse identity", max_inv_id, thr.identity);

    checks.truth("reject axis theta=0", std::holds_alternative<Error>(evaluate(p, 1.65, 0.0)));
    checks.truth("reject axis theta=pi", std::holds_alternative<Error>(evaluate(p, 1.65, std::numbers::pi)));
    checks.truth("reject r<=0", std::holds_alternative<Error>(evaluate(p, 0.0, 1.0)));
    checks.truth("reject shell extrapolation at r=3.0",
                 std::holds_alternative<Error>(evaluate(p, 3.0, 1.0, Region::shell)));

    // 3. Independent quad-precision (__float128) vector comparison across core, shell, exterior.
    const Real h_ref = static_cast<Real>(1e-6L);
    double max_g_err = 0.0, max_inv_err = 0.0, max_dg_err = 0.0;
    double max_ddg_err = 0.0, max_drrr_err = 0.0, max_ric_err = 0.0;
    double max_R_err = 0.0, max_K_err = 0.0;

    const std::array<double, 7> r_grid{0.5, 1.2, 1.56, 1.65, 1.74, 2.5, 6.0};
    const std::array<double, 4> th_grid{0.3, 0.9, 1.57, 2.4};
    for (double r : r_grid) {
        for (double th : th_grid) {
            const Evaluation e = std::get<Evaluation>(evaluate(p, r, th));
            const Real rq = static_cast<Real>(r), thq = static_cast<Real>(th);
            const auto dref = derivatives_q(pq, rq, thq, h_ref);
            const auto inv_ref = inverse_q(dref.g);
            const auto cref = curvature_q(dref.g, dref.first, dref.second);

            max_g_err = std::max(max_g_err, normalized_error_matrix(e.metric, dref.g));
            max_inv_err = std::max(max_inv_err, normalized_error_matrix(e.inverse, inv_ref));
            for (int k : {1, 2})
                max_dg_err = std::max(max_dg_err, normalized_error_matrix(e.dg[k], dref.first[k]));
            for (int k : {1, 2})
                for (int l : {1, 2})
                    max_ddg_err = std::max(max_ddg_err, normalized_error_matrix(e.ddg[k][l], dref.second[k][l]));
            max_drrr_err = std::max(max_drrr_err, normalized_error_matrix(e.drrr, dref.drrr));
            max_ric_err = std::max(max_ric_err, normalized_error_matrix(e.ricci, cref.ricci));
            max_R_err = std::max(max_R_err, normalized_error(e.ricci_scalar, cref.ricci_scalar));
            max_K_err = std::max(max_K_err, normalized_error(e.kretschmann, cref.kretschmann));
        }
    }
    checks.upper("C++ Jet vs quad-precision reference metric", max_g_err, thr.metric);
    checks.upper("C++ block inverse vs quad Gauss-Jordan inverse", max_inv_err, thr.inverse);
    checks.upper("C++ Jet vs quad finite-difference dg", max_dg_err, thr.dg);
    checks.upper("C++ Jet vs quad finite-difference ddg", max_ddg_err, thr.ddg);
    checks.upper("C++ Jet vs quad finite-difference drrr", max_drrr_err, thr.drrr);
    checks.upper("C++ curvature vs quad Hessian ricci", max_ric_err, thr.curvature);
    checks.upper("C++ curvature vs quad Hessian ricci_scalar", max_R_err, thr.curvature);
    checks.upper("C++ curvature vs quad Hessian kretschmann", max_K_err, thr.curvature);

    // 4. True one-sided C^0..C^3 and extrinsic curvature K_AB join matching at r_in and r_out.
    double max_join_c0 = 0.0, max_join_c1 = 0.0, max_join_c2 = 0.0, max_join_c3 = 0.0;
    double max_join_extrinsic = 0.0;
    double max_join_quad_ref = 0.0;

    for (double join_r : {p.r_in, p.r_out}) {
        const bool is_inner = (join_r == p.r_in);
        Parameters exact_p = p;
        exact_p.family = is_inner ? Family::de_sitter : Family::kerr;
        const Region exact_reg = is_inner ? Region::core : Region::exterior;
        ParametersQ exact_pq = pq;
        exact_pq.family = is_inner ? Family::de_sitter : Family::kerr;

        for (double th : {0.2, 0.8, std::numbers::pi / 3.0, 1.57, 2.6}) {
            const Evaluation shell_eval = std::get<Evaluation>(evaluate(p, join_r, th, Region::shell));
            const Evaluation region_eval = std::get<Evaluation>(evaluate(p, join_r, th, exact_reg));
            const Evaluation family_eval = std::get<Evaluation>(evaluate(exact_p, join_r, th));
            const auto q_shell = derivatives_q(pq, static_cast<Real>(join_r), static_cast<Real>(th),
                                               h_ref, Region::shell);
            const auto q_exact = derivatives_q(exact_pq, static_cast<Real>(join_r),
                                               static_cast<Real>(th), h_ref);

            max_join_quad_ref = std::max({
                max_join_quad_ref,
                normalized_error_matrix(shell_eval.dg[1], q_exact.first[1]),
                normalized_error_matrix(shell_eval.ddg[1][1], q_exact.second[1][1]),
                normalized_error_matrix(shell_eval.extrinsic_curvature, q_shell.extrinsic_curvature),
                normalized_error_matrix(shell_eval.extrinsic_curvature, q_exact.extrinsic_curvature)
            });

            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    auto rel = [](double a, double b) {
                        return std::abs(a - b) / (1.0 + std::abs(b));
                    };
                    max_join_c0 = std::max({max_join_c0,
                                            rel(shell_eval.metric[i][j], region_eval.metric[i][j]),
                                            rel(shell_eval.metric[i][j], family_eval.metric[i][j])});
                    max_join_c1 = std::max({max_join_c1,
                                            rel(shell_eval.dg[1][i][j], region_eval.dg[1][i][j]),
                                            rel(shell_eval.dg[2][i][j], region_eval.dg[2][i][j]),
                                            rel(shell_eval.dg[1][i][j], family_eval.dg[1][i][j])});
                    max_join_c2 = std::max({max_join_c2,
                                            rel(shell_eval.ddg[1][1][i][j], region_eval.ddg[1][1][i][j]),
                                            rel(shell_eval.ddg[1][2][i][j], region_eval.ddg[1][2][i][j]),
                                            rel(shell_eval.ddg[2][2][i][j], region_eval.ddg[2][2][i][j]),
                                            rel(shell_eval.ddg[1][1][i][j], family_eval.ddg[1][1][i][j])});
                    max_join_c3 = std::max({max_join_c3,
                                            rel(shell_eval.drrr[i][j], region_eval.drrr[i][j]),
                                            rel(shell_eval.drrr[i][j], family_eval.drrr[i][j])});
                    max_join_extrinsic = std::max({max_join_extrinsic,
                                                   rel(shell_eval.extrinsic_curvature[i][j],
                                                       region_eval.extrinsic_curvature[i][j]),
                                                   rel(shell_eval.extrinsic_curvature[i][j],
                                                       family_eval.extrinsic_curvature[i][j])});
                }
            }
        }
    }
    checks.upper("one-sided C0 join matching (shell vs de_sitter/kerr)", max_join_c0, thr.matching);
    checks.upper("one-sided C1 join matching (shell vs de_sitter/kerr)", max_join_c1, thr.matching);
    checks.upper("one-sided C2 join matching (shell vs de_sitter/kerr)", max_join_c2, thr.matching);
    checks.upper("one-sided C3 join matching (shell vs de_sitter/kerr)", max_join_c3, thr.matching);
    checks.upper("outward-normal extrinsic curvature K_AB join matching",
                 max_join_extrinsic, thr.extrinsic_matching);
    checks.upper("shell join vs quad-precision finite-difference reference",
                 max_join_quad_ref, thr.ddg);

    // 5. Convergence & precision refinement checks.
    const Real r_mid = (pq.r_in + pq.r_out) / static_cast<Real>(2);
    const Real th_mid = pi_q() / static_cast<Real>(2);
    const auto d_fine = derivatives_q(pq, r_mid, th_mid, static_cast<Real>(1e-7L), Region::shell);
    std::vector<double> step_errors;
    for (double h_step : {0.004, 0.002, 0.001, 0.0005}) {
        const auto d_coarse = derivatives_q(pq, r_mid, th_mid, static_cast<Real>(h_step), Region::shell);
        double err = 0.0;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                err = std::max(err, static_cast<double>(
                    abs_q(d_coarse.second[1][1][i][j] - d_fine.second[1][1][i][j])
                    / (static_cast<Real>(1) + abs_q(d_fine.second[1][1][i][j]))));
        step_errors.push_back(err);
    }
    double min_conv_ratio = 1e9;
    for (std::size_t i = 0; i + 1 < step_errors.size(); ++i) {
        min_conv_ratio = std::min(min_conv_ratio, step_errors[i] / step_errors[i + 1]);
    }
    checks.lower("finite-difference refinement convergence ratio", min_conv_ratio, thr.convergence_ratio_min);

    const auto d_ref_step2 = derivatives_q(pq, r_mid, th_mid, static_cast<Real>(5e-7L), Region::shell);
    double step_refinement_diff = 0.0;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            step_refinement_diff = std::max(step_refinement_diff, static_cast<double>(
                abs_q(d_fine.first[1][i][j] - d_ref_step2.first[1][i][j])
                / (static_cast<Real>(1) + abs_q(d_fine.first[1][i][j]))));
    checks.upper("quad reference step refinement (1e-7 vs 5e-7)",
                 step_refinement_diff, thr.reference_refinement);

    double trig_id_err = 0.0;
    for (Real x : {static_cast<Real>(0.05L), pi_q() / static_cast<Real>(2), pi_q() - static_cast<Real>(0.05L)}) {
        auto [s, c] = trig_q(x);
        trig_id_err = std::max(trig_id_err, static_cast<double>(abs_q(s * s + c * c - static_cast<Real>(1))));
    }
    checks.upper("quad Taylor trigonometry sin^2 + cos^2 = 1 identity", trig_id_err, 1e-30);

    // 6. Directional NEC violation witness (T_ab k^a k^b = R_ab k^a k^b / (8 pi) for null k^a).
    double min_tkk = 1e9;
    double witness_r = 0.0, witness_null_res = 0.0;
    int witness_axis = 1, witness_sign = 1;
    for (double u : {0.1, 0.25, 0.5, 0.75, 0.9}) {
        const double r = p.r_in + u * (p.r_out - p.r_in);
        const double th = std::numbers::pi / 2.0;
        const Evaluation e = std::get<Evaluation>(evaluate(p, r, th, Region::shell));
        const double omega = -e.metric[0][3] / e.metric[3][3];
        const double alpha = std::sqrt((e.metric[0][3] * e.metric[0][3]
                                        - e.metric[0][0] * e.metric[3][3]) / e.metric[3][3]);
        for (int axis : {1, 2, 3}) {
            for (int sign : {-1, 1}) {
                std::array<double, 4> k{1.0 / alpha, 0.0, 0.0, omega / alpha};
                k[axis] += static_cast<double>(sign) / std::sqrt(e.metric[axis][axis]);
                double null_norm = 0.0, rkk = 0.0;
                for (int i = 0; i < 4; ++i)
                    for (int j = 0; j < 4; ++j) {
                        null_norm += e.metric[i][j] * k[i] * k[j];
                        rkk += e.ricci[i][j] * k[i] * k[j];
                    }
                const double tkk = rkk / (8.0 * std::numbers::pi);
                if (tkk < min_tkk) {
                    min_tkk = tkk;
                    witness_r = r;
                    witness_null_res = std::abs(null_norm);
                    witness_axis = axis;
                    witness_sign = sign;
                }
            }
        }
    }

    // 7. Build deterministic SHA-256 source manifest and write atomic JSON artifacts.
    const SourceManifest manifest = build_manifest(project_root);
    {
        std::ostringstream ms;
        ms << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"computational_sha256\": \"" << manifest.computational_sha256 << "\",\n"
           << "  \"documentation_sha256\": \"" << manifest.documentation_sha256 << "\",\n"
           << "  \"files\": [\n";
        for (std::size_t i = 0; i < manifest.files.size(); ++i) {
            const auto& f = manifest.files[i];
            ms << "    {\"path\": \"" << f.path << "\", \"sha256\": \"" << f.sha256
               << "\", \"size\": " << f.size << ", \"role\": \"" << f.role << "\"}"
               << (i + 1 < manifest.files.size() ? ",\n" : "\n");
        }
        ms << "  ]\n}\n";
        write_atomic(manifest_path, ms.str());
    }

    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start_time).count();
    const bool passed = checks.all_passed();

    {
        std::ostringstream rs;
        rs << std::setprecision(17);
        rs << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"model\": \"toy-c3-v1\",\n"
           << "  \"preset_id\": \"rapid_rotation_project_v1\",\n"
           << "  \"language_runtime\": \"C++20 native (__float128 reference + BigInt/Rational Sturm/Bernstein)\",\n"
           << "  \"computational_source_sha256\": \"" << manifest.computational_sha256 << "\",\n"
           << "  \"passed\": " << (passed ? "true" : "false") << ",\n"
           << "  \"elapsed_seconds\": " << elapsed << ",\n"
           << "  \"radial_certificate\": {\n"
           << "    \"min_delta\": " << min_delta.to_double() << ",\n"
           << "    \"shell_bernstein_lower_bound\": \"" << radial_cert.shell_lower_bound.to_string() << "\",\n"
           << "    \"shell_bernstein_lower_bound_float\": " << radial_cert.shell_lower_bound.to_double() << ",\n"
           << "    \"core_factor_lower_bound\": " << radial_cert.core_factor_lower_bound.to_double() << ",\n"
           << "    \"exterior_delta_minimum\": " << radial_cert.exterior_delta_minimum.to_double() << ",\n"
           << "    \"internal_roots\": " << radial_cert.isolated_roots << ",\n"
           << "    \"sturm_count\": " << radial_cert.sturm_roots << "\n"
           << "  },\n"
           << "  \"independent_comparison\": {\n"
           << "    \"max_metric_error\": " << max_g_err << ",\n"
           << "    \"max_inverse_error\": " << max_inv_err << ",\n"
           << "    \"max_dg_error\": " << max_dg_err << ",\n"
           << "    \"max_ddg_error\": " << max_ddg_err << ",\n"
           << "    \"max_drrr_error\": " << max_drrr_err << ",\n"
           << "    \"max_ricci_error\": " << max_ric_err << ",\n"
           << "    \"max_ricci_scalar_error\": " << max_R_err << ",\n"
           << "    \"max_kretschmann_error\": " << max_K_err << "\n"
           << "  },\n"
           << "  \"matching\": {\n"
           << "    \"max_c0_error\": " << max_join_c0 << ",\n"
           << "    \"max_c1_error\": " << max_join_c1 << ",\n"
           << "    \"max_c2_error\": " << max_join_c2 << ",\n"
           << "    \"max_c3_error\": " << max_join_c3 << ",\n"
           << "    \"max_extrinsic_curvature_error\": " << max_join_extrinsic << ",\n"
           << "    \"max_quad_reference_join_error\": " << max_join_quad_ref << "\n"
           << "  },\n"
           << "  \"convergence\": {\n"
           << "    \"min_step_ratio\": " << min_conv_ratio << ",\n"
           << "    \"step_refinement_diff\": " << step_refinement_diff << ",\n"
           << "    \"trig_identity_error\": " << trig_id_err << "\n"
           << "  },\n"
           << "  \"energy\": {\n"
           << "    \"finding\": \"" << (min_tkk < -1e-20 ? "NEC violation witnessed" : "No violation in sample") << "\",\n"
           << "    \"min_Tkk\": " << min_tkk << ",\n"
           << "    \"witness_r\": " << witness_r << ",\n"
           << "    \"witness_axis\": " << witness_axis << ",\n"
           << "    \"witness_sign\": " << witness_sign << ",\n"
           << "    \"null_residual\": " << witness_null_res << ",\n"
           << "    \"excluded_from_mathematical_acceptance\": true\n"
           << "  },\n"
           << "  \"checks\": [\n";
        const auto& rows = checks.rows();
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];
            rs << "    {\"name\": \"" << r.name << "\", \"passed\": " << (r.passed ? "true" : "false");
            if (r.has_bound) {
                rs << ", \"observed\": " << r.observed_error << ", \"threshold\": " << r.threshold;
            }
            if (!r.reason.empty()) {
                rs << ", \"reason\": \"" << r.reason << "\"";
            }
            rs << "}" << (i + 1 < rows.size() ? ",\n" : "\n");
        }
        rs << "  ]\n}\n";
        write_atomic(report_path, rs.str());
    }

    std::size_t failed_count = 0;
    for (const auto& r : checks.rows())
        if (!r.passed) ++failed_count;
    std::cout << "{\n  \"passed\": " << (passed ? "true" : "false")
              << ",\n  \"checks\": " << checks.rows().size()
              << ",\n  \"failed\": " << failed_count
              << ",\n  \"computational_sha256\": \"" << manifest.computational_sha256 << "\"\n}\n";
    return passed ? 0 : 1;
}
