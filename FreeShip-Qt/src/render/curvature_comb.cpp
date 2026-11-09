// Ported from FreeSpline.Curvature/Draw and Normalize in current Plus.
// GPL-2.0-or-later. The finite-difference derivative and Single rounding are
// intentional; this display calculation never modifies the source spline.
#include "curvature_comb.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace fs::presentation {
CurvatureSample curvature(const native_geometry::Spline& spline,float t){
    auto base=spline.value(t),v=spline.firstDerivative(t),a=spline.secondDerivative(t);
    native_geometry::Vec3 cross{a.y*v.z-a.z*v.y,-(a.x*v.z-a.z*v.x),a.x*v.y-a.y*v.x};
    float length=std::sqrt(cross.x*cross.x+cross.y*cross.y+cross.z*cross.z);
    if(length==0)length=.00001f;
    float va=v.x*a.x+v.y*a.y+v.z*a.z,vv=v.x*v.x+v.y*v.y+v.z*v.z;
    float denominator=float(std::pow(double(vv),1.5));
    float c=denominator>0?length/denominator:0;
    native_geometry::Vec3 normal{vv*a.x-va*v.x,vv*a.y-va*v.y,vv*a.z-va*v.z};
    float n=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
    if(std::isnan(n))normal={};
    else {n=std::max(n,.000001f);normal={normal.x/n,normal.y/n,normal.z/n};}
    if(!std::isfinite(c)||!std::isfinite(normal.x)||!std::isfinite(normal.y)||!std::isfinite(normal.z))throw std::runtime_error("Non-finite curvature");
    return {base,normal,c};
}
QVector<CombPoint> curvatureComb(const SectionSpline& controls,int precision){
    std::vector<native_geometry::Vec3> points;std::vector<bool> knuckles;
    for(auto p:controls.points)points.push_back({float(p.x),float(p.y),float(p.z)});
    for(bool flag:controls.knuckles)knuckles.push_back(flag);
    native_geometry::Spline spline(std::move(points),std::move(knuckles));
    const auto& parameters=spline.parameters();const int fragments=384<<std::clamp(precision,0,3);
    QVector<CombPoint> result;result.reserve(fragments+controls.points.size());size_t previous=0;
    for(int i=0;i<fragments;++i){
        const double t=double(i)/(fragments-1);auto sample=curvature(spline,float(t));
        size_t lo=0,hi=parameters.size()-1;while(hi-lo>1){auto mid=(lo+hi)/2;if(parameters[mid]<t)lo=mid;else hi=mid;}
        if(previous!=lo&&spline.knuckles()[lo]){previous=lo;result.append({controls.points[int(lo)],{},0,false});}
        result.append({{sample.base.x,sample.base.y,sample.base.z},sample.normal,sample.value,false});
    }
    for(int i=0;i<result.size();++i)result[i].rib=i%4==0||i==1||i==fragments;
    return result;
}
Vec3 combTip(const CombPoint& p,float scale){
    float factor=p.value*2*scale;
    return {float(float(p.base.x)-factor*p.normal.x),float(float(p.base.y)-factor*p.normal.y),float(float(p.base.z)-factor*p.normal.z)};
}
}
