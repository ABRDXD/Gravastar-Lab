# SPDX-License-Identifier: GPL-3.0-or-later
"""Reproducible P1 scientific acceptance; stdlib only. Nonzero exit on failure."""
import argparse
from datetime import datetime, timezone
from decimal import Decimal as D, getcontext, localcontext
from fractions import Fraction as F
import hashlib
import json
import math
from pathlib import Path
import platform
import subprocess
import sys
import time

from reference import (PI, curvature, derivatives, flatten, floats, inverse, matrix,
                       metric, normalized_error, parameters, radial_derivative, trig)
from polynomial import (bernstein_positive_bound, count_roots, derivative,
                        isolate_roots, shell_delta, value)


def cpp_batch(executable, cases):
    lines = []
    for family, p, r, theta in cases:
        lines.append(" ".join(map(str, [family, p["M"], p["a"], p["Lambda"],
                                        p["r_in"], p["r_out"], r, theta])))
    proc = subprocess.run([str(executable), "batch"], input="\n".join(lines)+"\n",
                          capture_output=True, text=True, timeout=180, check=False)
    if proc.returncode:
        raise RuntimeError(f"C++ batch failed ({proc.returncode}): {proc.stderr[:2000]}")
    rows = [json.loads(line) for line in proc.stdout.splitlines()]
    if len(rows) != len(cases):
        raise RuntimeError("C++ batch output count differs from input")
    for row in rows:
        if "error" in row:
            raise RuntimeError(f"unexpected metric rejection: {row}")
        if any(not math.isfinite(x) for key in ("metric", "inverse", "dg", "ddg", "drrr",
                                               "ricci", "ricci_scalar", "kretschmann")
               for x in flatten(row[key])):
            raise RuntimeError("non-finite C++ output")
    return rows


class Checks:
    def __init__(self):
        self.rows = []

    def upper(self, name, measured, threshold, **details):
        passed = math.isfinite(float(measured)) and measured <= D(str(threshold))
        self.rows.append(dict(name=name, passed=passed, measured=float(measured),
                              threshold=float(threshold), comparison="<=", **details))

    def truth(self, name, passed, **details):
        self.rows.append(dict(name=name, passed=bool(passed), **details))


def domain_checks(executable, p, cfg, checks):
    box = cfg["domain"]
    rmin, rmax = D(box["r_min"]), D(box["r_max"])
    tmin = D(box["theta_min"])
    radii = [rmin+(rmax-rmin)*i/(box["radial_samples"]-1) for i in range(box["radial_samples"])]
    # Uniform broad grid plus dense shell and join-adjacent probes.
    radii += [p["r_in"]+(p["r_out"]-p["r_in"])*i/80 for i in range(81)]
    radii += [join+sgn*D("1e-7") for join in (p["r_in"], p["r_out"]) for sgn in (-1, 0, 1)]
    radii = sorted(set(radii))
    angles = [tmin+(PI-2*tmin)*j/(box["angular_samples"]-1) for j in range(box["angular_samples"])]
    cases = [("toy", p, r, th) for r in radii for th in angles]
    rows = cpp_batch(executable, cases)
    min_eig, max_condition, min_phi, min_lapse2 = math.inf, 0., math.inf, math.inf
    max_identity, max_det = 0., D(0)
    signature_failures, causal_failures = [], []
    extrema = {k: [math.inf, -math.inf] for k in ("ricci_scalar", "kretschmann", "Delta")}
    for index, (row, case) in enumerate(zip(rows, cases)):
        g, inv = row["metric"], row["inverse"]
        tt, tp, pp = g[0][0], g[0][3], g[3][3]
        disc = math.hypot(tt-pp, 2*tp)
        positive = (tt+pp+disc)/2
        # Product/positive avoids cancellation in negative eigenvalue.
        negative = (tt*pp-tp*tp)/positive
        eigs = [negative, positive, g[1][1], g[2][2]]
        if sum(x < 0 for x in eigs) != 1 or sum(x > 0 for x in eigs) != 3:
            signature_failures.append(index)
        lo, hi = min(map(abs, eigs)), max(map(abs, eigs))
        min_eig, max_condition = min(min_eig, lo), max(max_condition, hi/lo)
        min_phi = min(min_phi, pp)
        lapse2 = (tp*tp-tt*pp)/pp
        min_lapse2 = min(min_lapse2, lapse2)
        if pp <= 0 or lapse2 <= 0:
            causal_failures.append(index)
        ident = max(abs(sum(g[i][k]*inv[k][j] for k in range(4))-(i == j))
                    for i in range(4) for j in range(4))
        max_identity = max(max_identity, ident)
        _, _, _, theta = case
        s, c = trig(theta)
        sigma = case[2]**2+p["a"]**2*c*c
        from reference import transition
        _, lam = transition(case[2], p)
        xi = 1+p["Lambda"]*lam*p["a"]**2/3
        exact_det = -sigma**2*s*s/xi**4
        max_det = max(max_det, normalized_error(row["determinant"], exact_det))
        for k in extrema:
            extrema[k][0] = min(extrema[k][0], row[k])
            extrema[k][1] = max(extrema[k][1], row[k])
    checks.truth("sampled Lorentzian inertia", not signature_failures, failures=signature_failures)
    checks.truth("sampled azimuthal causality and ZAMO existence", not causal_failures,
                 failures=causal_failures)
    checks.upper("sampled inverse identity", D(str(max_identity)), cfg["thresholds"]["identity"])
    checks.upper("sampled determinant identity", max_det, cfg["thresholds"]["identity"])
    return dict(samples=len(cases), radial_count=len(radii), angular_count=len(angles),
                box=box, minimum_absolute_coordinate_eigenvalue=min_eig,
                maximum_coordinate_spectral_condition=max_condition,
                minimum_g_phiphi=min_phi, minimum_zamo_lapse_squared=min_lapse2,
                extrema=extrema, errors_are_coordinate_basis_dependent=True)


def radial_certificate(p, checks):
    delta = shell_delta(p)
    count = count_roots(delta)
    lower = bernstein_positive_bound(delta) if count == 0 else None
    critical = isolate_roots(derivative(delta))
    cp = [{"u_bracket": [str(l), str(h)],
           "r_midpoint": float(F(str(p["r_in"]))+(l+h)/2*F(str(p["r_out"]-p["r_in"]))),
           "Delta_midpoint": float(value(delta, (l+h)/2))} for l, h in critical]
    core_positive = p["Lambda"]*p["r_in"]**2 < 3
    ext_positive = p["r_out"] > p["M"] and (
        p["r_out"]**2-2*p["M"]*p["r_out"]+p["a"]**2 > 0)
    checks.truth("exact shell radial null-surface root count", count == 0, roots=count)
    checks.truth("exact shell positive Bernstein certificate", lower is not None and lower > 0)
    checks.truth("analytic core/exterior radial positivity", core_positive and ext_positive)
    # Explicit even-multiplicity and closely spaced root regression cases.
    checks.truth("Sturm catches tangency", count_roots([F(1,4), F(-1), F(1)]) == 1)
    checks.truth("Sturm catches close root pair",
                 count_roots([F(1,4)-F(1,10**12), F(-1), F(1)]) == 2)
    return dict(shell_roots=count, shell_delta_lower_bound_exact=str(lower),
                shell_delta_lower_bound=float(lower) if lower else None,
                stationary_points=cp, core_positive=bool(core_positive),
                exterior_positive=bool(ext_positive),
                scope="local radial null-surface exclusion; no global event-horizon theorem",
                method="exact rational Sturm including repeated roots; Bernstein convex-hull bound")


def independent_vectors(executable, p, cfg, checks):
    cases = []
    # All regions, exact joins, near joins, equator, and conditioning boundaries.
    for r in ["0.25", "1", "1.4999", "1.5", "1.5001", "1.53", "1.575",
              "1.65", "1.725", "1.77", "1.7999", "1.8", "1.8001", "3", "10"]:
        for th in [D("0.05"), PI/2, PI-D("0.05")]:
            cases.append(("toy", p, D(r), th))
    for family, r in [("flat", "0.25"), ("de_sitter", "1"), ("kerr", "3")]:
        cases.append((family, p, D(r), D("1.1")))
    p0 = {**p, "a": D(0)}
    cases += [("kerr", p0, D(3), D("1.1")), ("toy", p0, D("1.65"), D("1.1"))]
    # Signed spin and angular reflection test all derivative axes implicitly.
    cases.append(("toy", {**p, "a": -p["a"]}, D("1.65"), D("1.1")))
    cpp = cpp_batch(executable, cases)
    maxima = {k: D(0) for k in ("metric", "inverse", "dg", "ddg", "drrr", "curvature")}
    worst = {}
    analytic = D(0)
    samples = []
    for case, row in zip(cases, cpp):
        family, q, r, th = case
        # At joins, center differences span C3 join: analytic continuation into
        # the reference region avoids the C4 jump while testing common jets.
        branch = ("core" if r == q["r_in"] else "exterior" if r == q["r_out"] else None)
        g, dg, ddg = derivatives(r, th, q, family, h=D(cfg["reference"]["finite_difference_step"]),
                                branch=branch)
        curv = curvature(g, dg, ddg)
        rrr = radial_derivative(r, th, q, 3, family, branch)
        refs = {"metric": g, "inverse": inverse(g), "dg": dg, "ddg": ddg, "drrr": rrr}
        errors = {k: normalized_error(row[k], ref) for k, ref in refs.items()}
        errors["curvature"] = max(normalized_error(row[k], curv[k])
                                  for k in ("ricci", "ricci_scalar", "kretschmann"))
        for k, err in errors.items():
            if err > maxima[k]:
                maxima[k] = err
                worst[k] = dict(family=family, a=str(q["a"]), r=str(r), theta=str(th))
        if family == "flat":
            analytic = max(analytic, abs(D(str(row["ricci_scalar"]))),
                           abs(D(str(row["kretschmann"]))))
        elif family == "de_sitter" or (family == "toy" and r <= q["r_in"]):
            analytic = max(analytic, normalized_error(row["ricci_scalar"], 4*q["Lambda"]),
                           normalized_error(row["kretschmann"], D(8)/3*q["Lambda"]**2),
                           normalized_error(row["ricci"], [[q["Lambda"]*v for v in rr] for rr in g]))
        elif family == "kerr" or (family == "toy" and r >= q["r_out"]):
            _, c = trig(th)
            b = (q["a"]*c)**2
            exact_k = 48*q["M"]**2*(r**6-15*r**4*b+15*r*r*b*b-b**3)/(r*r+b)**6
            analytic = max(analytic, normalized_error(row["kretschmann"], exact_k),
                           max(map(abs, map(D, flatten(row["ricci"])))))
        if family == "toy" and q["a"] == 0:
            from reference import transition
            mu, lam = transition(r, q)
            f = 1-2*q["M"]*mu/r-q["Lambda"]*lam*r*r/3
            s, _ = trig(th)
            expected = [[-f, 0, 0, 0], [0, 1/f, 0, 0], [0, 0, r*r, 0],
                        [0, 0, 0, r*r*s*s]]
            analytic = max(analytic, normalized_error(row["metric"], expected))
        samples.append(dict(family=family, a=str(q["a"]), r=str(r), theta=str(th),
                            errors=floats(errors), ricci_scalar=float(curv["ricci_scalar"]),
                            kretschmann=float(curv["kretschmann"])))
    for k, maximum in maxima.items():
        checks.upper(f"independent {k}", maximum, cfg["thresholds"][k], worst_case=worst.get(k))
    checks.upper("analytic limits and invariant identities", analytic,
                 cfg["thresholds"]["analytic_curvature"])
    return dict(samples=samples, maxima=floats(maxima), analytic_maximum=float(analytic))


def matching_checks(executable, p, cfg, checks):
    from polynomial import derivative as pd, value as pv
    s = [F(v) for v in [0, 0, 0, 0, 35, -84, 70, -20]]
    exact = pv(s, F(0)) == 0 and pv(s, F(1)) == 1
    for _ in range(3):
        s = pd(s)
        exact = exact and pv(s, F(0)) == 0 and pv(s, F(1)) == 0
    checks.truth("exact rational C3 transition endpoint conditions", exact)
    endpoint_max = D(0)
    extrinsic_max = D(0)
    approach = []
    for join, branch in [(p["r_in"], "core"), (p["r_out"], "exterior")]:
        for th in [D(".1"), D("1.1"), PI/2]:
            for order in range(4):
                a = radial_derivative(join, th, p, order, branch=branch)
                b = radial_derivative(join, th, p, order, branch="shell")
                endpoint_max = max(endpoint_max, normalized_error(a, b))
            ga, da, _ = derivatives(join, th, p, branch=branch)
            gb, db, _ = derivatives(join, th, p, branch="shell")
            ka = [[da[1][i][j]/(2*ga[1][1].sqrt()) for j in (0, 2, 3)] for i in (0, 2, 3)]
            kb = [[db[1][i][j]/(2*gb[1][1].sqrt()) for j in (0, 2, 3)] for i in (0, 2, 3)]
            extrinsic_max = max(extrinsic_max, normalized_error(ka, kb))
        # Numerical production approach on each side vs analytic reference extension.
        for side in [-1, 1]:
            errors = []
            for h in [D("1e-3"), D("1e-4"), D("1e-5")]:
                r, th = join+side*h, D("1.1")
                row = cpp_batch(executable, [("toy", p, r, th)])[0]
                source = [row["metric"], row["dg"][1], row["ddg"][1][1], row["drrr"]]
                errors.append([float(normalized_error(source[k], radial_derivative(r, th, p, k,
                                                                                   branch=branch)))
                               for k in range(4)])
            shell_side = (branch == "core" and side == 1) or (branch == "exterior" and side == -1)
            checks.truth(f"join {join} side {side} C3 approach",
                         all(errors[-1][k] < errors[0][k]/20 for k in range(4)) if shell_side
                         else max(flatten(errors)) < cfg["thresholds"]["drrr"])
            approach.append(dict(join=str(join), side=side, steps=["1e-3", "1e-4", "1e-5"],
                                 errors_by_derivative_order=errors))
    checks.upper("independent analytic branch matching through order 3", endpoint_max,
                 cfg["thresholds"]["matching"])
    checks.upper("induced extrinsic curvature matching", extrinsic_max, cfg["thresholds"]["matching"])
    return dict(endpoint_maximum=float(endpoint_max), extrinsic_maximum=float(extrinsic_max),
                approach=approach)


def convergence_checks(executable, p, cfg, checks):
    r, th = D("1.62"), D("1.1")
    target = cpp_batch(executable, [("toy", p, r, th)])[0]
    rows = []
    for h in map(D, [".004", ".002", ".001"]):
        g, dg, ddg = derivatives(r, th, p, h=h)
        curv = curvature(g, dg, ddg)
        rows.append(dict(h=str(h), dg=float(normalized_error(dg, target["dg"])),
                         ddg=float(normalized_error(ddg, target["ddg"])),
                         kretschmann=float(normalized_error(curv["kretschmann"],
                                                            target["kretschmann"]))))
    ratios = {k: [rows[i][k]/rows[i+1][k] for i in (0, 1)]
              for k in ("dg", "ddg", "kretschmann")}
    checks.truth("finite-difference derivative/curvature convergence",
                 min(flatten(list(ratios.values()))) > cfg["thresholds"]["convergence_ratio_min"])
    # Precision/step refinement independently verifies the decimal reference.
    a = derivatives(r, th, p, h=D("1e-8"))
    b = derivatives(r, th, p, h=D("1e-9"))
    refine = max(normalized_error(a[i], b[i]) for i in range(3))
    with localcontext() as context:
        context.prec = 85
        c = derivatives(r, th, p, h=D("1e-9"))
    precision = max(normalized_error(b[i], c[i]) for i in range(3))
    checks.upper("reference step refinement", refine, cfg["thresholds"]["reference_refinement"])
    checks.upper("reference precision refinement 70 to 85 digits", precision,
                 cfg["thresholds"]["reference_refinement"])
    # Trig identities plus an independent standard lib double calculation.
    trig_error = max(abs(trig(x)[0]**2+trig(x)[1]**2-1)
                     for x in (D(".05"), PI/2, PI-D(".05")))
    checks.upper("Decimal trigonometry identity", trig_error, D("1e-65"))
    checks.upper("Decimal sin against platform libm",
                 max(abs(trig(x)[0]-D(str(math.sin(float(x))))) for x in (D(".05"), D("1.1"))),
                 D("3e-16"))
    return dict(refinement=rows, error_ratios=ratios, reference_step_error=float(refine),
                reference_precision_error=float(precision))


def energy_diagnostic(p):
    # A null vector k = e_0 + n^i e_i has Gkk=Rkk. Negative witness proves NEC
    # violation but absence of a negative sample would not prove any condition.
    minimum = None
    for u in [D(".1"), D(".25"), D(".5"), D(".75"), D(".9")]:
        r, th = p["r_in"]+u*(p["r_out"]-p["r_in"]), PI/2
        g, dg, ddg = derivatives(r, th, p)
        ric = curvature(g, dg, ddg)["ricci"]
        omega = -g[0][3]/g[3][3]
        alpha = ((g[0][3]**2-g[0][0]*g[3][3])/g[3][3]).sqrt()
        for axis in (1, 2, 3):
            for sign in (-1, 1):
                k = [1/alpha, D(0), D(0), omega/alpha]
                k[axis] += D(sign)/g[axis][axis].sqrt()
                null = sum(g[i][j]*k[i]*k[j] for i in range(4) for j in range(4))
                tkk = sum(ric[i][j]*k[i]*k[j] for i in range(4) for j in range(4))/(8*PI)
                if minimum is None or tkk < minimum["Tkk"]:
                    minimum = dict(Tkk=tkk, null_residual=abs(null), r=r, theta=th,
                                   spatial_axis=axis, sign=sign, null_vector=k)
    return dict(minimum_witness=floats(minimum),
                finding="NEC violation witnessed" if minimum["Tkk"] < -D("1e-20") else
                        "No violation found in finite directional sample; inconclusive",
                cosmological_term="Included once in T=G/(8*pi)",
                tensor_classification="not assessed", dynamical_stability="not assessed",
                excluded_from_mathematical_acceptance=True)


def write_report(path, report):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name+".tmp")
    tmp.write_text(json.dumps(report, indent=2, allow_nan=False)+"\n")
    tmp.replace(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--config", type=Path, default=Path("benchmarks/rotating_v1.json"))
    parser.add_argument("--output", type=Path, default=Path("results/rotating_v1.json"))
    args = parser.parse_args()
    started = time.monotonic()
    cfg = json.loads(args.config.read_text())
    if cfg["schema_version"] != 1:
        parser.error("unsupported benchmark schema")
    getcontext().prec = cfg["reference"]["decimal_digits"]
    p = parameters(cfg["parameters"])
    checks = Checks()
    report = dict(schema_version=1, model=cfg["model"], preset_id=cfg["preset_id"],
                  configuration=cfg, created_utc=datetime.now(timezone.utc).isoformat(),
                  python=sys.version, platform=platform.platform(),
                  executable_sha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
                  input_sha256=hashlib.sha256(args.config.read_bytes()).hexdigest(),
                  error_norm="max components |C++-reference|/(1+|reference|), unless explicitly absolute",
                  source_reference="https://arxiv.org/html/2309.00551v2",
                  published_figure_reproduction="unresolved: exact source radii unavailable",
                  full_interior_optics="not implemented; extension and axis charts unresolved")
    stages = [("radial_certificate", lambda: radial_certificate(p, checks)),
              ("domain", lambda: domain_checks(args.executable, p, cfg, checks)),
              ("independent", lambda: independent_vectors(args.executable, p, cfg, checks)),
              ("matching", lambda: matching_checks(args.executable, p, cfg, checks)),
              ("convergence", lambda: convergence_checks(args.executable, p, cfg, checks)),
              ("energy", lambda: energy_diagnostic(p))]
    try:
        for name, action in stages:
            print(f"Validating {name}...", flush=True)
            report[name] = action()
    except Exception as exc:
        checks.truth("benchmark execution", False, reason=f"{type(exc).__name__}: {exc}")
        report["execution_error"] = f"{type(exc).__name__}: {exc}"
    report["checks"] = checks.rows
    report["passed"] = bool(checks.rows) and all(row["passed"] for row in checks.rows)
    report["elapsed_seconds"] = time.monotonic()-started
    try:
        sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"tools"))
        from source_manifest import build_manifest
        report["computational_source_sha256"] = build_manifest(Path(__file__).resolve().parents[1])[
            "computational_sha256"]
    except ImportError:
        report["computational_source_sha256"] = "manifest tool unavailable"
    write_report(args.output, report)
    failed = [row for row in checks.rows if not row["passed"]]
    print(json.dumps(dict(passed=report["passed"], checks=len(checks.rows),
                          failed=failed, report=str(args.output)), indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
