// SPDX-License-Identifier: GPL-2.0-or-later
// Only the unified runner's private, non-input desktop. Never move the cursor.
#include "app/studio_ui.h"
#include "app/studio_motion.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

namespace {
void wait(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
class PaintCounter final:public QObject{
public:
    QMenu* menu=nullptr;int paints=0,shows=0,hides=0;
    bool eventFilter(QObject* object,QEvent* event)override{
        if(object==menu){
            if(event->type()==QEvent::Paint)++paints;
            if(event->type()==QEvent::Show)++shows;
            if(event->type()==QEvent::Hide)++hides;
        }
        return false;
    }
};
QJsonObject surface(QMenu* menu){
    QJsonObject result{{"visible",menu->isVisible()},{"opacity",menu->windowOpacity()},
        {"mask_empty",menu->mask().isEmpty()},{"width",menu->width()},{"height",menu->height()},
        {"progress",menu->property("studio_motion_progress").toDouble()}};
#ifdef Q_OS_WIN
    const auto hwnd=reinterpret_cast<HWND>(menu->winId());
    HRGN region=CreateRectRgn(0,0,0,0);const int kind=GetWindowRgn(hwnd,region);RECT bounds{};
    GetRgnBox(region,&bounds);DeleteObject(region);
    RECT native{};GetWindowRect(hwnd,&native);
    result["native_visible"]=bool(IsWindowVisible(hwnd));result["native_region_kind"]=kind;
    result["native_region_width"]=int(bounds.right-bounds.left);result["native_region_height"]=int(bounds.bottom-bounds.top);
    result["native_width"]=int(native.right-native.left);result["native_height"]=int(native.bottom-native.top);
    // ERROR here means no explicit window region: the full rectangle receives input.
    result["native_region_full"]=kind==ERROR||(bounds.right-bounds.left>=native.right-native.left&&bounds.bottom-bounds.top>=native.bottom-native.top);
#endif
    return result;
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setProperty("studio_motion_enabled",true);applyStudioTheme();
    const QString out=QString::fromLocal8Bit(argv[argc-1]);QDir().mkpath(out);QJsonArray cases;
    const auto check=[&](QString id,bool ok,QJsonObject observed=QJsonObject{}){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"},{"observed",observed}});};
    QMainWindow window;window.resize(900,620);auto* central=new QWidget;auto* layout=new QVBoxLayout(central);
    auto* bar=new StudioBar;layout->addWidget(bar);layout->addStretch();window.setCentralWidget(central);
    auto* menu=new QMenu(&window);menu->addAction("Open window...")->setProperty("studio_opens_dialog",true);
    auto* toggle=menu->addAction("Visible geometry");toggle->setCheckable(true);toggle->setChecked(true);
    menu->addAction("Apply operation");auto* nested=menu->addMenu("More operations");nested->addAction("Nested operation");
    auto* action=new QAction("Menu",bar);action->setMenu(menu);bar->addGroup({action},QColor("#edf5f8"));
    window.show();wait(150);QToolButton* button=nullptr;
    for(auto* candidate:bar->findChildren<QToolButton*>())if(candidate->defaultAction()==action)button=candidate;
    check("popup.button-discovered-by-action",button!=nullptr);
    PaintCounter counter;counter.menu=menu;menu->installEventFilter(&counter);
    const auto anchor=button->mapToGlobal(QPoint(0,button->height()));
    menu->popup(anchor);QJsonArray entrance;
    for(int delay:{0,16,32,120,330}){if(delay)wait(delay);entrance.append(surface(menu));}
    bool visible=true,full=true;for(const auto& value:entrance){const auto item=value.toObject();
        visible&=item["visible"].toBool()&&item["native_visible"].toBool();
        full&=item["mask_empty"].toBool()&&item["opacity"].toDouble()==1&&item["native_region_full"].toBool();}
    check("popup.visible-through-entire-entrance",visible,{{"frames",entrance}});
    check("popup.entrance-never-shrinks-native-input-region",full,{{"frames",entrance}});
    const int before=counter.paints;wait(220);const int idle=counter.paints-before;
    check("popup.idle-does-not-repaint-or-grab-continuously",idle<=3,{{"paints_in_220ms",idle}});
    menu->grab().save(out+"/menu-full-native-region.png");menu->close();wait(200);
    // A real QToolButton press enters its own popup dispatch. Keep it alive
    // long enough to observe the result, instead of auto-closing on aboutToShow.
    if(button){
        QJsonArray clickFrames;QElapsedTimer elapsed;elapsed.start();
        QTimer sample;sample.setInterval(35);
        QObject::connect(&sample,&QTimer::timeout,&window,[&]{
            if(menu->isVisible()){auto frame=surface(menu);frame["elapsed_ms"]=double(elapsed.elapsed());clickFrames.append(frame);}
        });
        sample.start();QTimer::singleShot(600,menu,&QMenu::close);
        const QPointF local=button->rect().center(),global=button->mapToGlobal(local.toPoint());
        QMouseEvent press(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(button,&press);wait(650);
        QMouseEvent release(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(button,&release);sample.stop();
        bool early=false,late=false,allFull=true;
        for(const auto& value:clickFrames){const auto frame=value.toObject();
            early|=frame["elapsed_ms"].toDouble()<180&&frame["native_visible"].toBool();
            late|=frame["elapsed_ms"].toDouble()>400&&frame["native_visible"].toBool();
            allFull&=frame["native_region_full"].toBool()&&frame["mask_empty"].toBool();}
        check("popup.actual-button-click-stays-visible",early&&late,{{"frames",clickFrames}});
        check("popup.clicked-menu-always-has-full-input-region",allFull&&!clickFrames.isEmpty(),{{"frames",clickFrames}});
    }
    int failures=0;for(const auto& item:cases)failures+=item.toObject()["status"]=="fail";
    QFile file(out+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(QJsonObject{{"cases",cases},{"status",failures?"fail":"pass"}}).toJson());
    return failures?1:0;
}
