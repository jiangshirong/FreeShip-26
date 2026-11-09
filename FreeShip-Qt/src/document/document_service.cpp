// GPL-2.0-or-later. Native document authority; legacy operations are a working
// adapter, never the owner of the committed history or the user's saved file.
#include "fbm_codec.h"
#include "preview_image.h"
#include "point_edit.h"
#include "topology_edit.h"
#include "transform_edit.h"
#include "lackenby_edit.h"
#include "attribute_edit.h"
#include "face_tools.h"
#include "project_edit.h"
#include "definition_edit.h"
#include "auxiliary_edit.h"
#include "background_edit.h"
#include "document_create.h"
#include "analysis_edit.h"
#include "hydrostatics.h"
#include "propulsion.h"
#include "hydrodynamics.h"
#include "added_mass.h"
#include "resistance.h"
#include "plates.h"
#include "cad_patches.h"
#include "publication.h"
#include "preferences.h"
#include "core/model_snapshot.h"
#include "core/recovery_store.h"
#include "core/model_file_save.h"
#include "scene.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QUuid>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace {
using fs::document::Fbm;
constexpr quint64 MaxInteger=9007199254740991ULL;
const QStringList Kinds{"points","edges","faces","layers","curves","flows"};
[[noreturn]] void bad(const QString& text){throw std::runtime_error(text.toUtf8().constData());}
quint64 integer(const QJsonValue& v,quint64 minimum=0,quint64 maximum=MaxInteger){
    auto d=v.toDouble(-1);if(!v.isDouble()||!std::isfinite(d)||d<double(minimum)||d>double(maximum)||std::floor(d)!=d)bad("Invalid document integer");return quint64(d);
}
QString text(const QJsonObject& o,const QString& key){auto v=o[key];auto s=v.toString();if(!v.isString()||s.isEmpty()||s.contains(QChar(0)))bad("Invalid or missing "+key);return s;}
QByteArray read(const QString& path,qint64 limit=Fbm::MaximumBytes){QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()<1||f.size()>limit)bad("Cannot read bounded document file: "+path);auto b=f.readAll();if(f.error()!=QFile::NoError)bad("Incomplete document read");return b;}
QJsonObject json(const QByteArray& bytes){QJsonParseError e;auto d=QJsonDocument::fromJson(bytes,&e);if(e.error!=QJsonParseError::NoError||!d.isObject())bad("Invalid document JSON: "+e.errorString());return d.object();}
QJsonObject requestJson(const QByteArray &bytes) {
    const auto object=json(bytes); // Validate syntax before inspecting key tokens.
    struct Scope { char kind;QSet<QString> keys; };
    std::vector<Scope> scopes;
    for (qsizetype at=0;at<bytes.size();++at) {
        const char c=bytes[at];
        if (c=='{' || c=='[') scopes.push_back({c,{}});
        else if (c=='}' || c==']') scopes.pop_back();
        else if (c=='"') {
            const auto start=at++;
            while (at<bytes.size()) {
                if (bytes[at]=='\\') {at+=2;continue;}
                if (bytes[at]=='"') break;
                ++at;
            }
            auto next=at+1;
            while (next<bytes.size() && (bytes[next]==' ' || bytes[next]=='\t' || bytes[next]=='\r' || bytes[next]=='\n')) ++next;
            if (next<bytes.size() && bytes[next]==':' && !scopes.empty() && scopes.back().kind=='{') {
                const auto key=QJsonDocument::fromJson("["+bytes.mid(start,at-start+1)+"]").array()[0].toString();
                if (scopes.back().keys.contains(key)) bad("Duplicate request field "+key);
                scopes.back().keys.insert(key);
            }
        }
    }
    return object;
}
void write(const QString& path,const QByteArray& bytes){QSaveFile f(path);f.setDirectWriteFallback(false);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())bad("Cannot commit document file: "+path);}
QString output(const QJsonObject& request){auto path=QFileInfo(text(request,"output")).absoluteFilePath();if(!QDir().mkpath(path))bad("Cannot create output directory");return path;}
QString artifact(const QJsonObject& response,const QString& key,const QString& directory){
    QFileInfo info(text(response,key));auto path=info.canonicalFilePath();auto root=QFileInfo(directory).canonicalFilePath();
    if(path.isEmpty()||root.isEmpty()||!info.isFile()||info.isSymLink()||!path.startsWith(root+'/',Qt::CaseInsensitive))bad("Adapter artifact outside transaction: "+key);return path;
}
QString digest(const QByteArray& bytes){return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
struct State {
    std::shared_ptr<const Fbm> saved,engine;
    QJsonObject bindings;
    quint64 token=0;
    quint64 memory()const{return quint64(saved->bytes().size())+(saved==engine?0:engine->bytes().size())+QJsonDocument(bindings).toJson(QJsonDocument::Compact).size()+quint64(engine->controlPoints().size()+(saved==engine?0:saved->controlPoints().size()))*sizeof(fs::document::FbmControlPoint);}
};
class Service {
    std::unique_ptr<fs::RecoveryLease> recovery_;
    QString recoveryRoot_,recoverySource_;
    quint64 recoveryRevision_=0;
    QTemporaryDir work_;
    QProcess adapter_;
    QByteArray buffer_;
    std::vector<State> history_;
    size_t index_=0;
    quint64 revision_=0,nextToken_=0,cleanToken_=0,limit_=64*1024*1024,adapterRequest_=0;
    QString document_;
    QJsonObject highWater_;
    QJsonObject plateContext_;
    std::shared_ptr<const Fbm> preparedSave_,preparedSaveBase_;
    quint64 preparedSaveRevision_=0;
    QJsonObject preparedReceipt_;
    QString confirmedSavePath_;
    void clearSaveReceipt(){
        preparedSave_.reset();preparedSaveBase_.reset();preparedSaveRevision_=0;
        preparedReceipt_={};confirmedSavePath_.clear();
    }
    bool synchronized_=false;
    std::shared_ptr<const Fbm> hydroSource_;
    QJsonObject hydroBindings_;
    std::unique_ptr<fs::document::Hydrostatics> hydro_;
    std::shared_ptr<const Fbm> publicationSource_;
    QJsonObject publicationBindings_;
    std::unique_ptr<fs::document::Publication> publication_;
    std::unique_ptr<fs::ModelSnapshot> validatedPublication_;
    QJsonObject validatedResponse_;
#ifdef Q_OS_WIN
    HANDLE job_=nullptr;
#endif
    const State& current()const{return history_.at(index_);}
    void stop(){synchronized_=false;if(adapter_.state()!=QProcess::NotRunning){adapter_.kill();adapter_.waitForFinished(2000);}buffer_.clear();
#ifdef Q_OS_WIN
        if(job_){CloseHandle(job_);job_=nullptr;}
#endif
    }
    void start(){
        // This service reads stdin synchronously. Poll the process handle before
        // trusting QProcess's event-driven state after an idle adapter crash.
        if(adapter_.state()!=QProcess::NotRunning&&!adapter_.waitForFinished(0))return;
        stop();
        auto path=QCoreApplication::applicationDirPath()+"/bridge/FreeShipGeometryBridge.exe";
        if(!QFileInfo::exists(path))bad("找不到模型运算适配器。");
        adapter_.setProgram(path);adapter_.setArguments({"--serve"});adapter_.setWorkingDirectory(work_.path());
        adapter_.setStandardErrorFile(work_.filePath("adapter.log"),QIODevice::Append);
#ifdef Q_OS_WIN
        adapter_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* a){a->flags|=CREATE_NO_WINDOW;});
        job_=CreateJobObjectW(nullptr,nullptr);if(!job_)bad("Cannot create document worker job");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!SetInformationJobObject(job_,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))bad("Cannot set document job limits");
#endif
        adapter_.start();if(!adapter_.waitForStarted(10000))bad("模型运算适配器启动失败。");
#ifdef Q_OS_WIN
        HANDLE process=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE,FALSE,DWORD(adapter_.processId()));
        bool assigned=process&&AssignProcessToJobObject(job_,process);if(process)CloseHandle(process);if(!assigned){stop();bad("Cannot contain document worker");}
#endif
        synchronized_=false;buffer_.clear();
    }
    QJsonObject call(QJsonObject request){
        start();request["id"]=double(++adapterRequest_);request["external_document"]=true;
        request["native_surface"]=request["native_linesplan"].toBool();
        auto bytes=QJsonDocument(request).toJson(QJsonDocument::Compact)+'\n';
        if(adapter_.write(bytes)!=bytes.size())bad("Cannot submit adapter operation");
        QElapsedTimer timer;timer.start();
        while(timer.elapsed()<110000){
            buffer_+=adapter_.readAllStandardOutput();if(buffer_.size()>1024*1024)bad("Adapter response too large");
            qsizetype end;
            while((end=buffer_.indexOf('\n'))>=0){auto line=buffer_.left(end).trimmed();buffer_.remove(0,end+1);if(!line.startsWith('{'))continue;
                auto response=json(line);if(integer(response["id"])!=adapterRequest_)bad("Mismatched adapter response");
                if(!response["ok"].isBool())bad("Invalid adapter status");
                if(!response["ok"].toBool())bad(response["error"].toString("模型运算失败。"));return response;
            }
            if(adapter_.state()==QProcess::NotRunning)bad("运算适配器已退出，本次操作未提交；可以重试，或保存当前工程。");
            adapter_.waitForReadyRead(100);
        }bad("模型运算超时，本次操作未提交。");
    }
    QJsonObject published(const State& state,const QJsonObject& request,const QString &id,quint64 rev){
        const auto directory=output(request);QElapsedTimer clock;clock.start();
        validatedPublication_.reset();validatedResponse_={};
        const bool reused=publication_ && publicationSource_==state.engine && publicationBindings_==state.bindings;
        if(!reused){
            auto prepared=std::make_unique<fs::document::Publication>(*state.engine,state.bindings);
            publication_=std::move(prepared);publicationSource_=state.engine;publicationBindings_=state.bindings;
        }
        const double prepareMs=clock.nsecsElapsed()/1e6;
        QTemporaryDir stage(directory+"/.pending-XXXXXX");if(!stage.isValid())bad("Cannot create publication transaction");
        QString cache=QDir(directory).absoluteFilePath("../native-lines-cache");
        if(request.contains("lines_cache_root")){
            if(!request["lines_cache_root"].isString())bad("Invalid lines-plan cache directory");
            cache=request["lines_cache_root"].toString();
            if(!cache.isEmpty()&&!QDir::isAbsolutePath(cache))bad("Lines-plan cache directory must be absolute");
        }
        auto result=publication_->generate(stage.path(),cache,id,rev);
        auto validated=validateArtifacts(result,stage.path(),id,rev,true);
        const QStringList keys{"scene","dxf","model","native_report","sections","surface"};
        for(const auto &key:keys){const auto from=result[key].toString(),to=directory+'/'+QFileInfo(from).fileName();write(to,read(from));result[key]=to;}
        write(directory+"/native-input.json",read(stage.filePath("native-input.json")));
        // An old adapter, if still alive for diagnostics, no longer represents
        // the committed revision after a native publication.
        synchronized_=false;
        result["adapter_operation"]="none";result["publication_reused"]=reused;
        result["publication_prepare_ms"]=prepareMs;result["publication_ms"]=clock.nsecsElapsed()/1e6;
        // The transaction has already passed every checksum and parser. Reuse
        // that exact immutable result for the ensuing edit-specific ID checks.
        validatedPublication_=std::make_unique<fs::ModelSnapshot>(std::move(validated));validatedResponse_=result;
        return result;
    }
    QJsonObject snapshot(const QJsonObject& request){
        if(request["native_linesplan"].toBool())return published(current(),request,document_,revision_);
        ensure(request);auto cmd=request;cmd["method"]="snapshot";return call(cmd);
    }
    QJsonObject restored(const State& state,const QJsonObject& request,quint64 rev,bool publish){
        if(publish && request["native_linesplan"].toBool())return published(state,request,document_,rev);
        auto bindings=state.bindings;auto high=bindings["high_water"].toObject();
        for(const auto& k:Kinds)high[k]=double(std::max(integer(high[k]),integer(highWater_[k])));
        bindings["high_water"]=high;
        auto fbm=work_.filePath("restore.fbm"),ids=work_.filePath("restore.json");
        // The old analysis decoder lacks its pre-1.65 gates. Transport a native
        // upgrade only; saved/history bytes retain their own actual version.
        auto transport=fs::document::promoteDocument(*state.engine,std::max(7,state.engine->version()),bindings);
        transport.writeAtomic(fbm);write(ids,QJsonDocument(bindings).toJson(QJsonDocument::Compact));
        QJsonObject cmd{{"method","restore_document"},{"path",fbm},{"bindings",ids},{"document_id",document_},{"revision",double(rev)},
            {"output",publish?text(request,"output"):work_.filePath("restore")},{"publish",publish},{"native_linesplan",request["native_linesplan"]}};
        auto result=call(cmd);if(result["document_id"]!=document_||integer(result["revision"],1)!=rev)bad("Restored document identity mismatch");
        synchronized_=true;return result;
    }
    void ensure(const QJsonObject& request){start();if(!synchronized_)restored(current(),request,revision_,false);}
    QJsonObject materialized(const State& candidate,const QJsonObject& request){
        if(request["native_linesplan"].toBool())return published(candidate,request,document_,revision_+1);
        ensure(request);
        const auto file=work_.filePath("candidate.fbm"),idsFile=work_.filePath("candidate-bindings.json"),coords=work_.filePath("candidate-points.json");
        fs::document::promoteDocument(*candidate.engine,std::max(7,candidate.engine->version()),candidate.bindings).writeAtomic(file);
        auto bindings=candidate.bindings;bindings["high_water"]=highWater_;
        write(idsFile,QJsonDocument(bindings).toJson(QJsonDocument::Compact));
        QJsonArray moves;const auto ids=bindings["points"].toArray();
        for(size_t i=0;i<candidate.engine->controlPoints().size();++i){
            const auto& p=candidate.engine->controlPoints()[i];const auto& old=current().engine->controlPoints()[i];
            if(p.x!=old.x||p.y!=old.y||p.z!=old.z)moves.append(QJsonObject{{"point_id",ids[qsizetype(i)]},{"position",QJsonArray{double(p.x),double(p.y),double(p.z)}}});
        }
        write(coords,QJsonDocument(moves).toJson(QJsonDocument::Compact));
        QJsonObject cmd{{"method","apply_document_points"},{"path",file},{"bindings",idsFile},{"coordinates",coords},
            {"expected_document_id",document_},{"expected_revision",double(revision_)},{"revision",double(revision_+1)},
            {"output",text(request,"output")},{"native_linesplan",request["native_linesplan"]}};
        auto result=call(cmd);if(result["document_id"]!=document_||integer(result["revision"],1)!=revision_+1)bad("Published point identity mismatch");
        return result;
    }
    void guard(const QJsonObject& request){if(history_.empty())bad("No document loaded");if(text(request,"expected_document_id")!=document_)bad("Stale document ID");if(integer(request["expected_revision"],1)!=revision_)bad("Stale document revision");}
    quint64 memory()const{quint64 total=0;for(const auto& s:history_)total+=s.memory();return total;}
    void trim(){auto used=memory();while(history_.size()>2&&used>limit_){
        if(index_>0){used-=history_.front().memory();history_.erase(history_.begin());--index_;}
        else {used-=history_.back().memory();history_.pop_back();}
    }}
    State checkpoint(const QJsonObject& response,const QString& directory,const QString& id,quint64 revision){
        auto bytes=read(artifact(response,"state_file",directory));auto fbm=std::make_shared<Fbm>(Fbm::read(bytes));
        auto bindings=json(read(artifact(response,"binding_file",directory),128*1024*1024));
        if(bindings["format"]!="FreeShipDocumentBindings"||integer(bindings["version"])!=1||bindings["document_id"]!=id||integer(bindings["revision"],1)!=revision)bad("Invalid document checkpoint identity");
        auto high=bindings["high_water"].toObject();
        for(const auto& k:Kinds){if(!bindings[k].isArray())bad("Missing document ID map");auto max=integer(high[k],0,2000000);QSet<quint64> seen;
            for(const auto& v:bindings[k].toArray()){auto id=integer(v,1,max);if(seen.contains(id))bad("Duplicate document ID");seen.insert(id);}
        }
        if(size_t(bindings["points"].toArray().size())!=fbm->controlPoints().size())bad("FBM/control point identity count mismatch");
        return {fbm,fbm,bindings,0};
    }
    fs::ModelSnapshot validateArtifacts(const QJsonObject& r,const QString& directory,const QString& id,quint64 rev,bool native){
        if(native && validatedPublication_ && r==validatedResponse_ && validatedPublication_->documentId==id && validatedPublication_->revision==rev)
            return *validatedPublication_;
        auto model=fs::ModelSnapshot::load(artifact(r,"model",directory));if(model.documentId!=id||model.revision!=rev)bad("Invalid model snapshot identity");
        auto scene=Scene::load(artifact(r,"scene",directory),false);auto dxf=read(artifact(r,"dxf",directory));if(!dxf.trimmed().endsWith("EOF"))bad("Incomplete DXF");
        if(native){auto report=json(read(artifact(r,"native_report",directory),1024*1024));auto sections=read(artifact(r,"sections",directory),256*1024*1024);
            if(report["status"]!="pass"||report["engine"]!="cpp-native"||report["document_id"]!=id||integer(report["revision"],1)!=rev||report["scene_sha256"].toString().toLatin1()!=scene.digest||report["dxf_sha256"]!=digest(dxf)||report["sections_sha256"]!=digest(sections))bad("Invalid computed document artifacts");
            model.loadSectionDisplay(sections);
            auto surface=read(artifact(r,"surface",directory),256*1024*1024);
            if(!report["surface_display"].toBool()||report["surface_sha256"]!=digest(surface))bad("Invalid surface display artifact");
            model.verifySurfaceDisplay(surface);
        }
        return model;
    }
    void state(QJsonObject& response)const{
        response["document_engine"]="cpp-document";response["adapter_pid"]=double(adapter_.processId());
        response["adapter_requests"]=double(adapterRequest_);
        if(history_.empty())return;
        response["document_id"]=document_;response["revision"]=double(revision_);response["modified"]=current().token!=cleanToken_;
        response["can_undo"]=index_>0;response["can_redo"]=index_+1<history_.size();response["history_states"]=double(history_.size());
        response["history_bytes"]=double(memory());response["history_limit_bytes"]=double(limit_);
        const auto info=current().saved->inspect();
        response["preview_enabled"]=info["preview"].toObject()["enabled"].toBool()&&info["opaque_tail_bytes"].toInteger()==0;
    }
public:
    Service(){if(!work_.isValid())bad("Cannot create native document workspace");}
    ~Service(){stop();}
    QJsonObject execute(const QJsonObject& request,bool& quit){
        QElapsedTimer timer;timer.start();QJsonObject result{{"id",request["id"]},{"ok",false}};
        try{
            integer(request["id"]);auto method=text(request,"method");
            if(method=="quit"){quit=true;result["ok"]=true;state(result);return result;}
            if(method=="document_context"){
                if(!request["colors"].isArray())bad("Expected context color array");
                result=fs::document::documentContext(QCoreApplication::applicationDirPath()+"/bridge",request["colors"].toArray());
                result["id"]=request["id"];result["ok"]=true;result["preferences_engine"]="cpp-preferences";
                result["preferences_ms"]=timer.nsecsElapsed()/1e6;state(result);return result;
            }
            if(request.contains("native_linesplan")&&!request["native_linesplan"].isBool())bad("Invalid native_linesplan flag");
            if(request.contains("preferences")&&!request["preferences"].isObject())bad("Invalid preferences object");
            bool native=request["native_linesplan"].toBool();
            const bool fresh=method=="load"||method=="new"||method=="import_model";
            if(request.contains("recovered")&&(!request["recovered"].isBool()||method!="load"))bad("Invalid recovered flag");
            if(request.contains("recovery_root")&&(!fresh||!request["recovery_root"].isString()))bad("Invalid recovery directory");
            if(request.contains("recovery_source")&&!request["recovery_source"].isString())bad("Invalid recovery source");
            if(!fresh||(!history_.empty()&&method!="load"))guard(request);
            if(fresh){
                auto id=text(request,"document_id");if(QUuid(id).isNull()||id==document_)bad("A new document UUID is required");
                std::shared_ptr<const Fbm> original;
                if(method=="load")original=std::make_shared<Fbm>(Fbm::read(read(text(request,"path"))));
                QJsonObject exchange;if(method=="import_model")exchange=json(read(text(request,"path"),128*1024*1024));
                auto directory=output(request);
                stop();QElapsedTimer contextTimer;contextTimer.start();
                auto context=fs::document::documentContext(QCoreApplication::applicationDirPath()+"/bridge",
                    original?fs::document::documentColors(*original):QJsonArray{},request["preferences"].toObject());
                const double contextMs=contextTimer.nsecsElapsed()/1e6;
                QElapsedTimer createTimer;createTimer.start();
                auto candidate=method=="load"?fs::document::loadDocument(*original,context):method=="new"?
                    fs::document::newDocument(request,context):fs::document::importDocument(exchange,context);
                const double createMs=createTimer.nsecsElapsed()/1e6;
                auto engine=std::make_shared<Fbm>(std::move(candidate.document));
                State next{original?original:engine,engine,candidate.bindings,0};
                auto bindings=next.bindings;bindings["document_id"]=id;bindings["revision"]=1;
                QJsonObject response;
                if(native)response=published(next,request,id,1);
                else {
                    auto fbm=work_.filePath("fresh.fbm"),ids=work_.filePath("fresh-bindings.json");
                    fs::document::promoteDocument(*engine,std::max(7,engine->version()),bindings).writeAtomic(fbm);
                    write(ids,QJsonDocument(bindings).toJson(QJsonDocument::Compact));
                    response=call({{"method","restore_document"},{"path",fbm},{"bindings",ids},{"document_id",id},
                        {"revision",1},{"output",directory},{"publish",true},{"native_linesplan",false}});
                }
                if(response["document_id"]!=id||integer(response["revision"],1)!=1)bad("New document identity mismatch");
                auto model=validateArtifacts(response,directory,id,1,native);
                if(size_t(model.points.size())!=engine->controlPoints().size())bad("Initial control point count changed");
                for(qsizetype i=0;i<model.points.size();++i){const auto &p=model.points[i];const auto &n=engine->controlPoints()[i];
                    if(p.id!=quint64(i+1)||p.position.x!=n.x||p.position.y!=n.y||p.position.z!=n.z||p.vertexType!=n.vertexType||p.locked!=n.locked)bad("Initial native control point changed");}
                auto newLimit=integer(context["history_limit_bytes"],0,1024ULL*1024*1024*1024);
                next.token=++nextToken_;history_.clear();history_.push_back(std::move(next));index_=0;document_=id;revision_=1;
                cleanToken_=method=="load"&&!request["recovered"].toBool()?current().token:0;limit_=newLimit;highWater_=current().bindings["high_water"].toObject();synchronized_=!native;result=response;
                plateContext_=context["plate_context"].toObject();
                result["creation_engine"]="cpp-document-create";result["creation_ms"]=createMs;result["adapter_operation"]=native?"none":"restore_document";
                result["preferences_engine"]="cpp-preferences";result["preferences_ms"]=contextMs;
            }else if(method=="configure_preferences"){
                if(!native)bad("Preferences require native publication");
                const auto overrides=request["preferences"].toObject();
                // Preference changes are not FBM edits or undo steps. Rebind
                // every retained state so undo does not restore an old theme.
                auto nextHistory=history_;QJsonArray requestedColors;QSet<int> seenColors;
                for(const auto &entry:nextHistory)for(auto value:fs::document::documentColors(*entry.engine)){
                    const int raw=value.toInt();if(!seenColors.contains(raw)){seenColors.insert(raw);requestedColors.append(raw);}
                }
                auto activeContext=fs::document::documentContext(QCoreApplication::applicationDirPath()+"/bridge",requestedColors,overrides);
                for(size_t i=0;i<nextHistory.size();++i){
                    auto &entry=nextHistory[i];
                    auto context=activeContext;
                    // Never reinterpret a live file's names under a new codec.
                    context["fbm_encoding"]=entry.bindings["fbm_encoding"];
                    context.remove("history_limit_bytes");
                    for(auto it=context.begin();it!=context.end();++it)entry.bindings[it.key()]=it.value();
                }
                const auto nextLimit=integer(activeContext["history_limit_bytes"],0,1024ULL*1024*1024*1024);
                const bool changed=nextHistory[index_].bindings!=current().bindings || nextLimit!=limit_;
                auto response=published(nextHistory[index_],request,document_,revision_+(changed?1:0));
                validateArtifacts(response,output(request),document_,revision_+(changed?1:0),true);
                history_=std::move(nextHistory);limit_=nextLimit;plateContext_=activeContext["plate_context"].toObject();
                if(changed)++revision_;trim();synchronized_=false;result=response;result["adapter_operation"]="none";
            }else if(method=="save"){
                clearSaveReceipt();
                auto saved=current().saved;
                if(request.contains("preview_jpeg")){
                    const auto value=request["preview_jpeg"];
                    if(!value.isString()||value.toString().size()>700000)bad("Invalid preview payload");
                    auto decoded=QByteArray::fromBase64Encoding(value.toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
                    if(!decoded)bad("Invalid preview encoding");
                    saved=std::make_shared<Fbm>(fs::document::withPreviewImage(*saved,decoded.decoded));
                }
                auto filename=output(request)+"/saved.fbm";saved->writeAtomic(filename);
                result["saved_file"]=filename;result["preview_updated"]=saved->bytes()!=current().saved->bytes();
                preparedSave_=std::move(saved);preparedSaveBase_=current().saved;preparedSaveRevision_=revision_;
                preparedReceipt_={{"version",1},{"token",QUuid::createUuid().toString(QUuid::WithoutBraces)},
                    {"document_id",document_},{"revision",double(revision_)},
                    {"bytes",double(preparedSave_->bytes().size())},{"sha256",digest(preparedSave_->bytes())}};
                result["save_receipt"]=preparedReceipt_;
            }else if(method=="mark_saved"){
                // A receipt binds the exact prepared bytes to one revision.
                // Retain the completed receipt for retries after a lost reply.
                if(!request["save_receipt"].isObject()||preparedReceipt_.isEmpty()||
                   request["save_receipt"].toObject()!=preparedReceipt_||!preparedSave_||
                   preparedSaveBase_!=current().saved||preparedSaveRevision_!=revision_||
                   preparedReceipt_["document_id"]!=document_)
                    bad("保存确认已过期或缺少对应的保存结果；当前修改仍保留。");
                const auto target=fs::modelFilePath(text(request,"saved_path"));
                if(!confirmedSavePath_.isEmpty()&&!fs::sameModelFile(confirmedSavePath_,target))
                    bad("重复保存确认指向了不同文件；未更改保存状态。");
                const auto actual=fs::observeModelFile(target);
                if(!actual.exists||actual.bytes!=preparedSave_->bytes().size()||
                   actual.sha256.toHex()!=preparedReceipt_["sha256"].toString().toLatin1())
                    bad("目标文件与本次准备保存的船模不同；当前修改和恢复副本仍保留。");
                history_[index_].saved=preparedSave_;preparedSaveBase_=current().saved;
                confirmedSavePath_=target;cleanToken_=current().token;
                result["save_receipt"]=preparedReceipt_;result["saved_path"]=target;result["save_confirmed"]=true;
            }
            else if(method=="discard_recovery"){
                if(recovery_)recovery_->remove();
                recoveryRevision_=revision_;
            }
            else if(method=="document_status"){
                result["fbm"]=current().saved->inspect();result["fbm_sha256"]=digest(current().saved->bytes());
            }else if(method=="hydrostatics"||method=="hydro_defaults"){
                const bool reused=hydro_ && hydroSource_==current().engine && hydroBindings_==current().bindings;
                if(!reused) {
                    auto next=std::make_unique<fs::document::Hydrostatics>(*current().engine,current().bindings);
                    hydro_=std::move(next);hydroSource_=current().engine;hydroBindings_=current().bindings;
                }
                if(method=="hydro_defaults")result["analysis"]=hydro_->defaults(current().bindings["fbm_encoding"].toString());
                else {
                    const auto directory=output(request);
                    auto report=hydro_->calculate(request,directory+"/cancel.flag");
                    report["document_id"]=document_;report["revision"]=double(revision_);report["mesh_reused"]=reused;
                    const auto filename=directory+"/hydro.json";write(filename,QJsonDocument(report).toJson(QJsonDocument::Compact));result["hydro"]=filename;
                }
                result["analysis_engine"]="cpp-hydrostatics";result["mesh_reused"]=reused;
            }else if((method=="performance_defaults"||method=="performance")&&request["calculator"]=="addmass"){
                const auto directory=output(request);
                if(method=="performance"&&(!validatedPublication_||validatedPublication_->documentId!=document_||validatedPublication_->revision!=revision_))
                    published(current(),request,document_,revision_);
                auto report=fs::document::addedMass(*current().engine,request,directory,current().bindings,validatedPublication_.get());
                report["document_id"]=document_;report["revision"]=double(revision_);
                const auto filename=directory+"/performance.json";write(filename,QJsonDocument(report).toJson(QJsonDocument::Compact));
                result["performance"]=filename;result["analysis_engine"]="cpp-addmass";
            }else if((method=="performance_defaults"||method=="performance")&&fs::document::nativeHydrodynamics(request["calculator"].toString())){
                const auto directory=output(request);
                auto report=fs::document::hydrodynamics(*current().engine,request,directory,current().bindings);
                report["document_id"]=document_;report["revision"]=double(revision_);
                const auto filename=directory+"/performance.json";write(filename,QJsonDocument(report).toJson(QJsonDocument::Compact));
                result["performance"]=filename;result["analysis_engine"]="cpp-hydrodynamics";
            }else if((method=="performance_defaults"||method=="performance")&&fs::document::nativePropulsion(request["calculator"].toString())){
                const auto directory=output(request);
                auto report=fs::document::propulsion(*current().engine,request,directory);
                report["document_id"]=document_;report["revision"]=double(revision_);
                const auto filename=directory+"/performance.json";write(filename,QJsonDocument(report).toJson(QJsonDocument::Compact));
                result["performance"]=filename;result["analysis_engine"]="cpp-propulsion";
            }else if((method=="performance_defaults"||method=="performance")&&fs::document::nativeResistance(request["calculator"].toString())){
                const auto directory=output(request);
                auto report=fs::document::resistance(*current().engine,request,directory,current().bindings);
                report["document_id"]=document_;report["revision"]=double(revision_);
                const auto filename=directory+"/performance.json";write(filename,QJsonDocument(report).toJson(QJsonDocument::Compact));
                result["performance"]=filename;result["analysis_engine"]="cpp-resistance";
            }else if(method=="cad_iges"){
                const auto directory=output(request);
                auto report=fs::document::cadPatches(*current().engine,current().bindings,directory+"/cancel.flag");
                report["document_id"]=document_;report["revision"]=double(revision_);
                const auto filename=directory+"/cad.json";write(filename,QJsonDocument(report).toJson(QJsonDocument::Compact));
                result["cad"]=filename;result["analysis_engine"]="cpp-cad";
            }else if(method=="develop_plates"){
                const auto directory=output(request);
                auto report=fs::document::developPlates(*current().engine,current().bindings,plateContext_,directory+"/cancel.flag");
                report["document_id"]=document_;report["revision"]=double(revision_);
                const auto filename=directory+"/plates.json";write(filename,QJsonDocument(report).toJson(QJsonDocument::Compact));
                result["plates"]=filename;result["analysis_engine"]="cpp-plates";
            }else if(method=="undo"||method=="redo"){
                if(revision_==MaxInteger)bad("Revision limit reached");
                if((method=="undo"&&index_==0)||(method=="redo"&&index_+1>=history_.size()))bad("No "+method+" state available");
                auto target=method=="undo"?index_-1:index_+1;auto directory=output(request);
                // Original undo reloads FBM and reconstructs each incident-edge
                // list in file order. Live edit order is not a saved FBM field.
                auto next=history_[target];next.bindings.remove("point_edges");next.bindings.remove("point_anchors");next.bindings.remove("point_lines");
                // Layer traversal participates in Single-precision hydrostatic
                // sums. Keep it with the snapshot so redo reproduces the same
                // derived values, not a different accumulation order.
                next.bindings.remove("last_used_layer_id");
                auto response=restored(next,request,revision_+1,true);validateArtifacts(response,directory,document_,revision_+1,native);
                history_[target]=std::move(next);index_=target;++revision_;result=response;
            }else if(method=="move_point"||method=="move_points"){
                if(revision_==MaxInteger)bad("Revision limit reached");
                if(current().saved->inspect()["opaque_tail_bytes"].toInteger()>0)bad("工程含有尚未识别的扩展数据；可原样保存，暂不能编辑，避免丢失这些数据。");
                auto directory=output(request);QElapsedTimer editTimer;editTimer.start();
                auto edited=std::make_shared<Fbm>(fs::document::editPoints(*current().engine,current().bindings,request));
                const bool changed=edited->bytes()!=current().engine->bytes();
                const double editMs=editTimer.nsecsElapsed()/1e6;
                if(changed){
                    State next{edited,edited,current().bindings,0};next.bindings["revision"]=double(revision_+1);
                    auto response=materialized(next,request);
                    auto model=validateArtifacts(response,directory,document_,revision_+1,native);
                    const auto ids=next.bindings["points"].toArray();QHash<quint64,qsizetype> indices;
                    for(qsizetype i=0;i<ids.size();++i)indices.insert(integer(ids[i],1),i);
                    if(model.points.size()!=ids.size())bad("Restored point count changed");
                    for(const auto& p:model.points){
                        auto it=indices.constFind(p.id);if(it==indices.cend())bad("Restored point identity changed");
                        const auto& expected=edited->controlPoints()[*it];
                        if(p.position.x!=expected.x||p.position.y!=expected.y||p.position.z!=expected.z)bad("Restored native point coordinates changed");
                    }
                    next.token=++nextToken_;history_.erase(history_.begin()+index_+1,history_.end());
                    history_.push_back(std::move(next));++index_;++revision_;trim();result=response;
                }else{
                    result=snapshot(request);
                    validateArtifacts(result,directory,document_,revision_,native);
                }
                result["edit_engine"]="cpp-points";result["fbm_write_engine"]="cpp-coordinate-writer";
                result["point_edit_ms"]=editMs;result["adapter_operation"]=native?"none":changed?"apply_document_points":"snapshot";
            }else if(method=="inspect_modeling"){
                result["analysis"]=fs::document::inspectLackenby(*current().engine);
                result["analysis_engine"]="cpp-lackenby";
            }else if(method=="append_model"||fs::document::nativeTopologyCommand(request)||fs::document::nativeTransformCommand(request)||fs::document::nativeLackenbyCommand(request)||fs::document::nativeAttributeCommand(request)||fs::document::nativeFaceToolsCommand(request)||fs::document::nativeProjectCommand(request)||fs::document::nativeDefinitionCommand(request)||fs::document::nativeAuxiliaryCommand(request)||fs::document::nativeBackgroundCommand(request)||fs::document::nativeAnalysisCommand(request)){
                if(revision_==MaxInteger)bad("Revision limit reached");
                if(current().saved->inspect()["opaque_tail_bytes"].toInteger()>0)bad("工程含有尚未识别的扩展数据；可原样保存，暂不能编辑，避免丢失这些数据。");
                const auto directory=output(request);State base=current();
                // Old formats cannot encode every constraint/lock field. The
                // format conversion is not an edit algorithm or history step.
                int minimumVersion=method=="append_model"?21:0;const auto operation=request["operation"].toString();
                if(operation=="set_locked"&&request["locked"].toBool())minimumVersion=13;
                if(operation=="linear_constraint")minimumVersion=41;
                if(operation=="anchor")minimumVersion=43;
                if(operation=="insert_plane"&&request["create_curve"].toBool())minimumVersion=12;
                if(operation=="intersect_layers")minimumVersion=41;
                if(operation=="point_properties"&&!request["name"].toString().isEmpty())minimumVersion=41;
                if(operation=="create_group"||operation=="update_group")minimumVersion=42;
                if(operation=="add_markers")minimumVersion=11;
                const bool project=fs::document::nativeProjectCommand(request);
                const bool definitions=fs::document::nativeDefinitionCommand(request);
                const bool auxiliary=fs::document::nativeAuxiliaryCommand(request);
                const bool background=fs::document::nativeBackgroundCommand(request);
                const bool analysis=fs::document::nativeAnalysisCommand(request);
                if(project)minimumVersion=fs::document::projectMinimumVersion(*base.engine,base.bindings,request);
                if(definitions)minimumVersion=fs::document::definitionMinimumVersion(*base.engine,base.bindings,request);
                if(auxiliary)minimumVersion=fs::document::auxiliaryMinimumVersion(*base.engine,base.bindings,request);
                if(background)minimumVersion=fs::document::backgroundMinimumVersion(*base.engine,base.bindings,request);
                if(base.engine->version()<minimumVersion){
                    base.engine=std::make_shared<Fbm>(fs::document::promoteDocument(*base.engine,minimumVersion,base.bindings));
                }
                base.bindings["high_water"]=highWater_;QElapsedTimer editTimer;editTimer.start();
                const bool transform=fs::document::nativeTransformCommand(request);
                const bool lackenby=fs::document::nativeLackenbyCommand(request);
                const bool attributes=fs::document::nativeAttributeCommand(request);
                const bool faceTools=fs::document::nativeFaceToolsCommand(request);
                // The adapter may normalize record padding while capturing its
                // canonical point order. Restore unrelated persisted calculator
                // records before applying a native transform to that geometry.
                auto candidate=[&](){
                    if(method=="append_model")return fs::document::appendDocument(base.engine->withCalculationRecordsFrom(*base.saved),base.bindings,json(read(text(request,"path"),128*1024*1024)));
                    if(analysis)return fs::document::editAnalysis(base.engine->withCalculationRecordsFrom(*base.saved),base.bindings,request);
                    if(auxiliary)return fs::document::editAuxiliary(base.engine->withCalculationRecordsFrom(*base.saved),base.bindings,request);
                    if(background)return fs::document::editBackground(base.engine->withCalculationRecordsFrom(*base.saved),base.bindings,request);
                    if(project)return fs::document::editProject(base.engine->withCalculationRecordsFrom(*base.saved),base.bindings,request);
                    if(definitions)return fs::document::editDefinitions(base.engine->withCalculationRecordsFrom(*base.saved),base.bindings,request);
                    if(attributes)return fs::document::editAttributes(base.engine->withCalculationRecordsFrom(*base.saved),base.bindings,request);
                    if(faceTools)return fs::document::editFaceTools(base.engine->withCalculationRecordsFrom(*base.saved),base.bindings,request);
                    if(lackenby){const auto source=base.engine->withCalculationRecordsFrom(*base.saved);
                        bool changed=false;auto edited=fs::document::editLackenby(source,base.bindings,request,&changed);
                        return fs::document::TopologyEdit{std::move(edited),base.bindings,changed};}
                    if(!transform)return fs::document::editTopology(*base.engine,base.bindings,request);
                    const auto source=base.engine->withCalculationRecordsFrom(*base.saved);
                    bool changed=false;auto edited=fs::document::editTransforms(source,base.bindings,request,&changed);
                    return fs::document::TopologyEdit{std::move(edited),base.bindings,changed};
                }();
                const double editMs=editTimer.nsecsElapsed()/1e6;
                if(candidate.changed){
                    auto edited=std::make_shared<Fbm>(std::move(candidate.document));
                    State next{edited,edited,candidate.bindings,0};next.bindings["revision"]=double(revision_+1);
                    auto response=restored(next,request,revision_+1,true);
                    auto model=validateArtifacts(response,directory,document_,revision_+1,native);
                    if(auxiliary && method=="auxiliary") {
                        const auto curveIds=next.bindings["curves"].toArray(),flowIds=next.bindings["flows"].toArray();
                        const auto controls=edited->controlNet();const auto pointIds=next.bindings["points"].toArray();
                        if(model.controlCurves.size()!=curveIds.size() || model.flowlines.size()!=flowIds.size())bad("Restored auxiliary count changed");
                        for(qsizetype i=0;i<curveIds.size();++i) {
                            const auto &curve=model.controlCurves[i];const auto &definition=controls.curves[size_t(i)];
                            if(curve.id!=integer(curveIds[i]) || size_t(curve.controls.size())!=definition.points.size())bad("Restored curve identity changed");
                            for(qsizetype j=0;j<curve.controls.size();++j)if(curve.controls[j]!=integer(pointIds[definition.points[size_t(j)]]))bad("Restored curve controls changed");
                        }
                        for(qsizetype i=0;i<flowIds.size();++i)if(model.flowlines[i].id!=integer(flowIds[i]))bad("Restored flow identity changed");
                        if(operation=="create_flow" && (model.flowlines.isEmpty() || model.flowlines.back().spline.points.size()<2))
                            bad("The selected position does not produce a flowline on the submerged hull");
                    }
                    const auto net=edited->controlNet();const auto points=next.bindings["points"].toArray(),edges=next.bindings["edges"].toArray(),faces=next.bindings["faces"].toArray(),layers=next.bindings["layers"].toArray();
                    if(model.points.size()!=points.size()||model.edges.size()!=edges.size()||model.faces.size()!=faces.size())bad("Restored control net count changed");
                    QHash<quint64,qsizetype> pi,ei,fi;
                    for(qsizetype i=0;i<points.size();++i)pi.insert(integer(points[i]),i);
                    for(qsizetype i=0;i<edges.size();++i)ei.insert(integer(edges[i]),i);
                    for(qsizetype i=0;i<faces.size();++i)fi.insert(integer(faces[i]),i);
                    for(const auto& p:model.points){if(!pi.contains(p.id))bad("Restored point identity changed");const auto& v=net.points[pi[p.id]].value;
                        auto reference=[&](int i)->quint64{return i<0?0:integer(points[i]);};
                        if(p.position.x!=v.x||p.position.y!=v.y||p.position.z!=v.z||p.locked!=v.locked||p.vertexType!=v.vertexType||p.anchor!=reference(v.anchor)||p.lineA!=reference(v.lineA)||p.lineB!=reference(v.lineB)||p.hardAnchor!=v.hardAnchor)bad("Restored native point properties changed");}
                    for(const auto& e:model.edges){if(!ei.contains(e.id))bad("Restored edge identity changed");const auto& v=net.edges[ei[e.id]];
                        if(e.a!=integer(points[v.a])||e.b!=integer(points[v.b])||e.crease!=v.crease)bad("Restored native edge changed");}
                    for(const auto& f:model.faces){if(!fi.contains(f.id))bad("Restored face identity changed");const auto& v=net.faces[fi[f.id]];
                        if(f.layer!=integer(layers[v.layer])||size_t(f.points.size())!=v.points.size())bad("Restored native face changed");
                        for(qsizetype i=0;i<f.points.size();++i)if(f.points[i]!=integer(points[v.points[size_t(i)]]))bad("Restored native face winding changed");}
                    next.token=++nextToken_;highWater_=next.bindings["high_water"].toObject();
                    history_.erase(history_.begin()+index_+1,history_.end());history_.push_back(std::move(next));++index_;++revision_;trim();result=response;
                }else{
                    result=snapshot(request);validateArtifacts(result,directory,document_,revision_,native);
                }
                result["edit_engine"]=analysis?"cpp-analysis-settings":background?"cpp-background":auxiliary?"cpp-auxiliary":project?"cpp-project":definitions?"cpp-definitions":lackenby?"cpp-lackenby":transform?"cpp-transforms":attributes?"cpp-attributes":faceTools?"cpp-face-tools":"cpp-topology";
                result["fbm_write_engine"]=analysis?"cpp-analysis-settings-writer":background?"cpp-background-writer":auxiliary?"cpp-auxiliary-writer":project?"cpp-project-writer":definitions?"cpp-definition-writer":lackenby?"cpp-coordinate-writer":transform?"cpp-transform-writer":attributes?"cpp-attribute-writer":"cpp-control-net-writer";
                result[analysis?"analysis_edit_ms":background?"background_edit_ms":auxiliary?"auxiliary_edit_ms":project?"project_edit_ms":definitions?"definition_edit_ms":lackenby?"lackenby_edit_ms":transform?"transform_edit_ms":attributes?"attribute_edit_ms":faceTools?"face_tools_edit_ms":"topology_edit_ms"]=editMs;result["adapter_operation"]=native?"none":candidate.changed?"restore_document":"snapshot";
            }else if(method=="snapshot" && native){
                result=snapshot(request);
            }else{
                const QSet<QString> readOnly{"snapshot","performance_defaults","performance"};
                const QSet<QString> edits{"move_point","move_points","update_project","update_layer","topology","advanced","modeling","sections","manage_layers","background","display","auxiliary","hydro_settings","performance_settings"};
                bool edit=edits.contains(method);if(!edit&&!readOnly.contains(method))bad("Unknown method "+method);
                if(edit&&current().saved->inspect()["opaque_tail_bytes"].toDouble()>0)bad("工程含有尚未识别的扩展数据；可原样保存，暂不能编辑，避免丢失这些数据。");
                ensure(request);auto cmd=request;cmd["checkpoint"]=edit;auto response=call(cmd);
                if(response["document_id"]!=document_)bad("Adapter returned another document");auto rev=integer(response["revision"],1);
                if((!edit&&rev!=revision_)||(edit&&rev!=revision_&&rev!=revision_+1))bad("Unexpected document revision");
                if(response.contains("model"))validateArtifacts(response,text(request,"output"),document_,rev,native);
                if(edit&&rev!=revision_){
                    auto next=checkpoint(response,text(request,"output"),document_,rev);next.token=++nextToken_;
                    auto high=next.bindings["high_water"].toObject();for(const auto& k:Kinds)if(integer(high[k])<integer(highWater_[k]))bad("Document IDs were reused");
                    history_.erase(history_.begin()+index_+1,history_.end());history_.push_back(std::move(next));++index_;revision_=rev;highWater_=high;trim();
                }result=response;
            }
            if(!preparedReceipt_.isEmpty()&&(preparedReceipt_["document_id"]!=document_||preparedSaveRevision_!=revision_))clearSaveReceipt();
            // Persist the exact save bytes after the transaction commits. A disk
            // failure must not turn an accepted edit into a reported rollback.
            QElapsedTimer recoveryClock;recoveryClock.start();
            try{
                if(fresh){
                    // A failed open never reaches here and keeps the old copy.
                    QString cleanupError;
                    if(recovery_)try{recovery_->remove();}catch(const std::exception& e){cleanupError=QString::fromUtf8(e.what());}
                    recovery_.reset();recoveryRevision_=0;
                    recoveryRoot_=request["recovery_root"].toString();recoverySource_=request["recovery_source"].toString();
                    if(!cleanupError.isEmpty())result["recovery_error"]=cleanupError;
                }else if(method=="mark_saved")recoverySource_=confirmedSavePath_;
                if(!recoveryRoot_.isEmpty()&&method!="discard_recovery"){
                    if(!recovery_)recovery_=std::make_unique<fs::RecoveryLease>(recoveryRoot_,document_);
                    if(current().token==cleanToken_){recovery_->remove();recoveryRevision_=0;}
                    else if(recoveryRevision_!=revision_){recovery_->checkpoint(current().saved->bytes(),revision_,recoverySource_);recoveryRevision_=revision_;}
                    if(current().token!=cleanToken_)result["recovery_file"]=recovery_->path();
                }
            }catch(const std::exception& e){result["recovery_error"]=QString::fromUtf8(e.what());}
            result["recovery_ms"]=recoveryClock.nsecsElapsed()/1e6;
            result["ok"]=true;
        }catch(const std::exception& e){stop();result={{"id",request["id"]},{"ok",false},{"error",QString::fromUtf8(e.what())}};}
        result["id"]=request["id"];result.remove("state_file");result.remove("binding_file");state(result);result["document_total_ms"]=timer.nsecsElapsed()/1e6;return result;
    }
};
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);if(argc<2||app.arguments()[1]!="--serve")return 3;
#ifdef Q_OS_WIN
    if(argc==3){bool ok=false;auto pid=app.arguments()[2].toUInt(&ok);HANDLE parent=ok?OpenProcess(SYNCHRONIZE,FALSE,pid):nullptr;if(!parent)return 3;
        std::thread([parent]{WaitForSingleObject(parent,INFINITE);TerminateProcess(GetCurrentProcess(),1);}).detach();}
#endif
    try{Service service;bool quit=false;std::string line;
        while(!quit){
            line.clear();bool over=false;char c;
            while(std::cin.get(c)&&c!='\n'){if(line.size()<1024*1024)line+=c;else over=true;}
            if(line.empty()&&!std::cin)break;
            QJsonObject response;
            try{if(over)bad("Request exceeds 1 MiB");response=service.execute(requestJson(QByteArray::fromStdString(line)),quit);}
            catch(const std::exception& e){response={{"id",0},{"ok",false},{"error",QString::fromUtf8(e.what())}};}
            auto bytes=QJsonDocument(response).toJson(QJsonDocument::Compact);std::cout.write(bytes.constData(),bytes.size());std::cout<<'\n'<<std::flush;
        }return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
