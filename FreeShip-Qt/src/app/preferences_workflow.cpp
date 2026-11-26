// SPDX-License-Identifier: GPL-2.0-or-later
// Hidden observation adapter for the production preferences action and dialog.
#include "main_window.h"
#include <QTextEdit>
#include <QSplitter>
#include "preferences_dialog.h"
#include "document/preferences.h"
#include "exchange_dialog.h"
#include "report_message_probe.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QElapsedTimer>
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
#include <QHeaderView>
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
        {"checkable", action.isCheckable()}, {"checked", action.isChecked()},
        {"has_icon", !action.icon().isNull()}};
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
            if(const auto* button=view->findChild<QToolButton*>("HullDisplayMode");button&&button->menu())
                viewport["display_modes"]=menuEvidence(*button->menu());
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
        if(operation=="begin"&&preferencesRequest_.value("observe_file_menu").toBool()){
            QJsonArray entries;for(auto* action:primaryMenu_->actions())if(!action->isSeparator())entries.append(QJsonObject{{"operation",action->property("file_operation").toString()},{"ordinary_open",action==open_},{"text",action->text()}});
            workflowArtifacts_["file_menu"]=entries;
        }
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
            item["document_count"]=documentWindows().size();
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
                    auto* dialog=frontStudioDialog();
                    if(!dialog||dialog->objectName()!="ShortcutGuide")return;
                    QApplication::setActiveWindow(dialog);
                    QKeyEvent press(QEvent::KeyPress,Qt::Key_L,Qt::ControlModifier|Qt::ShiftModifier,"l");
                    QApplication::sendEvent(dialog,&press);
                    if(dialog->isVisible())dialog->close();
                    if(uiLanguage()==before)return;
                    QTimer::singleShot(0,this,[this]{
                        auto* translated=frontStudioDialog();
                        auto* table=translated?translated->findChild<QTableWidget*>("ShortcutGuideTable"):nullptr;
                        workflowCase("language.guide-reopened-with-translated-table",table&&
                            table->horizontalHeader()->isHidden()&&table->item(0,0)&&
                            table->item(0,0)->text()==uiText("船模文件"));
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
                const auto settle=[this]{
                    QEventLoop wait;QTimer poll;QElapsedTimer elapsed;elapsed.start();
                    connect(&poll,&QTimer::timeout,&wait,[&]{if(!preferencesPending_||elapsed.elapsed()>15000)wait.quit();});
                    poll.start(10);wait.exec();
                    if(preferencesPending_)throw std::runtime_error("Automatic preference publication did not settle");
                };
                const auto chooseColor=[this](QPushButton* button,const QColor& color){
                    if(!color.isValid()||!button||!button->isEnabled())throw std::runtime_error("Preference color control is unavailable");
                    QTimer::singleShot(0,this,[this,color]{
                        if(auto* picker=qobject_cast<QColorDialog*>(frontStudioDialog())){
                            picker->setCurrentColor(color);picker->accept();
                        }else finishWorkflow("Missing preference color picker");
                    });
                    button->click();
                };
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
                    settle();
                    const auto message=dialog->property("preference_failure").toString();QString statusText,role;
                    for(auto* label:dialog->findChildren<QLabel*>())if(label->property("preference_error").toBool()){statusText=label->text();role=label->property("studio_message_role").toString();}
                    QJsonArray reportMessages;QString summaryText;
                    for(auto* text:dialog->findChildren<QTextEdit*>())if(text->property("preference_import_summary").toBool()){reportMessages=reportMessageProbe(text);summaryText=text->toPlainText();}
                    imports.append(QJsonObject{{"report_messages",reportMessages},{"summary_text",summaryText},{"file",file},{"draft",dialog->values()},{"report",dialog->property("preference_import_report").toJsonObject()},{"message",message},{"status_text",statusText},{"message_role",role},{"persisted",QJsonDocument::fromJson(bytes(preferencesStore_->filename())).object()}});
                }
                QJsonObject colorGroups;
                QString colorGroup;
                for(auto* object:dialog->findChildren<QWidget*>()){
                    if(object->property("preference_color_group").isValid())
                        colorGroup=object->property("preference_color_group").toString();
                    if(!object->property("preference_field").isValid())continue;
                    QJsonObject field{{"field",object->property("preference_field").toString()},{"type",object->metaObject()->className()}};
                    if(object->property("color").isValid()){
                        field["color"]=object->property("color").value<QColor>().name();
                        colorGroups[field["field"].toString()]=colorGroup;
                    }
                    if(auto* spin=qobject_cast<QSpinBox*>(object))field["value"]=spin->value();
                    if(auto* edit=qobject_cast<QLineEdit*>(object)){
                        field["value"]=edit->text();
                        field["placeholder"]=edit->placeholderText();
                        field["tooltip"]=edit->toolTip();
                    }
                    if(auto* combo=qobject_cast<QComboBox*>(object)){
                        field["value"]=object->property("preference_choice_id").toBool()?combo->currentData().toString():combo->currentText();
                        if(object->property("preference_choice_id").toBool()||field["field"]=="theme"){
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
                        auto* folder=exchange.findChild<QPushButton*>("StudioFolderButton");
                        workflowCase(importing?"exchange.import-folder-icon":"exchange.export-no-folder-field",
                            importing?isFolderButton(folder):folder==nullptr);
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
                    else if(auto* e=qobject_cast<QLineEdit*>(w)){
                        e->setText(it.value().toString());
                        QMetaObject::invokeMethod(e,"editingFinished",Qt::DirectConnection);
                    }
                    else if(auto* c=qobject_cast<QComboBox*>(w)){
                        if(w->property("preference_choice_id").toBool()){
                            const int i=c->findData(it.value().toString());if(i<0)throw std::runtime_error("Requested language choice unavailable");c->setCurrentIndex(i);
                        }else c->setCurrentText(it.value().toString());
                        if(it.key()=="theme")QMetaObject::invokeMethod(c,"activated",Qt::DirectConnection,Q_ARG(int,c->currentIndex()));
                    }
                    else if(auto* c=qobject_cast<QCheckBox*>(w))c->setChecked(it.value().toBool());
                    else if(auto* button=qobject_cast<QPushButton*>(w);button&&it.key()=="underwater_display.color"){
                        chooseColor(button,QColor(it.value().toString()));
                    }
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
                const auto action=command.value("action").toString("apply");
                bool confirmation=false;
                for(auto* button:dialog->findChildren<QPushButton*>()){
                    const auto operation=button->property("preference_action").toString();
                    confirmation|=operation=="apply"||operation=="cancel";
                    if(action=="reset_graphics"&&operation==action)button->click();
                }
                settle();
                if(command["verify_control_edge_colors"].toBool()){
                    auto* tabs=dialog->findChild<QTabWidget*>();
                    const auto sheets=dialog->findChildren<QScrollArea*>();
                    QScrollArea* graphics=nullptr;
                    QList<QPushButton*> buttons;
                    for(auto* sheet:sheets){
                        for(auto* button:sheet->findChildren<QPushButton*>())
                            if(button->property("color").isValid())buttons.append(button);
                        if(!buttons.isEmpty()){graphics=sheet;break;}
                    }
                    if(!graphics)throw std::runtime_error("Graphics swatches are unavailable");
                    const int originalTab=tabs->currentIndex();
                    const auto originalSize=dialog->size();
                    tabs->setCurrentWidget(graphics);
                    const auto gridColumns=[&](int width){
                        dialog->resize(width,originalSize.height());
                        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                        QSet<int> columns;
                        for(auto* button:buttons)
                            columns.insert(button->parentWidget()->mapTo(graphics->widget(),QPoint{}).x());
                        return columns.size();
                    };
                    const auto narrowColumns=gridColumns(580),wideColumns=gridColumns(960);
                    workflowCase("preferences.swatches-wrap-in-columns",narrowColumns>=2&&wideColumns>narrowColumns,
                        QStringLiteral("Columns at narrow and wide sizes: %1, %2").arg(narrowColumns).arg(wideColumns));
                    workflowCase("preferences.swatch-buttons-display-only-color",std::all_of(buttons.cbegin(),buttons.cend(),[](const auto* button){
                        return button->text().isEmpty()&&!button->icon().isNull()&&!button->accessibleName().isEmpty();
                    }));
                    const bool swatchesFollowLabels=std::all_of(buttons.cbegin(),buttons.cend(),[](const QPushButton* button){
                        const auto* label=button->parentWidget()->findChild<QLabel*>();
                        if(!label||label->buddy()!=button)return false;
                        const int gap=button->x()-label->geometry().right()-1;
                        return gap>=0&&gap<=8&&label->width()==label->sizeHint().width();
                    });
                    workflowCase("preferences.swatches-follow-their-own-labels",swatchesFollowLabels);
                    const auto frames=graphics->findChildren<QFrame*>("PreferenceColorGroup");
                    QSet<QString> frameStyles,groupNames;
                    for(const auto* frame:frames){
                        frameStyles.insert(frame->styleSheet());
                        for(const auto* label:frame->findChildren<QLabel*>())
                            if(label->property("preference_color_group").isValid())
                                groupNames.insert(label->property("preference_color_group").toString());
                    }
                    const bool swatchesFramed=std::all_of(buttons.cbegin(),buttons.cend(),[&](const auto* button){
                        return std::count_if(frames.cbegin(),frames.cend(),[&](const auto* frame){return frame->isAncestorOf(button);})==1;
                    });
                    workflowCase("preferences.color-groups-have-distinct-frames",
                                 frames.size()==5&&frameStyles.size()==5&&groupNames.size()==5&&swatchesFramed);
                    graphics->widget()->grab().save(workflowOutput_+"/graphics-color-grid.png");
                    dialog->resize(originalSize);
                    tabs->setCurrentIndex(originalTab);
                    HullView view;
                    view.resize(500,300);
                    view.setAnimationsEnabled(false);
                    view.setStudioAppearance(true);
                    view.setProjection(HullProjection::Profile);
                    auto model=std::make_shared<fs::ModelSnapshot>();
                    model->documentId="control-edge-color";
                    model->bothSides=false;
                    model->layers.append(fs::Layer{1,"Shell"});
                    for(int z=0;z<2;++z)for(int x=0;x<3;++x)
                        model->points.append(fs::ControlPoint{fs::EntityId(1+x+3*z),{double(x),0,double(z)}});
                    model->faces={{1,1,{1,2,5,4}},{2,1,{2,3,6,5}}};
                    model->edges={{1,1,4},{2,2,5,true},{3,3,6},{4,1,2},{5,2,3},{6,4,5},{7,5,6}};
                    view.setSnapshot(model);view.selectDisplayMode("wireframe");view.setControlNetVisible(true);
                    view.fit();view.finishPresentation();
                    const QColor unified("#8426e5");
                    view.setGraphicPreferences({{"edgecolor",int(unified.rgb()&0xffffff)},{"creaseedgecolor",0xff2727}});
                    const auto distanceToColor=[&](const QImage& image,double x,QColor expected){
                        const auto point=view.project({x,0,.5}).toPoint();
                        int closest=3*255;
                        for(int dy=-14;dy<=14;++dy)for(int dx=-2;dx<=2;++dx){
                            const auto color=image.pixelColor(point+QPoint(dx,dy));
                            closest=std::min(closest,std::abs(color.red()-expected.red())+
                                std::abs(color.green()-expected.green())+std::abs(color.blue()-expected.blue()));
                        }
                        return closest;
                    };
                    const auto ordinary=view.image();
                    ordinary.save(workflowOutput_+"/unified-control-edges.png");
                    const int ordinaryDistance=distanceToColor(ordinary,0,unified);
                    const int creaseDistance=distanceToColor(ordinary,1,unified);
                    workflowCase("preferences.control-edges-share-color",
                        ordinaryDistance<140&&creaseDistance<140,
                        QStringLiteral("RGB distance to swatch at each edge after antialiasing: %1, %2")
                            .arg(ordinaryDistance).arg(creaseDistance));
                    view.selectDisplayMode("inspect_lines");
                    const auto inspection=view.image();
                    workflowCase("preferences.line-inspection-keeps-classification-colors",
                        distanceToColor(inspection,0,QColor("#14699e"))<85&&
                        distanceToColor(inspection,1,QColor("#ac4a00"))<85);
                }
                if(command["verify_graphics_history"].toBool()){
                    const auto field=[dialog](const char* key){
                        for(auto* widget:dialog->findChildren<QWidget*>())
                            if(widget->property("preference_field")==key)return widget;
                        return static_cast<QWidget*>(nullptr);
                    };
                    auto* tab=dialog->findChild<QTabWidget*>();
                    auto* swatch=field("graphic.viewportcolor");
                    for(int i=0;i<tab->count();++i)if(tab->widget(i)->isAncestorOf(swatch))tab->setCurrentIndex(i);
                    QApplication::setActiveWindow(dialog);
                    const auto pressHistory=[&](bool redo){
                        auto* button=field("graphic.viewportcolor");
                        button->setFocus();
                        const auto modifiers=redo?(Qt::ControlModifier|Qt::ShiftModifier):Qt::KeyboardModifiers(Qt::ControlModifier);
                        QKeyEvent press(QEvent::KeyPress,Qt::Key_Z,modifiers,QStringLiteral("z"));
                        QApplication::sendEvent(button,&press);
                        settle();
                    };
                    const auto matches=[&](const QJsonObject& values){
                        const auto file=QJsonDocument::fromJson(bytes(preferencesStore_->filename())).object();
                        const auto graphic=effectivePreferences(values)["graphic"].toObject();
                        const QColor color=QColor::fromRgb(fs::document::resolvedPreferenceColors(graphic)["viewportcolor"].toInt());
                        return preferences_==values&&dialog->values()==values&&file["values"]==values&&
                               field("graphic.viewportcolor")->property("color").value<QColor>()==color;
                    };
                    const auto original=dialog->values();
                    const bool modified=session_.modified(),canUndo=session_.canUndo(),canRedo=session_.canRedo();
                    chooseColor(qobject_cast<QPushButton*>(swatch),QColor("#730CB7"));settle();
                    const auto edited=preferences_;
                    workflowCase("preferences.color-applied-before-undo",edited!=original&&matches(edited));
                    pressHistory(false);
                    workflowCase("preferences.color-undo-restores-settings-and-swatch",matches(original));
                    pressHistory(true);
                    workflowCase("preferences.color-redo-restores-settings-and-swatch",matches(edited));
                    auto* size=qobject_cast<QSpinBox*>(field("graphic.pointsize"));
                    const int pointSize=size->value()+1;size->setValue(pointSize);settle();
                    pressHistory(false);
                    workflowCase("preferences.color-undo-keeps-later-point-size",
                        preferences_.value("graphic").toObject()["pointsize"]==pointSize&&
                        preferences_.value("graphic").toObject().value("viewportcolor")==
                            original.value("graphic").toObject().value("viewportcolor"));
                    pressHistory(true);
                    const auto beforeReset=preferences_;
                    for(auto* button:dialog->findChildren<QPushButton*>())
                        if(button->property("preference_action")=="reset_graphics"){button->click();break;}
                    settle();
                    pressHistory(false);
                    workflowCase("preferences.graphics-reset-undo",matches(beforeReset));
                    auto* theme=qobject_cast<QComboBox*>(field("theme"));
                    if(theme->count()>1){
                        theme->setCurrentIndex(1);
                        QMetaObject::invokeMethod(theme,"activated",Qt::DirectConnection,Q_ARG(int,1));
                        settle();
                        const auto preset=preferences_;
                        pressHistory(false);
                        workflowCase("preferences.palette-preset-undo",preset.contains("theme")&&matches(beforeReset));
                        pressHistory(true);
                        workflowCase("preferences.palette-preset-redo",matches(preset));
                    }
                    workflowCase("preferences.graphics-history-preserves-document-history",
                        session_.modified()==modified&&session_.canUndo()==canUndo&&session_.canRedo()==canRedo);
                }
                workflowCase("preferences-autosave-keeps-window-open",dialog->isVisible()&&!confirmation);
                const auto message=dialog->property("preference_failure").toString();QString statusText,role;
                for(auto* label:dialog->findChildren<QLabel*>())if(label->property("preference_error").toBool()){statusText=label->text();role=label->property("studio_message_role").toString();}
                preferencesRequest_["dialog_observation"]=QJsonObject{{"fields",discovered},{"color_groups",colorGroups},{"message",message},{"status_text",statusText},{"message_role",role},{"draft",dialog->values()},{"imports",imports}};
                QFile evidence(workflowOutput_+"/dialog-"+QString::number(preferencesRequest_.value("next").toInt())+".json");
                if(evidence.open(QIODevice::WriteOnly))evidence.write(QJsonDocument(preferencesRequest_.value("dialog_observation").toObject()).toJson());
                if(action=="apply"&&!command.value("reject_after_apply").toBool()&&!message.isEmpty())
                    workflowCase("preferences_apply_completed",false,message);
                dialog->close();
            }catch(const std::exception& e){if(preferencesDialog_){preferencesDialog_->setBusy(false);preferencesDialog_->reject();}finishWorkflow(QString::fromUtf8(e.what()));}});
            actions[0]->trigger();
        }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}});
    }catch(const std::exception& e){finishWorkflow(QString::fromUtf8(e.what()));}
}

