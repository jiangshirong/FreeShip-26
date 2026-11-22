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
bool sameReferences(const presentation::HorizontalReferences& a,const presentation::HorizontalReferences& b){
    return equal(a.minimum,b.minimum)&&a.maximum.x==b.maximum.x&&a.maximum.y==b.maximum.y&&a.waterlineZ==b.waterlineZ;
}
presentation::HorizontalReferences references(const ModelSnapshot& model){return {model.minimum,model.maximum,model.waterlineZ};}
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
bool isClosed(const QVector<Vec3>& path,double length){
    return distance(path.front(),path.back())<=length*1e-8;
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
    if(std::min(lengthA,lengthB)<=1e-12||isClosed(a.path,lengthA)!=isClosed(b.path,lengthB))
        return std::numeric_limits<double>::infinity();
    const auto aLengths=lengths(a.path),bLengths=lengths(b.path);
    return std::min(orientationCost(a.path,aLengths,b.path,bLengths,false),
                    orientationCost(a.path,aLengths,b.path,bLengths,true));
}
bool matchingSegments(const QVector<Vec3>& a,const QVector<Vec3>& b){
    if(a.size()!=b.size())return false;
    for(qsizetype i=1;i<a.size();++i){
        const auto u=difference(a[i],a[i-1]),v=difference(b[i],b[i-1]);
        const auto limit=.95*std::hypot(u.x,u.y,u.z)*std::hypot(v.x,v.y,v.z);
        if(limit<=1e-12||u.x*v.x+u.y*v.y+u.z*v.z<limit)return false;
    }
    return true;
}
bool align(Shape& a,Shape& b,bool preserveCorners=false){
    if(!a.cubics.isEmpty()||!b.cubics.isEmpty())return !a.cubics.isEmpty()&&a.cubics.size()==b.cubics.size();
    const double lengthA=pathLength(a.path),lengthB=pathLength(b.path);
    if(std::min(lengthA,lengthB)<=1e-12||isClosed(a.path,lengthA)!=isClosed(b.path,lengthB))return false;
    auto aLengths=lengths(a.path);const auto bLengths=lengths(b.path);
    if(orientationCost(a.path,aLengths,b.path,bLengths,true)<orientationCost(a.path,aLengths,b.path,bLengths,false)){
        std::reverse(a.path.begin(),a.path.end());
        aLengths=lengths(a.path);
    }
    if(preserveCorners&&matchingSegments(a.path,b.path))return true;
    QVector<double> cuts;
    std::set_union(aLengths.begin(),aLengths.end(),bLengths.begin(),bLengths.end(),std::back_inserter(cuts));
    cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
    QVector<Vec3> from,to;
    from.reserve(cuts.size());to.reserve(cuts.size());
    auto resample=[&cuts](const QVector<Vec3>& path,const QVector<double>& positions,QVector<Vec3>& result){
        qsizetype end=1;
        for(double t:cuts){
            while(end<positions.size()&&positions[end]<=t)++end;
            if(end==positions.size())result.append(path.back());
            else result.append(mix(path[end-1],path[end],(t-positions[end-1])/(positions[end]-positions[end-1])));
        }
    };
    resample(a.path,aLengths,from);resample(b.path,bLengths,to);
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

bool horizontalReferencesChanged(const ModelSnapshot& before,const ModelSnapshot& after){
    return before.documentId==after.documentId&&before.units==after.units&&
        before.overlay.present&&after.overlay.present&&!sameReferences(references(before),references(after));
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
std::optional<presentation::HorizontalReferences> Motion::referenceFrame(double t)const{
    if(!references_)return {};
    const auto& [from,to]=*references_;
    if(t<=0)return from;
    if(t>=1)return to;
    return presentation::HorizontalReferences{mix(from.minimum,to.minimum,t),mix(from.maximum,to.maximum,t),mix(from.waterlineZ,to.waterlineZ,t)};
}
std::optional<Motion::Track> Motion::pathTrack(QVector<Vec3> from,QVector<Vec3> to){
    if(from.size()<2||to.size()<2)return {};
    Track track{{std::move(from),{},{},1.},{std::move(to),{},{},1.},true};
    if(!align(track.from,track.to,true))return {};
    return track;
}
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
    const PointPositions& pending,const std::shared_ptr<Motion>& previous,bool includeCurves){
    auto motion=std::make_shared<Motion>();
    if(before.overlay.present&&after.overlay.present){
        auto from=references(before);
        const auto to=references(after);
        if(previous&&previous->active())from=previous->referenceFrame(previous->progress()).value_or(from);
        if(!sameReferences(from,to))motion->references_=ReferenceTrack{from,to};
    }
    if(!includeCurves)return motion->references_?motion:nullptr;
    auto from=shapes(before),to=shapes(after);
    Shapes exiting;
    if(previous&&previous->active()){
        const double progress=previous->progress();
        for(const auto& [key,tracks]:previous->tracks()){
            auto& current=from[key];current.clear();
            for(const auto& track:tracks){
                auto shape=sample(track,progress);
                if(track.to.opacity>0)current.append(std::move(shape));
                else if(shape.opacity>0)exiting[key].append(std::move(shape));
            }
        }
    }
    for(const auto& curve:before.controlCurves)if(affects(curve,pending))
        for(auto& shape:from[{0,curve.id}])shape.cubics=preview(curve,before,pending,shape.cubics);
    std::set<Key> keys;
    for(const auto& [key,_]:from)keys.insert(key);
    for(const auto& [key,_]:to)keys.insert(key);
    for(const auto& key:keys){
        auto& old=from[key];auto& next=to[key];
        const auto leaving=exiting.find(key);
        if(leaving==exiting.end()&&old.size()==next.size()&&std::equal(old.begin(),old.end(),next.begin(),[](const auto& a,const auto& b){
            return a.color==b.color&&a.opacity==b.opacity&&equalShape(a,b);
        }))continue;
        auto& tracks=motion->tracks_[key];
        const auto leave=[&](const Shape& shape){auto faded=shape;faded.opacity=0;tracks.append({shape,std::move(faded)});};
        const auto enter=[&](const Shape& shape){auto faded=shape;faded.opacity=0;tracks.append({std::move(faded),shape});};
        const bool sameCount=old.size()==next.size();
        QVector<bool> used(next.size(),false);
        QVector<qsizetype> matches(old.size(),-1);
        qsizetype unmatched=0;
        // Pair unchanged siblings first. A sole changed branch on the same
        // section has a known counterpart even when it moves a long way.
        for(qsizetype i=0;i<old.size();++i){
            for(qsizetype j=0;j<next.size();++j)
                if(!used[j]&&old[i].color==next[j].color&&equalShape(old[i],next[j])){matches[i]=j;used[j]=true;break;}
            if(matches[i]<0)++unmatched;
        }
        if(leaving!=exiting.end())for(const auto& shape:leaving->second)leave(shape);
        for(qsizetype i=0;i<old.size();++i){
            auto a=old[i];
            auto match=matches[i];
            if(match<0&&sameCount){
                if(unmatched==1)match=std::find(used.begin(),used.end(),false)-used.begin();
                else if(unmatched<=8){
                    double first=std::numeric_limits<double>::infinity(),second=first;
                    for(qsizetype j=0;j<next.size();++j)if(!used[j]){
                        const double cost=correspondence(a,next[j]);
                        if(cost<first){second=first;first=cost;match=j;}
                        else second=std::min(second,cost);
                    }
                    if(second<=first*1.25)match=-1;
                }
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
    if(motion->tracks_.empty()&&!motion->references_)return {};
    return motion;
}
}
