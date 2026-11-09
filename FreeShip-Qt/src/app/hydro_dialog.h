#pragma once
#include "model_session.h"
#include <QDialog>
#include <QMap>
#include <QJsonArray>
class QLabel;class QPushButton;class QTableWidget;class QPlainTextEdit;class QTabWidget;
class HydroDialog final:public QDialog {
public:
    HydroDialog(ModelSession& session,const QString& mode,const QJsonObject& defaults,bool hidden,QWidget* parent);
    QJsonObject inputs()const;
    QJsonObject result()const{return result_;}
    QString exportReport(const QString& path,const QString& format)const;
    QString exportAuxiliary(const QString& path,int index)const;
    int auxiliaryCount()const;
    bool selectAuxiliary(int index);
    void receive(const QJsonObject& result);
    void showFailure(const QString& error);
    void settingsSaved();
    void refreshState();
    void reject()override;
private:
    void calculate();
    void render();
    void renderAuxiliaryPage();
    QByteArray csv()const;
    QString reportText()const;
    ModelSession& session_;QString mode_;bool closing_=false;
    QMap<QString,QWidget*> fields_;QJsonObject result_;
    QLabel *status_,*legend_;QPushButton *run_,*cancel_,*saveSettings_;
    QVector<QPushButton*> exports_;QTableWidget* table_;QPlainTextEdit* text_;QTabWidget* tabs_;QWidget* plot_=nullptr;
    QVector<QPair<QString,QString>> columns_;
    QTabWidget* auxiliaryTabs_=nullptr;
    QLabel* auxiliaryNotice_=nullptr;
    QPushButton* auxiliarySave_=nullptr;
};
