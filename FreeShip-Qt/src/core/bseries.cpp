// SPDX-License-Identifier: GPL-2.0-or-later
#include "bseries.h"
#include "bseries_tables.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fs {
namespace {
constexpr double Pi=3.1415926535897932384626433832795;
constexpr double Limit=1.5; // Published ahead-running interpolation envelope.
using Polynomial=std::vector<double>; // Increasing powers.
template<class P> double value(const P& p,double x) {
    double y=0;for(auto i=p.rbegin();i!=p.rend();++i)y=y*x+*i;return y;
}
// Isolate all real roots on a bounded interval by recursively splitting at the
// derivative roots. Each remaining interval is monotone. This also catches
// tangent roots, unlike a fixed sign-change scan or a unimodal maximum search.
std::vector<double> roots(Polynomial p,double low,double high) {
    const auto scale=*std::max_element(p.begin(),p.end(),[](double a,double b){return std::abs(a)<std::abs(b);});
    if(scale==0)return {};
    for(auto& c:p)c/=std::abs(scale);
    while(p.size()>1&&std::abs(p.back())<1e-15)p.pop_back();
    if(p.size()==1)return {};
    if(p.size()==2){double x=-p[0]/p[1];return x>=low&&x<=high?std::vector<double>{x}:std::vector<double>{};}
    Polynomial derivative;for(size_t i=1;i<p.size();++i)derivative.push_back(i*p[i]);
    auto cuts=roots(derivative,low,high);cuts.insert(cuts.begin(),low);cuts.push_back(high);
    std::vector<double> found;
    for(double x:cuts)if(std::abs(value(p,x))<1e-13)found.push_back(x);
    for(size_t i=1;i<cuts.size();++i){
        double a=cuts[i-1],b=cuts[i],fa=value(p,a),fb=value(p,b);
        if(fa==0||fb==0||std::signbit(fa)==std::signbit(fb))continue;
        for(int k=0;k<60;++k){double m=(a+b)*.5,fm=value(p,m);if(fm==0){a=b=m;break;}
            if(std::signbit(fm)==std::signbit(fa)){a=m;fa=fm;}else b=m;}
        found.push_back((a+b)*.5);
    }
    std::sort(found.begin(),found.end());
    found.erase(std::unique(found.begin(),found.end(),[](double a,double b){return std::abs(a-b)<1e-11;}),found.end());
    return found;
}
template<size_t N> std::array<double,4> polynomial(const bseries_data::Term (&terms)[N],double area,double pitch,int blades) {
    std::array<double,7> a{1},p{1},z{1};
    for(size_t i=1;i<a.size();++i){a[i]=a[i-1]*area;p[i]=p[i-1]*pitch;z[i]=z[i-1]*blades;}
    std::array<double,4> result{};
    for(auto term:terms)result[term.s]+=term.c*p[term.t]*a[term.u]*z[term.v];
    return result;
}
}
BSeriesValue BSeriesResult::evaluate(double j) const {
    if(!std::isfinite(j)||j<0||j>Limit)throw std::invalid_argument("B 系列进速系数 J 必须在 0–1.5 范围内；不作外推或自动截断。");
    const double kt=value(thrust,j),kq=value(torque,j);
    BSeriesValue point{j,kt,kq,{}};
    if(kt>=0&&kq>0)point.efficiency=j*kt/(2*Pi*kq);
    return point;
}
std::vector<double> BSeriesResult::torqueIntersections(double kq,double low,double high) const {
    if(!std::isfinite(kq)||!std::isfinite(low)||!std::isfinite(high)||low<0||high>Limit||low>=high)
        throw std::invalid_argument("Invalid B-series torque intersection interval");
    Polynomial p(torque.begin(),torque.end());p[0]-=kq;return roots(p,low,high);
}
bool BSeriesResult::propulsiveInterval(double low,double high) const {
    evaluate(low);evaluate(high);
    if(low>=high)throw std::invalid_argument("Invalid B-series interval");
    for(const auto* p:{&thrust,&torque}){
        auto extrema=roots({(*p)[1],2*(*p)[2],3*(*p)[3]},low,high);
        extrema.push_back(low);extrema.push_back(high);
        for(double j:extrema)if(p==&thrust?value(*p,j)<0:value(*p,j)<=0)return false;
    }
    return true;
}
std::vector<double> BSeriesResult::thrustIdentity(double coefficient,int power) const {
    if(!std::isfinite(coefficient)||coefficient<=0||(power!=2&&power!=4))
        throw std::invalid_argument("Invalid thrust identity constraint");
    Polynomial p(thrust.begin(),thrust.end());p.resize(std::max(size_t(power+1),p.size()),0);p[power]-=coefficient;
    return roots(p,0,Limit);
}
BSeriesResult bSeries(double area,double pitch,int blades,std::optional<double> reynolds,bool sampleCurve) {
    if(!std::isfinite(area)||area<.3||area>1.05||!std::isfinite(pitch)||pitch<.5||pitch>1.4||blades<2||blades>7)
        throw std::invalid_argument("B 系列适用范围：桨盘面比 0.30–1.05，螺距比 0.50–1.40，桨叶数为 2–7 的整数。");
    BSeriesResult result;
    result.thrust=polynomial(bseries_data::KT,area,pitch,blades);
    result.torque=polynomial(bseries_data::KQ,area,pitch,blades);
    result.referenceThrust=result.thrust;result.referenceTorque=result.torque;
    if(reynolds){
        if(!std::isfinite(*reynolds)||*reynolds<2e6||*reynolds>2e9)
            throw std::invalid_argument("B 系列修正雷诺数必须在 2×10^6–2×10^9 范围内。 ");
        // Oosterveld/van Oossanen (1975), Table 6. L uses the common logarithm
        // of the actual Re, NOT Re expressed in millions. Collect powers of J
        // once: all work points, roots, extrema and chart rows use one cubic.
        const double l=std::log10(*reynolds)-.301,l2=l*l,a=area,p=pitch,z=blades;
        const double p2=p*p,p3=p2*p,p6=p3*p3,a2=a*a;
        result.deltaThrust={.000353485,
            -.00478125*a*p+.0000954*l*z*a*p+.0000032049*l*z*z*a*p3,
            -.00333758*a+.000257792*l2*a+.0000643192*l*p6-.0000110636*l2*p6-.0000276305*l2*z*a,0};
        result.deltaTorque={-.000591412+.00696898*p-.0000666654*z*p6+.0160818*a2
            -.000938091*l*p-.00059593*l*p2+.0000782099*l2*p2
            +.0000230171*l*z*p6-.00000184341*l2*z*p6-.00400252*l*a2+.000220915*l2*a2,
            -.00000088528*l2*z*a*p,.0000052199*l*z*a,0};
        // Section 4 explicitly notes nonzero regression residuals at Re=2e6.
        // Preserve the published correction, including that endpoint. Choosing
        // reference mode (nullopt) above leaves the original result unchanged.
        for(size_t i=0;i<4;++i){result.thrust[i]+=result.deltaThrust[i];result.torque[i]+=result.deltaTorque[i];}
    }
    const auto ts=roots(Polynomial(result.thrust.begin(),result.thrust.end()),0,Limit);
    const auto qs=roots(Polynomial(result.torque.begin(),result.torque.end()),0,Limit);
    if(!ts.empty())result.zeroThrust=ts.front();
    if(!qs.empty())result.zeroTorque=qs.front();
    const double end=std::min({Limit,result.zeroThrust.value_or(Limit),result.zeroTorque.value_or(Limit)});
    if(value(result.thrust,0)<=0||value(result.torque,0)<=0||end<=0)
        throw std::invalid_argument("该组合在 J=0 附近没有有效的正推力、正转矩分支。");
    result.unboundedEfficiency=result.zeroTorque&&*result.zeroTorque<=end&&value(result.thrust,end)>1e-10;
    // d(J*KT/KQ)/dJ: ((KT+J*KT')*KQ-J*KT*KQ') / KQ^2.
    Polynomial stationary(7,0);
    for(int i=0;i<4;++i)for(int k=0;k<4;++k)stationary[i+k]+=(i+1-k)*result.thrust[i]*result.torque[k];
    auto candidates=roots(stationary,0,end);candidates.push_back(0);candidates.push_back(end);
    if(!result.unboundedEfficiency)for(double j:candidates){
        const auto point=result.evaluate(j);
        if(point.efficiency&&(!result.maximum||*point.efficiency>*result.maximum->efficiency))result.maximum=point;
    }
    result.maximumAtBoundary=result.maximum&&std::abs(result.maximum->j-end)<1e-10&&end==Limit;
    // Sampling is for display only. Working points, roots and extrema above
    // are evaluated from the full polynomial, never read back from the chart.
    if(sampleCurve){
        for(int i=0;i<=1500;++i){double j=i*.001;if(j>=end)break;result.curve.push_back(result.evaluate(j));}
        if(!result.unboundedEfficiency)result.curve.push_back(result.evaluate(end));
    }
    return result;
}
}
