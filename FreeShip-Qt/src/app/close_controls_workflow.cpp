// Production-widget audit. Only explicitly requested private startup probes run it.
#include "main_window.h"
#include "about_dialog.h"
#include "shortcuts_dialog.h"
#include "tutorial_dialog.h"
#include "exchange_dialog.h"
#include "performance_dialog.h"
#include "hydro_dialog.h"
#include "plates_dialog.h"
#include "section_dialog.h"
#include "project_dialog.h"
#include "preferences_dialog.h"
#include "recovery_dialog.h"
#include "light_dialog.h"
#include "window_chrome_probe.h"
#include "ui_copy_probe.h"
#include "message_lifecycle_probe.h"
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QEnterEvent>
#include <QMessageBox>
#include <QMenu>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QToolTip>
#include <stdexcept>

void MainWindow::auditCloseControls(){
    if(!startupProbe_||!session_.ready()||session_.busy()){finishWorkflow("Private close audit needs an idle model");return;}
    qApp->setQuitOnLastWindowClosed(false);
    MessageLifecycleProbe messageLifecycle;
    const auto original=session_.model();const bool modified=session_.modified();
    QJsonArray rows;QDir().mkpath(workflowOutput_+"/views");
    auto observe=[&](QDialog* dialog,const QString& scope,bool picture=false){
        dialog->setProperty("studio_private_window_probe",true);
        // QMessageBox::setVisible resets WA_DontShowOnScreen in Qt itself.
        // Explicitly private, transparent test message boxes still use owned HWNDs.
        if(qobject_cast<QMessageBox*>(dialog))dialog->setWindowOpacity(0);
        dialog->setAttribute(Qt::WA_DontShowOnScreen);dialog->ensurePolished();dialog->show();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        const auto chrome=windowChromeProbe(dialog,workflowOutput_+"/icons/"+QString::number(rows.size()));
        // The native chrome probe deliberately leaves its owned window hidden.
        // Re-show on the private QA desktop before observing actual visibility.
        dialog->show();QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        if(scope.startsWith("calculation-")||scope.startsWith("hydro-")){
            const bool hydro=scope.startsWith("hydro-");
            QMap<QString,QWidget*> fields;
            QMap<QString,QLabel*> markers;
            for(auto* widget:dialog->findChildren<QWidget*>()){
                const auto key=widget->property(hydro?"field":"performance_field").toString();
                if(!key.isEmpty())fields[key]=widget;
            }
            for(auto* label:dialog->findChildren<QLabel*>()){
                const auto key=label->property("calculation_input_status").toString();
                if(!key.isEmpty())markers[key]=label;
            }
            QPushButton* run=nullptr;
            for(auto* button:dialog->findChildren<QPushButton*>())
                if(button->property(hydro?"hydro_action":"performance_action")=="calculate")run=button;
            bool ready=true,each=!fields.isEmpty()&&fields.keys()==markers.keys();
            for(auto it=markers.cbegin();it!=markers.cend();++it){
                each&=!it.value()->pixmap().isNull()&&!it.value()->accessibleName().isEmpty();
                if(hydro||(fields.contains(it.key())&&fields.value(it.key())->property("calculation_declared_visible").toBool()))
                    ready&=it.value()->property("performance_field_valid").toBool();
            }
            workflowCase("parameter-status-each-"+scope,each);
            workflowCase("calculate-follows-parameter-status-"+scope,run&&run->isEnabled()==ready);
            bool immediate=false;
            QString observedTip;
            for(auto* marker:markers){
                if(!marker->isVisible()||!marker->isEnabled()||marker->toolTip().isEmpty())continue;
                const auto point=marker->rect().center();
                QEnterEvent enter(point,marker->mapTo(dialog,point),marker->mapToGlobal(point));
                QApplication::sendEvent(marker,&enter);
                immediate=QToolTip::isVisible()&&QToolTip::text()==marker->toolTip();
                observedTip=QString("%1 popup=%2 match=%3").arg(marker->property("calculation_input_status").toString())
                    .arg(QToolTip::isVisible()).arg(QToolTip::text()==marker->toolTip());
                QEvent leave(QEvent::Leave);
                QApplication::sendEvent(marker,&leave);
                break;
            }
            workflowCase("parameter-status-tooltip-on-entry-"+scope,immediate,observedTip);
            if(scope=="hydro-table"||scope=="hydro-cross"||scope=="hydro-single"||
               scope=="calculation-addmass"||scope=="calculation-wind"||scope=="calculation-propeller5"){
                const QString key=scope=="hydro-table"?"start":scope=="hydro-cross"?"angles":
                    scope=="hydro-single"?"heel":scope=="calculation-addmass"?"stations":
                    scope=="calculation-wind"?"Edit8":"Edit2";
                auto* field=fields.value(key);
                auto* marker=markers.value(key);
                const bool originallyEnabled=run&&run->isEnabled();
                bool blocked=false;
                if(auto* number=qobject_cast<QDoubleSpinBox*>(field)){
                    if(scope=="hydro-single"||scope=="calculation-propeller5"){
                        auto* edit=number->findChild<QLineEdit*>();
                        const auto saved=edit->text();
                        edit->setText({});
                        QMetaObject::invokeMethod(edit,"textEdited",Qt::DirectConnection,Q_ARG(QString,QString{}));
                        blocked=marker&&!marker->property("performance_field_valid").toBool()&&run&&!run->isEnabled();
                        edit->setText(saved);
                        QMetaObject::invokeMethod(edit,"textEdited",Qt::DirectConnection,Q_ARG(QString,saved));
                    }else{
                        const double saved=number->value();
                        number->setValue(scope=="hydro-table"?number->maximum():scope=="calculation-addmass"?20:
                                         scope=="calculation-wind"?number->maximum():0);
                        blocked=marker&&!marker->property("performance_field_valid").toBool()&&run&&!run->isEnabled();
                        number->setValue(saved);
                    }
                }else if(auto* edit=qobject_cast<QLineEdit*>(field)){
                    const auto saved=edit->text();edit->setText("10, 0");
                    blocked=marker&&!marker->property("performance_field_valid").toBool()&&run&&!run->isEnabled();
                    edit->setText(saved);
                }
                workflowCase("invalid-edit-blocks-and-restores-"+scope,blocked&&run&&run->isEnabled()==originallyEnabled);
            }
        }
        if(auto* hydro=dynamic_cast<HydroDialog*>(dialog);hydro&&hydro->findChild<QWidget*>("AuxiliaryReportBar")){
            const auto oldSize=hydro->size();hydro->selectAuxiliary(0);
            auto* notice=dynamic_cast<StudioMessage*>(hydro->findChild<QLabel*>("AuxiliaryReportNotice"));
            auto* bar=hydro->findChild<QWidget*>("AuxiliaryReportBar");
            const auto oldText=notice->text();
            // Known sample copy tests layout only, never numerical results.
            notice->setText(QString::fromUtf8("船体最低漏点 Z = 14.97 m。已跳过水线 Z = 15.19 m。"));
            for(int width:{1536,1100}){
                hydro->resize(width,900);QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                for(auto* layout:hydro->findChildren<QLayout*>())layout->activate();
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                workflowCase("auxiliary-warning-contained-"+QString::number(width),bar->rect().contains(QRect(notice->mapTo(bar,QPoint{}),notice->size())));
                // User's 2026-11-14 placement: beside the top-level tabs,
                // rather than beside the lower report selector.
                if(width==1536)workflowCase("auxiliary-warning-top-tab-row-wide",bar->property("studio_warning_layout")=="tab-row"&&
                    hydro->rect().contains(QRect(bar->mapTo(hydro,QPoint{}),bar->size())));
                hydro->grab().save(workflowOutput_+"/views/auxiliary-layout-"+QString::number(width)+".png");
            }
            // Even a wide window has a finite slot beside the tabs. Long
            // translations belong in the page so the final line stays visible.
            hydro->resize(1536,900);
            notice->setText(QStringLiteral("The lowest hull opening is above the requested level. ").repeated(4)+"The remaining waterlines were calculated.");
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            for(auto* layout:hydro->findChildren<QLayout*>())layout->activate();
            QEventLoop settle;
            QTimer::singleShot(180,&settle,&QEventLoop::quit);
            settle.exec(QEventLoop::ExcludeUserInputEvents);
            workflowCase("auxiliary-long-warning-wraps-below-tabs",bar->property("studio_warning_layout")=="wrapped"&&notice->isVisible()&&
                notice->height()>=notice->heightForWidth(notice->width())&&
                hydro->rect().contains(QRect(notice->mapTo(hydro,QPoint{}),notice->size())),
                QString("%1: %2x%3, required %4, visible %5").arg(bar->property("studio_warning_layout").toString())
                    .arg(notice->width()).arg(notice->height()).arg(notice->heightForWidth(notice->width())).arg(notice->isVisible()));
            notice->setText(oldText);hydro->resize(oldSize);
        }
        QJsonArray dropdowns,reportExports;
        QWidget* pages=nullptr;for(auto* item:dialog->findChildren<QWidget*>())if(item->property("calculation_pages").toBool()){pages=item;break;}
        for(auto* button:dialog->findChildren<QPushButton*>())if(button->property("performance_export").isValid()){
            const auto at=button->mapTo(dialog,QPoint());
            reportExports.append(QJsonObject{{"format",button->property("performance_export").toString()},
                {"text",button->text()},{"icon",button->icon().name()},{"tooltip",button->toolTip()},{"accessible",button->accessibleName()},
                {"has_menu",bool(button->menu())},{"visible",button->isVisible()},
                {"x",at.x()},{"y",at.y()},{"width",button->width()},
                {"before_pages",pages&&at.y()+button->height()<=pages->mapTo(dialog,QPoint()).y()}});
        }
        for(auto* menu:dialog->findChildren<QMenu*>()){
            bool exports=false;for(auto* action:menu->actions())exports=exports||action->property("performance_export").isValid();
            if(!exports)continue;
            menu->setAttribute(Qt::WA_DontShowOnScreen);menu->popup({40,40});menu->setActiveAction(nullptr);
            const auto image=menu->grab().toImage();const auto ratio=image.devicePixelRatio();
            const auto picture=workflowOutput_+"/views/exports-"+QString::number(rows.size())+".png";image.save(picture);
            for(auto* action:menu->actions())if(action->property("performance_export").isValid()){
                const auto rect=menu->actionGeometry(action);
                const auto gutter=QRectF(QPointF(rect.right()-25,rect.top())*ratio,QSizeF(23,rect.height())*ratio).toAlignedRect().intersected(image.rect());
                int ink=0;for(int y=gutter.top();y<=gutter.bottom();++y)for(int x=gutter.left();x<=gutter.right();++x){
                    const auto pixel=image.pixelColor(x,y);if(pixel.lightness()<150&&pixel.green()>pixel.red()*1.15)++ink;
                }
                dropdowns.append(QJsonObject{{"text",action->text()},{"format",action->property("performance_export").toString()},
                    {"opens_dialog",studioMenuOpensDialog(action)},{"explicit_dialog",action->property("studio_opens_dialog").toBool()},
                    {"marker_ink_pixels",ink},{"image",picture}});
            }
            menu->hide();
        }
        const bool titleClose=dialog->windowFlags().testFlag(Qt::WindowCloseButtonHint);
        QJsonArray controls;int duplicates=0;
        for(auto* button:dialog->findChildren<QAbstractButton*>()){
            if(button->window()!=dialog)continue;
            auto text=button->text();text.remove('&');text=text.trimmed();
            auto* box=qobject_cast<QDialogButtonBox*>(button->parentWidget());
            const bool standardClose=box&&box->standardButton(button)==QDialogButtonBox::Close;
            const bool close=text==uiText("关闭")||text.compare("Close",Qt::CaseInsensitive)==0||standardClose;
            if(close)++duplicates;
            controls.append(QJsonObject{{"text",text},{"visible",button->isVisibleTo(dialog)},{"close",close},{"standard_close",standardClose}});
        }
        const auto copy=uiCopyProbe(dialog);QJsonArray reportMessages;
        for(auto* document:dialog->findChildren<QTextEdit*>())if(document->window()==dialog)for(auto message:reportMessageProbe(document)){auto entry=message.toObject();entry["object"]=document->objectName();reportMessages.append(entry);}
        QJsonArray copyStates;
        if(scope=="project")for(auto* toggle:dialog->findChildren<QCheckBox*>())if(toggle->property("modelField")=="align_to_baseline"){
            const bool checked=toggle->isChecked();
            QList<QPair<QTabWidget*,int>> selected;
            for(auto* tabs:dialog->findChildren<QTabWidget*>())for(int i=0;i<tabs->count();++i)
                if(tabs->widget(i)->isAncestorOf(toggle)){selected.append({tabs,tabs->currentIndex()});tabs->setCurrentIndex(i);break;}
            for(bool enabled:{false,true,false}){
                toggle->setChecked(enabled);
                copyStates.append(QJsonObject{{"action","align_to_baseline"},{"checked",enabled},{"copy",uiCopyProbe(dialog)}});
            }
            toggle->setChecked(checked);for(const auto& tab:selected)tab.first->setCurrentIndex(tab.second);
        }
        QJsonArray expandedCopy;
        for(auto* button:dialog->findChildren<QAbstractButton*>())
            if(button->property("exchange_action")=="format_help"&&button->isVisibleTo(dialog)){
                button->click();expandedCopy=uiCopyProbe(dialog);button->click();
            }
        QJsonObject messageIcon;
        if(auto* message=qobject_cast<QMessageBox*>(dialog)){
            const auto icon=message->iconPixmap().toImage();int blue=0,ink=0;
            for(int y=0;y<icon.height();++y)for(int x=0;x<icon.width();++x){const auto c=icon.pixelColor(x,y);if(c.alpha()>30){++ink;if(c.blue()>c.green()+15&&c.blue()>c.red()+30)++blue;}}
            messageIcon=QJsonObject{{"kind",message->property("studio_message_kind").toInt()},{"blue_pixels",blue},{"ink_pixels",ink}};
        }
        if(scope=="project"||scope=="hydro-single"||scope=="plates")picture=true;
        QJsonArray tableSurfaces;
        for(auto* table:dialog->findChildren<QTableView*>()){
            table->ensurePolished();
            tableSurfaces.append(QJsonObject{{"type",table->metaObject()->className()},{"object",table->objectName()},
                {"visible",table->isVisibleTo(dialog)},{"alternating",table->alternatingRowColors()},
                {"base",table->palette().color(QPalette::Base).name()},
                {"alternate",table->palette().color(QPalette::AlternateBase).name()},
                {"policy",table->property("studio_table_surface").toString()}});
        }
        QJsonArray typography;
        for(auto* control:dialog->findChildren<QWidget*>())if(control->window()==dialog&&
            (qobject_cast<QLineEdit*>(control)||qobject_cast<QComboBox*>(control)||qobject_cast<QAbstractSpinBox*>(control)||qobject_cast<QPushButton*>(control))){
            typography.append(QJsonObject{{"type",control->metaObject()->className()},{"visible",control->isVisibleTo(dialog)},
                {"point_size",control->font().pointSizeF()},{"family",control->font().family()},
                {"height",control->height()},{"text_height",control->fontMetrics().height()}});
        }
        const auto path=workflowOutput_+"/views/"+QString::number(rows.size())+".png";
        if(picture)dialog->grab().save(path);
        QJsonArray messageStates;
        const auto messages=[&](const QString& state){
            QJsonArray labels;for(auto* label:dialog->findChildren<QLabel*>()){
                const auto role=label->property("studio_message_role").toString();if(role.isEmpty()||label->window()!=dialog)continue;
                labels.append(QJsonObject{{"object",label->objectName()},{"role",role},{"text",label->text()},
                    {"visible",label->isVisibleTo(dialog)},{"busy",label->property("studio_message_busy").toBool()},
                    {"foreground",label->property("studio_message_foreground").toString()},{"background",label->property("studio_message_background").toString()},
                    {"width",label->width()},{"height",label->height()}});
            }messageStates.append(QJsonObject{{"state",state},{"labels",labels}});
        };
        messages("initial");
        // Public production error/status handlers; no numerical request or file write.
        const QString failure="Private message-style probe";bool failed=false;
        if(auto* d=dynamic_cast<PerformanceDialog*>(dialog)){d->showFailure(failure);failed=true;}
        else if(auto* d=dynamic_cast<ExchangeDialog*>(dialog)){d->showFailure(failure);failed=true;}
        else if(auto* d=dynamic_cast<PlatesDialog*>(dialog)){d->showFailure(failure);failed=true;}
        else if(auto* d=dynamic_cast<PreferencesDialog*>(dialog)){d->showFailure(failure);failed=true;}
        else if(auto* d=dynamic_cast<SectionDialog*>(dialog)){d->showError(failure);failed=true;}
        if(failed){messages("failure");if(scope=="plates")dialog->grab().save(workflowOutput_+"/views/plates-warning.png");}
        // Calculation success feedback is exercised by the actual calculation
        // workflow; the removed manual parameter-save callback is not a state.
        if(auto* d=dynamic_cast<PreferencesDialog*>(dialog)){d->setBusy(true);messages("working-after-failure");d->setBusy(false);}
        const bool closed=dialog->close()&&!dialog->isVisible();
        rows.append(QJsonObject{{"scope",scope},{"title",dialog->windowTitle()},{"title_close",titleClose},
            {"title_maximize",dialog->windowFlags().testFlag(Qt::WindowMaximizeButtonHint)},
            {"icon_key",dialog->property("studio_window_icon").toString()},{"icon_name",dialog->windowIcon().name()},{"chrome",chrome},
            {"table_surfaces",tableSurfaces},{"close_controls",duplicates},{"buttons",controls},{"copy",copy},{"typography",typography},{"expanded_copy",expandedCopy},{"copy_states",copyStates},{"dropdowns",dropdowns},{"report_exports",reportExports},
            {"closed_by_window_close",closed},{"image",picture?path:QString{}},{"message_icon",messageIcon},{"message_states",messageStates},{"report_messages",reportMessages}});
        workflowCase("close-controls-"+scope,!titleClose||duplicates==0);
        workflowCase("title-close-"+scope,titleClose&&closed);
        workflowCase("title-double-click-"+scope,chrome["available"].toBool()&&chrome["first_double_click_toggled"].toBool()&&chrome["second_double_click_restored"].toBool());
        workflowCase("title-native-icon-"+scope,chrome["native_icon"].toBool()&&chrome["icon_pixels"].toInt()>0);
    };
    auto modal=[&](const QString& scope,const std::function<void()>& open,bool picture=false){
        bool handled=false;QTimer timer;timer.setSingleShot(true);
        connect(&timer,&QTimer::timeout,this,[&]{
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!dialog){finishWorkflow("Audit dialog did not open: "+scope);return;}
            if(scope=="about")for(const auto& entry:QList<QPair<QString,QString>>{{"AboutVersionToggle","about-version"},{"AboutCopyrightToggle","about-license"}}){
                QTimer childTimer;childTimer.setSingleShot(true);bool childObserved=false;
                connect(&childTimer,&QTimer::timeout,this,[&]{
                    auto* child=qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    if(child&&child!=dialog){childObserved=true;observe(child,entry.second);}
                });
                auto* button=dialog->findChild<QPushButton*>(entry.first);
                if(button){childTimer.start(0);button->click();}
                workflowCase("nested-"+entry.second,childObserved);
            }
            handled=true;observe(dialog,scope,picture);
        });
        timer.start(0);open();
        if(!handled&&!workflowFinished_)finishWorkflow("Audit dialog unavailable: "+scope);
    };
    // Fetch actual backend schemas, suppressing only the probe's normal callbacks.
    // No calculations, imports, exports or saved settings are submitted here.
    auto defaults=[&](const QString& method){
        const auto completed=session_.onCompleted;const auto failed=session_.onError;const auto performance=session_.onPerformance;
        QEventLoop loop;QTimer deadline;deadline.setSingleShot(true);QJsonObject schema;QString error;
        session_.onCompleted=[](const QString&){};
        session_.onError=[&](const QString& message){error=message;loop.quit();};
        session_.onPerformance=[&](const QJsonObject& data,bool isDefaults){if(isDefaults)schema=data;loop.quit();};
        connect(&deadline,&QTimer::timeout,&loop,[&]{error="Calculation defaults timed out";loop.quit();});
        deadline.start(15000);const bool sent=session_.performanceDefaults({{"calculator",method}});
        if(sent)loop.exec();else error="Defaults request rejected";
        session_.onCompleted=completed;session_.onError=failed;session_.onPerformance=performance;
        if(!error.isEmpty()||schema.isEmpty())throw std::runtime_error((method+": "+error).toStdString());
        return schema;
    };
    try{
        setAttribute(Qt::WA_DontShowOnScreen);ensurePolished();show();QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        workflowArtifacts_["main_window_chrome"]=windowChromeProbe(this,workflowOutput_+"/icons/main");
        for(auto* action:exchangeActions_){
            const bool importing=action->property("importing").toBool();const auto format=action->property("operation").toString().mid(7);
            ExchangeDialog dialog(session_,importing,format,this);observe(&dialog,(importing?"import-":"export-")+format,format=="fef"||format=="part"||(!importing&&format=="obj"));
        }
        QStringList methods;for(auto* action:performanceActions_){const auto op=action->property("operation").toString();if(op.startsWith("performance_"))methods.append(op.mid(12));}
        methods.removeDuplicates();
        for(const auto& method:methods){PerformanceDialog dialog(session_,defaults(method),true,this);observe(&dialog,"calculation-"+method,method=="delft");}
        for(const auto& mode:QStringList{"single","table","cross"}){HydroDialog dialog(session_,mode,{},true,this);observe(&dialog,"hydro-"+mode);}
        {PlatesDialog dialog(session_,true,this);observe(&dialog,"plates");}
        {ProjectDialog dialog(*original,this);observe(&dialog,"project");}
        {SectionDialog dialog(this);dialog.setSnapshot(original);observe(&dialog,"sections");}
        {PreferencesDialog dialog({},this);observe(&dialog,"preferences");}
        {RecoveryDialog dialog(workflowOutput_+"/empty-recovery",this);observe(&dialog,"recovery");}
        for(auto* action:findChildren<QAction*>())if(action->property("operation").toString()=="check_updates"){
            action->trigger();
            if(auto* dialog=findChild<QDialog*>("SoftwareUpdateDialog")){
                observe(dialog,"software-update");dialog->close();
            }
            break;
        }
        {LightDialog dialog(this);observe(&dialog,"lighting",true);}
        observe(layerDialog_,"layers",true);observe(linesDialog_,"lines-plan");
        const bool oldWorkflow=workflow_;workflow_=true;
        modal("point-groups",[this]{pointGroupsDialog();});
        modal("modeling-auto-group-layers",[this]{modelingDialog("auto_group_layers");},true);
        modal("model-check",[this]{modelCheckDialog();},true);
        modal("about",[this]{showFreeShipAbout(this);});
        modal("shortcuts",[this]{showShortcutGuide(this);});
        modal("tutorial",[this]{showTutorialGuide(this);},true);
        {QMessageBox dialog(QMessageBox::Question,uiText("确认"),uiText("是否继续？"),QMessageBox::Yes|QMessageBox::No,this);observe(&dialog,"confirmation",true);}
        for(const auto& family:QList<QPair<QMessageBox::Icon,QString>>{{QMessageBox::Information,"information"},{QMessageBox::Warning,"warning"},{QMessageBox::Critical,"error"}}){
            QMessageBox dialog(family.first,uiText("提示"),uiText("操作结果"),QMessageBox::Ok,this);observe(&dialog,"message-"+family.second,true);
        }
        workflow_=oldWorkflow;
        {
            QDialog negative(this);negative.setAttribute(Qt::WA_DontShowOnScreen);negative.ensurePolished();
            negative.setProperty("studio_caption_double_click_disabled",true);
            negative.setWindowFlag(Qt::WindowMaximizeButtonHint,false);negative.show();QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            const auto result=windowChromeProbe(&negative,workflowOutput_+"/icons/no-maximize");negative.close();
            workflowArtifacts_["chrome_no_maximize_control"]=result;
            workflowCase("native-probe-detects-disabled-title-gesture",!result["first_double_click_toggled"].toBool());
        }
        {
            QDialog negative(this);negative.setAttribute(Qt::WA_DontShowOnScreen);QPixmap empty(24,24);empty.fill(Qt::transparent);
            negative.setWindowIcon(QIcon(empty));negative.setProperty("studio_window_icon","negative-transparent");negative.show();
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            const auto result=windowChromeProbe(&negative,workflowOutput_+"/icons/blank");negative.close();
            workflowArtifacts_["chrome_blank_icon_control"]=result;
            workflowCase("native-probe-detects-blank-icon",result["icon_pixels"].toInt()==0);
        }
        workflowArtifacts_["close_controls"]=rows;
        workflowArtifacts_["unexpected_message_windows"]=messageLifecycle.unexpected;
        workflowCase("message-components-never-open-independent-windows",messageLifecycle.unexpected.isEmpty());
        workflowArtifacts_["close_calculators"]=QJsonArray::fromStringList(methods);
        workflowCase("close-audit-preserves-document",session_.model()==original&&session_.modified()==modified&&!session_.busy());
    }catch(const std::exception& error){workflowArtifacts_["close_controls"]=rows;finishWorkflow(QString::fromUtf8(error.what()));}
}
