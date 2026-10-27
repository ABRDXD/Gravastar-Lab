# Turning-point event coverage

Work in progress, 2026-10-26. The previous detector searched only a trial's
endpoint surface values. A shallow excursion across a boundary and back can
leave both endpoints on the same side even when the integration error is small.

The new detector retains the first and last Dormand-Prince derivatives. For
each active surface it evaluates the derivative along the ray. Strictly
opposite endpoint rates bracket a stationary point; reintegration and bisection
locate that point within the configured affine event tolerance. The event search
is then split there, allowing both signed crossings to be found. Both intervals
are refined from the original trial state, with the existing local-error and
null checks. Events remain ordered by affine parameter and are published only
after the earliest terminal/transition candidate is valid.

This handles a resolved single turning excursion. It does not certify arbitrary
hidden extrema, all exact tangencies, or penetrations below integration and
event resolution. It introduces no absorbing surface or altered null dynamics.
Additional work at turning points consumes RHS calls; correctness takes priority
over a performance claim. The ordinary Dormand-Prince stages already provide the
endpoint derivatives.

The four focused existing core, angular, original-reference and core-CLI CTests
pass. New native tests pass 63 analytic event checks and 73 near-critical
Schwarzschild checks. Both suites are registered with CTest. The flat fixtures
check a shallow excursion, an adjacent miss, two nonterminal roots and clipping
of the discarded angular trial tail at Cartesian-core entry. The Schwarzschild
fixtures compare six impact parameters on both sides of 3*sqrt(3)*M, at relative
offsets 1e-3, 1e-5 and 1e-7, against independently stepped long-double equations.
Full compiler/sanitizer verification and final provenance are still pending.
Full P2 acceptance and GYOTO comparison remain open.
