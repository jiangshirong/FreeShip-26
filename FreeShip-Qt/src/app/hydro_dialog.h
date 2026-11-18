#pragma once
#include "model_session.h"
#include <QDialog>
#include <QMap>
#include <QJsonArray>
class QLabel;class QPushButton;class QTableWidget;class QTableView;class QTabWidget;
namespace report {class Browser;}
class StudioMessage;
class QStackedWidget;class QButtonGroup;
class HydroDialog final:public QDialog {
public:
    HydroDialog(ModelSession& session,const QString& mode,const QJsonObject& defaults,bool hidden,QWidget* parent);
    QJsonObject inputs()const;
    QJsonObject result()const{return result_;}
    QString exportReport(const QString& path,const QString& format)const;
    QString exportAuxiliary(const QString& path,int index)const;
    QJsonObject auxiliaryFile(int index)const;
    bool selectAuxiliary(int index);
    void receive(const QJsonObject& result);
    void showFailure(const QString& error,bool finished=true);
    QJsonObject savedInputs()const;
    void refreshState();
    void showProgress(const QJsonObject& progress);
    std::function<void()> onBackground;
    void reject()override;
protected:
    void changeEvent(QEvent*) override;
private:
    void calculate(const QString& report={});
    bool refreshInputValidity();
    void render();
    void renderSingleResult(const QJsonObject& row);
    void renderAuxiliaryPage();
    int currentAuxiliaryIndex()const;
    QByteArray csv()const;
    QString reportText(bool html)const;
    QString draftReport(bool html)const;
    ModelSession& session_;QString mode_;bool closing_=false;
    QMap<QString,QWidget*> fields_;QMap<QString,QLabel*> fieldLabels_;QJsonObject result_;
    StudioMessage* status_;QLabel* legend_;QPushButton *run_,*cancel_;
    QVector<QPushButton*> exports_;QTableWidget* table_;report::Browser* text_;QTabWidget* tabs_;QWidget* plot_=nullptr;
    QVector<QPair<QString,QString>> columns_;
    QWidget* auxiliaryPage_=nullptr;
    QStackedWidget* auxiliaryStack_=nullptr;
    QButtonGroup* auxiliaryChoices_=nullptr;
    QMap<QString,QJsonObject> auxiliaryResults_;
    QMap<QString,QString> auxiliaryWarnings_;
    QVector<QTableView*> auxiliaryViews_;
    StudioMessage* auxiliaryNotice_=nullptr;
    QPushButton* auxiliarySave_=nullptr;
};
