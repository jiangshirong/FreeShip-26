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
class QPushButton;
class QSpinBox;
class GridPreviewPanel;
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
    GridPreviewPanel* gridPreview()const{return preview_;}
    void reject()override;
    QVector<fs::EntityId> faceIds;
    std::function<void(std::function<void()>)> beforeImport;
    std::function<bool(const QString&)> confirmOverwrite;
private:
    void submit();
    void startSections(const QString& path);
    QJsonObject referenceValues() const;
    void finishSections(int code);
    ModelSession& session_;bool importing_,succeeded_=false;QString format_;
    QLineEdit* path_;QComboBox* units_;QComboBox* encoding_=nullptr;QCheckBox *axes_,*control_,*symmetric_,*hidden_,*binary_,*closeDeck_;
    QMap<QString,QLineEdit*> references_;
    QMap<QString,QComboBox*> referenceChoices_;
    QSpinBox *columns_=nullptr,*rows_=nullptr;QCheckBox* intermediate_=nullptr;
    QLabel *status_,*details_;QPushButton* run_;
    QString pendingCadPath_;fs::exchange::Options pendingCadOptions_;
    QProcess exportProcess_;QTimer exportTimeout_;std::unique_ptr<QTemporaryDir> exportWork_;
    QString exportPath_,exportDocument_;quint64 exportRevision_=0;bool exporting_=false,cancelled_=false;
    GridPreviewPanel* preview_=nullptr;
};
