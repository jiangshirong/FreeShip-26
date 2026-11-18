// SPDX-License-Identifier: GPL-2.0-or-later
// FAST 1995 70-term regression; author-approved Peacock/Smith/Pal correction,
// Australian Naval Architect Nov 1999 p36 (including Cp in lambda's second term).
// Numerical coefficient facts: MNVDET independent implementation table.
#include "fung.h"
#include <cmath>
#include <stdexcept>
namespace fs {
namespace {
struct Term {double b;unsigned char p[9];};
constexpr Term Terms[]={
 {-.0768852,{0,0,0,0,0,0,0,0,0}}, {-5.38179,{1,0,0,0,0,0,0,0,0}},
 {-7.3172,{0,1,0,0,0,0,0,0,0}}, {4.02121,{0,0,1,0,0,0,0,0,0}},
 {-3.01541,{0,0,0,1,0,0,0,0,0}}, {-3.30347,{0,0,0,0,0,1,0,0,0}},
 {-.356016,{0,0,0,0,0,0,1,0,0}}, {-.78642,{0,0,0,0,0,0,0,1,0}},
 {.521915,{2,0,0,0,0,0,0,0,0}}, {3.67289,{1,1,0,0,0,0,0,0,0}},
 {-.526597,{1,0,1,0,0,0,0,0,0}}, {3.66578,{1,0,0,1,0,0,0,0,0}},
 {2.98649,{1,0,0,0,1,0,0,0,0}}, {2.98314,{1,0,0,0,0,1,0,0,0}},
 {.322491,{1,0,0,0,0,0,1,0,0}}, {1.30663,{1,0,0,0,0,0,0,0,1}},
 {2.19508,{0,2,0,0,0,0,0,0,0}}, {6.39624,{0,1,0,1,0,0,0,0,0}},
 {3.35822,{0,1,0,0,1,0,0,0,0}}, {.303679,{0,1,0,0,0,0,1,0,0}},
 {-1.4792,{0,1,0,0,0,0,0,0,1}}, {-.937577,{0,0,2,0,0,0,0,0,0}},
 {.865737,{0,0,1,1,0,0,0,0,0}}, {-.554399,{0,0,1,0,1,0,0,0,0}},
 {.588584,{0,0,1,0,0,1,0,0,0}}, {-.139506,{0,0,1,0,0,0,1,0,0}},
 {.693635,{0,0,1,0,0,0,0,0,1}}, {-7.49626,{0,0,0,1,0,1,0,0,0}},
 {-.619228,{0,0,0,1,0,0,1,0,0}}, {2.03959,{0,0,0,0,1,1,0,0,0}},
 {.34546,{0,0,0,0,1,0,1,0,0}}, {-5.49323,{0,0,0,0,1,0,0,0,1}},
 {.251484,{0,0,0,0,0,1,1,0,0}}, {-.946722,{0,0,0,0,0,1,0,0,1}},
 {.666034,{0,0,0,0,0,0,0,0,2}}, {-.635371,{2,0,0,1,0,0,0,0,0}},
 {-.25757,{2,0,0,0,0,1,0,0,0}}, {-.0217107,{2,0,0,0,0,0,1,0,0}},
 {.566362,{1,2,0,0,0,0,0,0,0}}, {-.438407,{1,1,1,0,0,0,0,0,0}},
 {-2.17903,{1,1,0,1,0,0,0,0,0}}, {-.11578,{1,1,0,0,0,0,1,0,0}},
 {.179566,{1,0,2,0,0,0,0,0,0}}, {-.429466,{1,0,1,1,0,0,0,0,0}},
 {-.279766,{1,0,1,0,0,1,0,0,0}}, {.0317788,{1,0,1,0,0,0,1,0,0}},
 {-.343553,{1,0,1,0,0,0,0,0,1}}, {-2.27502,{1,0,0,2,0,0,0,0,0}},
 {3.63388,{1,0,0,1,0,1,0,0,0}}, {-.108805,{1,0,0,1,0,0,1,0,0}},
 {-2.5626,{1,0,0,0,1,1,0,0,0}}, {-.118863,{1,0,0,0,0,1,1,0,0}},
 {-.18464,{1,0,0,0,0,0,1,0,1}}, {-.261647,{0,2,1,0,0,0,0,0,0}},
 {-3.81958,{0,2,0,0,1,0,0,0,0}}, {.122566,{0,1,2,0,0,0,0,0,0}},
 {-.280095,{0,1,1,0,0,1,0,0,0}}, {-.0338132,{0,1,1,0,0,0,1,0,0}},
 {.328726,{0,1,1,0,0,0,0,0,1}}, {-5.60445,{0,1,0,1,1,0,0,0,0}},
 {.403458,{0,1,0,0,0,2,0,0,0}}, {.780698,{0,1,0,0,0,1,0,0,1}},
 {.361519,{0,0,2,1,0,0,0,0,0}}, {.0332259,{0,0,2,0,0,0,1,0,0}},
 {-.973847,{0,0,1,2,0,0,0,0,0}}, {-.161135,{0,0,1,1,0,0,1,0,0}},
 {4.18896,{0,0,0,2,1,0,0,0,0}}, {.76225,{0,0,0,2,0,0,1,0,0}},
 {1.59206,{0,0,0,1,0,2,0,0,0}}, {.776198,{0,0,0,1,0,0,1,0,1}}};
static_assert(sizeof(Terms)/sizeof(Term)==70);
void require(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
bool within(double x,double lo,double hi){return std::isfinite(x)&&x>=lo-1e-12&&x<=hi+1e-12;}
}
FungModel fungModel(const FungInput& p){
    for(double v:{p.lwl,p.beam,p.draft,p.volume,p.cp,p.wettedArea,p.entrance,p.transomArea,p.transomBeam,p.density,p.viscosity,p.correlation,p.lcbPercent,p.cwp,p.transomDraftRatio})
        require(std::isfinite(v),"Fung–Leibman 输入必须为有限数值。");
    require(p.lwl>0&&p.beam>0&&p.draft>0&&p.volume>0&&p.wettedArea>0&&p.density>0&&p.viscosity>0,"尺度、排水体积、湿表面积、密度及黏度须为正。");
    require(within(p.cp,.526,.774),"Cp 须在 0.526–0.774 内。");
    const auto checks=fungDomain(p);
    require(checks[0].valid()&&checks[1].valid(),"LWL/B 或 B/T 超出 Fung–Leibman 试验范围。");
    require(within(p.entrance,2.600,31.730),"水线半进流角须在 2.600–31.730° 内。");
    require(checks[2].valid(),"浮心距首端的长度比须在 0.481–0.591 内。");
    require(p.cwp==0||within(p.cwp,.662,.841),"CWP 须在 0.662–0.841 内；0 表示尚未提供。");
    require(p.transomDraftRatio==-1||within(p.transomDraftRatio,0,.770),"尾封板浸深比须在 0–0.770 内；-1 表示尚未提供。");
    require(within(p.correlation,0,.01),"给定附加摩擦系数须在 0–0.01 内。");
    FungModel m{};m.input=p;m.cm=checks[3].value;m.dlr=checks[4].value;m.cws=checks[5].value;
    require(checks[3].valid(),"由排水体积和 Cp 算得的 Cm 须在 0.556–0.994 内，请核对输入。");
    require(checks[4].valid(),"排水量长度比 DLR 超出 16.239–359.180。");
    require(checks[5].valid(),"湿表面积系数 Cws 超出 14.324–23.673；请核对裸船面积。");
    m.areaRatio=checks[6].value;m.beamRatio=checks[7].value;
    require(checks[6].valid()&&checks[7].valid(),"尾封板面积比或宽度比超出试验范围，未作截断。");
    require(!(p.transomArea>0&&p.transomBeam==0),"浸水尾封板面积为正时，水线宽度须为正。");
    m.lambda=.75*p.cp+.034977*.035*p.cp*m.dlr;
    const double x[]={0,0,std::sqrt(.034977*m.dlr),m.areaRatio,p.cp*p.cp,m.beamRatio,p.beam/p.draft,std::log(90-p.entrance),m.cm};
    // Fold only speed-independent terms once; keep the full published polynomial.
    // Long-double accumulation reduces cancellation before storing the 9 doubles.
    long double folded[3][3]{};
    for(const auto& t:Terms){long double value=t.b;for(int j=2;j<9;++j)for(int k=0;k<t.p[j];++k)value*=x[j];folded[t.p[0]][t.p[1]]+=value;}
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)m.polynomial[i][j]=double(folded[i][j]);
    return m;
}
FungPoint fungPoint(const FungModel& m,double knots){
    require(std::isfinite(knots)&&knots>=0,"航速须为非负有限值。");FungPoint r{};r.knots=knots;if(knots==0)return r;
    const auto& p=m.input;r.speed=knots*(1852./3600.);r.fn=r.speed/std::sqrt(9.80665*p.lwl);
    require(within(r.fn,.15,.9),"Fung–Leibman 要求 0.15≤Fn≤0.90，未作外推。");
    r.reynolds=r.speed*p.lwl/p.viscosity;require(std::isfinite(r.reynolds)&&r.reynolds>=1e5,"雷诺数须不小于数值下限 10^5。");
    r.cf=.075/std::pow(std::log10(r.reynolds)-2,2);r.x1=std::pow(r.fn,-.7);
    r.x2=std::cos(m.lambda*std::pow(r.fn,-1.93))*std::exp(-.2/(r.fn*r.fn));
    for(int i=2;i>=0;--i){const auto& c=m.polynomial[i];r.logCr=std::fma(r.logCr,r.x1,std::fma(std::fma(c[2],r.x2,c[1]),r.x2,c[0]));}
    r.cr=std::exp(r.logCr);const double q=.5*p.density*r.speed*r.speed*p.wettedArea/1000;
    r.frictionKn=q*r.cf;r.residuaryKn=q*r.cr;r.allowanceKn=q*p.correlation;r.totalKn=r.frictionKn+r.residuaryKn+r.allowanceKn;r.powerKw=r.totalKn*r.speed;
    require(std::isfinite(r.powerKw)&&r.cr>0,"回归结果超出有效数值范围，未发布结果。");return r;
}
}
