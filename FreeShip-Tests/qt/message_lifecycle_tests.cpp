// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/studio_ui.h"
#include "app/calculation_input_status.h"
#include "app/tutorial_dialog.h"
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QEvent>
#include <QEnterEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QToolTip>
#include <QTextDocument>

class Shows final:public QObject {
public:
 QJsonArray unexpected,tutorials;
 bool eventFilter(QObject* object,QEvent* event)override{
  if(event->type()==QEvent::Show)if(auto* w=qobject_cast<QWidget*>(object);w&&w->isWindow()){
   if(dynamic_cast<StudioMessage*>(w))unexpected.append(QJsonObject{{"text",qobject_cast<QLabel*>(w)->text()},{"width",w->width()},{"height",w->height()}});
   if(w->objectName()=="TutorialGuide"){
    tutorials.append(QJsonObject{{"width",w->width()},{"height",w->height()},{"title",w->windowTitle()}});
    QTimer::singleShot(0,w,[w]{qobject_cast<QDialog*>(w)->accept();});
   }
  }
  return false;
 }
};
int main(int argc,char** argv){
 QApplication app(argc,argv);app.setProperty("studio_motion_enabled",false);applyStudioTheme();app.setQuitOnLastWindowClosed(false);
 const QString out=QString::fromLocal8Bit(argv[argc-1]);QDir().mkpath(out);QJsonArray cases;
 const auto check=[&](QString id,bool ok){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});};
 Shows shows;app.installEventFilter(&shows);
 for(auto role:{StudioMessageRole::Explanation,StudioMessageRole::Feedback,StudioMessageRole::Warning}){
  QDialog owner;auto* layout=new QVBoxLayout(&owner);auto* message=new StudioMessage(role,"Lifecycle test");
  check("unowned-message-never-visible-"+QString::number(int(role)),!message->isVisible());
  message->setVisible(true);check("unowned-conditional-show-deferred-"+QString::number(int(role)),!message->isVisible());
  layout->addWidget(message);owner.show();app.processEvents();
  check("message-text-reserves-icon-lane-"+QString::number(int(role)),message->contentsRect().left()>=36);
  owner.resize(640,180);app.processEvents();owner.grab().save(out+"/message-"+QString::number(int(role))+".png");
  check("adopted-message-visible-"+QString::number(int(role)),message->isVisible());
  message->clear();check("empty-message-hidden-"+QString::number(int(role)),!message->isVisible());
  message->setText("Updated");check("updated-child-visible-"+QString::number(int(role)),message->isVisible());owner.hide();
 }
 {
  QDialog owner;auto* layout=new QVBoxLayout(&owner);auto* message=new StudioMessage(StudioMessageRole::Feedback);
  message->setText("Updated before layout");check("pre-adoption-update-does-not-open-window",!message->isVisible());
  layout->addWidget(message);owner.show();app.processEvents();check("pre-adoption-update-shown-with-owner",message->isVisible());
 }
 {
  QDialog owner;auto* layout=new QVBoxLayout(&owner);auto* message=new StudioMessage(StudioMessageRole::Warning);
  layout->addWidget(message);message->enableMathNotation();
  const QString error=QStringLiteral("Error: Cp=1 or Cwp=1\nCheck the inputs.");
  message->setMessage(error,StudioMessageRole::Warning);
  owner.resize(640,180);owner.show();app.processEvents();
  check("calculation-warning-renders-both-symbols",message->textFormat()==Qt::RichText&&
      message->text().count("<!--studio-math-->")==2&&message->text().contains("<br>")&&message->accessibleName()==error);
  owner.grab().save(out+"/calculation-warning-math.png");
  message->setMessage("Check the inputs.",StudioMessageRole::Warning);
  check("subsequent-plain-message-remains-plain",message->textFormat()==Qt::PlainText&&message->text()=="Check the inputs.");
 }
 {
  QDialog owner;owner.setAttribute(Qt::WA_AlwaysShowToolTips);
  auto* layout=new QVBoxLayout(&owner);auto* field=new QDoubleSpinBox(&owner);
  layout->addWidget(calculationInput::caption(new QLabel("Propeller diameter"),field,"diameter"));
  layout->addWidget(field);
  calculationInput::update({{"diameter",field}},{{"diameter","Dp/Ta\nAllowed: 0.43–0.84."}});
  owner.show();app.processEvents();
  auto* marker=qobject_cast<QLabel*>(field->property("calculation_input_marker").value<QObject*>());
  const auto point=marker->rect().center();
  QEnterEvent hover(point,marker->mapTo(&owner,point),marker->mapToGlobal(point));
  QApplication::sendEvent(marker,&hover);
  check("calculation-tooltip-visible-before-delayed-help-event",QToolTip::isVisible());
  check("calculation-tooltip-rich-text-detected-by-qt",Qt::mightBeRichText(QToolTip::text())&&
      QToolTip::text().startsWith("<p>")&&QToolTip::text().contains("<!--studio-math-->")&&
      QToolTip::text().contains("0.43–0.84"));
  QLabel* popup=nullptr;
  for(auto* win:QApplication::topLevelWidgets())
   if(win->windowType()==Qt::ToolTip&&win->isVisible())popup=qobject_cast<QLabel*>(win);
  check("hovered-calculation-tooltip-renders-rich-math",popup&&Qt::mightBeRichText(popup->text()));
  if(popup)popup->grab().save(out+"/calculation-tooltip-math.png");
  calculationInput::update({{"diameter",field}},{});
  check("visible-calculation-tooltip-updates-with-field-status",
      QToolTip::isVisible()&&QToolTip::text()==marker->toolTip()&&!QToolTip::text().contains("0.43–0.84"));
  QEvent leave(QEvent::Leave);
  QApplication::sendEvent(marker,&leave);
 }
 QWidget owner;owner.resize(1200,800);owner.show();
 showTutorialGuide(&owner);
 check("real-tutorial-shown-once-at-final-size",shows.tutorials.size()==1&&shows.tutorials[0].toObject()["width"].toInt()>=660&&shows.tutorials[0].toObject()["height"].toInt()>=500);
 check("no-message-shown-as-independent-window",shows.unexpected.isEmpty());
 int failures=0;for(const auto& c:cases)failures+=c.toObject()["status"]=="fail";
 QFile file(out+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(QJsonObject{{"status",failures?"fail":"pass"},{"cases",cases},{"artifacts",QJsonObject{{"unexpected_windows",shows.unexpected},{"tutorial_first_shows",shows.tutorials}}}}).toJson());
 return failures?1:0;
}
