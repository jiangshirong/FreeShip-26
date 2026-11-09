// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_window.h"
#include "preferences_dialog.h"
#include "studio_ui.h"
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <stdexcept>

void MainWindow::loadPreferences(const QString& filename){
    preferencesStore_=std::make_unique<PreferencesStore>(filename);
    try{auto values=preferencesStore_->read();auto error=validatePreferences(values);
        if(!error.isEmpty())throw std::runtime_error(error.toStdString());
        preferences_=values;session_.setPreferences(nativePreferenceOptions(values),values.value("directories").toObject()["temporary"].toString());
        updatePreferenceAppearance();
    }catch(const std::exception& e){statusBar()->showMessage(QString::fromUtf8(e.what()));}
}
void MainWindow::updatePreferenceAppearance(){
    activatePreferences(preferences_);const auto colors=preferenceAppearance(preferences_);
    for(auto* view:views_)view->setGraphicPreferences(colors);
    if(studioBar_)studioBar_->setIconSize(preferences_.value("tool_icon_size").toInt(20));
}
void MainWindow::editPreferences(){
    if(session_.busy())return;
    try{
        if(!preferencesStore_)throw std::runtime_error("偏好设置的保存位置不可用。");
        storedPreferences_=preferencesStore_->read();
        PreferencesDialog dialog(storedPreferences_,this);preferencesDialog_=&dialog;
        dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
        dialog.onApply=[this](const QJsonObject& values){applyPreferences(values);};
        dialog.exec();preferencesDialog_=nullptr;
        if(preferencesWorkflow_)QTimer::singleShot(0,this,[this]{preferencesStep("closed");});
    }catch(const std::exception& e){preferencesDialog_=nullptr;showError(QString::fromUtf8(e.what()));}
}
void MainWindow::applyPreferences(const QJsonObject& values){
    if(!preferencesDialog_||preferencesPending_)return;
    const auto error=validatePreferences(values);if(!error.isEmpty()){preferencesDialog_->showFailure(error);return;}
    // Persist first; a rejected backend publication rolls the file back. This
    // avoids applying a setting that could not be saved (full disk/conflict).
    try{preferencesStore_->save(values);}catch(const std::exception& e){preferencesDialog_->showFailure(QString::fromUtf8(e.what()));return;}
    pendingPreferences_=values;preferencesPending_=true;preferencesDialog_->setBusy(true);
    if(session_.ready()&&nativePreferenceOptions(values)!=nativePreferenceOptions(preferences_)){
        if(!session_.configurePreferences(nativePreferenceOptions(values))&&preferencesPending_)preferencesFinished("设置未能提交，请稍后重试。");
    }else preferencesFinished();
}
void MainWindow::preferencesFinished(const QString& error){
    if(!preferencesPending_)return;
    preferencesPending_=false;
    if(!error.isEmpty()){
        auto message=error;try{preferencesStore_->save(storedPreferences_);}catch(const std::exception& e){message+="\n原设置文件未能恢复："+QString::fromUtf8(e.what());}
        if(preferencesDialog_)preferencesDialog_->showFailure(message);
        return;
    }
    preferences_=pendingPreferences_;storedPreferences_=preferences_;
    session_.setPreferences(nativePreferenceOptions(preferences_),preferences_.value("directories").toObject()["temporary"].toString());
    updatePreferenceAppearance();updateState();statusBar()->showMessage("偏好设置已保存。");
    if(preferencesDialog_){preferencesDialog_->setBusy(false);preferencesDialog_->accept();}
}
