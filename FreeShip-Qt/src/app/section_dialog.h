// FreeShip_26 section management. GPL-2.0-or-later.
#pragma once
#include "core/model_snapshot.h"
#include <QDialog>
#include <QJsonObject>
#include <QSet>
#include <functional>
#include <memory>
class QTabWidget;class QTableWidget;class StudioMessage;class QAction;class QPushButton;
class SectionDialog final:public QDialog {
public:
    explicit SectionDialog(QWidget* parent=nullptr);
    void setSnapshot(std::shared_ptr<const fs::ModelSnapshot> model,int historyStates=-1);
    void setEditingEnabled(bool enabled);
    void showError(const QString& message);
    void discardPending();
    void previewHistory(bool redo);
    std::function<bool(bool)> onPreviewHistory;
    std::function<void()> onPredictionMismatch;
    int kind()const;
    QSet<int> selectedIndices()const;
    std::function<bool(const QString&,int,QJsonObject)> onEdit;
    std::function<void(int,QSet<int>)> onSelection;
    std::function<void(QSet<int>)> onTemplates;
    std::function<void(int)> onComparison;
private:
    void refreshHeaders();void refill();void add(bool range);void clearCurrent();
    void selectionChanged();void updateActions();
    void submit(const QString& operation,int sectionKind,QJsonObject fields);
    QVector<fs::Section> draft_;
    // Presentation only: compact definitions, no spline/mesh data. The document
    // session still executes every undo/redo and checks every committed state.
    using Rows=QVector<fs::Section>;
    QList<Rows> before_,after_,expected_;
    Rows rendered_;
    int renderedKind_=0,pending_=0;
    QTabWidget* tabs_;QTableWidget* table_;StudioMessage* error_;
    QAction *addOne_,*addRange_,*clear_;
    QPushButton *templates_, *comparison_;
    bool filling_=false,enabled_=false;
    std::shared_ptr<const fs::ModelSnapshot> model_;
};
