// SPDX-License-Identifier: GPL-2.0-or-later
#include "print_preview_style.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QGraphicsView>
#include <QLineEdit>
#include <QLabel>
#include <QPrintPreviewDialog>
#include <QPrintPreviewWidget>
#include <QSet>
#include <QToolBar>
#include <QToolButton>

namespace {
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
void styleStudioPrintPreview(QPrintPreviewDialog* dialog,bool allowMultiPage){
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
        action->setProperty("print_operation",role.second);break;
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
