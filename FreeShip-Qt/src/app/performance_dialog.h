#pragma once
#include "model_session.h"
#include <QDialog>
#include <QMap>
#include <QJsonArray>
#include <QThreadPool>
struct CalculationChartState;
class QLabel;class StudioMessage;class QPushButton;class QTableView;class QPlainTextEdit;class QTabWidget;class QComboBox;class QLineEdit;class QScrollArea;class QProcess;class QGridLayout;class QVBoxLayout;
namespace report {class Browser;}
class GridPreviewPanel;
class PerformanceDialog final:public QDialog {
public:
    PerformanceDialog(ModelSession& session,const QJsonObject& schema,bool hidden,QWidget* parent);
    ~PerformanceDialog()override;
    QJsonObject inputs()const;
    QJsonObject result()const{return result_;}
    bool choicesPending()const{return choicesPending_;}
    bool usingDevelopmentSample()const{return developmentSample_;}
    GridPreviewPanel* gridPreview()const{return preview_;}
    QString exportReport(const QString& path,const QString& format)const;
    QString saveParameters(const QString& path)const;
    QString loadParameters(const QString& path);
    static QString defaultSolverDirectory();
    void useSolverDirectory(const QString& path){solverDirectory_=path;}
    void receive(const QJsonObject& result,bool defaults);
    void showFailure(const QString& error);
    void refreshState();
    void syncPrincipalDimensions(const QJsonObject& principal);
    std::function<void()> onBackground;
    void reject()override;
protected:
    void resizeEvent(QResizeEvent*) override;
    void changeEvent(QEvent*) override;
    bool eventFilter(QObject*,QEvent*) override;
private:
    void calculate();void setParameters(const QJsonObject& schema);void renderSeries();void refreshChoices();void refreshAccelerationStopControls();void openDatabaseViewer();void arrangeAdditional();void refreshWindLabels();
    void applyResult(const QJsonObject&,bool,std::shared_ptr<CalculationChartState>);
    QThreadPool resultPool_;quint64 resultGeneration_=0;bool preparingResult_=false;QVector<bool> sharedCurveAxes_;
    bool refreshInputValidity();
    void convertViscosityFromTemperature();
    void loadDevelopmentSample();
    QByteArray csv()const;QString reportText(bool html)const;
    ModelSession& session_;QString method_,solverDirectory_;bool closing_=false,hidden_=false,choicesPending_=false;
    bool developmentSample_=false;
    QMap<QString,QWidget*> fields_;QJsonObject result_,schema_;
    QJsonObject principalSource_;
    StudioMessage* status_;QPushButton *run_,*cancel_;
    QPushButton* sampleButton_=nullptr;
    QVector<QPushButton*> exports_;QTableView* table_;report::Browser* text_;QTabWidget* tabs_;
    QWidget* plot_;QComboBox *series_;QScrollArea* parameters_;
    QTabWidget* pages_=nullptr;QScrollArea* additional_=nullptr;report::Browser* help_=nullptr;
    QVector<QWidget*> additionalGroups_;QGridLayout* additionalLayout_=nullptr;
    QVBoxLayout* additionalColumnLayouts_[2]{};
    int additionalColumns_=0;quint64 parameterGeneration_=0;
    QProcess* databaseViewer_=nullptr;QPushButton* databaseButton_=nullptr;
    GridPreviewPanel* preview_=nullptr;
};
