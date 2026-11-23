// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native/scene_builder.h"
#include "native/local_geometry.h"
#include "native/sections.h"
#include "native/linesplan.h"
#include "render/drag_section_compare.h"
#include <QFile>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QPromise>
#include <QThreadPool>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

// One in-flight calculation plus the newest pointer position. The original cut
// is immutable across a gesture; no model, undo entry or export is modified.
class MoveSectionPreview final : public QObject {
    using Model=std::shared_ptr<const fs::ModelSnapshot>;
    using Sections=std::shared_ptr<const QVector<fs::Section>>;
    using Native=fs::native_geometry::DocumentInput;
    struct Trace {size_t curve;fs::EntityId a,b;bool symmetric;size_t segment;bool focus;fs::SectionSpline before;QVector<fs::presentation::Cubic> basis;fs::SectionSpline last;QVector<fs::presentation::Cubic> lastCubics;};
    struct Cache { QVector<Trace> traces; Native source;std::unique_ptr<fs::native_geometry::LocalGeometry> geometry; };
    std::shared_ptr<Cache> gestureCache;
    struct Input {QString path;Model model;QVector<fs::PointMove> moves;quint64 generation=0;int kind=0;double coordinate=0;bool full=false;Sections baseline;std::shared_ptr<Cache> cache;QSet<fs::EntityId> seeds;};
    struct Result {Sections display,baseline;DragSections pairs;DragShell shell;};
    QTimer delay;QFutureWatcher<Result> watcher;Input latest{},active{};bool running=false,queued=false;quint64 epoch=0;
    Sections original;int originalKind=0;double originalCoordinate=0;
    std::function<void(Model,Sections,DragSections,DragShell)> show;
    static QVector<fs::Section> selected(const Input& req){
        QVector<fs::Section> sections;if(!req.kind)return sections;
        for(const auto& s:req.model->sections)if(s.kind==req.kind&&!s.paths.isEmpty())sections.append(s);
        std::stable_sort(sections.begin(),sections.end(),[&](const auto& a,const auto& b){return std::abs(a.distance-req.coordinate)<std::abs(b.distance-req.coordinate);});
        if(sections.size()>2)sections.resize(2);
        return sections;
    }
    static void narrow(Native& input,const QVector<fs::Section>& sections){
        input.sections.clear();for(const auto& s:sections){const auto& p=s.plane;input.sections.push_back({s.kind,s.index,fs::native_geometry::Plane(float(p[0]),float(p[1]),float(p[2]),float(p[3]))});}
    }
    static QVector<fs::Section> slice(const Native& input,QVector<fs::Section> specs,Cache& cache){
        // Gesture and painter share a process. Use the same intersection, simplification
        // and sampling rules as the document service without a JSON round trip per cut.
        using namespace fs::native_geometry;
        cache.geometry->updatePreviewPoints(input.geometry);
        SectionIndex index(input.geometry,cache.geometry->mesh());size_t samples=0,controls=0;
        for(auto& spec:specs){spec.paths.clear();spec.splines.clear();const auto& p=spec.plane;
            auto result=index.intersect(Plane(p[0],p[1],p[2],p[3]));const bool multiple=result.connectedCurves.size()>1;
            for(auto& raw:result.curves){Spline line(std::move(raw.points),std::move(raw.knuckles),raw.fragments);
                if(input.simplify&&multiple)line.simplify(2);
                line.setFragments((256u<<input.geometry.precision)+1u);const auto path=samplePolyline(line,true);
                if(path.size()<2)continue;
                samples+=path.size();controls+=line.points().size();if(samples>4000000||controls>4000000)throw std::length_error("Section preview exceeds limit");
                QVector<fs::Vec3> pts;pts.reserve(path.size());for(auto v:path)pts.append({v.x,v.y,v.z});spec.paths.append(std::move(pts));
                fs::SectionSpline spline;spline.points.reserve(line.points().size());spline.knuckles.reserve(line.knuckles().size());
                for(auto v:line.points())spline.points.append({v.x,v.y,v.z});for(bool k:line.knuckles())spline.knuckles.append(k);spec.splines.append(std::move(spline));}}
        return specs;
    }
    // Tag surface edges in the gesture-local evaluator. Subdivision propagates these
    // tags to the actual face vertices; the document and its curve list stay untouched.
    static void trackShell(Native& input,const Input& req,Cache& cache){
        auto& g=input.geometry;using Key=std::pair<size_t,size_t>;
        std::map<Key,bool> visible;
        for(const auto& face:g.faces){const auto& layer=g.layers[face.layer];if(!layer.surfaceVisible||face.points.empty())continue;
            auto a=face.points.back();for(auto b:face.points){visible[std::minmax(a,b)]|=layer.symmetric;a=b;}}
        // An authored curve can own several consecutive surface edges. Reuse the
        // matching knot span rather than retagging (and breaking) its ownership.
        std::map<Key,size_t> spans;
        for(const auto& c:g.curves)for(size_t j=1;j<c.points.size();++j)spans[std::minmax(c.points[j-1],c.points[j])]=j-1;
        cache.traces.reserve(qsizetype(visible.size()));
        auto add=[&](size_t i,bool focus){auto& e=g.edges[i];const auto [a,b]=e.points;const auto key=std::minmax(a,b);
            size_t segment=0;if(e.curve<0){e.curve=int(g.curves.size());g.curves.push_back({{a,b},true});}
            else segment=spans.at(key);
            cache.traces.append({size_t(e.curve),req.model->points[a].id,req.model->points[b].id,visible.at(key),segment,focus,{}});};
        for(size_t i=0;i<g.edges.size();++i){const auto [a,b]=g.edges[i].points;if(!visible.count(std::minmax(a,b)))continue;
            add(i,req.seeds.contains(req.model->points[a].id)||req.seeds.contains(req.model->points[b].id));}

    }
    static fs::SectionSpline traceShape(const fs::native_geometry::SubdivisionLevel& mesh,const Trace& trace){
        fs::SectionSpline line;const auto& curve=mesh.curves.at(trace.curve);
        const size_t span=size_t(1)<<mesh.level,first=trace.segment*span;
        for(size_t i=first;i<=first+span;++i){auto p=mesh.points.at(curve.points.at(i)).position;line.points.append({p.x,p.y,p.z});line.knuckles.append(curve.knuckles.at(i));}
        return line;
    }
    static Result cut(const Input &req){
        try {
            auto& cache=*req.cache;const auto& model=*req.model;
            if(!cache.geometry){QFile file(req.path);if(!file.open(QIODevice::ReadOnly))return {};
                auto input=Native::read(QJsonDocument::fromJson(file.readAll()).object());
                if(input.documentId!=model.documentId||input.revision!=model.revision||input.geometry.points.size()!=size_t(model.points.size()))return {};
                for(qsizetype i=0;i<model.points.size();++i){const auto p=model.points[i].position;const auto src=input.geometry.points[i].position;
                    if(src.x!=float(p.x)||src.y!=float(p.y)||src.z!=float(p.z))return {};}
                input.surfaceDisplay=false;if(req.kind)trackShell(input,req,cache);cache.geometry=std::make_unique<fs::native_geometry::LocalGeometry>(input.geometry);cache.source=std::move(input);
                for(auto& t:cache.traces){t.last=traceShape(cache.geometry->mesh(),t);t.lastCubics=fs::presentation::splineCubics(t.last);
                    if(t.focus){t.before=t.last;t.basis=t.lastCubics;}}
            }
            auto& input=cache.source;if(input.documentId!=model.documentId||input.revision!=model.revision)return {};auto& geom=input.geometry;
            // Soft editing can both add and remove followers during one gesture.
            for(qsizetype i=0;i<model.points.size();++i){auto p=model.points[i].position;geom.points[i].position={float(p.x),float(p.y),float(p.z)};}
            auto originals=req.baseline?*req.baseline:selected(req);
            auto specs=req.full?model.sections:originals;
            // On a bare control net there are no saved sections to compare. Cut a
            // temporary plane near the picked point, just inside extreme hull bounds.
            bool temporary=req.kind&&!std::any_of(model.sections.begin(),model.sections.end(),[&](const auto& s){return s.kind==req.kind&&!s.paths.isEmpty();});
            if(temporary){
                if(!req.baseline){
                    // A control point can sit *outside* the subdivided shell. Only a face
                    // that participates in intersections can supply a useful cut.
                    cache.geometry->updatePreviewPoints(geom);
                    const auto& mesh=cache.geometry->mesh();
                    double lo=std::numeric_limits<double>::infinity(),hi=-lo;
                    for(const auto& face:mesh.faces){if(!mesh.layers[face.layer].useForIntersections)continue;
                        for(const auto index:face.points){const auto p=mesh.points[index].position;
                            const double d=req.kind==1?p.x:req.kind==2?p.y:p.z;lo=std::min(lo,d);hi=std::max(hi,d);}}
                    if(lo<hi){
                    fs::Section probe;probe.kind=req.kind;probe.index=9999;
                    while(probe.index>=0&&std::any_of(model.sections.begin(),model.sections.end(),[&](const auto& s){return s.kind==req.kind&&s.index==probe.index;}))--probe.index;
                    if(probe.index>=0){
                    probe.distance=float(std::clamp(req.coordinate,lo+(hi-lo)*.025,hi-(hi-lo)*.025));probe.plane[req.kind-1]=1;probe.plane[3]=-probe.distance;
                    const auto saved=input.sections;narrow(input,{probe});originals=slice(input,{probe},cache);input.sections=saved;}}
                }
                if(req.full)specs+=originals;else specs=originals;
            }
            if(specs.isEmpty()&&cache.traces.isEmpty())return {};
            narrow(input,specs);
            QHash<fs::EntityId,fs::Vec3> changes;for(auto m:req.moves)changes.insert(m.id,m.position);
            for(qsizetype i=0;i<model.points.size();++i){const auto &p=model.points[i];auto found=changes.constFind(p.id);if(found==changes.cend())continue;
                auto &src=geom.points[i].position;if(src.x!=float(p.position.x)||src.y!=float(p.position.y)||src.z!=float(p.position.z))return {};
                auto v=*found;src={float(v.x),float(v.y),float(v.z)};}
            QVector<fs::Section> current;
            if(specs.isEmpty())cache.geometry->updatePreviewPoints(geom);else current=slice(input,std::move(specs),cache);
            auto shell=std::make_shared<QVector<DragShellEdge>>();shell->reserve(cache.traces.size());
            const auto& mesh=cache.geometry->mesh();
            for(auto& t:cache.traces){
                // Reuse splines (Qt shared storage) only when every evaluated knot is identical.
                const auto& indices=mesh.curves[t.curve].points;const size_t first=t.segment*(size_t(1)<<mesh.level);
                const auto& knots=std::as_const(t.last.points);
                for(qsizetype j=0;j<knots.size();++j){auto p=mesh.points[indices[first+j]].position;const auto& q=knots[j];
                    if(p.x!=q.x||p.y!=q.y||p.z!=q.z){t.last=traceShape(mesh,t);t.lastCubics=fs::presentation::splineCubics(t.last);break;}}
                shell->append({t.a,t.b,t.symmetric,t.before,t.last,!t.focus,t.basis,t.lastCubics});}

            auto pairs=std::make_shared<QVector<DragSectionPair>>();
            for(const auto& before:originals)for(const auto& after:current)if(after.kind==before.kind&&after.index==before.index){pairs->append({before,after});break;}
            if(temporary&&req.full&&!originals.isEmpty())current.removeLast(); // synthetic geometry is overlay-only
            if(!req.full){auto display=model.sections;for(auto& s:display)for(const auto& after:current)if(s.kind==after.kind&&s.index==after.index){s=after;break;}current=std::move(display);}
            return {std::make_shared<const QVector<fs::Section>>(std::move(current)),req.baseline?req.baseline:std::make_shared<const QVector<fs::Section>>(std::move(originals)),pairs,shell};
        }catch(const std::exception&){return {};}
    }
    void start(){if(running||!queued)return;active=latest;queued=false;running=true;
        if(active.kind==originalKind&&active.coordinate==originalCoordinate)active.baseline=original;
        auto promise=std::make_shared<QPromise<Result>>();promise->start();watcher.setFuture(promise->future());
        QThreadPool::globalInstance()->start([promise,req=active]{promise->addResult(cut(req));promise->finish();});}
public:
    MoveSectionPreview(QObject *parent,std::function<void(Model,Sections,DragSections,DragShell)> publish):QObject(parent),show(std::move(publish)){
        delay.setSingleShot(true);delay.setInterval(0);connect(&delay,&QTimer::timeout,this,[this]{start();});
        connect(&watcher,&QFutureWatcher<Result>::finished,this,[this]{running=false;
            if(active.generation==epoch){auto result=watcher.result();if(result.display){original=result.baseline;originalKind=active.kind;originalCoordinate=active.coordinate;show(active.model,result.display,result.pairs,result.shell);}}active={};start();});}
    void hold(){++epoch;queued=false;latest={};original.reset();gestureCache.reset();delay.stop();}
    void request(QString path,Model model,const QVector<fs::PointMove>& moves,int kind,double coordinate,bool full,const QSet<fs::EntityId>& seeds){
        if(moves.isEmpty()||!model||(!kind&&(!full||model->sections.isEmpty()))){hold();show({},{},{},{});return;}
        if(!gestureCache)gestureCache=std::make_shared<Cache>();
        latest={std::move(path),std::move(model),moves,epoch,kind,coordinate,full,{},gestureCache,seeds};queued=true;if(!running&&!delay.isActive())delay.start();}
};
