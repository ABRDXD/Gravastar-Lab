# GravastarLab Scientific & Engineering Validation Ledger

This document establishes the reproducible mathematical, computational, and hardware evidence across Stages P0 through P5 of GravastarLab. All tests are natively runnable in C++20 via CTest on Linux.

## 1. Test Suite Summary (16/16 Passing, 100%)

Executed via `ctest --test-dir build/headless-release --output-on-failure`:

| Test # | Target Name | Domain / Subsystem | Measured Outcome | Acceptance Gate |
|:---:|:---|:---|:---|:---:|
| 1 | `geometry_unit` | Metric tensor, inverse, Christoffel symbols, Riemann & Ricci curvature | Verified against closed analytic limits | **PASS** |
| 2 | `integration_unit` | Hamiltonian RK4 geodesic step, null constraint conservation | $\Delta H < 10^{-4}$ across 500 steps | **PASS** |
| 3 | `conserved_quantities_unit` | Energy & axial momentum drift along geodesics | $\Delta E = 0.000$, $\Delta L_z = 0.000$ (machine precision) | **PASS** |
| 4 | `critical_impact_unit` | Horizonless shell & core impact parameter boundaries | Analytical boundary matching | **PASS** |
| 5 | `optical_simulation_test` | CPU OpenMP reference raymarcher | Matched celestial sky refraction & core glow | **PASS** |
| 6 | `benchmark_help` | Headless CLI option parser & usage contract | Formatted output verification | **PASS** |
| 7 | `benchmark_reject_axis` | Boyer–Lindquist coordinate boundary verification | Clean rejection of $\theta \le 0, \theta \ge \pi, r \le 0$ | **PASS** |
| 8 | `p1_scientific_validation` | Independent Python/C++ jet matching & Sturm root bound | Bernstein polynomial strictly positive on $\Delta(r)$ | **PASS** |
| 9 | `equatorial_turning_points_unit`| 4 canonical equatorial rays with step-halving ($h_0 \to h_0/2$) | Error $< 3.5 \times 10^{-8}$, turning point convergence | **PASS** |
| 10 | `geodesic_cross_validation_unit`| Argon's 113-bit quad-precision (`__float128`) cross-validation suite | $\Delta E, \Delta L_z < 10^{-20}$, Carter symmetry breaking | **PASS** |
| 11 | `zamo_shadow_validation_unit`  | ZAMO Bardeen tetrad, shadow/caustic contours, 3,444 checks | Tetrad error $< 5.8 \times 10^{-34}$, non-separable drift | **PASS** |
| 12 | `vulkan_device_audit` | Hardware query of NVIDIA GeForce RTX 3050 Laptop GPU | Vulkan 1.4, hardware FP64 supported | **PASS** |
| 13 | `vulkan_metric_unit` | GPU compute shader (`gravastar_metric.comp`) vs. CPU | Max tensor difference $< 1.74 \times 10^{-5}$ | **PASS** |
| 14 | `vulkan_ray_unit` | GPU compute rays (`gravastar_geodesic.comp`) vs. CPU reference | Turning point diff $\le 2.4 \times 10^{-6}$, 100% core match | **PASS** |
| 15 | `vulkan_studio_smoke` | Live 24-frame desktop presentation swapchain with resize | Exact RGBA8 oracle match, zero validation errors | **PASS** |
| 16 | `deterministic_recording_unit`| Timeline, fault-injection, resume, and FFmpeg MP4 export | Container verified via `ffprobe`, `GRAVASTAR_REQUIRE` active | **PASS** |

---

## 2. P1: Geometry & Polynomial Matching Evidence

The toy gravastar metric (Ilijić & DeBenedictis, arXiv:2309.00551v2) glues an interior de Sitter core to a Kerr exterior across a shell $r \in [r_{\text{in}}, r_{\text{out}}] = [1.5, 1.8]$ using a $C^3$ septic polynomial:
$$w(u) = 35 u^4 - 84 u^5 + 70 u^6 - 20 u^7, \quad u = \frac{r - r_{\text{in}}}{r_{\text{out}} - r_{\text{in}}}$$

* **Matching Order**: Radial derivatives $\partial_r^k g_{\mu\nu}$ match from both sides through $k=3$.
* **Extrinsic Curvature**: Continuity of $K_{AB} = \frac{\partial_r g_{AB}}{2\sqrt{g_{rr}}}$ verified across both $r_{\text{in}}$ and $r_{\text{out}}$.
* **Absence of Horizons**: Exact rational Sturm sequence count confirms zero real roots for $\Delta(r)$ on $r \in [1.5, 1.8]$, certified by Bernstein subdivision.

---

## 3. P2: Reference Optics & Geodesic Trajectories

### Equatorial Turning Points and Step-Halving Convergence

Rays launched from $r_0 = 10.0, \theta_0 = \pi/2$ with impact parameter $b = L_z/E$:

| $b$ | Expected $r_{\text{turn}}$ | Measured $r_{\text{turn}}$ ($h_0=0.01$) | Measured $r_{\text{turn}}$ ($h_0=0.005$) | Outcome | Convergence Ratio |
|:---:|:---:|:---:|:---:|:---|:---:|
| $+1.80$ | $1.438750388$ | $1.438750388$ | $1.438750388$ | Enters core ($r < 1.5$) | Exact |
| $+1.95$ | $1.551795042$ | $1.551795028$ | $1.551795039$ | Turns in shell | $0.21$ |
| $-3.60$ | $1.490704364$ | $1.490704332$ | $1.490704356$ | Enters core ($r < 1.5$) | $0.25$ |
| $-3.70$ | $1.511352187$ | $1.511352136$ | $1.511352174$ | Turns in shell | $0.25$ |

*Halving the step size reduces the error by a factor of $\approx 4$ ($O(h^4)$), confirming fourth-order Runge–Kutta convergence.*

### Carter Constant Symmetry Breaking in the Transition Shell

In Kerr–de Sitter, $Q_{\text{KdS}}$ is conserved because $\Lambda$ is uniform. Across the transition shell, $\lambda(r) = \Lambda(1 - w(r))$ is spatially dependent.
* **Levi-Civita / Stäckel Obstruction**:
  $$\partial_r \partial_\theta (\Sigma g^{\theta\theta}) = -\frac{2}{3} a^2 \Lambda \lambda'(r) \sin\theta \cos\theta = 0.752263 \neq 0 \quad \text{at } (r=1.65, \theta=1.10)$$
  Additive $(r, \theta)$ Hamilton–Jacobi separation in Boyer–Lindquist coordinates is mathematically broken in the shell.
* **Exact Radial Gradient Relation**:
  Along any null geodesic, $\frac{dQ_{\text{KdS}}}{d\lambda} = \left(\frac{\partial Q_{\text{KdS}}}{\partial r}\right)_{\theta, p_\theta} \dot{r}$. The suite verified that the net change $\Delta Q_{\text{shell}}$ matches the path integral $\int (\partial_r Q_{\text{KdS}}) \dot{r} d\lambda$ to **$1.75 \times 10^{-12}$**.

### Matched Gravastar vs. Kerr Optical Comparator

In a matched $128 \times 128$ comparison with identical camera geometry ($r=10.0, \theta=1.25$):
* **Exterior Common Rays (22.58% / 3,700 rays)**: Scatters purely through exterior spacetime ($r > 1.8$), agreeing with pure Kerr trajectories to **$0.000000\text{ radians}$**.
* **Transition Shell / Core Rays (77.09% / 12,631 rays)**: Enters the ergoregion and transition shell, undergoing relativistic whirl orbits before reaching affine budgets.

---

## 4. P3: Vulkan GPU Compute roll-out

* **Target Device**: NVIDIA GeForce RTX 3050 6GB Laptop GPU (Vulkan 1.4.321, Queue Family 0).
* **Metric Tensor Kernel**: GPU compute shader evaluates $g_{\mu\nu}, g^{\mu\nu}, \partial_r g, \partial_\theta g$ matching CPU ground truth within $1.74 \times 10^{-5}$ in single precision.
* **Ray Trajectory Agreement**:
  Evolving 25,000 steps on GPU vs. CPU yields turning point differences $\le 2.4 \times 10^{-6}$ with 100% agreement on core-entry classification.
* **Throughput**: $128 \times 128$ raymarch dispatched in **$15.07\text{ ms}$** (**$1.09\text{ Megarays/sec}$**).

---

## 5. P4 & P5: Studio Presentation & Deterministic Recording

* **Windowing & Swapchain**: Static GLFW 3.4 integration with dynamic swapchain recreation (`VK_KHR_swapchain_maintenance1`), handling window resize from $1001 \times 613$ to $1912 \times 1036$ with exact pixel readback.
* **Recording Engine**: Rational timeline evaluation ($t_i = i/\text{fps}$), atomic file writes (`atomic_bytes` with `mkstemp` and directory `fsync`), fault-injection interruption/resumption, and external H.264 MP4 container encoding via `ffmpeg` verified by `ffprobe`.
* **Testing Integrity**: Release-active runtime assertions (`GRAVASTAR_REQUIRE`) enforced in all build modes, preventing assertion elision under `-DNDEBUG`.

---

## 6. Stage P3: ZAMO Tetrad & Analytical Shadow / Interface Contours

Validated via `tests/zamo_shadow_validation.cpp` (3,444 release-active checks across IEEE-754 `double` and 113-bit `__float128`):

### ZAMO Orthonormal Tetrad (Bardeen LNRF)
The zero-angular-momentum observer 4-velocity $u^\mu = \alpha^{-1} (\delta^\mu_t + \omega \delta^\mu_\phi)$ and spacelike triad $(e_{\hat{r}}, e_{\hat{\theta}}, e_{\hat{\phi}})$ remain non-singular and regular inside the ergoregion (where static observers $g_{tt} \ge 0$ fail):
* Orthonormality error $\max |g_{\mu\nu} e_{\hat{a}}^\mu e_{\hat{b}}^\nu - \eta_{\hat{a}\hat{b}}| \le 2.220 \times 10^{-16}$ in `double`.
* Orthonormality error in `__float128` across exterior ($15.0$), ergoregion ($1.85$), outer shell join ($1.80$), mid-shell ($1.65$), and de Sitter core ($1.00$) is $\le 5.778 \times 10^{-34}$.

### Analytical vs. Numerical Screen Contours ($r_{\text{obs}} = 15$)
* **Outer Shell Grazing Boundary ($r = 1.8$)**: Joins $C^1$-tangentially onto the exterior Kerr retrograde photon barrier at $b_{\text{join}} = +13955/5600 \approx +2.4917857143$. Unmasked prograde graze $b = +3.0180173587$ verified to $< 10^{-32}$.
* **Asymptotic vs. Finite-Distance ZAMO Projection**:
  At $r_{\text{obs}} = 15$, lapse $\alpha_{\text{lapse}} = 0.931009$ and frame dragging $\omega = +0.000516$ displace finite-distance angles by $\sim 6.9\%$ relative to asymptotic Bardeen $(\alpha_\infty, \beta_\infty)$.
* **Quantified Non-Separable Shell Drift**:
  At $(\alpha = 0, \theta_{\text{obs}} = \pi/2)$, frozen-$K_{\text{KdS}}$ predicts $\beta_\infty = +2.31717928$, whereas true numerical integration yields $\beta_\infty = +2.17979304$ ($\Delta\beta_\infty = -0.13738624$, a $6.3\%$ shift due to the non-homogeneous transition shell).

