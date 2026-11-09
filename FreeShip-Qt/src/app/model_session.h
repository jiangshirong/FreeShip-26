#pragma once

#include "core/model_snapshot.h"
#include "scene.h"
#include "core/recovery_store.h"
#include "core/model_file_save.h"
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QJsonObject>
#include <QElapsedTimer>
#include <functional>
#include <memory>

// Serializes edits to one backend. A complete, validated revision is published
// together; views cannot observe half an edit or a scene from another document.
class ModelSession final : public QObject {
public:
    explicit ModelSession(QObject* parent = nullptr);
    ~ModelSession() override;
    bool load(const QString& file);
    bool recover(const QString& id);
    bool resumeCurrent();
    bool canResumeCurrent()const{return !busy_&&!healthy_&&bool(model_);}
    bool discardRecovery();
    void setRecoveryRoot(const QString& root) { if(!model_&&!busy_)recoveryRoot_=root; }
    QString recoveryRoot()const{return recoveryRoot_;}
    void setLinesCacheRoot(const QString& root){if(!model_&&!busy_)linesCacheRoot_=root;}
    QString recoveryError()const{return recoveryError_;}
    QString recoveryFile()const{return recoveryFile_;}
    QString recoveryOrigin()const{return recoveryOrigin_;}
    bool newHull(QJsonObject fields);
    bool importModel(const QJsonObject& exchange, bool append=false);
    bool editModeling(const QString& operation,QJsonObject fields = {});
    bool inspectModeling();
    bool hydroDefaults();
    bool calculateHydrostatics(QJsonObject fields);
    bool saveHydroSettings(QJsonObject fields);
    void cancelHydrostatics();
    bool performanceDefaults(QJsonObject fields);
    bool calculatePerformance(QJsonObject fields);
    bool savePerformanceSettings(QJsonObject fields);
    void cancelPerformance();
    bool developPlates();
    bool cadPatches();
    void cancelCad();
    void cancelPlates();
    bool movePoint(fs::EntityId id, fs::Vec3 position);
    bool movePoints(const QVector<fs::PointMove>& moves);
    bool updateProject(const QJsonObject& changes);
    bool updateLayer(fs::EntityId id, const QJsonObject& changes);
    bool editTopology(const QString& operation, QJsonObject fields = {});
    bool editSections(const QString& operation, int kind, QJsonObject fields = {});
    bool editDisplay(const QJsonObject& changes);
    bool editAuxiliary(const QString& operation,QJsonObject fields = {});
    bool editAdvanced(const QString& operation,QJsonObject fields = {});
    bool manageLayers(const QString& operation, QJsonObject fields = {});
    bool importBackground(int view,const QString& filename,double scale);
    bool editBackground(int view,const QString& operation,QJsonObject fields = {});
    bool undo();
    bool redo();
    bool save(const QString& destination);
    void setPreferences(const QJsonObject &options,const QString &temporaryDirectory={}) {preferences_=options;temporaryDirectory_=temporaryDirectory;}
    bool configurePreferences(const QJsonObject &options) {return currentRequest("configure_preferences",{{"preferences",options}});}
    bool busy() const { return busy_; }
    bool ready() const { return healthy_ && bool(model_); }
    bool modified() const { return modified_; }
    bool canUndo() const { return ready() && canUndo_; }
    bool canRedo() const { return ready() && canRedo_; }
    QString sourcePath() const { return sourcePath_; }
    QString lastBackupPath()const{return lastBackupPath_;}
    QString modelPath() const { return modelPath_; }
    QString scenePath() const { return scenePath_; }
    QString dxfPath() const { return dxfPath_; }
    std::shared_ptr<const fs::ModelSnapshot> model() const { return model_; }
    std::shared_ptr<const Scene> scene() const { return scene_; }
    double lastOperationMs() const { return lastOperationMs_; }
    double lastSavePreviewMs() const { return lastSavePreviewMs_; }
    static void atomicCopy(const QString& source, const QString& destination);

    std::function<void()> onUpdated;
    std::function<void()> onStateChanged;
    std::function<void(const QString&)> onCompleted;
    std::function<void(const QString&)> onError;
    std::function<void(const QString&)> onFileCommitted;
    std::function<QByteArray(std::shared_ptr<const fs::ModelSnapshot>)> makeSavePreview;
    std::function<void(const QJsonObject&)> onAnalysis;
    std::function<void(const QJsonObject&)> onHydroDefaults,onHydrostatics;
    std::function<void(const QJsonObject&,bool)> onPerformance;
    std::function<void(const QJsonObject&)> onPlates;
    std::function<void(const QJsonObject&)> onCad;

private:
    friend struct SaveConfirmationProbe;
    friend struct BackendExitProbe;
    friend struct BackendRecoveryProbe;
    bool loadInput(const QString& file,bool recovered,const fs::ModelFileVersion* requiredVersion=nullptr);
    bool currentRequest(const QString& method, QJsonObject fields = {});
    bool request(const QString& method, QJsonObject fields);
    void startProcess();
    void readOutput();
    void receive(const QJsonObject& response);
    void publishSnapshot(const QJsonObject& response);
    void readState(const QJsonObject& response);
    void finish(const QString& operation);
    void fail(const QString& message, bool lostBackend = false);
    QString artifact(const QJsonObject& response, const QString& key) const;
    void notifyState();
    void discardOldArtifacts();

    std::unique_ptr<QTemporaryDir> workspace_;
    std::unique_ptr<fs::RecoveryLease> pendingRecovery_;
    QString recoveryRoot_,recoveryError_,recoveryFile_,recoveryOrigin_,pendingRecoverySource_;
    QString linesCacheRoot_;
    QProcess process_;
    QTimer timeout_, saveRetry_;
    QElapsedTimer operationTimer_;
    QByteArray buffer_, pendingLine_, diagnostics_;
    QString method_, outputDir_, pendingDocument_, pendingSource_, pendingSave_;
    QString sourcePath_, modelPath_, scenePath_, dxfPath_;
    fs::ModelFileVersion sourceVersion_,pendingSourceVersion_,pendingSaveVersion_;
    QString lastBackupPath_;
    QString currentInputDir_, pendingInputDir_;
    QJsonObject preferences_;
    QString temporaryDirectory_;
    qint64 nextId_ = 0, pendingId_ = 0;
    bool busy_ = false, healthy_ = false, canUndo_ = false, canRedo_ = false;
    bool modified_ = false, stopping_ = false;
    bool previewEnabled_ = false;
    bool saveDestinationCommitted_ = false;
    QJsonObject saveReceipt_;
    QByteArray saveConfirmationLine_;
    unsigned saveConfirmationAttempts_ = 0;
    qint64 lastSaveConfirmationId_ = -1;
    bool plateCancelRequested_ = false;
    bool cadCancelRequested_ = false;
    double lastOperationMs_ = 0;
    double lastSavePreviewMs_ = 0;
    std::shared_ptr<const fs::ModelSnapshot> model_;
    std::shared_ptr<const Scene> scene_;
};
