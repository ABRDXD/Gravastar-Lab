# Scientific source audit

Audit date: 2026-10-20. This records evidence available during P0 and
distinguishes a reproducible implementation of the published construction from
an exact reproduction of the figures.

The pinned scientific reference is Saša Ilijić and Andrew DeBenedictis,
*On a Class of Exact Arbitrarily Differentiable de Sitter Cores with Kerr
Exteriors: Possible gravastar or regular black hole mimickers*,
[arXiv:2309.00551v2](https://arxiv.org/abs/2309.00551v2), submitted
2024-11-28 19:52:48 UTC. Its
[HTML](https://arxiv.org/html/2309.00551v2) was read. The available
[PDF extraction](https://arxiv.org/pdf/2309.00551) identifies itself internally
as v2, dated 28 November 2024, with 31 pages. The unversioned URL was a retrieval
fallback, not a replacement for the version pin.

## Source map and parameter provenance

| Item | Source location | Audit finding |
| --- | --- | --- |
| Base metric | Equation (1), PDF p. 2 | Use the full Kerr–de Sitter expression, including both factors containing the effective cosmological parameter. |
| Coordinate domain | Equation (2) and following paragraph, PDF p. 2 | Positive lower radial boundary; axes excluded; central continuation unspecified. |
| Replacement rule | Equation (6), PDF p. 9 | Replace both mass and cosmological parameter throughout the expression. |
| Inverse; determinant | Equations (7), (12) | Independent algebraic check targets. |
| Effective stress tensor | Equations (13), (15) | Cosmological contribution already included in the inferred stress tensor. |
| Junction tensor | Equations (16)–(17) | Check the outward-normal extrinsic curvature. |
| Toy transition | Equations (21)–(22), PDF pp. 13–14 | Seventh-degree polynomial; complementary transitions; matching through derivative order three. |
| Dimensionless parameters | Section III.I, PDF p. 14 | Explicit: `M = 1`, `a = 7/8`, `Lambda = 1/2`. |
| Exact shell radii of metric figures | Figures 4 and 7–10 | **Not recovered.** Their values are absent from the accessible text. |
| Figure 6 radial ticks | PDF p. 14 | Extracted ticks: `1.90, 1.95, 2.00, 2.05, 2.10`. These do not establish the shell radii of the later metric figures. |
| Full plotted radial range | Figures 7–10 | **Not recovered.** Embedded figure axes were unavailable. |

The figure-6 ticks are a clue, not a parameter specification. The source calls
that figure a sample transition, while figure 4 displays a vacuum ergoregion.
For the stated mass the equatorial exterior Kerr static limit is `r = 2`;
therefore the interval `[1.9, 2.1]` cannot be assumed to reproduce the
configuration in figure 4. No exact shell radii should be inferred from these
ticks alone.

Until the original figure files, source archive, or an explicit author
specification is available, any executable preset must label its radii and
inspected radial/angular range as **project choices**. Numerical agreement with
the equations can be accepted separately; exact published-figure reproduction
remains unresolved.

## Algebraic correction to the horizon diagnostic

The following is an independent deduction from the displayed metric, not an
unqualified adoption of the paper's terminology.

Write `k = Lambda/3`, `Sigma = r^2 + a^2 cos(theta)^2`,
`Dtheta = 1 + k a^2 cos(theta)^2`, and `Xi = 1 + k a^2`. In the pure core,

```text
Delta = (r^2 + a^2) (1 - k r^2)
g^rr = Delta / Sigma
g_tt = -[1 - k (r^2 + a^2 sin(theta)^2)] / Xi^2.
```

Equation (5) therefore solves `g_tt = 0`, whereas a constant-`r` null surface
solves `Delta = 0`. They differ whenever `a sin(theta) != 0`:

```text
equation (5): r^2 = 3/Lambda - a^2 sin(theta)^2
radial root: r^2 = 3/Lambda.
```

In fact, on the equation-(5) surface away from the axes, `Delta > 0`. For its
defining function `F = r^2 + a^2 sin(theta)^2 - 3/Lambda`, the normal norm is

```text
g^{ab} F_,a F_,b
  = [4 r^2 Delta
     + 4 a^4 Dtheta sin(theta)^2 cos(theta)^2] / Sigma > 0.
```

Thus that surface is timelike there, and cannot be a null horizon. Benchmark
reports must distinguish the static limit of the chosen time Killing vector,
radial null-surface roots, and the substantially stronger global event-horizon
question. With `Lambda = 1/2`, the pure-core radial root would be `sqrt(6)`;
whether that radius belongs to a particular piecewise core is a separate
domain question.

## Consequences for the implementation

For `q = (r-r_in)/(r_out-r_in)`, the published transition is algebraically
equivalent to

```text
mu(q) = 35 q^4 - 84 q^5 + 70 q^6 - 20 q^7
lambda(q) = 1 - mu(q).
```

Outside the shell the functions are clamped to the prescribed exact regions.
They must not be continued as polynomials into the core or exterior. Both
functions have zero first, second, and third endpoint derivatives; fourth
derivatives generally jump after clamping. The resulting metric is `C^3`
where its denominators stay nonzero. Curvature uses second metric derivatives.
The complementary form is a property of this toy case, not the whole family.

In the shell, define `L(r) = Lambda lambda(r)` and `m(r) = M mu(r)`. All
instances of `L` in `Delta`, `Dtheta`, and `Xi` must be differentiated. Holding
`Xi` constant while differentiating would change the model.

The following useful identity follows by expanding the two squared one-forms
in equation (1):

```text
g_tt g_phiphi - g_tphi^2 = -Delta Dtheta sin(theta)^2 / Xi^4
det(g) = -Sigma^2 sin(theta)^2 / Xi^4.
```

For `Delta > 0`, `Dtheta > 0`, `Sigma > 0`, `Xi != 0`, and off-axis angles,
the `t,phi` block has one positive and one negative eigenvalue, while the
radial and polar components are positive. This gives an analytic inertia
argument under explicit conditions; a negative determinant alone is less
informative. Coordinate eigenvalues and condition numbers remain
chart-dependent numerical diagnostics.

P1 should retain the original chart and a bounded positive-radius, off-axis
inspection domain. It should return an explicit domain error at excluded
coordinates. A complete central/axial extension belongs to the next chart
decision and must not be simulated by absorption or reflection.

## Retrieval and reproducibility limitations

The following reads were attempted:

- Versioned arXiv HTML: successful text extraction.
- Versioned arXiv PDF: web cache miss.
- Unversioned arXiv PDF and `.pdf` variant: successful text extraction,
  internally identified as v2.
- PDF screenshots of pages 8 and 14–16: cache miss.
- Embedded `polymetric.png` and `polyKretsch.png`: cache miss.
- Versioned TeX/source endpoint and export mirror: unsuccessful.
- Author-uploaded ResearchGate full text: corroborated the text, but its PDF
  and figure files also failed retrieval. The primary citation remains arXiv.
- Direct local Python HTTPS requests: DNS resolution failure.

No source archive, PDF bytes, or figure bytes were downloaded into the project.
Consequently **no source-file hash is available**, and no checksum is invented.
The downloaded-source cache is excluded from the repository. The remaining
provenance task is to retrieve versioned source bytes, record SHA-256 hashes,
inspect the plotting source/figures, and replace the unresolved entries above
with explicit evidence. This does not prevent an independent implementation
and validation of the available equations at clearly identified project
parameters.
