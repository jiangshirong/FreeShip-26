// SPDX-License-Identifier: GPL-2.0-or-later
#include "surface_analysis.h"
#include <QHash>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace {
using fs::Vec3;
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 unit(Vec3 a){const double n=std::hypot(a.x,a.y,a.z);return n>1e-30?Vec3{a.x/n,a.y/n,a.z/n}:Vec3{};}
quint64 edge(int a,int b){return (quint64(quint32(std::min(a,b)))<<32)|quint32(std::max(a,b));}
}
QColor gaussianColor(double value,double minimum,double maximum){
    const QColor zero("#dce8df");
    if(std::abs(value)<1e-4)return zero;
    const QColor end=value>0?QColor("#af3f2f"):QColor("#2452a0");
    const double bound=value>0?maximum:minimum;
    const double ratio=bound!=0?std::clamp(value/bound,0.,1.):0;
    const double strength=std::pow(ratio,.35);
    const auto blend=[strength](int start,int stop){return qRound(start+(stop-start)*strength);};
    return {blend(zero.red(),end.red()),blend(zero.green(),end.green()),blend(zero.blue(),end.blue())};
}
std::shared_ptr<const SurfaceAnalysis> analyzeSurface(const fs::ModelSnapshot& snapshot){
    if(!snapshot.surfaceTopology)throw std::runtime_error("Surface topology is unavailable");
    // Four cameras share one immutable analysis. Expired documents are evicted.
    struct Entry {std::weak_ptr<const fs::SurfaceTopology> owner;std::weak_ptr<const SurfaceAnalysis> analysis;};
    static std::map<const fs::SurfaceTopology*,Entry> cache;
    for(auto it=cache.begin();it!=cache.end();)if(it->second.owner.expired()||it->second.analysis.expired())it=cache.erase(it);else ++it;
    const auto key=snapshot.surfaceTopology.get();auto found=cache.find(key);
    if(found!=cache.end()&&found->second.owner.lock()==snapshot.surfaceTopology)if(auto old=found->second.analysis.lock())return old;
    const auto& mesh=*key;auto result=std::make_shared<SurfaceAnalysis>();auto& out=*result;
    const auto n=mesh.points.size();out.angleDefect.fill(0,n);out.normals.resize(n);out.valid.fill(true,n);
    QVector<int> incident(n),boundary(n),invalid(n);QVector<QVector<int>> fans(n);
    QVector<int> regionAtPoint(n,-1);
    QVector<bool> plateBoundary(n,false);
    QVector<double> angles;
    const double pi=std::acos(-1.);
    struct Edge {int count=0,orientation=0;};QHash<quint64,Edge> edges;
    for(qsizetype faceIndex=0;faceIndex<mesh.faces.size();++faceIndex){
        const auto& face=mesh.faces[faceIndex];
        const auto region=mesh.plateRegions.value(faceIndex,0);
        Vec3 area{};for(int j=2;j<face.size();++j)area=add(area,cross(sub(mesh.points[face[j-1]],mesh.points[face[0]]),sub(mesh.points[face[j]],mesh.points[face[0]])));
        const bool degenerate=std::hypot(area.x,area.y,area.z)<=1e-30;
        // A twisted quad can have four polygon-edge angles that hide its
        // angular defect. Use the same diagonal as the display mesh.
        angles.fill(0.,face.size());
        for(qsizetype j=2;j<face.size();++j){
            const auto a=mesh.points[face[0]],b=mesh.points[face[j-1]],c=mesh.points[face[j]];
            const auto ab=sub(b,a),ac=sub(c,a),bc=sub(c,b),normal=cross(ab,ac);
            const double sine=std::hypot(normal.x,normal.y,normal.z);
            // Subtract reversed fan triangles for concave polygon corners.
            const double sign=dot(area,normal)<0?-1.:1.;
            angles[0]+=sign*std::atan2(sine,dot(ab,ac));
            angles[j-1]+=sign*std::atan2(sine,-dot(ab,bc));
            angles[j]+=sign*std::atan2(sine,dot(ac,bc));
        }
        for(int j=0;j<face.size();++j){const int p=face[j],prev=face[(j+face.size()-1)%face.size()],next=face[(j+1)%face.size()];
            if(regionAtPoint[p]<0)regionAtPoint[p]=region;
            else if(regionAtPoint[p]!=region)plateBoundary[p]=true;
            auto& e=edges[edge(p,next)];++e.count;e.orientation+=p<next?1:-1;
            fans[p].append(prev);fans[p].append(next);if(degenerate)invalid[p]=1;
            auto a=sub(mesh.points[prev],mesh.points[p]),b=sub(mesh.points[next],mesh.points[p]);
            const double la=std::hypot(a.x,a.y,a.z),lb=std::hypot(b.x,b.y,b.z);++incident[p];
            if(la<=1e-30||lb<=1e-30){invalid[p]=1;continue;}
            a={a.x/la,a.y/la,a.z/la};b={b.x/lb,b.y/lb,b.z/lb};
            out.angleDefect[p]+=angles[j]<-1e-12?angles[j]+2*pi:std::max(angles[j],0.);
            // Use actual vector addition. The old AddPoint helper incorrectly
            // clamped negative Y on normals; that would distort zebra bands.
            out.normals[p]=add(out.normals[p],unit(cross(b,a)));
        }
    }
    for(auto it=edges.cbegin();it!=edges.cend();++it){int a=int(it.key()>>32),b=int(quint32(it.key()));
        if(it->count==1){boundary[a]=boundary[b]=1;}
        else if(it->count!=2||it->orientation!=0){invalid[a]=invalid[b]=1;}
    }
    for(int p=0;p<n;++p){
        // A bow-tie vertex may have two closed fans despite manifold edges.
        const auto& fan=fans[p];QHash<int,QVector<int>> links;
        for(int j=0;j+1<fan.size();j+=2){links[fan[j]].append(fan[j+1]);links[fan[j+1]].append(fan[j]);}
        QSet<int> reached;QVector<int> pending;if(!fan.isEmpty())pending.append(fan.front());
        while(!pending.isEmpty()){int v=pending.takeLast();if(reached.contains(v))continue;reached.insert(v);for(int next:links.value(v))if(!reached.contains(next))pending.append(next);}
        if(reached.size()!=links.size())invalid[p]=1;
        out.normals[p]=unit(out.normals[p]);out.valid[p]=incident[p]>0&&!boundary[p]&&!invalid[p];
        if(boundary[p])++out.boundary;else if(incident[p]&&invalid[p])++out.invalid;
        out.angleDefect[p]=out.valid[p]?360-out.angleDefect[p]*180/pi:0;
        if(out.valid[p]){out.minimum=std::min(out.minimum,out.angleDefect[p]);out.maximum=std::max(out.maximum,out.angleDefect[p]);}
    }
    out.developableEvaluated.resize(n);
    for(int p=0;p<n;++p)out.developableEvaluated[p]=out.valid[p]&&!plateBoundary[p];

    // Angle deficit needs a complete neighborhood. For display, carry nearby
    // evaluated colors to an open edge or plate seam on that side alone.
    // The extension never supplies a measurement or crosses an invalid vertex.
    struct BoundaryShade {
        QVector<int> neighbors;
        int distance=-1,count=0;
        double nonzero=0;
        quint8 classes=0;
    };
    QHash<quint64,int> boundaryIndices;
    QVector<BoundaryShade> boundaryShades;
    const auto boundaryKey=[](int region,int p){return (quint64(quint32(region))<<32)|quint32(p);};
    const auto boundaryIndex=[&](int region,int p){
        if(invalid[p]||(!boundary[p]&&!plateBoundary[p]))return -1;
        const auto key=boundaryKey(region,p);
        auto it=boundaryIndices.constFind(key);
        if(it!=boundaryIndices.cend())return it.value();
        const int index=int(boundaryShades.size());
        boundaryIndices.insert(key,index);
        boundaryShades.append(BoundaryShade{});
        return index;
    };
    const auto isNonzero=[&](int p){return std::abs(out.angleDefect[p])>=1e-4;};
    for(int i=0;i<mesh.faces.size();++i){
        const auto& face=mesh.faces[i];
        const int region=mesh.plateRegions.value(i,0);
        for(int j=0;j<face.size();++j){
            const int a=face[j],b=face[(j+1)%face.size()];
            const int ia=boundaryIndex(region,a),ib=boundaryIndex(region,b);
            if(ia>=0&&ib>=0){
                boundaryShades[ia].neighbors.append(ib);
                boundaryShades[ib].neighbors.append(ia);
            }else{
                const int index=ia>=0?ia:ib;
                const int sample=ia>=0?b:a;
                if(index<0||!out.developableEvaluated[sample])continue;
                auto& shade=boundaryShades[index];
                const bool nonzero=isNonzero(sample);
                shade.nonzero+=nonzero;
                shade.classes|=shadeBit(nonzero?ShadeClass::Nonzero:ShadeClass::NearZero);
                ++shade.count;
            }
        }
    }
    QVector<int> queue;
    for(int i=0;i<boundaryShades.size();++i)if(boundaryShades[i].count){
        boundaryShades[i].distance=0;
        queue.append(i);
    }
    for(int i=0;i<queue.size();++i){
        const auto& from=boundaryShades[queue[i]];
        for(int index:from.neighbors){
            auto& to=boundaryShades[index];
            if(to.distance<0){to.distance=from.distance+1;queue.append(index);}
            if(to.distance!=from.distance+1)continue;
            to.nonzero+=from.nonzero/from.count;
            to.classes|=from.classes;
            ++to.count;
        }
    }
    const auto unknown=developableDisplayColor(ShadeClass::Unknown);
    const auto nearZero=developableDisplayColor(ShadeClass::NearZero);
    const auto nonzero=developableDisplayColor(ShadeClass::Nonzero);
    const auto boundaryColor=[&](double weight){
        const auto mix=[weight](int a,int b){return qRound(a+(b-a)*weight);};
        return QColor(mix(nearZero.red(),nonzero.red()),mix(nearZero.green(),nonzero.green()),
                      mix(nearZero.blue(),nonzero.blue()));
    };
    for(int i=0;i<mesh.faces.size();++i){
        const auto& face=mesh.faces[i];
        const int region=mesh.plateRegions.value(i,0);
        for(int j=2;j<face.size();++j){
            const int indices[]{face[0],face[j-1],face[j]};
            SurfaceShadeTriangle triangle;
            const auto flat=unit(cross(sub(mesh.points[indices[1]],mesh.points[indices[0]]),
                                       sub(mesh.points[indices[2]],mesh.points[indices[0]])));
            for(int k=0;k<3;++k){
                const int p=indices[k];
                auto& v=triangle[k];
                v.normal=(mesh.types[p]==0||mesh.types[p]==2)?out.normals[p]:flat;
                const auto sign=isNonzero(p)?(out.angleDefect[p]>0?ShadeClass::Positive:ShadeClass::Negative):ShadeClass::NearZero;
                v.gaussian=out.valid[p]?gaussianColor(out.angleDefect[p],out.minimum,out.maximum):unknown;
                v.gaussianClasses=shadeBit(out.valid[p]?sign:ShadeClass::Unknown);
                if(out.developableEvaluated[p]){
                    const auto kind=isNonzero(p)?ShadeClass::Nonzero:ShadeClass::NearZero;
                    v.developable=developableDisplayColor(kind);
                    v.developableClasses=shadeBit(kind);
                }else{
                    const int index=boundaryIndices.value(boundaryKey(region,p),-1);
                    const auto* shade=index>=0?&boundaryShades[index]:nullptr;
                    v.developable=shade&&shade->count?boundaryColor(shade->nonzero/shade->count):unknown;
                    v.developableClasses=shade&&shade->count?shade->classes:shadeBit(ShadeClass::Unknown);
                }
            }
            out.triangles.append(triangle);
        }
    }
    if(out.triangles.size()!=snapshot.triangles.size())throw std::runtime_error("Analysis triangle count mismatch");
    cache[key]={snapshot.surfaceTopology,result};return result;
}
