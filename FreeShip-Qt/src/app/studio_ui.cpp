#include "studio_ui.h"
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QResizeEvent>
#include <QToolButton>
#include <algorithm>

void applyStudioTheme() {
    QApplication::setFont(QFont("Microsoft YaHei UI",9));
    auto palette=QApplication::palette();
    palette.setColor(QPalette::Window,QColor(246,249,247));
    palette.setColor(QPalette::WindowText,QColor(30,53,50));
    palette.setColor(QPalette::Base,QColor(250,252,250));
    palette.setColor(QPalette::AlternateBase,QColor(237,244,240));
    palette.setColor(QPalette::Button,QColor(237,244,240));
    palette.setColor(QPalette::Text,QColor(30,53,50));
    palette.setColor(QPalette::Highlight,QColor(0,126,116));
    palette.setColor(QPalette::HighlightedText,Qt::white);
    QApplication::setPalette(palette);
}
QIcon studioIcon(const QString& key) {
    const auto root=QCoreApplication::applicationDirPath()+"/bridge/Themes/StudioLight/icons/";
    QFile file(root+"manifest.json"); QString resolved=key;
    if(file.open(QIODevice::ReadOnly)) {
        const auto aliases=QJsonDocument::fromJson(file.readAll()).object()["aliases"].toObject();
        if(aliases.contains(key))resolved=aliases[key].toString();
    }
    QIcon icon;
    for(int size:{16,20,22,24,25,30,32,40,48,64,96,128}) {
        const auto path=root+QString::number(size)+"/"+resolved+".png";
        if(QFile::exists(path))icon.addFile(path,QSize(size,size));
    }
    return icon;
}
StudioBar::StudioBar(QWidget* parent):QWidget(parent) {
    setObjectName("StudioToolbar");setFixedHeight(58);
    setStyleSheet("#StudioToolbar {background:#e0ece8;} "
        "QToolButton {color:#173b37;background:transparent;border:1px solid transparent;border-radius:2px;padding:2px;}"
        "QToolButton:hover {background:#e7f2fc;border-color:#abd5f5;}"
        "QToolButton:checked,QToolButton:pressed {background:#cce8ff;border-color:#8dcaff;}"
        "QToolButton:disabled {color:#88938f;} QToolButton::menu-indicator {image:none;}"
        "QMenu {background:#f6f9f7;color:#173b37;border:1px solid #c4d4cc;}"
        "QMenu::item {padding:6px 28px 6px 24px;} QMenu::item:selected {background:#d5e9e1;}");
    more_=new QToolButton(this);more_->setText("更多");more_->setIcon(studioIcon("More"));
    more_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);more_->setIconSize({20,20});
    overflow_=new QMenu(more_);more_->setMenu(overflow_);more_->setPopupMode(QToolButton::InstantPopup);
}
void StudioBar::addGroup(const QList<QAction*>& actions,QColor background,bool views) {
    Group group{new QWidget(this),{},views};
    group.panel->setAutoFillBackground(true);auto palette=group.panel->palette();
    palette.setColor(QPalette::Window,background);group.panel->setPalette(palette);
    for(auto* action:actions) {
        auto* button=new QToolButton(group.panel);button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);button->setIconSize({20,20});
        button->setAutoRaise(true);
        if(action->menu())button->setPopupMode(QToolButton::InstantPopup);
        group.buttons.append(button);
    }
    groups_.append(group);
}
void StudioBar::setAbout(QAction* action) {
    about_=new QToolButton(this);about_->setDefaultAction(action);
    about_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);about_->setIconSize({20,20});
}
void StudioBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);layoutButtons();
}
void StudioBar::setIconSize(int size){
    iconSize_=std::clamp(size,12,128);setFixedHeight(iconSize_+38);
    for(auto* button:findChildren<QToolButton*>())button->setIconSize({iconSize_,iconSize_});
    layoutButtons();
}
void StudioBar::layoutButtons(){
    overflow_->clear();
    const int gap=width()<1000?16:40;
    const int aboutWidth=std::max(70,iconSize_+20),moreWidth=std::max(58,iconSize_+20);
    int x=8,available=width()-(about_?aboutWidth+8:8),reserve=0;
    const int buttonWidth=std::max(60,iconSize_+20);
    for(const auto& group:groups_)if(group.views)reserve+=int(group.buttons.size())*buttonWidth+gap;
    bool overflow=false;
    for(auto& group:groups_) {
        int natural=0;
        for(auto* button:group.buttons)natural+=std::max(60,fontMetrics().horizontalAdvance(button->text())+16);
        const int limit=group.views?available:available-reserve-moreWidth-10;
        int used=0;
        for(auto* button:group.buttons) {
            const int w=std::max(buttonWidth,fontMetrics().horizontalAdvance(button->text())+16);
            const bool fits=x+used+w<=limit;
            button->setVisible(fits);
            if(fits){button->setGeometry(used,2,w-2,iconSize_+28);used+=w;}
            else {overflow_->addAction(button->defaultAction());overflow=true;}
        }
        group.panel->setGeometry(x,3,used,iconSize_+32);group.panel->setVisible(used>0);
        if(used)x+=used+gap;
    }
    more_->setVisible(overflow);more_->setGeometry(std::min(x,available-moreWidth-2),5,moreWidth,iconSize_+28);
    if(about_)about_->setGeometry(width()-aboutWidth-8,5,aboutWidth,iconSize_+28);
}
