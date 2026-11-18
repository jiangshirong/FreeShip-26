// SPDX-License-Identifier: GPL-2.0-or-later
#include "propulsion_passport.h"
#include <cmath>
#include <stdexcept>

namespace fs {
namespace {
constexpr double Pi=3.1415926535897932384626433832795;
void require(bool condition,const char* message){if(!condition)throw std::invalid_argument(message);}
PassportPoint evaluate(const PassportInput& in,const BSeriesResult& model,double j,double rpm){
    const auto v=model.evaluate(j);const double n=rpm/60.,d=in.diameter;
    // N equal propellers; forces in kN, torques in kN m, powers in kW.
    const double thrust=in.propellers*in.density*n*n*std::pow(d,4)*v.kt/1000.;
    // Thrust identity: etaR=Q_open/Q_behind, not a thrust multiplier.
    const double torque=in.propellers*in.density*n*n*std::pow(d,5)*v.kq/(1000*in.relativeRotativeEfficiency);
    const double delivered=2*Pi*n*torque,va=j*n*d;
    return {j,rpm,va,va/(1-in.wake)*(3600./1852.),v.kt,v.kq,thrust,torque,delivered,
        delivered/(in.shaftEfficiency*in.gearEfficiency),(1-in.deduction)*thrust,v.efficiency};
}
}
PropulsionPassport propulsionPassport(const PassportInput& in){
    for(double x:{in.diameter,in.area,in.pitch,in.referenceJ,in.rpm,in.wake,in.deduction,in.relativeRotativeEfficiency,
                  in.legacyTorqueFactor,in.shaftEfficiency,in.gearEfficiency,in.density,in.enginePower})
        require(std::isfinite(x),"推进特性图参数须为有限数字。");
    require(in.diameter>0&&in.rpm>0&&in.density>0&&in.enginePower>0,"直径、转速、密度和主机总功率须大于零。");
    require(in.propellers>=1&&in.propellers<=3,"螺旋桨数量须为 1–3 的整数。");
    require(in.wake>=0&&in.wake<1&&in.deduction>=0&&in.deduction<1,"本方法伴流比和推力减额须在 [0,1) 内。");
    require(in.relativeRotativeEfficiency>0&&in.shaftEfficiency>0&&in.shaftEfficiency<=1&&
            in.gearEfficiency>0&&in.gearEfficiency<=1,"相对旋转效率须大于零，传动效率须在 (0,1] 内。");
    require(in.legacyTorqueFactor==1,"内置方法要求 i2 = 1。其他取值请选择 CALCPROP。");
    require(in.referenceJ>0&&in.referenceJ*1.3<=1.5,"本图使用参考 J 的 0.70–1.30 倍；全部工况须落在 J=0–1.5 内。");
    PropulsionPassport result;result.polynomial=bSeries(in.area,in.pitch,in.blades);
    const double js[]={.7,.85,1,1.15,1.3},ns[]={.55,.7,.85,1,1.15};
    for(int g=0;g<5;++g)for(int i=0;i<5;++i)
        result.points[g][i]=evaluate(in,result.polynomial,in.referenceJ*js[i],in.rpm*ns[g]);
    for(int g=0;g<4;++g){
        auto& branch=result.engine[g];branch.rpm=in.rpm*ns[g];branch.targetPower=in.enginePower*ns[g]*ns[g];
        const double n=branch.rpm/60.;
        const double scale=2*Pi*in.propellers*in.density*n*n*n*std::pow(in.diameter,5)/
            (1000*in.relativeRotativeEfficiency*in.shaftEfficiency*in.gearEfficiency);
        for(double j:result.polynomial.torqueIntersections(branch.targetPower/scale,in.referenceJ*.7,in.referenceJ*1.3)){
            auto p=evaluate(in,result.polynomial,j,branch.rpm);
            // A root in the windmilling/braking region is not an engine operating point.
            if(p.effectiveThrust>=0&&p.enginePower>0)branch.intersections.push_back(p);
        }
    }
    return result;
}
}
