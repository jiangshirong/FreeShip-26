// SPDX-License-Identifier: GPL-2.0-or-later
#include "document_launch.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#ifdef Q_OS_LINUX
#include <cerrno>
#include <sys/file.h>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
QString sessionKey(){
    QString session=qEnvironmentVariable("WAYLAND_DISPLAY")+'|'+qEnvironmentVariable("DISPLAY");
#ifdef Q_OS_WIN
    DWORD id=0; ProcessIdToSessionId(GetCurrentProcessId(),&id);
    wchar_t desktop[256]={}; DWORD length=0;
    GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktop,sizeof desktop,&length);
    session=QString::number(id)+'|'+QString::fromWCharArray(desktop);
#endif
    auto exe=QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
#ifdef Q_OS_WIN
    exe=exe.toCaseFolded();
#endif
    const auto profile=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return "fs26-open-"+QString::fromLatin1(QCryptographicHash::hash((exe+'|'+profile+'|'+session).toUtf8(),
        QCryptographicHash::Sha256).toHex().left(32));
}
constexpr int maxBytes=1024*1024;
}

DocumentLaunch::Result DocumentLaunch::start(const QStringList& paths){
    auto key=sessionKey();
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    if(!dir.mkpath(".")){error=dir.absolutePath();return Failed;}
#ifdef Q_OS_LINUX
    if(QFileInfo::exists("/.flatpak-info")){
        // /tmp is private to an instance. This Flatpak-provided directory is
        // shared between launches of this app by the same user.
        QDir shared(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)+"/app/io.github.jiangshirong.FreeShip26");
        if(!shared.exists()){error=shared.absolutePath();return Failed;}
        key=shared.filePath(key);
        sandboxLock_.setFileName(key+".lock");
        if(!sandboxLock_.open(QIODevice::ReadWrite)){error=sandboxLock_.errorString();return Failed;}
    }else
#endif
    {
        owner_=std::make_unique<QLockFile>(dir.filePath(key+".lock"));
        owner_->setStaleLockTime(0);
    }
    auto tryOwn=[&]{
#ifdef Q_OS_LINUX
        if(sandboxLock_.isOpen()){
            if(flock(sandboxLock_.handle(),LOCK_EX|LOCK_NB)==0)return true;
            if(errno!=EWOULDBLOCK&&errno!=EINTR)error=sandboxLock_.fileName();
            return false;
        }
#endif
        if(owner_->tryLock(0))return true;
        if(owner_->error()!=QLockFile::LockFailedError)error=dir.absolutePath();
        return false;
    };
    QElapsedTimer clock;clock.start();
    while(!tryOwn()){
        if(!error.isEmpty())return Failed;
        QLocalSocket socket;socket.connectToServer(key);
        if(socket.waitForConnected(150)){
#ifdef Q_OS_WIN
            ULONG pid=0;
            if(GetNamedPipeServerProcessId(reinterpret_cast<HANDLE>(socket.socketDescriptor()),&pid))AllowSetForegroundWindow(pid);
#endif
            QStringList absolute;for(const auto& p:paths)absolute.append(QFileInfo(p).absoluteFilePath());
            const auto message=QJsonDocument(QJsonArray::fromStringList(absolute)).toJson(QJsonDocument::Compact)+'\n';
            if(message.size()<=maxBytes && socket.write(message)==message.size() &&
                (!socket.bytesToWrite()||socket.waitForBytesWritten(5000)) &&
                (socket.bytesAvailable()||socket.waitForReadyRead(30000)) && socket.read(1)=="1")return Forwarded;
            error=socket.errorString();return Failed;
        }
        if(clock.elapsed()>5000){error=socket.errorString();return Failed;}
        QThread::msleep(25);
    }
    QLocalServer::removeServer(key); // Only the elected owner can remove a stale endpoint.
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    if(!server_.listen(key)){error=server_.errorString();return Failed;}
    QObject::connect(&server_,&QLocalServer::newConnection,&server_,[this]{
        while(auto* s=server_.nextPendingConnection()){
            s->setReadBufferSize(maxBytes+1);
            QObject::connect(s,&QLocalSocket::disconnected,s,&QObject::deleteLater);
            QTimer::singleShot(5000,s,[s]{s->disconnectFromServer();});
            auto receive=[this,s]{
                if(s->bytesAvailable()>maxBytes){s->abort();return;}
                if(!s->canReadLine())return;
                const auto doc=QJsonDocument::fromJson(s->readLine(maxBytes));
                if(!doc.isArray()){s->abort();return;}
                QStringList paths;
                for(const auto& value:doc.array()){
                    const auto path=value.toString();
                    if(!QDir::isAbsolutePath(path)){s->abort();return;}
                    paths.append(path);
                }
                s->write("1",1);s->disconnectFromServer();
                if(open_)open_(paths);else pending_.append(paths);
            };
            QObject::connect(s,&QLocalSocket::readyRead,s,receive);receive();
        }
    });
    return Primary;
}
void DocumentLaunch::ready(std::function<void(const QStringList&)> open){
    open_=std::move(open);
    const auto pending=std::move(pending_);
    for(const auto& paths:pending)open_(paths);
}
