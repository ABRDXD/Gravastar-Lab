// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/tools/artifacts.hpp"
#include "gravastar/tools/process.hpp"
#include <iostream>
namespace u = gravastar::tools;
namespace fs = std::filesystem;
using J = u::Json;
using u::run;
void require(bool b, const std::string &message) {
  if (!b)
    throw std::runtime_error(message);
}
bool permitted_source(const fs::path &path) {
  static const std::set<std::string> names{"LICENSE", "NOTICE", ".gitignore",
                                           ".gitkeep", ".clang-format"};
  static const std::set<std::string> extensions{".cpp",  ".hpp", ".cmake",
                                                ".json", ".md",  ".txt"};
  return names.contains(path.filename().string()) ||
         extensions.contains(path.extension().string());
}
void language_policy(const fs::path &root) {
  const std::set<std::string> ignored{".git",   "build",  "results",
                                      ".cache", ".venv",  "__pycache__",
                                      ".idea",  ".vscode"};
  for (fs::recursive_directory_iterator it(root), end; it != end; ++it) {
    const auto path = it->path();
    if (ignored.contains(path.filename().string())) {
      it.disable_recursion_pending();
      continue;
    }
    if (fs::is_symlink(path))
      throw std::runtime_error("source symlink requires review: " +
                               path.string());
    if (it->is_regular_file() && !permitted_source(path))
      throw std::runtime_error("unapproved source format: " + path.string());
  }
  std::cout << "C++20 source-format policy passed\n";
}
int self_test(const fs::path &self) {
  require(permitted_source("code.cpp") && permitted_source("api.hpp") &&
              permitted_source("CMakeLists.txt") &&
              !permitted_source("automation.py") &&
              !permitted_source("automation.sh") &&
              !permitted_source("automation.js") &&
              !permitted_source("automation"),
          "source-format allow list");
  require(
      u::sha256("") ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      "SHA256 empty");
  require(
      u::sha256("abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
      "SHA256 abc");
  require(
      u::sha256(std::string(1000000, 'a')) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
      "SHA256 multi-block vector");
  auto dir = fs::temp_directory_path() /
             ("gravastar tools test " + std::to_string(getpid()));
  fs::create_directories(dir / "src");
  struct Cleanup {
    fs::path p;
    ~Cleanup() {
      std::error_code error;
      fs::remove_all(p, error);
    }
  } cleanup{dir};
  std::ofstream(dir / "src/a.cpp") << "int main(){}\n";
  std::ofstream(dir / "CMakeLists.txt") << "project(test LANGUAGES CXX)\n";
  auto a = u::manifest(dir), b = u::manifest(dir);
  require(a == b, "manifest deterministic");
  fs::create_directories(dir / "results");
  std::ofstream(dir / "results/ignored.json") << "{}";
  require(u::manifest(dir) == a, "outputs excluded");
  std::ofstream(dir / "README.md") << "documentation\n";
  b = u::manifest(dir);
  require(a["computational_sha256"] == b["computational_sha256"] &&
              a["documentation_sha256"] != b["documentation_sha256"],
          "documentation digest separated");
  std::ofstream(dir / "src/a.cpp", std::ios::app) << "// edit\n";
  require(u::manifest(dir)["computational_sha256"] != a["computational_sha256"],
          "source changes detected");
  bool rejected = false;
  fs::create_symlink(dir / "README.md", dir / "src/linked.cpp");
  try {
    (void)u::manifest(dir);
  } catch (const std::exception &) {
    rejected = true;
  }
  require(rejected, "symlink rejected");
  fs::remove(dir / "src/linked.cpp");
  rejected = false;
  try {
    (void)u::result_path(dir, "../escape.json");
  } catch (const std::exception &) {
    rejected = true;
  }
  require(rejected, "output traversal rejected");
  auto report = u::result_path(dir, "results/report.json");
  u::atomic_json(report, {{"passed", true}});
  require(J::parse(u::read_file(report))["passed"] == true, "atomic write");
  require(run({self.string(), "return7"}) == 7,
          "subprocess failure propagated");
  require(J::parse(u::read_file(report))["passed"] == true,
          "subprocess failure preserves report");
  auto arguments = dir / "results/arguments.json";
  require(run({self.string(), "echo-arguments", arguments.string(),
               "space path", "literal $(no_shell) `text`"}) == 0,
          "literal argument subprocess");
  require(J::parse(u::read_file(arguments)) ==
              J::array({"space path", "literal $(no_shell) `text`"}),
          "argument array integrity");
  std::cout << "C++ tool migration checks passed\n";
  return 0;
}
int main(int argc, char **argv) {
  try {
    std::string command = argc > 1 ? argv[1] : "help";
    fs::path self = fs::canonical("/proc/self/exe");
    if (command == "return7")
      return 7;
    if (command == "echo-arguments") {
      if (argc < 3)
        return 2;
      J a = J::array();
      for (int i = 3; i < argc; ++i)
        a.push_back(argv[i]);
      u::atomic_json(argv[2], a);
      return 0;
    }
    if (command == "self-test")
      return self_test(self);
    if (command == "help" || command == "--help") {
      std::cout
          << "gravastar_tools manifest [--root PATH] [--output "
             "results/FILE.json] [--verify results/FILE.json]\n"
             "gravastar_tools workflow [--root PATH] [--preset "
             "headless-release|headless-debug|sanitizers] [--jobs N] [--config "
             "benchmarks/FILE.json] [--output results/FILE.json]\n";
      std::cout << "gravastar_tools language-policy [--root PATH]\n";
      return 0;
    }
    fs::path root = GRAVASTAR_SOURCE_ROOT,
             output = command == "manifest" ? "results/source_manifest.json"
                                            : "results/rotating_v1.json",
             verify, config = "benchmarks/rotating_v1.json";
    std::string preset = "headless-release", jobs = "4";
    for (int i = 2; i < argc; ++i) {
      std::string flag = argv[i];
      if (i + 1 >= argc)
        throw std::invalid_argument("missing option value");
      std::string value = argv[++i];
      if (flag == "--root")
        root = value;
      else if (flag == "--output")
        output = value;
      else if (flag == "--verify")
        verify = value;
      else if (flag == "--config")
        config = value;
      else if (flag == "--preset")
        preset = value;
      else if (flag == "--jobs")
        jobs = value;
      else
        throw std::invalid_argument("unknown option " + flag);
    }
    root = fs::canonical(root);
    if (command == "language-policy") {
      language_policy(root);
      return 0;
    }
    output = u::result_path(root, output);
    if (command == "manifest") {
      auto current = u::manifest(root);
      if (!verify.empty()) {
        if (J::parse(u::read_file(u::result_path(root, verify))) != current) {
          std::cerr << "Source manifest differs\n";
          return 1;
        }
        std::cout << "Source manifest verified\n";
      } else {
        u::atomic_json(output, current);
        std::cout << output << '\n';
      }
      return 0;
    }
    if (command != "workflow")
      throw std::invalid_argument("unknown command");
    if (preset != "headless-release" && preset != "headless-debug" &&
        preset != "sanitizers")
      throw std::invalid_argument("unknown build preset");
    std::size_t used{};
    int nj = std::stoi(jobs, &used);
    if (used != jobs.size() || nj < 1 || nj > 256)
      throw std::invalid_argument("jobs must be 1..256");
    auto baseline = u::manifest(root);
    fs::current_path(root);
    for (const std::vector<std::string> &args :
         std::vector<std::vector<std::string>>{
             {"cmake", "--preset", preset},
             {"cmake", "--build", "--preset", preset, "--parallel", jobs},
             {"ctest", "--preset", preset, "--output-on-failure"}}) {
      int status = run(args);
      if (status)
        return status;
    }
    if (u::manifest(root)["computational_sha256"] !=
        baseline["computational_sha256"])
      throw std::runtime_error("sources changed during build/test; rerun");
    return run({(root / "build" / preset / "gravastar_validate").string(),
                "--root", root.string(), "--config", config.string(),
                "--output", output.string()});
  } catch (const std::exception &e) {
    std::cerr << "project tool error: " << e.what() << '\n';
    return 2;
  }
}
