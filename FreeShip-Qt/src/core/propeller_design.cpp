// SPDX-License-Identifier: GPL-2.0-or-later
#include "propeller_design.h"
#include "bseries.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fs {
namespace {
constexpr double Pi=3.1415926535897932384626433832795;
constexpr int PitchIntervals=360;
void require(bool ok,const char* why){if(!ok)throw std::invalid_argument(why);}
void validate(const PropellerDesignInput& in){
    for(double x:{in.speedKnots,in.resistanceKn,in.density,in.wake,in.deduction,in.etaR,in.etaS,in.etaG,
                  in.area,in.pitchLow,in.pitchHigh,in.maxDiameter})require(std::isfinite(x),"选桨输入必须为有限数值。");
    require(in.speedKnots>0&&in.resistanceKn>0&&in.density>0,"航速、阻力、密度必须大于零。");
    require(in.wake>=0&&in.wake<1&&in.deduction>=0&&in.deduction<1,"伴流比和推力减额须在 [0,1) 内。");
    require(in.etaR>0&&in.etaS>0&&in.etaS<=1&&in.etaG>0&&in.etaG<=1,"相对旋转效率须大于零，传动效率须在 (0,1] 内。");
    require(in.propellers>=1&&in.propellers<=3,"螺旋桨数量须为 1–3 的整数。");
    require(in.pitchLow>=.5&&in.pitchHigh<=1.4&&in.pitchLow<=in.pitchHigh,"螺距搜索范围须在 0.5–1.4 内，下限不得大于上限。");
    require(in.maxDiameter>0&&in.maxDiameter<=25,"搜索直径上限须在 (0,25] m 内。");
    bSeries(in.area,in.pitchLow,in.blades,{},false); // Published geometry domain.
}
}
PropellerSearch designPropeller(const PropellerDesignInput& in,PropellerConstraint mode,double fixed,const DesignCheck& check){
    validate(in);require(std::isfinite(fixed)&&fixed>0,"固定直径或转速必须大于零。");
    if(mode==PropellerConstraint::Diameter)require(fixed<=in.maxDiameter,"固定直径超出搜索上限。");
    const double va=in.speedKnots*(1852./3600.)*(1-in.wake);
    const double thrust=in.resistanceKn*1000/(in.propellers*(1-in.deduction));
    const double coefficient=mode==PropellerConstraint::Diameter?thrust/(in.density*fixed*fixed*va*va):
        thrust*std::pow(fixed/60.,2)/(in.density*std::pow(va,4));
    require(std::isfinite(coefficient)&&coefficient>0,"选桨工况比例超出有效数值范围。");
    PropellerSearch result;
    auto evaluate=[&](double pitch){
        pitch=std::clamp(pitch,in.pitchLow,in.pitchHigh); // arithmetic at the final grid endpoint
        if(check&&result.evaluations%32==0)check();++result.evaluations;
        const auto polynomial=bSeries(in.area,pitch,in.blades,{},false);
        const auto roots=polynomial.thrustIdentity(coefficient,mode==PropellerConstraint::Diameter?2:4);
        result.maxRootCount=std::max(result.maxRootCount,int(roots.size()));
        double score=-std::numeric_limits<double>::infinity();
        for(double j:roots){
            if(j<=0)continue;
            const auto value=polynomial.evaluate(j);
            if(!value.efficiency||*value.efficiency<=0||*value.efficiency>1||value.kt<=0||value.kq<=0)continue;
            const double d=mode==PropellerConstraint::Diameter?fixed:va/(fixed/60.*j);
            if(d>in.maxDiameter)continue;
            const double rpm=mode==PropellerConstraint::Rpm?fixed:60*va/(j*d),n=rpm/60.;
            const double t=in.propellers*in.density*n*n*std::pow(d,4)*value.kt/1000.;
            const double q=in.propellers*in.density*n*n*std::pow(d,5)*value.kq/(1000*in.etaR);
            const double delivered=2*Pi*n*q,shaft=delivered/in.etaS,power=shaft/in.etaG;
            const double residual=(1-in.deduction)*t-in.resistanceKn;
            require(std::isfinite(power)&&std::abs(residual)<=1e-8*std::max(1.,in.resistanceKn),"选桨推力平衡残差超限，未发布结果。");
            PropellerDesignPoint point{in.speedKnots,in.resistanceKn,d,rpm,pitch,j,value.kt,value.kq,*value.efficiency,
                t,q,delivered,shaft,power,residual};
            if(!result.best||point.eta0>result.best->eta0)result.best=point;
            score=std::max(score,point.eta0);
        }
        return score;
    };
    if(in.pitchLow==in.pitchHigh)evaluate(in.pitchLow);
    else {
        result.pitchStep=(in.pitchHigh-in.pitchLow)/PitchIntervals;
        std::array<double,PitchIntervals+1> scores;
        for(int i=0;i<=PitchIntervals;++i)scores[i]=evaluate(in.pitchLow+i*result.pitchStep);
        // Refine every sampled local maximum and every feasible-domain edge.
        // Do not assume the published regression has a single efficiency peak.
        auto refine=[&](double left,double right){
            constexpr double ratio=.6180339887498948482;
            double a=right-ratio*(right-left),b=left+ratio*(right-left),fa=evaluate(a),fb=evaluate(b);
            for(int k=0;k<48;++k){
                if(fa<fb){left=a;a=b;fa=fb;b=left+ratio*(right-left);fb=evaluate(b);}
                else{right=b;b=a;fb=fa;a=right-ratio*(right-left);fa=evaluate(a);}
            }
        };
        for(int i=0;i<=PitchIntervals;++i)if(std::isfinite(scores[i])&&
            (i==0||scores[i]>=scores[i-1])&&(i==PitchIntervals||scores[i]>=scores[i+1]))
            refine(in.pitchLow+std::max(0,i-1)*result.pitchStep,in.pitchLow+std::min(PitchIntervals,i+1)*result.pitchStep);
        for(int i=1;i<=PitchIntervals;++i)if(std::isfinite(scores[i])!=std::isfinite(scores[i-1])){
            double feasible=in.pitchLow+(std::isfinite(scores[i])?i:i-1)*result.pitchStep;
            double infeasible=in.pitchLow+(std::isfinite(scores[i])?i-1:i)*result.pitchStep;
            // Golden-section optimization alone can discard a very thin feasible
            // strip when both trial points are invalid. Retain the known feasible
            // endpoint and bisect its boundary before any objective refinement.
            for(int k=0;k<48;++k){double middle=(feasible+infeasible)*.5;
                if(std::isfinite(evaluate(middle)))feasible=middle;else infeasible=middle;}
        }
    }
    if(result.best){
        auto& point=*result.best;
        point.pitchBoundary=std::abs(point.pitch-in.pitchLow)<1e-7||std::abs(point.pitch-in.pitchHigh)<1e-7;
        point.advanceBoundary=std::abs(point.j-1.5)<1e-7;
        point.diameterBoundary=std::abs(point.diameter-in.maxDiameter)<1e-7*std::max(1.,in.maxDiameter);
    }
    if(check)check();return result;
}
PropellerMatch matchPropeller(const PropellerDesignInput& in,double rpm,double power,
    const std::array<double,5>& speeds,const std::array<double,5>& resistance,const DesignCheck& check){
    require(std::isfinite(power)&&power>0,"主机总功率须大于零。");
    for(size_t i=0;i<5;++i){
        require(std::isfinite(speeds[i])&&std::isfinite(resistance[i])&&speeds[i]>0&&resistance[i]>0,"阻力表须为正的有限数值。");
        if(i)require(speeds[i]>speeds[i-1]&&resistance[i]>=resistance[i-1],"船速须递增，阻力不得递减；本初选方法不处理非单调阻力曲线。");
    }
    require(in.speedKnots>=speeds.front()&&in.speedKnots<=speeds.back(),"目标航速超出阻力表范围；不作外推。");
    auto at=[&](double speed){
        if(check)check();auto request=in;request.speedKnots=speed;
        size_t k=1;while(k<4&&speed>speeds[k])++k;
        request.resistanceKn=resistance[k-1]+(speed-speeds[k-1])/(speeds[k]-speeds[k-1])*(resistance[k]-resistance[k-1]);
        auto search=designPropeller(request,PropellerConstraint::Rpm,rpm,check);
        require(search.best.has_value(),"给定转速、直径上限和螺距范围内未找到可用选桨候选；不能外推主机匹配。");
        return *search.best;
    };
    PropellerMatch result;for(size_t i=0;i<5;++i)result.table[i]=at(speeds[i]);result.target=at(in.speedKnots);
    // Audit monotonicity on each resistance-table segment before accepting one
    // power bracket. A nonmonotone numerical design envelope is not a unique match.
    for(int segment=0;segment<4;++segment)for(int i=0;i<16;++i){
        auto point=at(speeds[segment]+(speeds[segment+1]-speeds[segment])*i/16.);
        if(!result.search.empty())require(point.enginePowerKw>=result.search.back().enginePowerKw-1e-8*power,
            "选桨功率包络非单调，未把它当作唯一主机匹配结果。");
        result.search.push_back(point);
    }
    require(result.table.back().enginePowerKw>=result.search.back().enginePowerKw-1e-8*power,"选桨功率包络非单调。");
    result.search.push_back(result.table.back());
    require(power>=result.search.front().enginePowerKw,"主机功率低于阻力表最低航速的需求；请扩展低速阻力数据，未作外推。");
    if(power>=result.search.back().enginePowerKw){result.matched=result.search.back();result.tableLimited=true;}
    else {
        size_t k=1;while(result.search[k].enginePowerKw<power)++k;
        double low=result.search[k-1].speedKnots,high=result.search[k].speedKnots;
        for(int i=0;i<48;++i){auto middle=at((low+high)*.5);result.matched=middle;
            if(std::abs(middle.enginePowerKw-power)<=1e-10*std::max(1.,power))break;
            if(middle.enginePowerKw<power)low=middle.speedKnots;else high=middle.speedKnots;}
        require(std::abs(result.matched.enginePowerKw-power)<=1e-8*std::max(1.,power),"主机功率残差超限，未发布匹配结果。");
    }
    result.powerResidualKw=result.matched.enginePowerKw-power;return result;
}
}
