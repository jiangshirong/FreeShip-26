// SPDX-License-Identifier: GPL-2.0-or-later
#include "studio_caption.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QWindow>
#include <QScreen>
#include <QTimer>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#include <commctrl.h>
#endif

namespace {
void toggleWindow(QWidget* window){
#ifdef Q_OS_WIN
    if(window->internalWinId()){
        const auto hwnd=reinterpret_cast<HWND>(window->internalWinId());
        ShowWindow(hwnd,IsZoomed(hwnd)?SW_RESTORE:SW_MAXIMIZE);return;
    }
#endif
    if(window->isMaximized())window->showNormal();else window->showMaximized();
}
QWidget* captionFor(QWidget* window){
    for(auto* caption:window->findChildren<QWidget*>("StudioWindowCaption"))if(caption->window()==window)return caption;
    return nullptr;
}
bool managedWindow(QWidget* window){
    return window&&window->isWindow()&&window->property("studio_window_prepared").toBool()
        &&!window->property("studio_caption_double_click_disabled").toBool();
}

class CaptionButton final : public QToolButton {
public:
    CaptionButton(bool close,QWidget* parent):QToolButton(parent),close_(close){
        setFixedSize(45,36);setFocusPolicy(Qt::NoFocus);
        setProperty("studio_caption_control",close?"close":"minimize");
        setAccessibleName(close?uiText("关闭"):uiText("最小化"));setToolTip(accessibleName());
    }
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        if(underMouse()||isDown())painter.fillRect(rect(),close_?QColor(isDown()?"#bd353b":"#d94a50"):QColor(isDown()?"#c5dbd2":"#d7e7e0"));
        painter.setPen(QPen(close_&&(underMouse()||isDown())?Qt::white:QColor("#274b45"),1.3,Qt::SolidLine,Qt::RoundCap));
        const QPointF c=QRectF(rect()).center();
        if(close_){painter.drawLine(c+QPointF(-4.5,-4.5),c+QPointF(4.5,4.5));painter.drawLine(c+QPointF(-4.5,4.5),c+QPointF(4.5,-4.5));}
        else painter.drawLine(c+QPointF(-5,0),c+QPointF(5,0));
    }
private:
    bool close_;
};

class StudioCaption final : public QWidget {
public:
    StudioCaption(QWidget* window,QWidget* tabs):QWidget(window),window_(window){
        setObjectName("StudioWindowCaption");setFixedHeight(36);setAutoFillBackground(true);
        auto colors=palette();colors.setColor(QPalette::Window,QColor("#e0ece7"));setPalette(colors);
        auto* row=new QHBoxLayout(this);row->setContentsMargins(10,0,0,0);row->setSpacing(8);
        icon_=new QLabel(this);icon_->setFixedSize(18,18);icon_->setAttribute(Qt::WA_TransparentForMouseEvents);row->addWidget(icon_);
        title_=new QLabel(this);title_->setMinimumWidth(0);title_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        title_->setObjectName("StudioCaptionTitle");title_->setProperty("ui_user_text",true);
        title_->setAttribute(Qt::WA_TransparentForMouseEvents);if(tabs){title_->hide();row->addWidget(tabs);row->addStretch(1);row->addSpacing(32);}
        else row->addWidget(title_,1);
        auto* minimize=new CaptionButton(false,this);auto* close=new CaptionButton(true,this);row->addWidget(minimize);row->addWidget(close);
        connect(minimize,&QToolButton::clicked,window,&QWidget::showMinimized);
        connect(close,&QToolButton::clicked,window,&QWidget::close);
        connect(window,&QWidget::windowTitleChanged,this,[this]{refresh();});
        connect(window,&QWidget::windowIconChanged,this,[this]{refresh();});
        refresh();
    }
protected:
    void resizeEvent(QResizeEvent* event)override{QWidget::resizeEvent(event);QTimer::singleShot(0,this,[this]{refresh();});}
    void mousePressEvent(QMouseEvent* event)override{
        if(event->button()==Qt::LeftButton&&window_->windowHandle())window_->windowHandle()->startSystemMove();
    }
    void mouseDoubleClickEvent(QMouseEvent* event)override{
        if(event->button()==Qt::LeftButton)toggleWindow(window_);
    }
private:
    void refresh(){title_->setText(title_->fontMetrics().elidedText(window_->windowTitle(),Qt::ElideMiddle,title_->width()));setToolTip(window_->windowTitle());icon_->setPixmap(window_->windowIcon().pixmap(QSize(18,18),devicePixelRatioF()));}
    QWidget* window_;QLabel* title_;QLabel* icon_;
};

class CaptionEvents final : public QObject,public QAbstractNativeEventFilter {
public:
    bool nativeEventFilter(const QByteArray& type,void* payload,qintptr* result)override{
#ifdef Q_OS_WIN
        if(type!="windows_generic_MSG")return false;
        const auto* message=static_cast<MSG*>(payload);
        // Let Qt's Windows backend observe the final NCCALCSIZE margins. Only
        // input double-clicks need the per-HWND subclass before Qt dispatch.
        return message->message!=WM_NCLBUTTONDBLCLK&&handle(message,result);
#else
        Q_UNUSED(type);Q_UNUSED(payload);Q_UNUSED(result);return false;
#endif
    }
    bool eventFilter(QObject* watched,QEvent* event)override{
#ifdef Q_OS_WIN
        auto* window=qobject_cast<QWidget*>(watched);
        if(managedWindow(window)&&(event->type()==QEvent::WinIdChange||event->type()==QEvent::Show)&&window->internalWinId()){
            const auto hwnd=reinterpret_cast<HWND>(window->internalWinId());
            SetWindowSubclass(hwnd,procedure,1,0);
        }
#else
        Q_UNUSED(watched);Q_UNUSED(event);
#endif
        return false;
    }
#ifdef Q_OS_WIN
private:
    static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam,UINT_PTR id,DWORD_PTR){
        if(message==WM_NCDESTROY){RemoveWindowSubclass(hwnd,procedure,id);return DefSubclassProc(hwnd,message,wParam,lParam);}
        MSG data{};data.hwnd=hwnd;data.message=message;data.wParam=wParam;data.lParam=lParam;qintptr result=0;
        if(message==WM_NCLBUTTONDBLCLK&&handle(&data,&result))return LRESULT(result);
        return DefSubclassProc(hwnd,message,wParam,lParam);
    }
    static bool handle(const MSG* message,qintptr* result){
        if(message->message!=WM_NCCALCSIZE&&message->message!=WM_NCHITTEST&&message->message!=WM_NCLBUTTONDBLCLK&&message->message!=WM_GETMINMAXINFO)return false;
        auto* window=QWidget::find(reinterpret_cast<WId>(message->hwnd));
        if(!window)for(auto* candidate:QApplication::topLevelWidgets())if(candidate->internalWinId()==reinterpret_cast<WId>(message->hwnd)){window=candidate;break;}
        if(!managedWindow(window))return false;
        if(message->message==WM_NCLBUTTONDBLCLK&&message->wParam==HTCAPTION){toggleWindow(window);*result=0;return true;}
        if(!window->property("studio_caption_minimize").toBool())return false;
        if(message->message==WM_GETMINMAXINFO){
            // Qt's native titled-window minimum adds a frame that this custom
            // caption removes. At high DPI that extra padding grows a centered
            // restored window downward. Our client and normal frame coincide.
            const auto minimum=window->minimumSize().expandedTo(window->minimumSizeHint());
            auto* limits=reinterpret_cast<MINMAXINFO*>(message->lParam);
            limits->ptMinTrackSize={qMax(1,qRound(minimum.width()*window->devicePixelRatioF())),qMax(1,qRound(minimum.height()*window->devicePixelRatioF()))};
            *result=0;return true;
        }
        if(message->message==WM_NCCALCSIZE){
            auto* bounds=message->wParam?&reinterpret_cast<NCCALCSIZE_PARAMS*>(message->lParam)->rgrc[0]:reinterpret_cast<RECT*>(message->lParam);
            if(IsZoomed(message->hwnd)){
                MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
                if(GetMonitorInfoW(MonitorFromWindow(message->hwnd,MONITOR_DEFAULTTONEAREST),&monitor))*bounds=monitor.rcWork;
            }
            *result=0;return true;
        }
        if(message->message==WM_NCHITTEST){
            const POINT point{short(LOWORD(message->lParam)),short(HIWORD(message->lParam))};RECT frame{};GetWindowRect(message->hwnd,&frame);
            if(!IsZoomed(message->hwnd)){
                const int edge=qMax(5,qRound(6*window->devicePixelRatioF()));
                const bool left=point.x<frame.left+edge,right=point.x>=frame.right-edge,top=point.y<frame.top+edge,bottom=point.y>=frame.bottom-edge;
                if(top||bottom||left||right){*result=top?(left?HTTOPLEFT:right?HTTOPRIGHT:HTTOP):bottom?(left?HTBOTTOMLEFT:right?HTBOTTOMRIGHT:HTBOTTOM):left?HTLEFT:HTRIGHT;return true;}
            }
            if(auto* caption=captionFor(window)){
                auto client=point;ScreenToClient(message->hwnd,&client);const auto ratio=window->devicePixelRatioF();
                const auto local=caption->mapFrom(window,QPoint(qRound(client.x/ratio),qRound(client.y/ratio)));
                if(caption->rect().contains(local)){
                    auto* child=caption->childAt(local);
                    for(auto* w=child;w&&w!=caption;w=w->parentWidget())if(w->property("studio_caption_control").isValid()||w->objectName()=="DocumentTabs"){*result=HTCLIENT;return true;}
                    *result=HTCAPTION;return true;
                }
            }
            *result=HTCLIENT;return true;
        }
        return false;
    }
#endif
};
}

void installStudioCaptionEvents(){static CaptionEvents events;auto* app=QCoreApplication::instance();app->installEventFilter(&events);app->installNativeEventFilter(&events);}
void enableStudioMinimizeCaption(QWidget* window){
    window->setProperty("studio_caption_minimize",true);
    auto flags=window->windowFlags();flags&=~Qt::FramelessWindowHint;
    flags|=Qt::CustomizeWindowHint|Qt::WindowTitleHint|Qt::WindowSystemMenuHint|Qt::WindowCloseButtonHint|Qt::WindowMinMaxButtonsHint;
    window->setWindowFlags(flags);
}
QWidget* createStudioCaption(QWidget* window,QWidget* tabs){return new StudioCaption(window,tabs);}
void showStudioMainWindow(QWidget* window){
    window->ensurePolished();
    auto* bar=dynamic_cast<StudioBar*>(window->findChild<QWidget*>("StudioToolbar"));
#ifdef Q_OS_WIN
    // During WM_CREATE QWidget::find cannot resolve this HWND yet. Recalculate
    // once while still hidden, after Qt has registered it, before maximizing.
    const auto hwnd=reinterpret_cast<HWND>(window->winId());
    SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
#endif
    if(auto* screen=window->screen()){
        const auto work=screen->availableGeometry();
        if(window->layout())window->layout()->activate();
        const QSize normal=QSize(qMin(1440,qRound(work.width()*.9)),qMin(960,qRound(work.height()*.9))).expandedTo(window->minimumSizeHint());
        window->setGeometry(QRect(work.center()-QPoint(normal.width()/2,normal.height()/2),normal));
        // Decide overflow while hidden. The normal placement above is for
        // restore; resize events for it must not drive the maximized first paint.
        if(bar)bar->setStartupLayoutWidth(work.width());
        if(window->layout())window->layout()->activate();
    }
    window->showMaximized();
    // This is the initial show before any session/load timer is scheduled.
    // Deliver Qt's native resize notifications before exposing a stale layout
    // to the first paint or starting model animations. No user input dispatched.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if(bar)bar->setStartupLayoutWidth(0);
}
