// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/bseries.h"
#include "core/propeller_design.h"
#include <cmath>
#include <stdexcept>
#include <iostream>
int main(){
    int n=0;auto check=[&](bool b){++n;if(!b)throw std::runtime_error("design assertion "+std::to_string(n));};
    fs::BSeriesResult p;p.thrust={.25,0,0,0};
    auto a=p.thrustIdentity(1,2),b=p.thrustIdentity(1,4);
    check(a.size()==1&&std::abs(a[0]-.5)<1e-12);check(b.size()==1&&std::abs(b[0]-std::sqrt(.5))<1e-12);
    p.thrust={.25,-1,2,0};a=p.thrustIdentity(1,2);check(a.size()==1&&std::abs(a[0]-.5)<1e-12);
    bool rejected=false;try{p.thrustIdentity(1,3);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
    fs::PropellerDesignInput in{10,30,1025,.1,.1,1,.98,.98,.6,.5,1.4,25,4,1};
    int calls=0;bool cancelled=false;
    try{fs::designPropeller(in,fs::PropellerConstraint::Diameter,3,[&]{if(++calls==2)throw std::runtime_error("cancel");});}
    catch(const std::runtime_error&){cancelled=true;}check(cancelled&&calls==2);
    auto full=fs::bSeries(.6,1,4),lean=fs::bSeries(.6,1,4,{},false);
    check(full.thrust==lean.thrust&&full.torque==lean.torque&&lean.curve.empty()&&!full.curve.empty());
    std::cout<<"analytic assertions: "<<n<<'\n';
}
