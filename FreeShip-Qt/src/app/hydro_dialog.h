#pragma once
#include "model_session.h"
#include <QDialog>
#include <QMap>
#include <QJsonArray>
#include <memory>
struct CalculationChartState;
class QLabel;class QPushButton;class QTableWidget;class QTableView;class QTabWidget;
namespace report {class Browser;struct PdfTableLayout;}
namespace loadingEditor {class Workspace;}
class StudioMessage;
class QStackedWidget;class QButtonGroup;
class CalculationLayers;
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
    CalculationLayers* layerChoice_=nullptr;
    QJsonObject loadingBook_;loadingEditor::Workspace* loadingEditor_=nullptr;QTableWidget* comparison_=nullptr;
    bool useLoadingBook_=false;
    QMap<QString,QWidget*> loadingFields_;
    QMap<QString,QLabel*> loadingFieldLabels_;
    QWidget* loadingSummary_=nullptr;QLabel* loadingSummaryTitle_=nullptr;QTableWidget* loadingValues_=nullptr;
    StudioMessage* loadingNotice_=nullptr;
    QString loadingError_;
    void refreshLoadingSummary();
    void translateLoadingInputs();
    void calculate(const QString& report={});
    bool refreshInputValidity();
    void render();
    void refreshOutputTabs();
    void renderCurve(const QJsonArray& rows,const QString& lengthUnit,const QString& massUnit);
    void renderSingleResult(const QJsonObject& row);
    void renderAuxiliaryPage();
    int currentAuxiliaryIndex()const;
    QByteArray csv()const;
    QString reportText(bool html,const report::PdfTableLayout* pdf=nullptr)const;
    QString draftReport(bool html,const report::PdfTableLayout* pdf)const;
    ModelSession& session_;QString mode_;bool closing_=false;
    QMap<QString,QWidget*> fields_;QMap<QString,QLabel*> fieldLabels_;QJsonObject result_;
    StudioMessage* status_;QPushButton *run_,*cancel_;
    QVector<QPushButton*> exports_;QTableWidget* table_;report::Browser* text_;QTabWidget* tabs_;QWidget* plot_=nullptr;
    std::shared_ptr<CalculationChartState> stabilityChart_;
    QVector<QPair<QString,QString>> columns_;
    QWidget *parameterPage_=nullptr,*loadingPage_=nullptr,*auxiliaryPage_=nullptr;
    QStackedWidget* auxiliaryStack_=nullptr;
    QButtonGroup* auxiliaryChoices_=nullptr;
    QMap<QString,QJsonObject> auxiliaryResults_;
    QMap<QString,QString> auxiliaryWarnings_;
    QVector<QTableView*> auxiliaryViews_;
    StudioMessage* auxiliaryNotice_=nullptr;
    QPushButton* auxiliarySave_=nullptr;
};
