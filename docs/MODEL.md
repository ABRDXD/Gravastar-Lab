# Model contract: toy-c3-v1

P0 specification frozen for the bounded P1 benchmark, 2026-10-20.
Scientific reference: Ilijić and DeBenedictis,
[arXiv:2309.00551v2](https://arxiv.org/html/2309.00551v2), equations
(1), (6), (21), (22); see [source audit](SOURCE_AUDIT.md).
This implementation uses both transition functions everywhere in the metric.

## Conventions and parameters

Use `G=c=1`, signature `(-,+,+,+)`, coordinates and tensor indices
`(t,r,theta,phi)=(0,1,2,3)`. All arrays are covariant except `inverse`.
`dg[k][i][j]=partial_k g_ij`, `ddg[k][l][i][j]=partial_k partial_l g_ij`;
`drrr[i][j]` is the third radial derivative. Stationary/axial derivatives vanish.
`J=M a`, with positive `a` the positive-phi spin convention.

The fixed rapid-rotation benchmark uses `M=1`, `a=7/8`, `Lambda=1/2`,
as specified in section III.I. **`r_in=3/2`, `r_out=9/5` are new project
choices.** The paper's exact figure radii could not be recovered. This is an
implementation of its construction, not a reproduction of its plotted figures.
The numerical validation box `r/M in [0.25,10]`,
`theta in [0.05,pi-0.05]` is also a project choice.

Controls are dimensionless `a/M`, `Lambda M^2`, `r_in/M`, `r_out/M`.
Changing a physical mass while holding these fixed changes units only:
length unit `GM_phys/c^2`, time unit `GM_phys/c^3`; curvature scalars of
dimension length^-2 and length^-4 scale accordingly. No SI constants are
hard-coded. Arbitrary parameter inputs are for diagnostics, not accepted presets.
`M=0` is allowed for analytic reference limits; a gravastar preset requires `M>0`.

## Formula and derivatives

In the shell let `u=(r-r_in)/(r_out-r_in)` and
`S(u)=35u^4-84u^5+70u^6-20u^7`. Set `mu=S`, `lambda=1-S`.
This is equation (22) after `x=2u-1`. Inside set `(mu,lambda)=(0,1)`,
outside `(1,0)`. `S` has endpoint values 0,1 and derivatives 1–3 zero.
The joined metric is C3, generally not C4.

Define `m=M mu`, `L=Lambda lambda`, `Sigma=r^2+a^2 cos^2(theta)`,
`B=r^2+a^2`, `D= B(1-L r^2/3)-2 m r`,
`T=1+L a^2 cos^2(theta)/3`, `X=1+L a^2/3`.
The nonzero components are

```text
g_tt       = (T a^2 sin^2(theta)-D)/(Sigma X^2)
g_tphi     = a sin^2(theta)(D-T B)/(Sigma X^2)
g_phiphi   = sin^2(theta)(T B^2-D a^2 sin^2(theta))/(Sigma X^2)
g_rr       = Sigma/D
g_thetatheta = Sigma/T
```

`g_phit=g_tphi`; the line element's mixed coefficient is `2 g_tphi`.
The inverse is the inverse 2x2 time/phi block plus the two diagonal reciprocals.
Its block determinant is `-D T sin^2(theta)/X^4` and the full determinant
is `-Sigma^2 sin^2(theta)/X^4`.

C++ evaluates double-precision bivariate Taylor jets through total order three,
including all first and second metric derivatives and the third radial
derivative. The complementary polynomial near the outer endpoint reduces
cancellation. Derivatives apply to **all** occurrences of `mu` and `lambda`,
including `T` and `X`. Fast-math is disabled.

The independent C++20 reference expands components algebraically, evaluates
the original polynomial in `x` using 90 fixed decimal places, inverts with
Gauss-Jordan, and takes high-precision numerical differences. It includes no
production geometry or AD header. Five-point fourth-order differences provide first/second
derivatives; tensor products provide mixed derivatives. Nine-point differences
and analytic branch continuation test radial matching through order three.
The reference implements sine/cosine by reduced Taylor series; precision and
step refinement and analytic reference identities check this additional code.

Curvature convention:
`R^a_bcd = partial_c Gamma^a_db - partial_d Gamma^a_cb
          + Gamma^a_ce Gamma^e_db - Gamma^a_de Gamma^e_cb`,
`R_bd=R^a_bad`, `R=g^bd R_bd`.
Consequently de Sitter has `R=4 Lambda`, `R_ab=Lambda g_ab`,
`R_abcd=(Lambda/3)(g_ac g_bd-g_ad g_bc)`, `K=8 Lambda^2/3`.
The high-precision reference builds the fully covariant Riemann tensor directly from
metric Hessians; C++ differentiates Christoffel symbols.

## Chart and interior decision

P1 uses exactly the paper's BL-type chart: `r>0`, `0<theta<pi`.
The validation box stays away from axes and `r=0`. There is **no** central
extension, axis-crossing chart, horizon-crossing chart, or ray continuation
implemented. The evaluator's wider mathematical input support is not a claim
of validated accuracy outside the reported box.

No r-dependent time/azimuth normalization is silently removed: `X(r)` in
this transition cannot be treated as a constant Kerr coordinate rescaling.
Coordinate transformation consistency for the present single chart consists
of tensor identities and angular reflection/spin reversal. Nontrivial chart
transformations and position/momentum/frame transitions are P2 work.

The singular-coordinate surfaces `D=0`, axes and `r=0` are rejected or excluded.
Production rejects `sin(theta)^2 <= 64 epsilon_double` and
`abs(D) <= 64 epsilon_double max(1,abs(B),abs(B L r^2/3),abs(2mr))`.
These numerical guards are wider than the exact singular surfaces and are
outside the accepted benchmark box.
They must not become fictitious absorbing boundaries in a future renderer.
Full interior optics is blocked pending a separately derived extension and
chart atlas. Reference Kerr horizon crossing is also outside P1.

## Domain, observers, matching, and matter

The shell polynomial is rational for the fixed preset. Exact rational Sturm
sequences count its radial null-surface roots, including even multiplicity;
Bernstein subdivision supplies a strict positive bound on `D`. Core and
exterior inequalities complete the radial argument. This is a local radial
null-surface diagnostic, not a proof of global event-horizon absence.
Equation (5) of the source is checked algebraically as `g_tt=0` and is not
used as a horizon locator.

`Sigma,T,X>0`, `D>0` imply positive radial/angular components and negative
time/phi block determinant, hence Lorentzian inertia. In the chosen model
`g_phiphi = sin^2(theta)[B X+2mr a^2 sin^2(theta)/Sigma]/X^2 > 0`
off axis, excluding timelike azimuthal loops there.
Coordinate eigenvalue magnitudes/condition numbers are basis dependent;
they measure numerical conditioning, not invariant physical severity.

For a circular observer `u proportional to partial_t+Omega partial_phi`,
timelikeness requires
`g_tt+2 Omega g_tphi+Omega^2 g_phiphi<0`.
With `g_phiphi>0`, allowed angular speeds lie strictly between
`(-g_tphi +/- sqrt(g_tphi^2-g_tt g_phiphi))/g_phiphi`.
The ZAMO choice `Omega=-g_tphi/g_phiphi` exists when this discriminant is
positive. This establishes observer existence, not an orbital emitter model.

At both joins compare induced metric and outward-normal extrinsic curvature
`K_AB=(partial_r g_AB)/(2 sqrt(g_rr))`, `A,B in {t,theta,phi}`,
using the same outward normal convention on both sides. Endpoint polynomial
identities enforce all radial derivatives through order three.

Effective stress-energy is `T_ab=G_ab/(8 pi)`; the cosmological contribution
is included once. A negative `T_ab k^a k^b` for an explicitly null vector
is a valid NEC violation witness and requires no perfect-fluid assumption.
A finite null-direction sample cannot prove the NEC. Full tensor
classification and energy-condition mapping are unresolved. No matter
closure, equilibrium matter solution or dynamical stability is claimed.
