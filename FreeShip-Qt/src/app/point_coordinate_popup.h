// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/model_snapshot.h"
#include <QFrame>
#include <QPoint>
#include <array>
#include <functional>
class QDoubleSpinBox;
class QPushButton;

// An inline view overlay: does not resize the document workspace or create a
// native window. The host owns revision validation and the actual model edit.
class PointCoordinatePopup final:public QFrame {
public:
    explicit PointCoordinatePopup(QWidget* parent);
    void present(fs::EntityId id,fs::Vec3 position,const QString& units,const QPoint& anchor);
    fs::EntityId pointId()const{return id_;}
    void setEditingEnabled(bool enabled);
    std::function<void(fs::EntityId,fs::Vec3)> onApply;
    std::function<void()> onCancelPlacement;
protected:
    bool eventFilter(QObject*,QEvent*)override;
private:
    void place();
    void acceptCoordinates();
    std::array<QDoubleSpinBox*,3> fields_{};
    std::array<bool,3> changed_{};
    fs::EntityId id_=0;fs::Vec3 original_{};
    QPoint anchor_;QPushButton* confirm_=nullptr;bool loading_=false;
};
