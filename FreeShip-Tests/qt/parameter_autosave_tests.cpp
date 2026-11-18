// SPDX-License-Identifier: GPL-2.0-or-later
// Real ModelSession transactions against its private native document process.
#include "app/model_session.h"
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <stdexcept>

static QByteArray bytes(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read fixture");return f.readAll();}
static QJsonObject values(const QJsonObject& schema){QJsonObject result;for(const auto& v:schema["fields"].toArray()){const auto f=v.toObject();result[f["key"].toString()]=f["value"];}return result;}
static void idle(ModelSession& s){
    if(!s.busy())return;QEventLoop loop;QTimer poll,deadline;bool expired=false;
    poll.setInterval(5);deadline.setSingleShot(true);deadline.setInterval(125000);
    QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(!s.busy())loop.quit();});
    QObject::connect(&deadline,&QTimer::timeout,&loop,[&]{expired=true;loop.quit();});poll.start();deadline.start();loop.exec();
    if(expired)throw std::runtime_error("Session timed out");
}
int main(int argc,char** argv){
    QApplication app(argc,argv);if(argc!=3)return 3;
    const auto source=app.arguments()[1],out=app.arguments()[2];QDir().mkpath(out+"/中文路径");
    const auto saved=out+"/中文路径/Женя.fbm";QJsonArray cases,events;QJsonObject artifacts;int failures=0;
    const auto check=[&](QString id,bool ok){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});failures+=!ok;};
    try{
        const auto original=bytes(source);QJsonObject bExpected,wExpected;
        {
            ModelSession s;QString error;QJsonObject report;int reports=0;
            s.onError=[&](const QString& e){error=e;events.append(QJsonObject{{"error",e}});};
            s.onCompleted=[&](const QString& op){events.append(QJsonObject{{"completed",op},{"dirty",s.modified()},{"revision",double(s.model()->revision)}});};
            s.onPerformance=[&](const QJsonObject& data,bool defaults){report=data;if(!defaults){++reports;check("result-published-after-parameter-commit",data["parameters_recorded"]==true&&!s.busy()&&data["revision"].toDouble()==double(s.model()->revision));}};
            const auto success=[&](bool accepted){if(!accepted)throw std::runtime_error("Session refused operation");idle(s);if(!error.isEmpty())throw std::runtime_error(error.toUtf8().constData());};
            success(s.load(source));check("load-clean",!s.modified());
            success(s.performanceDefaults({{"calculator","propeller5"}}));check("defaults-do-not-dirty",!s.modified());
            QJsonObject b{{"ComboBox",0},{"ComboBox1",0},{"Edit2",200.},{"Edit3",.7},{"Edit4",1.},{"Edit5",4.},{"Edit6",3.},{"Edit7",4.},{"BSeriesReynoldsMode",1},{"BSeriesReynoldsMillions",37.125}};
            const auto scene=bytes(s.scenePath());success(s.calculatePerformance({{"calculator","propeller5"},{"values",b}}));
            bExpected=values(report["parameters"].toObject());check("successful-calculation-dirties-model",s.modified());
            check("numerical-options-not-truncated-to-old-fbm-fields",bExpected["BSeriesReynoldsMode"]==1&&bExpected["BSeriesReynoldsMillions"]==37.125);
            check("parameter-commit-does-not-redraw-hull",bytes(s.scenePath())==scene);
            check("parameter-commit-reuses-visible-drawing",s.drawingUnchanged());
            success(s.save(saved));check("normal-save-marks-clean",!s.modified());const auto firstSaved=bytes(saved);
            const auto revision=s.model()->revision;const int beforeReports=reports;
            success(s.calculatePerformance({{"calculator","propeller5"},{"values",bExpected}}));
            check("identical-calculation-is-not-a-new-edit",!s.modified()&&s.model()->revision==revision&&reports==beforeReports+1);
            check("calculation-does-not-write-user-file",bytes(saved)==firstSaved);
            success(s.performanceDefaults({{"calculator","wind"}}));const auto originalWind=values(report);
            auto w=originalWind;w["Edit9"]=12;w["Edit10"]=3.75;w["Edit11"]=14.25;
            auto invalid=w;invalid["Edit2"]=0;
            if(!s.calculatePerformance({{"calculator","wind"},{"values",invalid}}))throw std::runtime_error("Failed calculation not submitted");idle(s);
            check("failed-calculation-not-recorded",!error.isEmpty()&&!s.modified()&&s.model()->revision==revision);error.clear();
            success(s.performanceDefaults({{"calculator","wind"}}));check("failure-preserves-last-inputs",values(report)==originalWind);
            if(!s.calculatePerformance({{"calculator","wind"},{"values",w}}))throw std::runtime_error("Cancellation not submitted");s.cancelPerformance();idle(s);
            check("cancel-including-fast-solver-race-does-not-record",!error.isEmpty()&&!s.modified()&&s.model()->revision==revision);error.clear();
            success(s.performanceDefaults({{"calculator","wind"}}));check("cancellation-preserves-last-inputs",values(report)==originalWind);
            success(s.calculatePerformance({{"calculator","wind"},{"values",w}}));wExpected=values(report["parameters"].toObject());
            check("second-method-stored-without-replacing-first",s.modified());
            success(s.undo());check("automatic-parameters-participate-in-undo",!s.modified());
            success(s.performanceDefaults({{"calculator","wind"}}));check("undo-restores-prior-wind-settings",values(report)==originalWind);
            success(s.redo());success(s.performanceDefaults({{"calculator","wind"}}));check("redo-restores-complete-wind-inputs",values(report)==wExpected);
            success(s.save(saved));check("all-methods-save-through-normal-model-save",!s.modified());
        }
        {
            ModelSession s;QString error;QJsonObject schema;s.onError=[&](const QString& e){error=e;};
            s.onPerformance=[&](const QJsonObject& data,bool defaults){if(defaults)schema=data;};
            const auto success=[&](bool accepted){if(!accepted)throw std::runtime_error("Fresh session refused operation");idle(s);if(!error.isEmpty())throw std::runtime_error(error.toUtf8().constData());};
            success(s.load(saved));check("fresh-session-loads-saved-model-clean",!s.modified());
            success(s.performanceDefaults({{"calculator","propeller5"}}));check("fresh-process-restores-all-propeller-options",values(schema)==bExpected&&!s.modified());
            success(s.performanceDefaults({{"calculator","wind"}}));check("fresh-process-restores-all-wind-inputs",values(schema)==wExpected&&!s.modified());
        }
        check("original-fixture-never-overwritten",bytes(source)==original);
        artifacts["saved_model"]=saved;artifacts["expected_propeller5"]=bExpected;artifacts["expected_wind"]=wExpected;
    }catch(const std::exception& e){cases.append(QJsonObject{{"id","execution"},{"status","error"},{"reason",QString::fromUtf8(e.what())}});++failures;}
    QFile f(out+"/report.json");f.open(QIODevice::WriteOnly);f.write(QJsonDocument(QJsonObject{{"status",failures?"fail":"pass"},{"cases",cases},{"events",events},{"artifacts",artifacts}}).toJson());return failures?1:0;
}
