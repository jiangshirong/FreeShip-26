// FreeShip_26 section management. GPL-2.0-or-later.
#pragma once
#include "core/model_snapshot.h"
#include <QDialog>
#include <QJsonObject>
#include <QSet>
#include <functional>
#include <memory>
class QTabWidget;class QTableWidget;class StudioMessage;class QAction;
class SectionDialog final:public QDialog {
public:
    explicit SectionDialog(QWidget* parent=nullptr);
    void setSnapshot(std::shared_ptr<const fs::ModelSnapshot> model);
    void setEditingEnabled(bool enabled);
    void showError(const QString& message);
    void discardPending();
    int kind()const;
    QSet<int> selectedIndices()const;
    std::function<bool(const QString&,int,QJsonObject)> onEdit;
    std::function<void(int,QSet<int>)> onSelection;
private:
    void refill();void add(bool range);void clearCurrent();
    void selectionChanged();void updateActions();
    void submit(const QString& operation,int sectionKind,QJsonObject fields);
    QVector<fs::Section> draft_;
    int pending_=0;
    QTabWidget* tabs_;QTableWidget* table_;StudioMessage* error_;
    QAction *addOne_,*addRange_,*clear_;
    bool filling_=false,enabled_=false;
    std::shared_ptr<const fs::ModelSnapshot> model_;
};
