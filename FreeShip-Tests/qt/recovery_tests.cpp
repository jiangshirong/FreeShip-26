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
#include <QtEndian>
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
// Exercise the production transport with real exits, including committed
// responses that never reach the UI. This probe only owns its private session.
struct BackendRecoveryProbe {
    QMetaObject::Connection interception;
    QByteArray buffer;
    QJsonObject dropped;
    void intercept(ModelSession& s,const QString& method) {
        QObject::disconnect(&s.process_,&QProcess::readyReadStandardOutput,&s,&ModelSession::readOutput);
        interception=QObject::connect(&s.process_,&QProcess::readyReadStandardOutput,&s,[this,&s,method]{
            buffer+=s.process_.readAllStandardOutput();qsizetype end;
            while((end=buffer.indexOf('\n'))>=0) {
                const auto line=buffer.left(end);buffer.remove(0,end+1);
                const auto response=QJsonDocument::fromJson(line).object();
                if(s.method_==method && response["ok"].toBool()) {dropped=response;s.process_.kill();return;}
                s.buffer_+=line+'\n';s.readOutput();
            }
        });
    }
    void restore(ModelSession& s) {
        QObject::disconnect(interception);
        QObject::connect(&s.process_,&QProcess::readyReadStandardOutput,&s,&ModelSession::readOutput);
    }
    static void exited(ModelSession& s) {
        QEventLoop loop;QTimer poll,deadline;bool timeout=false;
        QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(!s.healthy_&&!s.busy_)loop.quit();});
        QObject::connect(&deadline,&QTimer::timeout,&loop,[&]{timeout=true;loop.quit();});
        poll.start(5);deadline.setSingleShot(true);deadline.start(15000);
        if(s.healthy_||s.busy_)loop.exec();if(timeout)throw std::runtime_error("Owned backend did not exit");
    }
    static QByteArray payload(const QByteArray& raw) {
        const QByteArray magic("FreeShipRecovery\n");
        if(!raw.startsWith(magic)||raw.size()<magic.size()+4)throw std::runtime_error("Missing recovery payload");
        const auto n=qFromLittleEndian<quint32>(raw.constData()+magic.size());
        if(n>quint32(raw.size()-magic.size()-4))throw std::runtime_error("Truncated recovery payload");
        return raw.mid(magic.size()+4+n);
    }
    static QJsonObject run(const QJsonObject& config,const QString& out) {
        QJsonObject report;QJsonArray cases,artifacts;bool failed=false;
        auto check=[&](const QString& id,bool ok){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});failed|=!ok;};
        const auto original=read(config["model"].toString());
        try {
            for(const QString name:{"idle-edit","first-edit-reply-lost","edit-reply-lost","analysis-reply-lost","clean","changed-source","stale","corrupt","locked","missing","stale-save-receipt","save-ack-lost","changed-saved-file"}) {
                const auto folder=out+'/'+name;QDir().mkpath(folder);
                const auto source=folder+"/source.fbm",root=folder+"/recovery",saved=folder+"/saved.fbm";
                write(source,original);ModelSession s;s.setRecoveryRoot(root);QString error;int errors=0;
                s.onError=[&](const QString& e){error=e;++errors;};
                auto done=[&](bool accepted){if(!accepted)throw std::runtime_error((name+": rejected "+error).toUtf8().constData());idle(s);if(!error.isEmpty())throw std::runtime_error((name+": "+error).toUtf8().constData());};
                auto move=[&]{const auto p=s.model()->points.front();auto v=p.position;v.x+=.125;return s.movePoint(p.id,v);};
                done(s.load(source));check(name+".disabled-while-healthy",!s.canResumeCurrent()&&!s.resumeCurrent());
                QByteArray older;
                const bool clean=name=="clean"||name=="changed-source";
                if(!clean&&name!="first-edit-reply-lost")done(move());
                if(name=="stale"){older=read(s.recoveryFile());done(move());}
                if(name=="stale-save-receipt"){done(s.save(saved));done(move());}
                const auto before=s.model();const auto scene=read(s.scenePath()),dxf=read(s.dxfPath());
                const auto capsule=root+'/'+before->documentId+".fsr";
                BackendRecoveryProbe hook;
                const bool lostEdit=name=="first-edit-reply-lost"||name=="edit-reply-lost";
                const bool lostSave=name=="save-ack-lost"||name=="changed-saved-file";
                if(lostEdit||lostSave||name=="analysis-reply-lost") {
                    hook.intercept(s,lostEdit?"move_point":lostSave?"mark_saved":"hydrostatics");
                    const bool accepted=lostEdit?move():lostSave?s.save(saved):s.calculateHydrostatics({{"mode","single"},{"draft",2},{"heel",0},{"trim",0}});
                    if(!accepted)throw std::runtime_error("Faulted operation rejected");
                    exited(s);hook.restore(s);check(name+".actual-response-lost",!hook.dropped.isEmpty());
                } else {s.process_.kill();exited(s);}
                check(name+".dead-service-retains-view",!s.ready()&&s.canResumeCurrent()&&s.model()==before&&read(s.scenePath())==scene&&read(s.dxfPath())==dxf);
                check(name+".one-error-notification",errors==1);
                QByteArray raw=QFileInfo::exists(capsule)?read(capsule):QByteArray{};
                QByteArray expected=lostSave?read(saved):clean?original:payload(raw);
                if(lostEdit)check(name+".checkpoint-ahead-of-ui",quint64(hook.dropped["revision"].toDouble())>before->revision&&!raw.isEmpty());
                if(lostSave)check(name+".confirmed-file-survived",raw.isEmpty()&&s.modified()&&QFileInfo::exists(saved));
                const auto expectedFile=folder+"/expected.fbm";write(expectedFile,expected);
                if(!raw.isEmpty())write(folder+"/before.fsr",raw);
                std::unique_ptr<fs::RecoveryLease> other;
                const bool negative=name=="changed-source"||name=="stale"||name=="corrupt"||name=="locked"||name=="missing"||name=="stale-save-receipt"||name=="changed-saved-file";
                if(name=="changed-source")write(source,original+"external replacement");
                if(name=="changed-saved-file")write(saved,expected+"external replacement");
                if(name=="stale")write(capsule,older);
                if(name=="corrupt"){auto bad=raw;bad[bad.size()-1]^=1;write(capsule,bad);}
                if(name=="locked")other=std::make_unique<fs::RecoveryLease>(root,before->documentId);
                if(name=="missing"||name=="stale-save-receipt")QFile::remove(capsule);
                if(negative) {
                    const auto sourceBefore=read(source),savedBefore=QFileInfo::exists(saved)?read(saved):QByteArray{},capsuleBefore=QFileInfo::exists(capsule)?read(capsule):QByteArray{};
                    error.clear();const bool accepted=s.resumeCurrent();
                    check(name+".unsafe-resume-rejected",!accepted&&!error.isEmpty()&&s.model()==before&&!s.ready()&&!s.busy());
                    check(name+".rejection-preserves-evidence",read(source)==sourceBefore&&(QFileInfo::exists(saved)?read(saved):QByteArray{})==savedBefore&&(QFileInfo::exists(capsule)?read(capsule):QByteArray{})==capsuleBefore);
                    other.reset();if(!raw.isEmpty())write(capsule,raw);write(source,original);if(lostSave)write(saved,expected);
                }
                error.clear();done(s.resumeCurrent());
                check(name+".resumed-live-new-document",s.ready()&&!s.canResumeCurrent()&&s.model()->documentId!=before->documentId&&!s.canUndo()&&!s.canRedo());
                check(name+".dirty-state",s.modified()==(!clean&&!lostSave));
                check(name+".source-policy",(clean||lostSave)?fs::sameModelFile(s.sourcePath(),lostSave?saved:source):(s.sourcePath().isEmpty()&&!s.recoveryOrigin().isEmpty()));
                check(name+".old-capsule-retired-after-success",!QFileInfo::exists(capsule));
                if(!clean&&!lostSave)check(name+".replacement-checkpoint-exact",payload(read(s.recoveryFile()))==expected);
                const auto restored=folder+"/restored.fbm";done(s.save(restored));
                check(name+".restored-bytes-exact",read(restored)==expected);
                if(!lostEdit){check(name+".scene-exact",read(s.scenePath())==scene);check(name+".dxf-exact",read(s.dxfPath())==dxf);}
                done(move());check(name+".can-edit-after-resume",s.modified()&&s.canUndo());
                done(s.undo());check(name+".undo-new-edit",!s.modified()&&s.canRedo());
                done(s.save(restored));check(name+".new-undo-preserves-bytes",read(restored)==expected);
                check(name+".original-protected",read(source)==original);
                if(lostSave)check(name+".committed-file-not-rewritten",read(saved)==expected&&!QFileInfo::exists(saved+".Bak"));
                artifacts.append(QJsonObject{{"name",name},{"expected",expectedFile},{"restored",restored},{"capsule",raw.isEmpty()?QString{}:folder+"/before.fsr"}});
            }
            report["status"]=failed?"fail":"pass";
        } catch(const std::exception& ex) {report["status"]="error";report["error"]=QString::fromUtf8(ex.what());}
        report["cases"]=cases;report["artifacts"]=artifacts;return report;
    }
};
int main(int argc,char** argv){
    QApplication app(argc,argv);if(argc!=3)return 3;
    const auto config=QJsonDocument::fromJson(read(app.arguments()[1])).object();const auto out=app.arguments()[2];QDir().mkpath(out);
    if(config["mode"]=="backend") {const auto result=BackendRecoveryProbe::run(config,out);write(out+"/report.json",QJsonDocument(result).toJson());return result["status"]=="pass"?0:1;}
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
