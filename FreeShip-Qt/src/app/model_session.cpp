#include "model_session.h"
#include "core/platform_process.h"
#include "core/snapshot_sha256.h"
#include "core/snapshot_surface.h"
#include "calculation_scheduler.h"
#include "ui_language.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <QStandardPaths>
#include <QLocale>
#include <QThread>
#include <QThreadPool>
#include <future>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
[[noreturn]] void error(const QString& message) {
    throw std::runtime_error(message.toUtf8().constData());
}
QByteArray boundedBytes(const QString& path,qint64 limit=256LL*1024*1024){
    QFile f(path);if(f.size()<1||f.size()>limit||!f.open(QIODevice::ReadOnly))return {};
    auto data=f.readAll();return f.error()==QFile::NoError?data:QByteArray{};
}
QByteArray cacheHash(const QByteArray& bytes){return QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();}
// Hash verified publications in one pass. Keep streaming for large files.
QByteArray artifactDigest(QFile& file){
    if(!file.open(QIODevice::ReadOnly))return {};
    const auto size=file.size();
    if(size>=0&&size<=64*1024*1024){const auto data=file.readAll();return file.error()==QFile::NoError&&data.size()==size?fs::snapshotSha256(data):QByteArray{};}
    QCryptographicHash hash(QCryptographicHash::Sha256);return hash.addData(&file)?hash.result().toHex():QByteArray{};
}
QString dependencyToken(const QString& root){
    const auto bytes=boundedBytes(root+"/current",256);if(bytes.isEmpty())return {};
    const auto token=QString::fromLatin1(bytes).trimmed();
    if(!token.startsWith("set-{")||token.size()!=42||QUuid(token.mid(4)).isNull())error("计算缓存版本无效。");
    return token;
}
void copyDependencyGeneration(const QString& source,const QString& target,const QString& token){
    if(token.isEmpty()){if(QFile::exists(target+"/current")&&!QFile::remove(target+"/current"))error("无法更新计算缓存。");return;}
    const auto to=target+'/'+token;
    if(!QDir().mkpath(to))error("无法创建计算缓存目录。");
    for(const auto* name:{"RESIST.dat","RESISTp.dat","RESIST-native.json","RESIST-acceleration.json","RESIST-acceleration.dat","Propres.dat","Propres-native.json"}){
        const auto from=source+'/'+token+'/'+name;
        if(!QFileInfo::exists(from))continue;
        if(QFileInfo(from).isSymLink()||QFileInfo(from).size()>16*1024*1024)error("计算缓存文件无效。");
        ModelSession::atomicCopy(from,to+'/'+name);
    }
    QSaveFile file(target+"/current");file.setDirectWriteFallback(false);const auto bytes=(token+"\r\n").toLatin1();
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())error("无法提交计算缓存。");
}
void saveFirstFrameCache(const QString& root,const QByteArray& key,std::shared_ptr<const fs::ModelSnapshot> snapshot,
                         const QByteArray& model,const QByteArray& sections,const QByteArray& surface){
    if(root.isEmpty()||key.isEmpty()||model.isEmpty())return;
    const auto data=snapshot->displayCache(model);if(data.isEmpty()||!QDir().mkpath(root))return;
    QSaveFile payload(root+"/display.bin");payload.setDirectWriteFallback(false);
    if(!payload.open(QIODevice::WriteOnly)||payload.write(data)!=data.size()||!payload.commit())return;
    QJsonObject meta{{"version",2},{"key",QString::fromLatin1(key)},{"document_id",snapshot->documentId},
        {"display",QString::fromLatin1(cacheHash(data))},{"sections",QString::fromLatin1(cacheHash(sections))},{"surface",QString::fromLatin1(cacheHash(surface))}};
    QSaveFile f(root+"/entry.json");f.setDirectWriteFallback(false);const auto bytes=QJsonDocument(meta).toJson(QJsonDocument::Compact);
    if(f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size())f.commit();
}
quint64 revision(const QJsonObject& response) {
    const auto value = response.value("revision");
    const double number = value.toDouble(-1);
    if (!value.isDouble() || number < 1 || number > 9007199254740991.0 || std::floor(number) != number)
        error("计算组件返回了无效的模型版本。");
    return quint64(number);
}
}

struct PreparedSnapshot {
    std::shared_ptr<fs::ModelSnapshot> model;
    std::shared_ptr<const Scene> scene;
    QString modelFile,sceneFile,dxfFile;
    QByteArray sectionsHash,surfaceHash,sectionData,surfaceData,modelData;
    bool metadata=false,reusedFirstFrame=false,sameSections=false,sameSurface=false;
    double parseMs=0;
};
struct ModelSession::PendingMoveUndo {
    QJsonObject receipt, candidatePaths;
    QString output;
    std::shared_ptr<PreparedSnapshot> prepared;
    quint64 candidateRevision=0,restoredRevision=0;
    void prepare(ModelSession* session,const std::shared_ptr<PendingMoveUndo>& self,
        const QJsonObject& response,const QJsonObject& paths);
};
namespace {
QJsonObject reportTimings(const QJsonObject& response){
    QJsonObject timings;
    for(auto it=response.begin();it!=response.end();++it)if(it.key().endsWith("_ms"))timings[it.key()]=it.value();
    return timings;
}
PreparedSnapshot prepareSnapshot(const QJsonObject& response,const QJsonObject& paths,const QString& pendingDocument_,const QString& method_,
    std::shared_ptr<const fs::ModelSnapshot> model_,std::shared_ptr<const Scene> scene_,std::shared_ptr<fs::ModelSnapshot> firstFrame_,
    const QByteArray& sectionDigest_,const QByteArray& surfaceDigest_,const QByteArray& firstFrameSections_,const QByteArray& firstFrameSurface_,bool allowParallel=true){
    QElapsedTimer clock;clock.start();
    const auto path=[&](const char* key){return paths[key].toString();};
    const QString modelFile = path("model");
    const QString sceneFile = path("scene");
    const QString dxfFile = path("dxf");
    const bool deferred=response["drawing_deferred"].toBool();
    const auto readDrawing=[&]() -> std::shared_ptr<const Scene> {
        if(deferred)return {};
        if(scene_&&!scene_->digest.isEmpty()){
            QFile file(sceneFile);
            if(artifactDigest(file)==scene_->digest)return scene_;
        }
        return std::make_shared<Scene>(Scene::load(sceneFile));
    };
    std::future<std::shared_ptr<const Scene>> drawingJob;
    const bool pointEdit=method_=="move_point"||method_=="move_points"||response["point_placement"]==true;
    const bool parallel=allowParallel&&pointEdit&&QThread::idealThreadCount()>=4&&QFileInfo(modelFile).size()>=2*1024*1024;
    if(parallel&&!deferred)drawingJob=std::async(std::launch::async,readDrawing);
    const bool metadata=model_ && !model_->sourceDigest().isEmpty() && response["metadata_reused"]==true &&
        response["metadata_base_sha256"].toString().toLatin1()==model_->sourceDigest() && response["metadata_patch"].isObject();
    std::shared_ptr<fs::ModelSnapshot> nextModel;
    bool reusedFirstFrame=false;
    if(firstFrame_&&firstFrame_->documentId==pendingDocument_&&firstFrame_->revision==revision(response)) {
        QFile f(modelFile);
        if(f.size()<=256LL*1024*1024&&!firstFrame_->sourceDigest().isEmpty()&&artifactDigest(f)==firstFrame_->sourceDigest()){
            if(firstFrame_->isCachedPreview()){
                nextModel=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::load(modelFile));
                nextModel->loadSectionDisplay(boundedBytes(path("sections")));nextModel->loadSurfaceDisplay(boundedBytes(path("surface")));
                nextModel->inheritPreviewDrawing(*firstFrame_);
            }else nextModel=std::make_shared<fs::ModelSnapshot>(*firstFrame_);
            reusedFirstFrame=true;
        }
    }
    if(reusedFirstFrame) {}
    else if(metadata) {
        nextModel=std::make_shared<fs::ModelSnapshot>(model_->withMetadata(response["metadata_patch"].toObject(),pendingDocument_,revision(response)));
        QFile actual(modelFile);QCryptographicHash hash(QCryptographicHash::Sha256);
        if(actual.size()>256LL*1024*1024||!actual.open(QIODevice::ReadOnly)||!hash.addData(&actual)||hash.result().toHex()!=nextModel->sourceDigest())
            error("模型增量与完整结果不一致。");
    }
    QFile dxf(dxfFile);
    if (!deferred&&(!dxf.open(QIODevice::ReadOnly) || !dxf.seek(std::max<qint64>(0, dxf.size() - 128)) ||
        !dxf.readAll().trimmed().endsWith("EOF"))) error("DXF 结果不完整。");
    QFile nativeReport(path("native_report"));
    if (nativeReport.size()>1024*1024 || !nativeReport.open(QIODevice::ReadOnly)) error("无法核对型线图的生成结果。");
    const auto native=QJsonDocument::fromJson(nativeReport.readAll()).object();
    if (native["status"]!="pass" || native["engine"]!="cpp-native" ||
        native["document_id"]!=pendingDocument_ || revision(native)!=revision(response) ||
        native["drawing_deferred"].toBool()!=deferred)
        error("型线图与当前模型版本不一致。");
    QCryptographicHash dxfHash(QCryptographicHash::Sha256);
    if (!deferred&&(!dxf.seek(0) || !dxfHash.addData(&dxf) || native["dxf_sha256"].toString().toLatin1()!=dxfHash.result().toHex()))
        error("型线图导出结果校验失败。");
    const auto readSurface=[&]{
        QFile file(path("surface"));
        if(file.size()>256LL*1024*1024||!file.open(QIODevice::ReadOnly))error("无法读取船体曲面。");
        auto bytes=file.readAll();
        const auto hash=fs::snapshotSha256(bytes);
        if(!native["surface_display"].toBool()||native["surface_sha256"].toString().toLatin1()!=hash)
            error("船体曲面结果校验失败。");
        return std::pair{std::move(bytes),hash};
    };
    std::future<std::pair<QByteArray,QByteArray>> surfaceJob;
    if(parallel)surfaceJob=std::async(std::launch::async,readSurface);
    QFile sectionFile(path("sections"));
    if(sectionFile.size()>256LL*1024*1024||!sectionFile.open(QIODevice::ReadOnly))error("无法读取剖面线。");
    const auto sectionData=sectionFile.readAll();
    const auto sectionsHash=fs::snapshotSha256(sectionData);
    if(native["sections_sha256"].toString().toLatin1()!=sectionsHash)
        error("剖面线结果校验失败。");
    const auto [surfaceData,surfaceHash]=surfaceJob.valid()?surfaceJob.get():readSurface();
    std::optional<fs::SnapshotSurface> decodedSurface;
    if(!nextModel&&pointEdit&&surfaceData.size()>=1024*1024&&surfaceData.size()<=64LL*1024*1024&&
       surfaceHash!=surfaceDigest_){
        // The worker's surface and the snapshot carry the same large arrays.
        // Reuse the decode only after matching the actual serialized arrays.
        decodedSurface=fs::SnapshotSurface::parse(surfaceData);
        nextModel=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::fromBytes(boundedBytes(modelFile),&*decodedSurface));
    }
    if(!nextModel)nextModel=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::load(modelFile));
    auto nextScene=drawingJob.valid()?drawingJob.get():readDrawing();
    if(!deferred&&native["scene_sha256"].toString().toLatin1()!=nextScene->digest)
        error("型线图与当前模型版本不一致。");
    if (nextModel->documentId != pendingDocument_ || nextModel->documentId != response.value("document_id").toString() ||
        nextModel->revision != revision(response)) error("收到的图形与当前模型版本不一致。");
    if (method_ != "load" && method_ != "new" && method_ != "import_model" && model_ && nextModel->revision < model_->revision)
        error("计算组件返回了过期的模型。");
    if(reusedFirstFrame&&sectionsHash!=firstFrameSections_){
        nextModel=std::make_shared<fs::ModelSnapshot>(*nextModel);reusedFirstFrame=false;
    }
    const bool sameSections=metadata&&sectionsHash==sectionDigest_;
    if(!sameSections&&!(reusedFirstFrame&&sectionsHash==firstFrameSections_))nextModel->loadSectionDisplay(sectionData);
    if(reusedFirstFrame&&surfaceHash!=firstFrameSurface_){
        nextModel=std::make_shared<fs::ModelSnapshot>(*nextModel);reusedFirstFrame=false;
    }
    const bool sameSurface=model_&&surfaceHash==surfaceDigest_&&nextModel->reuseSurfaceDisplay(*model_);
    if(!sameSurface&&!(reusedFirstFrame&&surfaceHash==firstFrameSurface_)){
        if(decodedSurface)nextModel->loadDecodedSurfaceDisplay(*decodedSurface);
        else nextModel->loadSurfaceDisplay(surfaceData);
    }

    return {nextModel,nextScene,modelFile,sceneFile,dxfFile,sectionsHash,surfaceHash,sectionData,surfaceData,method_=="load"?boundedBytes(modelFile):QByteArray{},
        metadata,reusedFirstFrame,sameSections,sameSurface,clock.nsecsElapsed()/1e6};
}
}

void ModelSession::PendingMoveUndo::prepare(ModelSession* session,const std::shared_ptr<PendingMoveUndo>& self,
                                            const QJsonObject& response,const QJsonObject& paths){
    qsizetype bytes=0;
    for(const auto& path:paths)bytes+=QFileInfo(path.toString()).size();
    if(bytes>64LL*1024*1024||QThread::idealThreadCount()<4||session->parsingPool_.activeThreadCount())return;
    const auto document=session->pendingDocument_;
    const auto before=session->undoAfterMove_;
    const auto scene=session->scene_;
    const auto sections=session->sectionDigest_,surface=session->surfaceDigest_;
    candidatePaths=paths;candidateRevision=revision(response);
    session->parsingPool_.start([session,self,response,paths,document,before,scene,sections,surface]{
        std::shared_ptr<PreparedSnapshot> prepared;
        try{prepared=std::make_shared<PreparedSnapshot>(prepareSnapshot(response,paths,document,"move_points",
            before,scene,nullptr,sections,surface,{},{}));}
        catch(const std::exception&){return;}
        QMetaObject::invokeMethod(session,[session,self,prepared]{
            if(session->stopping_)return;
            self->prepared=prepared;
            session->keepPreparedMoveForRedo(*self);
        },Qt::QueuedConnection);
    });
}

ModelSession::ModelSession(QObject* parent) : QObject(parent) {
    calculationScheduler_=std::make_shared<CalculationScheduler>();
    timeout_.setSingleShot(true);
    timeout_.setInterval(120000);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        fail("模型计算超时。当前画面已保留。", true);
    });
    saveRetry_.setSingleShot(true);saveRetry_.setInterval(3000);
    connect(&saveRetry_,&QTimer::timeout,this,[this]{
        if(!busy_||method_!="mark_saved"||!saveDestinationCommitted_||saveConfirmationAttempts_>=3)return;
        ++saveConfirmationAttempts_;
        if(process_.state()!=QProcess::Running||process_.write(saveConfirmationLine_)!=saveConfirmationLine_.size()){
            fail("文件已写入，但无法再次确认保存状态。",true);return;
        }
        if(saveConfirmationAttempts_<3)saveRetry_.start();
    });
    connect(&process_, &QProcess::started, this, [this] {
        if (!pendingLine_.isEmpty()) {
            process_.write(pendingLine_);
            pendingLine_.clear();
        }
    });
    connect(&process_, &QProcess::readyReadStandardOutput, this, &ModelSession::readOutput);
    connect(&process_, &QProcess::readyReadStandardError, this, [this] {
        diagnostics_.append(process_.readAllStandardError());
        diagnostics_ = diagnostics_.right(32768);
    });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError code) {
        if (!stopping_ && (busy_ || healthy_))
            fail(QString(uiText("计算组件无法运行：")) + process_.errorString(), code != QProcess::UnknownError);
    });
    connect(&process_, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        if (!stopping_ && (busy_ || healthy_))
            fail(QString(uiText("计算组件已退出。当前画面已保留。\n")) + QString::fromUtf8(diagnostics_), true);
    });
}

void ModelSession::pauseCalculationQueue(bool paused){calculationScheduler_->setPaused(paused);}
void ModelSession::abortCalculationForExit(){
    // Only an isolated calculation worker may be terminated. The document owner
    // must finish its atomic save/parameter commit before the application exits.
    if(!calculationSession_||recordingHydroRange()||recordingPerformanceParameters())return;
    stopping_=true;++parseGeneration_;parsing_=false;healthy_=false;busy_=false;
    afterCalculationLoadCancelled_=true;afterCalculationLoadMethod_.clear();
    timeout_.stop();saveRetry_.stop();
    QObject::disconnect(&process_,nullptr,this,nullptr);
    if(process_.state()!=QProcess::NotRunning)process_.kill();
    calculationScheduler_->release(this);notifyState();
}

ModelSession::~ModelSession() {
    stopping_ = true;
    calculationScheduler_->release(this);
    parsingPool_.waitForDone();
    timeout_.stop();
    saveRetry_.stop();
    QObject::disconnect(&process_, nullptr, this, nullptr);
    if (process_.state() != QProcess::NotRunning) {
        process_.write("{\"id\":0,\"method\":\"quit\"}\n");
        if (!process_.waitForFinished(1000)) {
            process_.kill();
            process_.waitForFinished(3000);
        }
    }
}

void ModelSession::atomicCopy(const QString& source, const QString& destination) {
    QFile input(source);
    QSaveFile output(destination);
    // QSaveFile keeps the old file intact if writing or committing fails.
    output.setDirectWriteFallback(false);
    if (!input.open(QIODevice::ReadOnly)) error(QString(uiText("无法读取文件：")) + input.errorString());
    if (!output.open(QIODevice::WriteOnly)) error(QString(uiText("无法写入文件：")) + output.errorString());
    while (!input.atEnd()) {
        const QByteArray bytes = input.read(1024 * 1024);
        if (input.error() != QFile::NoError || output.write(bytes) != bytes.size())
            error("文件写入未完成，原文件未被替换。");
    }
    if (!output.commit()) error(QString(uiText("无法完成文件保存：")) + output.errorString());
}

void ModelSession::notifyState() { if (onStateChanged) onStateChanged(); }

void ModelSession::startProcess() {
    const QString executable = fs::platform::siblingExecutable("FreeShipDocumentService");
    if (!QFileInfo::exists(executable)) error(QString(uiText("找不到计算组件：")) + executable);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString key : {"APPDATA", "LOCALAPPDATA", "TEMP", "TMP"}) {
        const QString folder = workspace_->path() + "/" + key;
        if (!QDir().mkpath(folder)) error("无法创建临时目录。");
        env.insert(key, folder);
    }
    process_.setProcessEnvironment(env);
    process_.setWorkingDirectory(workspace_->path());
    process_.setProgram(executable);
    process_.setArguments({"--serve",QString::number(QCoreApplication::applicationPid())});
    process_.setProcessChannelMode(QProcess::SeparateChannels);
#ifdef Q_OS_WIN
    process_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    buffer_.clear();
    diagnostics_.clear();
    process_.start();
}

bool ModelSession::load(const QString& file) {
    return loadInput(file,false,QStringLiteral("cp1252"));
}
bool ModelSession::calculationQueued()const{return calculationScheduler_->position(this)>0;}
int ModelSession::calculationQueuePosition()const{return calculationScheduler_->position(this);}
bool ModelSession::calculationActive()const{return calculationScheduler_->active(this);}
int ModelSession::activeCalculationCount()const{return calculationScheduler_->running();}
bool ModelSession::calculationSnapshotCurrent()const{
    return calculationSession_&&calculationOwner_&&model_&&calculationOwner_->model_&&
        model_->documentId==calculationOwner_->model_->documentId&&calculationOriginGeneration_==calculationOwner_->calculationGeneration_;
}
bool ModelSession::loadCalculationSnapshot(const ModelSession& source){
    if(busy_||!source.ready()||source.busy_)return false;
    try{
        if(source.calculationState_.isEmpty()||source.calculationBindings_.isEmpty())error("当前模型尚未提供计算快照，请重新打开模型。");
        if(!workspace_){workspace_=std::make_unique<QTemporaryDir>((source.temporaryDirectory_.isEmpty()?QDir::tempPath():source.temporaryDirectory_)+"/freeship-qt-calculation-XXXXXX");
            if(!workspace_->isValid())error("无法创建计算工作目录。");}
        const auto dir=workspace_->path()+"/snapshot-"+QString::number(nextId_+1);
        if(!QDir().mkpath(dir))error("无法创建计算快照目录。");
        atomicCopy(source.calculationState_,dir+"/state.fbm");atomicCopy(source.calculationBindings_,dir+"/bindings.json");
        calculationSession_=true;calculationOwner_=const_cast<ModelSession*>(&source);calculationScheduler_=source.calculationScheduler_;
        calculationOriginGeneration_=source.calculationGeneration_;calculationOriginParameterVersions_=source.calculationParameterVersions_;
        calculationState_=dir+"/state.fbm";calculationBindings_=dir+"/bindings.json";
        model_=source.model_;sourcePath_=source.sourcePath_;scene_=source.scene_;scenePath_=source.scenePath_;dxfPath_=source.dxfPath_;
        modelPath_=dir+"/model.json";atomicCopy(source.modelPath_,modelPath_);
        const auto nativeInput=QFileInfo(source.modelPath_).dir().filePath("native-input.json");
        if(QFileInfo::exists(nativeInput))atomicCopy(nativeInput,dir+"/native-input.json");
        calculationCacheRevision_=source.calculationCacheRevision_;
        preferences_=source.preferences_;temporaryDirectory_=source.temporaryDirectory_;pendingDocument_=model_->documentId;healthy_=false;
        importCalculationDependencies();
        return request("load_calculation",{{"path",calculationState_},{"bindings",calculationBindings_},{"document_id",model_->documentId},{"revision",double(model_->revision)},{"calculation_cache_revision",double(calculationCacheRevision_)}});
    }catch(const std::exception& e){fail(QString::fromUtf8(e.what()));return false;}
}
QString ModelSession::calculationCacheDirectory()const{
    if(!workspace_||!model_)return {};auto id=model_->documentId;id.remove('-');
    return workspace_->path()+"/performance-cache/doc-"+id+"-r"+QString::number(calculationCacheRevision_);
}
std::optional<double> ModelSession::accelerationResistanceMaximum()const{
    if(!calculationSnapshotCurrent())return {};
    const auto root=calculationOwner_->calculationCacheDirectory();
    const auto token=QString::fromLatin1(boundedBytes(root+"/current",256)).trimmed();
    if(!token.startsWith("set-{")||token.size()!=42||QUuid(token.mid(4)).isNull())return {};
    const auto path=root+'/'+token;
    const auto statePath=path+"/RESIST-acceleration.json";
    QJsonObject state;
    if(QFileInfo::exists(statePath)){
        state=QJsonDocument::fromJson(boundedBytes(statePath,16384)).object();
        if(state["version"]!=1||state["available"]!=true)return {};
    }
    const auto resistance=boundedBytes(path+(state["separate_table"]==true?"/RESIST-acceleration.dat":"/RESIST.dat"),65536);
    if(state.contains("resist_sha256")&&state["resist_sha256"].toString().toLatin1()!=cacheHash(resistance))return {};
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    const auto cells=QString::fromLatin1(resistance).split(whitespace,Qt::SkipEmptyParts);
    if(cells.size()!=25)return {};
    double lastSpeed=0;
    for(int i=0;i<25;++i){
        bool valid=false;
        const double number=cells[i].toFloat(&valid);
        if(!valid||!std::isfinite(number))return {};
        if(i%5==0){
            if(number<=lastSpeed)return {};
            lastSpeed=number;
        }else if(i%5==1&&number<0)return {};
    }
    const double sampled=state["speed_max_kn"].toDouble();
    return sampled>0&&std::isfinite(sampled)?sampled:lastSpeed;
}
void ModelSession::importCalculationDependencies(){
    if(!calculationSession_||!calculationSnapshotCurrent())return;
    const auto from=calculationOwner_->calculationCacheDirectory();calculationDependencyToken_=dependencyToken(from);
    copyDependencyGeneration(from,calculationCacheDirectory(),calculationDependencyToken_);
}
void ModelSession::publishCalculationDependencies(){
    if(!calculationSession_||!calculationSnapshotCurrent())return;
    const auto token=dependencyToken(calculationCacheDirectory());if(token.isEmpty()||token==calculationDependencyToken_)return;
    auto* owner=calculationOwner_.data();
    if(calculationOriginParameterVersions_.value(calculationMethod_)!=owner->calculationParameterVersions_.value(calculationMethod_))return;
    // A propeller job derived from an older resistance generation cannot replace
    // a newer resistance result that completed while it was running.
    const bool propeller=calculationMethod_.startsWith("propeller");
    if(propeller&&dependencyToken(owner->calculationCacheDirectory())!=calculationDependencyToken_)return;
    copyDependencyGeneration(calculationCacheDirectory(),owner->calculationCacheDirectory(),token);
    calculationDependencyToken_=token;
}
bool ModelSession::refreshCalculationSnapshot(const QString& method,const QJsonObject& fields){
    if(!calculationSession_||(calculationSnapshotCurrent()&&ready()))return false;
    if(!calculationOwner_||!calculationOwner_->ready()||calculationOwner_->busy())error("请等待模型更新完成后再计算。");
    if(!model_||model_->units!=calculationOwner_->model_->units)error("模型单位已改变，请重新打开计算窗口后输入参数。");
    afterCalculationLoadMethod_=method;afterCalculationLoadFields_=fields;afterCalculationLoadCancelled_=false;
    if(!loadCalculationSnapshot(*calculationOwner_)){afterCalculationLoadMethod_.clear();afterCalculationLoadFields_={};error("无法更新计算快照。");}
    return true;
}
bool ModelSession::cancelQueuedCalculation(){
    if(!calculationQueued())return false;
    const auto mode=hydroRunningMode_;const auto method=method_;
    calculationScheduler_->release(this);pendingLine_.clear();finish(method);
    if(method=="hydrostatics"){
        const QJsonObject result{{"mode",mode},{"cancelled",true},{"message",uiText("计算已取消。")},{"rows",QJsonArray{}},
            {"document_id",model_->documentId},{"revision",double(model_->revision)}};
        if(onHydrostatics)onHydrostatics(result);
    }else if(onError)onError(uiText("计算已取消。"));
    return true;
}
bool ModelSession::ensureDrawing() {return ready()&&!busy_&&(scene_||currentRequest("snapshot"));}
bool ModelSession::resumeCurrent() {
    if(!canResumeCurrent())return false;
    try {
        // A request can commit its checkpoint before the UI receives the reply.
        // Find the document's capsule even when recoveryFile_ is still empty.
        if(!recoveryRoot_.isEmpty() && QFileInfo::exists(recoveryRoot_+'/'+model_->documentId+".fsr"))
            return recover(model_->documentId);
        // Confirmation may have committed and removed the checkpoint before the
        // service exited. The receipt must still describe this exact revision.
        const bool committed=saveReceipt_["document_id"]==model_->documentId &&
            saveReceipt_["revision"].toDouble(-1)==double(model_->revision) &&
            saveReceipt_["bytes"].toDouble(-1)==double(sourceVersion_.bytes) &&
            saveReceipt_["sha256"].toString().toLatin1()==sourceVersion_.sha256.toHex();
        if(sourcePath_.isEmpty() || (modified_&&!committed))
            error("没有包含当前修改的完整恢复副本。当前画面和已有副本已保留，请勿放弃修改或覆盖原船模。");
        // Bind the expected version to the staged bytes actually loaded, not
        // an earlier path check that another writer could invalidate.
        return loadInput(sourcePath_,false,model_->textEncoding,&sourceVersion_);
    } catch(const std::exception& ex) {fail(QString::fromUtf8(ex.what()));return false;}
}
bool ModelSession::recover(const QString& id) {
    if(busy_)return false;
    try{
        auto lease=std::make_unique<fs::RecoveryLease>(recoveryRoot_,id);
        fs::RecoveryEntry entry;const auto bytes=lease->read(&entry);
        if(canResumeCurrent() && model_->documentId==id && entry.revision<model_->revision)
            error("恢复副本不包含当前画面的全部修改，未替换当前模型。副本仍保留；重新启动后可从恢复菜单找回较早版本。");
        QTemporaryDir stage((temporaryDirectory_.isEmpty()?QDir::tempPath():temporaryDirectory_)+"/freeship-recover-XXXXXX");
        if(!stage.isValid())error("无法准备恢复船模。");
        QSaveFile file(stage.filePath("model.fbm"));file.setDirectWriteFallback(false);
        if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())error("无法读取恢复船模到工作目录。");
        pendingRecovery_=std::move(lease);pendingRecoverySource_=entry.source;
        return loadInput(file.fileName(),true,entry.textEncoding);
    }catch(const std::exception& ex){fail(QString::fromUtf8(ex.what()));return false;}
}
bool ModelSession::discardRecovery(){
    if(busy_)return false;
    if(ready())return currentRequest("discard_recovery");
    try{
        if(model_&&!recoveryRoot_.isEmpty()){fs::RecoveryLease lease(recoveryRoot_,model_->documentId);lease.remove();}
        recoveryFile_.clear();recoveryError_.clear();
        if(onCompleted)onCompleted("discard_recovery");return true;
    }catch(const std::exception& ex){if(onError)onError(QString::fromUtf8(ex.what()));return false;}
}
bool ModelSession::loadInput(const QString& file,bool recovered,const QString& legacyEncoding,
                             const fs::ModelFileVersion* requiredVersion) {
    if (busy_ || file.isEmpty()) return false;
    QElapsedTimer startupClock;startupClock.start();
    setProperty("startup_profile",QJsonObject{});
    const auto stamp=[&](const char* key){auto p=property("startup_profile").toJsonObject();p[key]=startupClock.nsecsElapsed()/1e6;setProperty("startup_profile",p);};
    try {
        const QFileInfo info(file);
        if (!info.isFile() || !info.isReadable()) error(QString(uiText("找不到可读取的模型文件：")) + file);
        if (!workspace_) {
            workspace_ = std::make_unique<QTemporaryDir>((temporaryDirectory_.isEmpty()?QDir::tempPath():temporaryDirectory_) + "/freeship-qt-session-XXXXXX");
            if (!workspace_->isValid()) error("无法创建模型工作目录。");
        }
        const QString folder = workspace_->path() + "/input-" + QString::number(nextId_ + 1);
        if (!QDir().mkpath(folder)) error("无法创建模型工作目录。");
        const QString copy = folder + "/model.fbm";
        atomicCopy(info.absoluteFilePath(), copy);
        // Bind the editor to the bytes actually loaded, not to a later version
        // that another writer may have installed while the backend was loading.
        pendingSourceVersion_={};
        if(!recovered){
            const auto observed=fs::observeModelFile(info.absoluteFilePath());
            auto copied=fs::observeModelFile(copy);copied.path=observed.path;
            if(!fs::sameModelVersion(observed,copied))error("船模在打开过程中发生变化，请重新打开。");
            if(requiredVersion&&!fs::sameModelVersion(*requiredVersion,copied))
                error("原船模文件已被改动，未用它替换当前画面。请先检查文件及恢复副本。");
            pendingSourceVersion_=copied;
        }
        pendingInputDir_ = folder;
        pendingSource_ = recovered?QString{}:info.absoluteFilePath();
        stamp("source_staged_ms");
        pendingDocument_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QJsonObject fields{{"path",copy},{"document_id",pendingDocument_}};
        auto loadPreferences=preferences_;
        if(!legacyEncoding.isEmpty())loadPreferences["fbm_encoding"]=legacyEncoding;
        fields["preferences"]=loadPreferences;
        if(recovered)fields["recovered"]=true;
        setProperty("startup_cache_candidate",false);setProperty("startup_cache_hit",false);
        setProperty("startup_cache_key",QByteArray{});setProperty("startup_cache_root",QString{});
        QByteArray cachedModel;QJsonObject cacheMeta;
        if(!model_&&!recovered&&!property("disable_first_frame").toBool()&&!property("disable_startup_cache").toBool()){
            const QString base=linesCacheRoot_.isNull()?QStandardPaths::writableLocation(QStandardPaths::CacheLocation):linesCacheRoot_;
            const auto manifest=boundedBytes(QCoreApplication::applicationDirPath()+"/build-manifest.json",16*1024*1024);
            if(!base.isEmpty()&&!manifest.isEmpty()){
                const auto key=cacheHash(pendingSourceVersion_.sha256+cacheHash(manifest)+QJsonDocument(loadPreferences).toJson(QJsonDocument::Compact)+QLocale().name().toUtf8());
                const auto root=base+"/first-frame-v2";
                setProperty("startup_cache_key",key);setProperty("startup_cache_root",root);
                cacheMeta=QJsonDocument::fromJson(boundedBytes(root+"/entry.json",4096)).object();
                if(cacheMeta["version"]==2&&cacheMeta["key"].toString().toLatin1()==key){
                    cachedModel=boundedBytes(root+"/display.bin");
                    if(!cachedModel.isEmpty()&&cacheHash(cachedModel)==cacheMeta["display"].toString().toLatin1())setProperty("startup_cache_candidate",true);
                }
            }
        }
        const bool started=request("load",fields);
        stamp("cache_read_and_request_ms");
        if(started&&property("startup_cache_candidate").toBool()){
            // Backend work is already running while the GUI verifies the cached hull.
            try{
                auto model=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::fromDisplayCache(cachedModel,pendingDocument_));
                stamp("cache_model_parsed_ms");
                if(model->documentId!=pendingDocument_||model->revision!=1)throw std::runtime_error("cache identity");

                firstFrameSections_=cacheMeta["sections"].toString().toLatin1();firstFrameSurface_=cacheMeta["surface"].toString().toLatin1();firstFrame_=std::move(model);
                setProperty("startup_cache_hit",true);if(onFirstFrame)onFirstFrame(firstFrame_);
                stamp("cache_views_prepared_ms");
            }catch(const std::exception&){firstFrame_.reset();/* Optional cache: ordinary load continues. */}
        }
        return started;
    } catch (const std::exception& ex) {
        fail(QString::fromUtf8(ex.what()));
        return false;
    }
}

bool ModelSession::newHull(QJsonObject fields) {
    if(busy_)return false;
    try {
        if(!workspace_){workspace_=std::make_unique<QTemporaryDir>((temporaryDirectory_.isEmpty()?QDir::tempPath():temporaryDirectory_)+"/freeship-qt-session-XXXXXX");
            if(!workspace_->isValid())error("无法创建模型工作目录。");}
        if(model_){fields["expected_document_id"]=model_->documentId;fields["expected_revision"]=double(model_->revision);}
        pendingDocument_=QUuid::createUuid().toString(QUuid::WithoutBraces);fields["document_id"]=pendingDocument_;
        pendingSource_.clear();pendingInputDir_.clear();return request("new",fields);
    } catch(const std::exception& ex){fail(QString::fromUtf8(ex.what()));return false;}
}
bool ModelSession::currentRequest(const QString& method, QJsonObject fields) {
    if (busy_ || !ready()) return false;
    fields.insert("expected_document_id", model_->documentId);
    fields.insert("expected_revision", double(model_->revision));
    pendingDocument_ = model_->documentId;
    return request(method, fields);
}

bool ModelSession::importModel(const QJsonObject& exchange, bool append) {
    if(busy_)return false;
    try {
        if(!workspace_){workspace_=std::make_unique<QTemporaryDir>((temporaryDirectory_.isEmpty()?QDir::tempPath():temporaryDirectory_)+"/freeship-qt-session-XXXXXX");if(!workspace_->isValid())error("无法创建模型工作目录。");}
        const auto folder=workspace_->path()+"/input-"+QString::number(nextId_+1);
        if(!QDir().mkpath(folder))error("无法创建导入目录。");
        const auto path=folder+"/exchange.json";QSaveFile f(path);f.setDirectWriteFallback(false);
        const auto bytes=QJsonDocument(exchange).toJson(QJsonDocument::Compact);
        if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())error("无法准备导入数据。");
        QJsonObject fields{{"path",path}};if(model_){fields["expected_document_id"]=model_->documentId;fields["expected_revision"]=double(model_->revision);}
        if(append)return currentRequest("append_model",fields);
        pendingDocument_=QUuid::createUuid().toString(QUuid::WithoutBraces);fields["document_id"]=pendingDocument_;
        pendingSource_.clear();pendingInputDir_=folder;return request("import_model",fields);
    }catch(const std::exception& ex){fail(QString::fromUtf8(ex.what()));return false;}
}

bool ModelSession::movePoint(fs::EntityId id, fs::Vec3 position) {
    if (busy_ || !ready()) return false;
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
        fail("控制点坐标必须是有限数值。");
        return false;
    }
    return currentRequest("move_point", {{"point_id", double(id)},
        {"position", QJsonArray{position.x, position.y, position.z}}});
}
bool ModelSession::movePoints(const QVector<fs::PointMove>& moves) {
    if (busy_ || !ready() || moves.isEmpty()) return false;
    if (moves.size() == 1) return movePoint(moves.first().id, moves.first().position);
    QJsonArray values;
    for (const auto& move : moves) {
        const auto p = move.position;
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
            fail("控制点坐标必须是有限数值。");
            return false;
        }
        values.append(QJsonObject{{"point_id", double(move.id)}, {"position", QJsonArray{p.x, p.y, p.z}}});
    }
    return currentRequest("move_points", {{"moves", values}});
}
bool ModelSession::updateProject(const QJsonObject& changes) {
    return !changes.isEmpty() && currentRequest("update_project", {{"changes", changes}});
}
bool ModelSession::updateLayer(fs::EntityId id, const QJsonObject& changes) {
    return !changes.isEmpty() && currentRequest("update_layer", {{"layer_id", double(id)}, {"changes", changes}});
}
bool ModelSession::undo() {
    if(canUndoMovingBatch()){
        undoAfterMove_=model_;
        if(onBatchHistoryPreview)onBatchHistoryPreview(model_);
        notifyState();return true;
    }
    if(editingSections()){
        if(editorQueue_.size()>=512)return false;
        editorQueue_.append(QJsonObject{{"_queued_history","undo"}});notifyState();return true;
    }
    return canUndo() && currentRequest("undo");
}
bool ModelSession::editTopology(const QString& operation, QJsonObject fields) {
    fields.insert("operation", operation);
    return currentRequest("topology", fields);
}
bool ModelSession::redo() {
    if(editingSections()){
        if(editorQueue_.size()>=512)return false;
        editorQueue_.append(QJsonObject{{"_queued_history","redo"}});notifyState();return true;
    }
    return canRedo() && currentRequest("redo");
}
bool ModelSession::sectionPreviewHistory(bool redo) {
    const QString op=redo?"redo":"undo";
    if(busy_){
        if(!canQueueSectionEdits()||editorQueue_.size()>=512)return false;
        editorQueue_.append(QJsonObject{{"_queued_history",op},{"_section_preview",true}});return true;
    }
    return (redo?canRedo():canUndo())&&currentRequest(op,{{"_section_preview",true}});
}
bool ModelSession::layerListCommand(const QString& method,QJsonObject fields){
    fields["_layer_preview"]=true;
    if(method!="manage_layers"&&method!="update_layer"&&method!="undo"&&method!="redo")return false;
    if(busy_){
        if(!editingLayerList()||editorQueue_.size()>=512)return false;
        fields[method=="undo"||method=="redo"?"_queued_history":"_queued_method"]=method;
        editorQueue_.append(fields);return true;
    }
    if((method=="undo"&&!canUndo())||(method=="redo"&&!canRedo()))return false;
    return currentRequest(method,fields);
}
bool ModelSession::editSections(const QString& operation,int kind,QJsonObject fields) {
    fields.insert("operation",operation);fields.insert("kind",kind);
    if(editingSections()){
        // Once history is queued, the optimistic list no longer describes the
        // eventual revision. Do not accept new index-based edits against it.
        if(!canQueueSectionEdits()||editorQueue_.size()>=512)return false;
        editorQueue_.append(fields);return true;
    }
    return currentRequest("sections",fields);
}
bool ModelSession::editDisplay(const QJsonObject& changes) {return currentRequest("display",{{"changes",changes}});}
bool ModelSession::editAuxiliary(const QString& operation,QJsonObject fields){fields.insert("operation",operation);return currentRequest("auxiliary",fields);}
bool ModelSession::editAdvanced(const QString& operation,QJsonObject fields){fields.insert("operation",operation);return currentRequest("advanced",fields);}
bool ModelSession::editModeling(const QString& operation,QJsonObject fields){fields.insert("operation",operation);return currentRequest("modeling",fields);}
bool ModelSession::inspectModeling(){return currentRequest("inspect_modeling");}
bool ModelSession::hydroDefaults(){return currentRequest("hydro_defaults");}
bool ModelSession::developPlates(const QJsonValue& layerIds){
    if(busy_)return false;
    QJsonObject fields;if(!layerIds.isUndefined())fields["layer_ids"]=layerIds;
    try{if(refreshCalculationSnapshot("develop_plates",fields))return true;return currentRequest("develop_plates",fields);}
    catch(const std::exception& e){if(onError)onError(QString::fromUtf8(e.what()));return false;}
}
bool ModelSession::cadPatches(){return currentRequest("cad_iges");}
void ModelSession::cancelCad(){
    if(!busy_||method_!="cad_iges")return;
    QSaveFile flag(outputDir_+"/cancel.flag");
    if(!flag.open(QIODevice::WriteOnly)||flag.write("cancel")!=6||!flag.commit()) {if(onError)onError("无法提交取消请求。");}
    else cadCancelRequested_=true;
}
void ModelSession::cancelPlates(){
    if(method_=="load_calculation"&&afterCalculationLoadMethod_=="develop_plates"){afterCalculationLoadCancelled_=true;return;}
    if(!busy_||method_!="develop_plates")return;
    if(cancelQueuedCalculation())return;
    QSaveFile flag(outputDir_+"/cancel.flag");
    if(!flag.open(QIODevice::WriteOnly)||flag.write("cancel")!=6||!flag.commit()) {if(onError)onError("无法提交取消请求。");}
    else plateCancelRequested_=true;
}
bool ModelSession::performanceDefaults(QJsonObject fields){
    if(busy_)return false;
    try{if(!fields.value("development_sample").toBool())importCalculationDependencies();return currentRequest("performance_defaults",fields);}
    catch(const std::exception& e){if(onError)onError(QString::fromUtf8(e.what()));return false;}
}
bool ModelSession::calculatePerformance(QJsonObject fields){
    if(busy_)return false;
    try{if(refreshCalculationSnapshot("performance",fields))return true;if(!fields.value("development_sample").toBool())importCalculationDependencies();fields["model_name"]=sourcePath_;return currentRequest("performance",fields);}
    catch(const std::exception& e){if(onError)onError(QString::fromUtf8(e.what()));return false;}
}
bool ModelSession::savePerformanceSettings(QJsonObject fields){return currentRequest("performance_settings",fields);}
void ModelSession::cancelPerformance(){
    if(method_=="load_calculation"&&afterCalculationLoadMethod_=="performance"){afterCalculationLoadCancelled_=true;return;}
    if(!busy_||(method_!="performance"&&method_!="performance_defaults"))return;
    if(recordingPerformanceParameters()||cancelQueuedCalculation())return;
    if(method_=="performance")performanceCancelRequested_=true;
    QSaveFile flag(outputDir_+"/cancel.flag");
    if(!flag.open(QIODevice::WriteOnly)||flag.write("cancel")!=6||!flag.commit())if(onError)onError("无法提交取消请求。");
}
bool ModelSession::calculateHydrostatics(QJsonObject fields){
    if(busy_)return false;
    hydroRunningMode_=fields["mode"].toString();hydroProgress_={};hydroCancelRequested_=false;
    try{if(refreshCalculationSnapshot("hydrostatics",fields))return true;}
    catch(const std::exception& e){if(onError)onError(QString::fromUtf8(e.what()));return false;}
    if(busy_||!ready())return false;
    if(hydroRunningMode_=="cross"||(calculationSession_&&hydroRunningMode_!="auxiliary"))fields["report_progress"]=true;
    return currentRequest("hydrostatics",fields);
}
bool ModelSession::saveHydroSettings(QJsonObject fields){return currentRequest("hydro_settings",fields);}
void ModelSession::cancelHydrostatics(){
    if(method_=="load_calculation"&&afterCalculationLoadMethod_=="hydrostatics"){afterCalculationLoadCancelled_=true;return;}
    if(!busy_||method_!="hydrostatics")return;
    if(recordingHydroRange()||cancelQueuedCalculation())return;
    QSaveFile flag(outputDir_+"/cancel.flag");
    if(!flag.open(QIODevice::WriteOnly)||flag.write("cancel")!=6||!flag.commit()){
        if(onError)onError("无法提交取消请求。");
        return;
    }
    hydroCancelRequested_=true;
}
bool ModelSession::manageLayers(const QString& operation,QJsonObject fields) {
    fields.insert("operation",operation);return currentRequest("manage_layers",fields);
}
bool ModelSession::save(const QString& destination,const QJsonObject& calculationDrafts) {
    if (destination.isEmpty() || busy_ || !ready()) return false;
    try{
        if(QFileInfo(destination).suffix().compare("ftm",Qt::CaseInsensitive)==0)
            error("FTM 仅支持读取，请另存为 FBM 文件。");
        pendingSave_ = QFileInfo(destination).absoluteFilePath();
        pendingSaveVersion_=fs::sameModelFile(pendingSave_,sourcePath_)?sourceVersion_:fs::observeModelFile(pendingSave_);
        saveDestinationCommitted_ = false;lastBackupPath_.clear();
        saveReceipt_={};saveConfirmationLine_.clear();saveConfirmationAttempts_=0;
        QJsonObject fields;
        pendingSavedCalculators_=calculationDrafts.keys();
        if(!calculationDrafts.isEmpty())fields["calculation_drafts"]=calculationDrafts;
        // An untouched file is still copied byte for byte, including its JPEG.
        lastSavePreviewMs_=0;
        if((modified_||!calculationDrafts.isEmpty())&&previewEnabled_&&makeSavePreview){
            QElapsedTimer timer;timer.start();fields["preview_jpeg"]=QString::fromLatin1(makeSavePreview(model_).toBase64());
            lastSavePreviewMs_=timer.nsecsElapsed()/1e6;
        }
        return currentRequest("save",fields);
    }catch(const std::exception& ex){fail(QString::fromUtf8(ex.what()));return false;}
}

bool ModelSession::editBackground(int view,const QString& operation,QJsonObject fields){
    fields.insert("view",view);fields.insert("operation",operation);return currentRequest("background",fields);
}
bool ModelSession::importBackground(int view,const QString& filename,double scale){
    if(busy_||!ready())return false;
    try{
        const auto jpeg=fs::BackgroundImage::importFile(filename);
        const auto folder=workspace_->path()+"/request-"+QString::number(nextId_+1);
        if(!QDir().mkpath(folder))error("无法创建背景图工作目录。");
        const auto path=folder+"/input-background.jpg";QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly)||file.write(jpeg)!=jpeg.size()||!file.commit())error("无法准备背景图片。");
        return editBackground(view,"import",{{"path",path},{"scale",scale}});
    }catch(const std::exception& ex){fail(QString::fromUtf8(ex.what()));return false;}
}

bool ModelSession::request(const QString& method, QJsonObject fields) {
    if (busy_) return false;
    try {
        method_ = method;
        layerPreviewActive_=fields.take("_layer_preview").toBool();
        sectionPreviewActive_=fields.take("_section_preview").toBool();
        if(method=="performance")calculationMethod_=fields["calculator"].toString();
        if(calculationSession_&&(method=="performance"||method=="hydrostatics")&&calculationSnapshotCurrent()){
            const auto calculator=method=="performance"?calculationMethod_:QString("hydro_")+fields["mode"].toString();
            calculationOriginParameterVersions_[calculator]=calculationOwner_->calculationParameterVersions_.value(calculator);
        }
        pendingParameterCalculator_=method=="performance_settings"?fields["calculator"].toString():
            method=="hydro_settings"?QString("hydro_")+fields["mode"].toString():QString{};
        setProperty("publication_background",false);
        if(method=="performance")performanceCancelRequested_=false;
        if(method=="hydrostatics")hydroCancelRequested_=false;
        plateCancelRequested_ = false;
        cadCancelRequested_ = false;
        pendingId_ = ++nextId_;
        outputDir_ = workspace_->path() + "/request-" + QString::number(pendingId_);
        if (!QDir().mkpath(outputDir_)) error("无法创建计算结果目录。");
        fields.insert("id", double(pendingId_));
        fields.insert("method", method);
        fields.insert("output", outputDir_);
        fields.insert("native_linesplan", true);
        fields.insert("first_frame",method=="load"&&!model_&&!property("disable_first_frame").toBool()&&!property("startup_cache_candidate").toBool());
        fields.insert("defer_drawing",method=="load"&&!model_&&property("defer_startup_drawing").toBool());
        if(method=="move_points"&&QThread::idealThreadCount()>=4&&QFileInfo(modelPath_).size()>=2*1024*1024)
            fields["prepare_point_publication"]=true;
        if(!linesCacheRoot_.isNull())fields["lines_cache_root"]=linesCacheRoot_;
        if(method=="load" || method=="new" || method=="import_model"){
            if(!fields.contains("preferences"))fields.insert("preferences",preferences_);
            fields["recovery_root"]=recoveryRoot_;
            fields["recovery_source"]=pendingRecovery_?pendingRecoverySource_:pendingSource_;
        }
        busy_ = true;
        operationTimer_.start();
        timeout_.start(method=="hydrostatics"||method=="performance"?150000:120000);
        if((method=="hydrostatics"&&hydroRunningMode_=="cross")||calculationSession_)timeout_.stop();
        notifyState();
        const QByteArray line = QJsonDocument(fields).toJson(QJsonDocument::Compact) + '\n';
        if(calculationSession_&&(method=="hydrostatics"||method=="performance"||method=="develop_plates")){
            calculationScheduler_->submit(this,[this,line,method]{try{sendRequestLine(line,method);}catch(const std::exception& e){fail(QString::fromUtf8(e.what()),true);}},[this]{notifyState();});
        }else sendRequestLine(line,method);
        if(busy_ && !pendingMoveUndo_ && !sectionPreviewActive_ && !layerPreviewActive_ && onBatchHistoryPreview)
            if(auto target=batchHistoryTarget(method))onBatchHistoryPreview(std::move(target));
        return true;
    } catch (const std::exception& ex) {
        fail(QString::fromUtf8(ex.what()), true);
        return false;
    }
}
void ModelSession::sendRequestLine(const QByteArray& line,const QString& method){
    if(calculationSession_&&method=="performance"&&calculationSnapshotCurrent()){
        const QStringList resistance{"delft","kaper","holtrop","hollenbach","oortmerssen","fung","ost","rbhs","mh","planing"};
        if(resistance.contains(calculationMethod_)){
            const auto pointer=calculationOwner_->calculationCacheDirectory()+"/current";
            if(QFile::exists(pointer)&&!QFile::remove(pointer))error("无法更新阻力计算缓存。");
        }
    }
    if (process_.state() == QProcess::NotRunning) {
        if (method != "load" && method != "new" && method != "import_model"&&method!="load_calculation") error("计算组件已停止，请重新打开模型。");
        pendingLine_ = line;startProcess();
    } else if (process_.state() == QProcess::Starting) pendingLine_ = line;
    else if (process_.write(line) != line.size()) error("无法提交模型操作。");
}

void ModelSession::readOutput() {
    if(parsing_||property("first_frame_yield").toBool())return;
    buffer_ += process_.readAllStandardOutput();
    if (buffer_.size() > 1024 * 1024) {
        fail("计算组件的响应过长，已停止本次操作。", true);
        return;
    }
    qsizetype end;
    while ((end = buffer_.indexOf('\n')) >= 0) {
        const auto line = buffer_.left(end).trimmed();
        buffer_.remove(0, end + 1);
        if (line.isEmpty()) continue;
        QJsonParseError parse;
        const auto json = QJsonDocument::fromJson(line, &parse);
        if (parse.error != QJsonParseError::NoError || !json.isObject()) {
            diagnostics_ = (diagnostics_ + line + '\n').right(32768);
            continue;
        }
        try { receive(json.object());
            if(parsing_)return;
            if(json.object()["first_frame"].toBool()){
                setProperty("first_frame_yield",true);
                QTimer::singleShot(16,this,[this]{setProperty("first_frame_yield",false);readOutput();});return;
            }
        }
        catch (const std::exception& ex) { fail(QString::fromUtf8(ex.what()), true); }
    }
}

QString ModelSession::artifact(const QJsonObject& response, const QString& key) const {
    const QFileInfo file(response.value(key).toString());
    const QString canonical = file.canonicalFilePath();
    const QString allowed = QFileInfo(outputDir_).canonicalFilePath() + '/';
    if (canonical.isEmpty() || !file.isFile() || file.size() == 0 ||
        !canonical.startsWith(allowed, Qt::CaseInsensitive))
        error(QString(uiText("计算组件未返回完整的文件：")) + key);
    return canonical;
}

void ModelSession::readState(const QJsonObject& response) {
    if (!model_ || response.value("document_id").toString() != model_->documentId ||
        revision(response) != model_->revision) error("计算组件与界面的模型版本不一致。");
    for (const QString key : {"can_undo", "can_redo", "modified"})
        if (!response.value(key).isBool()) error("计算组件返回了不完整的操作状态。");
    layerIdHighWater_=response.value("layer_id_high_water").toInt();
    historyStates_ = response.value("history_states").toInt(-1);
    canUndo_ = response.value("can_undo").toBool();
    canRedo_ = response.value("can_redo").toBool();
    modified_ = response.value("modified").toBool();
    previewEnabled_ = response.value("preview_enabled").toBool();
    if(response["calculation_cache_revision"].isDouble())calculationCacheRevision_=quint64(response["calculation_cache_revision"].toDouble());
}

void ModelSession::publishSnapshot(const QJsonObject& response,std::shared_ptr<PreparedSnapshot> prepared) {
    for (const QString key : {"can_undo", "can_redo", "modified"})
        if (!response.value(key).isBool()) error("计算组件返回了不完整的操作状态。");
    QElapsedTimer clock;clock.start();
    lastLoadTimings_=reportTimings(response);
    const auto rollback=method_=="undo"?pendingMoveUndo_:nullptr;
    if(!prepared){
        QJsonObject paths;for(const char* key:{"model","scene","dxf","sections","surface","native_report"})
            if(!response["drawing_deferred"].toBool()||(QByteArray(key)!="scene"&&QByteArray(key)!="dxf"))paths[key]=artifact(response,key);
        prepared=std::make_shared<PreparedSnapshot>(prepareSnapshot(response,paths,pendingDocument_,method_,model_,scene_,firstFrame_,sectionDigest_,surfaceDigest_,firstFrameSections_,firstFrameSurface_));
    }
    auto nextModel=prepared->model;auto nextScene=prepared->scene;
    if(response.contains("calculation_state")){
        calculationState_=artifact(response,"calculation_state");calculationBindings_=artifact(response,"calculation_bindings");
    }
    if(!model_||nextModel->documentId!=model_->documentId){++calculationGeneration_;calculationParameterVersions_.clear();}
    else if(nextModel->revision!=model_->revision){
        if(method_=="performance_settings"||method_=="hydro_settings")++calculationParameterVersions_[pendingParameterCalculator_];
        else ++calculationGeneration_;
    }
    const auto modelFile=prepared->modelFile,sceneFile=prepared->sceneFile,dxfFile=prepared->dxfFile;
    const auto sectionsHash=prepared->sectionsHash,surfaceHash=prepared->surfaceHash;
    const auto sectionData=prepared->sectionData,surfaceData=prepared->surfaceData;
    const bool metadata=prepared->metadata,reusedFirstFrame=prepared->reusedFirstFrame,sameSections=prepared->sameSections,sameSurface=prepared->sameSurface;
    drawingUnchanged_=reusedFirstFrame||(metadata&&sameSections&&sameSurface&&nextScene==scene_);
    const bool replay=response["move_history_reused"].toBool();
    if(replay)drawingUnchanged_=false; // A verified historical frame differs from the current view.
    if(!rollback&&(method_=="move_point"||method_=="move_points"||replay)&&model_&&
       nextModel->revision>model_->revision&&model_->documentId==nextModel->documentId&&
       !model_->isCachedPreview()&&!model_->sourceDigest().isEmpty()&&
       QFileInfo(modelPath_).size()+QFileInfo(scenePath_).size()+QFileInfo(QFileInfo(modelPath_).dir(),"surface.json").size()+QFileInfo(QFileInfo(modelPath_).dir(),"sections.json").size()<=64LL*1024*1024){
        batchHistoryDirection_=method_=="undo"?"redo":"undo";
        batchHistoryModel_=model_;batchHistoryScene_=scene_;batchHistorySections_=sectionDigest_;batchHistorySurface_=surfaceDigest_;
    }else{batchHistoryModel_.reset();batchHistoryScene_.reset();batchHistorySections_.clear();batchHistorySurface_.clear();}
    sectionDigest_=sectionsHash;surfaceDigest_=surfaceHash;
    lastLoadTimings_["ui_metadata_reused"]=metadata;lastLoadTimings_["ui_scene_reused"]=nextScene==scene_;
    lastLoadTimings_["ui_surface_reused"]=sameSurface;lastLoadTimings_["ui_drawing_unchanged"]=drawingUnchanged_;
    lastLoadTimings_["ui_parse_ms"]=prepared->parseMs;lastLoadTimings_["ui_parse_background"]=property("publication_background").toBool();
    lastLoadTimings_["ui_first_frame_reused"]=reusedFirstFrame&&!replay;
    lastLoadTimings_["ui_batch_history_reused"]=reusedFirstFrame&&replay;
    lastLoadTimings_["startup_cache_hit"]=property("startup_cache_hit").toBool();
    lastLoadTimings_["drawing_deferred"]=response["drawing_deferred"].toBool();
    if(method_=="load"&&!property("startup_cache_key").toByteArray().isEmpty()){
        const auto root=property("startup_cache_root").toString();const auto key=property("startup_cache_key").toByteArray();
        const auto bytes=prepared->modelData;const auto document=nextModel->documentId;
        QThreadPool::globalInstance()->start([root,key,nextModel,bytes,sectionData,surfaceData]{try{saveFirstFrameCache(root,key,nextModel,bytes,sectionData,surfaceData);}catch(const std::exception&){/* Optional cache. */}});
    }
    model_ = std::move(nextModel);
    firstFrame_.reset();
    scene_ = std::move(nextScene);
    modelPath_ = modelFile;
    scenePath_ = sceneFile;
    dxfPath_ = dxfFile;
    if (method_ == "load" || method_ == "new" || method_ == "import_model") {
        sourcePath_ = pendingSource_;
        sourceVersion_=method_=="load"&&!pendingRecovery_?pendingSourceVersion_:fs::ModelFileVersion{};
        lastBackupPath_.clear();
        recoveryOrigin_=pendingRecovery_?pendingRecoverySource_:QString{};
        currentInputDir_ = pendingInputDir_;
        pendingInputDir_.clear();
    }
    readState(response);
    healthy_ = true;
    if(rollback){
        // Release the optimistic old drawing only after the restored files
        // have passed the regular publication checks.
        rollback->restoredRevision=model_->revision;
        undoAfterMove_.reset();pendingMoveUndo_.reset();
        keepPreparedMoveForRedo(*rollback);
    }
    if (onUpdated) onUpdated();
    lastLoadTimings_["ui_publish_ms"]=clock.nsecsElapsed()/1e6;
    if(method_=="move_points"&&property("batch_move_latency_probe").toBool())setProperty("batch_move_load_timings",lastLoadTimings_);
}

void ModelSession::submitQueuedMoveUndo(const QJsonObject& move,const QJsonObject& paths){
    auto pending=std::move(pendingMoveUndo_);
    const bool preparing=pending&&pending->candidateRevision==revision(move)&&pending->candidatePaths==paths;
    if(!preparing)pending=std::make_shared<PendingMoveUndo>();
    pending->receipt=move;pending->output=outputDir_;pendingMoveUndo_=pending;
    const auto document=pendingDocument_;
    if(property("batch_move_latency_probe").toBool()){
        auto timings=reportTimings(move);timings["ui_parse_ms"]=0;timings["ui_publish_ms"]=0;
        timings["ui_move_skipped_for_undo"]=true;
        timings["ui_move_preparse_started"]=preparing;
        timings["ui_move_preparse_ready"]=bool(pending->prepared);
        setProperty("batch_move_load_timings",timings);
    }
    // The service has committed and validated this exact move. Advance with
    // its acknowledgement, without replacing the drawing just to undo it.
    busy_=false;
    if(!request("undo",{{"expected_document_id",document},{"expected_revision",move["revision"]}}))return;
    // Optional parsing never gates undo. Older services can still prepare
    // after acknowledgement, and redo can always load its authoritative files.
    if(!preparing)pending->prepare(this,pending,move,paths);
}
void ModelSession::keepPreparedMoveForRedo(const PendingMoveUndo& pending){
    if(!pending.prepared||!canRedo()||!pending.restoredRevision||model_->revision!=pending.restoredRevision||
       model_->documentId!=pending.prepared->model->documentId)return;
    batchHistoryDirection_="redo";
    batchHistoryModel_=pending.prepared->model;batchHistoryScene_=pending.prepared->scene;
    batchHistorySections_=pending.prepared->sectionsHash;batchHistorySurface_=pending.prepared->surfaceHash;
}
void ModelSession::recoverRejectedMoveUndo(const QJsonObject& response){
    auto pending=std::move(pendingMoveUndo_);
    const auto issue=response.value("error").toString("模型操作失败。");
    if(response["document_id"]!=pending->receipt["document_id"]||response["revision"]!=pending->receipt["revision"]){
        fail(issue,true);return;
    }
    // Undo did not commit. Restore the accepted move and report the error
    // after validating its files, so UI and document authority agree again.
    undoAfterMove_.reset();moveUndoRejection_=issue;
    method_="move_points";outputDir_=pending->output;pendingId_=qint64(pending->receipt["id"].toDouble());
    setProperty("publication_background",false);
    if(pending->prepared){publishSnapshot(pending->receipt,pending->prepared);completePublication();}
    else receive(pending->receipt);
}
void ModelSession::receive(const QJsonObject& response) {
    // A timed-out acknowledgement may arrive after its successful retry. It
    // belongs to a completed request and must never overwrite newer UI state.
    if(response.value("id").toDouble(-1)==double(lastSaveConfirmationId_)&&lastSaveConfirmationId_>=0)return;
    if (!busy_ || response.value("id").toDouble(-1) != double(pendingId_))
        error("计算组件返回了不对应当前操作的响应。");
    if (!response.value("ok").isBool()) error("计算组件返回了无效的操作结果。");
    if (!response.value("ok").toBool()) {
        if(method_=="undo"&&pendingMoveUndo_){recoverRejectedMoveUndo(response);return;}
        fail(response.value("error").toString("模型操作失败。"));
        return;
    }
    if (response.value("document_id").toString() != pendingDocument_)
        error("计算组件返回了另一个文档的结果。");
    if(response["point_publication_candidate"].toBool()){
        if(method_!="move_points"||!undoAfterMove_||pendingMoveUndo_||revision(response)!=undoAfterMove_->revision+1)return;
        // Never show this drawing or change history from a provisional notice.
        // If even one file is unavailable, wait for the authoritative reply.
        try{
            QJsonObject paths;
            for(const auto* key:{"model","scene","dxf","sections","surface","native_report"})paths[key]=artifact(response,key);
            auto pending=std::make_shared<PendingMoveUndo>();
            pending->prepare(this,pending,response,paths);
            if(pending->candidateRevision)pendingMoveUndo_=std::move(pending);
        }catch(const std::exception&){/* Optional, final reply is checked separately. */}
        return;
    }
    if(response["hydro_progress"].toBool()){
        if(method_!="hydrostatics"||!model_||revision(response)!=model_->revision)error("Invalid calculation progress identity");
        const auto done=response["completed"].toInt(-1),total=response["total"].toInt(-1);
        if(done<0||total<1||done>total||total>500||done<hydroProgress_["completed"].toInt())error("Invalid calculation progress");
        hydroProgress_=QJsonObject{{"completed",done},{"total",total}};
        if(onHydroProgress)onHydroProgress(hydroProgress_);return;
    }
    if(method_=="load_calculation"){
        if(!response["calculation_snapshot"].toBool())error("Invalid calculation snapshot acknowledgement");
        readState(response);healthy_=true;
        const auto next=afterCalculationLoadMethod_;const auto fields=afterCalculationLoadFields_;
        afterCalculationLoadMethod_.clear();afterCalculationLoadFields_={};
        if(afterCalculationLoadCancelled_&&!next.isEmpty()){
            afterCalculationLoadCancelled_=false;finish(next);
            if(next=="hydrostatics"){
                const QJsonObject data{{"mode",fields["mode"]},{"cancelled",true},{"message",uiText("计算已取消。")},{"rows",QJsonArray{}},
                    {"document_id",model_->documentId},{"revision",double(model_->revision)}};
                if(onHydrostatics)onHydrostatics(data);
            }else if(onError)onError(uiText("计算已取消。"));return;
        }
        if(!next.isEmpty()){
            busy_=false;method_.clear();
            if(next=="hydrostatics")calculateHydrostatics(fields);else if(next=="develop_plates")developPlates(fields.value("layer_ids"));else calculatePerformance(fields);
        }else finish("calculation_ready");return;
    }
    if(response["first_frame"].toBool()) {
        if(method_!="load"||model_||firstFrame_)error("Unexpected first-frame response");
        QJsonObject paths;for(const char* key:{"model","sections","surface","native_report"})paths[key]=artifact(response,key);
        const auto request=pendingId_,generation=++parseGeneration_;const auto document=pendingDocument_;
        parsing_=true;
        parsingPool_.start([this,response,paths,request,generation,document]{
            std::shared_ptr<fs::ModelSnapshot> ready;QByteArray sectionsHash,surfaceHash;QString issue;
            try{
                const auto path=[&](const char* key){return paths[key].toString();};
        auto model=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::load(path("model")));
        if(model->documentId!=document||model->revision!=revision(response))error("First-frame model identity mismatch");
        QFile reportFile(path("native_report"));
        if(reportFile.size()>1024*1024||!reportFile.open(QIODevice::ReadOnly))error("Invalid first-frame report");
        const auto report=QJsonDocument::fromJson(reportFile.readAll()).object();
        if(report["status"]!="pass"||report["engine"]!="cpp-native"||report["document_id"]!=document||revision(report)!=model->revision)error("Invalid first-frame provenance");
        auto readChecked=[&](const char* key){QFile f(path(key));
            if(f.size()>256LL*1024*1024||!f.open(QIODevice::ReadOnly))error("Invalid first-frame artifact");
            auto bytes=f.readAll();const auto hash=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();
            if(report[QString(key)+"_sha256"].toString().toLatin1()!=hash)error("First-frame checksum mismatch");return bytes;};
        const auto sections=readChecked("sections"),surface=readChecked("surface");
        model->loadSectionDisplay(sections);model->loadSurfaceDisplay(surface);

                ready=std::move(model);sectionsHash=report["sections_sha256"].toString().toLatin1();surfaceHash=report["surface_sha256"].toString().toLatin1();
            }catch(const std::exception& ex){issue=QString::fromUtf8(ex.what());}
            QMetaObject::invokeMethod(this,[this,request,generation,ready,sectionsHash,surfaceHash,issue]{
                if(stopping_||!busy_||request!=pendingId_||generation!=parseGeneration_)return;
                parsing_=false;if(!issue.isEmpty()){fail(issue,true);return;}
                firstFrame_=ready;firstFrameSections_=sectionsHash;firstFrameSurface_=surfaceHash;
                if(onFirstFrame)onFirstFrame(firstFrame_);
                QTimer::singleShot(0,this,[this]{readOutput();});
            },Qt::QueuedConnection);
        });
        return;
    }
    recoveryError_=response["recovery_error"].toString();
    recoveryFile_=response["recovery_file"].toString();
    if(method_=="discard_recovery"){readState(response);finish(method_);return;}
    if(method_=="hydrostatics"){
        readState(response);QFile file(artifact(response,"hydro"));
        if(!file.open(QIODevice::ReadOnly)||file.size()>32*1024*1024)error("无法读取静水力报告。");
        QJsonParseError parse;const auto document=QJsonDocument::fromJson(file.readAll(),&parse);auto data=document.object();
        if(parse.error!=QJsonParseError::NoError||!data["rows"].isArray()||data["document_id"].toString()!=model_->documentId||revision(data)!=model_->revision)
            error("静水力报告格式或模型版本不一致。");
        const auto mode=data["mode"].toString();
        if((mode=="single"||mode=="table"||mode=="cross")&&!data["cancelled"].toBool()){
            if(hydroCancelRequested_){
                data["cancelled"]=true;data["message"]=uiText("计算已取消。");
            }else{
                const auto rows=data["rows"].toArray();
                const bool succeeded=!rows.isEmpty()&&std::all_of(rows.begin(),rows.end(),[](const auto& value){
                    const auto row=value.toObject();
                    return row["status"]=="ok"&&!row["partial"].toBool();
                });
                if(succeeded){
                    auto range=data["inputs"].toObject();
                    if(mode=="table")range["exact_table_range"]=true;
                    else{
                        range.remove("mode");range.remove("model_name");
                        range={{"mode",mode},{"calculator",QString("hydro_")+mode},{"values",range},
                            {"snapshot_units",model_->units},{"complete_snapshot",true}};
                    }
                    if(calculationSession_){recordCalculationResult("hydro_settings",range,data);return;}
                    pendingHydro_=data;
                    busy_=false;
                    saveHydroSettings(range);
                    return;
                }
            }
        }
        finish(method_);if(onHydrostatics)onHydrostatics(data);return;
    }
    if(method_=="hydro_settings"&&!pendingHydro_.isEmpty()){
        publishSnapshot(response);
        auto data=std::move(pendingHydro_);pendingHydro_={};
        data["calculated_revision"]=data["revision"];data["revision"]=double(model_->revision);
        data["parameters_recorded"]=true;
        finish("hydrostatics");if(onHydrostatics)onHydrostatics(data);return;
    }
    if(method_=="performance"||method_=="performance_defaults"){
        const bool defaults=method_=="performance_defaults";
        if(!defaults&&performanceCancelRequested_){fail("计算已取消，未更改船模中的计算参数。");return;}
        if(!model_||response["revision"].toDouble()!=double(model_->revision))error("计算报告与当前模型版本不一致。");
        readState(response);
        const auto path=artifact(response,"performance");const auto request=pendingId_,generation=++parseGeneration_;
        const auto document=model_->documentId;const auto version=model_->revision;
        parsing_=true;
        parsingPool_.start([this,path,request,generation,document,version,defaults]{
            QJsonObject data;QString issue;QElapsedTimer timer;timer.start();
            try{
                QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>128*1024*1024)error("无法读取计算报告。");
                QJsonParseError parse;data=QJsonDocument::fromJson(file.readAll(),&parse).object();
                if(parse.error!=QJsonParseError::NoError||!data[defaults?"fields":"series"].isArray()||data["document_id"].toString()!=document||revision(data)!=version)error("计算报告格式或模型版本不一致。");
            }catch(const std::exception& ex){issue=QString::fromUtf8(ex.what());}
            const double elapsed=timer.nsecsElapsed()/1e6;
            QMetaObject::invokeMethod(this,[this,request,generation,data,issue,defaults,elapsed]{
                if(stopping_||!busy_||request!=pendingId_||generation!=parseGeneration_)return;
                parsing_=false;setProperty("performance_parse_ms",elapsed);
                if(!issue.isEmpty()){fail(issue);return;}
                if(!defaults&&performanceCancelRequested_){fail("计算已取消，未更改船模中的计算参数。");return;}
                try{completePerformance(data,defaults);}catch(const std::exception& ex){fail(QString::fromUtf8(ex.what()));}
                QTimer::singleShot(0,this,[this]{readOutput();});
            },Qt::QueuedConnection);
        });return;
    }
    if(method_=="performance_settings"&&!pendingPerformance_.isEmpty()){
        publishSnapshot(response);
        auto data=std::move(pendingPerformance_);pendingPerformance_={};
        data["calculated_revision"]=data["revision"];data["revision"]=double(model_->revision);data["parameters_recorded"]=true;
        finish("performance");if(onPerformance)onPerformance(data,false);return;
    }
    if(method_=="cad_iges"){
        if(cadCancelRequested_) {fail("IGES 曲面准备已取消。");return;}
        readState(response);QFile file(artifact(response,"cad"));
        if(!file.open(QIODevice::ReadOnly)||file.size()>256LL*1024*1024)error("无法读取曲面数据。");
        QJsonParseError parse;const auto data=QJsonDocument::fromJson(file.readAll(),&parse).object();
        if(parse.error!=QJsonParseError::NoError||!data["patches"].isArray()||data["document_id"].toString()!=model_->documentId||revision(data)!=model_->revision)error("曲面数据格式或模型版本不一致。");
        finish(method_);if(onCad)onCad(data);return;
    }
    if(method_=="develop_plates"){
        if(!model_||response["revision"].toDouble()!=double(model_->revision))error("展开结果与当前模型版本不一致。");
        // The worker may have committed a small result before cancel.flag was
        // written. Cancellation while the UI is still busy also rejects that
        // queued success, preserving the user's previous layout.
        if(plateCancelRequested_) {fail("板材展开已取消。");return;}
        readState(response);QFile file(artifact(response,"plates"));
        if(!file.open(QIODevice::ReadOnly)||file.size()>128*1024*1024)error("无法读取板材展开数据。");
        QJsonParseError parse;const auto data=QJsonDocument::fromJson(file.readAll(),&parse).object();
        if(parse.error!=QJsonParseError::NoError||!data["plates"].isArray()||data["document_id"].toString()!=model_->documentId||revision(data)!=model_->revision)error("展开数据格式或模型版本不一致。");
        finish(method_);if(onPlates)onPlates(data);return;
    }
    if(method_=="inspect_modeling"||method_=="hydro_defaults"){
        if(!response["analysis"].isObject()||!model_||response["revision"].toDouble()!=double(model_->revision))error("分析结果不属于当前模型版本。");
        const auto analysis=response["analysis"].toObject();const auto method=method_;finish(method_);
        if(method=="hydro_defaults"){if(onHydroDefaults)onHydroDefaults(analysis);}else if(onAnalysis)onAnalysis(analysis);return;
    }
    if (method_ == "save") {
        readState(response);
        const QString saved = artifact(response, "saved_file");
        try {
            saveReceipt_=response["save_receipt"].toObject();
            const auto sha=saveReceipt_["sha256"].toString().toLatin1();
            const auto bytes=saveReceipt_["bytes"].toDouble(-1);
            if(saveReceipt_["version"]!=1||QUuid(saveReceipt_["token"].toString()).isNull()||
               saveReceipt_["document_id"]!=model_->documentId||revision(saveReceipt_)!=model_->revision||
               bytes<1||bytes>512LL*1024*1024||std::floor(bytes)!=bytes||sha.size()!=64||
               QByteArray::fromHex(sha).toHex()!=sha)error("计算组件未返回有效的保存凭据；未覆盖目标文件。");
            const fs::ModelFileVersion prepared{fs::modelFilePath(saved),true,qint64(bytes),QByteArray::fromHex(sha)};
            fs::ModelFileSave transaction(saved,pendingSave_,pendingSaveVersion_,prepared);
            const auto result=transaction.commit();sourceVersion_=result.version;lastBackupPath_=result.backup;
        }
        catch (const std::exception& ex) { fail(QString::fromUtf8(ex.what())); return; }
        sourcePath_ = pendingSave_;
        saveDestinationCommitted_ = true;
        if(onFileCommitted)onFileCommitted(sourcePath_);
        // Mark clean only after the real destination was committed successfully.
        method_ = "mark_saved";
        pendingId_ = ++nextId_;
        const QJsonObject mark{{"id", double(pendingId_)}, {"method", method_},
            {"expected_document_id", model_->documentId}, {"expected_revision", double(model_->revision)},
            {"save_receipt",saveReceipt_},{"saved_path",sourcePath_}};
        saveConfirmationLine_=QJsonDocument(mark).toJson(QJsonDocument::Compact)+'\n';saveConfirmationAttempts_=1;
        if (process_.write(saveConfirmationLine_) != saveConfirmationLine_.size()) error("文件已写入，但无法确认保存状态。请重新打开已保存的文件。");
        saveRetry_.start();
        return;
    }
    if (method_ == "mark_saved") {
        if(response["save_confirmed"]!=true||response["save_receipt"].toObject()!=saveReceipt_||
           !fs::sameModelFile(response["saved_path"].toString(),sourcePath_)||response["modified"]!=false)
            error("保存确认与本次文件不一致；当前修改仍保留。");
        readState(response);
        if(response.contains("calculation_state")){
            calculationState_=artifact(response,"calculation_state");
            calculationBindings_=artifact(response,"calculation_bindings");
            for(const auto& calculator:pendingSavedCalculators_)++calculationParameterVersions_[calculator];
        }
        pendingSavedCalculators_.clear();
        sourcePath_ = pendingSave_;
        recoveryOrigin_.clear();
        saveDestinationCommitted_ = false;
        lastSaveConfirmationId_=pendingId_;
        finish("save");
        return;
    }
    if(method_=="move_point"||method_=="move_points"||(method_=="topology"&&response["point_placement"]==true)||method_=="manage_layers"||method_=="update_layer"||method_=="load"||method_=="snapshot"||method_=="sections"||method_=="undo"||method_=="redo"){
        QJsonObject paths;for(const char* key:{"model","scene","dxf","sections","surface","native_report"})
            if(!response["drawing_deferred"].toBool()||(QByteArray(key)!="scene"&&QByteArray(key)!="dxf"))paths[key]=artifact(response,key);
        if(method_=="move_points"&&undoAfterMove_&&response["document_engine"]=="cpp-document"&&
           revision(response)==undoAfterMove_->revision+1&&response["can_undo"]==true&&
           response["modified"].isBool()&&response["can_redo"].isBool()){
            submitQueuedMoveUndo(response,paths);return;
        }
        if(method_=="move_points")pendingMoveUndo_.reset();
        const auto document=pendingDocument_,method=method_;const auto request=pendingId_,generation=++parseGeneration_;
        const bool rollback=method_=="undo"&&pendingMoveUndo_;
        if(rollback&&(revision(response)!=revision(pendingMoveUndo_->receipt)+1||response["can_redo"]!=true))
            error("批量移动的撤销结果与当前模型版本不一致。");
        const auto replay=response["move_history_reused"].toBool()?batchHistoryTarget(method):nullptr;
        const auto history=rollback?undoAfterMove_:(replay&&replay->documentId==document?replay:nullptr);
        const auto model=model_;const auto scene=history&&!rollback?batchHistoryScene_:scene_;
        const auto first=method_=="snapshot"?std::const_pointer_cast<fs::ModelSnapshot>(model_):firstFrame_;
        const auto sections=sectionDigest_,surface=surfaceDigest_;
        const auto firstSections=rollback||method_=="snapshot"?sections:history?batchHistorySections_:firstFrameSections_;
        const auto firstSurface=rollback||method_=="snapshot"?surface:history?batchHistorySurface_:firstFrameSurface_;
        parsing_=true;setProperty("publication_background",true);
        parsingPool_.start([this,response,paths,document,method,request,generation,model,scene,history,first,sections,surface,firstSections,firstSurface]{
            std::shared_ptr<PreparedSnapshot> prepared;QString issue;
            try{
                auto predicted=first;
                if(history){
                    const auto value=response["history_layer_number"];
                    const std::optional<int> layer=value.isDouble()?std::optional<int>{value.toInt(-2)}:std::nullopt;
                    predicted=std::make_shared<fs::ModelSnapshot>(history->withMetadata({},document,revision(response),layer));
                }
                prepared=std::make_shared<PreparedSnapshot>(prepareSnapshot(response,paths,document,method,model,scene,predicted,sections,surface,firstSections,firstSurface));
            }
            catch(const std::exception& ex){issue=QString::fromUtf8(ex.what());}
            QMetaObject::invokeMethod(this,[this,response,request,generation,prepared,issue]{
                if(stopping_||!busy_||pendingId_!=request||parseGeneration_!=generation)return;
                parsing_=false;
                if(!issue.isEmpty()){fail(issue,true);return;}
                try{publishSnapshot(response,prepared);completePublication();readOutput();}
                catch(const std::exception& ex){fail(QString::fromUtf8(ex.what()),true);}
            },Qt::QueuedConnection);
        });return;
    }
    publishSnapshot(response);completePublication();
}
void ModelSession::completePublication(){
    if(pendingRecovery_){
        // Retire the crash copy only once the recovered revision has its own
        // durable copy. Failed parsing/publication/writes preserve the original.
        if(!recoveryFile_.isEmpty())try{pendingRecovery_->remove();}catch(const std::exception& ex){recoveryError_=QString::fromUtf8(ex.what());}
        pendingRecovery_.reset();pendingRecoverySource_.clear();
    }
    finish(method_);
}

void ModelSession::completePerformance(const QJsonObject& data,bool defaults){
        if(defaults||data["development_sample"]==true){finish(method_);if(onPerformance)onPerformance(data,defaults);return;}
        if(calculationSession_)publishCalculationDependencies();
        QJsonObject values;
        for(const auto& entry:data["parameters"].toObject()["fields"].toArray()){
            const auto field=entry.toObject();const auto key=field["key"].toString();
            if(key.isEmpty()||!field.contains("value"))error("计算结果缺少完整的输入参数。");
            values.insert(key,field["value"]);
        }
        if(values.isEmpty()||data["calculator"].toString().isEmpty())error("计算结果缺少完整的输入参数。");
        pendingPerformance_=data;
        // Continue the same user operation without publishing an idle interval.
        // Only the immutable, successfully evaluated inputs are committed; UI
        // edits, defaults, failed solvers and cancelled results never reach here.
        QJsonObject record{{"calculator",data["calculator"]},{"values",values},{"complete_snapshot",true},
            {"snapshot_units",model_->units},{"expected_document_id",model_->documentId},{"expected_revision",double(model_->revision)}};
        if(calculationSession_){pendingPerformance_={};recordCalculationResult("performance_settings",record,data);return;}
        busy_=false;request("performance_settings",record);return;
}

void ModelSession::finish(const QString& operation) {
    const QString completed = operation;
    lastOperationMs_ = operationTimer_.nsecsElapsed() / 1e6;
    timeout_.stop();
    saveRetry_.stop();saveConfirmationLine_.clear();
    busy_ = false;
    method_.clear();
    calculationScheduler_->release(this);
    if(recordingCalculation_)completeCalculationRecord(true);
    discardOldArtifacts();
    if(completed=="move_points"&&undoAfterMove_){
        const auto before=std::move(undoAfterMove_);
        if(model_&&before->documentId==model_->documentId&&model_->revision==before->revision+1&&canUndo()){
            if(!currentRequest("undo"))fail("Queued batch undo could not be submitted");
            return;
        }
        // No new history entry: never undo an older, unrelated change.
        if(onUpdated)onUpdated();
    }
    while(!editorQueue_.isEmpty()){
        auto next=editorQueue_.takeFirst();const auto method=next.take("_queued_method").toString("sections");
        const auto history=next.take("_queued_history").toString();
        if(!history.isEmpty()&&!(history=="undo"?canUndo():canRedo())){
            if(next["_section_preview"].toBool()||next["_layer_preview"].toBool()){fail("No "+history+" state available");return;}
            continue;
        }
        if(!currentRequest(history.isEmpty()?method:history,next))fail("Queued edit could not be submitted");
        return;
    }
    notifyState();
    if(!moveUndoRejection_.isEmpty()){
        const auto issue=std::exchange(moveUndoRejection_,QString{});
        if(onError)onError(uiText(issue));
    }else if (onCompleted) onCompleted(completed);
    if(!calculationRecords_.isEmpty())QTimer::singleShot(0,this,[this]{drainCalculationRecords();});
}

void ModelSession::recordCalculationResult(const QString& method,QJsonObject fields,QJsonObject result){
    const bool hydro=method=="hydro_settings";
    const auto calculator=hydro?QString("hydro_")+fields["mode"].toString():fields["calculator"].toString();
    if(hydro)pendingHydro_=result;else pendingPerformance_=result;
    calculationScheduler_->release(this);
    auto* owner=calculationOwner_.data();
    if(!owner||!calculationSnapshotCurrent()){
        result["parameters_recorded"]=false;result["parameter_record_status"]="model_changed";
        result["parameter_record_error"]=uiText("模型已改变，计算参数未写回。");
        pendingHydro_={};pendingPerformance_={};finish(hydro?"hydrostatics":"performance");
        if(hydro){if(onHydrostatics)onHydrostatics(result);}else if(onPerformance)onPerformance(result,false);
        return;
    }
    fields.remove("expected_document_id");fields.remove("expected_revision");
    owner->calculationRecords_.append({this,method,fields,result,calculationOriginGeneration_,calculationOriginParameterVersions_.value(calculator),calculator,model_->documentId});
    QTimer::singleShot(0,owner,[owner]{owner->drainCalculationRecords();});notifyState();
}
void ModelSession::drainCalculationRecords(){
    if(busy_||recordingCalculation_)return;
    while(!calculationRecords_.isEmpty()){
        auto item=calculationRecords_.takeFirst();if(!item.worker)continue;
        recordingCalculation_=std::make_unique<CalculationRecord>(std::move(item));
        const auto& record=*recordingCalculation_;
        if(!ready()||record.document!=model_->documentId||record.generation!=calculationGeneration_){completeCalculationRecord(false,"model_changed");continue;}
        if(record.parameterVersion!=calculationParameterVersions_.value(record.calculator)){completeCalculationRecord(false,"parameters_changed");continue;}
        if(!currentRequest(record.method,record.fields)&&recordingCalculation_)completeCalculationRecord(false,"record_failed");
        return;
    }
}
void ModelSession::completeCalculationRecord(bool recorded,const QString& issue){
    if(!recordingCalculation_)return;
    auto record=std::move(recordingCalculation_);auto* worker=record->worker.data();if(!worker)return;
    auto result=record->result;result["parameters_recorded"]=recorded;result["calculated_revision"]=result["revision"];
    result["parameter_record_status"]=recorded?QStringLiteral("recorded"):issue;
    if(!recorded)result["parameter_record_error"]=issue=="model_changed"?uiText("模型已改变，计算参数未写回。"):
        issue=="parameters_changed"?uiText("参数已改变，未覆盖较新的参数。"):uiText("计算参数未能记入当前船模：")+issue;
    if(recorded){result["recorded_revision"]=double(model_->revision);worker->calculationOriginParameterVersions_[record->calculator]=calculationParameterVersions_.value(record->calculator);}
    const bool hydro=record->method=="hydro_settings";
    worker->pendingHydro_={};worker->pendingPerformance_={};worker->finish(hydro?"hydrostatics":"performance");
    if(hydro){if(worker->onHydrostatics)worker->onHydrostatics(result);}else if(worker->onPerformance)worker->onPerformance(result,false);
}

void ModelSession::fail(const QString& message, bool lostBackend) {
    undoAfterMove_.reset();pendingMoveUndo_.reset();moveUndoRejection_.clear();batchHistoryDirection_.clear();
    ++parseGeneration_;parsing_=false;
    if(firstFrame_){firstFrame_.reset();if(onFirstFrame)onFirstFrame({});}
    editorQueue_.clear();
    pendingSavedCalculators_.clear();
    QString detail = uiText(message);
    if(!pendingPerformance_.isEmpty()||!pendingHydro_.isEmpty()){
        detail=QString(uiText("计算参数未能记入当前船模："))+detail;
        pendingPerformance_={};
        pendingHydro_={};
    }
    if (saveDestinationCommitted_) {
        modified_ = true;
        detail = QString(uiText("文件已保存到 ")) + sourcePath_ + uiText("，但无法确认会话的保存状态。\n") + detail;
        saveDestinationCommitted_ = false;
    }
    timeout_.stop();
    saveRetry_.stop();saveConfirmationLine_.clear();
    busy_ = false;
    method_.clear();
    calculationScheduler_->release(this);
    if(recordingCalculation_)completeCalculationRecord(false,detail);
    pendingLine_.clear();
    pendingInputDir_.clear();
    pendingRecovery_.reset();pendingRecoverySource_.clear();
    if (lostBackend) {
        healthy_ = false;
        stopping_ = true;
        process_.kill();
        process_.waitForFinished(1000);
        stopping_ = false;
        if(model_&&!calculationSession_)detail+=uiText("\n可从“菜单 → 恢复未保存的船模”找回已提交的模型。恢复后撤销历史会重新开始。");
    }
    discardOldArtifacts();
    notifyState();
    if (onError) onError(detail);
    if(!calculationRecords_.isEmpty())QTimer::singleShot(0,this,[this]{drainCalculationRecords();});
}

void ModelSession::discardOldArtifacts() {
    if (!workspace_ || busy_ || parsingPool_.activeThreadCount()) return;
    const QString root = QFileInfo(workspace_->path()).canonicalFilePath();
    if (root.isEmpty()) return;
    const QString retained = QFileInfo(modelPath_).absolutePath();
    const auto folders = QDir(root).entryInfoList({"request-*", "input-*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& folder : folders) {
        const QString path = folder.canonicalFilePath();
        // Only our private direct children can be removed. No external link or
        // source model can become a cleanup target.
        if (folder.isSymLink() || path.isEmpty() ||
            !path.startsWith(root + '/', Qt::CaseInsensitive) ||
            (path == retained || retained.startsWith(path+'/',Qt::CaseInsensitive)) || path == currentInputDir_ || path == pendingInputDir_||path==QFileInfo(calculationState_).absolutePath()||path==outputDir_) continue;
        QDir(path).removeRecursively();
    }
}
