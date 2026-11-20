// SPDX-License-Identifier: GPL-2.0-or-later
#include "update_controller.h"
#include "studio_ui.h"
#include "ui_language.h"
#include "core/platform_process.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QList>
#include <QLocale>
#include <QPainter>
#include <QPointer>
#include <QProgressBar>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QUuid>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <cmath>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
QString downloadSize(qint64 bytes){return QLocale().formattedDataSize(bytes,1);}
QString timeRemaining(qint64 seconds){
    if(seconds>=3600)return QString(uiText("%1 小时 %2 分钟")).arg(seconds/3600).arg(seconds%3600/60);
    if(seconds>=60)return QString(uiText("%1 分钟 %2 秒")).arg(seconds/60).arg(seconds%60);
    return QString(uiText("%1 秒")).arg(seconds);
}
QLabel* textLabel(QWidget* parent,const char* name){
    auto* label=new QLabel(parent);
    label->setObjectName(name);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}
void emphasize(QLabel* label,int extraSize=0){
    auto font=label->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF()+extraSize);
    label->setFont(font);
}
void styleUpdateButton(QPushButton* button){
    button->setAutoDefault(false);
    button->setDefault(false);
    button->setMinimumHeight(34);
    button->setStyleSheet(
        "QPushButton{background:#1e6255;color:white;border:1px solid #1e6255;border-radius:5px;padding:6px 14px}"
        "QPushButton:hover{background:#174f45} QPushButton:pressed{background:#123f37}"
        "QPushButton:focus{border:2px solid #448b78}"
        "QPushButton:disabled{background:#dcebe5;color:#315d52;border-color:#c2d8ce}");
}
class UpdateButtonBadge final : public QWidget {
public:
    explicit UpdateButtonBadge(QPushButton* button):QWidget(button){
        setObjectName("AboutUpdateBadge");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        position();
        button->installEventFilter(this);
        pulse_.setDuration(2300);
        pulse_.setLoopCount(-1);
        pulse_.setStartValue(0.0);
        pulse_.setEndValue(1.0);
        connect(&pulse_,&QVariantAnimation::valueChanged,this,[this]{update();});
        show();
    }
private:
    void position(){setGeometry(parentWidget()->width()-15,4,10,10);}
    bool eventFilter(QObject* watched,QEvent* event)override{
        if(event->type()==QEvent::Resize)position();
        return QWidget::eventFilter(watched,event);
    }
    void showEvent(QShowEvent*)override{pulse_.start();}
    void hideEvent(QHideEvent*)override{pulse_.stop();}
    void paintEvent(QPaintEvent*)override{
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setOpacity(.75+.25*std::cos(6.283185307179586*pulse_.currentValue().toDouble()));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#b1ead3"));
        painter.drawEllipse(QRectF(rect()).adjusted(.5,.5,-.5,-.5));
    }
    QVariantAnimation pulse_;
};
QString releaseNotesHtml(const QJsonObject& notes){
    struct Category { const char *key,*heading,*ink,*background; };
    const Category categories[]={
        {"bug_fixes","Bug 修复","#774619","#f7ead8"},
        {"improvements","优化","#205c51","#e0eee6"},
        {"features","新增功能","#2e5070","#e6eef7"}};
    for(const auto& language:{uiLanguage(),QStringLiteral("en"),QStringLiteral("zh")}){
        const auto translated=notes.value(language).toObject();
        QString sections;
        for(const auto& [key,heading,ink,background]:categories){
            QString entries;
            for(const auto& item:translated.value(key).toArray()){
                const auto text=item.toString().trimmed();
                if(!text.isEmpty())entries+="<li>"+text.toHtmlEscaped()+"</li>";
            }
            if(!entries.isEmpty())sections+=QString("<table width='100%' cellpadding='10' cellspacing='0'><tr><td bgcolor='%1'>"
                "<p style='margin:0;color:%2'><b>%3</b></p>"
                "<ul style='margin-top:4px;margin-bottom:0'>%4</ul></td></tr></table>"
                "<p style='margin:0;font-size:5pt'>&nbsp;</p>")
                .arg(QString::fromLatin1(background),QString::fromLatin1(ink),QString(uiText(heading)).toHtmlEscaped(),entries);
        }
        if(!sections.isEmpty())return sections;
    }
    return {};
}
class StudioUpdateDialog final:public QDialog {
public:
    QLabel *title,*installed,*status,*percent,*amount,*rate,*eta;
    QWidget* installedGroup;
    QProgressBar* bar;
    QTextBrowser* notes;
    QPushButton* action;

    explicit StudioUpdateDialog(QWidget* owner):QDialog(owner){
        setObjectName("FreeShipUpdate");
        bindUiText(this,"windowTitle","更新");
        setStudioWindowIcon(this,"ImportFile");
        setAttribute(Qt::WA_DontShowOnScreen,owner->testAttribute(Qt::WA_DontShowOnScreen));
        setMinimumSize(690,355);
        resize(846,436);
        setStyleSheet(
            "QDialog#FreeShipUpdate{background:#f7f8f4;color:#213b36}"
            "QDialog#FreeShipUpdate QLabel{color:#213b36}"
            "QDialog#FreeShipUpdate QLabel#UpdateReleaseNotesTitle{background:#dceee4;color:#164c40;border:1px solid #c0dccd;border-radius:7px;padding:4px 12px}"
            "QDialog#FreeShipUpdate QLabel#UpdateInstalledVersion{background:#f0f4ef;color:#425b50;border:1px solid #d7e2d9;border-radius:5px;padding:3px 8px}"
            "QFrame#UpdateTransfer{background:#edf4ee;border:1px solid #d2e0d7;border-radius:6px}"
            "QTextBrowser#UpdateReleaseNotes{background:#fcfdfa;color:#213b36;border:1px solid #d2e0d7;border-radius:5px}"
            "QProgressBar#UpdateDownloadBar{background:#dce9df;border:0;border-radius:4px}"
            "QProgressBar#UpdateDownloadBar::chunk{background:#007e74;border-radius:4px}");
        auto* outer=new QVBoxLayout(this);
        outer->setContentsMargins(22,18,22,20);
        outer->setSpacing(14);
        auto* columns=new QHBoxLayout;
        columns->setSpacing(18);
        auto* left=new QVBoxLayout;
        auto* header=new QHBoxLayout;
        header->setSpacing(9);
        header->addStretch();
        title=textLabel(this,"UpdateReleaseNotesTitle");emphasize(title,6);
        title->setWordWrap(false);
        header->addWidget(title,0,Qt::AlignVCenter);
        installedGroup=new QWidget(this);
        auto* current=new QHBoxLayout(installedGroup);
        current->setContentsMargins(0,0,0,0);
        current->setSpacing(6);
        auto* currentLabel=new QLabel(installedGroup);
        bindUiText(currentLabel,"text","当前版本");
        current->addWidget(currentLabel);
        installed=textLabel(installedGroup,"UpdateInstalledVersion");
        installed->setWordWrap(false);
        current->addWidget(installed);
        header->addWidget(installedGroup,0,Qt::AlignVCenter);
        header->addStretch();
        left->addLayout(header);
        notes=new QTextBrowser(this);
        notes->setObjectName("UpdateReleaseNotes");
        bindUiText(notes,"accessibleName","更新内容");
        notes->setOpenLinks(false);
        notes->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        notes->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        notes->document()->setDocumentMargin(12);
        left->addWidget(notes,1);
        columns->addLayout(left,7);

        auto* right=new QVBoxLayout;
        right->setSpacing(10);
        auto* card=new QFrame(this);card->setObjectName("UpdateTransfer");
        auto* cardLayout=new QVBoxLayout(card);cardLayout->setContentsMargins(12,11,12,12);cardLayout->setSpacing(8);
        auto* statusRow=new QHBoxLayout;
        status=textLabel(card,"UpdateDownloadStatus");emphasize(status);
        percent=textLabel(card,"UpdateDownloadPercent");emphasize(percent,5);
        statusRow->addWidget(status,1);statusRow->addWidget(percent);
        cardLayout->addLayout(statusRow);
        bar=new QProgressBar(card);bar->setObjectName("UpdateDownloadBar");
        bindUiText(bar,"accessibleName","更新下载进度");
        bar->setFixedHeight(8);bar->setTextVisible(false);
        cardLayout->addWidget(bar);
        const auto stat=[&](const char* heading,const char* name){
            auto* row=new QHBoxLayout;
            auto* label=new QLabel(card);
            bindUiText(label,"text",QString::fromUtf8(heading));
            auto* value=textLabel(card,name);value->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
            row->addWidget(label);row->addWidget(value,1);
            cardLayout->addLayout(row);
            return value;
        };
        amount=stat("已下载 / 总量","UpdateDownloadAmount");
        rate=stat("下载速度","UpdateDownloadRate");
        eta=stat("预计剩余","UpdateDownloadEta");
        right->addWidget(card);
        action=new QPushButton(this);action->setObjectName("UpdateWindowAction");styleUpdateButton(action);
        right->addWidget(action,0,Qt::AlignRight);right->addStretch();
        columns->addLayout(right,5);
        outer->addLayout(columns,1);
    }
};
}
struct StudioUpdateController::Impl {
    StudioUpdateController* self;
    QPointer<QWidget> owner;
    QPointer<QAction> about;
    QPointer<StudioUpdateDialog> window;
    QPointer<QProcess> process;
    QList<QPointer<QPushButton>> entryButtons;
    std::function<void(std::function<void()>)> save;
    QString state,helper,report,version,installedVersion,errorSource,lastMode,progressStage,notesHtml;
    QJsonObject releaseNotes;
    bool workflow=false;
    qint64 total=-1,downloaded=0,speed=0;
#ifdef Q_OS_WIN
    HANDLE pauseEvent=nullptr;
#endif

    QString work()const{return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/FreeShip26/UpdateUI";}
    bool canCheck()const{return workflow||QFile::exists(helper);}
    bool checkOnly()const{return self->property("update_check_only").toBool();}
    bool hasUpdate()const{return state=="available"||state=="ready"||state=="downloading"||state=="paused"||state=="pausing";}
    void syncProgress(){
        if(!window)return;
        const bool checking=lastMode=="--check"&&process;
        const bool verifying=state=="downloading"&&progressStage=="verifying";
        QString label;
        if(checking)label=uiText("正在检查更新…");
        else if(state=="pausing")label=uiText("正在暂停下载…");
        else if(state=="paused")label=uiText("下载已暂停");
        else if(state=="ready")label=uiText("下载完成");
        else if(state=="current")label=uiText("已是最新版本");
        else if(state=="available")label=uiText("有新版可用");
        else if(state=="downloading")label=verifying?uiText("正在校验并准备安装包…"):
            progressStage=="preparing"?uiText("正在计算所需下载量…"):uiText("正在下载更新…");
        else if(state=="not_configured")label=uiText("更新服务不可用");
        const QString missingPkexec=QStringLiteral("系统未安装 pkexec，无法请求管理员权限；请手动安装下载的更新包：");
        if(errorSource.startsWith(missingPkexec))
            label=uiText(missingPkexec+"%1").arg(errorSource.mid(missingPkexec.size()));
        else if(!errorSource.isEmpty())label=uiText(errorSource);
        window->status->setText(label);
        const int value=state=="ready"?100:total>0?int(downloaded*100/total):0;
        window->bar->setRange(0,(state=="downloading"&&total<0)||verifying?0:100);
        window->bar->setValue(value);
        window->bar->setVisible(!checkOnly());
        window->percent->setText(!checkOnly()&&total>=0?QString("%1%").arg(value):QString{});
        window->amount->setText(!checkOnly()&&total>=0?downloadSize(downloaded)+" / "+downloadSize(total):"—");
        window->rate->setText(speed>0&&state=="downloading"?QString(uiText("%1/s")).arg(downloadSize(speed)):QStringLiteral("—"));
        window->eta->setText(speed>0&&state=="downloading"&&downloaded<total?
            timeRemaining((total-downloaded+speed-1)/speed):QStringLiteral("—"));
        const char* action="重新检查";
        if(!checkOnly()){
            if(state=="available")action="下载更新";
            else if(state=="downloading"||state=="pausing")action="暂停";
            else if(state=="paused")action="继续下载";
            else if(state=="ready")action="重启并安装";
        }
        window->action->setText(uiText(action));
        window->action->setEnabled(canCheck()&&!checking&&state!="pausing"&&!verifying&&(!process||state=="downloading"));
    }
    void sync(){
        if(about){
            about->setProperty("update_state",state);
            if(about->property("update_available").toBool()!=hasUpdate()){
                about->setProperty("update_available",hasUpdate());
                for(auto* object:about->associatedObjects())if(auto* widget=qobject_cast<QWidget*>(object))widget->update();
            }
        }
        entryButtons.removeIf([](const auto& button){return button.isNull();});
        for(const auto& button:entryButtons)button->setVisible(canCheck()&&hasUpdate());
        if(!window)return;
        window->title->setText(version);
        window->title->setVisible(!version.isEmpty());
        window->installed->setText(installedVersion);
        window->installedGroup->setVisible(!installedVersion.isEmpty());
        auto html=releaseNotesHtml(releaseNotes);
        if(html.isEmpty()&&(hasUpdate()||state=="current"))html=QString(uiText("此版本没有附带更新说明。")).toHtmlEscaped();
        if(html!=notesHtml){notesHtml=html;window->notes->setHtml(html);}
        syncProgress();
    }
    void pause(){
        if(state!="downloading")return;
        state="pausing";speed=0;sync();
#ifdef Q_OS_WIN
        if(pauseEvent)SetEvent(pauseEvent);
#else
        if(process)process->terminate(); // The Linux helper reports "paused" on SIGTERM.
#endif
    }
    void showWindow(){
        if(!owner)return;
        if(!window){
            window=new StudioUpdateDialog(owner);
            QObject::connect(window->action,&QPushButton::clicked,self,&StudioUpdateController::activate);
            QObject::connect(window,&QDialog::finished,self,[this]{pause();});
        }
        window->show();window->raise();window->activateWindow();
        if((state.isEmpty()||state=="error"||state=="not_configured")&&!process)run("--check");
        else sync();
    }
    void run(const QString& mode){
        if(process)return;
        lastMode=mode;self->setProperty("update_requested_mode",mode);
        errorSource.clear();
        if(mode=="--download"){state="downloading";speed=0;progressStage="preparing";}
        if(workflow){sync();return;}
        if(!QFile::exists(helper)){state="not_configured";sync();return;}
        QDir().mkpath(work());report=work()+"/result-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".json";
        QStringList args{mode,"--report",report};
#ifdef Q_OS_WIN
        if(mode=="--download"){
            const QString name="Local\\FreeShip26-Pause-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
            pauseEvent=CreateEventW(nullptr,TRUE,FALSE,reinterpret_cast<LPCWSTR>(name.utf16()));
            if(!pauseEvent){self->receiveReport({{"status","error"},{"error",QStringLiteral("无法启动更新程序")}});return;}
            args<<"--pause-event"<<name;
        }
#endif
        process=new QProcess(self);auto* running=process.data();
#ifdef Q_OS_WIN
        running->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments){arguments->flags|=CREATE_NO_WINDOW;});
#endif
        const auto readProgress=[this,running,mode]{
            if(mode!="--download"){running->readAllStandardOutput();return;}
            while(running->canReadLine())self->receiveProgress(QJsonDocument::fromJson(running->readLine(4096)).object());
        };
        QObject::connect(running,&QProcess::readyReadStandardOutput,self,readProgress);
        const auto done=[this,running,readProgress]{
            if(process!=running)return;
            readProgress();
#ifdef Q_OS_WIN
            if(pauseEvent){CloseHandle(pauseEvent);pauseEvent=nullptr;}
#endif
            QFile file(report);QJsonObject result;
            if(file.open(QIODevice::ReadOnly))result=QJsonDocument::fromJson(file.readAll()).object();
            file.close();QFile::remove(report);
            if(result.isEmpty())result={{"status",state=="pausing"?"paused":"error"},
                {"error_code",running->property("update_check_timed_out").toBool()?"check_timeout":""},
                {"error",QStringLiteral("检查更新失败，请重试")}};
            process=nullptr;running->deleteLater();self->receiveReport(result);
        };
        QObject::connect(running,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),self,[done](int,QProcess::ExitStatus){done();});
        QObject::connect(running,&QProcess::errorOccurred,self,[done](QProcess::ProcessError e){if(e==QProcess::FailedToStart)done();});
        running->start(helper,args);
        if(mode=="--check")QTimer::singleShot(10000,running,[this,running]{
            if(process!=running||running->state()==QProcess::NotRunning)return;
            running->setProperty("update_check_timed_out",true);
            running->kill();
        });
        sync();
    }
    void install(){
        lastMode="--install";self->setProperty("update_requested_mode",lastMode);
        if(workflow||!owner||checkOnly())return;
        const QPointer<StudioUpdateController> alive(self);
        save([this,alive]{
            if(!alive||!owner)return;
#ifdef Q_OS_WIN
            QDir().mkpath(work());const auto runner=work()+"/updater-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".exe";
            if(!QFile::copy(helper,runner)){errorSource=QStringLiteral("无法准备更新程序");showWindow();return;}
#else
            // Run in place: it needs the bundled Qt libraries; dpkg may replace the files it has open.
            const auto runner=helper;
#endif
            if(window)window->hide();
            if(!owner->close()){if(runner!=helper)QFile::remove(runner);showWindow();return;}
            if(!QProcess::startDetached(runner,{"--install","--parent",QString::number(QCoreApplication::applicationPid()),"--report",work()+"/install-result.json"})){
                owner->show();if(runner!=helper)QFile::remove(runner);errorSource=QStringLiteral("无法启动更新程序");showWindow();
            }
        });
    }
};
StudioUpdateController::StudioUpdateController(QWidget* owner,QAction* about,bool workflow,std::function<void(std::function<void()>)> save):QObject(owner),impl_(std::make_unique<Impl>()){
    setObjectName("StudioUpdateController");impl_->self=this;impl_->owner=owner;impl_->about=about;impl_->workflow=workflow;impl_->save=std::move(save);
    QFile manifest(QCoreApplication::applicationDirPath()+"/build-manifest.json");
    if(manifest.open(QIODevice::ReadOnly)){
        const auto data=QJsonDocument::fromJson(manifest.readAll()).object();
        setProperty("update_check_only",data["local_installation_role"].toString().startsWith("development;"));
        impl_->installedVersion=data["release_version"].toString();
    }
    impl_->helper=fs::platform::siblingExecutable("FreeShipUpdater");impl_->sync();
    // The signed report already contains all available locales. Re-render it
    // and the dynamic transfer labels without touching the running updater.
    bindUiRefresh(this,[this]{if(impl_->window)impl_->sync();});
}
StudioUpdateController::~StudioUpdateController(){
    if(auto* child=impl_->process.data()){
        child->disconnect(this);
#ifdef Q_OS_WIN
        if(impl_->pauseEvent){SetEvent(impl_->pauseEvent);child->waitForFinished(1500);CloseHandle(impl_->pauseEvent);}
#endif
        if(child->state()!=QProcess::NotRunning){child->kill();child->waitForFinished(1500);}
        QFile::remove(impl_->report);
    }
}
QString StudioUpdateController::state()const{return impl_->state;}
QString StudioUpdateController::lastRequestedMode()const{return impl_->lastMode;}
void StudioUpdateController::attachButton(QPushButton* button){
    if(!button)return;
    button->setObjectName("AboutUpdateLatest");
    button->setText(uiText("更新"));
    button->setProperty("operation","update_to_latest");
    button->setMinimumSize(220,40);
    styleUpdateButton(button);
    QIcon icon;
    for(int size:{16,20,24,32}){
        auto mark=studioIcon("ImportFile").pixmap(size,size);
        QPainter painter(&mark);painter.setCompositionMode(QPainter::CompositionMode_SourceIn);painter.fillRect(mark.rect(),Qt::white);painter.end();
        icon.addPixmap(mark);
    }
    button->setIcon(icon);
    new UpdateButtonBadge(button);
    impl_->entryButtons.append(button);
    button->setVisible(impl_->canCheck()&&impl_->hasUpdate());
    connect(button,&QPushButton::clicked,this,[this,button]{
        if(auto* about=qobject_cast<QDialog*>(button->window());about&&about->objectName()=="FreeShipAbout")about->accept();
        QTimer::singleShot(0,this,[this]{impl_->showWindow();});
    });
}
void StudioUpdateController::receiveProgress(const QJsonObject& progress){
    if(impl_->state!="downloading"&&impl_->state!="pausing")return;
    const auto stage=progress.value("stage").toString();
    if(stage!="downloading"&&stage!="verifying")return;
    const auto total=progress.value("total_bytes").toInteger(-1);
    const auto transferred=progress.value("downloaded_bytes").toInteger(-1);
    const auto rate=progress.value("bytes_per_second").toInteger(-1);
    constexpr qint64 limit=1LL<<40;
    if(total<0||total>limit||transferred<0||transferred>total||rate<0||rate>limit)return;
    if(impl_->progressStage=="verifying"||
        (impl_->progressStage=="downloading"&&(total!=impl_->total||transferred<impl_->downloaded)))return;
    impl_->total=total;impl_->downloaded=transferred;impl_->speed=rate;impl_->progressStage=stage;
    impl_->syncProgress();
}
void StudioUpdateController::receiveReport(const QJsonObject& result){
    auto received=result.value("status").toString("error");
    const auto version=result.value("version").toString(impl_->version);
    const bool changed=version!=impl_->version;
    if(changed||received=="available"||received=="current"||received=="not_configured"||result.contains("release_notes"))
        impl_->releaseNotes=result.value("release_notes").toObject();
    impl_->version=version;impl_->installedVersion=result.value("current_version").toString(impl_->installedVersion);
    impl_->errorSource=result.value("error_code")=="check_timeout"?QStringLiteral("检查更新超时，请重试。"):result.value("error").toString();
    if(changed||received=="current"||received=="not_configured"){impl_->total=-1;impl_->downloaded=0;}
    if(const auto total=result.value("transfer_total_bytes").toInteger(-1), retained=result.value("downloaded_bytes").toInteger(-1);
       total>=0&&total<=(1LL<<40)&&retained>=0&&retained<=total){
        impl_->total=total;impl_->downloaded=retained;
    }
    if(received=="error"&&impl_->hasUpdate())
        received=impl_->state=="ready"?"ready":impl_->downloaded>0?"paused":"available";
    impl_->state=received;impl_->speed=0;impl_->progressStage.clear();impl_->sync();
}
void StudioUpdateController::activate(){
    if(property("update_check_only").toBool()){impl_->run("--check");return;}
    if(impl_->state=="downloading")impl_->pause();
    else if(impl_->state=="ready")impl_->install();
    else if(impl_->state=="available"||impl_->state=="paused")impl_->run("--download");
    else if(impl_->state!="pausing")impl_->run("--check");
}
void StudioUpdateController::schedule(){
    // Document windows share the update channel and its working directory.
    static QPointer<StudioUpdateController> scheduled;
    auto primary=[this]{if(!scheduled)scheduled=this;return scheduled==this;};
    auto check=[this,primary]{if(primary()&&QSettings().value("updates/automatic",true).toBool())impl_->run("--check");};
    QTimer::singleShot(0,this,check);
    if(impl_->workflow)return;
    QTimer::singleShot(1000,this,[this,primary]{
        if(!primary())return;
        QFile last(impl_->work()+"/install-result.json");if(last.open(QIODevice::ReadOnly)){
            const auto result=QJsonDocument::fromJson(last.readAll()).object();last.close();
            if(result.value("status").toString()=="install_failed"||result.value("status").toString()=="error"){
                impl_->state="ready";impl_->errorSource=result.value("error").toString("更新安装失败");impl_->sync();
            }
            QFile::remove(impl_->work()+"/previous-install-result.json");last.rename(impl_->work()+"/previous-install-result.json");
        }
        const QRegularExpression owned("^updater-[0-9a-f-]{36}\\.exe$");
        for(const auto& name:QDir(impl_->work()).entryList({"updater-*.exe"},QDir::Files))if(owned.match(name).hasMatch())QFile::remove(impl_->work()+"/"+name);
    });
    auto* repeat=new QTimer(this);repeat->setInterval(6*60*60*1000);
    connect(repeat,&QTimer::timeout,this,check);repeat->start();
}
void installStudioUpdates(QWidget* owner,QAction* about,bool workflow,const std::function<void(std::function<void()>)>& save){
    about->setProperty("studio_update_badge",true);
    auto* controller=new StudioUpdateController(owner,about,workflow,save);
#if defined(Q_OS_WIN)||defined(Q_OS_LINUX)
    controller->schedule();
#endif
}
void bindAboutUpdateButton(QWidget* owner,QPushButton* button){
    button->hide();
    if(!owner)return;
    for(auto* child:owner->children())if(auto* controller=dynamic_cast<StudioUpdateController*>(child)){controller->attachButton(button);return;}
}
