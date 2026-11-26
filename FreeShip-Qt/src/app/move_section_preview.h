// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native/scene_builder.h"
#include "native/local_geometry.h"
#include "native/sections.h"
#include "native/linesplan.h"
#include "render/hull_view.h"
#include <QElapsedTimer>
#include <QFile>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPromise>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <future>
#include <limits>
#include <map>

// One in-flight calculation plus the newest pointer position. The original cut
// is immutable across a gesture; no model, undo entry or export is modified.
class MoveSectionPreview final : public QObject {
    using Model=std::shared_ptr<const fs::ModelSnapshot>;
    using Sections=std::shared_ptr<const QVector<fs::Section>>;
    using Native=fs::native_geometry::DocumentInput;
    using Cancel=std::shared_ptr<std::atomic_bool>;
    struct Source {QString path;Model model;bool operator==(const Source& other)const{return path==other.path&&model==other.model;}};
    struct Trace {size_t curve;fs::EntityId a,b;bool symmetric;size_t segment;fs::SectionSpline rest,last;QVector<fs::presentation::Cubic> restCubics,lastCubics;};
    struct Cache { QVector<Trace> traces; Native source;std::unique_ptr<fs::native_geometry::LocalGeometry> geometry; };
    std::shared_ptr<Cache> gestureCache;
    Cancel gestureCancelled;
public:
    using RasterLayers=std::array<std::shared_ptr<const HullView::DragShellRaster>,4>;
    using RasterJobs=std::array<HullView::DragRasterJob,4>;
private:
    struct Input {Source source;QVector<fs::PointMove> moves;quint64 generation=0;int kind=0;double coordinate=0;bool full=false;Sections baseline;std::shared_ptr<Cache> cache;QSet<fs::EntityId> seeds;bool profile=false;RasterJobs rasterJobs;Cancel cancelled;RasterLayers originalRaster;};
    struct Result {Sections display,baseline;DragSections pairs;DragShell shell;QJsonObject timings;RasterLayers raster;};
    QTimer delay;QFutureWatcher<Result> watcher;Input latest{},active{};bool running=false,queued=false;quint64 epoch=0;
    struct Warmed { bool ok=false; double ms=0; };
    QTimer warmDelay;
    QFutureWatcher<Warmed> warmWatcher;
    Source current,warming;
    std::shared_ptr<Cache> reusable,preparing;
    Cancel warmCancelled;
    bool warmWanted=false;
    QString warmPhase;
    double warmMs=0;
    QTimer originalDelay;
    QFutureWatcher<RasterLayers> originalWatcher;
    Cancel originalCancelled;
    QSet<fs::EntityId> originalSelection;
    RasterLayers preparedOriginal;
    bool originalWanted=false,originalDirty=true;
    QString originalPhase;
    Sections original;int originalKind=0;double originalCoordinate=0;
    using Publish=std::function<void(Model,Sections,DragSections,DragShell,const QJsonObject&,const RasterLayers&)>;
    Publish show;
    std::function<RasterJobs(bool)> prepareRaster;
    static RasterLayers rasterize(const RasterJobs& jobs,const DragShell& shell,const RasterLayers& original){
        using Layer=RasterLayers::value_type;
        struct RasterPool:QThreadPool{RasterPool(){setMaxThreadCount(std::clamp(QThread::idealThreadCount()-2,1,3));}};
        static RasterPool pool;
        RasterLayers layers;
        std::array<std::future<Layer>,4> futures;
        // The large projected views get the helper threads; the section worker
        // can draw the shorter profile. Do not wait on the global worker pool.
        for(int i=int(jobs.size())-1;i>=0;--i){
            if(!jobs[i])continue;
            auto task=std::make_shared<std::packaged_task<Layer()>>([job=jobs[i],shell,before=original[i]]{return job(shell,before,nullptr);});
            futures[i]=task->get_future();
            if(i==0||QThread::idealThreadCount()<=2||!pool.tryStart([task]{(*task)();}))(*task)();
        }
        for(size_t i=0;i<futures.size();++i)if(futures[i].valid()){
            // Missing raster only takes this view through the ordinary painter.
            try{layers[i]=futures[i].get();}catch(const std::exception&){}
        }
        return layers;
    }
    static QVector<fs::Section> selected(const Input& req){
        QVector<fs::Section> sections;if(!req.kind)return sections;
        for(const auto& s:req.source.model->sections)if(s.kind==req.kind&&!s.paths.isEmpty())sections.append(s);
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
    static void trackShell(Native& input,const fs::ModelSnapshot& model,Cache& cache){
        auto& g=input.geometry;using Key=std::pair<size_t,size_t>;
        std::map<Key,bool> visible;
        for(const auto& face:g.faces){const auto& layer=g.layers[face.layer];if(!layer.surfaceVisible||face.points.empty())continue;
            auto a=face.points.back();for(auto b:face.points){visible[std::minmax(a,b)]|=layer.symmetric;a=b;}}
        // An authored curve can own several consecutive surface edges. Reuse the
        // matching knot span rather than retagging (and breaking) its ownership.
        std::map<Key,size_t> spans;
        for(const auto& c:g.curves)for(size_t j=1;j<c.points.size();++j)spans[std::minmax(c.points[j-1],c.points[j])]=j-1;
        cache.traces.reserve(qsizetype(visible.size()));
        auto add=[&](size_t i){auto& e=g.edges[i];const auto [a,b]=e.points;const auto key=std::minmax(a,b);
            size_t segment=0;if(e.curve<0){e.curve=int(g.curves.size());g.curves.push_back({{a,b},true});}
            else segment=spans.at(key);
            cache.traces.append({size_t(e.curve),model.points[a].id,model.points[b].id,visible.at(key),segment,{},{},{},{}});};
        for(size_t i=0;i<g.edges.size();++i){const auto [a,b]=g.edges[i].points;if(!visible.count(std::minmax(a,b)))continue;
            add(i);}

    }
    static fs::SectionSpline traceShape(const fs::native_geometry::SubdivisionLevel& mesh,const Trace& trace){
        fs::SectionSpline line;const auto& curve=mesh.curves.at(trace.curve);
        const size_t span=size_t(1)<<mesh.level,first=trace.segment*span;
        for(size_t i=first;i<=first+span;++i){auto p=mesh.points.at(curve.points.at(i)).position;line.points.append({p.x,p.y,p.z});line.knuckles.append(curve.knuckles.at(i));}
        return line;
    }
    static bool isCancelled(const Cancel& token){return token&&token->load(std::memory_order_relaxed);}
    static bool suitable(const Model& model){
        // Budget one useful cache for the active document. Very small boats are
        // cheap to prepare on demand; very large hulls need not retain another mesh.
        return model&&model->points.size()>=256&&model->points.size()<=16000&&model->triangles.size()<=250000&&
            !model->faces.isEmpty()&&!model->isCachedPreview();
    }
    static bool prepare(const Source& key,Cache& destination,const Cancel& cancelled,bool speculative=false){
        if(isCancelled(cancelled))return false;
        QFile file(key.path);if(!file.open(QIODevice::ReadOnly)||(speculative&&file.size()>32*1024*1024))return false;
        auto input=Native::read(QJsonDocument::fromJson(file.readAll()).object());
        const auto& model=*key.model;
        if(input.documentId!=model.documentId||input.revision!=model.revision||input.geometry.points.size()!=size_t(model.points.size()))return false;
        for(qsizetype i=0;i<model.points.size();++i){const auto p=model.points[i].position;const auto src=input.geometry.points[i].position;
            if(src.x!=float(p.x)||src.y!=float(p.y)||src.z!=float(p.z))return false;}
        if(isCancelled(cancelled))return false;
        Cache prepared;
        input.surfaceDisplay=false;trackShell(input,model,prepared);
        prepared.geometry=std::make_unique<fs::native_geometry::LocalGeometry>(input.geometry);
        prepared.source=std::move(input);
        if(isCancelled(cancelled))return false;
        const auto& mesh=prepared.geometry->mesh();
        for(auto& t:prepared.traces){
            t.rest=t.last=traceShape(mesh,t);
            t.restCubics=t.lastCubics=fs::presentation::splineCubics(t.rest);
        }
        if(isCancelled(cancelled))return false;
        destination=std::move(prepared);
        return true;
    }
    static Result cut(const Input &req){
        try {
            if(isCancelled(req.cancelled))return {};
            QJsonObject timings;
            QElapsedTimer timer,total;
            if(req.profile){timer.start();total.start();}
            const auto mark=[&](const char* key){if(req.profile){timings.insert(QLatin1String(key),timer.nsecsElapsed()/1e6);timer.start();}};
            auto& cache=*req.cache;const auto& model=*req.source.model;
            if(req.profile)timings["prepared_geometry"]=bool(cache.geometry);
            if(!cache.geometry&&!prepare(req.source,cache,req.cancelled))return {};
            mark("setup_ms");
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
            mark("prepare_points_ms");
            QVector<fs::Section> current;
            if(specs.isEmpty())cache.geometry->updatePreviewPoints(geom);else current=slice(input,std::move(specs),cache);
            auto shell=std::make_shared<QVector<DragShellEdge>>();shell->reserve(cache.traces.size());
            const auto& mesh=cache.geometry->mesh();
            mark("refine_and_slice_ms");
            for(auto& t:cache.traces){
                // Reuse splines (Qt shared storage) only when every evaluated knot is identical.
                const auto& indices=mesh.curves[t.curve].points;const size_t first=t.segment*(size_t(1)<<mesh.level);
                const auto& knots=std::as_const(t.last.points);
                for(qsizetype j=0;j<knots.size();++j){auto p=mesh.points[indices[first+j]].position;const auto& q=knots[j];
                    if(p.x!=q.x||p.y!=q.y||p.z!=q.z){t.last=traceShape(mesh,t);t.lastCubics=fs::presentation::splineCubics(t.last);break;}}
                const bool focus=req.seeds.contains(t.a)||req.seeds.contains(t.b);
                shell->append({t.a,t.b,t.symmetric,focus?t.rest:fs::SectionSpline{},t.last,!focus,
                               focus?t.restCubics:QVector<fs::presentation::Cubic>{},t.lastCubics});}

            mark("shell_splines_ms");
            auto pairs=std::make_shared<QVector<DragSectionPair>>();
            for(const auto& before:originals)for(const auto& after:current)if(after.kind==before.kind&&after.index==before.index){pairs->append({before,after});break;}
            if(temporary&&req.full&&!originals.isEmpty())current.removeLast(); // synthetic geometry is overlay-only
            if(!req.full){auto display=model.sections;for(auto& s:display)for(const auto& after:current)if(s.kind==after.kind&&s.index==after.index){s=after;break;}current=std::move(display);}
            if(req.profile){timings.insert("worker_ms",total.nsecsElapsed()/1e6);timer.start();}
            auto raster=isCancelled(req.cancelled)?RasterLayers{}:rasterize(req.rasterJobs,shell,req.originalRaster);
            if(req.profile)timings.insert("raster_wall_ms",timer.nsecsElapsed()/1e6);
            return {std::make_shared<const QVector<fs::Section>>(std::move(current)),req.baseline?req.baseline:std::make_shared<const QVector<fs::Section>>(std::move(originals)),pairs,shell,std::move(timings),std::move(raster)};
        }catch(const std::exception&){return {};}
    }
    void start(){
        if(running||!queued||(warming==current&&warmCancelled))return;
        if(!gestureCache){
            gestureCache=reusable?reusable:std::make_shared<Cache>();
            if(suitable(current.model))reusable=gestureCache;
        }
        if(warmDelay.isActive()){warmDelay.stop();warmPhase="on-demand";}
        active=latest;active.cache=gestureCache;queued=false;running=true;
        if(active.kind==originalKind&&active.coordinate==originalCoordinate)active.baseline=original;
        active.rasterJobs=prepareRaster(false);
        if(active.seeds==originalSelection)active.originalRaster=preparedOriginal;
        auto promise=std::make_shared<QPromise<Result>>();promise->start();watcher.setFuture(promise->future());
        QThreadPool::globalInstance()->start([promise,req=active]{promise->addResult(cut(req));promise->finish();});}
    void scheduleWarm(){
        if(warmWanted&&!reusable&&!warmCancelled&&!queued&&!running&&warmPhase.isEmpty()){
            warmPhase="scheduled";warmDelay.start();
        }
    }
    void startWarm(){
        if(!warmWanted||warmCancelled||reusable||queued||running){warmPhase.clear();return;}
        // Keep preparation off the GUI and off the shared interactive pool.
        struct WarmPool:QThreadPool{WarmPool(){setMaxThreadCount(1);setThreadPriority(QThread::LowPriority);}};
        static WarmPool pool;
        warming=current;warmCancelled=std::make_shared<std::atomic_bool>(false);warmPhase="running";
        preparing=std::make_shared<Cache>();
        auto promise=std::make_shared<QPromise<Warmed>>();promise->start();warmWatcher.setFuture(promise->future());
        pool.start([source=warming,cache=preparing,token=warmCancelled,promise]{
            QElapsedTimer timer;timer.start();bool ok=false;
            try{ok=prepare(source,*cache,token,true);}
            catch(const std::exception&){}
            promise->addResult(Warmed{ok,timer.nsecsElapsed()/1e6});promise->finish();
        });
    }
    void scheduleOriginal(){
        if(!originalWanted||!originalDirty||!reusable||running||queued||gestureCancelled||
           originalCancelled||originalSelection.size()<64||!reusable->geometry)return;
        originalPhase="scheduled";originalDelay.start();
    }
    void startOriginal(){
        if(!originalWanted||!originalDirty||!reusable||running||queued||gestureCancelled||originalCancelled||!reusable->geometry)return;
        auto jobs=prepareRaster(true);
        if(std::none_of(jobs.begin(),jobs.end(),[](const auto& job){return bool(job);}))return;
        // Copy only shared, immutable baseline cubics, before the interactive
        // worker can mutate the evaluator. Retain a single selection per window.
        auto shell=std::make_shared<QVector<DragShellEdge>>();
        shell->reserve(reusable->traces.size());
        for(const auto& trace:std::as_const(reusable->traces)){
            const bool focus=originalSelection.contains(trace.a)||originalSelection.contains(trace.b);
            shell->append({trace.a,trace.b,trace.symmetric,{},{},!focus,
                           focus?trace.restCubics:QVector<fs::presentation::Cubic>{},{}});
        }
        struct OriginalPool:QThreadPool{OriginalPool(){setMaxThreadCount(1);setThreadPriority(QThread::LowPriority);}};
        static OriginalPool pool;
        originalCancelled=std::make_shared<std::atomic_bool>(false);
        originalDirty=false;originalPhase="running";
        auto promise=std::make_shared<QPromise<RasterLayers>>();promise->start();originalWatcher.setFuture(promise->future());
        pool.start([jobs=std::move(jobs),shell=std::move(shell),token=originalCancelled,promise]{
            RasterLayers layers;
            try{
                for(size_t i=0;i<jobs.size()&&!isCancelled(token);++i)
                    if(jobs[i])layers[i]=jobs[i](shell,{},token.get());
            }catch(const std::exception&){}
            promise->addResult(std::move(layers));promise->finish();
        });
    }
    void invalidateOriginal(bool clear){
        originalDelay.stop();
        if(originalCancelled)originalCancelled->store(true,std::memory_order_relaxed);
        if(clear)preparedOriginal={};
        originalDirty=true;originalPhase.clear();
    }
public:
    MoveSectionPreview(QObject *parent,Publish publish,std::function<RasterJobs(bool)> capture)
        :QObject(parent),show(std::move(publish)),prepareRaster(std::move(capture)){
        delay.setSingleShot(true);delay.setInterval(0);connect(&delay,&QTimer::timeout,this,[this]{start();});
        warmDelay.setSingleShot(true);warmDelay.setInterval(120);connect(&warmDelay,&QTimer::timeout,this,[this]{startWarm();});
        originalDelay.setSingleShot(true);originalDelay.setInterval(100);
        connect(&originalDelay,&QTimer::timeout,this,[this]{startOriginal();});
        connect(&originalWatcher,&QFutureWatcher<RasterLayers>::finished,this,[this]{
            if(!isCancelled(originalCancelled)){
                preparedOriginal=originalWatcher.result();
                originalPhase=std::any_of(preparedOriginal.begin(),preparedOriginal.end(),[](const auto& layer){return bool(layer);})?"ready":"unavailable";
            }
            originalCancelled.reset();scheduleOriginal();
        });
        connect(&warmWatcher,&QFutureWatcher<Warmed>::finished,this,[this]{
            if(warming==current&&!isCancelled(warmCancelled)){
                const auto result=warmWatcher.result();if(result.ok)reusable=std::move(preparing);warmMs=result.ms;
                warmPhase=reusable?"ready":"unavailable";
            }
            warming={};preparing.reset();warmCancelled.reset();start();scheduleWarm();scheduleOriginal();
        });
        connect(&watcher,&QFutureWatcher<Result>::finished,this,[this]{running=false;
            if(active.generation==epoch){auto result=watcher.result();if(result.display){original=result.baseline;originalKind=active.kind;originalCoordinate=active.coordinate;show(active.source.model,result.display,result.pairs,result.shell,result.timings,result.raster);}}active={};start();scheduleWarm();scheduleOriginal();});}
    ~MoveSectionPreview() override {originalWanted=false;invalidateOriginal(true);hold();if(warmCancelled)warmCancelled->store(true,std::memory_order_relaxed);}
    void setSource(QString path,Model model,bool prepareAhead,bool prepareOriginal=false){
        const Source source{std::move(path),std::move(model)};
        if(source==current)return;
        hold();warmDelay.stop();if(warmCancelled)warmCancelled->store(true,std::memory_order_relaxed);
        invalidateOriginal(true);
        current=source;reusable.reset();warmPhase.clear();warmMs=0;
        warmWanted=prepareAhead&&QThread::idealThreadCount()>=4&&suitable(current.model);
        originalWanted=warmWanted&&prepareOriginal;
        scheduleWarm();
    }
    QJsonObject preparation()const {return {{"phase",warmPhase},{"ms",warmMs},{"original_phase",originalPhase}};}
    void setOriginalSelection(QSet<fs::EntityId> ids){
        if(originalSelection==ids)return;
        originalSelection=std::move(ids);invalidateOriginal(true);scheduleOriginal();
    }
    void originalTargetChanged(){
        invalidateOriginal(false);scheduleOriginal();
    }
    void hold(){
        ++epoch;queued=false;latest={};original.reset();
        if(gestureCancelled)gestureCancelled->store(true,std::memory_order_relaxed);
        gestureCancelled.reset();gestureCache.reset();delay.stop();scheduleOriginal();
    }
    void request(QString path,Model model,const QVector<fs::PointMove>& moves,int kind,double coordinate,bool full,const QSet<fs::EntityId>& seeds,bool profile){
        if(moves.isEmpty()||!model||(!kind&&(!full||model->sections.isEmpty()))){hold();show({},{},{},{},{},{});return;}
        setSource(std::move(path),std::move(model),false);
        // If the gesture gets here first, don't let a scheduled warmup jump the queue.
        if(warmDelay.isActive()){warmDelay.stop();warmPhase="on-demand";}
        if(!gestureCancelled){
            gestureCancelled=std::make_shared<std::atomic_bool>(false);
            if(originalCancelled||originalDelay.isActive())invalidateOriginal(false);
        }
        latest={current,moves,epoch,kind,coordinate,full,{},{},seeds,profile,{},gestureCancelled,{}};queued=true;
        if(!running&&!delay.isActive())delay.start();
    }
};
