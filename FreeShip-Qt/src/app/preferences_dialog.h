// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDialog>
#include <QJsonObject>
#include <QVector>
#include <functional>
class QLabel;
class StudioMessage;
class QPushButton;
class QTabWidget;
class QTextBrowser;
class QTimer;
class PreferencesDialog final : public QDialog {
public:
    explicit PreferencesDialog(QJsonObject values,QWidget* parent=nullptr,const QString& solverDefault={},
                               QJsonObject underwaterDefaults={});
    QJsonObject values()const{return draft_;}
    void setBusy(bool busy);
    void saved();
    void setLanguageValue(const QString& id);
    void showFailure(const QString& message);
    void reject() override;
    std::function<void(const QJsonObject&)> onApply;
protected:
    bool event(QEvent* event) override;
private:
    struct GraphicsEdit { QJsonObject before, after; };
    enum class GraphicsHistoryAction { None, Edit, Undo, Redo };
    void buildGraphics();
    void refreshGraphics();
    void changeGraphics(QJsonObject values);
    void applyGraphicsEdit(const GraphicsEdit& edit,bool forward);
    void stepGraphicsHistory(bool redo);
    void importLegacy();
    void scheduleApply();
    void applyChanges();
    QJsonObject draft_;
    QJsonObject underwaterDefaults_;
    QTabWidget* tabs_=nullptr;
    QWidget* graphics_=nullptr;
    StudioMessage* error_;
    QTextBrowser* importSummary_;
    QPushButton* import_;
    QTimer* applyTimer_;
    QVector<GraphicsEdit> graphicsHistory_;
    qsizetype graphicsHistoryIndex_=0;
    GraphicsEdit pendingGraphicsEdit_;
    GraphicsHistoryAction graphicsAction_=GraphicsHistoryAction::None;
    bool busy_=false, closeAfterApply_=false, importPending_=false;
};

bool chooseInitialLanguages(QWidget* parent,QJsonObject* values);
