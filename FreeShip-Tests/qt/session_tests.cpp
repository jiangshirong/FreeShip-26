// Exercise the production C++ session against its real persistent child process.
#include "app/model_session.h"
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <limits>
#include <stdexcept>

static QByteArray read(const QString& path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read evidence");
    return file.readAll();
}
static void write(const QString& path,const QByteArray& bytes) {
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size())
        throw std::runtime_error("Cannot write evidence");
}
static void waitIdle(ModelSession& session) {
    if(!session.busy())return;
    QEventLoop loop;QTimer poll,deadline;bool timedOut=false;
    poll.setInterval(5);deadline.setSingleShot(true);deadline.setInterval(125000);
    QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(!session.busy())loop.quit();});
    QObject::connect(&deadline,&QTimer::timeout,&loop,[&]{timedOut=true;loop.quit();});
    poll.start();deadline.start();loop.exec();
    if(timedOut)throw std::runtime_error("Session test timed out");
}
int main(int argc,char**argv) {
    QApplication app(argc,argv);if(argc!=3)return 3;
    const QString input=app.arguments()[1],out=app.arguments()[2];QDir().mkpath(out);
    QJsonArray cases,events;QJsonObject report,artifacts;bool failed=false;
    auto check=[&](const QString& name,bool ok){cases.append(QJsonObject{{"id",name},{"status",ok?"pass":"fail"}});failed|=!ok;};
    try {
        const QByteArray originalInput=read(input);QString workspace;
        {
            ModelSession session;QString error;int errorCount=0,completed=0,updates=0;
            session.onError=[&](const QString& message){error=message;++errorCount;events.append(QJsonObject{{"event","error"},{"message",message}});};
            session.onCompleted=[&](const QString& operation){++completed;events.append(QJsonObject{{"event","completed"},{"operation",operation},{"revision",double(session.model()->revision)},{"dirty",session.modified()}});};
            session.onUpdated=[&]{++updates;};
            auto success=[&](const QString& name,bool accepted) {
                check(name+"-accepted",accepted);
                if(!accepted)throw std::runtime_error("Required operation was not accepted");
                waitIdle(session);check(name+"-completed-without-error",error.isEmpty()&&session.ready()&&!session.busy());
                if(!session.ready()||!error.isEmpty())throw std::runtime_error(error.toUtf8().constData());
            };
            auto capture=[&](const QString& name) {
                for(const auto& item:{qMakePair(QString("scene"),session.scenePath()),qMakePair(QString("model"),session.modelPath()),qMakePair(QString("dxf"),session.dxfPath())}) {
                    const QString destination=out+"/"+name+"-"+QFileInfo(item.second).fileName();
                    ModelSession::atomicCopy(item.second,destination);artifacts[name+"_"+item.first]=destination;
                }
                const auto native=QJsonDocument::fromJson(read(QFileInfo(session.scenePath()).absolutePath()+"/native-report.json")).object();
                check(name+"-uses-native-engine",native["engine"]=="cpp-native"&&native["status"]=="pass"&&native["document_id"]==session.model()->documentId&&native["revision"].toDouble()==double(session.model()->revision));
                write(out+"/"+name+"-native-report.json",QJsonDocument(native).toJson());
            };
            success("load",session.load(input));capture("original");
            check("loaded-clean-without-history",!session.modified()&&!session.canUndo()&&!session.canRedo());
            const auto original=session.model();const auto originalScene=read(session.scenePath());
            const QString initialSource=session.sourcePath();
            workspace=QFileInfo(QFileInfo(session.modelPath()).absolutePath()).absolutePath();
            fs::ControlPoint selected;bool found=false;
            for(const auto& point:original->points)if(!point.locked){selected=point;found=true;break;}
            if(!found)throw std::runtime_error("Sample has no editable point");
            fs::Vec3 moved=selected.position;moved.z=double(float(moved.z+0.01));
            check("move-accepted",session.movePoint(selected.id,moved));
            const int errorsBeforeBusy=errorCount;
            const bool rejected=!session.movePoint(selected.id,{std::numeric_limits<double>::quiet_NaN(),0,0});
            check("busy-invalid-edit-does-not-cancel-request",rejected&&session.busy()&&errorCount==errorsBeforeBusy);
            check("busy-load-does-not-replace-request",!session.load(input)&&session.busy());
            waitIdle(session);check("in-flight-move-completes",session.ready()&&!session.busy()&&error.isEmpty());
            if(!session.ready()||!error.isEmpty())throw std::runtime_error("In-flight move failed");
            capture("moved");const auto movedScene=read(session.scenePath());
            check("edit-dirty-and-undoable",session.modified()&&session.canUndo()&&!session.canRedo());
            check("immutable-previous-snapshot",original->point(selected.id)->position.z==selected.position.z&&original->revision<session.model()->revision);
            check("scene-changed-with-edit",movedScene!=originalScene);

            const QString blocker=out+"/已有文件";const QByteArray sentinel("existing-user-data\x00\xff",20);write(blocker,sentinel);
            const auto revisionBeforeFailure=session.model()->revision;
            const int failuresBefore=errorCount;
            check("failed-destination-save-started",session.save(blocker+"/child.fbm"));waitIdle(session);
            check("failed-destination-reported",errorCount==failuresBefore+1&&!error.isEmpty());
            check("failed-destination-preserves-state",session.ready()&&session.modified()&&session.sourcePath()==initialSource&&session.model()->revision==revisionBeforeFailure&&session.canUndo());
            check("failed-destination-preserves-original-files",read(blocker)==sentinel&&read(input)==originalInput);
            check("failed-destination-preserves-scene",read(session.scenePath())==movedScene);
            error.clear();const QString saved=out+"/保存往返.fbm";
            success("save-after-failure",session.save(saved));artifacts["saved_file"]=saved;
            check("successful-save-clean-and-new-destination",!session.modified()&&session.sourcePath()==QFileInfo(saved).absoluteFilePath()&&QFileInfo(saved).size()>0);
            check("save-preserves-current-scene",read(session.scenePath())==movedScene);
            auto privateFolders=QDir(workspace).entryList({"request-*","input-*"},QDir::Dirs|QDir::NoDotAndDotDot);
            check("staged-and-old-output-folders-reclaimed",privateFolders.size()==2);

            success("undo",session.undo());check("undo-after-save-is-dirty",session.modified()&&read(session.scenePath())==originalScene);
            success("redo",session.redo());check("redo-to-saved-revision-is-clean",!session.modified()&&read(session.scenePath())==movedScene);
            const QString corrupt=out+"/损坏模型.fbm";write(corrupt,"invalid-fbm\0");
            const auto beforeBad=session.model();const QString beforeBadSource=session.sourcePath();
            const int beforeBadCount=errorCount;
            check("corrupt-load-submitted",session.load(corrupt));waitIdle(session);
            check("corrupt-load-reported",errorCount==beforeBadCount+1&&!error.isEmpty());
            check("corrupt-load-keeps-published-document",session.ready()&&session.model()==beforeBad&&session.sourcePath()==beforeBadSource&&!session.modified()&&read(session.scenePath())==movedScene);
            privateFolders=QDir(workspace).entryList({"request-*","input-*"},QDir::Dirs|QDir::NoDotAndDotDot);
            check("failed-input-and-output-folders-reclaimed",privateFolders.size()==2);
            error.clear();success("reopen",session.load(saved));capture("reopened");
            check("reopen-is-fresh-clean-document",!session.modified()&&!session.canUndo()&&!session.canRedo()&&session.model()->documentId!=beforeBad->documentId);
            check("reopen-scene-exact",read(session.scenePath())==movedScene);
            privateFolders=QDir(workspace).entryList({"request-*","input-*"},QDir::Dirs|QDir::NoDotAndDotDot);
            check("reopen-reclaims-replaced-input",privateFolders.size()==2);

            const QString protectedDestination=out+"/atomic-existing.fbm";write(protectedDestination,sentinel);
            bool failedCopy=false;try{ModelSession::atomicCopy(out+"/missing-source.fbm",protectedDestination);}catch(const std::exception&){failedCopy=true;}
            check("atomic-copy-missing-source-preserves-destination",failedCopy&&read(protectedDestination)==sentinel);
            report["completion_notifications"]=completed;report["snapshot_notifications"]=updates;
            report["workspace_before_close"]=workspace;
        }
        check("session-destruction-reclaims-private-workspace",!QFileInfo::exists(workspace));
        check("original-model-never-overwritten",read(input)==originalInput);
        cases.append(QJsonObject{{"id","destination-committed-but-mark-saved-response-lost"},{"status","not_run"},{"reason","No reliable production-process fault injection in this suite."}});
        cases.append(QJsonObject{{"id","native-file-dialog-and-desktop-input"},{"status","not_run"}});
        report["status"]=failed?"fail":"partial";
    }catch(const std::exception& exception){report["status"]="error";report["error"]=exception.what();}
    report["cases"]=cases;report["events"]=events;report["artifacts"]=artifacts;
    report["scope"]="Production ModelSession, actual bridge, actual QSaveFile destination writes; private inputs and hidden event loop.";
    write(out+"/report.json",QJsonDocument(report).toJson());
    return report["status"]=="partial"?2:report["status"]=="fail"?1:3;
}
