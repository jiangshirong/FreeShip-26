// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui_language.h"
#include "math_notation.h"
#include "numeric_range_text.h"
#include <QApplication>
#include <QComboBox>
#include <QSignalBlocker>
#include <QAbstractButton>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QRegularExpression>
#include <QTabWidget>
#include <QGroupBox>
#include <QTimer>
#include <QTranslator>
#include <QVariant>
#include <algorithm>

namespace {
QString language="zh",lastError;
QJsonObject selected,english;
const QStringList languageChoiceIds{"zh","en","ru","ja","de","fr","es"};
// Retain UTF-8 storage across switches: several legacy UI builders retain const char*.
QHash<QString,QByteArray> strings;
QHash<QString,QString> displayedSources;
QString catalogDirectory;
QString root(){return catalogDirectory.isEmpty()?QCoreApplication::applicationDirPath()+"/languages":catalogDirectory;}
QJsonObject catalog(const QString& id,QString* error,const QString& directory = {}){
    QFile f((directory.isEmpty()?root():directory)+"/"+id+".json");
    if(!f.open(QIODevice::ReadOnly)||f.size()>4*1024*1024){*error="语言文件无法读取 / Cannot read language file: "+id;return {};}
    QJsonParseError parse;auto d=QJsonDocument::fromJson(f.readAll(),&parse);
    auto o=d.object();
    if(parse.error!=QJsonParseError::NoError||o["format"]!="FreeShipQtLanguage"||o["version"]!=1||o["id"]!=id||!o["messages"].isObject()){
        *error="语言文件格式无效 / Invalid language file: "+id;return {};
    }
    const auto messages=o["messages"].toObject();
    QRegularExpression placeholders("%L?[1-9][0-9]?|%n");
    auto tokens=[&](const QString& s){QStringList t;auto it=placeholders.globalMatch(s);while(it.hasNext())t<<it.next().captured();std::sort(t.begin(),t.end());return t;};
    for(auto it=messages.begin();it!=messages.end();++it)if(!it.value().isString()||it.value().toString().contains(QChar(0))||(!it.value().toString().isEmpty()&&tokens(it.key())!=tokens(it.value().toString()))){
        *error="语言文件条目无效 / Invalid translation entry: "+id;return {};
    }
    return o;
}
QString lookup(const QString& source){
    const auto split=source.indexOf(QChar(4));
    const auto plain=split<0?source:source.mid(split+1);
    // Units are scientific notation, not translated UI vocabulary. This also
    // protects old callers which still send a standalone unit through uiText.
    if(!notation::unitLatex(plain).isEmpty())return notation::unit(plain);
    if(language=="zh"){
        auto t=selected.value(source).toString();
        if(t.isEmpty()&&split>=0)t=selected.value(plain).toString();
        return formatNumericRanges(t.isEmpty()?plain:t);
    }
    auto t=selected.value(source).toString();if(t.isEmpty())t=english.value(source).toString();
    if(t.isEmpty()&&split>=0){t=selected.value(plain).toString();if(t.isEmpty())t=english.value(plain).toString();}
    return formatNumericRanges(t.isEmpty()?plain:t);
}
struct Binding{QPointer<QObject> object;QByteArray property;int tab=-1;QString source,last;};
QList<Binding> bindings;
struct Refresh{QPointer<QObject> owner;std::function<void()> update;};
QList<Refresh> refreshers;
constexpr int ComboSourceRole=Qt::UserRole+701;
bool applying=false;
QString value(const Binding& b){if(!b.object)return {};if(b.tab>=0)return static_cast<QTabWidget*>(b.object.data())->tabText(b.tab);return b.object->property(b.property.constData()).toString();}
void capture(QObject* object,const char* property,int tab=-1){
    if(object->property("ui_user_text").toBool())return;
    Binding b{object,property,tab,{}, {}};b.last=value(b);
    // Only text explicitly emitted by uiText is eligible; no reverse matching
    // against arbitrary model/table/input contents or translation catalogs.
    if(!displayedSources.contains(b.last)||displayedSources[b.last].isEmpty())return;
    for(auto& old:bindings)if(old.object==object&&old.property==property&&old.tab==tab){
        if(old.last!=b.last){old.source=displayedSources[b.last];old.last=b.last;}return;
    }
    b.source=displayedSources[b.last];bindings.append(b);
}
void apply(){
    applying=true;
    for(auto it=bindings.begin();it!=bindings.end();){
        if(!it->object){it=bindings.erase(it);continue;}
        // A later data-dependent update supersedes an old static caption.
        if(value(*it)==it->last){auto t=uiText(it->source);if(it->tab>=0)static_cast<QTabWidget*>(it->object.data())->setTabText(it->tab,t);else it->object->setProperty(it->property.constData(),t);it->last=t;}
        ++it;
    }
    refreshers.removeIf([](const Refresh& entry){return entry.owner.isNull();});
    const auto pending=refreshers;
    for(const auto& entry:pending)if(entry.owner)entry.update();
    for(auto* w:QApplication::topLevelWidgets())w->update();
    applying=false;
}
class Filter:public QObject{
    bool eventFilter(QObject* o,QEvent* event) override{
        if(!applying&&event->type()==QEvent::Show)if(auto* w=qobject_cast<QWidget*>(o);w&&w->isWindow())bindUiLanguage(w);
        return false;
    }
};
class StandardTranslator:public QTranslator{
public:
    bool isEmpty()const override{return false;}
    QString translate(const char* context,const char* text,const char*,int)const override{
        // Qt 6.8 PrintSupport uses its own context, not QPlatformTheme. Route
        // these static captions through the same catalogs and live bindings.
        if(QByteArray(context)=="QPrintPreviewDialog"){
            static const QHash<QString,QString> printSources{{"Print Preview","打印预览"},{"Next page","下一页"},{"Previous page","上一页"},
                {"First page","首页"},{"Last page","末页"},{"Fit width","适合页面宽度"},{"Fit page","适合整页"},
                {"Zoom in","放大"},{"Zoom out","缩小"},{"Portrait","纵向"},{"Landscape","横向"},
                {"Show single page","单页显示"},{"Show facing pages","双页显示"},{"Show overview of all pages","所有页面概览"},
                {"Print","打印"},{"Page setup","页面设置"}};
            const auto it=printSources.constFind(QString::fromUtf8(text));return it==printSources.cend()?QString{}:uiText(*it);
        }
        // QColorDialog retranslates its own widgets on LanguageChange.
        // Use its Qt 6.8 source strings, including the mnemonic markers.
        if(QByteArray(context)=="QColorDialog"){
            static const QHash<QString,QString> colorSources{
                {"Select Color","选择颜色"},{"&Basic colors","基本颜色"},{"&Custom colors","自定义颜色"},
                {"&Add to Custom Colors","添加到自定义颜色"},{"&Pick Screen Color","屏幕取色"},
                {"Hu&e:","色相："},{"&Sat:","饱和度："},{"&Val:","明度："},
                {"&Red:","红色："},{"&Green:","绿色："},{"Bl&ue:","蓝色："},
                {"A&lpha channel:","Alpha 通道："},{"&HTML:","HTML:"},
                {"Cursor at %1, %2\nPress ESC to cancel","光标位置：%1, %2\n按 Esc 取消"}};
            const auto it=colorSources.constFind(QString::fromUtf8(text));
            if(it==colorSources.cend())return {};
            if(*it==QStringLiteral("选择颜色"))return lookup(*it);
            return lookup(QStringLiteral("qt-color-dialog\x04")+*it);
        }
        if(QByteArray(context)!="QPlatformTheme")return {};
        static const QHash<QString,QString> sources{{"OK","确定"},{"Cancel","取消"},{"Save","保存"},{"Open","打开"},{"Close","关闭"},{"Apply","应用"},{"Yes","是"},{"No","否"},{"Help","帮助"}};
        const auto it=sources.constFind(QString::fromUtf8(text));return it==sources.cend()?QString{}:lookup(*it);
    }
};
void install(){static QPointer<Filter> f;static QPointer<StandardTranslator> t;if(!f&&qApp){f=new Filter;f->setParent(qApp);qApp->installEventFilter(f);t=new StandardTranslator;t->setParent(qApp);qApp->installTranslator(t);}}
}
const char* uiText(const char* source){
    const QString s=QString::fromUtf8(source),translated=lookup(s),key=s+QChar(0)+translated;
    auto it=strings.find(key);if(it==strings.end())it=strings.insert(key,translated.toUtf8());
    const QString shown=QString::fromUtf8(*it);
    // Equal Chinese captions cannot reveal which context an object owns.
    // Scoped objects bind explicitly; leave the default caption discoverable.
    if(!(language=="zh"&&s.contains(QChar(4)))){
        if(!displayedSources.contains(shown))displayedSources.insert(shown,s);
        else if(displayedSources[shown]!=s)displayedSources[shown]={};
    }
    return it->constData();
}
QString uiText(const QString& source){return QString::fromUtf8(uiText(source.toUtf8().constData()));}
const char* uiText(const char* source,const char* context){
    return context&&*context?uiText((QString::fromUtf8(context)+QChar(4)+QString::fromUtf8(source)).toUtf8().constData()):uiText(source);
}
QString uiText(const QString& source,const QString& context){
    return context.isEmpty()?uiText(source):uiText(context+QChar(4)+source);
}
QString uiTextParts(const QJsonArray& parts){
    QString text;
    for(const auto& part:parts){
        if(part.isString())text+=uiText(part.toString());
        else if(part.isObject()){
            const auto object=part.toObject();
            if(object.contains("value"))text+=object["value"].toString();
            else text+=uiText(object["source"].toString(),object["context"].toString());
        }
    }
    return text;
}
QString uiMetadataText(const QJsonObject& metadata,const char* key){
    const auto parts=metadata[QString::fromUtf8(key)+"_parts"];
    return parts.isArray()?uiTextParts(parts.toArray()):uiText(metadata[key].toString());
}
void bindUiText(QObject* object,const char* property,const QString& source,const QString& context){
    if(!object||object->property("ui_user_text").toBool())return;
    const auto key=context.isEmpty()?source:context+QChar(4)+source;
    const auto text=lookup(key);object->setProperty(property,text);
    for(auto& binding:bindings)if(binding.object==object&&binding.property==property&&binding.tab<0){
        binding.source=key;binding.last=text;return;
    }
    bindings.append(Binding{object,property,-1,key,text});
}
void bindUiRefresh(QObject* owner,std::function<void()> refresh){
    refreshers.append({owner,std::move(refresh)});
}
void addUiItem(QComboBox* combo,const QString& source,const QVariant& data){
    combo->addItem(uiText(source),data);
    combo->setItemData(combo->count()-1,source,ComboSourceRole);
    if(combo->property("ui_localized_items").toBool())return;
    combo->setProperty("ui_localized_items",true);
    bindUiRefresh(combo,[combo]{
        const QSignalBlocker guard(combo);
        for(int i=0;i<combo->count();++i){
            const auto key=combo->itemData(i,ComboSourceRole);
            if(key.isValid())combo->setItemText(i,uiText(key.toString()));
        }
    });
}
void addUiItems(QComboBox* combo,const QStringList& sources){
    for(const auto& source:sources)addUiItem(combo,source);
}
QString uiLanguage(){return language;}
QString uiLanguageError(){return lastError;}
QJsonArray uiLanguages(){
    QJsonArray result{QJsonObject{{"id","zh"},{"name","简体中文"}}};
    for(const auto& file:QDir(root()).entryList({"*.json"},QDir::Files,QDir::Name)){
        auto id=file.chopped(5);if(id=="zh"||!QRegularExpression("^[a-z]{2,3}(-[A-Za-z0-9]{2,8})?$").match(id).hasMatch())continue;
        QString error;auto c=catalog(id,&error);if(error.isEmpty()&&!c["messages"].toObject().isEmpty())result.append(QJsonObject{{"id",id},{"name",c["name"].toString(id)},{"entries",c["messages"].toObject().size()}});
    }return result;
}
QJsonArray uiLanguageChoices(){
    QJsonArray result;
    const auto installed=uiLanguages();
    for(const auto& id:languageChoiceIds)
        for(const auto& entry:installed)
            if(entry.toObject()["id"].toString()==id){result.append(entry);break;}
    return result;
}
QPair<QString,QString> quickUiLanguages(const QJsonObject& preferences){
    auto first=preferences["quick_language_first"].toString();
    auto second=preferences["quick_language_second"].toString();
    if(!languageChoiceIds.contains(first))first=second=="zh"?"ru":"zh";
    if(!languageChoiceIds.contains(second))second=first=="ru"?"zh":"ru";
    return {first,second};
}
QString setUiLanguage(const QString& id,const QString& directory){
    install();
    if(!QRegularExpression("^[a-z]{2,3}(-[A-Za-z0-9]{2,8})?$").match(id).hasMatch())return lastError="语言标识无效 / Invalid language identifier";
    QString error;QJsonObject next,en;
    if(id=="zh"){
        // An absent Chinese catalog keeps the built-in source captions usable.
        // Reviewed display overrides do not rename source/catalog identities.
        const auto directoryRoot=directory.isEmpty()?root():directory;
        if(QFile::exists(directoryRoot+"/zh.json")){
            next=catalog("zh",&error,directory)["messages"].toObject();
            if(!error.isEmpty())return lastError=error;
        }
    }else{
        en=catalog("en",&error,directory)["messages"].toObject();if(!error.isEmpty())return lastError=error;
        next=id=="en"?en:catalog(id,&error,directory)["messages"].toObject();if(!error.isEmpty())return lastError=error;
    }
    for(auto* w:QApplication::topLevelWidgets())bindUiLanguage(w);
    if(!directory.isEmpty())catalogDirectory=directory;
    language=id;selected=next;english=en;lastError.clear();
    apply();
    for(auto* w:QApplication::topLevelWidgets()){QEvent changed(QEvent::LanguageChange);QCoreApplication::sendEvent(w,&changed);}
    return {};
}
QString checkUiLanguage(const QString& id){
    if(id=="zh"){
        if(!QFile::exists(root()+"/zh.json"))return {};
        QString error;catalog("zh",&error);return error;
    }
    if(!QRegularExpression("^[a-z]{2,3}(-[A-Za-z0-9]{2,8})?$").match(id).hasMatch())return "语言标识无效 / Invalid language identifier";
    QString error;catalog("en",&error);if(error.isEmpty()&&id!="en")catalog(id,&error);return error;
}
void bindUiLanguage(QWidget* rootWidget){
    if(!rootWidget||applying)return;
    auto widgets=rootWidget->findChildren<QWidget*>();widgets.prepend(rootWidget);
    for(auto* w:widgets){
        if(w->isWindow())capture(w,"windowTitle");
        capture(w,"toolTip");
        if(qobject_cast<QLabel*>(w)||qobject_cast<QAbstractButton*>(w))capture(w,"text");
        if(qobject_cast<QGroupBox*>(w))capture(w,"title");
        if(auto* tabs=qobject_cast<QTabWidget*>(w))for(int i=0;i<tabs->count();++i)capture(tabs,"",i);
    }
    for(auto* a:rootWidget->findChildren<QAction*>()){capture(a,"text");capture(a,"toolTip");capture(a,"statusTip");}
}
