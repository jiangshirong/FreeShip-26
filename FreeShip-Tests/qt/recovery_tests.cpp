// Retained recovery-store fault injection and real session crash producer.
#include "app/model_session.h"
#include "core/recovery_store.h"
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QUuid>
#include <cstdlib>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
static QByteArray read(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Read failed");return f.readAll();}
static void write(const QString& path,const QByteArray& data){QFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size())throw std::runtime_error("Write failed");}
static void idle(ModelSession& s){QEventLoop loop;QTimer timer,deadline;bool timedOut=false;
    QObject::connect(&timer,&QTimer::timeout,&loop,[&]{if(!s.busy())loop.quit();});
    QObject::connect(&deadline,&QTimer::timeout,&loop,[&]{timedOut=true;loop.quit();});
    timer.start(5);deadline.setSingleShot(true);deadline.start(120000);if(s.busy())loop.exec();
    if(timedOut)throw std::runtime_error("Session timeout");}
int main(int argc,char** argv){
    QApplication app(argc,argv);if(argc!=3)return 3;
    const auto config=QJsonDocument::fromJson(read(app.arguments()[1])).object();const auto out=app.arguments()[2];QDir().mkpath(out);
    QJsonObject report;QJsonArray cases;bool failed=false;
    auto check=[&](const QString& name,bool ok){cases.append(QJsonObject{{"id",name},{"status",ok?"pass":"fail"}});failed|=!ok;};
    try{
        if(config["mode"]=="crash"){
            ModelSession session;session.setRecoveryRoot(config["root"].toString());QString error;
            session.onError=[&](const QString& e){error=e;};
            auto done=[&](bool accepted){if(!accepted)throw std::runtime_error("Request rejected");idle(session);if(!error.isEmpty())throw std::runtime_error(error.toUtf8().constData());};
            if(config["new"].toBool())done(session.newHull({{"length",8},{"beam",3},{"draft",1},{"rows",4},{"columns",5},{"units",0}}));
            else done(session.load(config["model"].toString()));
            const fs::ControlPoint* selected=nullptr;for(const auto& p:session.model()->points)if(!p.locked&&p.batchMovable){selected=&p;break;}
            if(!selected)throw std::runtime_error("No movable point");
            auto position=selected->position;position.x+=.125;done(session.movePoint(selected->id,position));
            check("committed-edit-has-recovery",session.modified()&&!session.recoveryFile().isEmpty()&&session.recoveryError().isEmpty());
            for(const auto& item:{qMakePair(QString("model"),session.modelPath()),qMakePair(QString("scene"),session.scenePath()),qMakePair(QString("dxf"),session.dxfPath())}){
                const auto target=out+'/'+item.first+QFileInfo(item.second).fileName();ModelSession::atomicCopy(item.second,target);report[item.first]=target;
            }
            report["capsule"]=session.recoveryFile();report["document_id"]=session.model()->documentId;
            const auto lock=config["root"].toString()+'/'+session.model()->documentId+".lock";
            report["service_pid"]=QString::fromLatin1(read(lock).split('\n').first()).toDouble();
            report["cases"]=cases;report["status"]=failed?"fail":"pass";
            write(out+"/report.json",QJsonDocument(report).toJson());
            // Deliberately skip destructors: the document service's production
            // parent-death watcher must release its stale lease after this exit.
            std::_Exit(failed?1:0);
        }
        const auto root=out+"/store";const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);const auto fbm=read(config["model"].toString());QString path;
        {
            fs::RecoveryLease lease(root,id);lease.checkpoint(fbm,12,"D:/船模/Женя.fbm");path=lease.path();
            fs::RecoveryEntry entry;check("exact-fbm-and-unicode-metadata",lease.read(&entry)==fbm&&entry.source=="D:/船模/Женя.fbm"&&entry.revision==12);
            check("live-session-not-offered",fs::recoveryEntries(root).isEmpty());
            bool locked=false;try{fs::RecoveryLease other(root,id);}catch(...){locked=true;}check("second-instance-cannot-claim",locked);
            const auto original=read(path);bool rejected=false;try{lease.checkpoint({},13,"");}catch(...){rejected=true;}
            check("invalid-write-preserves-last-copy",rejected&&read(path)==original);
#ifdef Q_OS_WIN
            HANDLE block=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(block==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot inject destination lock");
            rejected=false;try{lease.checkpoint(fbm,13,"");}catch(...){rejected=true;}
            CloseHandle(block);check("failed-atomic-replacement-retains-complete-previous-copy",rejected&&read(path)==original);
#endif
            lease.checkpoint(fbm,14,"");check("retry-after-write-failure",lease.read(&entry)==fbm&&entry.revision==14);
            auto corrupted=read(path);corrupted[corrupted.size()-1]^=1;write(path,corrupted);rejected=false;
            try{lease.read();}catch(...){rejected=true;}check("payload-checksum-rejects-corruption-without-deletion",rejected&&read(path)==corrupted);
            lease.checkpoint(fbm,15,"");
        }
        check("released-session-is-offered",fs::recoveryEntries(root).size()==1);
        {fs::RecoveryLease lease(root,id);lease.remove();}check("explicit-discard-removes-only-capsule",!QFileInfo::exists(path)&&fs::recoveryEntries(root).isEmpty());
        bool rejected=false;try{fs::RecoveryLease lease(root,"../outside");}catch(...){rejected=true;}check("path-traversal-rejected",rejected);
        const auto blocker=out+"/not-a-directory";write(blocker,"sentinel");rejected=false;try{fs::RecoveryLease lease(blocker+"/recovery",id);}catch(...){rejected=true;}
        check("unwritable-root-keeps-existing-file",rejected&&read(blocker)=="sentinel");
        {fs::RecoveryLease lease(root,id);write(lease.path(),"bad");}
        const auto bad=fs::recoveryEntries(root);check("bad-header-is-listed-with-error",bad.size()==1&&!bad[0].error.isEmpty());
        report["status"]=failed?"fail":"pass";
    }catch(const std::exception& e){report["status"]="error";report["error"]=QString::fromUtf8(e.what());}
    report["cases"]=cases;write(out+"/report.json",QJsonDocument(report).toJson());return report["status"]=="pass"?0:1;
}
