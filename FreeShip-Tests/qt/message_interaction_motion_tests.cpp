// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/studio_ui.h"
#include "app/message_motion.h"
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
void wait(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char** argv){
 QApplication app(argc,argv);app.setProperty("studio_motion_enabled",true);applyStudioTheme();
 const QString out=QString::fromLocal8Bit(argv[argc-1]);QDir().mkpath(out);QJsonArray cases;
 const auto check=[&](QString id,bool pass,QJsonObject values=QJsonObject{}){cases.append(QJsonObject{{"id",id},{"status",pass?"pass":"fail"},{"observed",values}});};
 QDialog owner;owner.resize(600,360);auto* layout=new QVBoxLayout(&owner);
 auto* message=new StudioMessage(StudioMessageRole::Feedback,{},&owner);layout->addWidget(message);auto* button=new QPushButton("Save probe");layout->addWidget(button);layout->addStretch();owner.show();wait(200);
 message->setMessage("Calculating",StudioMessageRole::Feedback,true);wait(35);
 check("message-enters-by-growing-real-layout-height",message->height()>0&&message->property("studio_message_target_visible").toBool(),{{"height",message->height()}});
 const double phase=message->property("studio_message_busy_phase").toDouble();check("calculation-feedback-uses-live-busy-motion",phase>0&&phase<1);
 message->setMessage("Cancelled, previous result retained",StudioMessageRole::Feedback,false);wait(35);
 check("calculation-cancel-immediately-updates-text",message->text()=="Cancelled, previous result retained");
 check("completion-check-has-an-intermediate-frame",message->property("studio_message_confirmation_phase").toDouble()>0&&message->property("studio_message_confirmation_phase").toDouble()<1);
 const double stopped=message->property("studio_message_busy_phase").toDouble();wait(150);check("finished-calculation-stops-busy-driver",message->property("studio_message_busy_phase").toDouble()==stopped);
 message->setMessage("Warning persists until resolved",StudioMessageRole::Warning);wait(1500);check("warning-does-not-auto-dismiss",message->isVisible()&&message->text()=="Warning persists until resolved");
 message->clear();wait(30);check("message-exit-has-intermediate-layout-height",message->isVisible()&&message->height()>0&&!message->property("studio_message_target_visible").toBool());wait(160);check("message-finally-leaves-layout",!message->isVisible());
 message_motion::confirmation(button);wait(35);auto* mark=button->findChild<QWidget*>("StudioSuccessMotion");check("success-mark-draws-progressively",mark&&mark->property("studio_confirmation_progress").toDouble()>0&&mark->property("studio_confirmation_progress").toDouble()<1);
 owner.grab().save(out+"/messages-and-confirmation.png");wait(1300);check("success-presentation-cleans-up",!button->findChild<QWidget*>("StudioSuccessMotion"));
 app.setProperty("studio_motion_enabled",false);message->setText("Reduced motion");check("reduced-motion-message-immediate",message->isVisible());message->clear();check("reduced-motion-hide-immediate",!message->isVisible());
 int failed=0;for(const auto& c:cases)failed+=c.toObject()["status"]=="fail";
 QFile file(out+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(QJsonObject{{"status",failed?"fail":"pass"},{"cases",cases}}).toJson());return failed?1:0;
}
