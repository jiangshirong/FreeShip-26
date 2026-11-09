#pragma once

#include "core/model_snapshot.h"
#include <QDialog>
#include <QJsonObject>
#include <memory>

// Edits a small local draft. Construction and cancellation never call the core.
// changes() contains only deliberate edits, expressed in the chosen unit system.
class ProjectDialog final : public QDialog {
public:
    explicit ProjectDialog(const fs::ModelSnapshot& snapshot, QWidget* parent = nullptr);
    ~ProjectDialog() override;
    QJsonObject changes() const;
    QString validationError() const;
    QWidget* fieldEditor(const QString& field) const;
    void accept() override;
protected:
    void changeEvent(QEvent* event) override;
private:
    struct State;
    std::unique_ptr<State> state_;
    void switchUnits(const QString& units);
    void updateSplitState();
};
