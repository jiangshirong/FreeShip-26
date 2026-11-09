// Half-body drawing rules from current FreeShip Plus. GPL-2.0-or-later.
#pragma once
#include "core/model_snapshot.h"
#include <algorithm>
namespace fs::presentation {
inline double splitLocation(const ModelSnapshot& m){return m.useDefaultSplitSectionLocation?double(float(m.length*.5)):m.splitSectionLocation;}
inline Vec3 bodyPoint(Vec3 p,double split){if(p.x<=split)p.y=-p.y;return p;}
using Segment=std::array<Vec3,2>;
inline QVector<Segment> bodySegments(Vec3 a,Vec3 b,double split){
    if((a.x<split&&b.x>split)||(b.x<split&&a.x>split)){
        const double t=(split-a.x)/(b.x-a.x);Vec3 p{split,a.y+t*(b.y-a.y),a.z+t*(b.z-a.z)};
        auto first=p,second=p;if(a.x<split)first.y=-first.y;else second.y=-second.y;
        return {{bodyPoint(a,split),first},{second,bodyPoint(b,split)}};
    }
    // A seam endpoint belongs to the segment's side; never draw across the center.
    if(a.x<split||b.x<split||(a.x==split&&b.x==split)){a.y=-a.y;b.y=-b.y;}
    return {{a,b}};
}
inline QVector<QVector<Vec3>> bodyPolygons(const QVector<Vec3>& polygon,double split){
    QVector<QVector<Vec3>> result;if(polygon.isEmpty())return result;
    bool aft=false,fore=false;for(auto p:polygon){aft|=p.x<split;fore|=p.x>split;}
    for(bool left:{true,false}){
        if(left?!aft&&fore:!fore)continue;
        QVector<Vec3> out;auto a=polygon.back();bool insideA=left?a.x<=split:a.x>=split;
        for(auto b:polygon){bool insideB=left?b.x<=split:b.x>=split;
            if(insideA!=insideB){double t=(split-a.x)/(b.x-a.x);out.append({split,a.y+t*(b.y-a.y),a.z+t*(b.z-a.z)});}
            if(insideB)out.append(b);
            a=b;insideA=insideB;
        }
        if(out.size()<3)continue;
        if(left){for(auto& p:out)p.y=-p.y;std::reverse(out.begin(),out.end());}
        result.append(out);
    }
    return result;
}
// Stations in half-body mode show the forward body on one side and the aft
// body on the other. A station exactly at the split appears on both sides.
inline std::pair<bool,bool> sectionSides(const ModelSnapshot& m,int projection,int kind,const QVector<Vec3>& path){
    double maximum=-1e300;bool offCenter=false;for(auto p:path){maximum=std::max(maximum,p.x);offCenter|=p.y!=0;}
    bool original=true,mirror=m.bothSides;
    if(!m.bothSides&&projection==2){original=kind!=1||maximum>=splitLocation(m);mirror=maximum<=splitLocation(m);}
    return {original,mirror&&offCenter&&projection!=0};
}
}
