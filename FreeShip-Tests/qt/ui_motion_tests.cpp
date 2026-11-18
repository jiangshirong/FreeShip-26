#include "app/studio_ui.h"
#include "app/studio_motion.h"
#include <QAbstractAnimation>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {
void wait(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
QWidget* ghost(){for(auto* top:QApplication::topLevelWidgets())for(auto* widget:top->findChildren<QWidget*>("StudioMenuExitPresentation"))if(widget->isVisible())return widget;return nullptr;}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);const bool normalMotion=studio_motion::enabled();app.setProperty("studio_motion_probe",true);
    const QString out=argc>1?QString::fromLocal8Bit(argv[argc-1]):"motion-output";QDir().mkpath(out);
    applyStudioTheme();QJsonArray cases;QJsonObject observed;
    auto check=[&](QString id,bool pass,QJsonObject values=QJsonObject{}){cases.append(QJsonObject{{"id",id},{"status",pass?"pass":"fail"},{"observed",values}});};
    check("complete-runtime-theme-present",QFile::exists(QCoreApplication::applicationDirPath()+"/bridge/Themes/StudioLight/icons/manifest.json")&&!studioIcon("Menu").isNull());
    QMainWindow window;window.setWindowTitle("Isolated motion probe");window.resize(860,620);
    auto* central=new QWidget(&window);auto* layout=new QVBoxLayout(central);auto* bar=new StudioBar(central);
    auto* menu=new QMenu(&window);auto* checkAction=menu->addAction(studioIcon("ControlNet"),"状态");checkAction->setCheckable(true);checkAction->setChecked(true);
    auto* command=menu->addAction(studioIcon("Point"),"直接操作");
    auto* submenu=menu->addMenu("子菜单");submenu->addAction("子项");
    auto* menuAction=new QAction(studioIcon("Menu"),"菜单",bar);menuAction->setMenu(menu);
    auto* toggle=new QAction(studioIcon("ControlNet"),"控制网格",bar);toggle->setCheckable(true);
    bar->addGroup({menuAction,toggle},QColor("#edf5f8"));layout->addWidget(bar);
    auto* tabs=new QTabWidget(central);
    for(int i=0;i<3;++i){auto* page=new QWidget;auto* form=new QFormLayout(page);
        for(int row=0;row<4;++row)form->addRow(QString("参数 %1").arg(row+1),new QLineEdit(QString("页面 %1 · %2").arg(i+1).arg(row+1),page));
        tabs->addTab(page,QString("标签 %1").arg(i+1));}
    auto* data=new QTableWidget(20,5,tabs);tabs->addTab(data,"数据");layout->addWidget(tabs);window.setCentralWidget(central);
    window.show();wait(100);
    check("tabs.shared-flat-style",qApp->styleSheet().contains("QTabWidget::pane {border:0")&&tabs->property("studio_tab_navigation")=="flat");
    tabs->tabBar()->grab().save(out+"/tab-navigation.png");
    QToolButton* button=nullptr;for(auto* candidate:bar->findChildren<QToolButton*>())if(candidate->defaultAction()==toggle)button=candidate;
    check("approved-durations",studio_motion::MenuMs==140&&studio_motion::StateMs==120&&studio_motion::TabMs==180);
    check("motion-button-discovered-by-action",button&&button->property("studio_motion_button").toBool());
    if(button){
        const auto background=[](QToolButton* b){const auto image=b->grab().toImage();return image.pixelColor(qRound(5*image.devicePixelRatio()),qRound(5*image.devicePixelRatio()));};
        const auto initial=background(button);
        button->click();wait(36);const double normalProgress=button->property("studio_motion_checked").toDouble();const auto during=background(button);
        check("state.normal-launch-policy",normalMotion?(normalProgress>0&&normalProgress<1&&initial!=during&&(during.alpha()<255||during.rgb()!=QColor("#cce8ff").rgb())):normalProgress==1,{{"enabled",normalMotion},{"progress",normalProgress},{"before_pixel",initial.name(QColor::HexArgb)},{"intermediate_pixel",during.name(QColor::HexArgb)}});
        button->grab().save(out+"/normal-launch-state.png");
        button->click();wait(170);app.setProperty("studio_motion_enabled",true);
        int calls=0;QObject::connect(toggle,&QAction::triggered,&window,[&]{++calls;});
        button->click();check("state.semantic-immediate",toggle->isChecked()&&button->isChecked()&&calls==1);
        wait(36);const double p=button->property("studio_motion_checked").toDouble();
        check("state.visible-intermediate",p>0&&p<1,{{"progress",p}});button->grab().save(out+"/state-mid.png");
        button->click();const double reversed=button->property("studio_motion_checked").toDouble();
        check("state.reverse-does-not-reset",!toggle->isChecked()&&calls==2&&std::abs(p-reversed)<.0001,{{"before",p},{"after",reversed}});
        wait(170);check("state.finishes-at-target",button->property("studio_motion_checked").toDouble()==0);
    }
    menu->popup(bar->mapToGlobal(QPoint(0,bar->height())));wait(35);
    const auto menuProgress=menu->property("studio_motion_progress").toDouble();
    check("menu.registered",menu->property("studio_motion_menu").toBool()&&submenu->property("studio_motion_menu").toBool());
    check("menu.reveal-intermediate",menuProgress>0&&menuProgress<1,{{"progress",menuProgress},{"opacity",menu->windowOpacity()}});
    check("menu.entrance-has-full-native-input-region",menu->mask().isEmpty()&&menu->windowOpacity()==1&&menu->property("studio_motion_surface")=="paint-only");
    const auto intermediateMenu=menu->grab().toImage();
    menu->grab().save(out+"/menu-mid.png");wait(150);
    check("menu.complete-is-unmasked",menu->mask().isEmpty()&&menu->windowOpacity()==1);
    check("menu.entrance-changes-ink-without-changing-native-region",intermediateMenu!=menu->grab().toImage());
    int toggles=0;QObject::connect(checkAction,&QAction::triggered,&window,[&]{++toggles;});
    const auto pos=menu->actionGeometry(checkAction).center();
    for(auto type:{QEvent::MouseButtonPress,QEvent::MouseButtonRelease}){QMouseEvent event(type,QPointF(pos),QPointF(menu->mapToGlobal(pos)),Qt::LeftButton,type==QEvent::MouseButtonPress?Qt::LeftButton:Qt::NoButton,Qt::ControlModifier);QApplication::sendEvent(menu,&event);}
    check("menu.ctrl-toggle-applies-once",!checkAction->isChecked()&&toggles==1&&menu->isVisible());
    wait(30);const auto mark=studio_motion::checkProgress(menu,checkAction,0);
    check("menu.checkmark-intermediate",mark>0&&mark<1,{{"progress",mark}});wait(140);
    const auto originalPosition=menu->pos();const auto originalSize=menu->size();menu->close();
    check("menu.dismissal-is-immediate",!menu->isVisible());
    auto* exit=ghost();check("menu.exit-is-passive-child-not-another-native-popup",exit&&exit->testAttribute(Qt::WA_TransparentForMouseEvents)&&!exit->isWindow()&&exit->focusPolicy()==Qt::NoFocus);
    if(exit){
        check("menu.exit-keeps-native-dpr",exit->devicePixelRatioF()==menu->devicePixelRatioF());
        const auto pixels=exit->grab().toImage().convertToFormat(QImage::Format_ARGB32);
        int visible=0;for(int y=0;y<pixels.height();y+=2)for(int x=0;x<pixels.width();x+=2){const auto c=pixels.pixel(x,y);if(qAlpha(c)>20&&(qRed(c)>20||qGreen(c)>20||qBlue(c)>20))++visible;}
        const auto source=exit->property("studio_motion_source_pixels").value<QPixmap>().toImage().convertToFormat(QImage::Format_ARGB32);
        int sourceVisible=0;for(int y=0;y<source.height();y+=2)for(int x=0;x<source.width();x+=2)if(qAlpha(source.pixel(x,y))>20)++sourceVisible;
        check("menu.exit-has-rendered-content",visible>100,{{"visible_samples",visible},{"source_samples",sourceVisible},{"progress",exit->property("studio_motion_progress").toDouble()}});pixels.save(out+"/menu-exit.png");
    }
    wait(35);menu->popup(originalPosition);wait(1);
    check("menu.reopen-continues-exit",menu->isVisible()&&menu->property("studio_motion_progress").toDouble()>0);
    wait(175);check("menu.anchor-and-size-stay-fixed",menu->pos()==originalPosition&&menu->size()==originalSize);
    menu->close();wait(180);check("menu.exit-retires",ghost()==nullptr);
    window.grab().save(out+"/before-tabs.png");const auto geometry=tabs->geometry();
    tabs->setCurrentIndex(1);check("tabs.semantic-immediate",tabs->currentIndex()==1);
    wait(22);auto* overlay=tabs->findChild<QWidget*>("StudioTabContentMotion");
    check("tabs.native-dpr-overlay-visible",overlay&&overlay->isVisible()&&tabs->property("studio_motion_content").toString()=="native-dpr");
    if(overlay){
        overlay->grab().save(out+"/tabs-mid.png");const auto before=overlay->grab().toImage();tabs->setCurrentIndex(2);
        const auto after=overlay->grab().toImage();check("tabs.reverse-keeps-current-pixels",before==after);
        check("tabs.overlay-is-input-transparent",overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
    }
    wait(210);check("tabs.finished-overlay-hidden",overlay&&!overlay->isVisible());
    check("tabs.no-layout-movement",tabs->geometry()==geometry);
    window.grab().save(out+"/after-tabs.png");
    tabs->setCurrentIndex(0);wait(25);auto* edit=tabs->currentWidget()->findChild<QLineEdit*>();
    if(edit){edit->setFocus();QKeyEvent key(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier,"a");QApplication::sendEvent(edit,&key);check("tabs.input-restores-live-page",overlay&&!overlay->isVisible()&&edit->text().endsWith('a'));}
    tabs->setCurrentIndex(3);check("tabs.data-view-keeps-live-content",tabs->property("studio_motion_content").toString()=="live-surface"&&(!overlay||!overlay->isVisible()));
    wait(210);
    app.setProperty("studio_motion_enabled",false);
    if(button){button->click();check("reduced-motion.state-immediate",button->isChecked()&&button->property("studio_motion_checked").toDouble()==1);}
    tabs->setCurrentIndex(1);check("reduced-motion.no-content-presentation",!overlay||!overlay->isVisible());
    menu->popup(originalPosition);wait(1);check("reduced-motion.menu-is-complete",menu->windowOpacity()==1&&menu->mask().isEmpty());menu->hide();wait(1);check("reduced-motion.no-exit-presentation",ghost()==nullptr);
    app.setProperty("studio_motion_enabled",true);tabs->setCurrentIndex(0);wait(210);
    int running=0;for(auto* animation:window.findChildren<QAbstractAnimation*>())if(animation->state()==QAbstractAnimation::Running)++running;
    check("idle-no-animation-driver-work",running==0,{{"running",running}});
    QVector<double> raster;
    for(int i=0;i<36;++i){if(button)button->click();QElapsedTimer elapsed;elapsed.start();window.grab();raster.append(elapsed.nsecsElapsed()/1e6);wait(8);}
    std::sort(raster.begin(),raster.end());observed["hidden_raster_ms_p50"]=raster[raster.size()/2];observed["hidden_raster_ms_p95"]=raster[int(raster.size()*.95)];observed["dpr"]=window.devicePixelRatioF();
    observed["measurement_scope"]="Native widgets on private desktop; widget grab includes rasterization, not monitor presentation or physical input";
    wait(210);window.close();wait(30);check("closing-window-retires-presentations",ghost()==nullptr);
    int failures=0;for(const auto& value:cases)if(value.toObject()["status"]=="fail")++failures;
    QFile file(out+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(QJsonObject{{"status",failures?"fail":"pass"},{"cases",cases},{"artifacts",observed}}).toJson());
    return failures?1:0;
}
