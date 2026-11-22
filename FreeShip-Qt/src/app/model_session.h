#pragma once
#include <algorithm>

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
#include <QThreadPool>
#include <QPointer>
#include <QHash>
class CalculationScheduler;
struct PreparedSnapshot;
#include <functional>
#include <memory>
#include <optional>

// Serializes edits to one backend. A complete, validated revision is published
// together; views cannot observe half an edit or a scene from another document.
class ModelSession final : public QObject {
public:
    explicit ModelSession(QObject* parent = nullptr);
    ~ModelSession() override;
    bool load(const QString& file);
    bool loadCalculationSnapshot(const ModelSession& source);
    bool isCalculationSession()const{return calculationSession_;}
    bool calculationQueued()const;
    int calculationQueuePosition()const;
    bool calculationActive()const;
    bool calculationSnapshotCurrent()const;
    std::optional<double> accelerationResistanceMaximum()const;
    int activeCalculationCount()const;
    void pauseCalculationQueue(bool paused);
    void abortCalculationForExit();
    bool recordingCalculationParameters()const{return bool(recordingCalculation_)||!calculationRecords_.isEmpty();}
    bool ensureDrawing();
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
    bool recordingHydroRange()const{return !pendingHydro_.isEmpty();}
    void cancelHydrostatics();
    bool performanceDefaults(QJsonObject fields);
    bool calculatePerformance(QJsonObject fields);
    bool savePerformanceSettings(QJsonObject fields);
    bool recordingPerformanceParameters()const{return !pendingPerformance_.isEmpty();}
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
    bool editingSections()const{return busy_&&(method_=="sections"||sectionPreviewActive_);}
    bool sectionPreviewHistory(bool redo);
    void dropQueuedSectionCommands(){if(editingSections())editorQueue_.clear();}
    bool editingLayerList()const{return busy_&&layerPreviewActive_;}
    bool layerListCommand(const QString& method,QJsonObject fields={});
    void dropQueuedLayerCommands(){if(editingLayerList())editorQueue_.clear();}
    bool sectionHistoryPending()const{for(const auto& request:editorQueue_)if(request.contains("_queued_history"))return true;return false;}
    bool canQueueSectionEdits()const{return editingSections()&&std::none_of(editorQueue_.cbegin(),editorQueue_.cend(),[](const auto& r){return r.contains("_queued_history")&&!r.value("_section_preview").toBool();});}
    bool editDisplay(const QJsonObject& changes);
    bool editAuxiliary(const QString& operation,QJsonObject fields = {});
    bool editAdvanced(const QString& operation,QJsonObject fields = {});
    bool manageLayers(const QString& operation, QJsonObject fields = {});
    bool importBackground(int view,const QString& filename,double scale);
    bool editBackground(int view,const QString& operation,QJsonObject fields = {});
    bool undo();
    bool redo();
    bool save(const QString& destination,const QJsonObject& calculationDrafts={});
    void setPreferences(const QJsonObject &options,const QString &temporaryDirectory={}) {preferences_=options;temporaryDirectory_=temporaryDirectory;}
    bool configurePreferences(const QJsonObject &options) {return currentRequest("configure_preferences",{{"preferences",options}});}
    bool busy() const { return busy_; }
    bool hydroRunning()const{return busy_&&(method_=="hydrostatics"||(method_=="load_calculation"&&afterCalculationLoadMethod_=="hydrostatics"));}
    QString hydroRunningMode()const{return hydroRunning()?hydroRunningMode_:QString{};}
    QJsonObject hydroProgress()const{return hydroProgress_;}
    bool ready() const { return healthy_ && bool(model_); }
    bool drawingUnchanged() const {return drawingUnchanged_;}
    bool publishingPointEdit() const {return method_=="move_point"||method_=="move_points"||method_=="undo"||method_=="redo";}
    bool modified() const { return modified_; }
    bool canUndo() const { return ready() && canUndo_; }
    int layerIdHighWater() const { return layerIdHighWater_; }
    int historyStates() const { return historyStates_; }
    bool canRedo() const { return ready() && canRedo_; }
    QString pendingFilePath() const { return !busy_ ? QString{} : method_=="load" ? pendingSource_ : method_=="save" ? pendingSave_ : QString{}; }
    QString sourcePath() const { return sourcePath_; }
    QString lastBackupPath()const{return lastBackupPath_;}
    QString modelPath() const { return modelPath_; }
    QString calculationStatePath() const { return calculationState_; }
    QString scenePath() const { return scenePath_; }
    QString dxfPath() const { return dxfPath_; }
    std::shared_ptr<const fs::ModelSnapshot> model() const { return model_; }
    std::shared_ptr<const Scene> scene() const { return scene_; }
    QJsonObject lastLoadTimings() const { return lastLoadTimings_; }
    double lastOperationMs() const { return lastOperationMs_; }
    double lastSavePreviewMs() const { return lastSavePreviewMs_; }
    static void atomicCopy(const QString& source, const QString& destination);

    std::function<void()> onUpdated;
    std::function<void(std::shared_ptr<const fs::ModelSnapshot>)> onFirstFrame;
    std::function<void()> onStateChanged;
    std::function<void(const QString&)> onCompleted;
    std::function<void(const QString&)> onError;
    std::function<void(const QString&)> onFileCommitted;
    std::function<QByteArray(std::shared_ptr<const fs::ModelSnapshot>)> makeSavePreview;
    std::function<void(const QJsonObject&)> onAnalysis;
    std::function<void(const QJsonObject&)> onHydroDefaults,onHydrostatics;
    std::function<void(const QJsonObject&)> onHydroProgress;
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
    void completePerformance(const QJsonObject& data,bool defaults);
    void publishSnapshot(const QJsonObject& response,std::shared_ptr<PreparedSnapshot> prepared={});
    void completePublication();
    void readState(const QJsonObject& response);
    void finish(const QString& operation);
    void fail(const QString& message, bool lostBackend = false);
    QString artifact(const QJsonObject& response, const QString& key) const;
    void notifyState();
    void discardOldArtifacts();
    void sendRequestLine(const QByteArray& line,const QString& method);
    bool cancelQueuedCalculation();
    bool refreshCalculationSnapshot(const QString& method,const QJsonObject& fields);
    void recordCalculationResult(const QString& method,QJsonObject fields,QJsonObject result);
    void drainCalculationRecords();
    void completeCalculationRecord(bool recorded,const QString& issue={});
    QString calculationCacheDirectory()const;
    void importCalculationDependencies();
    void publishCalculationDependencies();

    std::unique_ptr<QTemporaryDir> workspace_;
    std::unique_ptr<fs::RecoveryLease> pendingRecovery_;
    QString recoveryRoot_,recoveryError_,recoveryFile_,recoveryOrigin_,pendingRecoverySource_;
    QString linesCacheRoot_;
    QProcess process_;
    QThreadPool parsingPool_;
    std::shared_ptr<CalculationScheduler> calculationScheduler_;
    QPointer<ModelSession> calculationOwner_;
    bool calculationSession_=false;
    QString calculationState_,calculationBindings_;
    quint64 calculationGeneration_=0,calculationOriginGeneration_=0;
    QHash<QString,quint64> calculationParameterVersions_,calculationOriginParameterVersions_;
    QString pendingParameterCalculator_;
    QString calculationMethod_,calculationDependencyToken_;
    quint64 calculationCacheRevision_=0;
    QString afterCalculationLoadMethod_;
    bool afterCalculationLoadCancelled_=false;
    QJsonObject afterCalculationLoadFields_;
    struct CalculationRecord {QPointer<ModelSession> worker;QString method;QJsonObject fields,result;quint64 generation=0,parameterVersion=0;QString calculator,document;};
    QList<CalculationRecord> calculationRecords_;
    std::unique_ptr<CalculationRecord> recordingCalculation_;
    QString hydroRunningMode_;
    QJsonObject hydroProgress_;
    bool parsing_=false;
    qint64 parseGeneration_=0;
    QTimer timeout_, saveRetry_;
    QElapsedTimer operationTimer_;
    QByteArray buffer_, pendingLine_, diagnostics_;
    QString method_, outputDir_, pendingDocument_, pendingSource_, pendingSave_;
    QStringList pendingSavedCalculators_;
    QString sourcePath_, modelPath_, scenePath_, dxfPath_;
    fs::ModelFileVersion sourceVersion_,pendingSourceVersion_,pendingSaveVersion_;
    QString lastBackupPath_;
    QString currentInputDir_, pendingInputDir_;
    QJsonObject preferences_;
    QJsonObject pendingPerformance_,pendingHydro_;
    QString temporaryDirectory_;
    qint64 nextId_ = 0, pendingId_ = 0;
    int layerIdHighWater_=0;
    int historyStates_ = -1;
    bool busy_ = false, healthy_ = false, canUndo_ = false, canRedo_ = false;
    bool modified_ = false, stopping_ = false;
    bool previewEnabled_ = false;
    QByteArray sectionDigest_,surfaceDigest_;
    bool drawingUnchanged_=false;
    bool saveDestinationCommitted_ = false;
    QJsonObject saveReceipt_;
    QByteArray saveConfirmationLine_;
    unsigned saveConfirmationAttempts_ = 0;
    qint64 lastSaveConfirmationId_ = -1;
    bool plateCancelRequested_ = false;
    bool cadCancelRequested_ = false;
    bool performanceCancelRequested_ = false;
    bool hydroCancelRequested_ = false;
    QJsonObject lastLoadTimings_;
    QList<QJsonObject> editorQueue_;
    bool sectionPreviewActive_=false,layerPreviewActive_=false;
    double lastOperationMs_ = 0;
    double lastSavePreviewMs_ = 0;
    std::shared_ptr<const fs::ModelSnapshot> model_;
    std::shared_ptr<const Scene> scene_;
    std::shared_ptr<fs::ModelSnapshot> firstFrame_;
    QByteArray firstFrameSections_,firstFrameSurface_;
};
