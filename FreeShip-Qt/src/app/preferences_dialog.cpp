// SPDX-License-Identifier: GPL-2.0-or-later
#include "preferences_dialog.h"
#include "feedback_text.h"
#include "preferences_store.h"
#include "studio_ui.h"
#include "studio_read_ui.h"
#include "document/preferences.h"
#include <QColorDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QToolButton>
#include <QToolTip>
#include <QEnterEvent>
#include <QFrame>
#include <QShortcut>
#include <QStandardItemModel>
#include "ui_language.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPointer>
#include <QTextBrowser>
#include "report_layout.h"
#include <QJsonArray>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <stdexcept>

namespace {
void disableChoice(QComboBox* box,const QString& id){
    auto* model=qobject_cast<QStandardItemModel*>(box->model());
    if(!model)return;
    for(int row=0;row<model->rowCount();++row){
        auto* item=model->item(row);
        if(!item)continue;
        const bool disabled=!id.isEmpty()&&item->data(Qt::UserRole).toString()==id;
        item->setFlags(disabled?item->flags()&~Qt::ItemIsEnabled:item->flags()|Qt::ItemIsEnabled);
    }
}
class LanguageHintButton final:public QToolButton{
public:
    LanguageHintButton(const QString& tip,QWidget* parent):QToolButton(parent),tip_(tip){
        setText("?");
        setAutoRaise(false);
        setFixedSize(32,32);
        setCursor(Qt::WhatsThisCursor);
        setStyleSheet(
            "QToolButton{background:#f7faf7;color:#245c4b;border:1px solid #c4d6cc;border-radius:6px;font-size:16px;font-weight:600}"
            "QToolButton:hover{background:#e6f1ea;border-color:#8eb6a1}"
            "QToolButton:pressed{background:#d4e8dc;border-color:#6f9e88}");
    }
    void setTip(const QString& tip){tip_=tip;}
protected:
    void enterEvent(QEnterEvent* event)override{
        QToolButton::enterEvent(event);
        QToolTip::showText(mapToGlobal(rect().bottomRight()),tip_,this,rect(),12000);
    }
    void leaveEvent(QEvent* event)override{
        if(QToolTip::isVisible())QToolTip::hideText();
        QToolButton::leaveEvent(event);
    }
private:
    QString tip_;
};
}

bool chooseInitialLanguages(QWidget* parent,QJsonObject* values){
    if(!values)return false;
    QDialog dialog(parent);
    bindUiText(&dialog,"windowTitle","选择你的语言");
    setStudioWindowIcon(&dialog,"ProjectSettings");
    dialog.resize(620,380);
    dialog.setMinimumSize(560,340);
    styleStudioReadDialog(&dialog);
    auto* layout=new QVBoxLayout(&dialog);
    layout->setContentsMargins(26,22,26,22);
    layout->setSpacing(16);
    QLabel* headingText=nullptr;
    auto* heading=studioReadHeading({},"ProjectSettings",&dialog,&headingText);
    bindUiText(headingText,"text","选择你的语言");
    layout->addWidget(heading);
    auto* panel=new QFrame(&dialog);panel->setObjectName("InitialLanguagePanel");
    panel->setStyleSheet(
        "QFrame#InitialLanguagePanel{background:#eef4ef;border:1px solid #d4e2d9;border-radius:8px}"
        "QLabel{color:#294e43;font-size:13px}"
        "QComboBox{background:#ffffff;color:#213f37;border:1px solid #c9dbd0;border-radius:5px;padding:7px 10px;min-height:20px}"
        "QComboBox:hover{border-color:#8fb5a1}"
        "QComboBox:focus{border:2px solid #4d8f7d;padding:6px 9px}");
    auto* panelLayout=new QVBoxLayout(panel);panelLayout->setContentsMargins(20,18,20,18);
    auto* form=new QFormLayout;form->setSpacing(16);panelLayout->addLayout(form);
    const auto languages=uiLanguageChoices();
    auto makeBox=[&](const QString& initial){
        auto* box=new QComboBox(&dialog);
        box->setMinimumWidth(250);
        for(const auto& entry:languages){
            const auto item=entry.toObject();
            box->addItem(item["name"].toString(),item["id"].toString());
        }
        const auto index=box->findData(initial);
        box->setCurrentIndex(index>=0?index:0);
        return box;
    };
    auto* first=makeBox(uiLanguage());
    const auto primary=first->currentData().toString();
    auto* second=makeBox(primary=="ru"?"en":"ru");
    if(second->currentData().toString()==primary && second->count()>1)
        second->setCurrentIndex((second->currentIndex()+1)%second->count());
    auto* hint=new LanguageHintButton(
        uiText("对于那些经常在多语言环境中使用该软件的人，设定第二语言是个不错的选择"),
        &dialog);
    auto* secondRow=new QHBoxLayout;secondRow->setContentsMargins(0,0,0,0);secondRow->setSpacing(8);
    secondRow->addWidget(second,1);secondRow->addWidget(hint);
    auto* firstLabel=new QLabel(panel);
    auto* secondLabel=new QLabel(panel);
    bindUiText(firstLabel,"text","第一语言");
    bindUiText(secondLabel,"text","第二语言");
    form->addRow(firstLabel,first);
    form->addRow(secondLabel,secondRow);
    auto* shortcutNote=new QWidget(panel);
    auto* shortcutLayout=new QHBoxLayout(shortcutNote);
    shortcutLayout->setContentsMargins(0,2,0,0);
    shortcutLayout->setSpacing(5);
    auto* shortcutPrefix=new QLabel(shortcutNote);
    auto* shortcutSuffix=new QLabel(shortcutNote);
    bindUiText(shortcutPrefix,"text","通过快捷键");
    bindUiText(shortcutSuffix,"text","在第一语言和第二语言之间切换");
    shortcutSuffix->setWordWrap(true);
    shortcutPrefix->setObjectName("InitialLanguageShortcutPrefix");
    shortcutSuffix->setObjectName("InitialLanguageShortcutSuffix");
    const auto keycap=[](const QString& text,QWidget* parent){
        auto* cap=new QFrame(parent);
        cap->setProperty("initial_language_keycap",true);
        cap->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        cap->setStyleSheet(
            "QFrame[initial_language_keycap=\"true\"]{background:#b5cbbf;border:1px solid #cadbd0;border-radius:5px}"
            "QLabel{color:#254e42;font-weight:600}");
        auto* inset=new QHBoxLayout(cap);
        inset->setContentsMargins(8,3,8,3);
        auto* label=new QLabel(text,cap);
        label->setMinimumWidth(10);
        label->setAlignment(Qt::AlignCenter);
        inset->addWidget(label);
        return cap;
    };
    shortcutLayout->addWidget(shortcutPrefix);
    shortcutLayout->addWidget(keycap("Ctrl",shortcutNote));
    shortcutLayout->addWidget(new QLabel("+",shortcutNote));
    shortcutLayout->addWidget(keycap("Shift",shortcutNote));
    shortcutLayout->addWidget(new QLabel("+",shortcutNote));
    shortcutLayout->addWidget(keycap("L",shortcutNote));
    shortcutLayout->addWidget(shortcutSuffix,1);
    form->addRow(shortcutNote);
    layout->addWidget(panel);
    auto* error=new StudioMessage(StudioMessageRole::Warning,{},&dialog);error->setVisible(false);layout->addWidget(error);
    auto* confirm=new QPushButton(&dialog);
    bindUiText(confirm,"text","继续");
    confirm->setDefault(true);
    confirm->setMinimumSize(112,36);
    confirm->setStyleSheet(
        "QPushButton{background:#1e6255;color:white;border:1px solid #1e6255;border-radius:5px;padding:7px 18px}"
        "QPushButton:hover{background:#174f45}"
        "QPushButton:pressed{background:#123f37}");
    layout->addWidget(confirm,0,Qt::AlignRight);
    const auto sync=[first,second]{
        disableChoice(first,second->currentData().toString());
        disableChoice(second,first->currentData().toString());
    };
    const auto displayLanguage=[&](const QString& id){
        const auto issue=setUiLanguage(id);
        if(!issue.isEmpty())return;
        hint->setTip(uiText("对于那些经常在多语言环境中使用该软件的人，设定第二语言是个不错的选择"));
        if(error->isVisible())error->setMessage(uiText("请选择两种不同的语言。"),StudioMessageRole::Warning);
    };
    QObject::connect(first,&QComboBox::currentIndexChanged,&dialog,[sync]{sync();});
    QObject::connect(first,&QComboBox::currentIndexChanged,&dialog,[&](int){
        displayLanguage(first->currentData().toString());
    });
    QObject::connect(second,&QComboBox::currentIndexChanged,&dialog,[sync]{sync();});
    auto* languageShortcut=new QShortcut(QKeySequence(Qt::CTRL|Qt::SHIFT|Qt::Key_L),&dialog);
    languageShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    languageShortcut->setAutoRepeat(false);
    QObject::connect(languageShortcut,&QShortcut::activated,&dialog,[&]{
        const auto [firstId,secondId]=qMakePair(first->currentData().toString(),second->currentData().toString());
        displayLanguage(uiLanguage()==firstId?secondId:firstId);
    });
    sync();
    QObject::connect(confirm,&QPushButton::clicked,&dialog,[&]{
        const auto firstId=first->currentData().toString(),secondId=second->currentData().toString();
        if(firstId.isEmpty()||secondId.isEmpty()||firstId==secondId){
            error->setMessage(uiText("请选择两种不同的语言。"),StudioMessageRole::Warning);
            error->setVisible(true);
            return;
        }
        (*values)["ui_language"]=firstId;
        (*values)["quick_language_first"]=firstId;
        (*values)["quick_language_second"]=secondId;
        (*values)["quick_language_enabled"]=true;
        dialog.accept();
    });
    return dialog.exec()==QDialog::Accepted;
}

PreferencesDialog::PreferencesDialog(QJsonObject values,QWidget* parent,const QString& solverDefault):QDialog(parent),draft_(std::move(values)){
    setWindowTitle(uiText("首选项"));setStudioWindowIcon(this,"ProjectSettings");resize(638,687);setMinimumSize(580,400);
    auto* layout=new QVBoxLayout(this);auto* buttons=new QHBoxLayout;
    import_=new QPushButton(uiText("导入旧版设置…"));import_->setAutoDefault(false);import_->setProperty("preference_action","import_legacy");
    buttons->addWidget(import_);buttons->addStretch();
    ok_=new QPushButton(studioIcon("Confirm"),uiText("确定"));cancel_=new QPushButton(studioIcon("Cancel"),uiText("取消"));
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
    const auto languages=uiLanguageChoices();
    auto languageBox=[&](const QString& key,const QString& initial){
        auto* box=new QComboBox;box->setProperty("preference_field",key);box->setProperty("preference_choice_id",true);
        for(const auto& entry:languages){const auto item=entry.toObject();box->addItem(item["name"].toString(),item["id"].toString());}
        box->setCurrentIndex(std::max(0,box->findData(initial)));
        draft_[key]=box->currentData().toString();
        connect(box,&QComboBox::currentIndexChanged,this,[this,box,key]{draft_[key]=box->currentData().toString();});return box;
    };
    auto* language=languageBox("ui_language",draft_.value("ui_language").toString("zh"));
    form->addRow(uiText("界面语言 / Language"),language);
    auto* quick=new QCheckBox(uiText("启用双语言切换 (Ctrl+Shift+L)"));quick->setProperty("preference_field","quick_language_enabled");quick->setChecked(draft_.value("quick_language_enabled").toBool());
    const auto [firstLanguage,secondLanguage]=quickUiLanguages(draft_);
    auto* first=languageBox("quick_language_first",firstLanguage);
    auto* second=languageBox("quick_language_second",secondLanguage);
    form->addRow(quick);form->addRow(uiText("第一语言"),first);form->addRow(uiText("第二语言"),second);
    first->setEnabled(quick->isChecked());second->setEnabled(quick->isChecked());
    connect(quick,&QCheckBox::toggled,this,[this,first,second](bool enabled){draft_["quick_language_enabled"]=enabled;first->setEnabled(enabled);second->setEnabled(enabled);});
    auto* undo=new QSpinBox;undo->setRange(0,1048576);undo->setSuffix(" MB");undo->setValue(effective.value("undo_memory_mb").toInt());undo->setProperty("preference_field","undo_memory_mb");
    form->addRow(uiText("撤销内存上限"),undo);connect(undo,&QSpinBox::valueChanged,this,[this](int v){draft_["undo_memory_mb"]=v;});
    auto* encoding=new QComboBox;encoding->setEditable(true);encoding->addItems({"cp1251","cp1252","cp936","UTF-8"});encoding->setCurrentText(effective.value("fbm_encoding").toString());encoding->setProperty("preference_field","fbm_encoding");
    form->addRow(uiText("FBM 文字编码"),encoding);connect(encoding,&QComboBox::currentTextChanged,this,[this](const QString& s){draft_["fbm_encoding"]=s;});
    encoding->setToolTip(uiText("用于之后打开的船模。"));addPage(general,uiText("常规"));
    auto* directories=new QWidget;auto* paths=new QFormLayout(directories);paths->setSpacing(12);
    paths->setRowWrapPolicy(QFormLayout::WrapLongRows);
    for(const auto& pair:{std::pair{"open",uiText("打开模型")},{"save",uiText("保存模型")},{"import",uiText("导入文件")},{"export",uiText("导出文件")},{"solver",uiText("外部求解器目录")},{"temporary",uiText("临时文件")}}){
        const QString key=pair.first;
        auto* row=new QHBoxLayout;
        auto* edit=new QLineEdit(draft_.value("directories").toObject()[key].toString());
        const auto fallback=key=="temporary"?QDir::tempPath():key=="solver"?solverDefault:defaultFileDirectory();
        if(fallback.isEmpty())bindUiText(edit,"placeholderText","未配置目录");
        else edit->setPlaceholderText(QDir::toNativeSeparators(fallback));
        const auto refreshTip=[edit,fallback]{edit->setToolTip(edit->text().isEmpty()?QDir::toNativeSeparators(fallback):edit->text());};
        refreshTip();connect(edit,&QLineEdit::textChanged,this,refreshTip);
        edit->setProperty("preference_field","directories."+key);
        auto* browse=studioFolderButton(uiText("选择目录"));
        row->addWidget(edit,1);row->addWidget(browse);paths->addRow(pair.second,row);
        connect(edit,&QLineEdit::textChanged,this,[this,key](const QString& s){auto d=draft_.value("directories").toObject();d[key]=s;draft_["directories"]=d;});
        connect(browse,&QPushButton::clicked,this,[this,edit,fallback]{
            const auto start=edit->text().isEmpty()?fallback:edit->text();
            const auto path=QFileDialog::getExistingDirectory(this,uiText("选择目录"),start);
            if(!path.isEmpty())edit->setText(QDir::toNativeSeparators(path));
        });
    }
    auto* pathNote=new StudioMessage(StudioMessageRole::Explanation,uiText("临时目录重启后生效。"));paths->addRow(pathNote);addPage(directories,uiText("目录"));buildGraphics();
    error_=new StudioMessage(StudioMessageRole::Warning);error_->setProperty("preference_error",true);layout->addWidget(error_);
    importSummary_=new QTextBrowser;importSummary_->setReadOnly(true);importSummary_->setMaximumHeight(210);importSummary_->setVisible(false);importSummary_->setProperty("preference_import_summary",true);layout->addWidget(importSummary_);
    connect(import_,&QPushButton::clicked,this,&PreferencesDialog::importLegacy);
    connect(cancel_,&QPushButton::clicked,this,&PreferencesDialog::reject);
    connect(ok_,&QPushButton::clicked,this,[this]{auto error=validatePreferences(draft_);if(!error.isEmpty()){showFailure(error);return;}if(onApply)onApply(draft_);});
}
void PreferencesDialog::importLegacy(){
    QFileDialog picker(this,uiText("导入旧版设置"));picker.setFileMode(QFileDialog::ExistingFile);
    picker.setNameFilter(uiText("FreeShip 设置 (*.ini *.dta)"));picker.setProperty("preference_file_action","import_legacy");
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
            if(key.startsWith("directories.")){if(auto* edit=qobject_cast<QLineEdit*>(widget))edit->setText(draft_.value("directories").toObject()[key.mid(12)].toString());}
            else if(key=="undo_memory_mb"){if(auto* spin=qobject_cast<QSpinBox*>(widget))spin->setValue(effective[key].toInt());}
            else if(key=="fbm_encoding"){if(auto* combo=qobject_cast<QComboBox*>(widget))combo->setCurrentText(effective[key].toString());}
        }
        buildGraphics();setProperty("preference_failure",QString{});error_->clear();QStringList notes;
        for(const auto& note:report["notes"].toArray())notes<<note.toString();
        report::Table sources{{},{uiText("来源：")},{},{0}};
        for(const auto& path:report["sources"].toArray())sources.rows.append({path.toString()});
        importSummary_->setProperty("studio_content_role","classified_report");
        report::setHtml(importSummary_->document(),report::message(notes.join("\n"),true,StudioMessageRole::Warning)+report::render(sources,true));importSummary_->setVisible(true);
        error_->setMessage(completionText(report["count"].toInt(),"已导入 %1 项，尚未保存。"),StudioMessageRole::Feedback);
        setProperty("preference_import_report",report);
    }catch(const std::exception& e){showFailure(QString::fromUtf8(e.what()));}
}
void PreferencesDialog::buildGraphics(){
    const auto effective=effectivePreferences(draft_),graphic=effective.value("graphic").toObject();
    const int current=tabs_->currentIndex();if(graphics_){tabs_->removeTab(tabs_->indexOf(graphics_));graphics_->setParent(nullptr);graphics_->deleteLater();}
    auto* scroll=new QScrollArea;scroll->setFrameShape(QFrame::NoFrame);scroll->setWidgetResizable(true);graphics_=scroll;auto* panel=new QWidget;auto* form=new QFormLayout(panel);form->setSpacing(10);form->setRowWrapPolicy(QFormLayout::WrapLongRows);scroll->setWidget(panel);
    auto* theme=new QComboBox;addUiItem(theme,draft_.value("graphic").toObject().isEmpty()?"当前配色":"自定义配色",QString{});
    QStringList names;const auto root=QCoreApplication::applicationDirPath()+"/bridge/";
    for(const auto& sub:{"Themes",".coordinate-settings/Themes"})for(const auto& n:QDir(root+sub).entryList(QDir::Dirs|QDir::NoDotAndDotDot))if(QFileInfo::exists(root+sub+'/'+n+"/theme.ini")&&!names.contains(n))names.append(n);
    for(const auto& n:names)theme->addItem(n,n);
    theme->setCurrentIndex(std::max(0,theme->findData(draft_.value("theme").toString())));theme->setProperty("preference_field","theme");form->addRow(uiText("绘图配色"),theme);
    connect(theme,&QComboBox::activated,this,[this,theme]{if(theme->currentData().toString().isEmpty())draft_.remove("theme");else draft_["theme"]=theme->currentData().toString();
        auto g=draft_.value("graphic").toObject();for(auto it=g.begin();it!=g.end();)if(it.key().endsWith("color")||it.key()=="underwatercoloralpha")it=g.erase(it);else ++it;draft_["graphic"]=g;
        QTimer::singleShot(0,this,[this]{refreshGraphics();});});
    auto spin=[&](const QString& key,const QString& caption,int value,int lo,int hi,bool nested){auto* s=new QSpinBox;s->setRange(lo,hi);s->setValue(value);s->setProperty("preference_field",nested?"graphic."+key:key);form->addRow(caption,s);connect(s,&QSpinBox::valueChanged,this,[this,key,nested](int v){if(nested){auto g=draft_.value("graphic").toObject();g[key]=v;draft_["graphic"]=g;}else draft_[key]=v;});};
    spin("pointsize",uiText("控制点大小"),graphic.value("pointsize").toInt(),1,100,true);spin("fontsize",uiText("绘图标注字号"),graphic.value("fontsize").toInt(),1,200,true);spin("tool_icon_size",uiText("工具栏图标大小"),effective.value("tool_icon_size").toInt(),12,128,false);
    const auto colors=fs::document::resolvedPreferenceColors(graphic);
    for(const auto& pair:{std::pair{"viewportcolor",uiText("视图背景")},{"edgecolor",uiText("控制网边线")},{"creaseedgecolor",uiText("折角边线")},{"regularpointcolor",uiText("普通控制点")},{"creasepointcolor",uiText("折角控制点")},{"cornerpointcolor",uiText("角点")},{"dartpointcolor",uiText("尖点")},{"leakpointcolor",uiText("泄漏点")},{"selectcolor",uiText("选中控制点")},{"normalcolor",uiText("面法线")},{"stationcolor",uiText("横剖线")},{"buttockcolor",uiText("纵剖线")},{"waterlinecolor",uiText("水线")},{"diagonalcolor",uiText("斜剖线")},{"gridcolor",uiText("网格")},{"gridfontcolor",uiText("网格文字")},{"markercolor",uiText("参考曲线")},{"curvatureplotcolor",uiText("曲率图")},{"controlcurvecolor",uiText("控制曲线")},{"hydrostaticsfontcolor",uiText("静水力标注")},{"layercolor",uiText("新建图层默认颜色")},{"underwatercolor",uiText("新船模水下部分")}}){
        const QString key=pair.first;auto* button=new QPushButton;button->setAutoDefault(false);button->setProperty("preference_field","graphic."+key);
        const auto color=QColor::fromRgb(colors[key].toInt());auto paint=[button](QColor c){QPixmap p(32,18);p.fill(c);button->setIcon(QIcon(p));button->setText(c.name().toUpper());button->setProperty("color",c);};paint(color);form->addRow(pair.second,button);
        connect(button,&QPushButton::clicked,this,[this,guard=QPointer<QPushButton>(button),key,paint]{if(!guard)return;auto c=QColorDialog::getColor(guard->property("color").value<QColor>(),this,uiText("选择颜色"), QColorDialog::DontUseNativeDialog);if(!guard||!c.isValid())return;auto g=draft_.value("graphic").toObject();g[key]=c.red()|(c.green()<<8)|(c.blue()<<16);draft_["graphic"]=g;paint(c);});
    }
    spin("underwatercoloralpha",uiText("新船模水下不透明度 (0–255)"),graphic.value("underwatercoloralpha").toInt(),0,255,true);
    auto* reset=new QPushButton(uiText("恢复默认绘图设置"));reset->setAutoDefault(false);reset->setProperty("preference_action","reset_graphics");form->addRow(reset);
    connect(reset,&QPushButton::clicked,this,[this]{draft_.remove("theme");draft_.remove("graphic");draft_.remove("tool_icon_size");QTimer::singleShot(0,this,[this]{refreshGraphics();});});
    tabs_->addTab(graphics_,uiText("图形"));tabs_->setCurrentIndex(current);
}
void PreferencesDialog::refreshGraphics(){try{buildGraphics();}catch(const std::exception& e){showFailure(QString::fromUtf8(e.what()));}}
void PreferencesDialog::setBusy(bool busy){busy_=busy;tabs_->setEnabled(!busy);ok_->setEnabled(!busy);cancel_->setEnabled(!busy);import_->setEnabled(!busy);if(busy){setProperty("preference_failure",QString{});error_->setMessage(uiText("正在应用设置…"),StudioMessageRole::Feedback,true);}}
void PreferencesDialog::setLanguageValue(const QString& id){
    draft_["ui_language"]=id;
    for(auto* widget:tabs_->findChildren<QWidget*>())
        if(widget->property("preference_field").toString()=="ui_language")
            if(auto* box=qobject_cast<QComboBox*>(widget)){
                const QSignalBlocker guard(box);
                const auto index=box->findData(id);
                if(index>=0)box->setCurrentIndex(index);
            }
}
void PreferencesDialog::showFailure(const QString& message){setBusy(false);setProperty("preference_failure",message);error_->setMessage(message,StudioMessageRole::Warning);}
void PreferencesDialog::reject(){if(!busy_)QDialog::reject();}
