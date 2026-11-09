#include "auxiliary_geometry.h"
#include "native/geometry.h"
#include <QLineF>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace fs::presentation {
namespace {
Vec3 mix(Vec3 a,Vec3 b,double t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
std::pair<Cubic,Cubic> cut(const Cubic& c,double t){auto a=mix(c[0],c[1],t),b=mix(c[1],c[2],t),d=mix(c[2],c[3],t);auto e=mix(a,b,t),f=mix(b,d,t),g=mix(e,f,t);return {{{c[0],a,e,g}},{{g,f,d,c[3]}}};}
}
Vec3 cubicValue(const Cubic& c,double t){return cut(c,t).first[3];}
QVector<Cubic> splineCubics(const SectionSpline& source){
    QVector<Cubic> result;if(source.points.size()<2)return result;
    std::vector<native_geometry::Vec3> p;std::vector<bool> flags;
    for(auto v:source.points)p.push_back({float(v.x),float(v.y),float(v.z)});for(bool k:source.knuckles)flags.push_back(k);
    native_geometry::Spline spline(p,flags);const auto& t=spline.parameters();const auto& d=spline.secondDerivatives();
    for(size_t i=1;i<p.size();++i){
        float h=t[i]-t[i-1];if(std::abs(h)<1e-6)continue;
        auto a=source.points[int(i-1)],b=source.points[int(i)];auto c1=mix(a,b,1./3),c2=mix(a,b,2./3);
        if(!(flags[i-1]&&flags[i])){
            const double f=double(h*h/6.f)/3;
            c1.x-=f*(2.*d[i-1].x+d[i].x);c1.y-=f*(2.*d[i-1].y+d[i].y);c1.z-=f*(2.*d[i-1].z+d[i].z);
            c2.x-=f*(d[i-1].x+2.*d[i].x);c2.y-=f*(d[i-1].y+2.*d[i].y);c2.z-=f*(d[i-1].z+2.*d[i].z);
        }
        result.append({a,c1,c2,b});
    }
    return result;
}
QVector<Cubic> splitBodyCubics(const QVector<Cubic>& input,double split){
    QVector<Cubic> result;
    for(const auto& c:input){
        // Split into monotone x intervals first, so tangencies and up to three
        // crossings are handled without a fixed sampling grid.
        double a=-c[0].x+3*c[1].x-3*c[2].x+c[3].x,b=2*(c[0].x-2*c[1].x+c[2].x),d=c[1].x-c[0].x;
        QVector<double> extrema{0,1};
        auto add=[&](double t){if(t>0&&t<1)extrema.append(t);};
        if(std::abs(a)<1e-14){if(b!=0)add(-d/b);}
        else if(auto discriminant=b*b-4*a*d;discriminant>=0){auto root=std::sqrt(discriminant);add((-b-root)/(2*a));add((-b+root)/(2*a));}
        std::sort(extrema.begin(),extrema.end());QVector<double> roots{0,1};
        for(int i=1;i<extrema.size();++i){
            auto lo=extrema[i-1],hi=extrema[i];double v=cubicValue(c,lo).x-split,w=cubicValue(c,hi).x-split;
            if(v==0)roots.append(lo);if(w==0)roots.append(hi);
            if((v<0)==(w<0)||v==0||w==0)continue;
            for(int k=0;k<52;++k){double mid=(lo+hi)*.5;if((cubicValue(c,mid).x<split)==(v<0))lo=mid;else hi=mid;}
            roots.append((lo+hi)*.5);
        }
        std::sort(roots.begin(),roots.end());roots.erase(std::unique(roots.begin(),roots.end(),[](double a,double b){return std::abs(a-b)<1e-12;}),roots.end());
        for(int i=1;i<roots.size();++i){double lo=roots[i-1],hi=roots[i];if(hi-lo<1e-12)continue;
            auto piece=cut(c,hi).first;if(lo>0)piece=cut(piece,lo/hi).second;
            if(cubicValue(c,(lo+hi)*.5).x<split)for(auto& p:piece)p.y=-p.y;
            result.append(piece);
        }
    }
    return result;
}
QPainterPath projectCubics(const QVector<Cubic>& cubics,const std::function<QPointF(Vec3)>& project,bool perspective){
    QPainterPath path;int segments=0;
    std::function<void(const Cubic&,int)> append=[&](const Cubic& c,int depth){
        auto a=project(c[0]),b=project(c[3]);auto ab=b-a;double length=QPointF::dotProduct(ab,ab);
        auto distance=[&](QPointF p){double t=length>0?std::clamp(QPointF::dotProduct(p-a,ab)/length,0.,1.):0.;return QLineF(p,a+t*ab).length();};
        if(depth<24&&(distance(project(c[1]))>.2||distance(project(c[2]))>.2)){
            auto halves=cut(c,.5);append(halves.first,depth+1);append(halves.second,depth+1);
        }else{if(++segments>1000000)throw std::runtime_error("Auxiliary curve presentation limit");path.lineTo(b);}
    };
    for(const auto& c:cubics){auto start=project(c[0]);if(path.isEmpty()||QLineF(path.currentPosition(),start).length()>1e-7)path.moveTo(start);
        if(perspective)append(c,0);else path.cubicTo(project(c[1]),project(c[2]),project(c[3]));
    }
    return path;
}
}
