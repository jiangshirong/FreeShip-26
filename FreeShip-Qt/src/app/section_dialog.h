// FreeShip_26 section management. GPL-2.0-or-later.
#pragma once
#include "core/model_snapshot.h"
#include <QDialog>
#include <QJsonObject>
#include <QSet>
#include <functional>
#include <memory>
class QTabWidget;class QTableWidget;class QLabel;class QAction;
class SectionDialog final:public QDialog {
public:
    explicit SectionDialog(QWidget* parent=nullptr);
    void setSnapshot(std::shared_ptr<const fs::ModelSnapshot> model);
    void setEditingEnabled(bool enabled);
    void showError(const QString& message);
    int kind()const;
    QSet<int> selectedIndices()const;
    std::function<void(const QString&,int,QJsonObject)> onEdit;
    std::function<void(int,QSet<int>)> onSelection;
private:
    void refill();void add(bool range);void remove(bool all);
    void selectionChanged();void updateActions();
    QTabWidget* tabs_;QTableWidget* table_;QLabel* error_;
    QAction *addOne_,*addRange_,*remove_,*clear_;
    bool filling_=false,enabled_=false;
    std::shared_ptr<const fs::ModelSnapshot> model_;
};
