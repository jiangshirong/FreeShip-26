// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QObject>
#include <QJsonObject>
#include <functional>
#include <memory>
class QAction;
class QWidget;
class QPushButton;
// Shared update state. Workflow instances never invoke the helper or install.
class StudioUpdateController final:public QObject {
public:
    StudioUpdateController(QWidget* owner,QAction* about,bool workflow,
        std::function<void(std::function<void()>)> withSavedChanges);
    ~StudioUpdateController() override;
    void attachButton(QPushButton* button);
    void receiveReport(const QJsonObject& report);
    void receiveProgress(const QJsonObject& progress);
    void activate();
    void schedule();
    QString state()const;
    QString lastRequestedMode()const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
void installStudioUpdates(QWidget* owner,QAction* about,bool workflow,
    const std::function<void(std::function<void()>)>& withSavedChanges);
void bindAboutUpdateButton(QWidget* owner,QPushButton* button);
