// SPDX-License-Identifier: GPL-2.0-or-later
#include "print_preview_style.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGraphicsView>
#include <QLineEdit>
#include <QLabel>
#include <QPrintPreviewDialog>
#include <QPrintPreviewWidget>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPushButton>
#include <QSpinBox>
#include <QSet>
#include <QToolBar>
#include <QToolButton>

namespace {
QDialogButtonBox* confirmation(QDialog& dialog,QFormLayout& form){
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    form.addRow(buttons);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    return buttons;
}
void previewPaper(QPrintPreviewDialog* preview,QPrintPreviewWidget* view,QPrinter& printer){
    QDialog dialog(preview);dialog.setObjectName("PrintPageSetup");dialog.setWindowTitle(uiText("页面设置"));setStudioWindowIcon(&dialog,"Print");
    auto layout=printer.pageLayout();layout.setUnits(QPageLayout::Millimeter);
    auto* form=new QFormLayout(&dialog);
    auto* paper=new QComboBox;
    auto sizes=QPrinterInfo::printerInfo(printer.printerName()).supportedPageSizes();
    if(sizes.isEmpty())for(const auto id:{QPageSize::A4,QPageSize::A3,QPageSize::Letter,QPageSize::Legal})sizes.append(QPageSize(id));
    if(!sizes.contains(layout.pageSize()))sizes.prepend(layout.pageSize());
    for(const auto& size:sizes)paper->addItem(size.name());
    paper->setCurrentIndex(sizes.indexOf(layout.pageSize()));form->addRow(uiText("纸张"),paper);
    auto* orientation=new QComboBox;
    orientation->setObjectName("PrintPageOrientation");
    orientation->addItems({uiText("纵向"),uiText("横向")});orientation->setCurrentIndex(layout.orientation()==QPageLayout::Landscape?1:0);
    form->addRow(uiText("方向"),orientation);
    const auto margins=layout.margins();
    QList<QDoubleSpinBox*> fields;
    for(const auto& edge:QList<QPair<QString,double>>{{uiText("左边距"),margins.left()},{uiText("上边距"),margins.top()},
            {uiText("右边距"),margins.right()},{uiText("下边距"),margins.bottom()}}){
        auto* field=new QDoubleSpinBox;field->setRange(0,1000);field->setSuffix(QStringLiteral(" mm"));field->setValue(edge.second);
        form->addRow(edge.first,field);fields.append(field);
    }
    confirmation(dialog,*form);
    if(runStudioDialog(dialog)!=QDialog::Accepted)return;
    layout.setOrientation(orientation->currentIndex()==1?QPageLayout::Landscape:QPageLayout::Portrait);
    layout.setPageSize(sizes.at(paper->currentIndex()),layout.minimumMargins());
    layout.setMargins({fields[0]->value(),fields[1]->value(),fields[2]->value(),fields[3]->value()},QPageLayout::OutOfBoundsPolicy::Clamp);
    if(printer.setPageLayout(layout))view->updatePreview();
}
void previewPrint(QPrintPreviewDialog* preview,QPrintPreviewWidget* view,QPrinter& printer){
    const auto names=QPrinterInfo::availablePrinterNames();
    QDialog dialog(preview);dialog.setObjectName("PrintDeviceSetup");dialog.setWindowTitle(uiText("打印"));setStudioWindowIcon(&dialog,"Print");
    auto* form=new QFormLayout(&dialog);
    auto* printers=new QComboBox;
    printers->addItems(names);printers->setCurrentIndex(std::max(0,int(names.indexOf(printer.printerName()))));
    form->addRow(uiText("打印机"),printers);
    auto* copies=new QSpinBox;copies->setRange(1,999);copies->setValue(printer.copyCount());form->addRow(uiText("份数"),copies);
    auto* color=new QComboBox;color->addItems({uiText("彩色"),uiText("灰度")});color->setCurrentIndex(printer.colorMode()==QPrinter::GrayScale?1:0);
    form->addRow(uiText("颜色"),color);
    auto* button=confirmation(dialog,*form)->button(QDialogButtonBox::Ok);
    button->setText(uiText("打印"));button->setEnabled(!names.isEmpty());
    if(runStudioDialog(dialog)!=QDialog::Accepted||printers->currentText().isEmpty())return;
    const auto page=printer.pageLayout();
    printer.setPrinterName(printers->currentText());printer.setPageLayout(page);
    printer.setCopyCount(copies->value());printer.setColorMode(color->currentIndex()==1?QPrinter::GrayScale:QPrinter::Color);
    if(printer.isValid())view->print();
}
// Keep Qt's original PrintSupport connections and widget actions. Only the
// visible toolbar policy changes; orientation stays in the native page setup.
class PreviewControls final:public QObject {
    QPrintPreviewDialog* dialog_;
    QPrintPreviewWidget* preview_;
    QAction* overview_=nullptr;
    bool allowMultiPage_,updating_=false;
    bool visible(QAction* action,bool multi)const{
        if(action->property("studio_native_overview").toBool())return false;
        const auto role=action->property("print_operation").toString();
        static const QSet<QString> common{"FitWidth","FitPage","ZoomOut","ZoomIn","Setup","Print"};
        return common.contains(role)||(multi&&(role=="Previous"||role=="Next"||action==overview_));
    }
public:
    PreviewControls(QPrintPreviewDialog* dialog,bool allowMultiPage):QObject(dialog),dialog_(dialog),
        preview_(dialog->findChild<QPrintPreviewWidget*>()),allowMultiPage_(allowMultiPage){
        if(!preview_)return;
        QAction* nativeOverview=nullptr;
        for(auto* action:dialog->findChildren<QAction*>())if(action->property("print_operation")=="Overview")nativeOverview=action;
        if(nativeOverview){
            overview_=new QAction(nativeOverview->icon(),nativeOverview->text(),dialog);overview_->setToolTip(nativeOverview->toolTip());
            overview_->setCheckable(true);overview_->setProperty("print_operation","Overview");
            nativeOverview->setProperty("print_operation",QVariant{});nativeOverview->setProperty("studio_native_overview",true);
            connect(nativeOverview,&QAction::changed,overview_,[nativeOverview,action=overview_]{
                action->setText(nativeOverview->text());action->setToolTip(nativeOverview->toolTip());
            });
            for(auto* button:dialog->findChildren<QToolButton*>())if(button->defaultAction()==nativeOverview)button->setProperty("print_operation",QVariant{});
            for(auto* bar:dialog->findChildren<QToolBar*>()){
                QAction* before=nullptr;
                for(auto* action:bar->actions()){
                    auto* widget=bar->widgetForAction(action);if(action==nativeOverview){before=action;break;}
                    if(widget)for(auto* button:widget->findChildren<QToolButton*>())if(button->defaultAction()==nativeOverview)before=action;
                    if(before)break;
                }
                if(before){bar->insertAction(before,overview_);if(auto* button=qobject_cast<QToolButton*>(bar->widgetForAction(overview_)))button->setProperty("print_operation","Overview");break;}
            }
            connect(overview_,&QAction::triggered,this,[this](bool checked){
                preview_->setViewMode(checked?QPrintPreviewWidget::AllPagesView:QPrintPreviewWidget::SinglePageView);
            });
        }
        connect(preview_,&QPrintPreviewWidget::previewChanged,this,[this]{update();});update();
    }
    void update(){
        if(updating_||!preview_)return;updating_=true;
        const bool multi=allowMultiPage_&&preview_->pageCount()>1;
        if(!multi&&preview_->viewMode()!=QPrintPreviewWidget::SinglePageView)preview_->setSinglePageViewMode();
        if(overview_)overview_->setChecked(preview_->viewMode()==QPrintPreviewWidget::AllPagesView);
        dialog_->setProperty("studio_print_multipage_tools",multi);
        for(auto* action:dialog_->findChildren<QAction*>())
            if(action->property("print_operation").isValid()||action->property("studio_native_overview").toBool())action->setVisible(visible(action,multi));
        for(auto* bar:dialog_->findChildren<QToolBar*>()){
            for(auto* action:bar->actions()){
                if(action->isSeparator())continue;
                auto* widget=bar->widgetForAction(action);if(!widget)continue;
                bool classified=false,show=false;
                QList<QToolButton*> buttons=widget->findChildren<QToolButton*>();
                if(auto* button=qobject_cast<QToolButton*>(widget))buttons.prepend(button);
                for(auto* button:buttons)if(auto* linked=button->defaultAction();linked&&
                    (linked->property("print_operation").isValid()||linked->property("studio_native_overview").toBool())){
                    const bool keep=visible(linked,multi);button->setVisible(keep);classified=true;show|=keep;
                }
                QList<QWidget*> controls=widget->findChildren<QWidget*>();controls.prepend(widget);
                for(auto* control:controls){const auto role=control->property("print_control").toString();
                    if(role=="zoom"||role=="page"||role=="page_count"){
                        const bool keep=role=="zoom"||multi;control->setVisible(keep);classified=true;show|=keep;
                    }
                }
                if(classified)action->setVisible(show);
            }
            QAction* pendingSeparator=nullptr;bool seen=false;
            for(auto* action:bar->actions()){
                if(action->isSeparator()){action->setVisible(false);if(seen)pendingSeparator=action;}
                else if(action->isVisible()){
                    if(pendingSeparator)pendingSeparator->setVisible(true);
                    pendingSeparator=nullptr;seen=true;
                }
            }
        }
        updating_=false;
    }
};
}
void styleStudioPrintPreview(QPrintPreviewDialog* dialog,bool allowMultiPage,QPrinter* printer){
    if(!dialog||dialog->property("studio_print_preview").toBool())return;
    dialog->setProperty("studio_print_preview",true);
    dialog->setProperty("studio_print_allow_multipage",allowMultiPage);
    setStudioWindowIcon(dialog,"Print");dialog->setWindowTitle(uiText("打印预览"));dialog->resize(1180,820);
    const QList<QPair<const char*,const char*>> roles{
        {"Fit width","FitWidth"},{"Fit page","FitPage"},{"Zoom in","ZoomIn"},{"Zoom out","ZoomOut"},
        {"Portrait","Portrait"},{"Landscape","Landscape"},{"First page","First"},{"Previous page","Previous"},
        {"Next page","Next"},{"Last page","Last"},{"Show single page","Single"},{"Show facing pages","Facing"},
        {"Show overview of all pages","Overview"},{"Page setup","Setup"},{"Print","Print"}};
    for(auto* action:dialog->findChildren<QAction*>())for(const auto& role:roles){
        const auto caption=QCoreApplication::translate("QPrintPreviewDialog",role.first);
        if(action->text()!=caption&&action->toolTip()!=caption)continue;
        // Qt retains its internal connections for the hidden controls. Only
        // controls the Studio toolbar can show need a first-party icon.
        const auto iconKey="PrintPreview"+QString::fromLatin1(role.second);
        const QSet<QString> iconRoles{"FitWidth","FitPage","ZoomIn","ZoomOut","Previous","Next","Overview","Setup","Print"};
        if(iconRoles.contains(QString::fromLatin1(role.second)))action->setIcon(studioIcon(iconKey));
        action->setProperty("print_operation",role.second);
        const auto operation=QLatin1String(role.second);
        if(printer&&(operation=="Setup"||operation=="Print")){
            if(auto* view=dialog->findChild<QPrintPreviewWidget*>()){
                QObject::disconnect(action,SIGNAL(triggered(bool)),nullptr,nullptr);
                QObject::connect(action,&QAction::triggered,dialog,[dialog,view,printer,setup=operation=="Setup"]{
                    if(setup)previewPaper(dialog,view,*printer);
                    else previewPrint(dialog,view,*printer);
                });
            }
        }
        break;
    }
    for(auto* bar:dialog->findChildren<QToolBar*>()){
        bar->setObjectName("StudioPrintToolbar");bar->setIconSize({22,22});bar->setMovable(false);bar->setFloatable(false);
        bar->setContentsMargins(10,6,10,6);bar->setMinimumHeight(52);
        bar->setStyleSheet(
            "QToolBar#StudioPrintToolbar{background:#f6f9f7;border:0;border-bottom:1px solid #d8e4dd;spacing:4px;padding:6px 10px;}"
            "QToolBar#StudioPrintToolbar QToolButton{background:transparent;border:1px solid transparent;border-radius:6px;min-width:32px;min-height:32px;padding:1px;color:#244b43;}"
            "QToolBar#StudioPrintToolbar QToolButton:hover{background:#edf3ef;border-color:#cfdfd5;}"
            "QToolBar#StudioPrintToolbar QToolButton:checked{background:#e1eee7;border-color:#9ebfaf;}"
            "QToolBar#StudioPrintToolbar QToolButton:pressed{background:#d3e6db;border-color:#80a894;}"
            "QToolBar#StudioPrintToolbar QToolButton:disabled{background:transparent;border-color:transparent;}"
            "QToolBar#StudioPrintToolbar QComboBox,QToolBar#StudioPrintToolbar QLineEdit{background:#ffffff;color:#244b43;border:1px solid #cbdcd2;border-radius:5px;min-height:28px;padding:1px 7px;selection-background-color:#d3e6db;selection-color:#173d33;}"
            "QToolBar#StudioPrintToolbar::separator{background:#d8e4dd;width:1px;margin:7px 9px;}"
        );
        for(auto* combo:bar->findChildren<QComboBox*>()){
            combo->setMinimumWidth(105);combo->setAccessibleName(uiText("预览缩放比例"));combo->setToolTip(uiText("预览缩放比例"));combo->setProperty("print_control","zoom");
        }
        for(auto* edit:bar->findChildren<QLineEdit*>())if(!qobject_cast<QComboBox*>(edit->parentWidget())){
            edit->setAccessibleName(uiText("跳转到页码"));edit->setToolTip(uiText("跳转到页码"));edit->setProperty("print_control","page");
        }
        for(auto* label:bar->findChildren<QLabel*>())label->setProperty("print_control","page_count");
        for(auto* button:bar->findChildren<QToolButton*>())if(auto* action=button->defaultAction()){
            if(action->property("print_operation").isValid())button->setProperty("print_operation",action->property("print_operation"));
        }
    }
    for(auto* canvas:dialog->findChildren<QGraphicsView*>()){
        canvas->setBackgroundBrush(QColor("#e4ebe7"));
        canvas->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);canvas->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        canvas->setDragMode(QGraphicsView::ScrollHandDrag);canvas->setProperty("studio_canvas_kind","print-preview");
    }
    new PreviewControls(dialog,allowMultiPage);
}
