// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/optics/ray.hpp"
#include "gravastar/optics/chart.hpp"
#include "gravastar/optics/core_chart.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace gravastar::optics {
namespace {
bool finite(const State &y) {
  return std::all_of(y.begin(), y.end(),
                     [](double x) { return std::isfinite(x); });
}
Vector velocity(const geometry::Evaluation &g, const State &y) {
  Vector v{};
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      v[a] += g.inverse[a][b] * y[b + 4];
  return v;
}
struct Step {
  State next{}, error{};
};
double residual(const Parameters &p, const State &y, Chart chart) {
  if (chart == Chart::core_cartesian)
    return core_null_residual(core_metric(p, y), y);
  return chart == Chart::boyer_lindquist
             ? null_residual(metric(p, y), y)
             : angular_null_residual(angular_metric(p, y), y);
}
Step step(const Parameters &p, const State &y, double h, std::size_t &calls,
          Chart chart) {
  // Dormand-Prince 5(4) tableau; autonomous Hamiltonian system.
  constexpr double a[7][7]{
      {},
      {1. / 5},
      {3. / 40, 9. / 40},
      {44. / 45, -56. / 15, 32. / 9},
      {19372. / 6561, -25360. / 2187, 64448. / 6561, -212. / 729},
      {9017. / 3168, -355. / 33, 46732. / 5247, 49. / 176, -5103. / 18656},
      {35. / 384, 0, 500. / 1113, 125. / 192, -2187. / 6784, 11. / 84}};
  constexpr double low[7]{
      5179. / 57600,    0,           7571. / 16695, 393. / 640,
      -92097. / 339200, 187. / 2100, 1. / 40};
  std::array<State, 7> k{};
  for (int s = 0; s < 7; ++s) {
    State v = y;
    for (int i = 0; i < 8; ++i)
      for (int j = 0; j < s; ++j)
        v[i] += h * a[s][j] * k[j][i];
    ++calls;
    k[s] = chart == Chart::boyer_lindquist  ? rhs(p, v)
           : chart == Chart::core_cartesian ? core_rhs(p, v)
                                            : angular_rhs(p, v);
  }
  Step out;
  out.next = y;
  for (int i = 0; i < 8; ++i)
    for (int j = 0; j < 7; ++j) {
      out.next[i] += h * a[6][j] * k[j][i];
      out.error[i] += h * (a[6][j] - low[j]) * k[j][i];
    }
  if (!finite(out.next) || !finite(out.error))
    throw std::runtime_error("nonfinite Runge-Kutta step");
  return out;
}
double error_norm(const Step &s, const State &y, const Settings &c) {
  double e = 0;
  for (int i = 0; i < 8; ++i)
    e = std::max(e, std::abs(s.error[i]) /
                        (c.absolute_tolerance +
                         c.relative_tolerance *
                             std::max(std::abs(y[i]), std::abs(s.next[i]))));
  return e;
}
enum class Transition { none, enter_core, exit_core };
struct Surface {
  int axis; // -1: angular q; -2: Cartesian oblate ellipsoid.
  double value;
  std::string name;
  std::optional<Outcome> terminal;
  Transition transition{Transition::none};
};
double surface_value(const Parameters &p, const State &y,
                     const Surface &surface) {
  if (surface.axis == -2) {
    const double k =
        p.family == geometry::Family::flat ? 0 : p.cosmological_constant / 3;
    const double a2 = p.spin * p.spin, r2 = surface.value * surface.value;
    return (1 + k * a2) * (y[1] * y[1] + y[2] * y[2]) / (r2 + a2) +
           y[3] * y[3] / r2 - 1;
  }
  return (surface.axis == -1 ? y[2] * y[2] + y[3] * y[3] : y[surface.axis]) -
         surface.value;
}
bool brackets(double a, double b) {
  return (a < 0 && b >= 0) || (a > 0 && b <= 0);
}
Point refine(const Parameters &p, const State &y, const State &end, double s,
             double h, const Surface &surface, const Settings &c,
             std::size_t &calls, Chart chart) {
  double lo = 0, hi = h, start = surface_value(p, y, surface);
  State best = end;
  for (int i = 0; i < 80 && hi - lo > c.event_tolerance; ++i) {
    double mid = (lo + hi) / 2;
    auto trial = step(p, y, mid, calls, chart);
    if (error_norm(trial, y, c) > 1)
      throw std::runtime_error("event substep fails integration tolerance");
    double value = surface_value(p, trial.next, surface);
    if (brackets(start, value)) {
      hi = mid;
      best = trial.next;
    } else
      lo = mid;
  }
  if (hi - lo > c.event_tolerance)
    throw std::runtime_error("event refinement failed");
  Point point{s + hi, best, residual(p, best, chart), chart};
  if (!std::isfinite(point.null_error) || point.null_error > c.null_tolerance)
    throw std::runtime_error("refined event exceeds null constraint tolerance");
  return point;
}
void validate(const Parameters &p, const State &y, const Settings &c,
              Chart chart) {
  if (chart != Chart::boyer_lindquist && chart != Chart::north &&
      chart != Chart::south && chart != Chart::core_cartesian)
    throw std::invalid_argument("unknown tracing chart");
  for (double x :
       {c.absolute_tolerance, c.relative_tolerance, c.initial_step,
        c.minimum_step, c.maximum_step, c.event_tolerance, c.null_tolerance,
        c.maximum_affine, c.inner_radius, c.outer_radius, c.polar_margin})
    if (!std::isfinite(x) || x <= 0)
      throw std::invalid_argument("settings must be finite and positive");
  if (c.minimum_step > c.initial_step || c.initial_step > c.maximum_step ||
      c.inner_radius >= c.outer_radius ||
      c.polar_margin >= std::numbers::pi / 2 || c.maximum_attempts == 0 ||
      c.maximum_attempts > 10000000)
    throw std::invalid_argument("invalid integration limits");
  if (chart != Chart::boyer_lindquist &&
      (!std::isfinite(c.angular_switch_q) || c.angular_switch_q <= 1 ||
       c.angular_switch_q > 2))
    throw std::invalid_argument("angular switch threshold must satisfy 1<q<=2");
  if (c.continue_core) {
    if (chart == Chart::boyer_lindquist || p.family == geometry::Family::kerr ||
        !std::isfinite(c.core_entry_radius) ||
        !std::isfinite(c.core_exit_radius) ||
        c.core_entry_radius <= c.inner_radius ||
        c.core_exit_radius <= c.core_entry_radius ||
        c.core_exit_radius >= p.r_in || c.core_exit_radius >= c.outer_radius ||
        (c.disk && c.disk_inner <= c.core_exit_radius))
      throw std::invalid_argument(
          "core continuation requires angular charts, exact core overlap, and "
          "disk outside the core patch");
    const double k =
        p.family == geometry::Family::flat ? 0 : p.cosmological_constant / 3;
    const double r2 = c.core_exit_radius * c.core_exit_radius,
                 a2 = p.spin * p.spin;
    if (!(1 - k * std::max(r2, (r2 + a2) / (1 + k * a2)) >
          64 * std::numeric_limits<double>::epsilon()))
      throw std::invalid_argument(
          "core overlap must remain inside the static horizon");
  } else if (chart == Chart::core_cartesian)
    throw std::invalid_argument("Cartesian core launch requires continue_core");
  if (!finite(y) ||
      (chart == Chart::core_cartesian
           ? core_oblate_radius(p, y) >= c.core_exit_radius
           : (y[1] <= (c.continue_core ? 0 : c.inner_radius) ||
              y[1] >= c.outer_radius)) ||
      (chart == Chart::boyer_lindquist &&
       (y[2] <= c.polar_margin || y[2] >= std::numbers::pi - c.polar_margin)))
    throw std::invalid_argument(
        "initial state outside the open tracing domain");
  if (c.disk && (!std::isfinite(c.disk_inner) || !std::isfinite(c.disk_outer) ||
                 c.disk_inner < c.inner_radius ||
                 c.disk_outer > c.outer_radius || c.disk_inner >= c.disk_outer))
    throw std::invalid_argument("invalid disk annulus");
  if (c.capture_radius) {
    double r = *c.capture_radius;
    if (p.family != geometry::Family::kerr || p.mass <= 0 ||
        std::abs(p.spin) >= p.mass || !std::isfinite(r) ||
        r <= p.mass + std::sqrt(p.mass * p.mass - p.spin * p.spin) ||
        r <= c.inner_radius || r >= y[1])
      throw std::invalid_argument("capture surface requires subextremal Kerr "
                                  "exterior and exterior launch");
  }
  double null, nu;
  if (chart == Chart::boyer_lindquist) {
    const auto g = metric(p, y);
    null = null_residual(g, y);
    nu = frequency(y, circular_frame(g).basis[0]);
  } else if (chart == Chart::core_cartesian) {
    const auto g = core_metric(p, y);
    null = core_null_residual(g, y);
    nu = core_normal_frequency(g, y);
  } else {
    const auto g = angular_metric(p, y);
    null = angular_null_residual(g, y);
    nu = angular_normal_frequency(g, y);
  }
  if (!std::isfinite(null) || null > c.null_tolerance || nu <= 0)
    throw std::invalid_argument(
        "initial momentum must be future-directed and null");
}
} // namespace

geometry::Evaluation metric(const Parameters &p, const State &y) {
  auto r = geometry::evaluate(p, y[1], y[2]);
  if (auto *e = std::get_if<geometry::Error>(&r))
    throw std::runtime_error(e->code + ": " + e->message);
  auto g = std::get<geometry::Evaluation>(r);
  if (g.metric[1][1] <= 0 || g.metric[2][2] <= 0 || g.metric[3][3] <= 0)
    throw std::runtime_error(
        "optics requires the exterior-type Lorentzian chart");
  return g;
}
Frame circular_frame(const geometry::Evaluation &g,
                     std::optional<double> omega) {
  const auto &m = g.metric;
  Frame f;
  f.omega = omega.value_or(-m[0][3] / m[3][3]);
  double norm = m[0][0] + 2 * f.omega * m[0][3] + f.omega * f.omega * m[3][3];
  double determinant = m[0][3] * m[0][3] - m[0][0] * m[3][3];
  if (!std::isfinite(norm) || norm >= 0 || !std::isfinite(determinant) ||
      determinant <= 0 || !std::isfinite(m[1][1]) || m[1][1] <= 0 ||
      !std::isfinite(m[2][2]) || m[2][2] <= 0 || !std::isfinite(m[3][3]) ||
      m[3][3] <= 0)
    throw std::invalid_argument(
        "circular observer is not timelike in this chart");
  f.basis[0] = {1 / std::sqrt(-norm), 0, 0, f.omega / std::sqrt(-norm)};
  f.basis[1][1] = 1 / std::sqrt(m[1][1]);
  f.basis[2][2] = 1 / std::sqrt(m[2][2]);
  const double ut = f.basis[0][0] * (m[0][0] + f.omega * m[0][3]);
  const double up = f.basis[0][0] * (m[0][3] + f.omega * m[3][3]);
  f.basis[3] = {up / std::sqrt(determinant), 0, 0,
                -ut / std::sqrt(determinant)};
  return f;
}
State launch(const Parameters &p, const Vector &x,
             const std::array<double, 3> &n, std::optional<double> omega) {
  State y{};
  std::copy(x.begin(), x.end(), y.begin());
  if (!finite(y))
    throw std::invalid_argument("nonfinite observer position");
  double length = std::hypot(n[0], n[1], n[2]);
  if (!std::isfinite(length) || length == 0)
    throw std::invalid_argument("invalid sight vector");
  auto g = metric(p, y);
  auto f = circular_frame(g, omega);
  Vector v = f.basis[0];
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 3; ++j)
      v[i] -= n[j] / length * f.basis[j + 1][i];
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      y[i + 4] += g.metric[i][j] * v[j];
  return y;
}
State rhs(const Parameters &p, const State &y) {
  if (!finite(y))
    throw std::invalid_argument("nonfinite ray state");
  auto g = metric(p, y);
  auto v = velocity(g, y);
  State out{};
  for (int i = 0; i < 4; ++i)
    out[i] = -v[i];
  for (int k : {1, 2})
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b)
        out[k + 4] -= .5 * g.dg[k][a][b] * v[a] * v[b];
  if (!finite(out))
    throw std::runtime_error("nonfinite Hamiltonian derivative");
  return out;
}
double null_residual(const geometry::Evaluation &g, const State &y) {
  double sum = 0, scale = 0;
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b) {
      double term = g.inverse[a][b] * y[a + 4] * y[b + 4];
      sum += term;
      scale += std::abs(term);
    }
  if (!std::isfinite(scale) || scale == 0)
    return std::numeric_limits<double>::infinity();
  return std::abs(sum) / scale;
}
double frequency(const State &y, const Vector &observer) {
  double nu = 0;
  for (int i = 0; i < 4; ++i)
    nu -= y[i + 4] * observer[i];
  if (!std::isfinite(nu))
    throw std::invalid_argument("nonfinite photon frequency");
  return nu;
}
std::string_view outcome_name(Outcome x) {
  switch (x) {
  case Outcome::escaped:
    return "escaped";
  case Outcome::emitted:
    return "emitted";
  case Outcome::capture_surface:
    return "capture_surface";
  case Outcome::domain_limit:
    return "domain_limit";
  case Outcome::affine_limit:
    return "affine_limit";
  case Outcome::step_limit:
    return "step_limit";
  case Outcome::invalid_input:
    return "invalid_input";
  case Outcome::numerical_failure:
    return "numerical_failure";
  }
  return "invalid_outcome";
}
static Trace trace_impl(const Parameters &p, const State &initial,
                        const Settings &c, Chart initial_chart) {
  Trace out;
  out.final.state = initial;
  out.final.chart = initial_chart;
  try {
    validate(p, initial, c, initial_chart);
  } catch (const std::exception &e) {
    out.detail = e.what();
    return out;
  }
  std::vector<Surface> surfaces{
      {1, c.outer_radius, "escape", Outcome::escaped}};
  if (c.continue_core)
    surfaces.push_back(
        {1, c.core_entry_radius, "core_entry", {}, Transition::enter_core});
  else
    surfaces.push_back(
        {1, c.inner_radius, "inner_chart_limit", Outcome::domain_limit});
  const std::vector<Surface> core_surfaces{
      {-2, c.core_exit_radius, "core_exit", {}, Transition::exit_core}};
  if (initial_chart == Chart::boyer_lindquist) {
    surfaces.push_back(
        {2, c.polar_margin, "north_chart_limit", Outcome::domain_limit});
    surfaces.push_back({2, std::numbers::pi - c.polar_margin,
                        "south_chart_limit", Outcome::domain_limit});
  }
  if (c.capture_radius)
    surfaces.push_back(
        {1, *c.capture_radius, "Kerr_capture_proxy", Outcome::capture_surface});
  if (c.disk)
    surfaces.push_back(
        initial_chart == Chart::boyer_lindquist
            ? Surface{2, std::numbers::pi / 2, "disk", Outcome::emitted}
            : Surface{-1, 1, "disk", Outcome::emitted});
  if (p.family == geometry::Family::toy) {
    surfaces.push_back({1, p.r_in, "inner_shell_join", {}});
    surfaces.push_back({1, p.r_out, "outer_shell_join", {}});
  }
  auto monitor = [&](const Point &q) {
    out.maximum_null_error = std::max(out.maximum_null_error, q.null_error);
    const double initial_pt = stationary_momentum(p, initial, initial_chart);
    out.energy_error = std::max(
        out.energy_error,
        std::abs(stationary_momentum(p, q.state, q.chart) - initial_pt) /
            (1 + std::abs(initial_pt)));
    const double initial_lz = axial_momentum(initial, initial_chart);
    out.angular_momentum_error =
        std::max(out.angular_momentum_error,
                 std::abs(axial_momentum(q.state, q.chart) - initial_lz) /
                     (1 + std::abs(initial_lz)));
  };
  auto record = [&](const Point &q) {
    out.final = q;
    monitor(q);
    if (c.save_trajectory)
      out.trajectory.push_back(q);
  };
  auto switch_patch = [&](Point &q) {
    if (q.chart == Chart::boyer_lindquist || q.chart == Chart::core_cartesian ||
        q.state[2] * q.state[2] + q.state[3] * q.state[3] < c.angular_switch_q)
      return;
    if (!std::isfinite(q.null_error) || q.null_error > c.null_tolerance)
      throw std::runtime_error(
          "pre-transition state exceeds null constraint tolerance");
    monitor(q);
    const Chart next = q.chart == Chart::north ? Chart::south : Chart::north;
    q.state = transform_state(q.state, q.chart, next);
    q.chart = next;
    q.null_error = residual(p, q.state, q.chart);
    if (!std::isfinite(q.null_error) || q.null_error > c.null_tolerance)
      throw std::runtime_error(
          "chart transition exceeds null constraint tolerance");
    ++out.chart_switches;
  };
  auto change_core = [&](Point &q, Transition transition) {
    // Prepare the candidate before publishing its crossing. A failed transform
    // must not publish half of an entry/exit transaction.
    if (transition == Transition::enter_core) {
      q.state = to_core(p, q.state, q.chart);
      q.chart = Chart::core_cartesian;
    } else if (transition == Transition::exit_core) {
      const Chart destination = q.state[3] >= 0 ? Chart::north : Chart::south;
      q.state = from_core(p, q.state, destination);
      q.chart = destination;
    }
    q.null_error = residual(p, q.state, q.chart);
    if (!std::isfinite(q.null_error) || q.null_error > c.null_tolerance)
      throw std::runtime_error(
          "core transition exceeds null constraint tolerance");
  };
  double h = c.initial_step;
  try {
    Point start{0, initial, residual(p, initial, initial_chart), initial_chart};
    if (c.continue_core && start.chart != Chart::core_cartesian &&
        start.state[1] <= c.core_entry_radius) {
      monitor(start);
      change_core(start, Transition::enter_core);
      ++out.chart_switches;
    }
    switch_patch(start);
    record(start);
    for (std::size_t attempt = 0; attempt < c.maximum_attempts; ++attempt) {
      const State y = out.final.state;
      const Chart chart = out.final.chart;
      const double s = out.final.affine;
      const double remaining = c.maximum_affine - s;
      if (remaining <= 8 * std::numeric_limits<double>::epsilon() *
                           std::max(1., c.maximum_affine)) {
        out.outcome = Outcome::affine_limit;
        out.detail = "affine budget exhausted; fate unresolved";
        return out;
      }
      h = std::min(h, remaining);
      Step trial;
      try {
        trial = step(p, y, h, out.rhs_calls, chart);
      } catch (const std::exception &) {
        ++out.rejected;
        if (h <= c.minimum_step)
          throw;
        h = std::max(c.minimum_step, h / 2);
        continue;
      }
      double error = error_norm(trial, y, c);
      double factor =
          error == 0 ? 5 : std::clamp(.9 * std::pow(error, -.2), .2, 5.);
      if (error > 1) {
        ++out.rejected;
        if (h <= c.minimum_step)
          throw std::runtime_error("minimum step cannot meet tolerance");
        h = std::max(c.minimum_step, h * std::min(.9, factor));
        continue;
      }
      Point next{s + h, trial.next, residual(p, trial.next, chart), chart};
      std::vector<std::pair<Surface, Point>> hits;
      std::optional<Outcome> terminal;
      Transition transition = Transition::none;
      std::size_t publish_count = 0;
      try {
        const auto &active =
            chart == Chart::core_cartesian ? core_surfaces : surfaces;
        for (const auto &surface : active)
          if (brackets(surface_value(p, y, surface),
                       surface_value(p, trial.next, surface))) {
            auto hit = refine(p, y, trial.next, s, h, surface, c, out.rhs_calls,
                              chart);
            if (surface.terminal == Outcome::emitted &&
                (hit.state[1] < c.disk_inner || hit.state[1] > c.disk_outer))
              continue;
            hits.emplace_back(surface, hit);
          }
        std::stable_sort(hits.begin(), hits.end(),
                         [](const auto &a, const auto &b) {
                           return a.second.affine < b.second.affine;
                         });
        for (const auto &[surface, hit] : hits) {
          ++publish_count;
          if (surface.terminal || surface.transition != Transition::none) {
            terminal = surface.terminal;
            transition = surface.transition;
            next = hit;
            if (transition != Transition::none)
              change_core(next, transition);
            break;
          }
        }
      } catch (const std::exception &) {
        // An accepted full-step estimate does not bound every shorter substep.
        // Reject the entire trial before committing any trajectory or event.
        ++out.rejected;
        if (h <= c.minimum_step)
          throw;
        h = std::max(c.minimum_step, h / 2);
        continue;
      }
      for (std::size_t i = 0; i < publish_count; ++i) {
        const auto &[surface, hit] = hits[i];
        monitor(hit);
        out.crossings.push_back({surface.name, hit});
        if (surface.terminal) {
          out.detail = surface.name;
        }
      }
      ++out.accepted;
      if (transition != Transition::none)
        ++out.chart_switches;
      if (!terminal)
        switch_patch(next);
      record(next);
      if (!std::isfinite(next.null_error) || next.null_error > c.null_tolerance)
        throw std::runtime_error(
            "null constraint exceeds configured tolerance");
      if (terminal) {
        out.outcome = *terminal;
        return out;
      }
      h = std::clamp(h * factor, c.minimum_step, c.maximum_step);
    }
    out.outcome = Outcome::step_limit;
    out.detail = "step attempt budget exhausted; fate unresolved";
  } catch (const std::exception &e) {
    out.outcome = Outcome::numerical_failure;
    out.detail = e.what();
  }
  return out;
}
Trace trace(const Parameters &p, const State &initial, const Settings &c) {
  return trace_impl(p, initial, c, Chart::boyer_lindquist);
}
Trace trace_angular(const Parameters &p, const State &initial, Chart chart,
                    const Settings &c) {
  if (chart == Chart::boyer_lindquist) {
    Trace out;
    out.final.state = initial;
    out.detail = "trace_angular requires a north or south patch";
    return out;
  }
  return trace_impl(p, initial, c, chart);
}
Emission disk_emission(const Parameters &p, const State &hit,
                       double nu_observer, double inner) {
  if (!std::isfinite(nu_observer) || nu_observer <= 0 ||
      !std::isfinite(inner) || inner <= 0)
    throw std::invalid_argument("invalid emission normalization");
  double nu = frequency(hit, circular_frame(metric(p, hit)).basis[0]);
  if (nu <= 0)
    throw std::runtime_error("nonpositive emitted frequency");
  double g = nu_observer / nu;
  double intensity = std::pow(g, 4) * std::pow(inner / hit[1], 3);
  if (!std::isfinite(intensity))
    throw std::runtime_error("nonfinite intensity");
  return {g, nu, intensity};
}
} // namespace gravastar::optics
