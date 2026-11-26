// SPDX-License-Identifier: GPL-2.0-or-later
// Hidden production-route probe for the companion isolated test suite.
#pragma once
#include "studio_ui.h"
#include "pdf_preview_probe.h"
#include "section_dialog.h"
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileDialog>
#include <QHeaderView>
#include <QHelpEvent>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QToolTip>
#include <QToolButton>
#include <QTransform>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

inline void selectComparisonFile(QDialog* dialog,const QStringList& paths){
    if(!paths.isEmpty()){
        const bool old=QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);
        const auto restore=qScopeGuard([old]{QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,old);});
        QTimer::singleShot(25,dialog,[&]{
            if(auto* f=qobject_cast<QFileDialog*>(frontStudioDialog())){
                if(paths.size()==1)f->selectFile(paths.first());
                else{
                    f->setDirectory(QFileInfo(paths.first()).absolutePath());
                    QStringList names;
                    for(const auto& path:paths)names.append(QStringLiteral("\"%1\"").arg(QFileInfo(path).fileName()));
                    f->findChild<QLineEdit*>("fileNameEdit")->setText(names.join(u' '));
                }
                QMetaObject::invokeMethod(f,"accept",Qt::DirectConnection);
            }
        });
        dialog->findChild<QPushButton*>("ComparisonBrowse")->click();
    }
}

inline QJsonObject observeModelComparison(QDialog* dialog,const QString& folder,const QJsonObject& cmd){
    if(!dialog)throw std::runtime_error("File menu did not open model comparison");
    const auto close=qScopeGuard([&]{dialog->close();});
    auto* tabs=dialog->findChild<QTabWidget*>("ModelComparisonTabs");
    auto* chips=dialog->findChild<QWidget*>("ComparisonSourceChips");
    auto* table=dialog->findChild<QTableWidget*>("ComparisonParameters");
    auto* browse=dialog->findChild<QPushButton*>("ComparisonBrowse");
    auto* mode=dialog->findChild<QComboBox*>("ComparisonMode");
    if(!browse || browse->isHidden() || !tabs || tabs->currentIndex()!=0 || !chips || !table || !mode ||
       dialog->findChild<QComboBox*>("ComparisonSource"))throw std::runtime_error("Missing primary comparison page");
    auto wait=[&](auto done){
        QElapsedTimer timer;timer.start();
        while(!done()){
            if(timer.elapsed()>90000)throw std::runtime_error("Comparison timed out");
            QEventLoop turn;QTimer::singleShot(10,&turn,&QEventLoop::quit);turn.exec();
        }
    };
    wait([&]{return browse->isEnabled();});
    QStringList paths;
    if(cmd.contains("reference_fbm"))paths.append(cmd["reference_fbm"].toString());
    for(const auto& path:cmd["reference_files"].toArray())paths.append(path.toString());
    selectComparisonFile(dialog,paths);
    wait([&]{return table->property("ready").toBool();});
    if(cmd.contains("mode")||cmd.contains("fields")){
        if(cmd.contains("mode"))mode->setCurrentIndex(cmd["mode"].toInt());
        const auto fields=cmd["fields"].toObject();
        for(auto it=fields.begin();it!=fields.end();++it){
            auto* box=dialog->findChild<QDoubleSpinBox*>("Comparison"+it.key());
            if(!box)throw std::runtime_error("Absent comparison control");
            box->setValue(it.value().toDouble());
        }
        dialog->findChild<QPushButton*>("ComparisonCalculate")->click();
        wait([&]{return table->property("ready").toBool();});
    }
    QJsonObject values;QJsonArray modelRows,modelChips,headers,solveStates;
    QStringList errors;
    const auto modelButtons=chips->findChildren<QToolButton*>("ComparisonModelChip");
    for(auto* button:modelButtons)if(!button->isHidden())modelChips.append(button->text());
    for(int row=0;row<table->rowCount();++row){
        const auto model=table->item(row,1)->text();modelRows.append(model);
        const auto* cell=table->item(row,0);
        if(!cell)throw std::runtime_error("Solve status missing from comparison row");
        const bool success=cell->data(Qt::UserRole).toBool();
        const auto reason=cell->toolTip();
        if(!success)errors.append(model+QStringLiteral(" · ")+reason);
        QJsonObject state{{"success",success},{"icon",cell->icon().name()},{"cell",cell->text()},{"reason",reason}};
        if(cmd["status_hover"].toBool()&&!success){
            table->scrollToItem(cell);
            const auto position=table->visualItemRect(cell).center();
            auto* viewport=table->viewport();
            QMouseEvent enter(QEvent::MouseMove,position,viewport->mapToGlobal(position),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(viewport,&enter);
            state["hover"]=QToolTip::isVisible()?QToolTip::text():QString{};
            QEvent leave(QEvent::Leave);QApplication::sendEvent(viewport,&leave);
        }
        solveStates.append(state);
    }
    for(int c=2;c<table->columnCount();++c){
        const auto* header=table->horizontalHeaderItem(c);
        headers.append(header->text());
        QJsonArray cells;
        for(int row=0;row<table->rowCount();++row)cells.append(QJsonValue::fromVariant(table->item(row,c)->data(Qt::UserRole)));
        if(cells.size()==2)cells.append(cells[0].isDouble()&&cells[1].isDouble()?QJsonValue(cells[0].toDouble()-cells[1].toDouble()):QJsonValue());
        values[header->data(Qt::UserRole).toString()]=cells;
    }
    if(cmd["capture"].toBool())dialog->grab().save(folder+"/"+cmd["label"].toString()+".png");
    auto* progress=dialog->findChild<QLabel*>("ComparisonProgress");
    QJsonArray modeNames;
    for(int index=0;index<mode->count();++index)modeNames.append(mode->itemText(index));
    QJsonObject result{{"values",values},{"error",errors.join('\n')},
        {"mode_names",modeNames},
        {"models",modelRows},{"chips",modelChips},{"headers",headers},{"solve_states",solveStates},
        {"solve_heading",table->horizontalHeaderItem(0)->text()},
        {"no_status_below_table",progress&&progress->isHidden()&&progress->text().isEmpty()}};
    if(cmd.contains("station_index")){
        tabs->setCurrentIndex(1);
        auto* stations=dialog->findChild<QComboBox*>("ComparisonStation");
        stations->setCurrentIndex(stations->findData(cmd["station_index"].toInt()));
        auto* canvas=dialog->findChild<QWidget*>("ComparisonCanvas");
        wait([&]{return canvas->property("ready").toBool();});
        result["section_mm"]=canvas->property("maximumMm").toDouble();
        if(cmd["exercise_chips"].toBool()){
            QJsonArray deviations;
            for(auto* chip:modelButtons)if(!chip->isHidden()){
                chip->click();
                wait([&]{return canvas->property("ready").toBool();});
                deviations.append(canvas->property("maximumMm").toDouble());
            }
            result["chip_section_deviations"]=deviations;
        }
    }
    if(cmd["exercise_chips"].toBool()){
        tabs->setCurrentIndex(0);
        const auto first=std::find_if(modelButtons.cbegin(),modelButtons.cend(),[](const auto* button){return !button->isHidden();});
        if(first==modelButtons.cend())throw std::runtime_error("No reference block to remove");
        auto* chip=*first;
        const auto pos=chip->rect().center();
        const auto send=[&](QEvent::Type type,QPoint point,Qt::MouseButton button,Qt::MouseButtons buttons){
            QMouseEvent event(type,point,chip->mapToGlobal(point),button,buttons,Qt::NoModifier);
            QApplication::sendEvent(chip,&event);
        };
        const auto previousRect=chip->geometry();
        const bool selected=chip->isChecked();
        send(QEvent::MouseButtonPress,pos,Qt::LeftButton,Qt::LeftButton);
        send(QEvent::MouseMove,pos+QPoint(70,0),Qt::NoButton,Qt::LeftButton);
        send(QEvent::MouseMove,pos,Qt::NoButton,Qt::LeftButton);
        send(QEvent::MouseButtonRelease,pos,Qt::LeftButton,Qt::NoButton);
        result["chip_drag_inactive"]=chip->geometry()==previousRect&&chip->isChecked()==selected&&table->rowCount()==modelRows.size();
        result["delete_buttons_removed"]=!chips->findChild<QToolButton*>("ComparisonRemoveModel");
        send(QEvent::MouseButtonPress,pos,Qt::RightButton,Qt::RightButton);
        send(QEvent::MouseButtonRelease,pos,Qt::RightButton,Qt::NoButton);
        wait([&]{return table->property("ready").toBool();});
        result["rows_after_removal"]=table->rowCount();
    }
    return result;
}

inline QJsonObject observeStationComparison(SectionDialog& editor,const QString& folder,const QJsonObject& cmd,
                                           const std::function<bool()>& editCurrent={}) {
    editor.findChild<QTabWidget*>()->setCurrentIndex(0);
    QAbstractButton* launch=nullptr;
    for(auto* button:editor.findChildren<QAbstractButton*>())
        if(button->property("section_operation")=="compare")launch=button;
    if(!launch||!launch->isEnabled())throw std::runtime_error("Station comparison action unavailable");
    launch->click();
    auto* dialog=editor.findChild<QDialog*>("StationComparison");
    if(!dialog)throw std::runtime_error("Station comparison did not open");
    if(auto* browse=dialog->findChild<QPushButton*>("ComparisonBrowse");browse&&!browse->isHidden())
        throw std::runtime_error("Unexpected model file picker in section comparison");
    const auto close=qScopeGuard([&]{dialog->close();editor.close();});
    auto* stations=dialog->findChild<QComboBox*>("ComparisonStation");
    auto* canvas=dialog->findChild<QWidget*>("ComparisonCanvas");
    auto* sources=dialog->findChild<QComboBox*>("ComparisonSource");
    if(!stations||!canvas||!sources)throw std::runtime_error("Station comparison controls unavailable");
    const auto wait=[&](auto done){
        QElapsedTimer elapsed;
        elapsed.start();
        while(!done()){
            const auto error=canvas->property("error").toString();
            if(!error.isEmpty())throw std::runtime_error(error.toStdString());
            if(elapsed.elapsed()>20000)throw std::runtime_error("Station comparison did not finish");
            QEventLoop turn;
            QTimer::singleShot(10,&turn,&QEventLoop::quit);
            turn.exec();
        }
    };
    wait([&]{return sources->isEnabled();});
    if(cmd.contains("reference_tab")){
        if(sources->count()!=2)throw std::runtime_error("Expected open reference tab");
        sources->setCurrentIndex(1);
    }
    const int row=stations->findData(cmd["station_index"].toInt(-1));
    if(row<0)throw std::runtime_error("Requested comparison station unavailable");
    stations->setCurrentIndex(row);
    wait([&]{return canvas->property("ready").toBool();});
    QJsonObject evidence{{"station_index",stations->currentData().toInt()}};
    QJsonArray visibleStations;
    for(int i=0;i<stations->count();++i)visibleStations.append(stations->itemData(i).toInt());
    evidence["available_stations"]=visibleStations;
    for(const char* key:{"maximumMm","referenceYmm","referenceZmm","modelYmm","modelZmm"}){
        const auto value=canvas->property(key);
        if(!value.isValid())throw std::runtime_error("Station comparison metric missing");
        evidence[key]=value.toDouble();
    }
    if(cmd["check_interaction"].toBool()){
        auto view=canvas->grab();
        const auto transform=canvas->property("contourToCanvas").value<QTransform>();
        const QPointF a(evidence["referenceYmm"].toDouble(),evidence["referenceZmm"].toDouble());
        const QPointF b(evidence["modelYmm"].toDouble(),evidence["modelZmm"].toDouble());
        const auto midpoint=transform.map((a+b)/2)*view.devicePixelRatio();
        const auto image=view.toImage();
        bool marker=false;
        for(int x=-2;x<=2;++x)for(int y=-2;y<=2;++y){
            const auto pixel=midpoint.toPoint()+QPoint(x,y);
            if(image.rect().contains(pixel)){
                const auto c=image.pixelColor(pixel);
                marker|=c.red()>c.green()+55&&c.green()>c.blue()+20;
            }
        }
        const auto hover=[&](QPointF point){
            const auto local=transform.map(point);
            QMouseEvent move(QEvent::MouseMove,local,canvas->mapToGlobal(local.toPoint()),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(canvas,&move);
            return QToolTip::isVisible()?QToolTip::text():QString{};
        };
        QJsonArray tooltips;
        for(const auto value:cmd["hover_mm"].toArray()){
            const auto point=value.toArray();
            tooltips.append(hover(QPointF(point[0].toDouble(),point[1].toDouble())));
        }
        const QJsonArray maximumTooltips{hover(a),hover(b),hover((a+b)/2)};
        QEvent leave(QEvent::Leave);QApplication::sendEvent(canvas,&leave);
        QJsonObject interaction{{"title_maximize",dialog->windowFlags().testFlag(Qt::WindowMaximizeButtonHint)},
            {"checkbox_count",dialog->findChildren<QCheckBox*>().size()},{"marker_ink",marker},
            {"automatic_label",canvas->accessibleDescription()},{"hover_tooltips",tooltips},
            {"maximum_tooltips",maximumTooltips}};
        const auto normal=dialog->size(),small=canvas->size();
#ifdef Q_OS_WIN
        const auto hwnd=reinterpret_cast<HWND>(dialog->winId());
        DWORD owner=0;GetWindowThreadProcessId(hwnd,&owner);
        interaction["native_maximize_button"]=owner==GetCurrentProcessId()&&(GetWindowLongPtrW(hwnd,GWL_STYLE)&WS_MAXIMIZEBOX);
#endif
        dialog->showMaximized();QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        interaction["maximized"]=dialog->isMaximized();
        dialog->showNormal();dialog->resize(normal+QSize(190,95));QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        interaction["canvas_grows"]=canvas->width()>small.width()&&canvas->height()>small.height();
        evidence["interaction"]=interaction;
    }
    if(cmd["capture"].toBool()){
        const QString path=folder+'/'+cmd["label"].toString()+".png";
        if(!dialog->grab().save(path))throw std::runtime_error("Cannot save comparison view");
        evidence["image"]=path;
    }
    if(editCurrent){
        const auto previous=canvas->property("comparison_revision").toULongLong();
        const bool nonModal=dialog->isVisible()&&!dialog->isModal()&&!QApplication::activeModalWidget();
        if(!editCurrent())throw std::runtime_error("Model edit failed while comparison was open");
        wait([&]{return canvas->property("comparison_revision").toULongLong()>previous&&canvas->property("ready").toBool();});
        evidence["live_update"]=QJsonObject{{"nonmodal",nonModal&&dialog->isVisible()},
            {"before",evidence["maximumMm"]},{"after",canvas->property("maximumMm").toDouble()},
            {"same_station",evidence["station_index"].toInt()==stations->currentData().toInt()}};
    }
    return evidence;
}

inline QJsonObject observeStationExport(SectionDialog& editor,const QString& folder,const QJsonObject& cmd) {
    const bool savedNativeOption=QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);
    const auto restoreOption=qScopeGuard([savedNativeOption]{QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,savedNativeOption);});
    auto wait=[](const std::function<bool()>& ready){
        QElapsedTimer time;time.start();
        while(!ready()) {
            if(time.elapsed()>90000) throw std::runtime_error("Station export probe timed out");
            QEventLoop turn;QTimer::singleShot(10,&turn,&QEventLoop::quit);turn.exec();
        }
    };
    editor.findChild<QTabWidget*>()->setCurrentIndex(0);
    auto* table=editor.findChild<QTableWidget*>("SectionDefinitions");
    table->clearSelection();
    for(auto id:cmd["indices"].toArray()) for(int row=0;row<table->rowCount();++row)
        if(table->item(row,0)->data(Qt::UserRole).toInt()==id.toInt())
            table->selectionModel()->select(table->model()->index(row,0),QItemSelectionModel::Select|QItemSelectionModel::Rows);
    QAbstractButton* launch=nullptr;
    for(auto* button:editor.findChildren<QAbstractButton*>()) if(button->property("section_operation")=="templates") launch=button;
    if(!launch || !launch->isEnabled()) throw std::runtime_error("No enabled station export action");
    launch->click();
    auto* dialog=editor.findChild<QDialog*>("StationTemplates");
    if(!dialog) throw std::runtime_error("Station export dialog did not open");
    if(cmd["measure_station_shell"].toBool())dialog->setProperty("measure_station_shell",true);
    dialog->findChild<QComboBox*>("FrameContourType")->setCurrentIndex(cmd.contains("kit")?2:cmd["thickness"].toDouble()>0?1:0);
    QJsonObject evidence;
    if(cmd.contains("kit")||cmd.contains("mould")){
        const bool kit=cmd.contains("kit");
        const auto key=kit?QStringLiteral("kit"):QStringLiteral("mould");
        const auto edits=cmd[key].toObject();
        const auto positions=edits["positions"].toArray();
        const auto prefix=kit?QStringLiteral("Kit"):QStringLiteral("Mould");
        if(kit)dialog->findChild<QDoubleSpinBox*>("KitStock")->setValue(edits["KitStock"].toDouble());
        auto* grid=dialog->findChild<QTableWidget*>(prefix+"Stations");
        auto* add=dialog->findChild<QToolButton*>(prefix+"AddStation");
        if(!grid||!add||!grid->isVisibleTo(dialog))throw std::runtime_error("Missing active station table");
        const auto captureStations=[grid]{
            QJsonArray rows;
            for(int row=0;row<grid->rowCount();++row){
                const auto* x=grid->item(row,1);
                rows.append(QJsonObject{{"number",grid->item(row,0)->text().toInt()},
                    {"x",x->data(Qt::EditRole).toDouble()},{"editable",x->flags().testFlag(Qt::ItemIsEditable)}});
            }
            return rows;
        };
        evidence[key+"_stations_before"]=captureStations();
        const auto removeAt=[grid,prefix](int row)->QToolButton*{
            for(auto* button:grid->findChildren<QToolButton*>(prefix+"RemoveStation")){
                const auto index=grid->indexAt(button->geometry().center());
                if(button->isVisibleTo(grid)&&index.row()==row&&index.column()==0)return button;
            }
            return nullptr;
        };
        auto commit=[&](int row,double x){
            auto* editor=qobject_cast<QDoubleSpinBox*>(grid->indexWidget(grid->model()->index(row,1)));
            if(!editor)throw std::runtime_error("Missing editable station coordinate");
            editor->setValue(x);
            QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
            QApplication::sendEvent(editor,&enter);
            wait([&]{return grid->indexWidget(grid->model()->index(row,1))==nullptr;});
            if(grid->item(row,1)->data(Qt::EditRole).toDouble()!=x)
                throw std::runtime_error("Station coordinate edit did not commit");
        };
        const auto remove=[&](int row){
            const auto before=captureStations();
            grid->scrollTo(grid->model()->index(row,0));
            auto* button=removeAt(row);
            if(!button)throw std::runtime_error("Missing station row action");
            button->click();
            auto expected=before;
            expected.removeAt(row);
            if(captureStations()!=expected)throw std::runtime_error("Station row action did not remove its own row");
        };
        const auto insert=[&](int row,double x){
            const int count=grid->rowCount();
            if(count>0)grid->setCurrentCell(row-1,1);
            add->click();
            if(grid->rowCount()!=count+1)throw std::runtime_error("Station insertion failed");
            commit(row,x);
        };
        if(edits.contains("remove_row"))remove(edits["remove_row"].toInt());
        if(edits.contains("insert_x"))insert(edits["insert_row"].toInt(),edits["insert_x"].toDouble());
        if(edits.contains("positions")){
            while(grid->rowCount()>0)remove(grid->rowCount()-1);
            for(const auto x:positions)insert(grid->rowCount(),x.toDouble());
        }
        const auto beforeSwitch=captureStations();
        evidence[key+"_stations_after"]=beforeSwitch;
        if(edits["check_switch"].toBool()){
            auto* mode=dialog->findChild<QComboBox*>("FrameContourType");
            auto* other=dialog->findChild<QTableWidget*>(kit?"MouldStations":"KitStations");
            const int count=other->rowCount();
            mode->setCurrentIndex(kit?1:2);
            const bool switched=other->isVisibleTo(dialog)&&!grid->isVisibleTo(dialog);
            mode->setCurrentIndex(kit?2:1);
            evidence["station_tables_keep_separate_edits"]=switched&&other->rowCount()==count&&captureStations()==beforeSwitch;
        }
    }
    if(cmd.contains("layer")) dialog->findChild<QComboBox*>("FrameLayer")->setCurrentIndex(cmd["layer"].toInt()+1);
    QElapsedTimer previewElapsed;
    previewElapsed.start();
    if(cmd.contains("thickness")) dialog->findChild<QDoubleSpinBox*>("FrameThickness")->setValue(cmd["thickness"].toDouble());
    auto* cap=dialog->findChild<QCheckBox*>("FrameCap");
    evidence["cap_visible"]=cap->isVisibleTo(dialog);
    evidence["cap_initially_checked"]=cap->isChecked();
    if(cmd.contains("cap"))cap->setChecked(cmd["cap"].toBool());
    auto status=[&]()->QLabel*{for(auto* l:dialog->findChildren<QLabel*>())if(l->property("studio_message_busy").isValid())return l;return nullptr;};
    wait([&]{return status()&&!status()->property("studio_message_busy").toBool();});
    evidence["preview_elapsed_ms"]=double(previewElapsed.elapsed());
    if(cmd["measure_station_shell"].toBool())evidence["shell_profile"]=dialog->property("station_shell_profile").toJsonArray();
    if(cmd["exercise_preview_motion"].toBool()){
        const auto oldMotion=qApp->property("studio_motion_enabled");
        qApp->setProperty("studio_motion_enabled",true);
        const auto restoreMotion=qScopeGuard([oldMotion]{qApp->setProperty("studio_motion_enabled",oldMotion);});
        auto* canvas=dialog->findChild<QWidget*>("StationTemplatePreview");
        auto* footer=dialog->findChild<QLabel*>("StationTemplateStatus");
        auto* exportButton=dialog->findChild<QPushButton*>("FrameDxf");
        auto* stock=dialog->findChild<QDoubleSpinBox*>("KitStock");
        auto* skin=dialog->findChild<QDoubleSpinBox*>("FrameThickness");
        if(!canvas||!footer||!exportButton||!stock||!skin)throw std::runtime_error("Missing station preview motion controls");
        QJsonArray edits;
        const auto frameRect=[&]{return QRect(canvas->mapTo(dialog,QPoint{}),canvas->size());};
        const auto observe=[&](const QString& setting,const std::function<void()>& change){
            const auto bounds=frameRect();
            const auto before=canvas->grab().toImage();
            change();
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            const bool held=footer->property("studio_message_busy").toBool()&&before==canvas->grab().toImage();
            const bool disabled=!exportButton->isEnabled();
            const bool sameHeight=frameRect()==bounds;
            const auto footerPos=footer->mapTo(dialog,QPoint{});
            const auto buttonPos=exportButton->mapTo(dialog,QPoint{});
            const bool footerInRow=footerPos.x()<buttonPos.x()&&footerPos.y()>bounds.bottom()&&
                std::abs((footerPos.y()+footer->height()/2)-(buttonPos.y()+exportButton->height()/2))<=1;
            if(setting=="stock")dialog->grab().save(folder+"/station-preview-while-computing.png");
            wait([&]{return !footer->property("studio_message_busy").toBool();});
            const QString motion=canvas->property("station_preview_motion").toString();
            QEventLoop step;QTimer::singleShot(86,&step,&QEventLoop::quit);step.exec(QEventLoop::ExcludeUserInputEvents);
            const auto intermediate=canvas->grab().toImage();
            if(setting=="stock")dialog->grab().save(folder+"/station-preview-transition.png");
            wait([&]{return !canvas->property("station_preview_motion").isValid();});
            const auto after=canvas->grab().toImage();
            edits.append(QJsonObject{{"setting",setting},{"mode",motion},{"holds_preview",held},
                {"locked_during_compute",disabled},{"same_canvas_geometry",sameHeight&&frameRect()==bounds},
                {"status_in_footer",footerInRow},{"intermediate_frame",intermediate!=before&&intermediate!=after},
                {"result_changed",before!=after},{"export_ready",exportButton->isEnabled()}});
        };
        observe("stock",[&]{stock->setValue(64);});
        observe("skin",[&]{skin->setValue(35);});
        observe("cap-off",[&]{cap->setChecked(false);});
        observe("cap-on",[&]{cap->setChecked(true);});
        evidence["preview_motion"]=edits;
    }
    auto* list=dialog->findChild<QListWidget*>("FrameStations");
    QJsonArray selected;
    for(int i=0;i<list->count();++i) if(list->item(i)->checkState()==Qt::Checked) selected.append(list->item(i)->data(Qt::UserRole).toInt());
    evidence["indices"]=selected;
    const auto name=cmd["label"].toString();
    evidence["message"]=status()->text();
    const bool enabled=dialog->findChild<QAbstractButton*>("FrameDxf")->isEnabled();evidence["export_enabled"]=enabled;
    evidence["pdf_enabled"]=dialog->findChild<QAbstractButton*>("FramePdf")->isEnabled();
    if(cmd["capture_warning"].toBool()){
        const auto mode=dialog->findChild<QComboBox*>("FrameContourType")->currentIndex();
        auto* grid=dialog->findChild<QTableWidget*>(mode==2?"KitStations":"MouldStations");
        QJsonArray states;
        if(grid&&grid->isVisibleTo(dialog))for(int row=0;row<grid->rowCount();++row){
            auto* cell=grid->item(row,2);
            const auto reason=cell->toolTip();
            QString hover;
            if(!reason.isEmpty()){
                const auto point=grid->visualItemRect(cell).center();
                QHelpEvent help(QEvent::ToolTip,point,grid->viewport()->mapToGlobal(point));
                QApplication::sendEvent(grid->viewport(),&help);
                hover=QToolTip::isVisible()?QToolTip::text():QString{};
                QToolTip::hideText();
            }
            states.append(QJsonObject{{"number",grid->item(row,0)->text().toInt()},
                {"warning",!cell->icon().isNull()},{"reason",reason},{"hover",hover}});
        }
        evidence["station_status"]=states;
        auto* preview=dialog->findChild<QWidget*>("StationTemplatePreview");
        const auto image=preview->grab().toImage();
        int diagnosticPixels=0,outlinePixels=0;
        for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){
            const auto c=image.pixelColor(x,y);
            if(y>qRound(45*image.devicePixelRatio())&&c.red()>c.green()+40&&c.green()>c.blue()+30&&c.blue()<100)++diagnosticPixels;
            if(c.red()<70&&c.green()>c.red()+20&&c.green()<120)++outlinePixels;
        }
        evidence["diagnostic_outline_pixels"]=diagnosticPixels;
        evidence["regular_outline_pixels"]=outlinePixels;
        const auto path=folder+'/'+name+"-warning.png";
        evidence["warning_image"]=dialog->grab().save(path)?path:QString{};
    }
    if(cmd["preview_only"].toBool()){
        if(cmd.contains("change_thickness")){
            QJsonArray samples;
            for(const auto value:cmd["change_thickness"].toArray()){
                previewElapsed.restart();
                dialog->findChild<QDoubleSpinBox*>("FrameThickness")->setValue(value.toDouble());
                wait([&]{return !status()->property("studio_message_busy").toBool();});
                samples.append(QJsonObject{{"thickness",value},{"elapsed_ms",double(previewElapsed.elapsed())},
                    {"valid",dialog->findChild<QAbstractButton*>("FrameDxf")->isEnabled()},{"message",status()->text()}});
            }
            evidence["change_thickness"]=samples;
        }
        dialog->close();editor.close();
        return evidence;
    }
    auto* pdfButton=dialog->findChild<QPushButton*>("FramePdf");
    auto* dxfButton=dialog->findChild<QPushButton*>("FrameDxf");
    const QString prefix=folder+'/'+name;
    const int tiledIndex=cmd["edge"].toBool()?0:1;
    const int targetIndex=cmd["whole_page"].toBool()?2:tiledIndex;
    evidence["pdf_click_opens_settings"]=pdfButton->menu()==nullptr;
    evidence["footer_mode_controls"]=dialog->findChildren<QRadioButton*>().size();
    evidence["footer_scale"]=dialog->findChild<QSpinBox*>("FramePdfScale")!=nullptr;
    if(cmd["capture_kit"].toBool()){
        auto* grid=dialog->findChild<QTableWidget*>("KitStations");
        auto* header=grid->horizontalHeader();
        auto* add=dialog->findChild<QToolButton*>("KitAddStation");
        const QRect firstHeading(header->sectionViewportPosition(0),0,header->sectionSize(0),header->height());
        double longest=QFontMetricsF(header->font()).horizontalAdvance(grid->horizontalHeaderItem(1)->text());
        for(int row=0;row<grid->rowCount();++row)
            longest=std::max(longest,QFontMetricsF(grid->font()).horizontalAdvance(grid->item(row,1)->text()));
        int rowActions=0;
        for(auto* button:grid->findChildren<QToolButton*>("KitRemoveStation"))
            if(button->isVisibleTo(grid)&&grid->indexAt(button->geometry().center()).column()==0)++rowActions;
        evidence["kit_table"]=QJsonObject{{"add_inside_station_heading",firstHeading.contains(add->geometry())},
            {"row_actions",rowActions},{"rows",grid->rowCount()},{"coordinate_width",grid->columnWidth(1)},
            {"coordinate_text_width",longest}};
        auto* preview=dialog->findChild<QWidget*>("StationTemplatePreview");
        const auto image=preview->grab().toImage();
        const int divider=image.height()*3/5;
        auto outlinePixels=[](const QImage& view,int top,int bottom){
            int count=0;
            for(int y=top;y<bottom;++y)for(int x=0;x<view.width();++x){
                const auto c=view.pixelColor(x,y);
                if(c.red()<70&&c.green()>c.red()+20&&c.green()<120)++count;
            }
            return count;
        };
        const bool both=outlinePixels(image,0,divider)>10&&outlinePixels(image,divider,image.height())>10;
        QRect spineInk;
        for(int y=divider+2;y<image.height();++y)for(int x=0;x<image.width();++x){
            const auto color=image.pixelColor(x,y);
            if(color.red()<80&&color.green()>color.red()+15&&color.green()<155)
                spineInk=spineInk.united(QRect(x,y,1,1));
        }
        evidence["spine_preview_centered"]=!spineInk.isEmpty()&&
            std::abs(spineInk.center().x()-image.width()/2)<=qRound(4*image.devicePixelRatio())&&
            std::abs(spineInk.center().y()-(divider+image.height())/2)<=qRound(7*image.devicePixelRatio());
        const auto before=image.copy(0,divider,image.width(),image.height()-divider);
        const int row=grid->currentRow();
        grid->setCurrentCell(row==0?1:0,0);
        const auto changed=preview->grab().toImage();
        const auto after=changed.copy(0,divider,changed.width(),changed.height()-divider);
        evidence["kit_preview"]=both&&before==after&&!dialog->findChild<QAbstractButton*>("KitSpine")&&
            dialog->grab().save(prefix+"-assembled-preview.png");
    }
    const auto openSettings=[&](bool exportPdf){
        bool handled=false,accepted=false;
        QTimer::singleShot(0,dialog,[&]{
            auto* settings=dialog->findChild<QDialog*>("StationPdfSettings");
            if(!settings){
                if(auto* modal=qobject_cast<QDialog*>(frontStudioDialog());modal&&modal->parentWidget()==dialog)
                    modal->reject();
                return;
            }
            auto* layout=settings->findChild<QComboBox*>("StationPdfLayout");
            auto* numerator=settings->findChild<QDoubleSpinBox*>("StationPdfNumerator");
            auto* denominator=settings->findChild<QDoubleSpinBox*>("StationPdfDenominator");
            auto* pages=settings->findChild<QLabel*>("StationPdfPageCount");
            auto* buttons=settings->findChild<QDialogButtonBox*>();
            if(!layout||layout->count()!=3||!numerator||!denominator||!pages||!buttons){settings->reject();return;}
            handled=true;
            auto* confirm=buttons->button(QDialogButtonBox::Ok);
            evidence["initial_scale"]=denominator->value();
            evidence["initial_numerator"]=numerator->value();
            evidence["pages_read_only"]=pages->frameShape()==QFrame::NoFrame&&pages->focusPolicy()==Qt::NoFocus;
            if(cmd["edge"].toBool()){
                layout->setCurrentIndex(1);
                layout->showPopup();
                auto* view=layout->view();
                QElapsedTimer opening;opening.start();
                while(!view->isVisible() && opening.elapsed()<500){
                    QEventLoop turn;QTimer::singleShot(10,&turn,&QEventLoop::quit);turn.exec();
                }
                auto* viewport=view->viewport();
                const bool popupOpened=view->isVisible();
                const auto hover=[&](int row){
                    const auto index=layout->model()->index(row,0);
                    const auto pos=view->visualRect(index).center();
                    QMouseEvent move(QEvent::MouseMove,pos,viewport->mapToGlobal(pos),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
                    QApplication::sendEvent(viewport,&move);
                };
                hover(0);
                evidence["edge_hover_observation"]=QJsonObject{
                    {"popup_opened",popupOpened},{"popup_visible",view->isVisible()},{"tooltip_visible",QToolTip::isVisible()},
                    {"text",QToolTip::text()},{"selected",layout->currentIndex()},
                    {"row_at_hover",view->indexAt(view->visualRect(layout->model()->index(0,0)).center()).row()}};
                evidence["edge_popup_hover"]=view->isVisible()&&QToolTip::isVisible()&&
                    QToolTip::text()==layout->itemData(0,Qt::ToolTipRole).toString()&&layout->currentIndex()==1;
                layout->hidePopup();
            }
            layout->setCurrentIndex(tiledIndex);
            evidence["initial_a4_enabled"]=confirm->isEnabled();
            if(cmd["preview_motion"].toBool()){
                auto* fit=settings->findChild<QCheckBox*>("PdfFitSinglePage");
                if(fit){
                    const double original=denominator->value();
                    denominator->setValue(50);
                    evidence["fit_motion"]=observePdfPreviewMotion(settings,[&]{fit->setChecked(true);})&&
                        !numerator->isVisible()&&!denominator->isVisible()&&pages->locale().toInt(pages->text())==cmd["indices"].toArray().size();
                    settings->grab().save(prefix+"-pdf-fit-settings.png");
                    layout->setCurrentIndex(2);
                    auto* computed=settings->findChild<QLabel*>("StationPdfComputedRatio");
                    evidence["whole_fit_available"]=fit->isEnabled()&&fit->isChecked()&&!numerator->isVisible()&&
                        !denominator->isVisible()&&computed&&computed->isVisible()&&computed->text().contains(':');
                    fit->setChecked(false);
                    evidence["manual_ratio_restored"]=numerator->isVisible()&&denominator->isVisible()&&denominator->value()==50&&
                        numerator->buttonSymbols()==QAbstractSpinBox::UpDownArrows&&denominator->buttonSymbols()==QAbstractSpinBox::UpDownArrows;
                    denominator->setValue(original);layout->setCurrentIndex(tiledIndex);
                }
            }
            QJsonArray layouts,initialAllowed;
            for(int i=0;i<layout->count();++i){
                layout->setCurrentIndex(i);
                layouts.append(layout->currentText());
                initialAllowed.append(confirm->isEnabled());
                if(i==0) evidence["edge_tooltip"]=layout->toolTip();
            }
            evidence["pdf_layouts"]=layouts;
            evidence["initial_allowed"]=initialAllowed;
            if(cmd["decimal_input"].toBool()){
                for(auto* field:{numerator,denominator}){
                    const auto key=field==numerator?"numerator":"scale";
                    if(auto* editor=field->findChild<QLineEdit*>())editor->setText(field->locale().toString(cmd[key].toDouble(1),'f',3));
                    field->interpretText();
                }
            }else{
                numerator->setValue(cmd["numerator"].toDouble(1));
                denominator->setValue(cmd["scale"].toDouble(1));
            }
            layout->setCurrentIndex(tiledIndex);
            evidence["a4_enabled"]=confirm->isEnabled();
            layout->setCurrentIndex(targetIndex);
            auto* fit=settings->findChild<QCheckBox*>("PdfFitSinglePage");
            if(fit)fit->setChecked(cmd["fit_page"].toBool());
            evidence["pdf_layout"]=layout->currentText();
            evidence["page_count"]=pages->locale().toInt(pages->text());
            if(cmd["capture"].toBool()||cmd["preview_motion"].toBool()){
                waitForPdfPreview(340);
                settings->grab().save(prefix+"-pdf-settings.png");
            }
            accepted=exportPdf&&confirm->isEnabled();
            if(accepted) confirm->click();
            else buttons->button(QDialogButtonBox::Cancel)->click();
        });
        pdfButton->click();
        if(!handled || (exportPdf&&!accepted))throw std::runtime_error("Station PDF settings unavailable");
    };
    if(cmd["capture"].toBool()){
        QApplication::setActiveWindow(dialog);
        dxfButton->setFocus(Qt::TabFocusReason);
        const auto idlePdf=pdfButton->grab().toImage();
        pdfButton->setFocus(Qt::TabFocusReason);
        const auto focusedPdf=pdfButton->grab().toImage();
        const auto borderMatches=[&]{
            if(idlePdf.isNull()||idlePdf.size()!=focusedPdf.size())return false;
            for(int x=8;x<idlePdf.width()-8;++x)
                if(idlePdf.pixel(x,0)!=focusedPdf.pixel(x,0)||
                   idlePdf.pixel(x,idlePdf.height()-1)!=focusedPdf.pixel(x,focusedPdf.height()-1))return false;
            return true;
        };
        const auto control=[](const QPushButton* button){
            return QJsonObject{{"caption",button->text()},{"icon",button->icon().name()},
                {"description",button->accessibleName()},{"tooltip",button->toolTip()}};
        };
        evidence["format_buttons"]=QJsonObject{{"pdf",control(pdfButton)},{"dxf",control(dxfButton)},
            {"pdf_tab_focus",pdfButton->hasFocus()},{"neutral_focus_border",borderMatches()},
            {"focus_fill_changes",focusedPdf!=idlePdf}};
        dialog->grab().save(prefix+".png");
    }
    if(pdfButton->isEnabled() && !cmd["pdf"].toBool()) openSettings(false);
    if(enabled && cmd["cancel_save"].toBool()){
        dialog->findChild<QAbstractButton*>("FrameDxf")->click();
        auto* picker=dialog->findChild<QFileDialog*>();
        if(!picker) throw std::runtime_error("No file picker for cancellation");
        picker->selectFile(prefix+"-cancel.dxf");picker->reject();
        evidence["cancel_clean"]=!status()->property("studio_message_busy").toBool() && !QFileInfo::exists(prefix+"-cancel.dxf");
    }
    if(enabled) for(const auto suffix:{QString("pdf"),QString("dxf")}) {
        if(suffix=="pdf"&&!cmd["pdf"].toBool())continue;
        const auto path=prefix+'.'+suffix;
        const QFileInfo target(path);
        QElapsedTimer elapsed;elapsed.start();
        QTimer choose;choose.setInterval(15);
        bool filled=false,confirmed=false;
        QString picked,acceptedPath;
        int pickerResult=-1;
        QObject::connect(&choose,&QTimer::timeout,dialog,[&]{
            auto* picker=qobject_cast<QFileDialog*>(frontStudioDialog());
            if(!picker || picker->parentWidget()!=dialog) return;
            if(elapsed.elapsed()>6000){choose.stop();picker->reject();return;}
            if(!filled){
                QObject::connect(picker,&QFileDialog::fileSelected,&choose,[&](const QString& file){acceptedPath=file;});
                QObject::connect(picker,&QDialog::finished,&choose,[&](int result){pickerResult=result;});
                if(auto* focus=picker->focusWidget()) focus->clearFocus();
                picker->setDirectory(target.absolutePath());picker->selectFile(target.fileName());
                if(auto* edit=picker->findChild<QLineEdit*>("fileNameEdit")) edit->setText(target.fileName());
                filled=true;return;
            }
            picked=picker->selectedFiles().join('|');
            if(QFileInfo(picked).absoluteFilePath()!=target.absoluteFilePath()) return;
            choose.stop();confirmed=true;
            QMetaObject::invokeMethod(picker,"accept",Qt::DirectConnection);
        });
        choose.start();
        if(suffix=="pdf") openSettings(true);
        else dxfButton->click();
        wait([&]{return pickerResult!=-1;});
        choose.stop();
        if(!acceptedPath.isEmpty())
            wait([&]{return !status()->property("studio_message_busy").toBool() && !status()->text().isEmpty();});
        evidence[suffix]=confirmed&&QFileInfo::exists(path);evidence[suffix+"_message"]=status()->text();
        evidence[suffix+"_selection"]=picked;
        evidence[suffix+"_picker"]=QJsonObject{{"sent",confirmed},{"result",pickerResult},{"path",acceptedPath}};
    }
    dialog->close();editor.close();
    return evidence;
}

