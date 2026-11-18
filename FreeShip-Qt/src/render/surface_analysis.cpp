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
    double f=.5;
    if(std::abs(value)>=1e-4){
        const double bound=value>0?maximum:minimum;
        const double ratio=bound!=0?std::clamp(value/bound,0.,1.):0;
        f=.5+(value>0?-.5:.5)*std::pow(ratio,.1);
    }
    // Legacy 420..700 nm display ramp, with its 0.8 gamma correction.
    const double w=420+(1-f)*280;double r=0,g=0,b=0;
    if(w<440){r=(440-w)/60;b=1;}else if(w<490){g=(w-440)/50;b=1;}
    else if(w<510){g=1;b=(510-w)/20;}else if(w<580){r=(w-510)/70;g=1;}
    else if(w<645){r=1;g=(645-w)/65;}else r=1;
    auto c=[](double x){return qRound(255*std::pow(std::clamp(x,0.,1.),.8));};return {c(r),c(g),c(b)};
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
    struct Edge {int count=0,orientation=0;};QHash<quint64,Edge> edges;
    for(const auto& face:mesh.faces){
        Vec3 area{};for(int j=2;j<face.size();++j)area=add(area,cross(sub(mesh.points[face[j-1]],mesh.points[face[0]]),sub(mesh.points[face[j]],mesh.points[face[0]])));
        const bool degenerate=std::hypot(area.x,area.y,area.z)<=1e-30;
        for(int j=0;j<face.size();++j){const int p=face[j],prev=face[(j+face.size()-1)%face.size()],next=face[(j+1)%face.size()];
            auto& e=edges[edge(p,next)];++e.count;e.orientation+=p<next?1:-1;
            fans[p].append(prev);fans[p].append(next);if(degenerate)invalid[p]=1;
            auto a=sub(mesh.points[prev],mesh.points[p]),b=sub(mesh.points[next],mesh.points[p]);
            const double la=std::hypot(a.x,a.y,a.z),lb=std::hypot(b.x,b.y,b.z);++incident[p];
            if(la<=1e-30||lb<=1e-30){invalid[p]=1;continue;}
            a={a.x/la,a.y/la,a.z/la};b={b.x/lb,b.y/lb,b.z/lb};
            out.angleDefect[p]+=std::atan2(std::hypot(cross(a,b).x,cross(a,b).y,cross(a,b).z),std::clamp(dot(a,b),-1.,1.));
            // Use actual vector addition. The old AddPoint helper incorrectly
            // clamped negative Y on normals; that would distort zebra bands.
            out.normals[p]=add(out.normals[p],unit(cross(b,a)));
        }
    }
    for(auto it=edges.cbegin();it!=edges.cend();++it){int a=int(it.key()>>32),b=int(quint32(it.key()));
        if(it->count==1){boundary[a]=boundary[b]=1;}
        else if(it->count!=2||it->orientation!=0){invalid[a]=invalid[b]=1;}
    }
    const double pi=std::acos(-1.);
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
    const QColor unknown(150,150,150);
    for(const auto& face:mesh.faces)for(int j=2;j<face.size();++j){const int indices[]{face[0],face[j-1],face[j]};SurfaceShadeTriangle triangle;
        auto flat=unit(cross(sub(mesh.points[indices[1]],mesh.points[indices[0]]),sub(mesh.points[indices[2]],mesh.points[indices[0]])));
        for(int k=0;k<3;++k){int p=indices[k];auto& v=triangle[k];v.normal=(mesh.types[p]==0||mesh.types[p]==2)?out.normals[p]:flat;
            v.gaussian=out.valid[p]?gaussianColor(out.angleDefect[p],out.minimum,out.maximum):unknown;
            v.developable=out.valid[p]?(std::abs(out.angleDefect[p])<1e-4?QColor(0,255,0):QColor(255,0,0)):unknown;}
        out.triangles.append(triangle);
    }
    if(out.triangles.size()!=snapshot.triangles.size())throw std::runtime_error("Analysis triangle count mismatch");
    cache[key]={snapshot.surfaceTopology,result};return result;
}
