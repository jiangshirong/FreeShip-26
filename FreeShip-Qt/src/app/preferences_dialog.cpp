// SPDX-License-Identifier: GPL-2.0-or-later
#include "preferences_dialog.h"
#include "preferences_store.h"
#include "studio_ui.h"
#include "document/preferences.h"
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QJsonArray>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <stdexcept>

PreferencesDialog::PreferencesDialog(QJsonObject values,QWidget* parent):QDialog(parent),draft_(std::move(values)){
    setWindowTitle("偏好设置");resize(638,687);setMinimumSize(580,400);
    auto* layout=new QVBoxLayout(this);auto* buttons=new QHBoxLayout;
    import_=new QPushButton("导入旧版设置…");import_->setAutoDefault(false);import_->setProperty("preference_action","import_legacy");
    buttons->addWidget(import_);buttons->addStretch();
    ok_=new QPushButton(studioIcon("Confirm"),"确定");cancel_=new QPushButton(studioIcon("Cancel"),"取消");
    ok_->setProperty("preference_action","apply");cancel_->setProperty("preference_action","cancel");
    ok_->setDefault(true);buttons->addWidget(ok_);buttons->addWidget(cancel_);layout->addLayout(buttons);
    tabs_=new QTabWidget;layout->addWidget(tabs_,1);
    auto addPage=[this](QWidget* content,const QString& title){
        auto* scroll=new QScrollArea;scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);scroll->setWidget(content);tabs_->addTab(scroll,title);
    };
    auto* general=new QWidget;auto* form=new QFormLayout(general);form->setSpacing(14);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    const auto effective=effectivePreferences(draft_);
    auto* undo=new QSpinBox;undo->setRange(0,1048576);undo->setSuffix(" MB");undo->setValue(effective.value("undo_memory_mb").toInt());undo->setProperty("preference_field","undo_memory_mb");
    form->addRow("撤销内存上限",undo);connect(undo,&QSpinBox::valueChanged,this,[this](int v){draft_["undo_memory_mb"]=v;});
    auto* encoding=new QComboBox;encoding->setEditable(true);encoding->addItems({"cp1251","cp1252","cp936","UTF-8"});encoding->setCurrentText(effective.value("fbm_encoding").toString());encoding->setProperty("preference_field","fbm_encoding");
    form->addRow("FBM 文字编码",encoding);connect(encoding,&QComboBox::currentTextChanged,this,[this](const QString& s){draft_["fbm_encoding"]=s;});
    auto* note=new QLabel("编码用于之后打开的船模；当前船模中的文字保持原样。");note->setWordWrap(true);form->addRow(note);addPage(general,"常规");
    auto* directories=new QWidget;auto* paths=new QFormLayout(directories);paths->setSpacing(12);
    paths->setRowWrapPolicy(QFormLayout::WrapLongRows);
    for(const auto& pair:{std::pair{"open","打开模型"},{"save","保存模型"},{"import","导入文件"},{"export","导出文件"},{"solver","计算程序"},{"temporary","临时文件"}}){
        const QString key=pair.first;auto* row=new QHBoxLayout;auto* edit=new QLineEdit(draft_.value("directories").toObject()[key].toString());edit->setPlaceholderText("使用默认目录");edit->setProperty("preference_field","directories."+key);
        auto* browse=new QPushButton("浏览…");browse->setAutoDefault(false);row->addWidget(edit,1);row->addWidget(browse);paths->addRow(pair.second,row);
        connect(edit,&QLineEdit::textChanged,this,[this,key](const QString& s){auto d=draft_.value("directories").toObject();d[key]=s;draft_["directories"]=d;});
        connect(browse,&QPushButton::clicked,this,[this,edit]{auto p=QFileDialog::getExistingDirectory(this,"选择目录",edit->text());if(!p.isEmpty())edit->setText(p);});
    }
    auto* pathNote=new QLabel("目录留空时沿用默认位置。临时目录在下次启动软件后生效。");pathNote->setWordWrap(true);paths->addRow(pathNote);addPage(directories,"目录");buildGraphics();
    error_=new QLabel;error_->setWordWrap(true);error_->setStyleSheet("color:#a03030");error_->setProperty("preference_error",true);layout->addWidget(error_);
    importSummary_=new QPlainTextEdit;importSummary_->setReadOnly(true);importSummary_->setMaximumHeight(105);importSummary_->setVisible(false);importSummary_->setProperty("preference_import_summary",true);layout->addWidget(importSummary_);
    connect(import_,&QPushButton::clicked,this,&PreferencesDialog::importLegacy);
    connect(cancel_,&QPushButton::clicked,this,&PreferencesDialog::reject);
    connect(ok_,&QPushButton::clicked,this,[this]{auto error=validatePreferences(draft_);if(!error.isEmpty()){showFailure(error);return;}if(onApply)onApply(draft_);});
}
void PreferencesDialog::importLegacy(){
    QFileDialog picker(this,"导入旧版设置");picker.setFileMode(QFileDialog::ExistingFile);
    picker.setNameFilter("FreeShip 设置 (*.ini *.dta)");picker.setProperty("preference_file_action","import_legacy");
    if(picker.exec()!=QDialog::Accepted || picker.selectedFiles().isEmpty())return;
    try{
        const auto report=fs::document::importLegacyPreferences(picker.selectedFiles().front(),QCoreApplication::applicationDirPath()+"/bridge");
        const auto imported=report["values"].toObject();auto candidate=draft_;
        for(auto it=imported.begin();it!=imported.end();++it){
            if(it.key()=="graphic" || it.key()=="directories"){
                auto object=it.key()=="graphic"?effectivePreferences(draft_)["graphic"].toObject():draft_[it.key()].toObject();
                const auto additions=it.value().toObject();for(auto f=additions.begin();f!=additions.end();++f)object[f.key()]=f.value();
                candidate[it.key()]=object;
            }else candidate[it.key()]=it.value();
        }
        if(imported.contains("graphic"))candidate.remove("theme");
        const auto validation=validatePreferences(candidate);if(!validation.isEmpty())throw std::runtime_error(validation.toStdString());
        draft_=candidate;const auto effective=effectivePreferences(draft_);
        for(auto* widget:tabs_->findChildren<QWidget*>()){
            const auto key=widget->property("preference_field").toString();const QSignalBlocker guard(widget);
            if(key.startsWith("directories.")){if(auto* edit=qobject_cast<QLineEdit*>(widget))edit->setText(draft_["directories"].toObject()[key.mid(12)].toString());}
            else if(key=="undo_memory_mb"){if(auto* spin=qobject_cast<QSpinBox*>(widget))spin->setValue(effective[key].toInt());}
            else if(key=="fbm_encoding"){if(auto* combo=qobject_cast<QComboBox*>(widget))combo->setCurrentText(effective[key].toString());}
        }
        buildGraphics();error_->clear();QStringList lines;
        lines<<QString("已导入 %1 项到草稿；点击确定后保存，取消则放弃。").arg(report["count"].toInt());
        for(const auto& note:report["notes"].toArray())lines<<note.toString();
        for(const auto& path:report["sources"].toArray())lines<<"来源："+path.toString();
        importSummary_->setPlainText(lines.join('\n'));importSummary_->setVisible(true);
        setProperty("preference_import_report",report);
    }catch(const std::exception& e){showFailure(QString::fromUtf8(e.what()));}
}
void PreferencesDialog::buildGraphics(){
    const auto effective=effectivePreferences(draft_),graphic=effective.value("graphic").toObject();
    const int current=tabs_->currentIndex();if(graphics_){tabs_->removeTab(tabs_->indexOf(graphics_));graphics_->setParent(nullptr);graphics_->deleteLater();}
    auto* scroll=new QScrollArea;scroll->setFrameShape(QFrame::NoFrame);scroll->setWidgetResizable(true);graphics_=scroll;auto* panel=new QWidget;auto* form=new QFormLayout(panel);form->setSpacing(10);form->setRowWrapPolicy(QFormLayout::WrapLongRows);scroll->setWidget(panel);
    auto* theme=new QComboBox;theme->addItem(draft_["graphic"].toObject().isEmpty()?"当前 Qt 配色":"自定义配色",QString{});
    QStringList names;const auto root=QCoreApplication::applicationDirPath()+"/bridge/";
    for(const auto& sub:{"Themes",".coordinate-settings/Themes"})for(const auto& n:QDir(root+sub).entryList(QDir::Dirs|QDir::NoDotAndDotDot))if(QFileInfo::exists(root+sub+'/'+n+"/theme.ini")&&!names.contains(n))names.append(n);
    for(const auto& n:names)theme->addItem(n,n);
    theme->setCurrentIndex(std::max(0,theme->findData(draft_.value("theme").toString())));theme->setProperty("preference_field","theme");form->addRow("绘图配色",theme);
    connect(theme,&QComboBox::activated,this,[this,theme]{if(theme->currentData().toString().isEmpty())draft_.remove("theme");else draft_["theme"]=theme->currentData().toString();
        auto g=draft_.value("graphic").toObject();for(auto it=g.begin();it!=g.end();)if(it.key().endsWith("color")||it.key()=="underwatercoloralpha")it=g.erase(it);else ++it;draft_["graphic"]=g;
        QTimer::singleShot(0,this,[this]{refreshGraphics();});});
    auto spin=[&](const QString& key,const QString& caption,int value,int lo,int hi,bool nested){auto* s=new QSpinBox;s->setRange(lo,hi);s->setValue(value);s->setProperty("preference_field",nested?"graphic."+key:key);form->addRow(caption,s);connect(s,&QSpinBox::valueChanged,this,[this,key,nested](int v){if(nested){auto g=draft_.value("graphic").toObject();g[key]=v;draft_["graphic"]=g;}else draft_[key]=v;});};
    spin("pointsize","控制点大小",graphic.value("pointsize").toInt(),1,100,true);spin("fontsize","绘图标注字号",graphic.value("fontsize").toInt(),1,200,true);spin("tool_icon_size","工具栏图标大小",effective.value("tool_icon_size").toInt(),12,128,false);
    const auto colors=fs::document::resolvedPreferenceColors(graphic);
    for(const auto& pair:{std::pair{"viewportcolor","视图背景"},{"edgecolor","控制网边线"},{"creaseedgecolor","折角边线"},{"regularpointcolor","普通控制点"},{"creasepointcolor","折角控制点"},{"cornerpointcolor","角点"},{"dartpointcolor","尖点"},{"leakpointcolor","泄漏点"},{"selectcolor","选中控制点"},{"normalcolor","面法线"},{"stationcolor","横剖线"},{"buttockcolor","纵剖线"},{"waterlinecolor","水线"},{"diagonalcolor","斜剖线"},{"gridcolor","网格"},{"gridfontcolor","网格文字"},{"markercolor","标记线"},{"curvatureplotcolor","曲率图"},{"controlcurvecolor","控制曲线"},{"hydrostaticsfontcolor","静水力标注"},{"layercolor","新建图层默认颜色"},{"underwatercolor","新船模水下部分"}}){
        const QString key=pair.first;auto* button=new QPushButton;button->setAutoDefault(false);button->setProperty("preference_field","graphic."+key);
        const auto color=QColor::fromRgb(colors[key].toInt());auto paint=[button](QColor c){QPixmap p(32,18);p.fill(c);button->setIcon(QIcon(p));button->setText(c.name().toUpper());button->setProperty("color",c);};paint(color);form->addRow(pair.second,button);
        connect(button,&QPushButton::clicked,this,[this,button,key,paint]{auto c=QColorDialog::getColor(button->property("color").value<QColor>(),this,"选择颜色");if(!c.isValid())return;auto g=draft_.value("graphic").toObject();g[key]=c.red()|(c.green()<<8)|(c.blue()<<16);draft_["graphic"]=g;paint(c);});
    }
    spin("underwatercoloralpha","新船模水下不透明度 (0–255)",graphic.value("underwatercoloralpha").toInt(),0,255,true);
    auto* reset=new QPushButton("恢复默认绘图设置");reset->setAutoDefault(false);reset->setProperty("preference_action","reset_graphics");form->addRow(reset);
    connect(reset,&QPushButton::clicked,this,[this]{draft_.remove("theme");draft_.remove("graphic");draft_.remove("tool_icon_size");QTimer::singleShot(0,this,[this]{refreshGraphics();});});
    tabs_->addTab(graphics_,"图形");tabs_->setCurrentIndex(current);
}
void PreferencesDialog::refreshGraphics(){try{buildGraphics();}catch(const std::exception& e){showFailure(QString::fromUtf8(e.what()));}}
void PreferencesDialog::setBusy(bool busy){busy_=busy;tabs_->setEnabled(!busy);ok_->setEnabled(!busy);cancel_->setEnabled(!busy);import_->setEnabled(!busy);if(busy)error_->setText("正在应用设置…");}
void PreferencesDialog::showFailure(const QString& message){setBusy(false);error_->setText(message);}
void PreferencesDialog::reject(){if(!busy_)QDialog::reject();}
