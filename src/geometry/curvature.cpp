// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/geometry/metric.hpp"
namespace gravastar {
Curvature curvature(const Evaluation& m) {
    Tensor3 gamma{};
    Tensor4 dgamma{};
    Tensor3 dinverse{};
    for(int k=0;k<4;++k) for(int a=0;a<4;++a) for(int b=0;b<4;++b)
        for(int i=0;i<4;++i) for(int j=0;j<4;++j)
            dinverse[k][a][b]-=m.inverse[a][i]*m.first[k][i][j]*m.inverse[j][b];
    for(int a=0;a<4;++a) for(int b=0;b<4;++b) for(int c=0;c<4;++c)
        for(int e=0;e<4;++e) {
            const double q=m.first[b][e][c]+m.first[c][e][b]-m.first[e][b][c];
            gamma[a][b][c]+=0.5*m.inverse[a][e]*q;
            for(int k=0;k<4;++k)
                dgamma[k][a][b][c]+=0.5*(dinverse[k][a][e]*q+m.inverse[a][e]*(
                    m.second[k][b][e][c]+m.second[k][c][e][b]-m.second[k][e][b][c]));
        }
    Tensor4 R{};
    Curvature out;
    for(int a=0;a<4;++a) for(int b=0;b<4;++b) for(int c=0;c<4;++c) for(int d=0;d<4;++d) {
        double v=dgamma[c][a][d][b]-dgamma[d][a][c][b];
        for(int e=0;e<4;++e) v+=gamma[a][c][e]*gamma[e][d][b]-gamma[a][d][e]*gamma[e][c][b];
        R[a][b][c][d]=v;
    }
    for(int b=0;b<4;++b) for(int d=0;d<4;++d) {
        for(int a=0;a<4;++a) out.ricci[b][d]+=R[a][b][a][d];
        out.scalar+=m.inverse[b][d]*out.ricci[b][d];
    }
    for(int a=0;a<4;++a) for(int b=0;b<4;++b) for(int c=0;c<4;++c) for(int d=0;d<4;++d)
        for(int e=0;e<4;++e) out.riemann_lower[a][b][c][d]+=m.g[a][e]*R[e][b][c][d];
    // Raise one index at a time (4*4^5, rather than a direct 4^8 contraction).
    Tensor4 raised=out.riemann_lower;
    for(int axis=0;axis<4;++axis) {
        Tensor4 next{};
        for(int a=0;a<4;++a) for(int b=0;b<4;++b) for(int c=0;c<4;++c) for(int d=0;d<4;++d) {
            const std::array<int,4> target{a,b,c,d};
            for(int e=0;e<4;++e) {
                auto source=target; source[axis]=e;
                next[a][b][c][d]+=m.inverse[target[axis]][e]*raised[source[0]][source[1]][source[2]][source[3]];
            }
        }
        raised=next;
    }
    for(int a=0;a<4;++a) for(int b=0;b<4;++b) for(int c=0;c<4;++c) for(int d=0;d<4;++d)
        out.kretschmann+=out.riemann_lower[a][b][c][d]*raised[a][b][c][d];
    return out;
}
} // namespace gravastar
