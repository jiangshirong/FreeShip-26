// SPDX-License-Identifier: GPL-2.0-or-later
// Independent catalogs test fallback, invalid resources and live-caption isolation.
#include "app/ui_language.h"
#include "app/feedback_text.h"
#include <QApplication>
#include <QAction>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QVBoxLayout>

int main(int argc,char** argv){
    QApplication app(argc,argv);if(argc!=2)return 3;
    QDir().mkpath(app.arguments()[1]);QJsonArray cases;bool failed=false;
    auto check=[&](const QString& id,bool ok){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});failed|=!ok;};
    QTemporaryDir fixture;
    auto write=[&](const QString& id,const QJsonObject& messages){
        QFile f(fixture.path()+"/"+id+".json");if(!f.open(QIODevice::WriteOnly))return false;
        return f.write(QJsonDocument(QJsonObject{{"format","FreeShipQtLanguage"},{"version",1},{"id",id},{"name",id},{"messages",messages}}).toJson())>0;
    };
    check("catalogs.created",fixture.isValid()&&write("en",{{"首选项","Preferences"},{"确定","OK"},{"取消","Cancel"},{"结果 %1","Result %1"},{"计算","Calculate"},{QString("menu")+QChar(4)+"计算","Calculations"}})&&write("xx",{{"确定","XX OK"},{"首选项","XX Preferences"}}));
    check("initial.chinese",setUiLanguage("zh",fixture.path()).isEmpty());
    QWidget window;auto* layout=new QVBoxLayout(&window);
    auto* caption=new QLabel(uiText("首选项"));layout->addWidget(caption);
    auto* action=new QAction(uiText("首选项"),&window);window.addAction(action);
    auto* user=new QLineEdit("首选项");layout->addWidget(user);
    auto* userLabel=new QLabel("首选项");userLabel->setProperty("ui_user_text",true);layout->addWidget(userLabel);
    auto* overwritten=new QLabel(uiText("首选项"));layout->addWidget(overwritten);
    auto* scoped=new QLabel;bindUiText(scoped,"text","计算","menu");layout->addWidget(scoped);
    auto* defaultMeaning=new QLabel(uiText("计算"));layout->addWidget(defaultMeaning);
    auto* tabs=new QTabWidget;tabs->addTab(new QWidget,uiText("首选项"));layout->addWidget(tabs);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    window.setAttribute(Qt::WA_DontShowOnScreen);window.show();app.processEvents();bindUiLanguage(&window);
    overwritten->setText("User ship Ж 2026");
    const char* retained=uiText("首选项");
    check("selected.loaded",setUiLanguage("xx").isEmpty()&&uiLanguage()=="xx");
    check("live.static-captions",caption->text()=="XX Preferences"&&action->text()=="XX Preferences"&&tabs->tabText(0)=="XX Preferences");
    check("live.user-text-preserved",user->text()=="首选项"&&userLabel->text()=="首选项"&&overwritten->text()=="User ship Ж 2026");
    check("fallback.existing-english",QString::fromUtf8(uiText("取消"))=="Cancel");
    check("fallback.original-if-no-english",QString::fromUtf8(uiText("尚未翻译"))=="尚未翻译");
    check("fallback.placeholders",QString(uiText("结果 %1")).arg(42)=="Result 42");
    check("scope.english-before-ambiguous-global",scoped->text()=="Calculations"&&defaultMeaning->text()=="Calculate");
    check("scope.direct-fallback",QString(uiText("计算","menu"))=="Calculations");
    check("fragments.preserve-user-and-numeric-values",uiTextParts(QJsonArray{
        QJsonObject{{"source","计算"}},QJsonObject{{"value","首选项 Ж 0.00100"}}})=="Calculate首选项 Ж 0.00100");
    check("fragments.translate-complete-static-paragraphs",uiMetadataText(QJsonObject{{"help","fallback"},{"help_parts",QJsonArray{"首选项","\r\n","取消"}}},"help")=="XX Preferences\r\nCancel");
    check("standard.buttons",buttons->button(QDialogButtonBox::Ok)->text()=="XX OK"&&buttons->button(QDialogButtonBox::Cancel)->text()=="Cancel");
    check("reject.missing",!setUiLanguage("zz").isEmpty()&&uiLanguage()=="xx");
    check("reject.path",!setUiLanguage("../en").isEmpty()&&uiLanguage()=="xx");
    check("reject.placeholder",write("zz",{{"结果 %1","Bad %2"}})&&!setUiLanguage("zz").isEmpty()&&uiLanguage()=="xx");
    QFile bad(fixture.path()+"/zz.json");bad.open(QIODevice::WriteOnly);bad.write("{");bad.close();
    check("reject.corrupt",!setUiLanguage("zz").isEmpty()&&uiLanguage()=="xx");
    bool switches=true;for(int i=0;i<100;++i)switches&=setUiLanguage(i%2?"xx":"en").isEmpty();
    check("repeated-switches.caption-storage",switches&&QByteArray(retained)==QString("首选项").toUtf8()&&caption->text()=="XX Preferences");
    check("return.chinese",setUiLanguage("zh").isEmpty()&&caption->text()=="首选项"&&action->text()=="首选项"&&tabs->tabText(0)=="首选项");
    check("scope.chinese-no-context-id-leak",scoped->text()=="计算"&&defaultMeaning->text()=="计算");
    check("chinese-overrides.created",write("zh",{{"首选项","中文选项"},{"计算","开始计算"},{QString("menu")+QChar(4)+"计算","计算工具"},{"结果 %1","结果值 %1"},{"Native diagnostic","原生诊断"},{"米","错误单位"}}));
    check("chinese-overrides.loaded",setUiLanguage("zh",fixture.path()).isEmpty());
    check("chinese-overrides.live-captions",caption->text()=="中文选项"&&action->text()=="中文选项"&&tabs->tabText(0)=="中文选项");
    check("chinese-overrides.scoped-identity",scoped->text()=="计算工具"&&defaultMeaning->text()=="开始计算");
    check("chinese-overrides.placeholder",QString(uiText("结果 %1")).arg(42)=="结果值 42");
    check("chinese-overrides.English-source",QString(uiText("Native diagnostic"))=="原生诊断");
    check("chinese-overrides.missing-key-falls-back",QString(uiText("内置但尚无覆盖的文字"))=="内置但尚无覆盖的文字");
    check("chinese-overrides.units-invariant",QString(uiText("米"))=="m"&&QString(uiText("度"))=="°");
    check("chinese-overrides.user-data-untouched",user->text()=="首选项"&&userLabel->text()=="首选项"&&overwritten->text()=="User ship Ж 2026");
    int chineseChoices=0;for(const auto& item:uiLanguages())if(item.toObject()["id"]=="zh")++chineseChoices;
    check("chinese-overrides.single-language-choice",chineseChoices==1);
    check("chinese-overrides.switch-away",setUiLanguage("en").isEmpty()&&caption->text()=="Preferences");
    check("chinese-overrides.reject-bad-placeholder",write("zh",{{"结果 %1","错误 %2"}})&&!checkUiLanguage("zh").isEmpty()&&!setUiLanguage("zh").isEmpty()&&uiLanguage()=="en");
    check("chinese-overrides.optional-catalog",QFile::remove(fixture.path()+"/zh.json")&&setUiLanguage("zh").isEmpty()&&caption->text()=="首选项");
    // User's 2026-11-13 rule: omit zero diagnostics, never actual exceptions.
    // These are synthetic formatter cases; real calculated feedback is observed
    // separately by the hydro/report workflows.
    QJsonObject feedback;
    const auto success=hydroFeedbackText(1,0,0,5);
    feedback["zh_success"]=success;
    check("feedback.zh-success",success=="完成 1 个工况，耗时 0.005 s。");
    check("feedback.zh-invalid-retained",hydroFeedbackText(2,1,0,5)=="完成 2 个工况，耗时 0.005 s。\n其中 1 个未得到有效结果。");
    check("feedback.zh-partial-retained",hydroFeedbackText(2,0,1,5)=="完成 2 个工况，耗时 0.005 s。\n其中 1 个工况的部分量未提供（见状态列）。");
    check("feedback.zh-both-retained",hydroFeedbackText(3,1,1,5)=="完成 3 个工况，耗时 0.005 s。\n其中 1 个未得到有效结果。\n其中 1 个工况的部分量未提供（见状态列）。");
    check("feedback.zh-curves",performanceFeedbackText(2,false,5)=="完成 2 条曲线，耗时 0.005 s。");
    check("feedback.zh-groups",performanceFeedbackText(1,true,5)=="完成 1 组数据，耗时 0.005 s。");
    check("feedback.zh-zero-output-omitted",performanceFeedbackText(0,false,5).isEmpty()&&performanceFeedbackText(0,true,5).isEmpty());
    check("feedback.zh-no-empty-completions",hydroFeedbackText(0,0,0,5).isEmpty()&&
          completionText(0,"已展开 %1 块板片。").isEmpty()&&completionText(0,"已导入 %1 项，尚未保存。").isEmpty());
    check("feedback.zh-empty-export-keeps-warning",sectionsFeedbackText(0,0,0,1,"output.txt")=="有 1 个断面未与船体相交。\noutput.txt");
    const auto exported=sectionsFeedbackText(10,42,0,0,"ship.out");feedback["zh_export"]=exported;
    check("feedback.zh-export-clean",exported=="已导出 10 个断面、42 个点。\nship.out");
    check("feedback.zh-cap-only",sectionsFeedbackText(10,42,1,0,{})=="已导出 10 个断面、42 个点。\n已水平封顶 1 个断面。");
    check("feedback.zh-empty-only",sectionsFeedbackText(10,42,0,1,{})=="已导出 10 个断面、42 个点。\n有 1 个断面未与船体相交。");
    check("feedback.zh-export-both",sectionsFeedbackText(10,42,1,2,{})=="已导出 10 个断面、42 个点。\n已水平封顶 1 个断面。\n有 2 个断面未与船体相交。");
    check("feedback.real-english-loaded",setUiLanguage("en",QCoreApplication::applicationDirPath()+"/languages").isEmpty());
    feedback["en_success"]=hydroFeedbackText(1,0,0,5);
    check("feedback.en-success",hydroFeedbackText(1,0,0,5)=="Cases completed: 1. Time: 0.005 s.");
    check("feedback.en-invalid-retained",hydroFeedbackText(2,1,0,5)=="Cases completed: 2. Time: 0.005 s.\nWithout valid results: 1.");
    check("feedback.en-partial-retained",hydroFeedbackText(2,0,1,5)=="Cases completed: 2. Time: 0.005 s.\nCases with missing values: 1 (see Status).");
    check("feedback.en-curves",performanceFeedbackText(1,false,5)=="Curves completed: 1. Time: 0.005 s.");
    check("feedback.en-groups",performanceFeedbackText(1,true,5)=="Data groups completed: 1. Time: 0.005 s.");
    check("feedback.en-zero-output-omitted",performanceFeedbackText(0,false,5).isEmpty()&&performanceFeedbackText(0,true,5).isEmpty());
    check("feedback.en-empty-hydro-keeps-warning",hydroFeedbackText(0,1,0,5)=="Without valid results: 1.");
    check("feedback.en-export-omits-empty-point-count",sectionsFeedbackText(2,0,0,0,{})=="Exported 2 sections.");
    check("feedback.en-export-both",sectionsFeedbackText(10,42,1,2,{})=="Exported 10 sections and 42 points.\nSections capped horizontally: 1.\nSections not intersecting the hull: 2.");
    QFile report(app.arguments()[1]+"/report.json");if(!report.open(QIODevice::WriteOnly))return 3;
    report.write(QJsonDocument(QJsonObject{{"status",failed?"fail":"pass"},{"cases",cases},{"feedback_samples",feedback}}).toJson());return failed?1:0;
}
