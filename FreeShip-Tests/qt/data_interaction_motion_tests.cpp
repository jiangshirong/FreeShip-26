// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/data_motion.h"
#include "app/studio_ui.h"
#include <QApplication>
#include <QDialog>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QTableView>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <QVBoxLayout>

namespace {void wait(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}}
int main(int argc,char** argv){
 QApplication app(argc,argv);applyStudioTheme();app.setProperty("studio_motion_enabled",true);app.setQuitOnLastWindowClosed(false);
 const QString out=QString::fromLocal8Bit(argv[argc-1]);QDir().mkpath(out);QJsonArray cases;
 const auto check=[&](QString id,bool ok,QJsonObject observation=QJsonObject{}){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"},{"observed",observation}});};
 QDialog owner;owner.resize(780,580);auto* layout=new QVBoxLayout(&owner);
 QStandardItemModel model(3,2);model.setHorizontalHeaderLabels({"Parameter","Value"});
 for(int row=0;row<3;++row){model.setData(model.index(row,0),QString("parameter-%1").arg(row));model.setData(model.index(row,1),100+row);}
 auto* table=new QTableView;table->setModel(&model);layout->addWidget(table);owner.show();wait(30);
 data_motion::beginTableUpdate(table);data_motion::finishTableUpdate(table,"case-1");wait(10);
 check("results.first-presentation-is-not-changed",!table->property("studio_motion_result_active").toBool());
 data_motion::beginTableUpdate(table);model.setData(model.index(1,1),202.125);data_motion::finishTableUpdate(table,"case-1");wait(30);
 check("results.changed-only-visible-cell",table->property("studio_motion_result_changed_cells").toInt()==1&&table->property("studio_motion_result_active").toBool());
 check("results.numeric-value-applies-immediately",model.index(1,1).data().toDouble()==202.125);
 check("results.visible-intermediate-frame",table->property("studio_motion_result_progress").toDouble()>0&&table->property("studio_motion_result_progress").toDouble()<1);
 table->grab().save(out+"/result-change.png");wait(440);
 check("results.finite-animation-stops",!table->property("studio_motion_result_active").toBool());
 data_motion::beginTableUpdate(table);data_motion::finishTableUpdate(table,"case-1");wait(10);check("results.unchanged-update-not-highlighted",table->property("studio_motion_result_changed_cells").toInt()==0);
 data_motion::beginTableUpdate(table);model.setData(model.index(1,1),203.125);data_motion::finishTableUpdate(table,"different-case");wait(10);check("results.different-dataset-not-compared",table->property("studio_motion_result_changed_cells").toInt()==0);
 table->hide();
 auto* rows=new QTableWidget(3,2);layout->addWidget(rows);for(int row=0;row<3;++row){rows->setRowHeight(row,36);rows->setItem(row,0,new QTableWidgetItem(QString::number(row)));}
 wait(20);data_motion::beginRowUpdate(rows);data_motion::finishRowUpdate(rows,"sections-x",{"a","b","c"});
 data_motion::beginRowUpdate(rows);rows->insertRow(1);rows->setRowHeight(1,36);rows->setItem(1,0,new QTableWidgetItem("new"));data_motion::finishRowUpdate(rows,"sections-x",{"a","new","b","c"});
 check("rows.insert-immediate-model-change",rows->rowCount()==4&&rows->item(1,0)->text()=="new");
 check("rows.insert-has-native-height-transition",rows->property("studio_motion_rows_inserted").toInt()==1&&rows->property("studio_motion_rows_active").toBool()&&rows->rowHeight(1)<36);
 wait(200);check("rows.insert-restores-final-height",rows->rowHeight(1)==36&&!rows->property("studio_motion_rows_active").toBool(),{{"height",rows->rowHeight(1)},{"active",rows->property("studio_motion_rows_active") .toBool()},{"progress",rows->property("studio_motion_rows_progress").toDouble()},{"minimum",rows->verticalHeader()->minimumSectionSize()}});
 data_motion::beginRowUpdate(rows);rows->removeRow(1);data_motion::finishRowUpdate(rows,"sections-x",{"a","b","c"});
 check("rows.delete-immediate-model-change",rows->rowCount()==3&&rows->property("studio_motion_rows_removed").toInt()==1);
 rows->grab().save(out+"/row-departure.png");wait(160);check("rows.deleted-presentation-retires",!rows->property("studio_motion_rows_active").toBool());
 data_motion::beginRowUpdate(rows);rows->insertRow(1);rows->setRowHeight(1,36);data_motion::finishRowUpdate(rows,"sections-x",{"a","new","b","c"});
 check("rows.restore-emphasizes-same-identity",rows->property("studio_motion_rows_inserted").toInt()==1);wait(200);
 rows->hide();auto* panel=new QWidget;auto* panelLayout=new QVBoxLayout(panel);auto* editor=new QLineEdit;panelLayout->addWidget(editor);layout->addWidget(panel);data_motion::installProperties(panel);panel->show();wait(30);
 check("properties.reveal-preserves-complete-input-region",panel->property("studio_motion_property_active").toBool()&&panel->mask().isEmpty());
 const QPoint editorCenter=editor->mapTo(panel,editor->rect().center());
 check("properties.reveal-hit-test-still-reaches-editor",panel->childAt(editorCenter)==editor);
 data_motion::pulseProperties(panel,"point-17");wait(40);check("properties.object-identity-recorded",panel->property("studio_motion_property_identity").toString()=="point-17"&&panel->property("studio_motion_property_active").toBool());
 QKeyEvent key(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier,"a");QApplication::sendEvent(editor,&key);
 check("properties.typing-cancels-presentation-without-blocking-input",editor->text()=="a"&&!panel->property("studio_motion_property_active").toBool()&&panel->mask().isEmpty());
 data_motion::pulseProperties(panel,"point-18");wait(30);const auto phase=panel->property("studio_motion_property_progress").toDouble();data_motion::pulseProperties(panel,"point-18");check("properties.same-identity-does-not-restart",panel->property("studio_motion_property_progress").toDouble()==phase);wait(250);
 app.setProperty("studio_motion_enabled",false);data_motion::pulseProperties(panel,"point-19");check("properties.reduced-motion-has-immediate-final-state",!panel->property("studio_motion_property_active").toBool());
 int failed=0;for(const auto& c:cases)failed+=c.toObject()["status"]=="fail";
 QFile file(out+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(QJsonObject{{"status",failed?"fail":"pass"},{"cases",cases}}).toJson());
 return failed?1:0;
}
