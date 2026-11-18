// SPDX-License-Identifier: GPL-2.0-or-later
// Analytic edge cases for the actual production root/interval implementation.
#include "core/bseries.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main(){
    using fs::BSeriesResult;int checked=0;
    auto expect=[&](bool b){++checked;if(!b)throw std::runtime_error("root assertion "+std::to_string(checked));};
    auto near=[](double a,double b){return std::abs(a-b)<1e-10;};
    BSeriesResult p;p.torque={-.192,1.36,-2.2,1}; // (J-.2)(J-.8)(J-1.2)
    auto roots=p.torqueIntersections(0,0,1.5);
    expect(roots.size()==3);expect(near(roots[0],.2)&&near(roots[1],.8)&&near(roots[2],1.2));
    p.torque={.25,-1,1,0}; // tangent at .5, no sign change
    roots=p.torqueIntersections(0,0,1.5);expect(roots.size()==1&&near(roots[0],.5));
    p.torque={0,1,0,0};roots=p.torqueIntersections(0,0,1);expect(roots.size()==1&&roots[0]==0);
    roots=p.torqueIntersections(1,0,1);expect(roots.size()==1&&roots[0]==1);
    expect(p.torqueIntersections(2,0,1).empty());
    p.thrust={.21,-1,1,0};p.torque={1,0,0,0}; // positive ends, negative middle
    expect(!p.propulsiveInterval(0,1));expect(p.propulsiveInterval(0,.2));
    p.thrust={1,0,0,0};p.torque={.25,-1,1,0};expect(!p.propulsiveInterval(0,1));
    p.torque={1,0,0,0};expect(p.propulsiveInterval(0,1.5));
    bool rejected=false;try{p.torqueIntersections(0,0,1.6);}catch(const std::invalid_argument&){rejected=true;}expect(rejected);
    std::cout<<"analytic assertions: "<<checked<<'\n';
}
