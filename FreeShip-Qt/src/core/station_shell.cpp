// SPDX-License-Identifier: GPL-2.0-or-later
#include "station_shell.h"
#include <clipper2/clipper.engine.h>
#include <QScopeGuard>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <limits>
#include <stdexcept>
#include <thread>

namespace fs::frames {
namespace {
namespace clip = Clipper2Lib;
using Clock=std::chrono::steady_clock;
struct ShellPhase {
    double* value;
    Clock::time_point start;
    explicit ShellPhase(double* output):value(output),start(output?Clock::now():Clock::time_point{}){}
    ~ShellPhase(){if(value)*value=std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
};
constexpr double grid = 10000.;       // integer units per mm
constexpr double envelopeError = .02; // mm in the actual station plane
clip::Point64 integerPoint(QPointF point) {
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()) ||
        std::max(std::abs(point.x()),std::abs(point.y())) > 1.e10)
        throw std::runtime_error("剖面坐标超出可用精度范围。");
    return {std::llround(point.x()*grid),std::llround(point.y()*grid)};
}
double length(QPointF p) { return std::hypot(p.x(),p.y()); }

// At X, the radius-t neighbourhood of a triangle is convex. Its support
// contact along direction n maximises n·p.yz + sqrt(t² - (p.x-X)²).
// At fixed p.x the maximum is on an edge. Along each edge the unconstrained
// maximum is analytic; clamping it to the edge also covers its two vertices.
QPointF contact(const ShellTriangle& tri,double x,double t,QPointF n) {
    double best = -std::numeric_limits<double>::infinity();
    QPointF point;
    for (int i=0;i<3;++i) {
        auto a=tri[i], b=tri[(i+1)%3];
        if (a.x>b.x) std::swap(a,b);
        const double left=std::max(x-t,a.x), right=std::min(x+t,b.x);
        if (left>right) continue;
        const double dx=b.x-a.x, dn=QPointF::dotProduct(b.yz-a.yz,n);
        double at=a.x;
        QPointF center;
        if (dx==0) center=dn>0?b.yz:a.yz;
        else {
            at=std::clamp(x+t*dn/std::hypot(dx,dn),left,right);
            center=a.yz+(b.yz-a.yz)*((at-a.x)/dx);
        }
        const double d=std::abs(at-x);
        const auto rim=center+n*std::sqrt(std::max(0.,(t-d)*(t+d)));
        const double value=QPointF::dotProduct(rim,n);
        if (value>best) { best=value; point=rim; }
    }
    if (!std::isfinite(best)) throw std::runtime_error("无法计算板厚包络。");
    return point;
}

// A convex arc lies inside the triangle between its two contacts and the
// intersection of their support lines. This bounds the planar error even at
// a sharply raked bow where a fixed angular discretisation would not.
void boundArc(clip::Path64& out,const ShellTriangle& tri,double x,double t,
              QPointF n,QPointF a,QPointF m,QPointF b,int depth) {
    const QPointF tangent(-n.y(),n.x());
    const QPointF corner=a+tangent*(QPointF::dotProduct(b-a,m)/QPointF::dotProduct(tangent,m));
    const QPointF chord=b-a;
    const double squared=QPointF::dotProduct(chord,chord);
    const double u=squared?std::clamp(QPointF::dotProduct(corner-a,chord)/squared,0.,1.):0.;
    if (length(corner-a-chord*u)<=envelopeError) {
        const auto v=integerPoint(corner);
        if (out.empty() || out.back()!=v) out.push_back(v);
        return;
    }
    if (depth>=30) throw std::runtime_error("该站无法满足板厚轮廓的离散精度。");
    const QPointF middle=(n+m)/length(n+m);
    const QPointF p=contact(tri,x,t,middle);
    boundArc(out,tri,x,t,n,a,middle,p,depth+1);
    boundArc(out,tri,x,t,middle,p,m,b,depth+1);
}
clip::Path64 sectionOfShell(const ShellTriangle& tri,double x,double t) {
    const std::array<QPointF,4> directions{{{1,0},{0,1},{-1,0},{0,-1}}};
    std::array<QPointF,4> contacts;
    for(int i=0;i<4;++i) contacts[i]=contact(tri,x,t,directions[i]);
    clip::Path64 band;
    for(int i=0;i<4;++i) {
        const int j=(i+1)%4;
        boundArc(band,tri,x,t,directions[i],contacts[i],directions[j],contacts[j],0);
    }
    return band;
}

// Limit the extra work across concurrent previews, including requests that
// are still winding down after cancellation. Leave cores for the editor.
std::atomic_uint unionWorkers{0};
bool reserveUnionWorker() {
    static const unsigned limit=std::clamp(std::thread::hardware_concurrency(),2u,5u)-2;
    auto active=unionWorkers.load(std::memory_order_relaxed);
    while(active<limit) {
        if(unionWorkers.compare_exchange_weak(active,active+1,std::memory_order_relaxed))return true;
    }
    return false;
}

clip::Paths64 unionBands(clip::Paths64 bands,const std::atomic_bool& cancelled) {
    constexpr size_t batchSize=64;
    if(cancelled.load())return {};
    if(bands.size()>batchSize){
        const auto middle=bands.begin()+bands.size()/2;
        clip::Paths64 second(std::make_move_iterator(middle),std::make_move_iterator(bands.end()));
        bands.erase(middle,bands.end());
        // Preserve the split tree and left-before-right input to Clipper.
        // Changing the partition can change rounded intersections.
        const auto mergeRight=[&]{return unionBands(std::move(second),cancelled);};
        std::future<clip::Paths64> worker;
        if(second.size()>=2*batchSize && !cancelled.load() && reserveUnionWorker()) {
            try {
                worker=std::async(std::launch::async,[&]{
                    const auto release=qScopeGuard([]{unionWorkers.fetch_sub(1,std::memory_order_relaxed);});
                    return mergeRight();
                });
            } catch(const std::system_error&) {
                unionWorkers.fetch_sub(1,std::memory_order_relaxed);
            } catch(...) {
                unionWorkers.fetch_sub(1,std::memory_order_relaxed);
                throw;
            }
        }
        // The async future joins on destruction, including if the left merge
        // throws, before its referenced inputs go out of scope.
        bands=unionBands(std::move(bands),cancelled);
        auto right=worker.valid()?worker.get():mergeRight();
        bands.insert(bands.end(),std::make_move_iterator(right.begin()),std::make_move_iterator(right.end()));
    }
    if(cancelled.load()||bands.size()<=1)return bands;
    clip::Clipper64 engine;
    engine.AddSubject(bands);
    clip::Paths64 combined;
    if(!engine.Execute(clip::ClipType::Union,clip::FillRule::NonZero,combined))
        throw std::runtime_error("无法计算板厚包络。");
    return combined;
}
}
QList<QPolygonF> innerContours(const QList<QPolygonF>& exterior,const std::vector<ShellTriangle>& nearby,
                              double x,double t,const std::atomic_bool& cancelled,ShellContourProfile* profile) {
    clip::Paths64 bands;
    size_t vertices=0;
    if(profile)profile->triangles=nearby.size();
    {
        ShellPhase elapsed(profile?&profile->envelopeMs:nullptr);
        for(const auto& triangle:nearby) {
            if(cancelled.load()) return {};
            auto band=sectionOfShell(triangle,x,t);
            const double area=clip::Area(band);
            if(area==0) continue;
            if(area<0) std::reverse(band.begin(),band.end());
            vertices+=band.size();
            if(vertices>5000000) throw std::runtime_error("该站板厚包络过于复杂，请单独选择船壳图层。");
            bands.push_back(std::move(band));
        }
    }
    if(profile){profile->bands=bands.size();profile->vertices=vertices;}
    // Merge groups of overlapping bands before the final difference. With a
    // large thickness, a single sweep over all interior edges is expensive.
    {
        ShellPhase elapsed(profile?&profile->unionMs:nullptr);
        if(bands.size()>128)bands=unionBands(std::move(bands),cancelled);
    }
    if(cancelled.load())return {};
    ShellPhase elapsed(profile?&profile->differenceMs:nullptr);
    QList<QPolygonF> result;
    for(const auto& outline:exterior) {
        clip::Path64 subject;
        for(auto p:outline) subject.push_back(integerPoint(p));
        clip::Clipper64 engine;
        engine.AddSubject({subject});
        engine.AddClip(bands);
        clip::Paths64 remaining;
        if(!engine.Execute(clip::ClipType::Difference,clip::FillRule::NonZero,remaining) ||
           remaining.size()!=1 || remaining.front().size()<3)
            throw std::runtime_error("扣厚后轮廓消失、分裂或出现内孔，请检查板厚和船壳图层。");
        QPolygonF loop;
        for(auto p:remaining.front()) loop.append(QPointF(p.x/grid,p.y/grid));
        loop.append(loop.front());
        result.append(std::move(loop));
    }
    return result;
}
}

