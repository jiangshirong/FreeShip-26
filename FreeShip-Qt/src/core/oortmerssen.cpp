// SPDX-License-Identifier: GPL-2.0-or-later
// Equations: van Oortmerssen (1971), Table II / Appendix II; corrections:
// Helmore (2008), section 2. Numerical facts transcribed from the primary paper.
#include "oortmerssen.h"
#include <cmath>
#include <stdexcept>
namespace fs {
namespace {
constexpr double Gravity=9.80665;
// Rows are j=0..11, columns i=1..4. d[5][2] is POSITIVE per Helmore's erratum.
constexpr double D[12][4]={
 {79.32134,6714.88397,-908.44371,3012.14549},
 {-.09287,19.83000,2.52704,2.71437},
 {-.00209,2.66997,-.35794,.25521},
 {-246.45896,-19662.02400,755.18660,-9198.80840},
 {187.13664,14099.90400,-48.93952,6886.60416},
 {-1.42893,137.33613,9.86873,-159.92694},
 {.11898,-13.36938,-.77652,16.23621},
 {.15727,-4.49852,3.79020,-.82014},
 {-.00064,.02100,-.01879,.00225},
 {-2.52862,216.44923,-9.24399,236.37970},
 {.50619,-35.07602,1.28571,-44.17820},
 {1.62851,-128.72535,250.64910,207.25580}};
void require(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
}
OortmerssenModel oortmerssenModel(const OortmerssenInput& p){
    for(double v:{p.lwl,p.lpp,p.beam,p.draft,p.volume,p.cpLwl,p.lcbLwlPercent,p.entranceDegrees,
                  p.wettedArea,p.density,p.viscosity,p.correlation})require(std::isfinite(v),"阻力输入必须是有限数值。");
    require(p.lwl>0&&p.lpp>0&&p.lpp<=p.lwl&&p.beam>0&&p.draft>0&&p.volume>0&&p.cpLwl>0,
        "请输入正的船体尺度、排水体积和棱形系数；垂线间长不得大于水线长。");
    require(p.density>0&&p.viscosity>0&&p.wettedArea>=0&&p.correlation>=0&&p.correlation<=.01,
        "密度与黏度须为正；湿表面积非负；附加摩擦系数须在 0–0.01 内。");
    OortmerssenModel r{};r.input=p;r.length=oortmerssenLength(p);
    const auto checks=oortmerssenDomain(p);
    const char* messages[]={"排水长度 LD/船宽须在 3.0–6.2 内，未作外推。","船宽/吃水须在 1.9–4.0 内，未作外推。",
        "换算到排水长度的 Cp 须在 0.50–0.725 内，未作外推。","由排水体积和 Cp 推得的 Cm 须在 0.73–0.97 内，请核对尺度与系数。",
        "换算到排水长度的 LCB 须在 -8.0%–2.8% 内，未作外推。","水线半进流角须在 10–46 度内，未作外推。"};
    for(std::size_t i=0;i<checks.size();++i)require(checks[i].valid(),messages[i]);
    const double lb=checks[0].value,bt=checks[1].value,cwl=p.entranceDegrees*lb;
    r.cp=checks[2].value;r.cm=checks[3].value;r.lcb=checks[4].value;
    r.wettedArea=p.wettedArea>0?p.wettedArea:3.223*std::pow(p.volume,2./3.)+.5402*r.length*std::cbrt(p.volume);
    const double x[]={1,r.lcb,r.lcb*r.lcb,r.cp,r.cp*r.cp,lb,lb*lb,cwl,cwl*cwl,bt,bt*bt,r.cm};
    for(int i=0;i<4;++i){double sum=0,correction=0;for(int j=0;j<12;++j){double y=D[j][i]*x[j]-correction;
        double next=sum+y;correction=(next-sum)-y;sum=next;}r.coefficients[i]=sum/1000.;}
    r.m=.14347*std::pow(r.cp,-2.1976);return r;
}
OortmerssenPoint oortmerssenPoint(const OortmerssenModel& model,double knots){
    require(std::isfinite(knots)&&knots>=0,"航速须为非负有限值。");
    OortmerssenPoint r{};r.knots=knots;if(knots==0)return r;
    const auto& p=model.input;r.speed=knots*(1852./3600.);r.fn=r.speed/std::sqrt(Gravity*model.length);
    require(r.fn<=.5+1e-12,"弗劳德数超过 0.50，未外推 van Oortmerssen 方法。");
    r.reynolds=r.speed*model.length/p.viscosity;
    require(std::isfinite(r.reynolds)&&r.reynolds>=1e5,"雷诺数低于本实现的数值下限 10^5，不使用湍流摩擦线。");
    r.cf=.075/std::pow(std::log10(r.reynolds)-2,2);
    const double q=.5*p.density*r.speed*r.speed*model.wettedArea/1000;
    r.frictionKn=q*r.cf;r.allowanceKn=q*p.correlation;
    const double inverse=1/(r.fn*r.fn),e=std::exp(-model.m*inverse);
    const auto& c=model.coefficients;
    const double rw=c[0]*std::exp(-model.m*inverse/9)+e*(c[1]+c[2]*std::sin(inverse)+c[3]*std::cos(inverse));
    r.residuaryKn=rw*p.density*p.volume*Gravity/1000;
    require(r.residuaryKn>=0,"此参数组合产生负剩余阻力，未截为零或发布结果；请更换适用方法。");
    r.totalKn=r.frictionKn+r.allowanceKn+r.residuaryKn;r.powerKw=r.totalKn*r.speed;
    require(std::isfinite(r.powerKw)&&r.totalKn>=0,"阻力或功率超出有效数值范围。");return r;
}
}
