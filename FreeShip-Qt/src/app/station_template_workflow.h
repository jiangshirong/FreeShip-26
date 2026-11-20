// SPDX-License-Identifier: GPL-2.0-or-later
// Hidden production-route probe for the companion isolated test suite.
#pragma once
#include "section_dialog.h"
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>

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
    dialog->findChild<QComboBox*>("FrameContourType")->setCurrentIndex(cmd["thickness"].toDouble()>0?1:0);
    if(cmd.contains("layer")) dialog->findChild<QComboBox*>("FrameLayer")->setCurrentIndex(cmd["layer"].toInt()+1);
    if(cmd.contains("thickness")) dialog->findChild<QDoubleSpinBox*>("FrameThickness")->setValue(cmd["thickness"].toDouble());
    dialog->findChild<QCheckBox*>("FrameCap")->setChecked(cmd["cap"].toBool());
    auto status=[&]()->QLabel*{for(auto* l:dialog->findChildren<QLabel*>())if(l->property("studio_message_busy").isValid())return l;return nullptr;};
    wait([&]{return status()&&!status()->property("studio_message_busy").toBool();});
    QJsonObject evidence;
    auto* list=dialog->findChild<QListWidget*>("FrameStations");
    QJsonArray selected;
    for(int i=0;i<list->count();++i) if(list->item(i)->checkState()==Qt::Checked) selected.append(list->item(i)->data(Qt::UserRole).toInt());
    evidence["indices"]=selected;
    const auto name=cmd["label"].toString();
    evidence["message"]=status()->text();
    const bool enabled=dialog->findChild<QAbstractButton*>("FrameDxf")->isEnabled();evidence["export_enabled"]=enabled;
    evidence["pdf_enabled"]=dialog->findChild<QAbstractButton*>("FramePdf")->isEnabled();
    auto* pdfButton=dialog->findChild<QPushButton*>("FramePdf");
    auto* dxfButton=dialog->findChild<QPushButton*>("FrameDxf");
    auto* menu=pdfButton->menu();
    auto* tiled=menu->findChild<QAction*>("StationPdfTiles");
    auto* whole=menu->findChild<QAction*>("StationPdfWhole");
    auto* scale=dialog->findChild<QSpinBox*>("FramePdfScale");
    evidence["initial_scale"]=scale->value();
    evidence["initial_a4_enabled"]=tiled->isEnabled();
    auto* noTrim=dialog->findChild<QRadioButton*>("FramePdfNoTrim");
    evidence["default_trim"]=!noTrim->isChecked();
    evidence["edge_help"]=dialog->findChild<QToolButton*>("FramePdfNoTrimHelp")->toolTip();
    if(cmd["edge"].toBool())noTrim->click();
    if(cmd.contains("scale"))scale->setValue(cmd["scale"].toInt());
    evidence["a4_enabled"]=tiled->isEnabled();
    evidence["whole_enabled"]=whole->isEnabled();
    auto* requested=cmd["whole_page"].toBool()?whole:tiled;
    evidence["pdf_layout"]=requested->text();
    const QString prefix=folder+'/'+name;
    if(cmd["capture"].toBool()){
        QApplication::setActiveWindow(dialog);
        scale->setFocus(Qt::TabFocusReason);
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
            auto* picker=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
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
        if(suffix=="pdf") {
            if(!requested->isEnabled()) throw std::runtime_error("Requested PDF layout unavailable");
            QObject::connect(menu,&QMenu::aboutToShow,&choose,[menu,requested,&choose]{
                QTimer::singleShot(0,&choose,[menu,requested]{
                    menu->close();requested->trigger();
                });
            });
            pdfButton->showMenu();
        } else dialog->findChild<QAbstractButton*>("FrameDxf")->click();
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

