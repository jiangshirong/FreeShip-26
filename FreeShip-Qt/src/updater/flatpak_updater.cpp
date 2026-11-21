// SPDX-License-Identifier: GPL-2.0-or-later
// This program only asks the host's Flatpak portal to update the calling ref.
// The small HTTPS index supplies user-visible versions; it cannot select a
// download, key, command, app ID or installation target.
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QTimer>
#include <cstdio>

static const QString appId="io.github.jiangshirong.FreeShip26";
static const QString service="org.freedesktop.portal.Flatpak";
static const QString monitor=service+".UpdateMonitor";
static const QString failCheck=QStringLiteral("检查更新失败，请重试");
static const QString failInstall=QStringLiteral("更新安装失败");
static QByteArray encode(const QJsonObject &o){return QJsonDocument(o).toJson(QJsonDocument::Compact);}
static void output(const QJsonObject &o){auto b=encode(o);fwrite(b.constData(),1,b.size(),stdout);fputc('\n',stdout);fflush(stdout);}
static bool isCommit(const QString &s){static const QRegularExpression re("^[a-f0-9]{64}$");return re.match(s).hasMatch();}
static QJsonObject identity(){
    QSettings info("/.flatpak-info",QSettings::IniFormat);
    QFile f(QCoreApplication::applicationDirPath()+"/build-manifest.json");
    if(!f.open(QIODevice::ReadOnly))return {};
    auto m=QJsonDocument::fromJson(f.readAll()).object();
    return {{"app",info.value("Application/name").toString()},{"arch",info.value("Instance/arch").toString()},
            {"branch",info.value("Instance/branch").toString()},{"commit",info.value("Instance/app-commit").toString()},
            {"version",m["release_version"]},{"revision",m["flatpak_revision"]}};
}
static QJsonObject fetch(const QJsonObject &installed){
    const auto branch=installed["branch"].toString();
    if(installed["app"]!=appId||installed["arch"]!="x86_64"||(branch!="stable" && branch!="update-test")||!isCommit(installed["commit"].toString()))return {};
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl("https://updates.freeship26.work/freeship/flatpak/"+branch+"/x86_64/latest.json"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto *reply=manager.get(request);reply->setReadBufferSize(65537);
    QEventLoop loop;QTimer timer;timer.setSingleShot(true);
    QObject::connect(&timer,&QTimer::timeout,reply,&QNetworkReply::abort);
    QObject::connect(reply,&QNetworkReply::readyRead,reply,[reply]{if(reply->bytesAvailable()>65536)reply->abort();});
    QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);timer.start(7500);loop.exec();
    if(reply->error()!=QNetworkReply::NoError||reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200)return {};
    const auto remote=QJsonDocument::fromJson(reply->readAll()).object();
    for(auto k:{"app","arch","branch"})if(remote[k]!=installed[k])return {};
    if(!isCommit(remote["commit"].toString())||remote["revision"].toInteger()<=0||
       !QRegularExpression("^[0-9]+(?:\\.[0-9]+){2,4}$").match(remote["version"].toString()).hasMatch())return {};
    return remote;
}
class Transaction final:public QObject {
    Q_OBJECT
public:
    bool ok=false;
    void run(const QString &parent){
        auto bus=QDBusConnection::sessionBus();
        QDBusInterface portal(service,"/org/freedesktop/portal/Flatpak",service,bus);
        QDBusReply<QDBusObjectPath> handle=portal.call("CreateUpdateMonitor",QVariantMap{});
        if(!handle.isValid()){qWarning("%s",qPrintable(handle.error().message()));return;}
        path=handle.value().path();
        if(!bus.connect(service,path,monitor,"Progress",this,SLOT(progress(QVariantMap))))return;
        QDBusInterface op(service,path,monitor,bus);
        op.setTimeout(600000);
        QDBusPendingCallWatcher response(op.asyncCall("Update",parent,QVariantMap{}));
        connect(&response,&QDBusPendingCallWatcher::finished,this,[this](auto *r){
            if(r->isError()){qWarning("%s",qPrintable(r->error().message()));loop.quit();}
        });
        // Includes waiting for the desktop permission dialog and the transfer.
        QTimer::singleShot(600000,&loop,&QEventLoop::quit);loop.exec();
        op.asyncCall("Close");
    }
private slots:
    void progress(const QVariantMap &m){
        const auto status=m.value("status").toUInt();
        if(!status){
            int n=qMax(1,m.value("n_ops").toInt());
            output({{"percent",qBound(0,(100*m.value("op").toInt()+m.value("progress").toInt())/n,100)}});
        }else{
            ok=status==1||status==2;
            if(!ok)qWarning("%s",qPrintable(m.value("error_message").toString()));
            loop.quit();
        }
    }
private:
    QString path;QEventLoop loop;
};
static bool stagedMatches(const QJsonObject &target){
    // The portal can return Empty (already deployed). Prove what a restart
    // would really run instead of treating a no-op as a successful update.
    QProcess p;p.start("/usr/bin/flatpak-spawn",{"--latest-version","/app/lib/freeship26/FreeShipFlatpakUpdater","--identity"});
    if(!p.waitForFinished(20000)||p.exitStatus()!=QProcess::NormalExit||p.exitCode()!=0)return false;
    auto probed=QJsonDocument::fromJson(p.readAllStandardOutput()).object();
    for(auto k:{"app","arch","branch","commit","revision","version"})if(probed[k]!=target[k])return false;
    return true;
}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    const auto args=app.arguments();const auto mode=args.value(1);
    const auto current=identity();
    if(mode=="--identity"){output(current);return current["app"]==appId?0:1;}
    if((mode!="--check"&&mode!="--download")||args.value(2)!="--report"||args.value(3).isEmpty())return 2;
    auto target=fetch(current);
    QString state="error",error=failCheck;
    if(!target.isEmpty()&&current["revision"].toInteger()>0){
        if(target["commit"]==current["commit"] && target["version"]==current["version"] && target["revision"]==current["revision"]){state="current";error.clear();}
        else if(target["revision"].toInteger()>current["revision"].toInteger()){
            if(mode=="--check"){state="available";error.clear();}
            else {Transaction tx;tx.run(args.value(4)=="--parent-window"?args.value(5):QString{});
                if(tx.ok&&stagedMatches(target)){state="ready";error.clear();}else error=failInstall;}
        }
    }
    QJsonObject result{{"status",state},{"version",target["version"]},{"current_version",current["version"]},
                       {"release_notes",target["release_notes"]},{"error",error}};
    QSaveFile f(args[3]);auto bytes=encode(result);
    if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())return 2;
    return state=="error"?1:0;
}
#include "flatpak_updater.moc"
