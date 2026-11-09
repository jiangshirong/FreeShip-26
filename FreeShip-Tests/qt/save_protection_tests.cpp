// Actual filesystem failures and two production ModelSessions; no desktop input.
#include "app/model_session.h"
#include "core/model_file_save.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <stdexcept>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
static QByteArray read(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Read failed");return f.readAll();}
static void write(const QString& path,const QByteArray& data){QFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size())throw std::runtime_error("Write failed");}
static void idle(ModelSession& s){
    QEventLoop loop;QTimer timer,deadline;bool timedOut=false;
    QObject::connect(&timer,&QTimer::timeout,&loop,[&]{if(!s.busy())loop.quit();});
    QObject::connect(&deadline,&QTimer::timeout,&loop,[&]{timedOut=true;loop.quit();});
    timer.start(5);deadline.setSingleShot(true);deadline.start(120000);if(s.busy())loop.exec();
    if(timedOut)throw std::runtime_error("Session timeout");
}
// Fault injection at the owned QProcess transport: responses are produced by
// the real service. No simulated success and no production fault flag.
struct SaveConfirmationProbe {
    QByteArray buffer,request;
    int dropped=0,received=0;
    qint64 requestId=-1;
    QMetaObject::Connection connection;
    static std::shared_ptr<SaveConfirmationProbe> dropReplies(ModelSession& session,int count){
        auto probe=std::make_shared<SaveConfirmationProbe>();
        QObject::disconnect(&session.process_,&QProcess::readyReadStandardOutput,&session,&ModelSession::readOutput);
        probe->connection=QObject::connect(&session.process_,&QProcess::readyReadStandardOutput,&session,[&session,probe,count]{
            probe->buffer+=session.process_.readAllStandardOutput();qsizetype end;
            while((end=probe->buffer.indexOf('\n'))>=0){
                const auto line=probe->buffer.left(end);probe->buffer.remove(0,end+1);
                const auto response=QJsonDocument::fromJson(line).object();
                if(response["save_confirmed"]==true){
                    ++probe->received;
                    if(probe->dropped<count){
                        probe->request=session.saveConfirmationLine_;probe->requestId=session.pendingId_;++probe->dropped;continue;
                    }
                }
                session.buffer_+=line+'\n';session.readOutput();
            }
        });
        return probe;
    }
    void repeat(ModelSession& session){
        if(request.isEmpty()||session.process_.write(request)!=request.size())throw std::runtime_error("Cannot repeat owned save confirmation");
    }
    void restore(ModelSession& session){
        QObject::disconnect(connection);
        QObject::connect(&session.process_,&QProcess::readyReadStandardOutput,&session,&ModelSession::readOutput);
    }
};
int main(int argc,char** argv){
    QApplication app(argc,argv);if(argc!=3)return 3;
    const auto input=app.arguments()[1],out=app.arguments()[2];QDir().mkpath(out);
    QJsonObject report;QJsonArray cases;bool failed=false;
    auto check=[&](const QString& id,bool ok){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});failed|=!ok;};
    auto fails=[&](const QString& id,auto action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(id,rejected);};
    try{
        const auto candidate=out+"/candidate.fbm",target=out+"/已有-Женя.fbm",backup=fs::modelBackupPath(target);
        const QByteArray a("old model\0bytes",15),b("new model\0bytes",15),older("older backup\0bytes",18);
        write(candidate,b);write(target,a);write(backup,older);
        const auto promised=fs::observeModelFile(candidate);write(candidate,"changed staged bytes");
        fails("staged-file-must-match-promised-content",[&]{fs::ModelFileSave save(candidate,target,fs::observeModelFile(target),promised);save.commit();});
        check("staged-mismatch-keeps-target-and-backup",read(target)==a&&read(backup)==older);write(candidate,b);
        auto put=[&]{fs::ModelFileSave save(candidate,target,fs::observeModelFile(target));return save.commit();};
        auto result=put();check("overwrite-and-previous-version-exact",read(target)==b&&read(backup)==a&&result.changed&&result.backup==backup);
        const auto mtime=QFileInfo(backup).lastModified();result=put();check("unchanged-save-keeps-previous-backup",!result.changed&&read(backup)==a&&QFileInfo(backup).lastModified()==mtime);
        write(candidate,a);put();check("next-overwrite-rotates-one-backup",read(target)==a&&read(backup)==b);
        auto stale=fs::observeModelFile(target);write(target,b);
        fails("changed-target-rejected",[&]{fs::ModelFileSave save(candidate,target,stale);save.commit();});check("changed-target-and-backup-preserved",read(target)==b&&read(backup)==b);
        QFile::remove(target);fails("deleted-target-not-recreated",[&]{fs::ModelFileSave save(candidate,target,stale);save.commit();});check("deletion-stays-visible",!QFileInfo::exists(target));
        write(target,a);write(candidate,b);write(backup,older);
        {
            fs::ModelFileSave prepared(candidate,target,fs::observeModelFile(target));
            fails("concurrent-save-lock",[&]{fs::ModelFileSave other(candidate,target,fs::observeModelFile(target));});
            write(target,"external replacement");fails("late-target-change-rejected",[&]{prepared.commit();});
        }
        check("late-change-keeps-both-files",read(target)=="external replacement"&&read(backup)==older);
        write(target,a);
        {
            fs::ModelFileSave prepared(candidate,target,fs::observeModelFile(target));
            write(backup,"external backup");fails("late-backup-change-rejected",[&]{prepared.commit();});
        }
        check("late-backup-keeps-both-files",read(target)==a&&read(backup)=="external backup");write(backup,older);
#ifdef Q_OS_WIN
        {
            fs::ModelFileSave prepared(candidate,target,fs::observeModelFile(target));
            HANDLE lock=CreateFileW(reinterpret_cast<LPCWSTR>(target.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(lock==INVALID_HANDLE_VALUE)throw std::runtime_error("Target fault injection unavailable");
            fails("target-commit-failure",[&]{prepared.commit();});CloseHandle(lock);
            check("failed-commit-rolls-back-existing-backup",read(target)==a&&read(backup)==older);
        }
        QFile::remove(backup);
        {
            fs::ModelFileSave prepared(candidate,target,fs::observeModelFile(target));
            HANDLE lock=CreateFileW(reinterpret_cast<LPCWSTR>(target.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(lock==INVALID_HANDLE_VALUE)throw std::runtime_error("Target fault injection unavailable");
            fails("commit-failure-with-no-previous-backup",[&]{prepared.commit();});CloseHandle(lock);
            check("failed-commit-removes-only-new-backup",read(target)==a&&!QFileInfo::exists(backup));
        }
        write(backup,older);
        {
            fs::ModelFileSave prepared(candidate,target,fs::observeModelFile(target));
            HANDLE lock=CreateFileW(reinterpret_cast<LPCWSTR>(backup.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(lock==INVALID_HANDLE_VALUE)throw std::runtime_error("Backup fault injection unavailable");
            fails("backup-commit-failure",[&]{prepared.commit();});CloseHandle(lock);
            check("failed-backup-keeps-both-files",read(target)==a&&read(backup)==older);
        }
        const auto attributes=GetFileAttributesW(reinterpret_cast<LPCWSTR>(target.utf16()));
        if(!SetFileAttributesW(reinterpret_cast<LPCWSTR>(target.utf16()),attributes|FILE_ATTRIBUTE_READONLY))throw std::runtime_error("Read-only injection failed");
        fails("read-only-target-rejected",[&]{put();});SetFileAttributesW(reinterpret_cast<LPCWSTR>(target.utf16()),attributes);
        check("read-only-target-and-backup-intact",read(target)==a&&read(backup)==older);
#endif
        const auto fresh=out+"/new.fbm";auto missing=fs::observeModelFile(fresh);
        {
            fs::ModelFileSave prepared(candidate,fresh,missing);write(fresh,"created meanwhile");
            fails("new-target-appeared-during-save",[&]{prepared.commit();});
        }
        check("new-target-is-not-overwritten",read(fresh)=="created meanwhile"&&!QFileInfo::exists(fs::modelBackupPath(fresh)));
        QFile::remove(fresh);{fs::ModelFileSave save(candidate,fresh,missing);save.commit();}check("new-target-installed-completely",read(fresh)==b&&!QFileInfo::exists(fs::modelBackupPath(fresh)));
        const auto bakTarget=out+"/opened.Bak";write(bakTarget,a);
        {fs::ModelFileSave save(candidate,bakTarget,fs::observeModelFile(bakTarget));save.commit();}
        check("saving-bak-never-aliases-itself",read(bakTarget)==b&&read(bakTarget+".Bak")==a);
        const auto blocked=out+"/blocked.fbm";write(blocked,a);QDir().mkpath(fs::modelBackupPath(blocked));
        fails("backup-directory-rejected",[&]{fs::ModelFileSave save(candidate,blocked,fs::observeModelFile(blocked));save.commit();});
        check("backup-directory-and-target-intact",read(blocked)==a&&QFileInfo(fs::modelBackupPath(blocked)).isDir());
        check("temporary-save-files-cleaned",QDir(out).entryList({".freeship-*","*.freeship-save.lock"},QDir::Files|QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot).isEmpty());

        const auto model=out+"/two-editors.fbm";const auto source=read(input);write(model,source);
        ModelSession left,right;left.setRecoveryRoot(out+"/recovery");right.setRecoveryRoot(out+"/recovery");QString le,re;
        left.onError=[&](const QString& error){le=error;};right.onError=[&](const QString& error){re=error;};
        auto good=[&](ModelSession& session,bool accepted,QString& error){idle(session);if(!accepted||!error.isEmpty()||!session.ready())throw std::runtime_error(error.toUtf8().constData());};
        good(left,left.load(model),le);good(right,right.load(model),re);
        auto move=[&](ModelSession& s,double dx,QString& error){
            for(const auto& p:s.model()->points)if(!p.locked&&p.batchMovable){auto v=p.position;v.x+=dx;good(s,s.movePoint(p.id,v),error);return;}
            throw std::runtime_error("No movable point");
        };
        move(left,.125,le);move(right,.25,re);const auto rightScene=read(right.scenePath());
        good(left,left.save(model),le);const auto firstSaved=read(model),firstBackup=read(fs::modelBackupPath(model));
        check("first-editor-saves-with-backup",firstSaved!=source&&firstBackup==source&&!left.modified());
        right.save(model);idle(right);
        check("second-editor-cannot-silently-overwrite",!re.isEmpty()&&right.modified()&&right.ready()&&right.canUndo()&&read(model)==firstSaved&&read(fs::modelBackupPath(model))==source);
        check("conflict-preserves-current-scene-and-recovery",read(right.scenePath())==rightScene&&QFileInfo::exists(right.recoveryFile()));
        re.clear();const auto other=out+"/other-editor.fbm";good(right,right.save(other),re);
        check("conflicted-editor-can-save-as",!right.modified()&&read(other)!=firstSaved&&read(model)==firstSaved&&read(fs::modelBackupPath(model))==source);
        // A new Save As destination can appear while the backend prepares bytes.
        move(right,.125,re);const auto race=out+"/appeared-during-backend.fbm";
        const auto accepted=right.save(race);write(race,"external file");idle(right);
        check("session-rechecks-new-save-as-destination",accepted&&!re.isEmpty()&&right.modified()&&read(race)=="external file");
        re.clear();good(right,right.save(other),re);
        good(left,left.save(model),le);check("session-unchanged-save-keeps-backup",read(fs::modelBackupPath(model))==source);

        {
            const auto path=out+"/acknowledgement.fbm";write(path,source);
            ModelSession session;QString message;session.setRecoveryRoot(out+"/ack-recovery");
            session.onError=[&](const QString& text){message=text;};
            good(session,session.load(path),message);move(session,.125,message);
            int installations=0;session.onFileCommitted=[&](const QString&){++installations;};
            auto probe=SaveConfirmationProbe::dropReplies(session,2);
            good(session,session.save(path),message);
            check("lost-ack-retries-real-confirmation",probe->dropped==2&&probe->received==3&&!session.modified());
            check("retry-does-not-rewrite-or-rotate-backup",installations==1&&read(fs::modelBackupPath(path))==source);
            check("retry-cleans-recovery-and-keeps-history",session.canUndo()&&QDir(out+"/ack-recovery").entryList({"*.fsr"},QDir::Files).isEmpty());
            probe->repeat(session);move(session,.125,message);
            check("late-duplicate-cannot-replace-newer-state",probe->received==4&&message.isEmpty()&&session.modified()&&session.ready());
            probe->restore(session);
            QByteArray installed;const auto recovery=session.recoveryFile();const auto recoveryBytes=read(recovery);
            session.onFileCommitted=[&](const QString& file){installed=read(file);write(file,"external replacement before confirmation");};
            const bool accepted=session.save(path);idle(session);
            check("changed-target-cannot-clear-session",accepted&&!message.isEmpty()&&session.modified()&&session.ready()&&session.canUndo());
            check("changed-target-keeps-recovery",read(path)=="external replacement before confirmation"&&read(recovery)==recoveryBytes);
            session.onFileCommitted={};message.clear();write(path,installed);
            good(session,session.save(path),message);
            check("rejected-confirmation-can-be-saved-again",!session.modified()&&session.ready());
        }

#ifdef Q_OS_WIN
        // Kill only this session's own child, after the real destination commit
        // but before mark_saved. No synthetic success or error response.
        move(left,.125,le);const auto recovery=left.recoveryFile();const auto checkpoint=read(recovery);
        const auto lockPath=out+"/recovery/"+left.model()->documentId+".lock";
        bool pidOk=false;const auto pid=QString::fromLatin1(read(lockPath).split('\n').first()).toUInt(&pidOk);
        if(!pidOk||pid==0)throw std::runtime_error("Missing owned service PID");bool killed=false;
        left.onFileCommitted=[&](const QString&){
            HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_TERMINATE|SYNCHRONIZE,FALSE,pid);
            if(!process)return;wchar_t name[32768];DWORD size=32768;
            const bool own=QueryFullProcessImageNameW(process,0,name,&size)&&fs::sameModelFile(QString::fromWCharArray(name,int(size)),QCoreApplication::applicationDirPath()+"/FreeShipDocumentService.exe");
            killed=own&&TerminateProcess(process,31);if(killed)WaitForSingleObject(process,5000);CloseHandle(process);
        };
        left.save(model);idle(left);left.onFileCommitted={};
        check("committed-before-confirmation-interruption",killed&&!le.isEmpty()&&!left.ready()&&left.modified()&&read(model)!=firstSaved);
        check("interruption-keeps-previous-file-and-recovery",read(fs::modelBackupPath(model))==firstSaved&&read(recovery)==checkpoint);
        report["confirmation_error"]=le;le.clear();good(left,left.load(model),le);
        check("reopen-after-confirmation-interruption",left.ready()&&!left.modified()&&read(model)!=firstSaved);
#endif
        check("baseline-input-never-written",read(input)==source);
        report["status"]=failed?"fail":"pass";
    }catch(const std::exception& error){report["status"]="error";report["error"]=QString::fromUtf8(error.what());}
    report["cases"]=cases;write(out+"/report.json",QJsonDocument(report).toJson());return report["status"]=="pass"?0:1;
}
