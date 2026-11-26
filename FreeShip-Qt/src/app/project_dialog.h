#pragma once

#include "core/model_snapshot.h"
#include <QDialog>
#include <QJsonObject>
#include <memory>
#include <functional>

// Submits validated edits through the owning document session.
// changes() contains only deliberate edits, expressed in the chosen unit system.
class ProjectDialog final : public QDialog {
public:
    explicit ProjectDialog(const fs::ModelSnapshot& snapshot, QWidget* parent = nullptr);
    ~ProjectDialog() override;
    QJsonObject changes() const;
    QString validationError() const;
    QWidget* fieldEditor(const QString& field) const;
    bool matchesDocument(const fs::ModelSnapshot& snapshot) const;
    bool matchesRevision(const fs::ModelSnapshot& snapshot) const;
    void refreshSnapshot(const fs::ModelSnapshot& snapshot);
    void setEditingEnabled(bool enabled);
    void reject() override;
    void applyChanges(bool alignBaseline=false);
    std::function<void(const QJsonObject&)> onApply;
protected:
    bool eventFilter(QObject* watched,QEvent* event) override;
private:
    struct State;
    std::unique_ptr<State> state_;
    void scheduleApply();
    void switchUnits(const QString& units);
    void updateSplitState();
    void updatePrincipalState();
    QJsonObject principalDimensions()const;
};
