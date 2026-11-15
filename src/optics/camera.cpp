// SPDX-License-Identifier: GPL-3.0-or-later
// ============================================================================
// GravastarLab Optics Engine — Observer Camera Frame & Ray Momentum Mapping
// ============================================================================
//
// 9TH-GRADER HIGH SCHOOL ANALOGY:
// ------------------------------
// Imagine you are in a tiny spaceship holding a video camera, flying near a
// colossal spinning gravastar.
//
// 1. WHY CAN'T YOU JUST "STAND STILL"?
//    If you try to drop an anchor and park your spaceship motionless (what physicists
//    call a "static observer"), you run into a huge problem: near the spinning
//    gravastar, spacetime itself is swirling around like water in a violent whirlpool!
//    Inside a region called the "ergosphere", space is swirling so fast that
//    standing still is literally impossible—it would require you to travel faster
//    than the speed of light. Your spaceship's engine would explode.
//
// 2. THE CLEVER SOLUTION: THE ZAMO (Zero-Angular-Momentum Observer)
//    Instead of fighting the whirlpool, you put your spaceship in neutral and let
//    the swirling current carry you in a circle. You don't fire your thrusters
//    left or right; you just drift along with the flow.
//    Because you drift at the exact same speed that spacetime is spinning, inside
//    your cockpit you feel completely calm! You feel zero centrifugal force pulling
//    you sideways. This is called a "Zero-Angular-Momentum Observer" (ZAMO),
//    also known as the Bardeen Locally Non-Rotating Frame (LNRF).
//
// 3. WHAT IS A "TETRAD"?
//    A tetrad is just 4 ruler-arrows you draw inside your spaceship cockpit:
//    - e0: Your clock's second-hand ticking forward into the future (time direction).
//    - e1: The laser pointer pointing straight ahead at the gravastar (forward/depth).
//    - e2: The arrow pointing straight up towards the ceiling (vertical/pitch).
//    - e3: The arrow pointing straight out your right window (horizontal/yaw).
//    These 4 arrows are perfectly perpendicular to each other ("orthonormal") inside
//    your cockpit, making it easy to shoot camera rays through your screen pixels!
// ============================================================================

#include "gravastar/optics/camera.hpp"
#include <cmath>
#include <stdexcept>

namespace gravastar::optics {

// ============================================================================
// FUNCTION: make_observer_tetrad
// ----------------------------------------------------------------------------
// Calculates the 4 cockpit ruler-arrows (e0, e1, e2, e3) for an observer
// located at radial distance 'r' and polar angle 'theta'.
//
// 9TH-GRADER SYMBOL DICTIONARY:
// - r        : Distance from the very center of the gravastar.
// - theta    : Latitude angle (0 = North Pole, pi/2 = Equator, pi = South Pole).
// - g_tt     : Time dilation metric term (how slow clocks tick).
// - g_rr     : Radial stretch metric term (how space stretches inwards/outwards).
// - g_thth   : Polar stretch metric term (how space stretches North/South).
// - g_phiphi : Azimuthal circle size (circumference of the latitude circle).
// - g_tphi   : Whirlpool swirl term (how tightly time and rotation are dragged together).
// - omega    : Spin speed of spacetime itself at this spot (radians per second).
// - alpha    : "Lapse" function (how much proper time ticks per second of coordinate time).
// ============================================================================
Tetrad make_observer_tetrad(const geometry::Parameters& params, double r, double theta) {
    // [Step 1: Evaluate the spacetime geometry at the spaceship's position]
    // Ask our geometry engine: "How curved and bent is space at this exact (r, theta)?"
    const auto eval_res = geometry::evaluate(params, r, theta);
    if (std::holds_alternative<geometry::Error>(eval_res)) {
        throw std::runtime_error("Cannot construct observer frame: invalid coordinate or domain");
    }
    const auto& g = std::get<geometry::Evaluation>(eval_res);

    Tetrad tetrad{};

    // [Step 2: Read out the metric tensor components]
    // These numbers describe the curvature of spacetime at the observer's location.
    const double g_tt     = g.metric[0][0]; // Time-time component
    const double g_rr     = g.metric[1][1]; // Radius-radius component
    const double g_thth   = g.metric[2][2]; // Theta-theta component
    const double g_phiphi = g.metric[3][3]; // Phi-phi component
    const double g_tphi   = g.metric[0][3]; // Frame-dragging cross-term

    // [Step 3: Physical Sanity Check — Lorentzian Metric Signature]
    // In our universe, space must have 3 positive spatial directions (+,+,+).
    // If any of these are zero or negative, space has collapsed into a singularity!
    if (g_phiphi <= 0.0 || g_rr <= 0.0 || g_thth <= 0.0) {
        throw std::runtime_error("Invalid metric signature for observer frame: spatial diagonals must be > 0");
    }

    // [Step 4: Calculate the Whirlpool Angular Velocity 'omega']
    // Imagine stirring honey with a spoon: the honey right next to the spoon spins.
    // 'omega' is the angular speed at which spacetime itself is rotating.
    // Formula: omega = -g_{t phi} / g_{phi phi}
    //
    // WHY THE MINUS SIGN?
    // In our metric signature (-,+,+,+), g_{t phi} is negative for a prograde
    // spinning gravastar. The minus sign makes omega positive, so the observer
    // drifts in the SAME direction as the gravastar (clockwise matches clockwise).
    //
    // DEBUGGING HINT:
    // If you remove this minus sign, the observer will try to swim backwards against
    // the whirlpool, and the camera image will violently distort and roll!
    const double omega = -g_tphi / g_phiphi;

    // [Step 5: Calculate the Observer's Time Flow Rate 'alpha' (The Lapse)]
    // In deep space far away from any gravity, time flows at 1 second per second (alpha = 1.0).
    // Near a massive body, gravity slows time down (gravitational time dilation), so alpha < 1.0.
    //
    // Formula: lapse^2 = -g_tt + omega^2 * g_phiphi
    //
    // WHY THIS WORKS INSIDE THE ERGOSPHERE:
    // When you cross into the ergosphere, g_tt becomes positive (+), which would break
    // a stationary clock (giving the square root of a negative number!).
    // But because the ZAMO drifts with speed omega, the rotational energy term
    // (omega^2 * g_phiphi) is always larger than g_tt, guaranteeing that lapse^2
    // remains strictly POSITIVE everywhere outside any true event horizon.
    const double lapse_sq = -g_tt + omega * omega * g_phiphi;
    if (lapse_sq <= 0.0) {
        throw std::runtime_error("ZAMO lapse squared must be strictly positive (observer cannot exist in imaginary time)");
    }
    const double alpha = std::sqrt(lapse_sq);

    // [Step 6: Build the 4 Orthonormal Cockpit Vectors]
    // Each vector has 4 spacetime components: [t, r, theta, phi].

    // e0 (Time axis / 4-velocity):
    // Your spaceship moves through time at rate 1/alpha, and drifts around
    // the gravastar in the phi direction at rate omega/alpha.
    tetrad.e0 = {1.0 / alpha, 0.0, 0.0, omega / alpha};

    // e1 (Forward axis pointing straight at the gravastar):
    // In Boyer-Lindquist coordinates, increasing r means flying AWAY from the gravastar.
    // Since our camera wants to look INWARD at the gravastar, we point along -partial_r!
    // We normalize by 1 / sqrt(g_rr) so this ruler has length exactly 1 meter in the cockpit.
    tetrad.e1 = {0.0, -1.0 / std::sqrt(g_rr), 0.0, 0.0};

    // e2 (Upward axis pointing towards the North Pole):
    // Increasing theta means moving South. To make "Up" on our screen point North,
    // we set e2 along partial_theta normalized by 1 / sqrt(g_thth).
    tetrad.e2 = {0.0, 0.0, 1.0 / std::sqrt(g_thth), 0.0};

    // e3 (Rightward axis pointing East / prograde):
    // Points along the azimuthal direction partial_phi, normalized by 1 / sqrt(g_phiphi).
    tetrad.e3 = {0.0, 0.0, 0.0, 1.0 / std::sqrt(g_phiphi)};

    return tetrad;
}

// ============================================================================
// FUNCTION: pixel_ray_momentum
// ----------------------------------------------------------------------------
// Converts a 2D screen pixel coordinate (like x=320, y=240 on a monitor)
// into the physical 4-momentum (p_t, p_r, p_theta, p_phi) of a light ray
// traveling backwards into the universe.
//
// 9TH-GRADER MOVIE PROJECTOR ANALOGY:
// In real life, light leaves a star, bends around the gravastar, and hits your eye.
// But 99.999% of stars never hit your tiny camera lens!
// So instead of simulating billions of useless star rays, we reverse time:
// we shoot a ray backward OUT of each camera pixel, trace it into space,
// and check what star or gas cloud it hits. That is "ray tracing"!
//
// INPUTS:
// - ndc_x, ndc_y : Normalized screen coordinates from -1.0 (left/bottom) to +1.0 (right/top).
// - tan_half_fov : Field-of-view angle control (zoom in vs. zoom out).
// - aspect       : Screen aspect ratio (e.g. 16/9 for widescreen or 1.0 for square).
// ============================================================================
Vec4 pixel_ray_momentum(const Tetrad& tetrad,
                        const geometry::Evaluation& geom,
                        double ndc_x,
                        double ndc_y,
                        double tan_half_fov,
                        double aspect) {
    // [Step 1: Compute the 3D direction vector inside the cockpit]
    // dx: How far to the right this pixel is.
    // dy: How far up this pixel is.
    // dz: Forward into the screen (always 1.0).
    const double dx = ndc_x * tan_half_fov * aspect;
    const double dy = ndc_y * tan_half_fov;
    const double dz = 1.0; // Pointing forward through the glass windshield

    // [Step 2: Normalize the 3D direction to a unit vector of length 1.0]
    // The Pythagorean theorem in 3D: norm = sqrt(dx^2 + dy^2 + dz^2).
    const double norm = std::sqrt(dx * dx + dy * dy + dz * dz);
    const double nx = dx / norm; // Unit horizontal direction
    const double ny = dy / norm; // Unit vertical direction
    const double nz = dz / norm; // Unit forward direction

    // [Step 3: Construct the past-directed null photon vector in cockpit coordinates]
    // In Einstein's relativity, light always travels at speed c (length = 0, "null vector"):
    //   k_cockpit = [ -1, nz, -ny, nx ]
    // Check: -(-1)^2 + nz^2 + (-ny)^2 + nx^2 = -1 + 1 = 0 (Null condition satisfied!)
    //
    // Then we multiply each component by our cockpit ruler-arrows to transform
    // from spaceship coordinates into raw universe spacetime coordinates:
    Vec4 k_up{};
    for (int mu = 0; mu < 4; ++mu) {
        k_up[mu] = -tetrad.e0[mu]       // Move backward in time (-e0)
                   + nz * tetrad.e1[mu] // Forward towards gravastar
                   - ny * tetrad.e2[mu] // Upwards towards North
                   + nx * tetrad.e3[mu]; // Rightwards towards East
    }

    // [Step 4: Lower the index to get covariant momentum p_mu = g_{mu nu} k^nu]
    // In Hamiltonian mechanics, the equations of motion use the "covariant momentum" p_mu.
    // We convert contravariant velocity k^nu into covariant momentum p_mu using the metric ruler.
    Vec4 p_down{};
    for (int mu = 0; mu < 4; ++mu) {
        p_down[mu] = 0.0;
        for (int nu = 0; nu < 4; ++nu) {
            p_down[mu] += geom.metric[mu][nu] * k_up[nu];
        }
    }

    // Returns [p_t, p_r, p_theta, p_phi] ready to be integrated by our RK4 solver!
    return p_down;
}

} // namespace gravastar::optics
