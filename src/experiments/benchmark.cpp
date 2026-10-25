// SPDX-License-Identifier: MIT
#include "gravastar/geometry/geometry.hpp"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using namespace gravastar::geometry;

template <typename Array> void json_array(const Array &array) {
  std::cout << '[';
  bool first = true;
  for (const auto &entry : array) {
    if (!first)
      std::cout << ',';
    first = false;
    if constexpr (std::is_same_v<std::decay_t<decltype(entry)>, double>)
      std::cout << entry;
    else
      json_array(entry);
  }
  std::cout << ']';
}

void error_json(const Error &error) {
  // Error strings are fixed internal literals, never unescaped user input.
  std::cout << "{\"schema_version\":1,\"error\":{\"code\":\"" << error.code
            << "\",\"message\":\"" << error.message << "\"}}\n";
}

bool parse_double(const std::string &text, double &value) {
  const auto result =
      std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc() && result.ptr == text.data() + text.size();
}

bool evaluate_record(const std::vector<std::string> &fields) {
  if (fields.size() != 8 && fields.size() != 9) {
    error_json({"invalid_request",
                "expected FAMILY M a Lambda r_in r_out r theta [REGION]"});
    return false;
  }
  const auto family = parse_family(fields[0]);
  if (const auto *error = std::get_if<std::string>(&family)) {
    error_json({"invalid_request", *error});
    return false;
  }
  std::array<double, 7> numbers{};
  for (std::size_t i = 0; i < numbers.size(); ++i) {
    if (!parse_double(fields[i + 1], numbers[i])) {
      error_json({"invalid_request",
                  "numeric fields must be complete floating-point literals"});
      return false;
    }
  }
  const Parameters p{std::get<Family>(family),
                     numbers[0],
                     numbers[1],
                     numbers[2],
                     numbers[3],
                     numbers[4]};
  Region region = Region::automatic;
  const std::string region_text = fields.size() == 9 ? fields[8] : "automatic";
  if (region_text == "core")
    region = Region::core;
  else if (region_text == "shell")
    region = Region::shell;
  else if (region_text == "exterior")
    region = Region::exterior;
  else if (region_text != "automatic") {
    error_json({"invalid_request", "unknown region"});
    return false;
  }
  const auto result = evaluate(p, numbers[5], numbers[6], region);
  if (const auto *error = std::get_if<Error>(&result)) {
    error_json(*error);
    return false;
  }
  const auto &e = std::get<Evaluation>(result);
  std::cout << "{\"schema_version\":1,\"region\":\"" << region_text
            << "\",\"family\":\"" << family_name(p.family)
            << "\",\"parameters\":{\"M\":" << p.mass << ",\"a\":" << p.spin
            << ",\"Lambda\":" << p.cosmological_constant
            << ",\"r_in\":" << p.r_in << ",\"r_out\":" << p.r_out
            << "},\"coordinates\":{\"r\":" << numbers[5]
            << ",\"theta\":" << numbers[6] << "},\"mu\":" << e.mu
            << ",\"lambda\":" << e.lambda << ",\"Sigma\":" << e.sigma
            << ",\"Delta\":" << e.delta << ",\"Delta_theta\":" << e.delta_theta
            << ",\"Xi\":" << e.xi << ",\"metric\":";
  json_array(e.metric);
  std::cout << ",\"inverse\":";
  json_array(e.inverse);
  std::cout << ",\"dg\":";
  json_array(e.dg);
  std::cout << ",\"ddg\":";
  json_array(e.ddg);
  std::cout << ",\"drrr\":";
  json_array(e.drrr);
  std::cout << ",\"determinant\":" << e.determinant << ",\"ricci\":";
  json_array(e.ricci);
  std::cout << ",\"ricci_scalar\":" << e.ricci_scalar
            << ",\"kretschmann\":" << e.kretschmann << "}\n";
  return true;
}

void help() {
  std::cout
      << "GravastarLab headless rotating geometry evaluator, schema 1\n"
         "Usage:\n"
         "  gravastar_benchmark evaluate FAMILY M a Lambda r_in r_out r theta "
         "[REGION]\n"
         "  gravastar_benchmark batch\n"
         "Families: toy, kerr, de_sitter, flat\n"
         "Regions (toy only): automatic, core, shell, exterior; closed "
         "intervals, no extrapolation.\n"
         "Batch: one whitespace-separated evaluation record per input line; "
         "JSONL output.\n"
         "Indices: t,r,theta,phi. dg[k][i][j], ddg[k][l][i][j], drrr[i][j].\n"
         "Units: G=c=1; signature (-,+,+,+). No axis or central continuation.\n"
         "Status: 0 all records succeeded; 1 any record failed; 2 invalid "
         "invocation.\n";
}
} // namespace

int main(int argc, char **argv) {
  std::cout << std::setprecision(17);
  if (argc == 2 && std::string(argv[1]) == "--help") {
    help();
    return 0;
  }
  if (argc >= 2 && std::string(argv[1]) == "evaluate")
    return evaluate_record({argv + 2, argv + argc}) ? 0 : 1;
  if (argc == 2 && std::string(argv[1]) == "batch") {
    std::string line;
    bool success = true;
    while (std::getline(std::cin, line)) {
      std::istringstream input(line);
      std::vector<std::string> fields;
      std::string token;
      while (input >> token)
        fields.push_back(token);
      success = evaluate_record(fields) && success;
    }
    if (std::cin.bad()) {
      error_json({"input_failure", "failed to read batch input"});
      return 1;
    }
    return success ? 0 : 1;
  }
  help();
  return 2;
}
