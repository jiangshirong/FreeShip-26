#pragma once

#include "core/model_snapshot.h"
#include <QJsonObject>
#include <QWidget>
#include <functional>
#include <memory>
class QDialog;

class LayerPanel final : public QWidget {
public:
    explicit LayerPanel(QWidget* parent = nullptr);
    ~LayerPanel() override;
    void setSnapshot(std::shared_ptr<const fs::ModelSnapshot> snapshot,int historyStates=-1,int highWater=0);
    void previewCreate();
    void setEditingEnabled(bool enabled,bool allowList=false);
    bool canDelete(fs::EntityId id) const;
    void previewDelete(fs::EntityId id);
    bool previewHistory(bool redo);
    void discardPredictions();
    QSet<fs::EntityId> projectedVisibilityChanges() const;
    std::function<void()> onVisibilityPreview;
    std::function<bool(const QString&,QJsonObject)> onListCommand;
    std::function<void()> onMismatch;
    fs::EntityId selectedLayerId() const;
    void setSelectedLayerId(fs::EntityId id);
    QWidget* fieldEditor(const QString& field) const;
    void finishEditing();
    void editLayerName(fs::EntityId id);
    std::function<void()> onCreateLayer;
    std::function<void(fs::EntityId)> onDeleteLayer;
    std::function<void(fs::EntityId,int)> onMoveLayer;
    std::function<void(fs::EntityId,bool)> onControlNetChanged;
    std::function<void()> onSelectionChanged;
private:
    struct State;
    std::unique_ptr<State> state_;
    bool previewValue(fs::EntityId id,const QJsonObject& value);
    void submitProperty(const QString& field,const QJsonValue& value);
    void submitTransparency();
    void commitText();
    void updateValueRow(int index);
    void rebuildRows();
    void refreshRow(int index,bool remove);
    void insertRow(int index,const fs::Layer& layer);
    void loadSelected();
    void updateEnabledState();
    void updateMeasurements();
};

QString layerDisplayName(const fs::Layer& layer);
QDialog* createLayerPropertiesDialog(QWidget* parent,LayerPanel*& panel);
