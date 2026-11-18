// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "render/view_lighting.h"
#include <QDialog>
#include <QHash>
#include <functional>
class QCheckBox;
class QSpinBox;
class LightDialog final:public QDialog {
public:
    explicit LightDialog(QWidget* parent=nullptr);
    void setLight(const HullLight& light);
    std::function<void(const HullLight&)> onLightChanged;
private:
    void publish();
    QCheckBox* enabled_=nullptr;
    QHash<QString,QSpinBox*> fields_;
    bool loading_=false;
};
