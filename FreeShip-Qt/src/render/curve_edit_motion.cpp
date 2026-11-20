// SPDX-License-Identifier: GPL-2.0-or-later
#include "curve_edit_motion.h"
#include <QColor>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <set>

namespace fs::curveEdit {
namespace {
Vec3 difference(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 sum(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
double mix(double a,double b,double t){return a+(b-a)*t;}
Vec3 mix(Vec3 a,Vec3 b,double t){return {mix(a.x,b.x,t),mix(a.y,b.y,t),mix(a.z,b.z,t)};}
double distance(Vec3 a,Vec3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
bool equal(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool equalShape(const Shape& a,const Shape& b){
    if(a.path.size()!=b.path.size()||a.cubics.size()!=b.cubics.size())return false;
    if(!std::equal(a.path.begin(),a.path.end(),b.path.begin(),equal))return false;
    return std::equal(a.cubics.begin(),a.cubics.end(),b.cubics.begin(),
        [](const auto& x,const auto& y){return std::equal(x.begin(),x.end(),y.begin(),equal);});
}
QVector<double> lengths(const QVector<Vec3>& path){
    QVector<double> values(path.size(),0.);
    for(qsizetype i=1;i<path.size();++i)values[i]=values[i-1]+distance(path[i-1],path[i]);
    if(!values.isEmpty()&&values.back()>0){
        const double extent=values.back();
        for(double& length:values)length/=extent;
    }
    return values;
}
Vec3 at(const QVector<Vec3>& points,const QVector<double>& lengths,double t){
    auto next=std::upper_bound(lengths.begin(),lengths.end(),t);
    if(next==lengths.begin())return points.front();
    if(next==lengths.end())return points.back();
    const auto i=next-lengths.begin();
    return mix(points[i-1],points[i],(t-lengths[i-1])/(lengths[i]-lengths[i-1]));
}
double pathLength(const QVector<Vec3>& path){
    double length=0;
    for(qsizetype i=1;i<path.size();++i)length+=distance(path[i-1],path[i]);
    return length;
}
double orientationCost(const QVector<Vec3>& a,const QVector<double>& aLengths,
                       const QVector<Vec3>& b,const QVector<double>& bLengths,bool reversed){
    double cost=0;
    for(double t:{0.,.25,.75,1.})cost+=distance(at(a,aLengths,t),at(b,bLengths,reversed?1-t:t));
    return cost;
}
double correspondence(const Shape& a,const Shape& b){
    if(a.path.size()<2||b.path.size()<2)return std::numeric_limits<double>::infinity();
    const double lengthA=pathLength(a.path),lengthB=pathLength(b.path);
    const bool closedA=distance(a.path.front(),a.path.back())<=lengthA*1e-8;
    const bool closedB=distance(b.path.front(),b.path.back())<=lengthB*1e-8;
    if(closedA!=closedB||std::min(lengthA,lengthB)<=1e-12||std::min(lengthA,lengthB)<.5*std::max(lengthA,lengthB))
        return std::numeric_limits<double>::infinity();
    const auto aLengths=lengths(a.path),bLengths=lengths(b.path);
    const double score=std::min(orientationCost(a.path,aLengths,b.path,bLengths,false),
                                orientationCost(a.path,aLengths,b.path,bLengths,true));
    return score<=.75*std::max(lengthA,lengthB)?score:std::numeric_limits<double>::infinity();
}
bool align(Shape& a,Shape& b){
    if(!a.cubics.isEmpty()||!b.cubics.isEmpty())return !a.cubics.isEmpty()&&a.cubics.size()==b.cubics.size();
    if(!std::isfinite(correspondence(a,b)))return false;
    if(a.path.size()+b.path.size()>4096)return false;
    auto aLengths=lengths(a.path);const auto bLengths=lengths(b.path);
    if(orientationCost(a.path,aLengths,b.path,bLengths,true)<orientationCost(a.path,aLengths,b.path,bLengths,false)){
        std::reverse(a.path.begin(),a.path.end());
        aLengths=lengths(a.path);
    }
    QVector<double> cuts;
    std::set_union(aLengths.begin(),aLengths.end(),bLengths.begin(),bLengths.end(),std::back_inserter(cuts));
    cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
    QVector<Vec3> from,to;
    from.reserve(cuts.size());to.reserve(cuts.size());
    for(double t:cuts){from.append(at(a.path,aLengths,t));to.append(at(b.path,bLengths,t));}
    a.path=std::move(from);b.path=std::move(to);
    return true;
}
Shapes shapes(const ModelSnapshot& model){
    Shapes result;
    for(const auto& section:model.sections){
        if(section.kind<1||section.kind>4)continue;
        for(const auto& path:section.paths)if(path.size()>=2)
            result[{section.kind,EntityId(section.index)}].append({path,{},QColor::fromRgb(section.color&255,(section.color>>8)&255,(section.color>>16)&255)});
    }
    for(bool flow:{false,true})for(const auto& curve:flow?model.flowlines:model.controlCurves){
        auto cubics=presentation::splineCubics(curve.spline);
        if(cubics.isEmpty())continue;
        const auto color=QColor::fromRgb(curve.color&255,(curve.color>>8)&255,(curve.color>>16)&255);
        result[{flow?-1:0,curve.id}].append({{},std::move(cubics),color});
    }
    return result;
}
}

bool onlyPointPositionsChanged(const ModelSnapshot& before,const ModelSnapshot& after){
    if(before.documentId!=after.documentId||before.units!=after.units||before.revision==after.revision||
        before.points.size()!=after.points.size()||before.edges.size()!=after.edges.size()||
        before.faces.size()!=after.faces.size())return false;
    bool moved=false;
    for(qsizetype i=0;i<before.points.size();++i){
        if(before.points[i].id!=after.points[i].id)return false;
        moved|=!equal(before.points[i].position,after.points[i].position);
    }
    if(!moved)return false;
    for(qsizetype i=0;i<before.edges.size();++i){
        const auto& a=before.edges[i];const auto& b=after.edges[i];
        if(a.id!=b.id||a.a!=b.a||a.b!=b.b)return false;
    }
    for(qsizetype i=0;i<before.faces.size();++i){
        const auto& a=before.faces[i];const auto& b=after.faces[i];
        if(a.id!=b.id||a.points!=b.points)return false;
    }
    return moved;
}

bool affects(const AuxiliaryCurve& curve,const PointPositions& points){
    return std::any_of(curve.controls.begin(),curve.controls.end(),[&](EntityId id){return points.contains(id);});
}

QVector<presentation::Cubic> preview(const AuxiliaryCurve& curve,const ModelSnapshot& model,
    const PointPositions& points,const QVector<presentation::Cubic>& basis){
    if(basis.isEmpty()||curve.controls.size()<2||!affects(curve,points))return basis;
    QVector<Vec3> controls,offsets;
    for(const auto id:curve.controls){
        const auto* point=model.point(id);
        if(!point)return basis;
        controls.append(point->position);
        offsets.append(difference(points.value(id,point->position),point->position));
    }
    const auto axes=lengths(controls);
    if(axes.back()==0)return basis;
    QVector<Vec3> knots{basis.front().front()};
    for(const auto& cubic:basis)knots.append(cubic.back());
    const auto progress=lengths(knots);
    auto result=basis;
    for(qsizetype i=0;i<result.size();++i)
        for(size_t j=0;j<4;++j)
            result[i][j]=sum(result[i][j],at(offsets,axes,mix(progress[i],progress[i+1],double(j)/3)));
    return result;
}

qint64 Motion::now(){
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
Motion::Motion(){
    timer_.setTimerType(Qt::PreciseTimer);
    timer_.setInterval(16);
    QObject::connect(&timer_,&QTimer::timeout,&timer_,[this]{
        elapsed_=std::min(now()-start_,qint64(DurationMs));
        if(elapsed_==DurationMs)timer_.stop();
    });
}
void Motion::start(){elapsed_=0;start_=now();timer_.start();}
bool Motion::active() const{return start_>=0&&elapsed_<DurationMs;}
bool Motion::contains(Key key) const{return tracks_.find(key)!=tracks_.end();}

QMetaObject::Connection Motion::onFrame(QObject* owner,std::function<void()> repaint){
    return QObject::connect(&timer_,&QTimer::timeout,owner,std::move(repaint));
}

static double eased(qint64 elapsed){
    const double remaining=1.-double(elapsed)/Motion::DurationMs;
    return 1.-remaining*remaining*remaining;
}
double Motion::progress()const{return eased(elapsed_);}
Shape Motion::sample(const Track& track,double t){
    if(t<=0)return track.from;
    if(t>=1)return track.to;
    auto shape=track.from;
    shape.opacity=mix(track.from.opacity,track.to.opacity,t);
    if(track.morph){
        for(qsizetype i=0;i<shape.path.size();++i)shape.path[i]=mix(shape.path[i],track.to.path[i],t);
        for(qsizetype i=0;i<shape.cubics.size();++i)
            for(size_t j=0;j<4;++j)shape.cubics[i][j]=mix(shape.cubics[i][j],track.to.cubics[i][j],t);
    }
    return shape;
}
const Shapes& Motion::frame()const{return frameAt(elapsed_);}
const Shapes& Motion::frameAt(qint64 elapsed)const{
    elapsed=std::clamp(elapsed,qint64(0),qint64(DurationMs));
    if(elapsed==sampled_)return frame_;
    sampled_=elapsed;
    const double t=eased(elapsed);
    frame_.clear();
    for(const auto& [key,tracks]:tracks_){
        auto& paths=frame_[key];
        for(const auto& track:tracks){
            auto shape=sample(track,t);
            if(shape.opacity>0)paths.append(std::move(shape));
        }
    }
    return frame_;
}

std::shared_ptr<Motion> Motion::between(const ModelSnapshot& before,const ModelSnapshot& after,
    const PointPositions& pending,const std::shared_ptr<Motion>& previous){
    auto from=shapes(before),to=shapes(after);
    if(previous&&previous->active())
        for(const auto& [key,frame]:previous->frame())from[key]=frame;
    for(const auto& curve:before.controlCurves)if(affects(curve,pending))
        for(auto& shape:from[{0,curve.id}])shape.cubics=preview(curve,before,pending,shape.cubics);
    auto motion=std::make_shared<Motion>();
    std::set<Key> keys;
    for(const auto& [key,_]:from)keys.insert(key);
    for(const auto& [key,_]:to)keys.insert(key);
    for(const auto& key:keys){
        auto& old=from[key];auto& next=to[key];
        if(old.size()==next.size()&&std::equal(old.begin(),old.end(),next.begin(),[](const auto& a,const auto& b){
            return a.color==b.color&&a.opacity==b.opacity&&equalShape(a,b);
        }))continue;
        auto& tracks=motion->tracks_[key];
        const auto leave=[&](const Shape& shape){auto faded=shape;faded.opacity=0;tracks.append({shape,std::move(faded)});};
        const auto enter=[&](const Shape& shape){auto faded=shape;faded.opacity=0;tracks.append({std::move(faded),shape});};
        // Exact siblings survive splits and merges. Changed paths in such a
        // group fade independently rather than being joined to the wrong branch.
        const bool sameCount=old.size()==next.size()&&old.size()<=8;
        QVector<bool> used(next.size(),false);
        for(auto a:old){
            qsizetype match=-1;
            for(qsizetype j=0;j<next.size();++j)
                if(!used[j]&&equalShape(a,next[j])){match=j;break;}
            if(match<0&&sameCount){
                double first=std::numeric_limits<double>::infinity(),second=first;
                for(qsizetype j=0;j<next.size();++j)if(!used[j]){
                    const double cost=key.kind<=0&&old.size()==1?0.:correspondence(a,next[j]);
                    if(cost<first){second=first;first=cost;match=j;}
                    else second=std::min(second,cost);
                }
                if(second<=first*1.25)match=-1;
            }
            if(match>=0){
                auto b=next[match];
                const bool unchanged=equalShape(a,b);
                if(a.color==b.color&&(unchanged||align(a,b))){
                    used[match]=true;tracks.append({std::move(a),std::move(b),!unchanged});continue;
                }
            }
            leave(a);
        }
        for(qsizetype i=0;i<next.size();++i)if(!used[i])enter(next[i]);
    }
    if(motion->tracks_.empty())return {};
    return motion;
}
}
