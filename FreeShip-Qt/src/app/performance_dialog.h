#pragma once
#include "model_session.h"
#include <QDialog>
#include <QMap>
#include <QJsonArray>
class QLabel;class QPushButton;class QTableView;class QPlainTextEdit;class QTabWidget;class QComboBox;class QLineEdit;class QScrollArea;class QProcess;class QGridLayout;class QVBoxLayout;
class GridPreviewPanel;
class PerformanceDialog final:public QDialog {
public:
    PerformanceDialog(ModelSession& session,const QJsonObject& schema,bool hidden,QWidget* parent);
    QJsonObject inputs()const;
    QJsonObject result()const{return result_;}
    GridPreviewPanel* gridPreview()const{return preview_;}
    QString exportReport(const QString& path,const QString& format)const;
    QString saveParameters(const QString& path)const;
    QString loadParameters(const QString& path);
    static QString defaultSolverDirectory();
    void receive(const QJsonObject& result,bool defaults);
    void showFailure(const QString& error);
    void settingsSaved();
    void refreshState();
    void reject()override;
protected:
    void resizeEvent(QResizeEvent*) override;
    void changeEvent(QEvent*) override;
    bool eventFilter(QObject*,QEvent*) override;
private:
    void calculate();void setParameters(const QJsonObject& schema);void renderSeries();void refreshChoices();void openDatabaseViewer();void arrangeAdditional();
    QByteArray csv()const;QString reportText()const;
    ModelSession& session_;QString method_;bool closing_=false,hidden_=false,choicesPending_=false;
    QMap<QString,QWidget*> fields_;QJsonObject result_,schema_;
    QLabel* status_;QPushButton *run_,*cancel_,*save_,*extract_;QLineEdit* solver_;
    QVector<QPushButton*> exports_;QTableView* table_;QPlainTextEdit* text_;QTabWidget* tabs_;
    QWidget* plot_;QComboBox *series_,*resultSeries_;QScrollArea* parameters_;
    QTabWidget* pages_=nullptr;QScrollArea* additional_=nullptr;QPlainTextEdit* help_=nullptr;QWidget* overview_=nullptr;
    QVector<QWidget*> additionalGroups_;QGridLayout* additionalLayout_=nullptr;
    QVBoxLayout* additionalColumnLayouts_[2]{};
    int additionalColumns_=0;quint64 parameterGeneration_=0;
    QProcess* databaseViewer_=nullptr;QPushButton* databaseButton_=nullptr;
    GridPreviewPanel* preview_=nullptr;
};
