#include "app/ui_language.h"
#include "core/platform_process.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "grid_preview_panel.h"
#include "model_session.h"
#include "studio_ui.h"
#include "render/grid_preview.h"
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QSaveFile>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <stdexcept>
GridPreviewPanel::GridPreviewPanel(ModelSession& session,QString format,std::function<QJsonObject()> options,QWidget* parent):QWidget(parent),session_(session),format_(std::move(format)),options_(std::move(options)){
    setProperty("offset_preview_panel",true);auto* layout=new QVBoxLayout(this);layout->setContentsMargins(6,6,6,6);auto* bar=new QHBoxLayout;
    auto* projection=new QComboBox;projection->setProperty("offset_preview_projection",true);addUiItems(projection,{"侧视","横剖视图","俯视","三维"});projection->setCurrentIndex(3);bar->addWidget(projection);
    auto* fit=new QPushButton(studioIcon("ZoomExtents"),uiText("全图"));fit->setProperty("offset_preview_action","fit");bar->addWidget(fit);bar->addStretch();
    refresh_=new QPushButton(uiText("更新预览"));refresh_->setProperty("offset_preview_action","refresh");bar->addWidget(refresh_);stop_=new QPushButton(uiText("取消"));stop_->setProperty("offset_preview_action","cancel");stop_->setEnabled(false);bar->addWidget(stop_);layout->addLayout(bar);
    view_=new GridPreviewView;layout->addWidget(view_,1);status_=new StudioMessage(StudioMessageRole::Feedback);status_->setObjectName("GridPreviewStatus");layout->addWidget(status_);
    connect(projection,qOverload<int>(&QComboBox::currentIndexChanged),view_,&GridPreviewView::setProjection);connect(fit,&QPushButton::clicked,view_,&GridPreviewView::fit);
    view_->onOrbit=[projection]{QSignalBlocker blocker(projection);projection->setCurrentIndex(3);};
    connect(refresh_,&QPushButton::clicked,this,[this]{invalidate();requestNow();});connect(stop_,&QPushButton::clicked,this,&GridPreviewPanel::cancel);
    delay_.setSingleShot(true);delay_.setInterval(350);connect(&delay_,&QTimer::timeout,this,&GridPreviewPanel::requestNow);
    timeout_.setSingleShot(true);timeout_.setInterval(90000);connect(&timeout_,&QTimer::timeout,this,[this]{cancel();fail(QStringLiteral("预览生成超时。"));});
    connect(&process_,&QProcess::finished,this,[this](int code,QProcess::ExitStatus status){finish(status==QProcess::NormalExit?code:-1);});
    connect(&process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart)finish(-1);});
}
GridPreviewPanel::~GridPreviewPanel(){delay_.stop();timeout_.stop();process_.disconnect(this);if(process_.state()!=QProcess::NotRunning){process_.kill();process_.waitForFinished(2000);}}
void GridPreviewPanel::showEvent(QShowEvent* e){QWidget::showEvent(e);if(dirty_)delay_.start();}
void GridPreviewPanel::changeEvent(QEvent* e){QWidget::changeEvent(e);if(e->type()==QEvent::LanguageChange&&!errorSource_.isEmpty())refreshError();}
void GridPreviewPanel::invalidate(){++generation_;dirty_=true;current_=false;errorSource_.clear();status_->setMessage(data_.isEmpty()?uiText("正在准备型值预览…"):uiText("参数已更改，所示预览待更新…"),data_.isEmpty()?StudioMessageRole::Feedback:StudioMessageRole::Warning);if(isVisible())delay_.start();}
void GridPreviewPanel::cancel(){errorSource_.clear();++generation_;dirty_=false;current_=false;delay_.stop();timeout_.stop();if(running_)process_.kill();status_->setMessage(uiText("预览已取消。"),StudioMessageRole::Feedback);stop_->setEnabled(false);}
QByteArray GridPreviewPanel::key()const{if(!session_.ready())throw std::runtime_error(QStringLiteral("请先打开船模。").toStdString());return QJsonDocument(QJsonObject{{"format",format_},{"document_id",session_.model()->documentId},{"revision",qint64(session_.model()->revision)},{"options",options_()}}).toJson(QJsonDocument::Compact);}
void GridPreviewPanel::requestNow(){
    delay_.stop();if(!isVisible())return;
    if(running_||session_.busy()){if(dirty_)delay_.start(100);return;}
    try{auto requested=key();if(requested==completedKey_&&!data_.isEmpty()){dirty_=false;current_=true;status_->setMessage(uiText("预览已是最新。"),StudioMessageRole::Feedback);return;}
        work_=std::make_unique<QTemporaryDir>(QDir::tempPath()+"/FreeShip26-grid-preview-XXXXXX");if(!work_->isValid())throw std::runtime_error(QStringLiteral("无法建立预览临时目录。").toStdString());
        const auto id=session_.model()->documentId;const auto revision=session_.model()->revision;
        QJsonObject request{{"format",format_},{"references",options_()},{"model",session_.modelPath()},{"input",QFileInfo(session_.modelPath()).dir().filePath("native-input.json")},{"document_id",id},{"revision",qint64(revision)},{"units","m"},{"close_deck",false},{"preview",true},{"output",work_->filePath("input.txt")}};
        QSaveFile file(work_->filePath("request.json"));const auto bytes=QJsonDocument(request).toJson();if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())throw std::runtime_error(QStringLiteral("无法准备型值预览。").toStdString());
        launchedKey_=requested;launched_=generation_;++launchCount_;dirty_=false;running_=true;current_=false;errorSource_.clear();
        process_.setProgram(fs::platform::siblingExecutable("FreeShipGeometryExport"));process_.setArguments({file.fileName(),work_->filePath("report.json"),QString::number(QCoreApplication::applicationPid())});process_.setStandardOutputFile(QProcess::nullDevice());process_.setStandardErrorFile(work_->filePath("worker.log"));
#ifdef Q_OS_WIN
        process_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* a){a->flags|=0x08000000;});
#endif
        status_->setMessage(uiText("正在生成预览…"),StudioMessageRole::Feedback,true);stop_->setEnabled(true);timeout_.start();process_.start();
    }catch(const std::exception& e){dirty_=false;fail(QString::fromUtf8(e.what()));}
}
void GridPreviewPanel::finish(int code){
    if(!running_)return;timeout_.stop();running_=false;stop_->setEnabled(false);
    if(launched_!=generation_){if(dirty_&&isVisible())delay_.start(0);return;}
    try{if(key()!=launchedKey_){invalidate();return;}QFile file(work_->filePath("report.json"));if(!file.open(QIODevice::ReadOnly)||file.size()>2*1024*1024)throw std::runtime_error(QStringLiteral("未收到完整型值预览。").toStdString());
        QJsonParseError parse;auto report=QJsonDocument::fromJson(file.readAll(),&parse).object();if(parse.error!=QJsonParseError::NoError||code||report["status"]!="pass")throw std::runtime_error(report["error"].toString(QStringLiteral("型值预览生成失败。")).toStdString());
        if(report["document_id"]!=session_.model()->documentId||report["revision"].toInteger()!=qint64(session_.model()->revision))throw std::runtime_error(QStringLiteral("船模版本已变更，旧预览未采用。").toStdString());
        auto preview=report["preview"].toObject();if(preview["document_id"]!=report["document_id"]||preview["revision"]!=report["revision"])throw std::runtime_error(QStringLiteral("型值预览版本不匹配。").toStdString());
        QFile input(work_->filePath("input.txt"));if(!input.open(QIODevice::ReadOnly)||input.size()>16*1024*1024)throw std::runtime_error(QStringLiteral("无法校验预览型值。").toStdString());auto hash=QString::fromLatin1(QCryptographicHash::hash(input.readAll(),QCryptographicHash::Sha256).toHex());if(hash!=report["sha256"].toString())throw std::runtime_error(QStringLiteral("预览型值与计算文件不一致。").toStdString());
        view_->setGrid(preview);data_=preview;inputHash_=hash;completedKey_=launchedKey_;current_=true;errorSource_.clear();
        status_->setMessage(QString(uiText("%1 个船体 · 每体 %2 个横剖站 × %3 条水线 · L=%4 m · T=%5 m")).arg(preview["hulls"].toInt()).arg(preview["stations"].toInt()).arg(preview["waterlines"].toInt()).arg(preview["length_m"].toDouble(),0,'g',9).arg(preview["draft_m"].toDouble(),0,'g',9),StudioMessageRole::Feedback);
    }catch(const std::exception& e){fail(QString::fromUtf8(e.what()));}
}
void GridPreviewPanel::fail(QString source){current_=false;errorSource_=std::move(source);refreshError();}
void GridPreviewPanel::refreshError(){
    const auto prefix=data_.isEmpty()?QString{}:QString::fromUtf8(uiText("保留上次预览，当前参数尚未生成。\n"));
    status_->setMessage(prefix+uiText(errorSource_),StudioMessageRole::Warning);
}
QJsonObject GridPreviewPanel::evidence()const{return {{"current",current_},{"pending",pending()},{"launches",qint64(launchCount_)},{"error",uiText(errorSource_)},{"status_text",status_->text()},{"input_sha256",inputHash_},{"data",data_},{"view",view_->evidence()}};}
