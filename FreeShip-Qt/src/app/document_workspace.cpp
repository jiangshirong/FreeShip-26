// SPDX-License-Identifier: GPL-2.0-or-later
#include "document_workspace.h"
#include "main_window.h"
#include "studio_caption.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QCloseEvent>
#include <QFileInfo>
#include <QPainter>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTimer>

namespace {
class DocumentTabCloseButton final : public QAbstractButton {
public:
    explicit DocumentTabCloseButton(QTabBar* parent):QAbstractButton(parent){
        setFixedSize(20,20);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
        setAccessibleName(uiText("关闭"));
    }
protected:
    bool event(QEvent* event) override{
        if(event->type()==QEvent::HoverEnter||event->type()==QEvent::HoverLeave)update();
        return QAbstractButton::event(event);
    }
    void paintEvent(QPaintEvent*) override{
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const bool pressed=isDown();
        const bool hovered=underMouse();
        const QRectF bounds(rect());
        if(hovered||pressed){
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(pressed?"#c5dbd2":"#d7e7e0"));
            painter.drawRoundedRect(bounds.adjusted(1,1,-1,-1),4,4);
        }
        painter.setPen(QPen(QColor(hovered||pressed?"#245b51":"#526f66"),1.3,Qt::SolidLine,Qt::RoundCap));
        const auto center=bounds.center();
        painter.drawLine(center+QPointF(-3.5,-3.5),center+QPointF(3.5,3.5));
        painter.drawLine(center+QPointF(-3.5,3.5),center+QPointF(3.5,-3.5));
    }
};
}

DocumentWorkspace::DocumentWorkspace(bool persistent):tabs_(new QTabBar),pages_(new QStackedWidget(this)),persistent_(persistent){
    setWindowTitle("FreeShip 26");setStudioWindowIcon(this,"brand");enableStudioMinimizeCaption(this);
    tabs_->setObjectName("DocumentTabs");tabs_->setExpanding(false);tabs_->setDrawBase(false);
    tabs_->setElideMode(Qt::ElideMiddle);tabs_->setUsesScrollButtons(true);tabs_->setFocusPolicy(Qt::NoFocus);
    tabs_->setStyleSheet(QStringLiteral(
        "QTabBar#DocumentTabs::tab {background:transparent;color:#38574d;"
        "border:1px solid transparent;border-radius:6px;margin:2px 3px 2px 0;"
        "padding:2px 7px 2px 10px;min-height:20px;}"
        "QTabBar#DocumentTabs::tab:hover:!selected {background:#d7e7e0;color:#245b51;}"
        "QTabBar#DocumentTabs::tab:selected {background:#eff7f2;color:#17594e;border-color:#a8c8ba;}"));
    setMenuWidget(createStudioCaption(this,tabs_));setCentralWidget(pages_);setAcceptDrops(true);
    connect(tabs_,&QTabBar::currentChanged,this,[this](int i){if(i>=0&&i<pages_->count())pages_->setCurrentIndex(i);});
    connect(tabs_,&QTabBar::tabCloseRequested,this,[this](int i){if(i>=0&&i<pages_->count()){tabs_->setCurrentIndex(i);active()->close();}});
    for(int direction:{-1,1}){auto* shortcut=new QShortcut(direction>0?QKeySequence("Ctrl+Tab"):QKeySequence("Ctrl+Shift+Tab"),this);
        connect(shortcut,&QShortcut::activated,this,[this,direction]{const int n=tabs_->count();if(n>1)tabs_->setCurrentIndex((tabs_->currentIndex()+direction+n)%n);});}
    auto* closeTab=new QShortcut(QKeySequence("Ctrl+W"),this);connect(closeTab,&QShortcut::activated,this,[this]{if(active())active()->close();});
}
MainWindow* DocumentWorkspace::addDocument(){
    auto* doc=new MainWindow(this,persistent_);doc->setWindowFlags(Qt::Widget);doc->setAttribute(Qt::WA_DeleteOnClose);
    pages_->addWidget(doc);
    const int index=tabs_->addTab(QString{});
    const auto side=static_cast<QTabBar::ButtonPosition>(tabs_->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition,nullptr,tabs_));
    auto* close=new DocumentTabCloseButton(tabs_);
    tabs_->setTabButton(index,side,close);
    connect(close,&QAbstractButton::clicked,this,[this,doc]{
        const int i=pages_->indexOf(doc);
        if(i>=0)emit tabs_->tabCloseRequested(i);
    });
    connect(doc,&QWidget::windowTitleChanged,this,[this,doc]{refresh(doc);});refresh(doc);activate(doc);return doc;
}
void DocumentWorkspace::refresh(MainWindow* doc){
    const int i=pages_->indexOf(doc);if(i<0)return;
    auto path=doc->session_.sourcePath();if(path.isEmpty())path=doc->session_.recoveryOrigin();if(path.isEmpty())path=doc->session_.pendingFilePath();
    auto name=path.isEmpty()?QString::fromUtf8(uiText("未命名船模")):QFileInfo(path).fileName();if(doc->session_.modified())name+=" *";
    tabs_->setTabText(i,name.replace('&',"&&"));tabs_->setTabToolTip(i,QDir::toNativeSeparators(path));
}
MainWindow* DocumentWorkspace::active() const{return dynamic_cast<MainWindow*>(pages_->currentWidget());}
QList<MainWindow*> DocumentWorkspace::documents() const{QList<MainWindow*> docs;for(int i=0;i<pages_->count();++i)docs.append(static_cast<MainWindow*>(pages_->widget(i)));return docs;}
void DocumentWorkspace::activate(MainWindow* doc){const int i=pages_->indexOf(doc);if(i>=0)tabs_->setCurrentIndex(i);}
void DocumentWorkspace::accepted(MainWindow* doc){const int i=pages_->indexOf(doc);if(i<0)return;
    pages_->removeWidget(doc);tabs_->removeTab(i);if(tabs_->currentIndex()>=0)pages_->setCurrentIndex(tabs_->currentIndex());
    if(!pages_->count()||closing_)QTimer::singleShot(0,this,[this]{if(!pages_->count())close();else if(closing_)closeNext();});
}
void DocumentWorkspace::cancelled(){closing_=false;
#ifdef Q_OS_LINUX
    setProperty("fs26_restart_after_close",false);
#endif
}
void DocumentWorkspace::closeNext(){if(auto* doc=active()){
    if(!doc->close()&&doc->documentExit_==MainWindow::DocumentExit::None&&!doc->calculationExitPending_&&!doc->closeApproved_)cancelled();
}}
void DocumentWorkspace::closeEvent(QCloseEvent* event){if(!active()){event->accept();
#ifdef Q_OS_LINUX
    if(persistent_&&property("fs26_restart_after_close").toBool()){qApp->exit(75);return;}
#endif
    if(persistent_)qApp->quit();return;}
    event->ignore();if(!closing_){closing_=true;closeNext();}}
void DocumentWorkspace::dragEnterEvent(QDragEnterEvent* e){if(active())active()->dragEnterEvent(e);}
void DocumentWorkspace::dropEvent(QDropEvent* e){if(active())active()->dropEvent(e);}
