// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gravastar/geometry/geometry.hpp"
#include <optional>
#include <vector>

namespace gravastar::optics {
using Vector = std::array<double, 4>;
using State = std::array<double, 8>; // x^mu, future-directed p_mu
using Parameters = geometry::Parameters;
struct Frame {
  std::array<Vector, 4> basis{}; // u, e_r, e_theta, e_phi
  double omega{};
};
geometry::Evaluation metric(const Parameters &, const State &);
Frame circular_frame(const geometry::Evaluation &,
                     std::optional<double> omega = {});
State launch(const Parameters &, const Vector &position,
             const std::array<double, 3> &sight,
             std::optional<double> omega = {});
State rhs(const Parameters &,
          const State &); // increasing parameter goes into past
double null_residual(const geometry::Evaluation &, const State &);
double frequency(const State &, const Vector &observer);

struct Settings {
  double absolute_tolerance{1e-10}, relative_tolerance{1e-10};
  double initial_step{.01}, minimum_step{1e-10}, maximum_step{.1};
  double event_tolerance{1e-9}, null_tolerance{1e-6}, maximum_affine{40};
  double inner_radius{.25}, outer_radius{10}, polar_margin{.05};
  std::size_t maximum_attempts{100000};
  std::optional<double> capture_radius;
  bool disk{false}, save_trajectory{true};
  double disk_inner{3}, disk_outer{6};
};
enum class Outcome {
  escaped,
  emitted,
  capture_surface,
  domain_limit,
  affine_limit,
  step_limit,
  invalid_input,
  numerical_failure
};
std::string_view outcome_name(Outcome);
struct Point {
  double affine{};
  State state{};
  double null_error{};
};
struct Crossing {
  std::string surface;
  Point point;
};
struct Trace {
  Outcome outcome{Outcome::invalid_input};
  std::string detail;
  Point final;
  std::size_t accepted{}, rejected{}, rhs_calls{};
  double maximum_null_error{}, energy_error{}, angular_momentum_error{};
  std::vector<Point> trajectory;
  std::vector<Crossing> crossings;
};
Trace trace(const Parameters &, const State &, const Settings & = {});
struct Emission {
  double redshift{}, emitted_frequency{}, linear_intensity{};
};
Emission disk_emission(const Parameters &, const State &hit,
                       double observer_frequency, double disk_inner);
} // namespace gravastar::optics
