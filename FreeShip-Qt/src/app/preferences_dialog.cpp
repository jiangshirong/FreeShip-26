// SPDX-License-Identifier: GPL-2.0-or-later
#include "preferences_dialog.h"
#include "preferences_store.h"
#include "studio_help_hint.h"
#include "studio_flow_layout.h"
#include "studio_ui.h"
#include "studio_read_ui.h"
#include "document/preferences.h"
#include <QColorDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QToolButton>
#include <QFrame>
#include <QShortcut>
#include <QScrollBar>
#include <QStandardItemModel>
#include "ui_language.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
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
#include <utility>
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
    auto* hint=new StudioHelpHint(&dialog);
    bindUiText(hint,"toolTip","对于那些经常在多语言环境中使用该软件的人，设定第二语言是个不错的选择");
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
    return runStudioDialog(dialog)==QDialog::Accepted;
}

PreferencesDialog::PreferencesDialog(QJsonObject values,QWidget* parent,const QString& solverDefault,
                                     QJsonObject underwaterDefaults)
    :QDialog(parent),draft_(std::move(values)),underwaterDefaults_(std::move(underwaterDefaults)){
    setWindowTitle(uiText("首选项"));setStudioWindowIcon(this,"ProjectSettings");resize(638,687);setMinimumSize(580,400);
    auto* layout=new QVBoxLayout(this);auto* buttons=new QHBoxLayout;
    import_=new QPushButton(uiText("导入旧版设置…"));import_->setAutoDefault(false);import_->setProperty("preference_action","import_legacy");
    buttons->addWidget(import_);buttons->addStretch();
    layout->addLayout(buttons);
    applyTimer_=new QTimer(this);applyTimer_->setSingleShot(true);
    connect(applyTimer_,&QTimer::timeout,this,&PreferencesDialog::applyChanges);
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
        connect(box,&QComboBox::currentIndexChanged,this,[this,box,key]{draft_[key]=box->currentData().toString();scheduleApply();});return box;
    };
    auto* language=languageBox("ui_language",draft_.value("ui_language").toString("zh"));
    form->addRow(uiText("界面语言 / Language"),language);
    auto* quick=new QCheckBox(uiText("启用双语言切换 (Ctrl+Shift+L)"));quick->setProperty("preference_field","quick_language_enabled");quick->setChecked(draft_.value("quick_language_enabled").toBool());
    const auto [firstLanguage,secondLanguage]=quickUiLanguages(draft_);
    auto* first=languageBox("quick_language_first",firstLanguage);
    auto* second=languageBox("quick_language_second",secondLanguage);
    form->addRow(quick);form->addRow(uiText("第一语言"),first);form->addRow(uiText("第二语言"),second);
    first->setEnabled(quick->isChecked());second->setEnabled(quick->isChecked());
    connect(quick,&QCheckBox::toggled,this,[this,first,second](bool enabled){draft_["quick_language_enabled"]=enabled;first->setEnabled(enabled);second->setEnabled(enabled);scheduleApply();});
    auto* undo=new QSpinBox;undo->setRange(0,1048576);undo->setSuffix(" MB");undo->setValue(effective.value("undo_memory_mb").toInt());undo->setProperty("preference_field","undo_memory_mb");
    undo->setKeyboardTracking(false);
    form->addRow(uiText("撤销内存上限"),undo);connect(undo,&QSpinBox::valueChanged,this,[this](int v){draft_["undo_memory_mb"]=v;scheduleApply();});
    addPage(general,uiText("常规"));
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
        const auto commit=[this,key,edit]{
            auto d=draft_.value("directories").toObject();
            if(d[key].toString()==edit->text())return;
            d[key]=edit->text();draft_["directories"]=d;scheduleApply();
        };
        connect(edit,&QLineEdit::editingFinished,this,commit);
        connect(browse,&QPushButton::clicked,this,[this,edit,fallback,commit]{
            const auto start=edit->text().isEmpty()?fallback:edit->text();
            const auto path=studioChooseDirectory(this,uiText("选择目录"),start);
            if(!path.isEmpty()){edit->setText(QDir::toNativeSeparators(path));commit();}
        });
    }
    auto* pathNote=new StudioMessage(StudioMessageRole::Explanation,uiText("临时目录重启后生效。"));paths->addRow(pathNote);addPage(directories,uiText("目录"));buildGraphics();
    error_=new StudioMessage(StudioMessageRole::Warning);error_->setProperty("preference_error",true);layout->addWidget(error_);
    importSummary_=new QTextBrowser;importSummary_->setReadOnly(true);importSummary_->setMaximumHeight(210);importSummary_->setVisible(false);importSummary_->setProperty("preference_import_summary",true);layout->addWidget(importSummary_);
    connect(import_,&QPushButton::clicked,this,&PreferencesDialog::importLegacy);
}
void PreferencesDialog::scheduleApply(){if(onApply&&!busy_)applyTimer_->start(0);}
void PreferencesDialog::applyChanges(){if(onApply&&!busy_)onApply(draft_);}
void PreferencesDialog::importLegacy(){
    QFileDialog picker(this,uiText("导入旧版设置"));picker.setFileMode(QFileDialog::ExistingFile);
    picker.setNameFilter(uiText("FreeShip 设置 (*.ini *.dta)"));picker.setProperty("preference_file_action","import_legacy");
    if(runStudioDialog(picker)!=QDialog::Accepted || picker.selectedFiles().isEmpty())return;
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
        }
        buildGraphics();setProperty("preference_failure",QString{});error_->clear();QStringList notes;
        for(const auto& note:report["notes"].toArray())notes<<note.toString();
        report::Table sources{{},{uiText("来源：")},{},{0}};
        for(const auto& path:report["sources"].toArray())sources.rows.append({path.toString()});
        importSummary_->setProperty("studio_content_role","classified_report");
        report::setHtml(importSummary_->document(),report::message(notes.join("\n"),true,StudioMessageRole::Warning)+report::render(sources,true));importSummary_->setVisible(true);
        setProperty("preference_import_report",report);
        importPending_=true;scheduleApply();
    }catch(const std::exception& e){showFailure(QString::fromUtf8(e.what()));}
}
namespace {
QJsonObject graphicsValues(const QJsonObject& values){
    QJsonObject result;
    for(const auto* key:{"theme","graphic","underwater_display","tool_icon_size"})
        if(values.contains(key))result.insert(key,values.value(key));
    return result;
}
}

void PreferencesDialog::changeGraphics(QJsonObject values){
    if(!onApply||busy_||graphicsAction_!=GraphicsHistoryAction::None)return;
    GraphicsEdit edit{graphicsValues(draft_),graphicsValues(values)};
    if(edit.before==edit.after)return;
    pendingGraphicsEdit_=std::move(edit);
    graphicsAction_=GraphicsHistoryAction::Edit;
    draft_=std::move(values);
    scheduleApply();
}

void PreferencesDialog::applyGraphicsEdit(const GraphicsEdit& edit,bool forward){
    const auto& from=forward?edit.before:edit.after;
    const auto& to=forward?edit.after:edit.before;
    for(const auto* key:{"theme","tool_icon_size"})
        if(from.value(key)!=to.value(key))draft_.insert(key,to.value(key));
    for(const auto* group:{"graphic","underwater_display"}){
        if(from.value(group)==to.value(group))continue;
        const auto previous=from.value(group).toObject(),next=to.value(group).toObject();
        auto fields=draft_.value(group).toObject();
        // A color change must leave later edits to point sizes and other fields alone.
        for(auto it=previous.begin();it!=previous.end();++it)
            if(it.value()!=next.value(it.key()))fields.insert(it.key(),next.value(it.key()));
        for(auto it=next.begin();it!=next.end();++it)
            if(!previous.contains(it.key()))fields.insert(it.key(),it.value());
        if(fields.isEmpty()&&!to.contains(group))draft_.remove(group);
        else draft_.insert(group,fields);
    }
}

void PreferencesDialog::stepGraphicsHistory(bool redo){
    if(busy_||graphicsAction_!=GraphicsHistoryAction::None)return;
    if(redo?graphicsHistoryIndex_==graphicsHistory_.size():graphicsHistoryIndex_==0)return;
    pendingGraphicsEdit_=graphicsHistory_.at(redo?graphicsHistoryIndex_:graphicsHistoryIndex_-1);
    graphicsAction_=redo?GraphicsHistoryAction::Redo:GraphicsHistoryAction::Undo;
    applyGraphicsEdit(pendingGraphicsEdit_,redo);
    scheduleApply();
}

bool PreferencesDialog::event(QEvent* event){
    if(graphics_&&tabs_->currentWidget()==graphics_&&
       (event->type()==QEvent::ShortcutOverride||event->type()==QEvent::KeyPress)){
        const auto* key=static_cast<QKeyEvent*>(event);
        const auto modifiers=key->modifiers();
        const bool redo=modifiers==(Qt::ControlModifier|Qt::ShiftModifier);
        if(key->key()==Qt::Key_Z&&(redo||modifiers==Qt::ControlModifier)){
            if(event->type()==QEvent::KeyPress&&!key->isAutoRepeat())stepGraphicsHistory(redo);
            event->accept();
            return true;
        }
    }
    return QDialog::event(event);
}

void PreferencesDialog::buildGraphics(){
    const auto effective=effectivePreferences(draft_),graphic=effective.value("graphic").toObject();
    const auto* prior=qobject_cast<QScrollArea*>(graphics_);
    const int position=prior?prior->verticalScrollBar()->value():0;
    const int current=tabs_->currentIndex();if(graphics_){tabs_->removeTab(tabs_->indexOf(graphics_));graphics_->setParent(nullptr);graphics_->deleteLater();}
    auto* scroll=new QScrollArea;scroll->setFrameShape(QFrame::NoFrame);scroll->setWidgetResizable(true);graphics_=scroll;auto* panel=new QWidget;auto* form=new QFormLayout(panel);form->setSpacing(10);form->setRowWrapPolicy(QFormLayout::WrapLongRows);scroll->setWidget(panel);
    auto* theme=new QComboBox;addUiItem(theme,draft_.value("graphic").toObject().isEmpty()?"当前配色":"自定义配色",QString{});
    QStringList names;const auto root=QCoreApplication::applicationDirPath()+"/bridge/";
    for(const auto& sub:{"Themes",".coordinate-settings/Themes"})for(const auto& n:QDir(root+sub).entryList(QDir::Dirs|QDir::NoDotAndDotDot))if(QFileInfo::exists(root+sub+'/'+n+"/theme.ini")&&!names.contains(n))names.append(n);
    auto selectedTheme=draft_.value("theme").toString();
    if(names.contains("Default")&&names.contains("Original")){
        try{
            const auto defaults=fs::document::preferenceValues(root,{{"theme","Default"}});
            const auto original=fs::document::preferenceValues(root,{{"theme","Original"}});
            if(defaults["graphic"]==original["graphic"]){
                names.removeAll("Original");
                if(selectedTheme.compare("Original",Qt::CaseInsensitive)==0)selectedTheme="Default";
            }
        }catch(const std::exception&){
            // Keep both names when a local theme cannot be resolved.
        }
    }
    for(const auto& n:names){
        if(n==QStringLiteral("Default"))addUiItem(theme,QStringLiteral("基础配色"),n);
        else if(n==QStringLiteral("StudioLight"))addUiItem(theme,QStringLiteral("工作室浅色"),n);
        else theme->addItem(n,n);
    }
    theme->setCurrentIndex(std::max(0,theme->findData(selectedTheme)));theme->setProperty("preference_field","theme");theme->setProperty("preference_choice_id",true);form->addRow(uiText("绘图配色"),theme);
    connect(theme,&QComboBox::activated,this,[this,theme]{
        auto values=draft_;
        const auto name=theme->currentData().toString();
        if(name.isEmpty())values.remove("theme");else values["theme"]=name;
        auto graphic=values.value("graphic").toObject();
        for(auto it=graphic.begin();it!=graphic.end();)
            if(it.key().endsWith("color")||it.key()=="underwatercoloralpha")it=graphic.erase(it);else ++it;
        if(graphic.isEmpty())values.remove("graphic");else values["graphic"]=graphic;
        changeGraphics(std::move(values));
    });
    auto spin=[&](QFormLayout* target,const QString& key,const QString& caption,int value,int lo,int hi,bool nested){auto* s=new QSpinBox;s->setRange(lo,hi);s->setKeyboardTracking(false);s->setValue(value);s->setProperty("preference_field",nested?"graphic."+key:key);target->addRow(caption,s);connect(s,&QSpinBox::valueChanged,this,[this,key,nested](int v){if(nested){auto g=draft_.value("graphic").toObject();g[key]=v;draft_["graphic"]=g;}else draft_[key]=v;scheduleApply();});};
    spin(form,"pointsize",uiText("控制点大小"),graphic.value("pointsize").toInt(),1,100,true);
    spin(form,"fontsize",uiText("绘图标注字号"),graphic.value("fontsize").toInt(),1,200,true);
    spin(form,"tool_icon_size",uiText("工具栏图标大小"),effective.value("tool_icon_size").toInt(),12,128,false);
    const auto colors=fs::document::resolvedPreferenceColors(graphic);
    QVector<std::pair<QLabel*,QSpacerItem*>> colorCells;
    const auto colorGrid=[](QFormLayout* group){
        auto* grid=new QWidget;
        auto* layout=new StudioFlowLayout;
        layout->setSpacing(12);
        grid->setLayout(layout);
        group->addRow(grid);
        return layout;
    };
    const auto addColor=[&](StudioFlowLayout* grid,const QString& group,const QString& key,const QString& caption,QColor color){
        auto* cell=new QWidget;
        auto* row=new QHBoxLayout(cell);
        row->setContentsMargins(0,0,0,0);
        row->setSpacing(0);
        auto* label=new QLabel(cell);
        bindUiText(label,"text",caption);
        label->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
        auto* button=new QPushButton(cell);
        button->setAutoDefault(false);button->setProperty("preference_field",group+'.'+key);
        button->setFixedSize(52,28);
        button->setIconSize({42,18});
        QPixmap swatch(42,18);swatch.fill(color);
        button->setIcon(QIcon(swatch));
        button->setProperty("color",color);
        bindUiText(button,"accessibleName",caption);
        label->setBuddy(button);
        row->addWidget(label);
        row->addSpacing(6);
        row->addWidget(button);
        auto* spacer=new QSpacerItem(0,0,QSizePolicy::Fixed,QSizePolicy::Minimum);
        row->addSpacerItem(spacer);
        colorCells.append({label,spacer});
        grid->addWidget(cell);
        connect(button,&QPushButton::clicked,this,[this,guard=QPointer<QPushButton>(button),group,key]{
            if(!guard)return;
            const auto original=guard->property("color").value<QColor>();
            const auto color=studioChooseColor(original,this,uiText("选择颜色"),QColorDialog::DontUseNativeDialog);
            if(!guard||!color.isValid()||color.rgb()==original.rgb())return;
            auto values=draft_;
            auto fields=values.value(group).toObject();
            fields[key]=color.red()|(color.green()<<8)|(color.blue()<<16);
            values[group]=fields;changeGraphics(std::move(values));
        });
        return button;
    };
    const auto colorGroup=[&](const char* title,const char* background,const char* border){
        auto* frame=new QFrame(panel);
        frame->setObjectName("PreferenceColorGroup");
        frame->setStyleSheet(QStringLiteral("QFrame#PreferenceColorGroup{background:%1;border:1px solid %2;border-radius:7px;}")
                                 .arg(QString::fromLatin1(background),QString::fromLatin1(border)));
        auto* group=new QFormLayout(frame);
        group->setContentsMargins(12,10,12,12);
        group->setSpacing(8);
        group->setRowWrapPolicy(QFormLayout::WrapLongRows);
        auto* label=new QLabel;
        bindUiText(label,"text",QString::fromUtf8(title),"graphicColors");
        auto font=label->font();
        font.setWeight(QFont::DemiBold);
        label->setFont(font);
        label->setProperty("preference_color_group",title);
        group->addRow(label);
        form->addRow(frame);
        return group;
    };
    const auto graphicColors=[&](StudioFlowLayout* grid,std::initializer_list<std::pair<const char*,const char*>> swatches){
        for(const auto& [key,title]:swatches)
            addColor(grid,"graphic",QString::fromLatin1(key),QString::fromUtf8(title),QColor::fromRgb(colors[key].toInt()));
    };
    graphicColors(colorGrid(colorGroup("点","#eaf3ed","#bed4c7")),{{"regularpointcolor","普通控制点"},{"creasepointcolor","折角控制点"},{"cornerpointcolor","角点"},
                   {"dartpointcolor","尖点"},{"leakpointcolor","泄漏点"},{"selectcolor","选中控制点"}});
    graphicColors(colorGrid(colorGroup("线","#eaf1f5","#bfd0db")),{{"edgecolor","控制网格边线"},{"stationcolor","横剖线"},{"buttockcolor","纵剖线"},
                   {"waterlinecolor","水线"},{"diagonalcolor","斜剖线"},{"controlcurvecolor","控制曲线"},
                   {"markercolor","参考曲线"},{"curvatureplotcolor","曲率图"}});
    auto* surfaceGroup=colorGroup("面","#f5f0e8","#d8c7b3");
    const auto underwater=draft_.value("underwater_display").toObject();
    auto* shade=new QCheckBox(uiText("水下部分着色"));
    shade->setProperty("preference_field","underwater_display.shade");
    shade->setChecked(underwater["shade"].toBool(underwaterDefaults_.value("shade").toBool(true)));
    surfaceGroup->addRow(shade);
    spin(surfaceGroup,"underwatercoloralpha",uiText("新船模水下不透明度 (0–255)"),graphic.value("underwatercoloralpha").toInt(),0,255,true);
    auto* surfaceColors=colorGrid(surfaceGroup);
    const int color=underwater["color"].toInt(underwaterDefaults_.value("color").toInt(graphic["underwatercolor"].toInt()));
    auto* swatch=addColor(surfaceColors,"underwater_display","color",QStringLiteral("水下颜色"),
                          QColor(color&255,(color>>8)&255,(color>>16)&255));
    swatch->setEnabled(shade->isChecked());
    connect(shade,&QCheckBox::toggled,swatch,&QWidget::setEnabled);
    connect(shade,&QCheckBox::toggled,this,[this](bool checked){
        auto fields=draft_["underwater_display"].toObject();fields["shade"]=checked;
        draft_["underwater_display"]=fields;scheduleApply();
    });
    graphicColors(surfaceColors,{{"normalcolor","面法线"},{"layercolor","新建图层默认颜色"}});
    graphicColors(colorGrid(colorGroup("文字","#f2edf5","#d4c6dc")),{{"gridfontcolor","网格文字"},{"hydrostaticsfontcolor","静水力标注"}});
    graphicColors(colorGrid(colorGroup("背景与网格","#eef1ef","#cad3ce")),{{"viewportcolor","视图背景"},{"gridcolor","网格"}});
    const auto alignColors=[colorCells]{
        int width=112;
        for(const auto& [label,spacer]:colorCells)width=std::max(width,label->sizeHint().width());
        for(const auto& [label,spacer]:colorCells){
            spacer->changeSize(width-label->sizeHint().width(),0,QSizePolicy::Fixed,QSizePolicy::Minimum);
            auto* cell=label->parentWidget();
            cell->layout()->invalidate();
            cell->updateGeometry();
        }
    };
    alignColors();
    bindUiRefresh(panel,alignColors);
    auto* reset=new QPushButton(uiText("恢复默认绘图设置"));reset->setAutoDefault(false);reset->setProperty("preference_action","reset_graphics");form->addRow(reset);
    connect(reset,&QPushButton::clicked,this,[this]{
        auto values=draft_;
        for(const auto* key:{"theme","graphic","tool_icon_size","underwater_display"})values.remove(key);
        changeGraphics(std::move(values));
    });
    tabs_->addTab(graphics_,uiText("图形"));tabs_->setCurrentIndex(current);
    if(position)QTimer::singleShot(0,scroll,[scroll,position]{scroll->verticalScrollBar()->setValue(position);});
}
void PreferencesDialog::refreshGraphics(){try{buildGraphics();}catch(const std::exception& e){showFailure(QString::fromUtf8(e.what()));}}
void PreferencesDialog::setBusy(bool busy){busy_=busy;tabs_->setEnabled(!busy);import_->setEnabled(!busy);if(busy){setProperty("preference_failure",QString{});error_->setMessage(uiText("正在应用设置…"),StudioMessageRole::Feedback,true);}}
void PreferencesDialog::saved(){
    const auto action=std::exchange(graphicsAction_,GraphicsHistoryAction::None);
    if(action==GraphicsHistoryAction::Edit){
        graphicsHistory_.resize(graphicsHistoryIndex_);
        graphicsHistory_.append(std::move(pendingGraphicsEdit_));
        ++graphicsHistoryIndex_;
    }else if(action==GraphicsHistoryAction::Undo)--graphicsHistoryIndex_;
    else if(action==GraphicsHistoryAction::Redo)++graphicsHistoryIndex_;
    if(importPending_){graphicsHistory_.clear();graphicsHistoryIndex_=0;}
    pendingGraphicsEdit_={};
    setBusy(false);setProperty("preference_failure",QString{});
    if(action!=GraphicsHistoryAction::None)QTimer::singleShot(0,this,&PreferencesDialog::refreshGraphics);
    if(std::exchange(importPending_,false))error_->setMessage(uiText("偏好设置已保存。"),StudioMessageRole::Feedback);
    else error_->clear();
    if(std::exchange(closeAfterApply_,false))QDialog::reject();
}
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
void PreferencesDialog::showFailure(const QString& message){
    const auto action=std::exchange(graphicsAction_,GraphicsHistoryAction::None);
    if(action!=GraphicsHistoryAction::None){
        applyGraphicsEdit(pendingGraphicsEdit_,action==GraphicsHistoryAction::Undo);
        pendingGraphicsEdit_={};
        QTimer::singleShot(0,this,&PreferencesDialog::refreshGraphics);
    }
    closeAfterApply_=false;setBusy(false);setProperty("preference_failure",message);error_->setMessage(message,StudioMessageRole::Warning);
}
void PreferencesDialog::reject(){
    if(auto* field=focusWidget();field&&isAncestorOf(field))field->clearFocus();
    if(applyTimer_->isActive()){
        applyTimer_->stop();applyChanges();
    }
    if(busy_){closeAfterApply_=true;return;}
    QDialog::reject();
}
