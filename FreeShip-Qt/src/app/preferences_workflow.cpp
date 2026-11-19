// SPDX-License-Identifier: GPL-2.0-or-later
// Hidden observation adapter for the production preferences action and dialog.
#include "main_window.h"
#include <QTextEdit>
#include <QSplitter>
#include "preferences_dialog.h"
#include "exchange_dialog.h"
#include "report_message_probe.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <algorithm>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif
namespace {
QByteArray bytes(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray{};}
QJsonObject actionEvidence(const QAction& action) {
    QJsonObject entry{{"text", action.text()}, {"operation", action.property("operation").toString()},
        {"object_name", action.objectName()},
        {"document_operation", action.property("document_operation").toString()},
        {"shortcut", action.shortcut().toString(QKeySequence::PortableText)},
        {"visible", action.isVisible()}, {"enabled", action.isEnabled()},
        {"checkable", action.isCheckable()}, {"checked", action.isChecked()}};
    for(const char* key:{"file_operation","view_operation","overlay_kind","background_operation","focal_mm","shade_mode","coordinate_axis"}){
        const auto value=action.property(key);
        if(value.isValid())entry[key]=QJsonValue::fromVariant(value);
    }
    return entry;
}
QJsonArray menuEvidence(const QMenu& menu) {
    QJsonArray entries;
    for (auto* action : menu.actions()) {
        if (action->isSeparator()) continue;
        auto entry = actionEvidence(*action);
        if (const auto* submenu = action->menu()) {
            entry["menu_role"]=submenu->property("menu_role").toString();
            entry["children"] = menuEvidence(*submenu);
        }
        entries.append(entry);
    }
    return entries;
}
int menuColorPixels(const QPixmap& rendered, const QRect& area, QColor color) {
    const auto pixels = rendered.toImage();
    const auto scale = rendered.devicePixelRatio();
    const auto region = QRectF(area.topLeft() * scale, QSizeF(area.size()) * scale).toAlignedRect().intersected(pixels.rect());
    int count = 0;
    for (int y = region.top(); y <= region.bottom(); ++y)
        for (int x = region.left(); x <= region.right(); ++x)
            if (pixels.pixelColor(x, y) == color) ++count;
    return count;
}
}
bool MainWindow::captureStudioMenus(bool includeViewports){
    QJsonArray shortcuts;
    QJsonObject evidence;
    const auto captureToolbar=[this,includeViewports](QJsonObject& observation,bool saveExample){
        QJsonArray toolbar;
        for(auto* button:studioBar_->buttons()){
            const auto* action=button->defaultAction();
            if(!action)continue;
            auto entry=actionEvidence(*action);
            entry["button_visible"]=button->isVisible();
            if(auto* menu=action->menu()){
                entry["children"]=menuEvidence(*menu);
                if(action->objectName()=="StudioMenu"){
                    observation["entries"]=entry["children"];
                    if(saveExample){
                        menu->resize(menu->sizeHint());
                        const auto image=workflowOutput_+"/menu.png";
                        if(menu->grab().save(image))workflowArtifacts_["menu_image"]=image;
                    }
                }
                if (includeViewports && saveExample && action->objectName() == "StudioObserve") {
                    const auto items = menu->actions();
                    const auto grid = std::find_if(items.begin(), items.end(), [](const auto* item) {
                        return item->property("operation") == "overlay_grid";
                    });
                    if (grid == items.end() || (*grid)->icon().isNull()) {
                        workflowCases_.append(QJsonObject{{"name","visibility-check-mark"},{"status","not_run"}});
                    } else {
                        auto* item = *grid;
                        const QSignalBlocker block(item);
                        const bool initial = item->isChecked();
                        menu->ensurePolished();
                        menu->resize(menu->sizeHint());
                        const auto row = menu->actionGeometry(item);
                        const auto gutter = row.adjusted(row.width() - 28, 0, 0, 0);
                        const QColor badge("#d6ebe2");
                        item->setChecked(true);
                        const auto checked = menu->grab();
                        menu->setActiveAction(item);
                        const auto focused = menu->grab();
                        const auto example = workflowOutput_ + "/visibility-menu.png";
                        if (focused.save(example)) workflowArtifacts_["visibility_menu_image"] = example;
                        item->setChecked(false);
                        const auto unchecked = menu->grab();
                        menu->setActiveAction(nullptr);
                        item->setChecked(initial);
                        workflowCase("menu-checked-icon-has-trailing-badge", menuColorPixels(checked, gutter, badge) > 40);
                        workflowCase("menu-unchecked-has-empty-gutter", menuColorPixels(unchecked, gutter, badge) == 0);
                        workflowCase("menu-hover-independent-of-check-state",
                            menuColorPixels(focused, gutter, badge) > 40 && menuColorPixels(unchecked, row, QColor("#e0efe9")) > 100);
                    }
                }
            }
            toolbar.append(entry);
        }
        observation["toolbar"]=toolbar;
    };
    captureToolbar(evidence,uiLanguage()=="zh");
    for(const auto* shortcut:findChildren<QShortcut*>())
        for(const auto& key:shortcut->keys())shortcuts.append(key.toString(QKeySequence::PortableText));
    evidence["window_shortcuts"]=shortcuts;
    if(includeViewports){
        const auto captureViewport=[this](HullView* view,bool filled,bool saveExample){
            QContextMenuEvent event(QContextMenuEvent::Mouse,{100,100},{100,100});
            QApplication::sendEvent(view,&event);
            QList<QMenu*> menus;
            for(auto* menu:view->findChildren<QMenu*>(QString{},Qt::FindDirectChildrenOnly))
                if(menu->isVisible())menus.append(menu);
            QJsonObject viewport{{"projection",int(view->projection())},{"filled",filled},
                {"candidates",menus.size()},{"language",uiLanguage()}};
            if(menus.size()==1){
                auto* menu=menus.front();
                viewport["entries"]=menuEvidence(*menu);
                if(saveExample){
                    const auto suffix=filled?"-fill.png":"-wire.png";
                    const auto image=workflowOutput_+"/context-zh-"+QString::number(int(view->projection()))+suffix;
                    if(menu->grab().save(image))viewport["image"]=image;
                    if (view->projection() == HullProjection::Perspective && !filled) {
                        for (auto* action : menu->actions()) {
                            auto* axes = action->menu();
                            if (!axes || axes->property("menu_role") != "coordinates") continue;
                            axes->ensurePolished();
                            axes->resize(axes->sizeHint());
                            const auto rendered = axes->grab();
                            const auto example = workflowOutput_ + "/coordinate-menu.png";
                            if (rendered.save(example)) workflowArtifacts_["coordinate_menu_image"] = example;
                            for (auto* axis : axes->actions()) {
                                if (!axis->isChecked()) continue;
                                const auto row = axes->actionGeometry(axis);
                                // 2026-11-12: coordinate actions now have vector
                                // icons; the checked badge must remain separate.
                                workflowCase("menu-coordinate-radio-has-icon-and-trailing-badge", !axis->icon().isNull() &&
                                    menuColorPixels(rendered, row.adjusted(row.width() - 28, 0, 0, 0), QColor("#d6ebe2")) > 40);
                            }
                        }
                    }
                }
                menu->close();
                QCoreApplication::sendPostedEvents(menu,QEvent::DeferredDelete);
            }
            return viewport;
        };
        const auto initialLanguage=uiLanguage();
        QJsonArray observations;
        for(const auto& entry:uiLanguages()){
            const auto language=entry.toObject()["id"].toString();
            const auto error=setUiLanguage(language);
            QJsonObject observation{{"id",language},{"error",error}};
            if(error.isEmpty()){
                captureToolbar(observation,false);
                QJsonArray viewports;
                for(auto* view:views_){
                    viewports.append(captureViewport(view,false,language=="zh"));
                    view->setFillVisible(true);
                    viewports.append(captureViewport(view,true,language=="zh"));
                    view->setFillVisible(false);
                }
                observation["viewports"]=viewports;
            }
            observations.append(observation);
        }
        setUiLanguage(initialLanguage);
        evidence["languages"]=observations;
    }
    QFile file(workflowOutput_+"/menu.json");
    if(!file.open(QIODevice::WriteOnly)||file.write(QJsonDocument(evidence).toJson(QJsonDocument::Compact))<0)return false;
    workflowArtifacts_["menu"]=file.fileName();
    return true;
}
void MainWindow::verifyPreferences(const QString& request,const QString& output){
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    workflow_=true;preferencesWorkflow_=true;workflowOutput_=QFileInfo(output).absoluteFilePath();workflowTimer_.start();QDir().mkpath(workflowOutput_);
    QJsonParseError error;const auto config=QJsonDocument::fromJson(bytes(request),&error);
    if(error.error!=QJsonParseError::NoError||!config.isObject()){finishWorkflow("Invalid preferences probe request");return;}
    preferencesRequest_=config.object();loadPreferences(preferencesRequest_.value("settings_file").toString());
    // Initialize the actual visible canvas hierarchy before observing overlay
    // visibility and geometry; a later screenshot must not be the first show.
    setAttribute(Qt::WA_DontShowOnScreen);ensurePolished();show();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if(preferencesRequest_.value("observe_menu").toBool()&&!captureStudioMenus(false)){
        finishWorkflow("Cannot write menu observations");
        return;
    }
    for(auto* view:views_)view->setAnimationsEnabled(false);
    QTimer::singleShot(120000,this,[this]{if(!workflowFinished_){if(preferencesDialog_){preferencesDialog_->setBusy(false);preferencesDialog_->reject();}finishWorkflow("Preferences probe timed out");}});
    preferencesStep("begin");
}
void MainWindow::preferencesStep(const QString& operation){
    if(workflowFinished_)return;
    try{
        const auto workspace=[this]{
            QJsonArray frames;for(auto* frame:viewFrames_){const auto r=frame->geometry();frames.append(QJsonArray{r.x(),r.y(),r.width(),r.height()});}
            return QJsonObject{{"panes",qobject_cast<QSplitter*>(centralWidget())->count()},{"frames",frames}};
        };
        int next=preferencesRequest_.value("next").toInt();auto observations=preferencesRequest_.value("observations").toArray();
        if(operation!="begin"){
            auto item=preferencesRequest_.value("dialog_observation").toObject();preferencesRequest_.remove("dialog_observation");
            item["operation"]=operation;item["active"]=preferences_;item["persisted"]=QJsonDocument::fromJson(bytes(preferencesStore_->filename())).object();
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            item["workspace_before"]=preferencesRequest_["workspace_before"];item["workspace_after"]=workspace();
            if(auto* notice=findChild<QWidget*>("ShellStatusNotice")){
                const auto* text=notice->findChild<QTextEdit*>("ShellStatusNoticeText");
                item["feedback"]=QJsonObject{{"visible",notice->isVisible()},{"text",text?text->toPlainText():QString{}},
                    {"parent_is_splitter",bool(qobject_cast<QSplitter*>(notice->parentWidget()))},
                    {"inside_canvas",notice->parentWidget()->contentsRect().contains(notice->geometry())},
                    {"height",notice->height()},{"canvas_height",notice->parentWidget()->height()}};
                if(operation=="feedback_visible")grab().save(workflowOutput_+"/preferences-feedback.png");
            }
            item["ui_language"]=uiLanguage();item["preferences_caption"]=preferencesAction_->text();
            if(preferencesRequest_["observe_view_titles"].toBool()){
                QJsonArray titles;
                for(auto* view:views_){
                    const auto capture=[&](bool studio){
                        view->image();const auto painted=view->overlayEvidence();
                        titles.append(QJsonObject{{"studio",studio},{"projection",int(view->projection())},
                            {"painted_title",painted["projection_title_text"]},{"accessible_name",view->accessibleName()},
                            {"text_width",painted["projection_title_text_width"]},{"available_width",painted["projection_title_available_width"]}});
                    };
                    capture(true);
                    if(preferencesRequest_["observe_plain_view_titles"].toBool()){view->setStudioAppearance(false);capture(false);view->setStudioAppearance(true);}
                }
                item["viewport_titles"]=titles;
            }
            item["language_shortcut"]=languageShortcut_->key().toString(QKeySequence::PortableText);
            item["language_switch_enabled"]=languageShortcut_->isEnabled();
            QJsonArray captions;for(auto* a:findChildren<QAction*>())if(a->property("operation").isValid())captions.append(actionEvidence(*a));item["actions"]=captions;
            item["appearance"]=preferenceAppearance(preferences_);
            item["modified"]=session_.modified();item["undo"]=session_.canUndo();item["redo"]=session_.canRedo();item["toolbar_height"]=studioBar_->height();
            item["export_start"]=preferenceFileLocation("export","example.dxf");item["open_start"]=preferenceFileLocation("open");
            if(session_.model()){
                const auto model=session_.model();item["revision"]=double(model->revision);item["point_size"]=model->overlay.pointSize;item["font_size"]=model->overlay.fontSize;
                item["selected"]=views_[0]->selectedPoints().contains(workflowPoint_);item["coordinate_draft"]=coordinates_[0]->value();item["coordinate_changed"]=coordinatesChanged_[0];
                const auto at=views_[0]->project({0,0,0});item["camera_origin"]=QJsonArray{at.x(),at.y()};
                item["camera_origin_from_center"]=QJsonArray{at.x()-views_[0]->width()/2.,at.y()-views_[0]->height()/2.};
                const auto folder=workflowOutput_+'/'+QString::number(next);QDir().mkpath(folder);
                ModelSession::atomicCopy(session_.modelPath(),folder+"/model.json");ModelSession::atomicCopy(session_.dxfPath(),folder+"/lines.dxf");
                item["workspace_model"]=session_.modelPath();
                views_[0]->image().save(folder+"/view.png");item["model"]=folder+"/model.json";item["dxf"]=folder+"/lines.dxf";item["image"]=folder+"/view.png";
            }
            observations.append(item);preferencesRequest_["observations"]=observations;
        }
        const auto commands=preferencesRequest_.value("commands").toArray();
        if(next>=commands.size()){
            QFile f(workflowOutput_+"/observations.json");if(!f.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot write preference observations");f.write(QJsonDocument(observations).toJson());
            workflowArtifacts_["observations"]=f.fileName();workflowCase("preference_commands_observed",true);
            workflowCases_.append(QJsonObject{{"name","desktop_acceptance"},{"status","not_run"}});finishWorkflow();return;
        }
        const auto command=commands[next].toObject();preferencesRequest_["next"]=next+1;
        QTimer::singleShot(0,this,[this,command,workspace]{try{
            const auto kind=command.value("kind").toString();
            if(kind=="load"){session_.load(command.value("path").toString());return;}
            if(kind=="save"){session_.save(command.value("path").toString());return;}
            if(kind=="undo"){session_.undo();return;}
            if(kind=="redo"){session_.redo();return;}
            if(kind=="observe"){preferencesStep("observe");return;}
            if(kind=="wait_feedback"){QTimer::singleShot(1600,this,[this]{preferencesStep("feedback_elapsed");});return;}
            if(kind=="observe_feedback"){QTimer::singleShot(200,this,[this]{preferencesStep("feedback_visible");});return;}
            if(kind=="switch_language_guide"){
                QAction* guide=nullptr;
                for(auto* action:findChildren<QAction*>())if(action->property("operation")=="shortcut_guide")guide=action;
                if(!guide)throw std::runtime_error("Shortcut guide action unavailable");
                const auto before=uiLanguage();
                QTimer::singleShot(0,this,[this,before]{
                    auto* dialog=QApplication::activeModalWidget();
                    if(!dialog||dialog->objectName()!="ShortcutGuide")return;
                    QApplication::setActiveWindow(dialog);
                    QKeyEvent press(QEvent::KeyPress,Qt::Key_L,Qt::ControlModifier|Qt::ShiftModifier,"l");
                    QApplication::sendEvent(dialog,&press);
                    if(dialog->isVisible())dialog->close();
                    if(uiLanguage()==before)return;
                    QTimer::singleShot(0,this,[this]{
                        auto* translated=QApplication::activeModalWidget();
                        auto* table=translated?translated->findChild<QTableWidget*>("ShortcutGuideTable"):nullptr;
                        workflowCase("language.guide-reopened-with-translated-table",table&&
                            table->horizontalHeaderItem(0)->text()==uiText("操作"));
                        if(translated)translated->close();
                    });
                });
                guide->trigger();
                workflowCase("language.shortcut-while-guide-is-open",uiLanguage()!=before);
                preferencesStep("switch_language_guide");return;
            }
            if(kind=="switch_language"||kind=="switch_language_native"){
                show();
                activateWindow();
                QApplication::setActiveWindow(this);
                auto* view=views_.front();
                view->setFocus();
                if(kind=="switch_language_native"){
#ifdef Q_OS_WIN
                    if(!testAttribute(Qt::WA_DontShowOnScreen))throw std::runtime_error("Native key test requires a private window");
                    BYTE original[256]{};
                    if(!GetKeyboardState(original))throw std::runtime_error("Cannot capture private thread keyboard state");
                    BYTE chord[256]{};
                    chord[VK_CONTROL]=chord[VK_LCONTROL]=chord[VK_MENU]=chord[VK_LMENU]=0x80;
                    if(!SetKeyboardState(chord))throw std::runtime_error("Cannot set private thread keyboard state");
                    SendMessageW(reinterpret_cast<HWND>(winId()),WM_SYSKEYDOWN,'L',1|(1L<<29));
                    SetKeyboardState(original);
                    QTimer::singleShot(0,this,[this]{preferencesStep("switch_language_native");});
                    return;
#else
                    throw std::runtime_error("Native Windows key test requires Windows");
#endif
                }
                const auto modifiers=Qt::ControlModifier|(command["retired"].toBool()?Qt::AltModifier:Qt::ShiftModifier);
                QKeyEvent press(QEvent::KeyPress,Qt::Key_L,modifiers,"l");
                QApplication::sendEvent(view,&press);
                QKeyEvent release(QEvent::KeyRelease,Qt::Key_L,Qt::NoModifier);
                QApplication::sendEvent(view,&release);
                preferencesStep("switch_language");return;
            }
            if(kind=="move"){
                for(const auto& point:session_.model()->points)if(!point.locked&&point.batchMovable){workflowPoint_=point.id;auto p=point.position;p.x+=0.125;session_.movePoint(point.id,p);return;}
                throw std::runtime_error("Fixture has no movable point");
            }
            if(kind=="draft"){
                if(!workflowPoint_)workflowPoint_=session_.model()->points.front().id;
                selectPoint(workflowPoint_);coordinates_[0]->setValue(command.value("value").toDouble());views_[0]->panBy({23,17});views_[0]->zoomAt({180,150},1.1);
                QTimer::singleShot(180,this,[this]{preferencesStep("draft");});return;
            }
            if(kind!="preferences")throw std::runtime_error("Unknown preferences probe command");
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);preferencesRequest_["workspace_before"]=workspace();
            QList<QAction*> actions;for(auto* action:findChildren<QAction*>())if(action->property("operation")=="preferences"&&action->isEnabled())actions.append(action);
            if(actions.size()!=1){workflowCases_.append(QJsonObject{{"name","preferences_action_unavailable"},{"status","not_run"}});finishWorkflow();return;}
            QTimer::singleShot(0,this,[this,command]{try{
                if(!preferencesDialog_)throw std::runtime_error("Preferences callback did not open a dialog");
                auto* dialog=preferencesDialog_;auto fields=command.value("fields").toObject();QJsonArray discovered;
                QJsonArray imports;
                for(const auto& file:command.value("legacy_imports").toArray()){
                    QList<QPushButton*> buttons;for(auto* b:dialog->findChildren<QPushButton*>())if(b->property("preference_action")=="import_legacy")buttons.append(b);
                    if(buttons.size()!=1){workflowCases_.append(QJsonObject{{"name","preferences_import_unavailable"},{"status","not_run"}});dialog->reject();finishWorkflow();return;}
                    bool handled=false;
                    QTimer::singleShot(0,dialog,[dialog,file,&handled]{
                        for(auto* picker:dialog->findChildren<QFileDialog*>())if(picker->property("preference_file_action")=="import_legacy"){
                            handled=true;
                            if(file.isNull())picker->reject();
                            else {picker->selectFile(file.toString());QMetaObject::invokeMethod(picker,"accept",Qt::DirectConnection);}
                            return;
                        }
                        dialog->reject();
                    });
                    buttons.front()->click();
                    if(!handled)throw std::runtime_error("Import callback did not open the reviewed file dialog");
                    const auto message=dialog->property("preference_failure").toString();QString statusText,role;
                    for(auto* label:dialog->findChildren<QLabel*>())if(label->property("preference_error").toBool()){statusText=label->text();role=label->property("studio_message_role").toString();}
                    QJsonArray reportMessages;QString summaryText;
                    for(auto* text:dialog->findChildren<QTextEdit*>())if(text->property("preference_import_summary").toBool()){reportMessages=reportMessageProbe(text);summaryText=text->toPlainText();}
                    imports.append(QJsonObject{{"report_messages",reportMessages},{"summary_text",summaryText},{"file",file},{"draft",dialog->values()},{"report",dialog->property("preference_import_report").toJsonObject()},{"message",message},{"status_text",statusText},{"message_role",role},{"persisted",QJsonDocument::fromJson(bytes(preferencesStore_->filename())).object()}});
                }
                for(auto* object:dialog->findChildren<QWidget*>())if(object->property("preference_field").isValid()){
                    QJsonObject field{{"field",object->property("preference_field").toString()},{"type",object->metaObject()->className()}};
                    if(object->property("color").isValid())field["color"]=object->property("color").value<QColor>().name();
                    if(auto* spin=qobject_cast<QSpinBox*>(object))field["value"]=spin->value();
                    if(auto* edit=qobject_cast<QLineEdit*>(object)){
                        field["value"]=edit->text();
                        field["placeholder"]=edit->placeholderText();
                        field["tooltip"]=edit->toolTip();
                    }
                    if(auto* combo=qobject_cast<QComboBox*>(object)){
                        field["value"]=object->property("preference_choice_id").toBool()?combo->currentData().toString():combo->currentText();
                        if(object->property("preference_choice_id").toBool()){
                            QJsonArray choices;for(int i=0;i<combo->count();++i)choices.append(QJsonObject{{"id",combo->itemData(i).toString()},{"text",combo->itemText(i)}});field["choices"]=choices;
                        }
                    }
                    if(auto* check=qobject_cast<QCheckBox*>(object))field["value"]=check->isChecked();
                    discovered.append(field);
                }
                if(command["verify_directory_controls"].toBool()){
                    const auto isFolderButton=[](const QPushButton* button){
                        return button&&button->text().isEmpty()&&!button->icon().isNull()&&
                            !button->toolTip().isEmpty()&&!button->accessibleName().isEmpty();
                    };
                    const auto folders=dialog->findChildren<QPushButton*>("StudioFolderButton");
                    workflowCase("preferences.folder-icons",folders.size()==6&&std::all_of(folders.begin(),folders.end(),isFolderButton));
                    for(bool importing:{true,false}){
                        ExchangeDialog exchange(session_,importing,"obj",this);
                        workflowCase(importing?"exchange.import-folder-icon":"exchange.export-folder-icon",
                            isFolderButton(exchange.findChild<QPushButton*>("StudioFolderButton")));
                    }
                    QScrollArea scroll;
                    auto* content=new QWidget;
                    auto* layout=new QVBoxLayout(content);
                    auto* integer=new QSpinBox;
                    auto* decimal=new QDoubleSpinBox;
                    integer->setValue(20);
                    decimal->setValue(20.5);
                    layout->addWidget(integer);
                    layout->addWidget(decimal);
                    layout->addStretch();
                    content->setMinimumHeight(500);
                    scroll.setWidget(content);
                    scroll.setWidgetResizable(true);
                    scroll.resize(220,120);
                    scroll.setAttribute(Qt::WA_DontShowOnScreen);
                    scroll.show();
                    const auto scrollFrom=[&](QWidget* field){
                        field->setFocus();
                        scroll.verticalScrollBar()->setValue(0);
                        const QPoint point=field->rect().center();
                        QWheelEvent wheel(point,field->mapToGlobal(point),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                        QApplication::sendEvent(field,&wheel);
                        return scroll.verticalScrollBar()->value()>0;
                    };
                    workflowCase("preferences.wheel-scrolls-past-integer",scrollFrom(integer)&&integer->value()==20);
                    workflowCase("preferences.wheel-scrolls-past-decimal-editor",scrollFrom(decimal->findChild<QLineEdit*>())&&decimal->value()==20.5);
                    integer->stepUp();
                    decimal->stepDown();
                    workflowCase("preferences.spin-steps-remain-functional",integer->value()==21&&decimal->value()==19.5);
                }
                for(auto it=fields.begin();it!=fields.end();++it){
                    QList<QWidget*> found;for(auto* w:dialog->findChildren<QWidget*>())if(w->property("preference_field").toString()==it.key())found.append(w);
                    if(found.size()!=1){workflowCases_.append(QJsonObject{{"name","preference_field_unavailable:"+it.key()},{"status","not_run"}});dialog->reject();finishWorkflow();return;}
                    auto* w=found.front();if(auto* s=qobject_cast<QSpinBox*>(w))s->setValue(it.value().toInt());
                    else if(auto* e=qobject_cast<QLineEdit*>(w))e->setText(it.value().toString());
                    else if(auto* c=qobject_cast<QComboBox*>(w)){
                        if(w->property("preference_choice_id").toBool()){
                            const int i=c->findData(it.value().toString());if(i<0)throw std::runtime_error("Requested language choice unavailable");c->setCurrentIndex(i);
                        }else c->setCurrentText(it.value().toString());
                        if(it.key()=="theme")QMetaObject::invokeMethod(c,"activated",Qt::DirectConnection,Q_ARG(int,c->currentIndex()));
                    }
                    else if(auto* c=qobject_cast<QCheckBox*>(w))c->setChecked(it.value().toBool());
                    else throw std::runtime_error("Unreviewed preference field adapter");
                }
                dialog->grab().save(workflowOutput_+"/dialog-"+QString::number(preferencesRequest_.value("next").toInt())+".png");
                for(auto* tabs:dialog->findChildren<QTabWidget*>()){
                    const int current=tabs->currentIndex();
                    for(int i=0;i<tabs->count();++i){tabs->setCurrentIndex(i);dialog->grab().save(workflowOutput_+"/dialog-"+QString::number(preferencesRequest_.value("next").toInt())+"-tab-"+QString::number(i)+".png");}
                    tabs->setCurrentIndex(current);
                }
                if(command.value("block_next_request").toBool()){
                    QDir current(QFileInfo(session_.modelPath()).absolutePath());const auto name=current.dirName();bool ok=false;int n=name.mid(8).toInt(&ok);
                    if(!name.startsWith("request-")||!ok||!current.cdUp())throw std::runtime_error("No private request directory for failure injection");
                    QFile blocked(current.filePath("request-"+QString::number(n+1)));
                    if(!blocked.open(QIODevice::WriteOnly|QIODevice::NewOnly))throw std::runtime_error("Cannot inject request directory failure");
                    blocked.write("blocked");
                }
                if(command.contains("concurrent_write")){QFile f(preferencesStore_->filename());if(!f.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot inject concurrent preference write");f.write(QJsonDocument(command.value("concurrent_write").toObject()).toJson());}
                const auto action=command.value("action").toString("apply");QList<QPushButton*> buttons;
                for(auto* b:dialog->findChildren<QPushButton*>())if(b->property("preference_action")==action)buttons.append(b);
                if(buttons.size()!=1){workflowCases_.append(QJsonObject{{"name","preference_button_unavailable"},{"status","not_run"}});dialog->reject();finishWorkflow();return;}
                buttons[0]->click();const auto message=dialog->property("preference_failure").toString();QString statusText,role;
                for(auto* label:dialog->findChildren<QLabel*>())if(label->property("preference_error").toBool()){statusText=label->text();role=label->property("studio_message_role").toString();}
                preferencesRequest_["dialog_observation"]=QJsonObject{{"fields",discovered},{"message",message},{"status_text",statusText},{"message_role",role},{"draft",dialog->values()},{"imports",imports}};
                QFile evidence(workflowOutput_+"/dialog-"+QString::number(preferencesRequest_.value("next").toInt())+".json");
                if(evidence.open(QIODevice::WriteOnly))evidence.write(QJsonDocument(preferencesRequest_.value("dialog_observation").toObject()).toJson());
                if(action=="apply"&&!command.value("reject_after_apply").toBool()&&!preferencesPending_&&dialog->result()!=QDialog::Accepted){workflowCase("preferences_apply_completed",false,message);dialog->reject();}
                if(command.value("reject_after_apply").toBool()||action=="reset_graphics")dialog->reject();
            }catch(const std::exception& e){if(preferencesDialog_){preferencesDialog_->setBusy(false);preferencesDialog_->reject();}finishWorkflow(QString::fromUtf8(e.what()));}});
            actions[0]->trigger();
        }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}});
    }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}
}
