// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>
namespace gravastar::tools {
using Json = nlohmann::json;
namespace fs = std::filesystem;
inline std::string sha256(const std::string &input) {
  constexpr std::array<std::uint32_t, 64> k{
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
      0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
      0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
      0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  std::array<std::uint32_t, 8> h{0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                 0xa54ff53a, 0x510e527f, 0x9b05688c,
                                 0x1f83d9ab, 0x5be0cd19};
  std::vector<std::uint8_t> data(input.begin(), input.end());
  auto bits = static_cast<std::uint64_t>(data.size()) * 8;
  data.push_back(0x80);
  while (data.size() % 64 != 56)
    data.push_back(0);
  for (int i = 7; i >= 0; --i)
    data.push_back((bits >> (8 * i)) & 255);
  for (std::size_t pos = 0; pos < data.size(); pos += 64) {
    std::array<std::uint32_t, 64> w{};
    for (int i = 0; i < 16; ++i)
      for (int j = 0; j < 4; ++j)
        w[i] = (w[i] << 8) | data[pos + 4 * i + j];
    for (int i = 16; i < 64; ++i) {
      auto s0 =
          std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      auto s1 =
          std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    auto [a, b, c, d, e, f, g, hh] = h;
    for (int i = 0; i < 64; ++i) {
      auto s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
      auto ch = (e & f) ^ ((~e) & g);
      auto t1 = hh + s1 + ch + k[i] + w[i];
      auto s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
      auto t2 = s0 + ((a & b) ^ (a & c) ^ (b & c));
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  }
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (auto v : h)
    out << std::setw(8) << v;
  return out.str();
}
inline std::string read_file(const fs::path &p) {
  std::ifstream in(p, std::ios::binary);
  if (!in)
    throw std::runtime_error("cannot read " + p.string());
  std::string data((std::istreambuf_iterator<char>(in)), {});
  if (in.bad())
    throw std::runtime_error("read failed " + p.string());
  return data;
}
inline Json manifest(const fs::path &project) {
  auto root = fs::canonical(project);
  std::map<std::string, std::string> selected;
  const std::set<std::string> excluded{"build",  "results", ".git",
                                       ".cache", ".venv",   "__pycache__"};
  auto take = [&](const fs::path &p, const std::string &scope) {
    if (fs::is_symlink(p))
      throw std::runtime_error("symlink in source inventory: " + p.string());
    if (fs::is_regular_file(p))
      selected[fs::relative(p, root).generic_string()] = scope;
  };
  for (const auto &name : {"CMakeLists.txt", "CMakePresets.json"})
    take(root / name, "computational");
  for (const auto &name : {"AGENTS.md", "README.md", "README.txt", "PLAN.md",
                           "LICENSE", "NOTICE", "THIRD_PARTY.md"})
    take(root / name, "documentation");
  for (const auto &name :
       {"include", "src", "reference", "tests", "benchmarks", "tools",
        "schemas", "scenes", "shaders", "plugins", "third_party", "docs"}) {
    auto p = root / name;
    if (fs::is_symlink(p))
      throw std::runtime_error("symlinked source directory");
    if (!fs::exists(p))
      continue;
    for (fs::recursive_directory_iterator it(p), end; it != end; ++it) {
      auto path = it->path();
      if (excluded.contains(path.filename().string())) {
        it.disable_recursion_pending();
        continue;
      }
      if (fs::is_symlink(path))
        throw std::runtime_error("symlinked source entry: " + path.string());
      if (path.extension() == ".pyc" || path.extension() == ".o" ||
          path.extension() == ".a" || path.extension() == ".so")
        continue;
      take(path,
           std::string(name) == "docs" ? "documentation" : "computational");
    }
  }
  Json files = Json::array(), code = Json::array(), docs = Json::array();
  for (const auto &[path, scope] : selected) {
    auto data = read_file(root / path);
    Json e{{"path", path},
           {"scope", scope},
           {"bytes", data.size()},
           {"sha256", sha256(data)}};
    files.push_back(e);
    (scope == "computational" ? code : docs).push_back(e);
  }
  if (code.empty())
    throw std::runtime_error("no computational sources");
  return {{"schema_version", 1},
          {"algorithm", "sha256"},
          {"digest_encoding",
           "UTF-8 compact JSON of sorted records and object keys"},
          {"computational_sha256", sha256(code.dump())},
          {"documentation_sha256", sha256(docs.dump())},
          {"source_sha256", sha256(files.dump())},
          {"file_count", files.size()},
          {"files", files}};
}
inline fs::path result_path(const fs::path &root, const fs::path &input) {
  auto base = fs::weakly_canonical(root / "results"),
       p = fs::weakly_canonical(input.is_absolute() ? input : root / input);
  auto relative = p.lexically_relative(base);
  if (relative.empty() || relative == "." || *relative.begin() == "..")
    throw std::invalid_argument("output must be within project results/");
  return p;
}
// Linux durability: exclusive temporary file, fsync, close, atomic rename.
inline void atomic_bytes(const fs::path &path, const std::string &bytes) {
  fs::create_directories(path.parent_path());
  std::string pattern =
      (path.parent_path() / ("." + path.filename().string() + ".XXXXXX"))
          .string();
  std::vector<char> name(pattern.begin(), pattern.end());
  name.push_back('\0');
  int fd = mkstemp(name.data());
  if (fd < 0)
    throw std::runtime_error("cannot create atomic temporary file");
  try {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
      auto n = write(fd, bytes.data() + offset, bytes.size() - offset);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        throw std::runtime_error("report write failed");
      offset += static_cast<std::size_t>(n);
    }
    if (fsync(fd) != 0)
      throw std::runtime_error("report flush failed");
    int closed = close(fd);
    fd = -1;
    if (closed != 0)
      throw std::runtime_error("report close failed");
    fs::rename(name.data(), path);
    int dir = open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
    if (dir >= 0) {
      (void)fsync(dir);
      (void)close(dir);
    }
  } catch (...) {
    if (fd >= 0)
      close(fd);
    fs::remove(name.data());
    throw;
  }
}
inline void atomic_json(const fs::path &path, const Json &value) {
  atomic_bytes(path, value.dump(2) + "\n");
}
} // namespace gravastar::tools
