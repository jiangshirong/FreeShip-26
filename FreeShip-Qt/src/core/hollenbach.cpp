// SPDX-License-Identifier: GPL-2.0-or-later
// Hollenbach, ICCAS 1999, equations 1-12 / Tables 2-3 (numerical facts).
// Count convention (1+N)^a: Steelandt 2020, equation 2.7.
// See docs/migration/NATIVE-HOLLENBACH.md for the printed 1999 ambiguity.
#include "hollenbach.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace fs {
namespace {
struct Coefficients {double a[10],b[9],d[3],e[2],f[3],g[3],maximum;};
constexpr Coefficients Mean[]={
 {{-.3382,.8086,-6.0258,-3.5632,9.4405,.0146,0,0,0,0},
  {-.57424,13.3893,90.596,4.6614,-39.721,-351.483,-1.14215,-12.3296,459.254},
  {.854,-1.228,.497},{2.1701,-.1602},{.17,.2,.6},{.642,-.635,.15},1.204},
 {{-.7139,.2558,-1.1606,.4534,11.222,.4524,0,0,0,0},
  {-1.50162,12.9678,-36.7985,5.55536,-45.8815,121.82,-4.33571,36.0782,-85.3741},
  {.032,.803,-.739},{1.9994,-.1446},{.15,.1,.5},{.42,-.2,0},1.194},
 {{-.2748,.5747,-6.761,-4.3834,8.8158,-.1418,-.1258,.0481,.1699,.0728},
  {-5.3475,55.6532,-114.905,19.2714,-192.388,388.333,-14.3571,142.738,-254.762},
  {.897,-1.457,.767},{1.8319,-.1237},{.16,.24,.6},{.83,-.66,0},1.206}};
constexpr double MinimumB[2][9]={
 {-.91424,13.3893,90.596,4.6614,-39.721,-351.483,-1.14215,-12.3296,459.254},
 {3.27279,-44.1138,171.692,-11.5012,166.559,-644.456,12.4626,-179.505,680.921}};
constexpr double Area[4][12]={
 {-.6837,.2771,.6542,.6422,.0075,.0275,-.0045,-.4798,.0376,0,0,0},
 {-.8037,.2726,.7133,.6699,.0243,.0265,-.0061,.2349,.0131,0,0,0},
 {-.4319,.1685,.5637,.5891,.0033,.0134,-.0005,-2.7932,.0072,.0131,-.0030,.0061},
 {-.0887,0,.5192,.5839,-.0130,.0050,-.0007,-.9486,.0506,.0076,-.0036,.0049}};
// Lpp, Lpp/volume^(1/3), CB, Lpp/B, B/T, LOS/LWL, LWL/Lpp, Dp/TA.
constexpr double Bounds[3][8][2]={
 {{42,205},{4.49,6.01},{.60,.83},{4.71,7.11},{1.99,4},{1,1.05},{1,1.06},{.43,.84}},
 {{50.2,224.8},{5.45,7.05},{.56,.79},{4.95,6.62},{2.97,6.12},{1,1.05},{.95,1},{.66,1.05}},
 {{30.5,206.8},{4.41,7.27},{.51,.78},{3.96,7.13},{2.31,6.11},{1,1.05},{1,1.07},{.50,.86}}};
constexpr const char* RangeErrors[]={"LPP 超出该船况的 Hollenbach 试验范围。","LPP/排水体积立方根超出该船况的 Hollenbach 试验范围。",
 "由 LPP 计算的方形系数 CB 超出该船况的 Hollenbach 试验范围。","LPP/B 超出该船况的 Hollenbach 试验范围。",
 "B/T 超出该船况的 Hollenbach 试验范围。","LOS/LWL 须在 1.00–1.05 内。","LWL/LPP 超出该船况的 Hollenbach 试验范围。","Dp/TA 超出该船况的 Hollenbach 试验范围。"};
void require(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
bool between(double x,double lo,double hi){return std::isfinite(x)&&x>=lo-1e-12&&x<=hi+1e-12;}
double polynomial(const double* b,double cb,double fn){
    // Horner form in CB and Fn, with fused multiply-add where available.
    double result=0;for(int i=2;i>=0;--i)result=std::fma(result,cb,std::fma(std::fma(b[i*3+2],fn,b[i*3+1]),fn,b[i*3]));return result;
}
double quadratic(const double* c,double x){return std::fma(std::fma(c[2],x,c[1]),x,c[0]);}
double blockCoefficient(const HollenbachInput& p){return p.volume/(p.lpp*p.beam*p.draft);}
double referenceLength(const HollenbachInput& p){
    // Preserve the published literal 1.0667.
    return p.los<p.lpp?p.los:p.los<1.1*p.lpp?p.lpp+2*(p.los-p.lpp)/3:1.0667*p.lpp;
}
std::pair<double,double> froudeRange(int mode,double cb){
    const auto& c=Mean[mode];
    return {std::min(c.f[0],c.f[0]+c.f[1]*(c.f[2]-cb)),quadratic(c.g,cb)};
}
}
bool HollenbachGeometryCheck::valid() const{return between(value,minimum,maximum);}
std::array<HollenbachGeometryCheck,8> hollenbachGeometryChecks(const HollenbachInput& p){
    const int mode=int(p.condition);
    require(mode>=0&&mode<=2,"未知 Hollenbach 船况。");
    const double ta=p.draft-p.trim/2;
    const double ratios[]={p.lpp,p.lpp/std::cbrt(p.volume),blockCoefficient(p),p.lpp/p.beam,
        p.beam/p.draft,p.los/p.lwl,p.lwl/p.lpp,p.diameter/ta};
    std::array<HollenbachGeometryCheck,8> checks;
    for(int i=0;i<int(checks.size());++i)
        checks[i]={HollenbachGeometryLimit(i),ratios[i],Bounds[mode][i][0],Bounds[mode][i][1]};
    return checks;
}
bool hollenbachEndDraftsValid(const HollenbachInput& p){
    return p.draft-p.trim/2>0&&p.draft+p.trim/2>0&&1-p.trim/p.lpp>0;
}
std::pair<double,double> hollenbachSpeedRange(const HollenbachInput& p){
    const int mode=int(p.condition);
    require(mode>=0&&mode<=2,"未知 Hollenbach 船况。");
    const auto [minimum,maximum]=froudeRange(mode,blockCoefficient(p));
    const double scale=std::sqrt(9.80665*referenceLength(p))*3600/1852;
    return {(minimum-1e-12)*scale,(maximum+1e-12)*scale};
}
HollenbachModel hollenbachModel(const HollenbachInput& p){
    const int mode=int(p.condition);require(mode>=0&&mode<=2,"未知 Hollenbach 船况。");
    for(double x:{p.lpp,p.lwl,p.los,p.beam,p.draft,p.trim,p.volume,p.diameter,p.wettedArea,p.density,p.viscosity,p.correlation})
        require(std::isfinite(x),"Hollenbach 输入必须是有限数值。");
    require(p.lpp>0&&p.lwl>0&&p.los>0&&p.beam>0&&p.draft>0&&p.volume>0&&p.diameter>0,"请输入正的尺度、排水体积和桨直径；LPP 须单独填写。");
    require(p.density>0&&p.viscosity>0&&p.wettedArea>=0&&between(p.correlation,0,.01),"密度、黏度须为正；湿表面积非负；附加摩擦系数须在 0–0.01 内。");
    require(p.rudders>=1&&p.rudders<=2&&p.brackets>=0&&p.brackets<=2&&p.bossings>=0&&p.bossings<=2&&p.thrusters>=0&&p.thrusters<=4,"附体数量超出 Hollenbach 范围。");
    HollenbachModel r{};r.input=p;r.ta=p.draft-p.trim/2;r.tf=p.draft+p.trim/2;
    require(hollenbachEndDraftsValid(p),"首尾吃水及纵倾修正项须为正。");
    r.cb=blockCoefficient(p);
    for(const auto& check:hollenbachGeometryChecks(p))require(check.valid(),RangeErrors[int(check.limit)]);
    r.length=referenceLength(p);
    const auto& c=Mean[mode];r.fnCritical=quadratic(c.d,r.cb);
    const auto [minimum,maximum]=froudeRange(mode,r.cb);r.fnMin=minimum;r.fnMax=maximum;
    r.lengthFactor=c.e[0]*std::pow(p.lpp,c.e[1]);
    const double bases[]={p.draft/p.beam,p.beam/p.lpp,p.los/p.lwl,p.lwl/p.lpp,1-p.trim/p.lpp,p.diameter/r.ta,
        1.+p.rudders,1.+p.brackets,1.+p.bossings,1.+p.thrusters};
    r.geometryFactor=r.minimumGeometryFactor=1;
    for(int i=0;i<10;++i){r.geometryFactor*=std::pow(bases[i],c.a[i]);if(i<4)r.minimumGeometryFactor*=std::pow(bases[i],c.a[i]);}
    r.wettedArea=p.wettedArea;
    if(p.wettedArea==0){
        require(mode!=2||((p.brackets==2&&p.bossings==0)||(p.brackets==0&&p.bossings==2)),"双桨湿表面积估计要求两副轴架或两个轴包套；其他配置请输入含附体的实际面积。");
        const auto& a=Area[mode<2?mode:p.bulb?2:3];
        const double terms[]={1,p.los/p.lwl,p.lwl/p.lpp,r.cb,p.lpp/p.beam,p.beam/p.draft,p.lpp/p.draft,-p.trim/p.lpp,p.diameter/p.draft,double(p.rudders),double(p.brackets),double(p.bossings)};
        double k=0;for(int i=0;i<12;++i)k=std::fma(a[i],terms[i],k);r.wettedArea=k*p.lpp*(p.beam+2*p.draft);
    }
    require(std::isfinite(r.wettedArea)&&r.wettedArea>0,"湿表面积估计非正，请输入实测面积。");
    r.minimumGeometryValid=mode!=1&&between(r.cb,.525,.825)&&between(p.lpp/p.beam,5,7.5)&&between(p.beam/p.draft,2.5,4.5);
    r.minimumFnMin=mode==0?std::min(.17,.17+.2*(.6-r.cb)):.15;
    r.minimumFnMax=mode==0?.614-.717*r.cb+.261*r.cb*r.cb:.952-1.406*r.cb+.643*r.cb*r.cb;
    require(std::isfinite(r.geometryFactor)&&r.fnCritical>0&&r.fnMax>r.fnMin,"Hollenbach 修正系数无效。");return r;
}
HollenbachPoint hollenbachPoint(const HollenbachModel& m,double knots){
    require(std::isfinite(knots)&&knots>=0,"航速须为非负有限值。");
    HollenbachPoint r{};r.knots=knots;if(knots==0)return r;
    const auto& p=m.input;const auto& c=Mean[int(p.condition)];r.speed=knots*(1852./3600.);r.fn=r.speed/std::sqrt(9.80665*m.length);
    require(between(r.fn,m.fnMin,m.fnMax),"航速超出该船况及 CB 对应的 Hollenbach Fn 适用范围，未作外推。");
    r.reynolds=r.speed*p.lpp/p.viscosity;require(std::isfinite(r.reynolds)&&r.reynolds>=1e5,"雷诺数须不小于数值下限 10^5。");
    r.cf=.075/std::pow(std::log10(r.reynolds)-2,2);
    const double ratio=r.fn/m.fnCritical,exponent=p.condition==HollenbachCondition::SingleBallast?10*m.cb*(ratio-1):ratio;
    r.fnFactor=std::max(1.,std::pow(ratio,exponent));r.crStandard=polynomial(c.b,m.cb,r.fn);
    r.cr=r.crStandard*r.fnFactor*m.lengthFactor*m.geometryFactor;
    require(r.cr>=0&&std::isfinite(r.cr),"此参数组合产生负或非有限剩余阻力，未截为零或发布结果。");
    const double q=.5*p.density*r.speed*r.speed/1000;
    r.frictionKn=q*m.wettedArea*r.cf;r.allowanceKn=q*m.wettedArea*p.correlation;r.residuaryKn=q*(p.beam*p.draft/10)*r.cr;
    const double mean=r.frictionKn+r.residuaryKn;
    r.totalKn=mean+r.allowanceKn;r.powerKw=r.totalKn*r.speed;
    // h1 applies to the base calm-water mean; an explicitly supplied allowance
    // is added once, and the separate user sea margin is applied by the workflow.
    r.maximumKn=c.maximum*mean+r.allowanceKn;
    r.minimumValid=m.minimumGeometryValid&&between(r.fn,m.minimumFnMin,m.minimumFnMax);
    if(r.minimumValid){const double cr=polynomial(MinimumB[p.condition==HollenbachCondition::SingleDesign?0:1],m.cb,r.fn)*m.minimumGeometryFactor;
        r.minimumValid=std::isfinite(cr)&&cr>=0;if(r.minimumValid)r.minimumKn=r.frictionKn+r.allowanceKn+q*p.beam*p.draft/10*cr;}
    require(std::isfinite(r.powerKw)&&std::isfinite(r.maximumKn),"Hollenbach 阻力或功率超出有效数值范围。");return r;
}
}
