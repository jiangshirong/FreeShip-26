// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "station_deviation.h"
#include <QHash>
#include <QLineF>

namespace fs {
struct StationPairing {
    struct Span { QLineF reference,model; };
    QVector<Span> spans;
    station_distance::Contour reference,model;
    StationDeviation maximum;

    StationDeviation pairAt(const station_distance::Contour::Hit& hit,bool onReference) const {
        const auto& index=onReference?reference:model;
        const auto& segment=index.segments[hit.edge];
        const auto& span=spans[segment.path];
        const double t=std::clamp(QPointF::dotProduct(hit.point-segment.a,segment.unit)/segment.length,0.,1.);
        const auto a=span.reference.pointAt(t),b=span.model.pointAt(t);
        return {QLineF(a,b).length(),0,a,b};
    }
};

namespace station_pairing {
// Imported offsets may provide each edge as a separate marker. Join only at
// coincident endpoints. Never bridge a gap or arbitrarily follow a branch.
inline QPolygonF chain(const QVector<QPolygonF>& paths,const std::atomic_bool* cancel){
    constexpr double joinMm=.01;
    using Cell=QPair<qint64,qint64>;
    QHash<Cell,QVector<int>> grid;
    QVector<QPointF> points;
    QVector<QVector<int>> edges;
    QVector<QPair<int,int>> ends;
    const auto node=[&](QPointF point){
        const Cell cell(qint64(std::floor(point.x()/joinMm)),qint64(std::floor(point.y()/joinMm)));
        for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy){
            const auto found=grid.constFind({cell.first+dx,cell.second+dy});
            if(found==grid.cend())continue;
            for(int i:*found)if(QLineF(points[i],point).length()<=joinMm)return i;
        }
        const int id=int(points.size());
        points.append(point);edges.append(QVector<int>{});grid[cell].append(id);
        return id;
    };
    QVector<QPolygonF> nonempty;
    for(const auto& path:paths){
        if(station_distance::cancelled(cancel))return {};
        if(path.size()<2)continue;
        auto a=node(path.first()),b=node(path.last());
        if(a==b&&std::all_of(path.begin(),path.end(),[&](auto p){return QLineF(p,path.first()).length()<=joinMm;}))continue;
        const int edge=int(ends.size());
        ends.append({a,b});nonempty.append(path);
        edges[a].append(edge);edges[b].append(edge);
    }
    if(ends.isEmpty())return {};
    int start=0;
    const auto ordered=[&](int a,int b){
        const auto x=points[a],y=points[b];
        return x.y()<y.y()||(x.y()==y.y()&&std::abs(x.x())<std::abs(y.x()));
    };
    int openEnds=0;
    for(int i=0;i<edges.size();++i){
        if(edges[i].size()>2)return {};
        if(edges[i].size()==1){if(openEnds==0||ordered(i,start))start=i;++openEnds;}
    }
    if(openEnds!=0&&openEnds!=2)return {};
    QPolygonF line;
    QVector<bool> visited(ends.size(),false);
    int at=start;
    for(qsizetype i=0;i<ends.size();++i){
        const auto& incident=edges[at];
        const auto next=std::find_if(incident.begin(),incident.end(),[&](int edge){return !visited[edge];});
        if(next==incident.end()||station_distance::cancelled(cancel))return {};
        const int edge=*next;
        visited[edge]=true;
        const auto& path=nonempty[edge];
        const auto [a,b]=ends[edge];
        const bool forward=a==at;
        for(qsizetype j=0;j<path.size();++j){
            const auto p=path[forward?j:path.size()-1-j];
            if(line.isEmpty()||line.back()!=p){
                if(j==0&&!line.isEmpty())continue;
                line.append(p);
            }
        }
        at=forward?b:a;
    }
    if(openEnds==0){
        line.removeLast();
        if(line.size()<3)return {};
        double area=0;
        for(qsizetype i=0;i<line.size();++i){
            const auto a=line[i],b=line[(i+1)%line.size()];area+=a.x()*b.y()-a.y()*b.x();
        }
        if(area<0)std::reverse(line.begin(),line.end());
        const auto lowest=std::min_element(line.begin(),line.end(),[](auto a,auto b){
            return a.y()<b.y()||(a.y()==b.y()&&std::abs(a.x())<std::abs(b.x()));
        });
        std::rotate(line.begin(),lowest,line.end());line.append(line.first());
    }
    return line;
}

struct Landmarks { QVector<qsizetype> vertex; QVector<bool> turnsLeft; };
inline Landmarks landmarks(const QPolygonF& line){
    constexpr double cornerCos=.9659258263; // 15-degree change of tangent
    Landmarks result{{0},{}};
    for(qsizetype i=1;i+1<line.size();++i){
        const QLineF in(line[i-1],line[i]),out(line[i],line[i+1]);
        const auto a=in.p2()-in.p1(),b=out.p2()-out.p1();
        if(QPointF::dotProduct(a,b)<cornerCos*in.length()*out.length()){
            result.vertex.append(i);result.turnsLeft.append(a.x()*b.y()>a.y()*b.x());
        }
    }
    result.vertex.append(line.size()-1);
    return result;
}

inline QVector<double> relativeLengths(const QPolygonF& line,qsizetype first,qsizetype last){
    QVector<double> lengths{0.};
    for(auto i=first;i<last;++i)lengths.append(lengths.back()+QLineF(line[i],line[i+1]).length());
    const auto total=lengths.back();
    for(auto& x:lengths)x/=total;
    return lengths;
}

inline bool appendSpans(StationPairing& pairing,const QPolygonF& reference,const QPolygonF& model,const std::atomic_bool* cancel){
    if(reference.size()<2||model.size()<2||reference.isClosed()!=model.isClosed())return false;
    const auto a=landmarks(reference),b=landmarks(model);
    if(a.turnsLeft!=b.turnsLeft)return false;
    for(qsizetype k=1;k<a.vertex.size();++k){
        const auto i=a.vertex[k-1],j=b.vertex[k-1];
        const auto ra=relativeLengths(reference,i,a.vertex[k]),ma=relativeLengths(model,j,b.vertex[k]);
        qsizetype r=1,m=1;
        auto p=reference[i],q=model[j];
        while(r<ra.size()&&m<ma.size()){
            if(station_distance::cancelled(cancel))return false;
            const double t=std::min(ra[r],ma[m]);
            const QLineF rr(reference[i+r-1],reference[i+r]),mm(model[j+m-1],model[j+m]);
            const auto aa=ra[r]==t?rr.p2():rr.pointAt((t-ra[r-1])/(ra[r]-ra[r-1]));
            const auto bb=ma[m]==t?mm.p2():mm.pointAt((t-ma[m-1])/(ma[m]-ma[m-1]));
            if(p!=aa&&q!=bb)pairing.spans.append({QLineF(p,aa),QLineF(q,bb)});
            r+=ra[r]==t;m+=ma[m]==t;p=aa;q=bb;
        }
    }
    return true;
}
} // namespace station_pairing

// Each input entry is one side of the hull, in the same order for both models.
inline std::optional<StationPairing> pairStationContours(const QVector<QVector<QPolygonF>>& reference,
        const QVector<QVector<QPolygonF>>& model,const std::atomic_bool* cancel=nullptr){
    if(reference.size()!=model.size())return std::nullopt;
    StationPairing out;
    for(qsizetype i=0;i<reference.size();++i){
        if(reference[i].isEmpty()&&model[i].isEmpty())continue;
        if(!station_pairing::appendSpans(out,station_pairing::chain(reference[i],cancel),
                station_pairing::chain(model[i],cancel),cancel))return std::nullopt;
    }
    QVector<QPolygonF> refs,models;
    out.maximum.maximumMm=-1;
    for(const auto& span:out.spans){
        if(station_distance::cancelled(cancel))return std::nullopt;
        refs.append(QPolygonF{span.reference.p1(),span.reference.p2()});
        models.append(QPolygonF{span.model.p1(),span.model.p2()});
        // Over a span the vector between points varies linearly. Its norm is
        // convex, so the span maximum is attained at one of its endpoints.
        for(const auto t:{0.,1.}){
            auto a=span.reference.pointAt(t),b=span.model.pointAt(t);
            const auto distance=QLineF(a,b).length();
            if(distance>out.maximum.maximumMm)out.maximum={distance,0,a,b};
        }
    }
    if(!out.reference.load(refs,cancel)||!out.model.load(models,cancel))return std::nullopt;
    return out;
}
} // namespace fs
