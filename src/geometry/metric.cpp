// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/geometry/metric.hpp"
#include "gravastar/math/jet.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace gravastar {
using math::Jet;
namespace {
Jet smoothstep(const Jet& u) {
    return u*u*u*u*(Jet(35)+u*(Jet(-84)+u*(Jet(70)-Jet(20)*u)));
}
}
Geometry::Geometry(Parameters p): parameters_(p) {
    for (double v : {p.mass,p.spin,p.lambda,p.r_in,p.r_out})
        if (!std::isfinite(v)) throw std::invalid_argument("nonfinite_parameters");
    // Zero mass is supported for analytic flat/de Sitter reference limits.
    if (p.mass<0 || p.r_in<=0 || p.r_out<=p.r_in)
        throw std::invalid_argument("invalid_parameters");
}
MetricResult Geometry::evaluate(double radius, double theta, Region region) const {
    if (!std::isfinite(radius)||!std::isfinite(theta)) return DomainError{"nonfinite_coordinates"};
    if (radius<=0 || theta<=0 || theta>=std::numbers::pi) return DomainError{"chart_boundary"};
    const auto& p=parameters_;
    if (region==Region::automatic)
        region=radius<=p.r_in?Region::core:(radius>=p.r_out?Region::exterior:Region::shell);
    // Explicit regions allow analytic one-sided join tests only, not extrapolation.
    if ((region==Region::core && radius>p.r_in) ||
        (region==Region::exterior && radius<p.r_out) ||
        (region==Region::shell && (radius<p.r_in || radius>p.r_out)))
        return DomainError{"region_mismatch"};
    const Jet r=Jet::variable(radius,0), th=Jet::variable(theta,1);
    Jet mu, lw;
    if (region==Region::core) { mu=Jet(0); lw=Jet(1); }
    else if (region==Region::exterior) { mu=Jet(1); lw=Jet(0); }
    else {
        const Jet u=(r-Jet(p.r_in))/Jet(p.r_out-p.r_in);
        if (u.value()<=0.5) { mu=smoothstep(u); lw=Jet(1)-mu; }
        else { lw=smoothstep(Jet(1)-u); mu=Jet(1)-lw; }
    }
    const Jet a(p.spin), L=Jet(p.lambda)*lw, s=sin(th), c=cos(th);
    const Jet rho=r*r+a*a*c*c, A=r*r+a*a;
    const Jet D=A*(Jet(1)-L*r*r/Jet(3))-Jet(2*p.mass)*mu*r;
    const Jet T=Jet(1)+L*a*a*c*c/Jet(3), X=Jet(1)+L*a*a/Jet(3);
    for (double v : {rho.value(),D.value(),T.value(),X.value()})
        if (v==0.0 || !std::isfinite(v)) return DomainError{"singular_chart"};
    std::array<std::array<Jet,4>,4> g{};
    g[0][0]=(a*a*T*s*s-D)/(rho*X*X);
    g[0][3]=g[3][0]=a*s*s*(D-A*T)/(rho*X*X);
    g[3][3]=s*s*(A*A*T-a*a*s*s*D)/(rho*X*X);
    g[1][1]=rho/D; g[2][2]=rho/T;
    Evaluation out;
    out.delta=D.value(); out.mu=mu.value(); out.lambda_weight=lw.value();
    out.determinant=-(rho*rho*s*s/(X*X*X*X)).value();
    for (int i=0;i<4;++i) for(int j=0;j<4;++j) {
        const Jet& v=g[i][j];
        out.g[i][j]=v.value();
        out.first[1][i][j]=v.derivative(1,0); out.first[2][i][j]=v.derivative(0,1);
        out.second[1][1][i][j]=v.derivative(2,0);
        out.second[1][2][i][j]=out.second[2][1][i][j]=v.derivative(1,1);
        out.second[2][2][i][j]=v.derivative(0,2);
        out.radial_third[i][j]=v.derivative(3,0);
        for(int k=0;k<=3;++k) for(int l=0;l<=3-k;++l)
            if (!std::isfinite(v.c[k][l])) return DomainError{"nonfinite_evaluation"};
    }
    const double b=out.g[0][0]*out.g[3][3]-out.g[0][3]*out.g[0][3];
    if (b==0 || out.determinant==0) return DomainError{"singular_metric"};
    out.inverse[0][0]=out.g[3][3]/b; out.inverse[3][3]=out.g[0][0]/b;
    out.inverse[0][3]=out.inverse[3][0]=-out.g[0][3]/b;
    out.inverse[1][1]=1/out.g[1][1]; out.inverse[2][2]=1/out.g[2][2];
    for(const auto& row:out.inverse) for(double v:row)
        if(!std::isfinite(v)) return DomainError{"nonfinite_inverse"};
    return out;
}
} // namespace gravastar
