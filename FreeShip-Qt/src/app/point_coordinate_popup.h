// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/model_snapshot.h"
#include <QFrame>
#include <QPoint>
#include <array>
#include <functional>
enum class HullProjection;
class QDoubleSpinBox;
class QPushButton;
class QLineEdit;class QComboBox;class QStackedWidget;class QLabel;

// An inline view overlay: does not resize the document workspace or create a
// native window. The host owns revision validation and the actual model edit.
class PointCoordinatePopup final:public QFrame {
public:
    explicit PointCoordinatePopup(QWidget* parent);
    void present(fs::EntityId id,fs::Vec3 position,const QString& units,HullProjection projection,const QPoint& anchor,int clearance,bool focusField,bool properties,const QString& name,int type,const QString& layers);
    void presentEntity(bool edge,const QPoint& anchor,const QString& layers,int count,bool crease);
    void follow(fs::Vec3 position,const QPoint& anchor);
    fs::EntityId pointId()const{return id_;}
    void setEditingEnabled(bool enabled,bool movable,bool typeEditable);
    std::function<void(fs::EntityId,const QString&,int)> onProperties;
    std::function<void(fs::EntityId,fs::Vec3)> onApply;
    std::function<bool()> canUndoPlacement;
    std::function<void()> undoPlacement,onCancelPlacement;
protected:
    bool eventFilter(QObject*,QEvent*)override;
private:
    QStackedWidget* pages_;QLineEdit* name_;QComboBox* type_;QLabel* heading_;QLabel *pointLayers_,*entityLayers_,*countTitle_,*countValue_,*creaseValue_;
    void place();
    void acceptCoordinates();
    std::array<QDoubleSpinBox*,3> fields_{};
    std::array<bool,3> changed_{};
    fs::EntityId id_=0;fs::Vec3 original_{};
    int clearance_=12;QPoint anchor_;QPushButton* confirm_=nullptr;bool loading_=false;
};
