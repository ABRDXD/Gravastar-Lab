// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/geometry/metric.hpp"
#include "gravastar/math/jet.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
using namespace gravastar;
using gravastar::math::Jet;
Jet smoothstep(const Jet& u) {
    return u*u*u*u*(Jet(35)+u*(Jet(-84)+u*(Jet(70)-Jet(20)*u)));
}
void require(bool ok, const char* why) { if(!ok) throw std::runtime_error(why); }
void near(double a,double b,double tol=1e-11) { require(std::abs(a-b)<=tol*(1+std::abs(b)),"numerical comparison failed"); }
Evaluation metric(const Geometry& g,double r,double th,Region reg=Region::automatic) {
    auto x=g.evaluate(r,th,reg); require(std::holds_alternative<Evaluation>(x),"unexpected domain error"); return std::get<Evaluation>(x);
}
int main() {
    try {
        const Jet x=Jet::variable(0.3,0), y=Jet::variable(0.7,1);
        const Jet f=x*x*y+sin(y)/x;
        near(f.derivative(1,0),0.6*0.7-std::sin(0.7)/0.09);
        near(f.derivative(1,1),0.6-std::cos(0.7)/0.09);
        near(f.derivative(3,0),-6*std::sin(0.7)/std::pow(0.3,4));
        for(double u:{0.0,1.0}) {
            const Jet s=smoothstep(Jet::variable(u,0)); near(s.value(),u);
            for(int k=1;k<=3;++k) near(s.derivative(k,0),0);
        }
        const Geometry model({1,.875,.5,1.8,2.2});
        for(double r:{.2,1.5,1.8,1.9,2.,2.1,2.2,4.,12.}) for(double th:{.05,.7,1.57,3.09}) {
            const auto m=metric(model,r,th);
            for(int i=0;i<4;++i) for(int j=0;j<4;++j) {
                near(m.g[i][j],m.g[j][i]);
                double v=0; for(int k=0;k<4;++k) v+=m.g[i][k]*m.inverse[k][j];
                near(v,i==j?1:0);
            }
            require(m.delta>0,"preset has a radial null surface");
        }
        for(double r:{1.8,2.2}) for(double th:{.2,1.,2.7}) {
            const auto a=metric(model,r,th,Region::shell);
            const auto b=metric(model,r,th,r==1.8?Region::core:Region::exterior);
            for(int i=0;i<4;++i) for(int j=0;j<4;++j) {
                near(a.g[i][j],b.g[i][j]);
                near(a.first[1][i][j],b.first[1][i][j],1e-10);
                near(a.second[1][1][i][j],b.second[1][1][i][j],1e-9);
                near(a.radial_third[i][j],b.radial_third[i][j],1e-8);
            }
        }
        const Geometry ds({0,.875,.5,3,4}), flat({0,.875,0,3,4});
        for(double th:{.1,.7,1.57,2.8}) {
            const auto m=metric(ds,1.2,th); const auto c=curvature(m);
            near(c.scalar,2,1e-10); near(c.kretschmann,2./3,1e-10);
            for(int a=0;a<4;++a) for(int b=0;b<4;++b) near(c.ricci[a][b],.5*m.g[a][b],1e-10);
            const auto fcurv=curvature(metric(flat,1.2,th));
            near(fcurv.scalar,0,1e-10); near(fcurv.kretschmann,0,1e-10);
        }
        const Geometry schwarz({1,0,0,.5,1});
        const auto s=curvature(metric(schwarz,4,.8)); near(s.scalar,0); near(s.kretschmann,48/std::pow(4.,6));
        require(std::holds_alternative<DomainError>(schwarz.evaluate(2,1)),"Kerr horizon must be excluded");
        for(auto pair:{std::pair{0.,1.},std::pair{1.,0.},std::pair{1.,std::numbers::pi},std::pair{1.,std::numeric_limits<double>::quiet_NaN()}})
            require(std::holds_alternative<DomainError>(model.evaluate(pair.first,pair.second)),"invalid chart accepted");
        require(std::holds_alternative<DomainError>(model.evaluate(3,1,Region::shell)),"shell extrapolation accepted");
        bool rejected=false; try { Geometry invalid({1,0,.5,2,1}); } catch(const std::invalid_argument&) { rejected=true; }
        require(rejected,"invalid radii accepted");
        std::cout<<"geometry tests passed: Taylor algebra, endpoints, identities, C3 joins, exact curvature, domain errors\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
