# Static Cartesian core continuation

This document specifies the project's regular, single-sheet continuation of
the exact de Sitter core. It is an explicit interior extension, not an interior
topology recovered from the source paper. The existing shell and exterior
metric, including the radial normalization Xi(r), are unchanged.

The construction paper restricts its coordinate domain to positive radius
bounded away from zero and leaves the deeper interior unspecified. It also
notes that the zero-mass oblate metric can have nontrivial topology. Our choice
below supplies the missing interior for this project.
[Ilijic and DeBenedictis, equations (1), (2), (4), (6) and their domain discussion](https://arxiv.org/html/2309.00551v2).
Local constant curvature alone does not fix topology: a different extension
of the zero-mass Kerr family can retain two regions joined across a disk and a
distributional ring source. That is not the extension chosen here.
[Gibbons and Volkov, Phys. Rev. D 96, 024053](https://journals.aps.org/prd/abstract/10.1103/PhysRevD.96.024053).

The algebraic derivation and implementation conditions below are project
calculations from the canonical metric in `MODEL.md`.

## Coordinate map and time normalization

Only in an exact constant-curvature region define

```text
k = Lambda/3                 (k=0 for Family::flat)
Xi = 1+k*a^2
A = r^2+a^2
Sigma = r^2+a^2*cos(theta)^2
Theta = 1+k*a^2*cos(theta)^2
T = t/Xi
Phi = phi-k*a*T
X = sqrt(A/Xi)*sin(theta)*cos(Phi)
Y = sqrt(A/Xi)*sin(theta)*sin(Phi)
Z = r*cos(theta)
R^2 = X^2+Y^2+Z^2
F = 1-k*R^2 = Theta*(1-k*r^2)/Xi
```

Both the time rescaling and the azimuthal rotation are necessary for the
normalization used in this repository. Setting T=t or rotating with k*a*t
would give the wrong metric and photon energy. These are constant factors in
the exact core; substituting the shell's radius-dependent lambda into this
map would not be a valid extension of this derivation.

The pulled-back static metric is

```text
ds^2 = -F*dT^2 + dX^2+dY^2+dZ^2
       + (k/F)*(X*dX+Y*dY+Z*dZ)^2.

g_TT = -F,       g_Ti = 0,
g_ij = delta_ij + k*X_i*X_j/F,
g^TT = -1/F,     g^Ti = 0,
g^ij = delta_ij - k*X_i*X_j,
det(g) = -1.
```

One direct verification is that the azimuthal coefficient is A*sin(theta)^2/Xi
and the mixed time/azimuth coefficient is -k*a*A*sin(theta)^2/Xi^2. The time
coefficient reduces to `-(1-k*(r^2+a^2*sin(theta)^2))/Xi^2`, matching the exact
core of `MODEL.md`. For the spatial pullback, the Euclidean r/theta mixed term
is `-k*a^2*r*sin(theta)*cos(theta)/Xi`; the rank-one k/F contribution cancels
it. The remaining diagonal terms are Sigma/[A*(1-k*r^2)] and Sigma/Theta.
This verifies the coordinate isometry rather than merely matching curvature
scalars.

The Cartesian origin and focal disk have no metric singularity. At the origin
g is Minkowski and all first derivatives vanish. In the stated curvature
convention, `R_ab=3*k*g_ab`, `R=12*k=4*Lambda`, and
`R_abcd*R^abcd=24*k^2=8*Lambda^2/3`.

## Angular-patch implementation

The production transition uses either stereographic patch without evaluating
theta or phi at a pole. Write q=u^2+v^2 and epsilon=+1 for north, -1 for south:

```text
n_x = 2*u/(1+q)
n_y = 2*v/(1+q)
n_z = epsilon*(1-q)/(1+q)
h = sqrt((r^2+a^2)/Xi),  phase = k*a*T
X = h*(cos(phase)*n_x + sin(phase)*n_y)
Y = h*(-sin(phase)*n_x + cos(phase)*n_y)
Z = r*n_z
```

The Jacobian J from (t,r,u,v) to (T,X,Y,Z) is regular on a positive-radius
overlap, including either angular pole. Its time column is
`(1/Xi, omega*Y, -omega*X, 0)` with `omega=k*a/Xi`. Its radial column is
`(0, r*X/A, r*Y/A, n_z)`. Angular columns follow by differentiating the three
bounded rational n components. Its absolute determinant is
`Sigma*[4/(1+q)^2]/Xi^2`, providing an independent determinant check.

For covariant momentum, `p_mu=J^A_mu*P_A` and `P=J^(-T)*p`. Contravariant
vectors use `V^A=J^A_mu*v^mu`; all four tetrad vectors must be transformed this
way. The canonical one-form, Hamiltonian and observer frequency
`nu=-p_mu*u^mu=-P_A*U^A` are invariant. A scalar circular-frame angular-speed
label remains a reference to its original BL convention; it is not itself a
Cartesian vector component or a parallel-transport prescription.

The two global Killing momenta must be compared in the same convention:

```text
L_z = p_phi = X*P_Y - Y*P_X
P_T = Xi*p_t + k*a*L_z
p_t = (P_T-k*a*L_z)/Xi
```

P_T is conserved in the static core, but it is generally not the incoming
BL p_t. Global energy diagnostics use the last expression while the ray is
in Cartesian coordinates. In the core the original BL ZAMO maps to the static
observer `(1/sqrt(F),0,0,0)`, whose photon frequency is `-P_T/sqrt(F)`.

## Focal disk and the extension choice

For a nonzero spin, the positive-r oblate chart omits the disk
`Z=0, X^2+Y^2<=a^2/Xi`. Its two limiting coordinate faces describe the two
approaches to that disk. The boundary circle is the oblate-coordinate focal
ring. They are coordinate defects of this Cartesian core metric, not absorbing
surfaces or matter boundaries of the extension.

The project fills this disk once, using ordinary Cartesian points, and fills
the entire bounded core ellipsoid with the smooth static de Sitter metric.
Each constant-T core slice is a ball with one boundary; it joins the single
existing exterior through the already specified shell. There is no second
copy of the interior, negative-r sheet, imposed reflection, ring source or
extra disk junction. A ray crosses the disk by continuous Cartesian evolution
and returns to the positive-r chart on the appropriate hemisphere. Smooth
Cartesian coefficients across the disk entail no distributional curvature
there for this chosen extension. This is not a claim that every global
zero-mass limit of a Kerr manifold has the same topology.

For a=0, the omitted set collapses to the origin and this reduces to ordinary
spherical-to-Cartesian continuation. For k=0, the Cartesian core is Minkowski,
although its oblate chart still has the focal disk when a is nonzero.

The positive inverse oblate radius can be recovered away from the disk using

```text
B = Xi*(X^2+Y^2)+Z^2-a^2
D = hypot(B, 2*a*Z)
r^2 = (B+D)/2                      if B>=0
      2*a^2*Z^2/(D-B)              if B<0.
```

The rationalized second branch avoids cancellation near the disk. The disk
itself has r=0 under this scalar labeling but no regular inverse oblate
coordinate map. Do not recover theta, angular momenta or frame components
there. Inverse chart transformations are performed only on a positive-radius
overlap. The scalar radius label is not differentiable at the focal ring;
Cartesian geometry and smooth ellipsoid event functions avoid that defect.

## Supported region and transition surfaces

For a toy core the physical region is the ellipsoid

```text
Xi*(X^2+Y^2)/(r_in^2+a^2) + Z^2/r_in^2 <= 1.
```

It is independent of the static azimuthal rotation and can be checked without
the singular inverse map. Exact de Sitter and flat reference families can use
the same Cartesian formulas on their own supported static domain. The massive
Kerr family is ineligible.

The static coordinate patch requires F>0; the current geometry adds a numerical
margin F>64*epsilon_double. It does not cross a cosmological horizon. A simple
sufficient condition covering the entire toy core is `k*r_in^2<1`; the largest
Cartesian radius of its ellipsoid is sqrt((r_in^2+a^2)/Xi) in that case. This
condition holds for the fixed benchmark. A pointwise F check alone should not
be advertised as a certification of every possible parameterized core.

Entry and exit transitions must both lie strictly inside the exact core, with
`0<r_enter<r_exit<r_in` for the toy family. The separation supplies hysteresis;
it also leaves room for event refinement and trial stages. The chart's overlap
surface at a positive radius b can be evaluated smoothly as
`Xi*(X^2+Y^2)/(b^2+a^2)+Z^2/b^2-1`. Neither numerical trajectory stages nor a
postponed exit event may use the constant-curvature backend in the transition
shell. A rejected trial is preferable to evaluating the wrong metric there.

The old inner radius remains meaningful in modes without this extension. A
core-enabled mode must explicitly document which radial stop it replaces;
outer escape limits, affine budgets, unsupported horizons and other unresolved
outcomes remain explicit. Enabling core continuation is a model/atlas choice,
not a reinterpretation of historical BL reports as resolved rays.

## Independent checks and analytic core rays

Suitable checks include metric pullback at both angular poles and away from
them, both spin signs, k=0 and a=0 limits, state/frame round trips, canonical
pairings, Hamiltonian invariance and both global Killing momenta. Testing only
round trips is insufficient: mutually wrong forward/inverse transforms can
cancel. Compare the transformed metric directly with the original core metric
and use independently differentiated coordinate formulas for the reference.

A useful exact ray reference follows directly from the Cartesian Hamiltonian.
Let W=X dot P and let s increase into the past, as in the existing tracer:

```text
H = (-P_T^2/F + P dot P - k*W^2)/2
T' = P_T/F
X' = -P+k*X*W
P_T' = 0
P' = -k*X*P_T^2/F^2-k*P*W
X'' = -2*k*H*X.
```

For an exactly null ray, the affine Cartesian spatial path is therefore the
straight line `X(s)=X0+V*s`, even through the origin, focal disk or focal ring.
Its momentum is `P=-V-k*X*(X dot V)/F`, with
`P_T^2=V dot V-k*|X cross V|^2`. Future momentum uses the negative P_T root.
This is a strong independent reference for central crossing, with no production
metric-derivative arithmetic in the reference path.

For nonzero k and w=|V|, define z0=(X0 dot V)/w, b2=|X0|^2-z0^2,
C=1-k*b2, and u(s)=sqrt(k/C)*(z0+w*s). While both endpoints and the segment
remain in the static patch,

```text
T(s)-T(0) = P_T/[w*sqrt(k*C)] * (atanh(u(s))-atanh(u(0))).
```

For k=0 the limit is P_T*s. A reference implementation must handle small
arguments/differences accurately, for example with log1p identities, rather
than letting time cancellation mask trajectory error. Ordinary numerically
launched rays have small nonzero H; their Cartesian acceleration follows
`-2*k*H*X`, so silently projecting them onto a null straight line would change
the algorithm and must not be hidden inside a coordinate transform.

Whole-ray tests should vary core entry/exit radii and integration tolerances,
check round-trip Killing drift, cross the center and focal disk in flat and
de Sitter cases, and revisit the previously unresolved toy pixels. Curvature
identities, numerical precision/refinement and end-to-end image evidence are
recorded separately; this derivation alone is not a completed P2 acceptance,
matter-model validation, stability result or global geodesic-completeness proof.
