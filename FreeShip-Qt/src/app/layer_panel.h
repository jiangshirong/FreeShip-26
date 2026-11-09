#pragma once

#include "core/model_snapshot.h"
#include <QJsonObject>
#include <QWidget>
#include <functional>
#include <memory>
class QAction;

// Existing layers only. Properties are submitted as one explicit edit command.
class LayerPanel final : public QWidget {
public:
    explicit LayerPanel(QWidget* parent = nullptr);
    ~LayerPanel() override;
    void setSnapshot(std::shared_ptr<const fs::ModelSnapshot> snapshot);
    void setEditingEnabled(bool enabled);
    fs::EntityId selectedLayerId() const;
    void setSelectedLayerId(fs::EntityId id);
    QJsonObject changes() const;
    QString validationError() const;
    QWidget* fieldEditor(const QString& field) const;
    void applyChanges();
    void setManagementActions(const QList<QAction*>& actions);
    void discardDrafts();
    std::function<void(fs::EntityId,bool)> onControlNetChanged;
    std::function<void()> onSelectionChanged;
    std::function<void(fs::EntityId, const QJsonObject&)> onLayerChanged;
private:
    struct State;
    std::unique_ptr<State> state_;
    void loadSelected();
    void updateEnabledState();
    void updateMeasurements();
};
