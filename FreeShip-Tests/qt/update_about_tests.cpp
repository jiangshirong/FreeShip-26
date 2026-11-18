// SPDX-License-Identifier: GPL-2.0-or-later
// Deterministic real update-controller presentation checks. Reports are injected
// at its normal helper-result boundary; no network, installer or user input.
#include "app/update_controller.h"
#include "app/about_dialog.h"
#include "app/studio_ui.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QProgressBar>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextTable>
#include <QToolButton>
#include <QTimer>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSettings>
#include <QKeyEvent>
#include <QTableWidget>
#include <QVBoxLayout>

int verifyLiveDownload(QApplication& app,const QString& out,bool reopen){
    QWidget owner;
    QAction about(&owner);
    QVBoxLayout layout(&owner);
    auto* button=new QPushButton;
    layout.addWidget(button);
    bool ready=false,showedNotes=false,measuredSpeed=false;
    int step=0,beforePause=-1,paused=-1,exiting=-1,restored=-1;
    QFile state(out+"/retained-progress.json");
    if(reopen&&state.open(QIODevice::ReadOnly)){
        const auto data=QJsonDocument::fromJson(state.readAll()).object();
        exiting=data["exit_percent"].toInt(-1);
        state.close();
    }
    StudioUpdateController controller(&owner,&about,false,[&](std::function<void()>){ready=true;});
    controller.attachButton(button);
    owner.resize(600,220);owner.show();controller.activate();
    QTimer poll;
    QObject::connect(&poll,&QTimer::timeout,&owner,[&]{
        auto* window=owner.findChild<QDialog*>("FreeShipUpdate");
        if(!window){if(button->isVisible())button->click();return;}
        auto* bar=window->findChild<QProgressBar*>("UpdateDownloadBar");
        auto* action=window->findChild<QPushButton*>("UpdateWindowAction");
        auto* rate=window->findChild<QLabel*>("UpdateDownloadRate");
        auto* notes=window->findChild<QTextBrowser*>("UpdateReleaseNotes");
        const auto status=controller.state();
        const int percent=bar->maximum()==100?bar->value():-1;
        if(step==0&&action->isEnabled()&&status==(reopen?"paused":"available")){
            showedNotes=notes->isVisible()&&notes->toPlainText().contains("Added version notes.");
            if(reopen){
                restored=percent;
                window->grab().save(out+"/resumed-after-restart.png");
            }
            step=1;action->click();
        }else if(!reopen&&step==1&&status=="downloading"&&percent>0&&percent<100&&rate->text().contains("/s")){
            measuredSpeed=true;
            beforePause=percent;
            window->grab().save(out+"/live-download-progress.png");
            step=2;action->click();
        }else if(!reopen&&step==2&&status=="paused"&&action->isEnabled()){
            paused=percent;
            window->grab().save(out+"/paused-download.png");
            step=3;action->click();
        }else if(!reopen&&step==3&&status=="downloading"&&percent>paused&&percent<100){
            exiting=percent;
            app.quit();
        }else if(reopen&&step==1&&status=="ready"){
            step=2;
            action->click();
            app.quit();
        }
    });
    poll.start(25);
    QTimer::singleShot(35000,&app,&QCoreApplication::quit);
    app.exec();
    const bool pass=reopen?showedNotes&&exiting>0&&restored>=exiting&&restored<100&&ready:
        showedNotes&&measuredSpeed&&beforePause>0&&paused>=beforePause&&exiting>paused;
    const QJsonObject evidence{{"before_pause",beforePause},{"paused",paused},{"exit_percent",exiting},{"reopened_percent",restored},
        {"state_at_quit",controller.state()},{"last_step",step},
        {"verified_notes",showedNotes},{"reached_install_request",ready}};
    if(!reopen&&state.open(QIODevice::WriteOnly))state.write(QJsonDocument(evidence).toJson());
    QFile report(out+"/report.json");report.open(QIODevice::WriteOnly);
    report.write(QJsonDocument(QJsonObject{{"status",pass?"pass":"fail"},{"cases",QJsonArray{
        QJsonObject{{"id",reopen?"reopened-process-resumes-saved-progress":"same-window-pause-resume-and-exit"},
            {"status",pass?"pass":"fail"},{"observed",evidence}}
    }}}).toJson());
    return pass?0:1;
}

int main(int argc,char** argv){
    QApplication app(argc,argv);app.setProperty("studio_motion_enabled",false);applyStudioTheme();
    const QString out=QString::fromLocal8Bit(argv[argc-1]);QDir().mkpath(out);QJsonArray cases;
    if(app.arguments().contains("--live-download")||app.arguments().contains("--live-reopen"))
        return verifyLiveDownload(app,out,app.arguments().contains("--live-reopen"));
    if(app.arguments().contains("--english")&&!setUiLanguage("en").isEmpty())return 3;
    if(app.arguments().contains("--japanese")&&!setUiLanguage("ja").isEmpty())return 3;
    const auto check=[&](QString id,bool pass,QJsonObject values=QJsonObject{}){cases.append(QJsonObject{{"id",id},{"status",pass?"pass":"fail"},{"observed",values}});};
    for(const auto* asset:{":/brand/freeship26.png",":/ai-vendors/openai.png",":/ai-vendors/deepseek.png",":/ai-vendors/deepl.png",":/ai-vendors/qwen.png"})check(QString("actual-about-asset-")+asset,!QIcon(asset).isNull());
    QWidget owner;owner.resize(1040,600);QAction about(studioIcon("Info"),"About",&owner);
    const auto original=about.icon().pixmap(32,32).toImage();
    bool saveRequested=false;
    StudioUpdateController updates(&owner,&about,true,[&](std::function<void()>){saveRequested=true;});
    updates.setProperty("update_check_only",false);
    QSettings().setValue("updates/automatic",true);
    QElapsedTimer startup;startup.start();updates.schedule();
    while(updates.lastRequestedMode().isEmpty()&&startup.elapsed()<500)app.processEvents();
    check("startup-check-on-first-event-loop",updates.lastRequestedMode()=="--check"&&startup.elapsed()<500,{{"elapsed_ms",startup.elapsed()}});
    auto* layout=new QVBoxLayout(&owner);auto* bar=new StudioBar;QAction menu(studioIcon("Menu"),"Menu",&owner);
    bar->addGroup({&menu,&about},QColor("#edf4f8"));layout->addWidget(bar);
    QToolButton* aboutButton=nullptr;for(auto* button:bar->findChildren<QToolButton*>())if(button->defaultAction()==&about)aboutButton=button;
    auto* first=new QPushButton;layout->addWidget(first);
    updates.attachButton(first);
    const QJsonObject localizedNotes{{"zh",QJsonObject{{"bug_fixes",QJsonArray{"修复报告公式排版。"}},
        {"improvements",QJsonArray{"改善下载进度显示。"}},{"features",QJsonArray{"新增版本更新说明。"}}}},
        {"en",QJsonObject{{"bug_fixes",QJsonArray{"Fixed report formula layout."}},
        {"improvements",QJsonArray{"Improved download status."}},{"features",QJsonArray{"Added version notes."}}}}};
    owner.show();app.processEvents();
    const auto* entryBadge=first->findChild<QWidget*>("AboutUpdateBadge");
    check("unknown-update-status-hides-about-button",first->isHidden()&&entryBadge&&!entryBadge->isVisible());
    const auto before=aboutButton->grab().toImage();
    app.setProperty("studio_motion_enabled",true);
    updates.receiveReport({{"status","available"},{"version","0.9.1"},{"current_version","0.9.0"},
        {"transfer_total_bytes",512000},{"downloaded_bytes",0},{"release_notes",localizedNotes}});
    app.processEvents();
    check("available-update-shows-about-button",first->isVisible()&&first->text()==uiText("更新"));
    check("background-check-does-not-open-window",!owner.findChild<QDialog*>("FreeShipUpdate"));
    const auto badgePoint=QPoint(aboutButton->width()-10,8);
    const auto colorAt=[&](const QImage& image){
        return image.pixelColor(QPointF(badgePoint*image.devicePixelRatio()).toPoint());
    };
    const auto dot=colorAt(aboutButton->grab().toImage());
    const auto entryColor=[&]{
        if(!entryBadge)return QColor{};
        const auto image=first->grab().toImage();
        return image.pixelColor(QPointF(entryBadge->geometry().center()*image.devicePixelRatio()).toPoint());
    };
    const auto entryDot=entryColor();
    QEventLoop breathing;QTimer::singleShot(900,&breathing,&QEventLoop::quit);breathing.exec();
    const auto faded=colorAt(aboutButton->grab().toImage());
    check("about-update-entry-badge-breathes-and-does-not-intercept-clicks",entryBadge&&entryBadge->isVisible()&&
        entryBadge->geometry().top()==4&&first->width()-entryBadge->geometry().right()<=7&&entryDot!=entryColor()&&
        entryBadge->testAttribute(Qt::WA_TransparentForMouseEvents));
    check("badge-pulses-at-button-corner",dot!=colorAt(before)&&dot!=faded&&
        about.property("update_available").toBool()&&about.icon().pixmap(32,32).toImage()==original,
        {{"empty",colorAt(before).name(QColor::HexArgb)},{"initial",dot.name(QColor::HexArgb)},{"faded",faded.name(QColor::HexArgb)}});
    aboutButton->grab().save(out+"/about-toolbar-badge.png");
    app.setProperty("studio_motion_enabled",false);
    first->click();app.processEvents();
    auto* window=owner.findChild<QDialog*>("FreeShipUpdate");
    if(!window)return 3;
    auto* progressBar=window->findChild<QProgressBar*>("UpdateDownloadBar");
    auto* stage=window->findChild<QLabel*>("UpdateDownloadStatus");
    auto* amount=window->findChild<QLabel*>("UpdateDownloadAmount");
    auto* rate=window->findChild<QLabel*>("UpdateDownloadRate");
    auto* eta=window->findChild<QLabel*>("UpdateDownloadEta");
    auto* notes=window->findChild<QTextBrowser*>("UpdateReleaseNotes");
    auto* update=window->findChild<QPushButton*>("UpdateWindowAction");
    check("independent-window-uses-layout-b",window->isWindow()&&window->isVisible()&&progressBar->isVisible()&&
        notes->mapTo(window,QPoint{}).x()<progressBar->mapTo(window,QPoint{}).x());
    check("first-open-is-available-until-download-is-clicked",stage->text()==uiText("有新版可用")&&
        update->text()==uiText("下载更新")&&progressBar->value()==0&&updates.lastRequestedMode()=="--check");
    const auto notesText=notes->toPlainText();
    check("new-version-notes-show-three-ordered-sections",notesText.contains(uiText("Bug 修复"))&&
        notesText.indexOf(uiText("Bug 修复"))<notesText.indexOf(uiText("优化"))&&
        notesText.indexOf(uiText("优化"))<notesText.indexOf(uiText("新增功能"))&&notesText.contains(uiText("新增功能")));
    const auto* target=window->findChild<QLabel*>("UpdateReleaseNotesTitle");
    const auto* current=window->findChild<QLabel*>("UpdateInstalledVersion");
    const auto groupLeft=target->mapTo(window,QPoint{}).x();
    const auto groupRight=current->mapTo(window,QPoint(current->width(),0)).x();
    const auto noteCenter=notes->mapTo(window,QPoint(notes->width()/2,0)).x();
    check("version-badges-centered-above-notes",target->text()=="0.9.1"&&current->text()=="0.9.0"&&
        groupRight>groupLeft&&qAbs((groupLeft+groupRight)/2-noteCenter)<=3&&target->font().pointSizeF()>current->font().pointSizeF());
    const QList<QColor> backgrounds{QColor("#f7ead8"),QColor("#e0eee6"),QColor("#e6eef7")};
    const QStringList headings{uiText("Bug 修复"),uiText("优化"),uiText("新增功能")};
    int styledGroups=0;
    for(auto* frame:notes->document()->rootFrame()->childFrames()){
        const auto* table=qobject_cast<QTextTable*>(frame);
        if(!table||styledGroups==backgrounds.size())break;
        const auto cell=table->cellAt(0,0);
        auto cursor=cell.firstCursorPosition();
        cursor.setPosition(cell.lastCursorPosition().position(),QTextCursor::KeepAnchor);
        const auto text=cursor.selectedText();
        if(cell.format().background().color()==backgrounds[styledGroups]&&text.startsWith(headings[styledGroups])&&
            text.trimmed().size()>headings[styledGroups].size())++styledGroups;
    }
    check("headings-and-entries-share-their-category-background",styledGroups==backgrounds.size(),{{"groups",styledGroups}});
    update->click();
    check("download-is-an-explicit-window-action",updates.lastRequestedMode()=="--download"&&!saveRequested);
    updates.receiveProgress({{"stage","downloading"},{"total_bytes",102400},{"downloaded_bytes",51200},{"bytes_per_second",10240}});
    app.processEvents();
    check("download-displays-amount-speed-time-and-progress",progressBar->value()==50&&amount->text().contains("50")&&
        amount->text().contains("100")&&rate->text().contains("10")&&eta->text().contains("5")&&notes->toPlainText()==notesText);
    window->grab().save(out+"/about-download-progress.png");
    updates.receiveProgress({{"stage","downloading"},{"total_bytes",102400},{"downloaded_bytes",25600},{"bytes_per_second",10240}});
    check("out-of-order-frame-cannot-rewind-progress",progressBar->value()==50);
    update->click();
    updates.receiveReport({{"status","paused"},{"transfer_total_bytes",102400},{"downloaded_bytes",51200}});
    check("paused-state-keeps-progress",stage->text()==uiText("下载已暂停")&&progressBar->value()==50&&
        update->text()==uiText("继续下载")&&rate->text()=="—");
    update->click();
    check("resuming-keeps-progress-before-first-new-frame",progressBar->value()==50);
    updates.receiveProgress({{"stage","verifying"},{"total_bytes",102400},{"downloaded_bytes",102400},{"bytes_per_second",0}});
    check("verification-has-own-label-and-disables-pause",stage->text()==uiText("正在校验并准备安装包…")&&
        progressBar->maximum()==0&&!update->isEnabled());
    updates.receiveReport({{"status","ready"},{"transfer_total_bytes",102400},{"downloaded_bytes",102400}});
    check("waits-for-explicit-install",updates.lastRequestedMode()=="--download"&&update->text()==uiText("重启并安装"));
    update->click();
    check("workflow-never-closes-owner-or-invokes-save-policy",updates.lastRequestedMode()=="--install"&&owner.isVisible()&&!saveRequested);
    updates.receiveReport({{"status","current"},{"version","0.9.1"},{"release_notes",localizedNotes}});
    check("current-version-hides-about-button-and-badge",first->isHidden()&&!about.property("update_available").toBool()&&notes->toPlainText()==notesText);
    check("no-update-hides-button-in-real-about",[&]{
        bool hidden=false;
        QTimer::singleShot(0,&owner,[&]{
            auto* dialog=owner.findChild<QDialog*>("FreeShipAbout");
            if(!dialog)return;
            auto* button=dialog->findChild<QPushButton*>("AboutUpdateLatest");
            hidden=button&&button->isHidden();dialog->accept();
        });
        showFreeShipAbout(&owner);return hidden;
    }());
    updates.receiveReport({{"status","error"},{"error_code","check_timeout"},{"error","原始错误"}});
    check("timeout-is-localized-and-allows-retry",stage->text()==uiText("检查更新超时，请重试。")&&update->isEnabled());
    updates.setProperty("update_check_only",true);updates.activate();
    check("work-copy-never-downloads-or-installs",updates.lastRequestedMode()=="--check"&&!saveRequested);
    updates.setProperty("update_check_only",false);
    QJsonArray longList;
    for(int i=0;i<24;++i)longList.append(QString("A longer release note entry %1 <test>").arg(i+1));
    updates.receiveReport({{"status","available"},{"version","0.9.2"},{"release_notes",QJsonObject{
        {"en",QJsonObject{{"improvements",longList}}}}}});
    app.processEvents();
    check("long-release-notes-scroll-in-left-pane",notes->verticalScrollBar()->maximum()>0&&
        notes->horizontalScrollBar()->maximum()==0&&notes->toPlainText().contains("entry 24 <test>"));
    updates.receiveReport({{"status","available"},{"version","0.9.2"},{"transfer_total_bytes",512000},
        {"downloaded_bytes",0},{"release_notes",localizedNotes}});
    window->close();
    QTimer::singleShot(250,&owner,[&]{
        auto* dialog=owner.findChild<QDialog*>("FreeShipAbout");auto* action=dialog?dialog->findChild<QPushButton*>("AboutUpdateLatest"):nullptr;
        auto* version=dialog?dialog->findChild<QPushButton*>("AboutVersionToggle"):nullptr;
        auto* legal=dialog?dialog->findChild<QPushButton*>("AboutCopyrightToggle"):nullptr;
        check("actual-about-contains-only-update-entry",dialog&&action&&action->text()==uiText("更新")&&
            !dialog->findChild<QTextBrowser*>("UpdateReleaseNotes")&&!dialog->findChild<QProgressBar*>());
        check("actual-about-footer-has-central-update-action",action&&version&&legal&&action->isVisible()&&action->geometry().center().x()>version->geometry().right()&&action->geometry().center().x()<legal->geometry().left());
        check("about-information-buttons-have-no-default-action",version&&legal&&!version->autoDefault()&&!legal->autoDefault()&&!version->isDefault()&&!legal->isDefault());
        check("about-information-buttons-share-one-style",version&&legal&&!version->styleSheet().isEmpty()&&version->styleSheet()==legal->styleSheet());
        if(dialog&&version&&legal){
            if(auto* credits=dialog->findChild<QTableWidget*>("AboutCredits"))credits->setFocus(Qt::OtherFocusReason);
            app.processEvents();version->grab().save(out+"/about-version-normal.png");
            version->setFocus(Qt::TabFocusReason);app.processEvents();
            check("about-version-keeps-keyboard-focus",version->hasFocus()&&!version->isDefault());
            const auto focused=version->grab().toImage();focused.save(out+"/about-version-keyboard-focus.png");
            check("about-keyboard-focus-uses-sea-green-border",focused.pixelColor(focused.width()/2,0).rgb()==QColor("#4d8f7d").rgb());
            QKeyEvent tab(QEvent::KeyPress,Qt::Key_Tab,Qt::NoModifier);QApplication::sendEvent(version,&tab);app.processEvents();
            check("about-tab-navigation-retained",QApplication::focusWidget()!=version);
            if(auto* credits=dialog->findChild<QTableWidget*>("AboutCredits"))credits->setFocus(Qt::OtherFocusReason);
            QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QApplication::sendEvent(dialog,&enter);app.processEvents();
            check("about-enter-without-focused-button-opens-no-information-window",!dialog->findChild<QDialog*>("AboutInformationWindow"));
        }
        if(dialog&&version){
            QTimer::singleShot(100,dialog,[&]{
                auto* info=dialog->findChild<QDialog*>("AboutInformationWindow");
                auto* grid=info?info->findChild<QTableWidget*>("InformationGrid"):nullptr;
                QFile manifest(QCoreApplication::applicationDirPath()+"/build-manifest.json");manifest.open(QIODevice::ReadOnly);
                const auto expected=QJsonDocument::fromJson(manifest.readAll()).object()["release_version"].toString();
                bool git=false,actual=false;if(grid)for(int row=0;row<grid->rowCount();++row){
                    const auto* label=grid->item(row,0);const auto* value=grid->item(row,1);
                    git|=label&&label->text().contains("Git",Qt::CaseInsensitive);
                    actual|=value&&value->text()==expected;
                }
                check("version-shows-packaged-number",grid&&!expected.isEmpty()&&actual,{{"expected_version",expected}});
                check("version-removes-git-rows",grid&&grid->rowCount()==6&&!git);
                if(info){info->grab().save(out+"/version-information.png");info->accept();}
            });version->click();
        }
        if(dialog&&action)action->click();
        else if(dialog)dialog->reject();
    });showFreeShipAbout(&owner);
    app.processEvents();
    check("actual-about-action-opens-independent-window",window->isVisible()&&window->parentWidget()==&owner);
    window->grab().save(out+"/about-update-available.png");
    int failed=0;for(const auto& c:cases)failed+=c.toObject()["status"]=="fail";
    QFile file(out+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(QJsonObject{{"status",failed?"fail":"pass"},{"cases",cases},{"limitations",QJsonArray{"No network, signed-update download, MSI installation or physical desktop interaction is performed by this UI probe."}}}).toJson());
    return failed?1:0;
}
