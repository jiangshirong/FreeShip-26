// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_window.h"
#include "preferences_dialog.h"
#include "performance_dialog.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QMessageBox>
#include <QShortcut>
#include <QStatusBar>
#include <QTimer>
#include <stdexcept>

void MainWindow::loadPreferences(const QString& filename){
    preferencesStore_=std::make_unique<PreferencesStore>(filename);
    try{auto values=preferencesStore_->read();auto error=validatePreferences(values);
        if(!error.isEmpty())throw std::runtime_error(error.toStdString());
        preferences_=values;session_.setPreferences(nativePreferenceOptions(values),values.value("directories").toObject()["temporary"].toString());
        updatePreferenceAppearance();
    }catch(const std::exception& e){showStatusMessage(QString::fromUtf8(e.what()),StudioMessageRole::Warning);}
}
void MainWindow::updatePreferenceAppearance(){
    const auto requested=preferences_.value("ui_language").toString("zh");
    if(requested!=uiLanguage()){
        const auto error=setUiLanguage(requested);
        if(!error.isEmpty())showStatusMessage(error,StudioMessageRole::Warning);
    }
    if(languageShortcut_)languageShortcut_->setEnabled(preferences_.value("quick_language_enabled").toBool());
    activatePreferences(preferences_);const auto colors=preferenceAppearance(preferences_);
    for(auto* view:views_)view->setGraphicPreferences(colors);
    if(studioBar_)studioBar_->setIconSize(preferences_.value("tool_icon_size").toInt(20));
}
void MainWindow::switchUiLanguage(){
    if(!preferencesStore_||preferencesPending_)return;
    try{
        // Read again so changes saved by another window/component are retained.
        auto values=preferencesStore_->read();
        if(!values.value("quick_language_enabled").toBool())return;
        const auto [first,second]=quickUiLanguages(values);
        const auto next=uiLanguage()==first?second:first;
        const auto error=checkUiLanguage(next);
        if(!error.isEmpty()){showError(error);return;}
        values["ui_language"]=next;
        preferencesStore_->save(values);
        // Language changes have no backend request, document revision or undo item.
        preferences_["ui_language"]=next;
        if(preferencesDialog_)preferencesDialog_->setLanguageValue(next);
        activatePreferences(values);
        const auto issue=setUiLanguage(next);
        if(!issue.isEmpty())showError(issue);
        else for(auto* view:views_)view->update();
    }catch(const std::exception& e){showError(QString::fromUtf8(e.what()));}
}
void MainWindow::editPreferences(){
    if(session_.busy())return;
    try{
        if(!preferencesStore_)throw std::runtime_error(uiText("偏好设置的保存位置不可用。"));
        storedPreferences_=preferencesStore_->read();
        // Verification uses its private settings and must not read legacy user QSettings.
        const auto solverDefault=workflow_?QString{}:PerformanceDialog::defaultSolverDirectory();
        PreferencesDialog dialog(storedPreferences_,this,solverDefault);preferencesDialog_=&dialog;
        dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
        dialog.onApply=[this](const QJsonObject& values){applyPreferences(values);};
        dialog.exec();preferencesDialog_=nullptr;
        if(preferencesWorkflow_)QTimer::singleShot(0,this,[this]{preferencesStep("closed");});
    }catch(const std::exception& e){preferencesDialog_=nullptr;showError(QString::fromUtf8(e.what()));}
}
void MainWindow::applyPreferences(const QJsonObject& values){
    if(!preferencesDialog_||preferencesPending_)return;
    const auto error=validatePreferences(values);if(!error.isEmpty()){preferencesDialog_->showFailure(error);return;}
    const auto languageError=checkUiLanguage(values.value("ui_language").toString("zh"));
    if(!languageError.isEmpty()){preferencesDialog_->showFailure(languageError);return;}
    if(values.value("quick_language_enabled").toBool()){
        const auto [first,second]=quickUiLanguages(values);
        for(const auto& id:{first,second}){
            const auto issue=checkUiLanguage(id);
            if(!issue.isEmpty()){preferencesDialog_->showFailure(issue);return;}
        }
        if(first==second){
            preferencesDialog_->showFailure(uiText("请选择两种不同的语言。"));return;
        }
    }
    // Persist first; a rejected backend publication rolls the file back. This
    // avoids applying a setting that could not be saved (full disk/conflict).
    try{preferencesStore_->save(values);}catch(const std::exception& e){preferencesDialog_->showFailure(QString::fromUtf8(e.what()));return;}
    pendingPreferences_=values;preferencesPending_=true;preferencesDialog_->setBusy(true);
    if(session_.ready()&&nativePreferenceOptions(values)!=nativePreferenceOptions(preferences_)){
        if(!session_.configurePreferences(nativePreferenceOptions(values))&&preferencesPending_)preferencesFinished(uiText("设置未能提交，请稍后重试。"));
    }else preferencesFinished();
}
void MainWindow::preferencesFinished(const QString& error){
    if(!preferencesPending_)return;
    preferencesPending_=false;
    if(!error.isEmpty()){
        auto message=error;try{preferencesStore_->save(storedPreferences_);}catch(const std::exception& e){message+=uiText("\n原设置文件未能恢复：")+QString::fromUtf8(e.what());}
        if(preferencesDialog_)preferencesDialog_->showFailure(message);
        return;
    }
    preferences_=pendingPreferences_;storedPreferences_=preferences_;
    session_.setPreferences(nativePreferenceOptions(preferences_),preferences_.value("directories").toObject()["temporary"].toString());
    updatePreferenceAppearance();updateState();showStatusMessage(uiText("偏好设置已保存。"));
    if(preferencesDialog_){preferencesDialog_->setBusy(false);preferencesDialog_->accept();}
}
