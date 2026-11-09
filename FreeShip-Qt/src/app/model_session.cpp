#include "model_session.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <cmath>
#include <stdexcept>
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
quint64 revision(const QJsonObject& response) {
    const auto value = response.value("revision");
    const double number = value.toDouble(-1);
    if (!value.isDouble() || number < 1 || number > 9007199254740991.0 || std::floor(number) != number)
        error("计算组件返回了无效的模型版本。");
    return quint64(number);
}
}

ModelSession::ModelSession(QObject* parent) : QObject(parent) {
    timeout_.setSingleShot(true);
    timeout_.setInterval(120000);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        fail("模型计算超时。当前画面已保留，请重新打开模型。", true);
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
            fail("计算组件无法运行：" + process_.errorString(), code != QProcess::UnknownError);
    });
    connect(&process_, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        if (!stopping_ && (busy_ || healthy_))
            fail("计算组件已退出。当前画面已保留，请重新打开模型。\n" + QString::fromUtf8(diagnostics_), true);
    });
}

ModelSession::~ModelSession() {
    stopping_ = true;
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
    if (!input.open(QIODevice::ReadOnly)) error("无法读取文件：" + input.errorString());
    if (!output.open(QIODevice::WriteOnly)) error("无法写入文件：" + output.errorString());
    while (!input.atEnd()) {
        const QByteArray bytes = input.read(1024 * 1024);
        if (input.error() != QFile::NoError || output.write(bytes) != bytes.size())
            error("文件写入未完成，原文件未被替换。");
    }
    if (!output.commit()) error("无法完成文件保存：" + output.errorString());
}

void ModelSession::notifyState() { if (onStateChanged) onStateChanged(); }

void ModelSession::startProcess() {
    const QString executable = QCoreApplication::applicationDirPath() + "/FreeShipDocumentService.exe";
    if (!QFileInfo::exists(executable)) error("找不到计算组件：" + executable);
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

bool ModelSession::load(const QString& file) {return loadInput(file,false);}
bool ModelSession::recover(const QString& id) {
    if(busy_)return false;
    try{
        auto lease=std::make_unique<fs::RecoveryLease>(recoveryRoot_,id);
        fs::RecoveryEntry entry;const auto bytes=lease->read(&entry);
        QTemporaryDir stage((temporaryDirectory_.isEmpty()?QDir::tempPath():temporaryDirectory_)+"/freeship-recover-XXXXXX");
        if(!stage.isValid())error("无法准备恢复船模。");
        QSaveFile file(stage.filePath("model.fbm"));file.setDirectWriteFallback(false);
        if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())error("无法读取恢复船模到工作目录。");
        pendingRecovery_=std::move(lease);pendingRecoverySource_=entry.source;
        return loadInput(file.fileName(),true);
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
bool ModelSession::loadInput(const QString& file,bool recovered) {
    if (busy_ || file.isEmpty()) return false;
    try {
        const QFileInfo info(file);
        if (!info.isFile() || !info.isReadable()) error("找不到可读取的模型文件：" + file);
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
            pendingSourceVersion_=copied;
        }
        pendingInputDir_ = folder;
        pendingSource_ = recovered?QString{}:info.absoluteFilePath();
        pendingDocument_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QJsonObject fields{{"path",copy},{"document_id",pendingDocument_}};
        if(recovered)fields["recovered"]=true;
        return request("load",fields);
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
bool ModelSession::undo() { return canUndo() && currentRequest("undo"); }
bool ModelSession::editTopology(const QString& operation, QJsonObject fields) {
    fields.insert("operation", operation);
    return currentRequest("topology", fields);
}
bool ModelSession::redo() { return canRedo() && currentRequest("redo"); }
bool ModelSession::editSections(const QString& operation,int kind,QJsonObject fields) {
    fields.insert("operation",operation);fields.insert("kind",kind);return currentRequest("sections",fields);
}
bool ModelSession::editDisplay(const QJsonObject& changes) {return currentRequest("display",{{"changes",changes}});}
bool ModelSession::editAuxiliary(const QString& operation,QJsonObject fields){fields.insert("operation",operation);return currentRequest("auxiliary",fields);}
bool ModelSession::editAdvanced(const QString& operation,QJsonObject fields){fields.insert("operation",operation);return currentRequest("advanced",fields);}
bool ModelSession::editModeling(const QString& operation,QJsonObject fields){fields.insert("operation",operation);return currentRequest("modeling",fields);}
bool ModelSession::inspectModeling(){return currentRequest("inspect_modeling");}
bool ModelSession::hydroDefaults(){return currentRequest("hydro_defaults");}
bool ModelSession::developPlates(){return currentRequest("develop_plates");}
bool ModelSession::cadPatches(){return currentRequest("cad_iges");}
void ModelSession::cancelCad(){
    if(!busy_||method_!="cad_iges")return;
    QSaveFile flag(outputDir_+"/cancel.flag");
    if(!flag.open(QIODevice::WriteOnly)||flag.write("cancel")!=6||!flag.commit()) {if(onError)onError("无法提交取消请求。");}
    else cadCancelRequested_=true;
}
void ModelSession::cancelPlates(){
    if(!busy_||method_!="develop_plates")return;
    QSaveFile flag(outputDir_+"/cancel.flag");
    if(!flag.open(QIODevice::WriteOnly)||flag.write("cancel")!=6||!flag.commit()) {if(onError)onError("无法提交取消请求。");}
    else plateCancelRequested_=true;
}
bool ModelSession::performanceDefaults(QJsonObject fields){return currentRequest("performance_defaults",fields);}
bool ModelSession::calculatePerformance(QJsonObject fields){fields["model_name"]=sourcePath_;return currentRequest("performance",fields);}
bool ModelSession::savePerformanceSettings(QJsonObject fields){return currentRequest("performance_settings",fields);}
void ModelSession::cancelPerformance(){
    if(!busy_||(method_!="performance"&&method_!="performance_defaults"))return;
    QSaveFile flag(outputDir_+"/cancel.flag");
    if(!flag.open(QIODevice::WriteOnly)||flag.write("cancel")!=6||!flag.commit())if(onError)onError("无法提交取消请求。");
}
bool ModelSession::calculateHydrostatics(QJsonObject fields){return currentRequest("hydrostatics",fields);}
bool ModelSession::saveHydroSettings(QJsonObject fields){return currentRequest("hydro_settings",fields);}
void ModelSession::cancelHydrostatics(){
    if(!busy_||method_!="hydrostatics")return;
    QSaveFile flag(outputDir_+"/cancel.flag");
    if(!flag.open(QIODevice::WriteOnly)||flag.write("cancel")!=6||!flag.commit())
        if(onError)onError("无法提交取消请求。");
}
bool ModelSession::manageLayers(const QString& operation,QJsonObject fields) {
    fields.insert("operation",operation);return currentRequest("manage_layers",fields);
}
bool ModelSession::save(const QString& destination) {
    if (destination.isEmpty() || busy_ || !ready()) return false;
    try{
        pendingSave_ = QFileInfo(destination).absoluteFilePath();
        pendingSaveVersion_=fs::sameModelFile(pendingSave_,sourcePath_)?sourceVersion_:fs::observeModelFile(pendingSave_);
        saveDestinationCommitted_ = false;lastBackupPath_.clear();
        saveReceipt_={};saveConfirmationLine_.clear();saveConfirmationAttempts_=0;
        QJsonObject fields;
        // An untouched file is still copied byte for byte, including its JPEG.
        lastSavePreviewMs_=0;
        if(modified_&&previewEnabled_&&makeSavePreview){
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
        plateCancelRequested_ = false;
        cadCancelRequested_ = false;
        pendingId_ = ++nextId_;
        outputDir_ = workspace_->path() + "/request-" + QString::number(pendingId_);
        if (!QDir().mkpath(outputDir_)) error("无法创建计算结果目录。");
        fields.insert("id", double(pendingId_));
        fields.insert("method", method);
        fields.insert("output", outputDir_);
        fields.insert("native_linesplan", true);
        if(!linesCacheRoot_.isNull())fields["lines_cache_root"]=linesCacheRoot_;
        if(method=="load" || method=="new" || method=="import_model"){
            fields.insert("preferences",preferences_);fields["recovery_root"]=recoveryRoot_;
            fields["recovery_source"]=pendingRecovery_?pendingRecoverySource_:pendingSource_;
        }
        busy_ = true;
        operationTimer_.start();
        timeout_.start(method=="hydrostatics"||method=="performance"?150000:120000);
        notifyState();
        const QByteArray line = QJsonDocument(fields).toJson(QJsonDocument::Compact) + '\n';
        if (process_.state() == QProcess::NotRunning) {
            if (method != "load" && method != "new" && method != "import_model") error("计算组件已停止，请重新打开模型。");
            pendingLine_ = line;
            startProcess();
        } else if (process_.state() == QProcess::Starting) pendingLine_ = line;
        else if (process_.write(line) != line.size()) error("无法提交模型操作。");
        return true;
    } catch (const std::exception& ex) {
        fail(QString::fromUtf8(ex.what()), true);
        return false;
    }
}

void ModelSession::readOutput() {
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
        try { receive(json.object()); }
        catch (const std::exception& ex) { fail(QString::fromUtf8(ex.what()), true); }
    }
}

QString ModelSession::artifact(const QJsonObject& response, const QString& key) const {
    const QFileInfo file(response.value(key).toString());
    const QString canonical = file.canonicalFilePath();
    const QString allowed = QFileInfo(outputDir_).canonicalFilePath() + '/';
    if (canonical.isEmpty() || !file.isFile() || file.size() == 0 ||
        !canonical.startsWith(allowed, Qt::CaseInsensitive))
        error("计算组件未返回完整的文件：" + key);
    return canonical;
}

void ModelSession::readState(const QJsonObject& response) {
    if (!model_ || response.value("document_id").toString() != model_->documentId ||
        revision(response) != model_->revision) error("计算组件与界面的模型版本不一致。");
    for (const QString key : {"can_undo", "can_redo", "modified"})
        if (!response.value(key).isBool()) error("计算组件返回了不完整的操作状态。");
    canUndo_ = response.value("can_undo").toBool();
    canRedo_ = response.value("can_redo").toBool();
    modified_ = response.value("modified").toBool();
    previewEnabled_ = response.value("preview_enabled").toBool();
}

void ModelSession::publishSnapshot(const QJsonObject& response) {
    const QString modelFile = artifact(response, "model");
    const QString sceneFile = artifact(response, "scene");
    const QString dxfFile = artifact(response, "dxf");
    auto nextModel = std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::load(modelFile));
    auto nextScene = std::make_shared<Scene>(Scene::load(sceneFile));
    if (nextModel->documentId != pendingDocument_ || nextModel->documentId != response.value("document_id").toString() ||
        nextModel->revision != revision(response)) error("收到的图形与当前模型版本不一致。");
    if (method_ != "load" && method_ != "new" && method_ != "import_model" && model_ && nextModel->revision < model_->revision)
        error("计算组件返回了过期的模型。");
    for (const QString key : {"can_undo", "can_redo", "modified"})
        if (!response.value(key).isBool()) error("计算组件返回了不完整的操作状态。");
    QFile dxf(dxfFile);
    if (!dxf.open(QIODevice::ReadOnly) || !dxf.seek(std::max<qint64>(0, dxf.size() - 128)) ||
        !dxf.readAll().trimmed().endsWith("EOF")) error("DXF 结果不完整。");
    QFile nativeReport(artifact(response, "native_report"));
    if (nativeReport.size()>1024*1024 || !nativeReport.open(QIODevice::ReadOnly)) error("无法核对型线图的生成结果。");
    const auto native=QJsonDocument::fromJson(nativeReport.readAll()).object();
    if (native["status"]!="pass" || native["engine"]!="cpp-native" ||
        native["document_id"]!=nextModel->documentId || revision(native)!=nextModel->revision ||
        native["scene_sha256"].toString().toLatin1()!=nextScene->digest)
        error("型线图与当前模型版本不一致。");
    QCryptographicHash dxfHash(QCryptographicHash::Sha256);
    if (!dxf.seek(0) || !dxfHash.addData(&dxf) || native["dxf_sha256"].toString().toLatin1()!=dxfHash.result().toHex())
        error("型线图导出结果校验失败。");
    QFile sectionFile(artifact(response,"sections"));
    if(sectionFile.size()>256LL*1024*1024||!sectionFile.open(QIODevice::ReadOnly))error("无法读取剖面线。");
    const auto sectionData=sectionFile.readAll();
    if(native["sections_sha256"].toString().toLatin1()!=QCryptographicHash::hash(sectionData,QCryptographicHash::Sha256).toHex())
        error("剖面线结果校验失败。");
    nextModel->loadSectionDisplay(sectionData);
    QFile surfaceFile(artifact(response,"surface"));
    if(surfaceFile.size()>256LL*1024*1024||!surfaceFile.open(QIODevice::ReadOnly))error("无法读取船体曲面。");
    const auto surfaceData=surfaceFile.readAll();
    if(!native["surface_display"].toBool()||native["surface_sha256"].toString().toLatin1()!=QCryptographicHash::hash(surfaceData,QCryptographicHash::Sha256).toHex())
        error("船体曲面结果校验失败。");
    nextModel->loadSurfaceDisplay(surfaceData);
    model_ = std::move(nextModel);
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
    if (onUpdated) onUpdated();
}

void ModelSession::receive(const QJsonObject& response) {
    // A timed-out acknowledgement may arrive after its successful retry. It
    // belongs to a completed request and must never overwrite newer UI state.
    if(response.value("id").toDouble(-1)==double(lastSaveConfirmationId_)&&lastSaveConfirmationId_>=0)return;
    if (!busy_ || response.value("id").toDouble(-1) != double(pendingId_))
        error("计算组件返回了不对应当前操作的响应。");
    if (!response.value("ok").isBool()) error("计算组件返回了无效的操作结果。");
    if (!response.value("ok").toBool()) {
        fail(response.value("error").toString("模型操作失败。"));
        return;
    }
    if (response.value("document_id").toString() != pendingDocument_)
        error("计算组件返回了另一个文档的结果。");
    recoveryError_=response["recovery_error"].toString();
    recoveryFile_=response["recovery_file"].toString();
    if(method_=="discard_recovery"){readState(response);finish(method_);return;}
    if(method_=="hydrostatics"){
        readState(response);QFile file(artifact(response,"hydro"));
        if(!file.open(QIODevice::ReadOnly)||file.size()>32*1024*1024)error("无法读取静水力报告。");
        QJsonParseError parse;const auto document=QJsonDocument::fromJson(file.readAll(),&parse);const auto data=document.object();
        if(parse.error!=QJsonParseError::NoError||!data["rows"].isArray()||data["document_id"].toString()!=model_->documentId||revision(data)!=model_->revision)
            error("静水力报告格式或模型版本不一致。");
        finish(method_);if(onHydrostatics)onHydrostatics(data);return;
    }
    if(method_=="performance"||method_=="performance_defaults"){
        const bool defaults=method_=="performance_defaults";
        if(!model_||response["revision"].toDouble()!=double(model_->revision))error("计算报告与当前模型版本不一致。");
        readState(response);QFile file(artifact(response,"performance"));
        if(!file.open(QIODevice::ReadOnly)||file.size()>128*1024*1024)error("无法读取计算报告。");
        QJsonParseError parse;const auto data=QJsonDocument::fromJson(file.readAll(),&parse).object();
        if(parse.error!=QJsonParseError::NoError||!data[defaults?"fields":"series"].isArray()||data["document_id"].toString()!=model_->documentId||revision(data)!=model_->revision)error("计算报告格式或模型版本不一致。");
        finish(method_);if(onPerformance)onPerformance(data,defaults);return;
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
        sourcePath_ = pendingSave_;
        recoveryOrigin_.clear();
        saveDestinationCommitted_ = false;
        lastSaveConfirmationId_=pendingId_;
        finish("save");
        return;
    }
    publishSnapshot(response);
    if(pendingRecovery_){
        // Retire the crash copy only once the recovered revision has its own
        // durable copy. Failed parsing/publication/writes preserve the original.
        if(!recoveryFile_.isEmpty())try{pendingRecovery_->remove();}catch(const std::exception& ex){recoveryError_=QString::fromUtf8(ex.what());}
        pendingRecovery_.reset();pendingRecoverySource_.clear();
    }
    finish(method_);
}

void ModelSession::finish(const QString& operation) {
    const QString completed = operation;
    lastOperationMs_ = operationTimer_.nsecsElapsed() / 1e6;
    timeout_.stop();
    saveRetry_.stop();saveConfirmationLine_.clear();
    busy_ = false;
    method_.clear();
    discardOldArtifacts();
    notifyState();
    if (onCompleted) onCompleted(completed);
}

void ModelSession::fail(const QString& message, bool lostBackend) {
    QString detail = message;
    if (saveDestinationCommitted_) {
        modified_ = true;
        detail = "文件已保存到 " + sourcePath_ + "，但无法确认会话的保存状态。\n" + detail;
        saveDestinationCommitted_ = false;
    }
    timeout_.stop();
    saveRetry_.stop();saveConfirmationLine_.clear();
    busy_ = false;
    method_.clear();
    pendingLine_.clear();
    pendingInputDir_.clear();
    pendingRecovery_.reset();pendingRecoverySource_.clear();
    if (lostBackend) {
        healthy_ = false;
        stopping_ = true;
        process_.kill();
        process_.waitForFinished(1000);
        stopping_ = false;
    }
    discardOldArtifacts();
    notifyState();
    if (onError) onError(detail);
}

void ModelSession::discardOldArtifacts() {
    if (!workspace_ || busy_) return;
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
            path == retained || path == currentInputDir_ || path == pendingInputDir_) continue;
        QDir(path).removeRecursively();
    }
}
