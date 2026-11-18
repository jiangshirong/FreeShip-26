// Private visible HWND startup checks. No personal settings or external input.
#include "studio_caption.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEvent>
#include <QMainWindow>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QToolBar>
#include <QWindow>
#include <memory>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

namespace {
class CanvasPaintProbe final:public QObject {
public:
    explicit CanvasPaintProbe(QMainWindow* window):QObject(window),window_(window),
        bar_(dynamic_cast<StudioBar*>(window->findChild<QWidget*>("StudioToolbar"))){
        window_->centralWidget()->installEventFilter(this);
    }
    QJsonObject layout()const{
        if(!bar_)return {};
        const auto* surface=window_->centralWidget();
        const auto* rail=bar_->sideBar();
        int top=0,side=0;
        for(const auto* button:bar_->buttons())if(button->isVisible()){
            if(button->property("studio_vertical_toolbar_label").toBool())++side;else ++top;
        }
        return {{"rail_visible",rail->isVisible()},{"top_buttons",top},{"side_buttons",side},
                {"right_gap",window_->menuWidget()->geometry().right()-surface->geometry().right()}};
    }
    QJsonArray startupPaints;
    bool recording=true;
protected:
    bool eventFilter(QObject*,QEvent* event)override{
        if(recording&&event->type()==QEvent::Paint){
            const auto state=layout();
            if(startupPaints.isEmpty()||startupPaints.last()!=state)startupPaints.append(state);
        }
        return false;
    }
private:
    QMainWindow* window_;
    StudioBar* bar_;
};
}

void verifyStudioWindowStartup(QWidget* window,const QString& output){
    window->setProperty("studio_private_window_probe",true);
    // Use the real activating show path. WA_ShowWithoutActivating changes
    // Windows' initial ShowWindow command and would invalidate this observation.
    QDir().mkpath(output);
    auto* probe=new CanvasPaintProbe(static_cast<QMainWindow*>(window));
    auto rows=std::make_shared<QJsonArray>();
    auto capture=[window,output,rows,probe](QString stage){
        QJsonObject row{{"stage",stage},{"qt_maximized",window->isMaximized()},{"qt_width",window->width()},{"qt_height",window->height()},
            {"dpr",window->devicePixelRatioF()},{"toolbar",probe->layout()}};
#ifdef Q_OS_WIN
        const auto hwnd=reinterpret_cast<HWND>(window->winId());DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);
        row["own_process"]=pid==GetCurrentProcessId();
        if(pid!=GetCurrentProcessId())return;
        RECT frame{},client{};GetWindowRect(hwnd,&frame);GetClientRect(hwnd,&client);POINT origin{};ClientToScreen(hwnd,&origin);
        MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
        const QRect work(monitor.rcWork.left,monitor.rcWork.top,monitor.rcWork.right-monitor.rcWork.left,monitor.rcWork.bottom-monitor.rcWork.top);
        const QRect physical(origin.x,origin.y,client.right,client.bottom),native(frame.left,frame.top,frame.right-frame.left,frame.bottom-frame.top);
        auto rect=[](const QRect& r){return QJsonArray{r.x(),r.y(),r.width(),r.height()};};
        row["work"]=rect(work);row["client"]=rect(physical);row["frame"]=rect(native);row["native_maximized"]=bool(IsZoomed(hwnd));
        row["client_matches_work"]=physical==work;
        row["client_within_frame"]=native.adjusted(-1,-1,1,1).contains(physical);
        QJsonArray buttons;for(auto* button:window->findChildren<QToolButton*>()){
            if(!button->property("studio_caption_control").isValid()||button->window()!=window)continue;
            const auto at=button->mapTo(window,QPoint());const auto ratio=window->devicePixelRatioF();
            const int left=qRound(at.x()*ratio),top=qRound(at.y()*ratio);
            const QRect actual(origin.x+left,origin.y+top,qRound((at.x()+button->width())*ratio)-left,qRound((at.y()+button->height())*ratio)-top);
            buttons.append(QJsonObject{{"kind",button->property("studio_caption_control").toString()},{"visible",button->isVisible()},
                {"rect",rect(actual)},{"within_client",physical.contains(actual)},{"within_work",work.contains(actual)}});
        }row["buttons"]=buttons;
#endif
        const auto image=output+"/"+stage+".png";window->grab().save(image);row["image"]=image;rows->append(row);
    };
    showStudioMainWindow(window);capture("first-show");
    QTimer::singleShot(0,window,[capture]{capture("first-event-turn");});
    QTimer::singleShot(250,window,[capture,probe]{probe->recording=false;capture("settled");});
    QTimer::singleShot(350,window,[window,capture]{
#ifdef Q_OS_WIN
        const auto hwnd=reinterpret_cast<HWND>(window->winId());SendMessageW(hwnd,WM_NCLBUTTONDBLCLK,HTCAPTION,0);
#else
        window->showNormal();
#endif
        QTimer::singleShot(75,window,[capture]{capture("restore");});
    });
    QTimer::singleShot(550,window,[window,capture]{window->showMaximized();capture("maximize-again");});
    QTimer::singleShot(850,window,[window,output,rows,capture,probe]{
        capture("final");QFile file(output+"/report.json");file.open(QIODevice::WriteOnly);
        file.write(QJsonDocument(QJsonObject{{"status","partial"},{"cases",QJsonArray{QJsonObject{{"id","private-startup-observed"},{"status","pass"}}}},
            {"frames",*rows},{"startup_paints",probe->startupPaints},{"language",uiLanguage()}}).toJson());
        file.close();window->close();qApp->quit();
    });
}
