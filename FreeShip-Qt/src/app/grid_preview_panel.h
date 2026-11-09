// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <QProcess>
#include <QTimer>
#include <QTemporaryDir>
#include <QJsonObject>
#include <functional>
#include <memory>
class ModelSession;class GridPreviewView;class QLabel;class QPushButton;
class GridPreviewPanel final:public QWidget {
public:
    GridPreviewPanel(ModelSession&,QString format,std::function<QJsonObject()> options,QWidget* parent=nullptr);
    ~GridPreviewPanel()override;
    void invalidate();void requestNow();void cancel();
    bool pending()const{return dirty_||running_;}
    bool current()const{return current_;}
    QJsonObject evidence()const;
    GridPreviewView* view()const{return view_;}
protected:
    void showEvent(QShowEvent*)override;
private:
    void finish(int code);void fail(QString text);QByteArray key()const;
    ModelSession& session_;QString format_;std::function<QJsonObject()> options_;
    GridPreviewView* view_;QLabel* status_;QPushButton* refresh_;QPushButton* stop_;
    QProcess process_;QTimer delay_,timeout_;std::unique_ptr<QTemporaryDir> work_;
    QJsonObject data_;QByteArray launchedKey_,completedKey_;QString inputHash_,error_;
    quint64 generation_=0,launched_=0,launchCount_=0;bool dirty_=true,running_=false,current_=false;
};
