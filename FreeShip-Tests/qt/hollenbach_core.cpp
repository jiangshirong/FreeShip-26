// Core-only domains not all reachable through the positive-speed UI.
#include "core/hollenbach.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
int main(){
    fs::HollenbachInput p{fs::HollenbachCondition::SingleDesign,120,123,126,20,6.5,0,10608,4.5,2800,1025,1.189e-6,0,1,0,0,0,true};
    const auto model=fs::hollenbachModel(p);const auto zero=fs::hollenbachPoint(model,0);
    if(zero.totalKn!=0||zero.powerKw!=0||zero.residuaryKn!=0)return 1;
    auto knots=[&](double fn){return fn*std::sqrt(9.80665*model.length)*3600/1852;};
    for(double f:{model.fnMin,model.fnMax,model.fnCritical})if(!std::isfinite(fs::hollenbachPoint(model,knots(f)).totalKn))return 2;
    int rejected=0;
    for(double speed:{-1.,std::numeric_limits<double>::quiet_NaN(),1e-12,knots(model.fnMax+1e-7),knots(model.fnMin-1e-7)})
        try{fs::hollenbachPoint(model,speed);}catch(const std::invalid_argument&){++rejected;}
    for(int i=0;i<7;++i){auto bad=p;
        if(i==0)bad.density=-1;if(i==1)bad.lpp=0;if(i==2)bad.correlation=std::numeric_limits<double>::infinity();
        if(i==3)bad.rudders=0;if(i==4)bad.trim=15;if(i==5)bad.condition=fs::HollenbachCondition(3);if(i==6)bad.viscosity=1;
        try{fs::hollenbachPoint(fs::hollenbachModel(bad),16);}catch(const std::invalid_argument&){++rejected;}}
    if(rejected!=12)return 3;
    for(double ratio:{1.099999,1.1,1.100001}){auto input=p;input.lwl=127.2;input.los=120*ratio;auto m=fs::hollenbachModel(input);
        const double wanted=ratio<1.1?120+2*(input.los-120)/3:120*1.0667;
        if(std::abs(m.length-wanted)>1e-10)return 4;}
    std::cout<<"zero, finite inputs, all three length branches, Fn boundaries, invalid geometry and Re verified\n";return 0;
}
