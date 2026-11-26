// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDialog>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QMap>
#include <memory>
#include <functional>
#include "core/model_exchange.h"
class ModelSession;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
class StudioMessage;
class QPushButton;
class QSpinBox;
class QToolButton;
class GridPreviewPanel;
class PrintHullPanel;
class QButtonGroup;
class ObjImportLayerTree;
class ExchangeDialog final:public QDialog {
public:
    ExchangeDialog(ModelSession& session,bool importing,QString format,QWidget* parent=nullptr);
    ~ExchangeDialog() override;
    void refreshState();
    void showFailure(const QString& error);
    void imported();
    void receiveCad(const QJsonObject& data);
    QString failure()const;
    bool succeeded()const{return succeeded_;}
    bool busy()const;
    bool handlesSessionError()const{return importPending_||!pendingCadPath_.isEmpty();}
    QJsonObject objFitEvidence()const{return objFitEvidence_;}
    GridPreviewPanel* gridPreview()const{return preview_;}
    PrintHullPanel* printHull()const{return printHull_;}
    void reject()override;
    QVector<fs::EntityId> faceIds;
    std::function<void(std::function<void()>)> beforeImport;
    std::function<bool(const QJsonObject&,const QString&)> importInNewTab;
    std::function<bool(const QString&)> confirmOverwrite;
private:
    void submit();
    void estimateObjTolerance();
    void scheduleObjLayers();
    void readObjLayers();
    void startObjFit(const QString& path,const fs::exchange::Options& options);
    void importPrepared(fs::exchange::Document document,const QString& path);
    QString chooseExportPath();
    void startSections(const QString& path);
    QJsonObject referenceValues() const;
    void finishSections(int code);
    ModelSession& session_;bool importing_,succeeded_=false;QString format_;
    QLineEdit* importPath_=nullptr;QComboBox* units_;QComboBox* encoding_=nullptr;QCheckBox *axes_,*control_,*symmetric_,*hidden_,*binary_,*closeDeck_;
    QWidget* objImportCards_=nullptr;
    QButtonGroup* objImportMode_=nullptr;
    ObjImportLayerTree* objLayerTree_=nullptr;
    QTimer objLayersTimer_;
    std::shared_ptr<std::atomic_bool> objLayersCancel_;
    QString objLayersError_;
    bool objLayersLoading_=false;
    bool importPending_=false;
    QCheckBox* objInferCreases_=nullptr;
    QWidget* objToleranceRow_=nullptr;
    QToolButton* objAutomaticTolerance_=nullptr;
    QTimer objToleranceTimer_;
    std::shared_ptr<std::atomic_bool> objToleranceCancel_;
    QLineEdit* objTolerance_=nullptr;
    std::shared_ptr<std::atomic_bool> importCancel_;
    QJsonObject objFitEvidence_;
    QMap<QString,QLineEdit*> references_;
    QMap<QString,QComboBox*> referenceChoices_;
    QSpinBox *columns_=nullptr,*rows_=nullptr;QCheckBox* intermediate_=nullptr;
    StudioMessage *status_,*details_;QPushButton* run_=nullptr;
    QString pendingCadPath_;fs::exchange::Options pendingCadOptions_;
    QProcess exportProcess_;QTimer exportTimeout_;std::unique_ptr<QTemporaryDir> exportWork_;
    QString exportPath_,exportDocument_;quint64 exportRevision_=0;bool exporting_=false,cancelled_=false;
    GridPreviewPanel* preview_=nullptr;
    PrintHullPanel* printHull_=nullptr;
    QCheckBox* printMode_=nullptr;
    int ordinaryUnits_=0;
};
