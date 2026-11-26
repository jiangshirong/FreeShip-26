// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_window.h"
#include "preferences_dialog.h"
#include "performance_dialog.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QApplication>
#include <QKeyEvent>
#include <QMessageBox>
#include <QShortcut>
#include <QSettings>
#include <QScopedValueRollback>
#include <QAction>
#include <QStatusBar>
#include <QTimer>
#include <stdexcept>

void MainWindow::loadPreferences(const QString& filename){
    preferencesStore_=std::make_unique<PreferencesStore>(filename);
    try{auto values=preferencesStore_->read();auto error=validatePreferences(values);
        if(!error.isEmpty())throw std::runtime_error(error.toStdString());
        preferences_=values;session_.setPreferences(nativePreferenceOptions(values),values.value("directories").toObject()["temporary"].toString());
        updatePreferenceAppearance();
        const auto path=preferencesStore_->filename()+".views.ini";
        QSettings settings(path,QSettings::IniFormat);
        for(auto* view:views_){
            const auto prefix="projection/"+QString::number(int(view->projection()))+"/";
            const auto value=[&](const QString& key){return settings.value(prefix+key);};
            view->onViewSettingsChanged={};
            view->restoreDisplayMode(value("mode").toString());
            view->setCoordinateAxis(value("axis").toInt());
            view->setFocalLength(value("lens").toInt());
            const auto components=value("light").toList();
            if(components.size()==6){
                HullLight light;
                light.direction={components[0].toDouble(),components[1].toDouble(),components[2].toDouble()};
                light.ambient=components[3].toInt();light.intensity=components[4].toInt();light.enabled=components[5].toBool();
                view->setLight(light);
            }
            view->onViewSettingsChanged=[this,path,prefix,view](const QString& key){
                QVariant setting;
                if(key=="mode")setting=view->displayModeKey();
                else if(key=="axis")setting=view->coordinateAxis();
                else if(key=="lens")setting=view->focalLength();
                else if(key=="light"){
                    refreshLightDialog(view);
                    const auto l=view->light();
                    setting=QVariantList{l.direction.x,l.direction.y,l.direction.z,l.ambient,l.intensity,l.enabled};
                }else return;
                QSettings(path,QSettings::IniFormat).setValue(prefix+key,setting);
            };
        }
        for(auto* action:findChildren<QAction*>()){
            const auto key=action->property("viewPreference").toString();
            if(key.isEmpty())continue;
            const auto saved=settings.value(key);
            if(saved.isValid()){
                action->setChecked(saved.toBool());
                // File-provided overlay defaults apply until the first explicit choice.
                if(key=="overlay_grid")for(auto* v:views_)v->setGridVisible(saved.toBool());
                if(key=="overlay_hydro")for(auto* v:views_)v->setHydroVisible(saved.toBool());
            }
            if(!action->property("viewPreferenceBound").toBool()){
                action->setProperty("viewPreferenceBound",true);
                connect(action,&QAction::triggered,this,[this,key,action]{
                    QSettings(preferencesStore_->filename()+".views.ini",QSettings::IniFormat).setValue(key,action->isChecked());
                });
            }
        }
        if(std::none_of(viewActions_.begin(),viewActions_.end(),[](auto* a){return a->isChecked();}))
            viewActions_.back()->setChecked(true);
        layoutViews();
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
bool MainWindow::eventFilter(QObject* watched,QEvent* event){
    // Qt can block a shortcut owned by the main window while a modal child is active.
    // Handle the same binding at the focused widget, without intercepting other keys.
    if(languageShortcut_&&languageShortcut_->isEnabled()&&
       (event->type()==QEvent::ShortcutOverride||event->type()==QEvent::KeyPress)){
        const auto* key=static_cast<QKeyEvent*>(event);
        if(QKeySequence(key->keyCombination())==languageShortcut_->key()){
            for(auto* owner=watched;owner;owner=owner->parent())if(owner==this){
                event->accept();
                if(event->type()==QEvent::KeyPress&&!key->isAutoRepeat())switchUiLanguage();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched,event);
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
    if(preferencesDialog_){preferencesDialog_->raise();preferencesDialog_->activateWindow();return;}
    if(session_.busy())return;
    try{
        if(!preferencesStore_)throw std::runtime_error(uiText("偏好设置的保存位置不可用。"));
        storedPreferences_=preferencesStore_->read();
        // Verification uses its private settings and must not read legacy user QSettings.
        const auto solverDefault=workflow_?QString{}:PerformanceDialog::defaultSolverDirectory();
        QJsonObject underwaterDefaults;
        if(const auto model=session_.model())
            underwaterDefaults={{"shade",model->shadeUnderwater},{"color",qint64(model->underwaterColor)}};
        PreferencesDialog dialog(storedPreferences_,this,solverDefault,underwaterDefaults);preferencesDialog_=&dialog;
        dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_);
        dialog.onApply=[this](const QJsonObject& values){applyPreferences(values);};
        runStudioDialog(dialog);preferencesDialog_=nullptr;
        if(preferencesWorkflow_)QTimer::singleShot(0,this,[this]{preferencesStep("closed");});
    }catch(const std::exception& e){preferencesDialog_=nullptr;showError(QString::fromUtf8(e.what()));}
}
void MainWindow::applyPreferences(const QJsonObject& values){
    if(!preferencesDialog_||preferencesPending_)return;
    if(values==storedPreferences_){preferencesDialog_->saved();return;}
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
    if(preferencesDialog_)preferencesDialog_->saved();
}
