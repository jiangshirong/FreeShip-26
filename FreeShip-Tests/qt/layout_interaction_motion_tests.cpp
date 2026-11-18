// SPDX-License-Identifier: GPL-2.0-or-later
// Unified runner's isolated desktop only. No global cursor or system input.
#include "app/layout_motion.h"
#include "app/studio_motion.h"
#include "app/studio_ui.h"
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTimer>
#include <QVBoxLayout>

namespace {
void wait(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
QWidget* exitPresentation(){for(auto* widget:QApplication::topLevelWidgets())if(widget->objectName()=="StudioFeatureWindowExitPresentation"&&widget->isVisible())return widget;return nullptr;}
}
int main(int argc,char** argv){
 QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);app.setProperty("studio_motion_enabled",true);app.setProperty("studio_motion_probe",true);applyStudioTheme();
 const QString output=QString::fromLocal8Bit(argv[argc-1]);QDir().mkpath(output);QJsonArray cases;
 const auto check=[&](QString id,bool ok,QJsonObject values=QJsonObject{}){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"},{"observed",values}});};
 {
  QDialog dialog;dialog.resize(800,560);dialog.setWindowTitle("Feature motion probe");auto* layout=new QVBoxLayout(&dialog);auto* edit=new QLineEdit("Original");layout->addWidget(new QLabel("Complete native-resolution content"));layout->addWidget(edit);
  layout_motion::installFeatureWindow(&dialog);dialog.show();const QSize first=dialog.size();wait(35);
  for(int attempt=0;attempt<5&&dialog.property("studio_window_motion_phase").toDouble()==0;++attempt)wait(10);
  const auto phase=dialog.property("studio_window_motion_phase").toDouble();
  check("feature.first-frame-final-size",first==dialog.size()&&first.width()>=800&&first.height()>=560);
  check("feature.entrance-intermediate",dialog.property("studio_window_motion_active").toBool()&&phase>0&&phase<1,{{"phase",phase}});
  dialog.grab().save(output+"/feature-mid.png");
  edit->setFocus();QKeyEvent key(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier,"a");QApplication::sendEvent(edit,&key);
  check("feature.input-immediately-restores-live-content",!dialog.property("studio_window_motion_active").toBool()&&edit->text().contains('a'));
  dialog.hide();wait(20);auto* ghost=exitPresentation();
  check("feature.exit-is-passive-no-caption",ghost&&ghost->testAttribute(Qt::WA_TransparentForMouseEvents)&&ghost->windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus)&&!ghost->windowFlags().testFlag(Qt::WindowTitleHint));
  check("feature.semantic-hide-immediate",!dialog.isVisible());
  dialog.show();wait(20);check("feature.reopen-dismisses-old-exit",!exitPresentation());wait(190);
  check("feature.entrance-settles",!dialog.property("studio_window_motion_active").toBool()&&dialog.property("studio_window_motion_phase").toDouble()==1);
  app.setProperty("studio_motion_enabled",false);dialog.hide();dialog.show();wait(10);check("feature.reduced-motion-final-state",!dialog.property("studio_window_motion_active").toBool());dialog.hide();app.setProperty("studio_motion_enabled",true);
 }
 {
  QWidget container;container.resize(820,620);auto* grid=new QGridLayout(&container);grid->setContentsMargins(0,0,0,0);grid->setSpacing(2);
  QList<QWidget*> frames;
  for(int i=0;i<4;++i){auto* frame=new QFrame;frame->setMinimumSize(80,80);frame->setProperty("camera_identity",i+1);frame->setStyleSheet("background:#e8efeb;border:1px solid #8aafa3");frames<<frame;grid->addWidget(frame,i/2,i%2);}
  container.show();wait(30);const auto initial=frames[0]->geometry();
  const auto target=[&](bool all){
   for(auto* frame:frames)grid->removeWidget(frame);
   for(int i=0;i<4;++i){frames[i]->setVisible(all||i==0);if(all||i==0)grid->addWidget(frames[i],all?i/2:0,all?i%2:0);}
   for(int i=0;i<2;++i){grid->setRowStretch(i,all||i==0?1:0);grid->setColumnStretch(i,all||i==0?1:0);}
  };
  layout_motion::animateViewLayout(&container,frames,[&]{target(false);});
  check("views.visibility-target-immediate",frames[0]->isVisible()&&frames[1]->isHidden()&&frames[2]->isHidden()&&frames[3]->isHidden());
  wait(45);const auto middle=frames[0]->geometry();
  check("views.native-layout-intermediate",container.property("studio_view_layout_active").toBool()&&middle.width()>initial.width()&&middle.width()<container.width(),{{"initial_width",initial.width()},{"middle_width",middle.width()},{"target_width",container.width()}});
  container.grab().save(output+"/views-mid.png");
  layout_motion::animateViewLayout(&container,frames,[&]{target(true);});wait(260);
  check("views.reversal-settles-layout",!container.property("studio_view_layout_active").toBool()&&grid->isEnabled()&&frames[0]->geometry()==initial);
  bool identities=true;for(int i=0;i<4;++i)identities&=frames[i]->isVisible()&&frames[i]->property("camera_identity").toInt()==i+1;
  check("views.original-widgets-and-camera-identity-retained",identities);
  const auto fills=[&]{QRect used;for(auto* frame:frames)if(frame->isVisible())used=used.united(frame->geometry());return used==container.contentsRect();};
  container.resize(1280,860);wait(50);
  check("views.resize-after-settled-motion-fills-current-container",fills());
  container.hide();wait(15);container.show();wait(50);
  check("views.hide-show-after-resize-keeps-current-layout",fills());
  layout_motion::animateViewLayout(&container,frames,[&]{target(false);});wait(40);
  container.resize(1040,760);wait(260);
  check("views.resize-interrupts-animation-with-current-container-size",frames[0]->geometry()==container.contentsRect()&&grid->isEnabled());
  layout_motion::animateViewLayout(&container,frames,[&]{target(true);});wait(260);
  check("views.reexpand-after-resize-fills-container",fills());
  app.setProperty("studio_motion_enabled",false);layout_motion::animateViewLayout(&container,frames,[&]{target(false);});
  check("views.reduced-motion-immediate-target",!container.property("studio_view_layout_active").toBool()&&grid->isEnabled()&&frames[0]->width()==container.width());app.setProperty("studio_motion_enabled",true);
 }
 {
  QSplitter splitter(Qt::Horizontal);splitter.addWidget(new QLabel("Left"));splitter.addWidget(new QLabel("Right"));splitter.resize(800,350);layout_motion::installSplitter(&splitter);splitter.show();wait(35);
  auto* handle=splitter.handle(1);check("splitter.hit-width-16",splitter.handleWidth()==16&&handle->width()==16);
  QEvent enter(QEvent::Enter);QApplication::sendEvent(handle,&enter);wait(30);const double phase=handle->property("studio_splitter_motion_progress").toDouble();
  check("splitter.hover-progress-intermediate",phase>0&&phase<1,{{"phase",phase}});
  const int total=splitter.sizes()[0]+splitter.sizes()[1];splitter.setSizes({total/2+4,total-total/2-4});
  const QPoint local=handle->rect().center();QMouseEvent release(QEvent::MouseButtonRelease,local,handle->mapToGlobal(local),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(handle,&release);wait(110);
  auto sizes=splitter.sizes();check("splitter.near-midpoint-snaps-after-release",std::abs(sizes[0]-sizes[1])<=1&&splitter.property("studio_splitter_snap_count").toInt()==1);
  splitter.setSizes({total/3,total-total/3});QApplication::sendEvent(handle,&release);wait(110);sizes=splitter.sizes();
  check("splitter.distant-boundary-does-not-snap",std::abs(sizes[0]-total/3)<=1&&splitter.property("studio_splitter_snap_count").toInt()==1);
  splitter.grab().save(output+"/splitter.png");
 }
 wait(140);int failures=0;for(const auto& row:cases)failures+=row.toObject()["status"]=="fail";
 QFile file(output+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(QJsonObject{{"status",failures?"fail":"pass"},{"cases",cases}}).toJson());return failures?1:0;
}
