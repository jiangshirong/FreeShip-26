// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/print_hull.h"
#include <QTemporaryDir>
#include <QString>
#include <QWidget>
#include <functional>
class ModelSession;
class QLabel;
class QListWidget;
class QDoubleSpinBox;
class QCheckBox;
class PrintHullPanel final:public QWidget {
public:
    explicit PrintHullPanel(ModelSession&,QWidget* parent);
    ~PrintHullPanel() override;
    void setActive(bool);
    void syncSource();
    bool pending() const {return pending_;}
    bool ready() const;
    QString failure() const {return error_;}
    QJsonObject evidence() const;
    void prepare(std::function<void()> onReady);
    void save(const QString&) const;
    std::function<void()> changed;
private:
    void invalidate();

    ModelSession& session_;
    std::shared_ptr<const fs::ModelSnapshot> source_;
    std::shared_ptr<manifold::ExecutionContext> job_;
    std::shared_ptr<const fs::printing::Result> result_;
    std::shared_ptr<QTemporaryDir> stage_;
    QDoubleSpinBox *length_,*thickness_;
    QListWidget* layers_;
    QCheckBox* seam_;
    QLabel* message_;
    std::function<void()> onReady_;
    int builds_=0;
    qint64 buildMs_=0,stlMs_=0;
    QString error_;
    bool active_=false,pending_=false;
};
