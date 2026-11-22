// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QPolygonF>
#include <QVector>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <vector>

namespace fs {
struct StationDeviation {
    double maximumMm = 0, errorBoundMm = 0;
    QPointF referenceMm, modelMm;
};

namespace station_distance {
inline bool cancelled(const std::atomic_bool* flag) {
    return flag && flag->load(std::memory_order_relaxed);
}
struct Segment { QPointF a,b,unit; double length; qsizetype path; };

// A small AABB tree keeps a nearest-point query independent of vertex sampling density.
class Contour {
    struct Node {
        double xmin,xmax,ymin,ymax;
        int first,last,left=-1,right=-1;
        double lowerBound(QPointF p) const {
            return std::hypot(std::max({xmin-p.x(),p.x()-xmax,0.}),
                              std::max({ymin-p.y(),p.y()-ymax,0.}));
        }
    };
    std::vector<Node> nodes;
    int build(int first,int last,const std::atomic_bool* cancel) {
        if(cancelled(cancel))return -1;
        const auto p=segments[first].a;
        Node node{p.x(),p.x(),p.y(),p.y(),first,last};
        for(int i=first;i<last;++i)for(auto v:{segments[i].a,segments[i].b}){
            node.xmin=std::min(node.xmin,v.x());node.xmax=std::max(node.xmax,v.x());
            node.ymin=std::min(node.ymin,v.y());node.ymax=std::max(node.ymax,v.y());
        }
        const int index=int(nodes.size());nodes.push_back(node);
        if(last-first>8){
            const bool byX=node.xmax-node.xmin>=node.ymax-node.ymin;
            const int mid=first+(last-first)/2;
            std::nth_element(segments.begin()+first,segments.begin()+mid,segments.begin()+last,
                [byX](const Segment& a,const Segment& b){
                    return byX ? a.a.x()+a.b.x()<b.a.x()+b.b.x()
                               : a.a.y()+a.b.y()<b.a.y()+b.b.y();
                });
            const int left=build(first,mid,cancel),right=build(mid,last,cancel);
            nodes[index].left=left;nodes[index].right=right;
        }
        return index;
    }
public:
    struct Hit { int edge=-1; double distance=std::numeric_limits<double>::infinity(); QPointF point; };
    std::vector<Segment> segments;
    bool load(const QVector<QPolygonF>& lines,const std::atomic_bool* cancel) {
        for(qsizetype path=0;path<lines.size();++path){
            const auto& line=lines[path];
            // Leave headroom for differences, projections and sums of two bounds.
            const double limit=std::numeric_limits<double>::max()/16;
            for(const auto& p:line){
                if(cancelled(cancel))return false;
                if(!std::isfinite(p.x())||!std::isfinite(p.y())||
                   std::abs(p.x())>limit||std::abs(p.y())>limit)return false;
            }
            for(qsizetype i=1;i<line.size();++i){
                if(cancelled(cancel))return false;
                const auto a=line[i-1],b=line[i];
                const auto ab=b-a;
                const double length=std::hypot(ab.x(),ab.y());
                if(length>0)segments.push_back({a,b,ab/length,length,path});
            }
        }
        if(segments.empty())return false;
        build(0,int(segments.size()),cancel);
        return !cancelled(cancel);
    }
    Hit project(int edge,QPointF p) const {
        const auto& s=segments[edge];
        const double along=QPointF::dotProduct(p-s.a,s.unit);
        const auto foot=along<=0?s.a:along>=s.length?s.b:s.a+s.unit*along;
        return {edge,std::hypot(p.x()-foot.x(),p.y()-foot.y()),foot};
    }
    Hit nearest(QPointF p) const {
        Hit hit;
        auto visit=[&](auto&& self,int index)->void{
            const auto& n=nodes[index];
            if(n.left<0){
                for(int i=n.first;i<n.last;++i){
                    const auto candidate=project(i,p);
                    if(candidate.distance<hit.distance)hit=candidate;
                }
            }else{
                int near=n.left,far=n.right;
                const double dl=nodes[near].lowerBound(p),dr=nodes[far].lowerBound(p);
                if(dl>dr)std::swap(near,far);
                if(std::min(dl,dr)<hit.distance)self(self,near);
                if(std::max(dl,dr)<hit.distance)self(self,far);
            }
        };
        visit(visit,0);return hit;
    }
};

struct Sample { QPointF point; Contour::Hit hit; };
struct Interval {
    Sample a,b;
    double upper;
    bool operator<(const Interval& other)const{return upper<other.upper;}
};

// Distance to any one closed segment is convex along the source segment. The
// two endpoint-nearest target segments therefore supply two affine upper bounds.
// Their minimum peaks either at an endpoint or where those bounds intersect.
inline double upperBound(const Sample& a,const Sample& b,const Contour& target) {
    const double da=a.hit.distance,db=b.hit.distance;
    const double ends=std::max(da,db);
    if(a.hit.edge==b.hit.edge)return ends;
    const double crossA=target.project(b.hit.edge,a.point).distance;
    const double crossB=target.project(a.hit.edge,b.point).distance;
    const double gapA=std::max(0.,crossA-da),gapB=std::max(0.,crossB-db);
    if(gapA+gapB==0)return ends;
    return std::max(ends, da+(crossB-da)*(gapA/(gapA+gapB)));
}
} // namespace station_distance

// The greatest distance from any reference point to its nearest model point.
// The shown pair realizes maximumMm; the true maximum is at most errorBoundMm higher.
// Distinct runs are never joined. Repeat the first vertex for a closed curve.
inline std::optional<StationDeviation> compareIndexedStationContours(
        const station_distance::Contour& reference,const station_distance::Contour& model,double toleranceMm,
        const std::atomic_bool* cancel=nullptr) {
    using namespace station_distance;
    if(!std::isfinite(toleranceMm)||toleranceMm<=0||reference.segments.empty()||model.segments.empty())return std::nullopt;
    StationDeviation result;result.maximumMm=-1;
    double upper=0;
    std::priority_queue<Interval> pending;
    auto sample=[&](QPointF point){
        const auto hit=model.nearest(point);
        if(hit.distance>result.maximumMm)result={hit.distance,0,point,hit.point};
        return Sample{point,hit};
    };
    auto consider=[&](const Sample& a,const Sample& b){
        const auto bound=upperBound(a,b,model);
        if(bound>result.maximumMm+toleranceMm)pending.push({a,b,bound});
        else upper=std::max(upper,bound);
    };
    for(const auto& edge:reference.segments){
        if(cancelled(cancel))return std::nullopt;
        const auto a=sample(edge.a),b=sample(edge.b);
        consider(a,b);
    }
    while(!pending.empty()){
        if(cancelled(cancel))return std::nullopt;
        const auto span=pending.top();
        if(span.upper<=result.maximumMm+toleranceMm){upper=std::max(upper,span.upper);break;}
        pending.pop();
        const QPointF mid=span.a.point+(span.b.point-span.a.point)*0.5;
        // If a representable midpoint no longer exists, keep its remaining bound.
        if((mid.x()==span.a.point.x()&&mid.y()==span.a.point.y())||
           (mid.x()==span.b.point.x()&&mid.y()==span.b.point.y())){
            upper=std::max(upper,span.upper);continue;
        }
        const auto m=sample(mid);
        consider(span.a,m);consider(m,span.b);
    }
    if(cancelled(cancel))return std::nullopt;
    result.errorBoundMm=std::max(0.,upper-result.maximumMm);
    return result;
}

inline std::optional<StationDeviation> compareStationContours(
        const QVector<QPolygonF>& referenceMm,const QVector<QPolygonF>& modelMm,double toleranceMm=0.01,
        const std::atomic_bool* cancel=nullptr) {
    station_distance::Contour reference,model;
    if(!reference.load(referenceMm,cancel)||!model.load(modelMm,cancel))return std::nullopt;
    return compareIndexedStationContours(reference,model,toleranceMm,cancel);
}
} // namespace fs
