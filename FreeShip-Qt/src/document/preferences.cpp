// GPL-2.0-or-later. Defaults and precedence follow Plus FreePreferences.inc,
// FreeShip.Create and ModelService.PresentationContext. Original FREE!ship:
// Martijn van Engeland; Plus: Timoshenko V.F. and Mark Malakanov.
#include "preferences.h"
#include "dxf_palette.h"
#include "fbm_text.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStringDecoder>
#include <QStringView>
#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>
#include <stdexcept>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs::document {
namespace {
[[noreturn]] void invalid(const QString &message) {
    throw std::invalid_argument(("偏好设置：" + message).toStdString());
}
QString readText(const QString &path, qint64 limit) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > limit) invalid("无法读取或文件过大：" + path);
    auto bytes = file.readAll();
    if (file.error() != QFile::NoError) invalid("读取未完成：" + path);
    // Each buffer is a complete file. Reject a truncated final code point,
    // instead of retaining it as if another streaming chunk were forthcoming.
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    QString result = decoder.decode(bytes);
    if (decoder.hasError() || result.contains(QChar(0))) invalid("文件不是有效的 UTF-8 文本：" + path);
    if (result.startsWith(QChar(0xfeff))) result.remove(0, 1);
    return result;
}
using Ini = QHash<QString, QString>;
Ini readIni(const QString &path) {
    Ini values;
    if (!QFileInfo::exists(path)) return values;
    QString section;
    for (auto line : readText(path, 4 * 1024 * 1024).split('\n')) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(';') || line.startsWith('#')) continue;
        if (line.startsWith('[') && line.endsWith(']')) {section = line.mid(1, line.size()-2).trimmed().toLower(); continue;}
        const auto equals = line.indexOf('=');
        if (equals < 1) continue;
        const auto key = section + '/' + line.left(equals).trimmed().toLower();
        // TIniFile returns the first occurrence of a duplicate key.
        auto value=line.mid(equals+1).trimmed();
        if(value.size()>1 && (value.front()=='"' || value.front()=='\'') && value.back()==value.front()) value=value.mid(1,value.size()-2);
        if (!values.contains(key)) values.insert(key, value);
    }
    return values;
}
qint64 number(QString text, bool *ok) {
    int base = 10;
    if (text.startsWith('$')) {text.remove(0,1); base = 16;}
    else if (text.startsWith("0x", Qt::CaseInsensitive)) {text.remove(0,2); base = 16;}
    return text.toLongLong(ok, base);
}
int integer(const Ini &ini, const QString &key, int fallback) {
    if (!ini.contains(key)) return fallback;
    bool ok; const auto value = number(ini[key], &ok);
    // ReadInteger in the old INI reader retains the default for bad text.
    return ok && value >= INT_MIN && value <= INT_MAX ? int(value) : fallback;
}
bool boolean(const Ini &ini, const QString &key, bool fallback) {
    if (!ini.contains(key)) return fallback;
    const auto value = ini[key];
    // FPC TIniFile's default CharToBool uses the first character, '1'.
    // In particular, a literal "True" in an old file is not the saved form.
    return value.isEmpty() ? fallback : value.front()=='1';
}
int color(QString value) {
    static const QHash<QString, quint32> names{
        {"clblack",0},{"clmaroon",0x80},{"clgreen",0x8000},{"clolive",0x8080},
        {"clnavy",0x800000},{"clpurple",0x800080},{"clteal",0x808000},{"clgray",0x808080},
        {"clsilver",0xc0c0c0},{"clred",0xff},{"cllime",0xff00},{"clyellow",0xffff},
        {"clblue",0xff0000},{"clfuchsia",0xff00ff},{"claqua",0xffff00},{"clwhite",0xffffff},
        {"clltgray",0xc0c0c0},{"cldkgray",0x808080},{"clmoneygreen",0xc0dcc0},
        {"clskyblue",0xf0caa6},{"clcream",0xf0fbff},{"clmedgray",0xa4a0a0},
        {"clnone",0x1fffffff},{"cldefault",0x20000000},
        {"clscrollbar",0x80000000},{"clbackground",0x80000001},{"clactivecaption",0x80000002},
        {"clinactivecaption",0x80000003},{"clmenu",0x80000004},{"clwindow",0x80000005},
        {"clwindowframe",0x80000006},{"clmenutext",0x80000007},{"clwindowtext",0x80000008},
        {"clcaptiontext",0x80000009},{"clactiveborder",0x8000000a},{"clinactiveborder",0x8000000b},
        {"clappworkspace",0x8000000c},{"clhighlight",0x8000000d},{"clhighlighttext",0x8000000e},
        {"clbtnface",0x8000000f},{"clbtnshadow",0x80000010},{"clgraytext",0x80000011},
        {"clbtntext",0x80000012},{"clinactivecaptiontext",0x80000013},{"clbtnhighlight",0x80000014},
        {"cl3ddkshadow",0x80000015},{"cl3dlight",0x80000016},{"clinfotext",0x80000017},
        {"clinfobk",0x80000018},{"clhotlight",0x8000001a},{"clgradientactivecaption",0x8000001b},
        {"clgradientinactivecaption",0x8000001c},{"clmenuhighlight",0x8000001d},{"clmenubar",0x8000001e},
        {"clform",0x8000001f},{"clcolordesktop",0x80000001},{"cl3dface",0x8000000f},
        {"cl3dshadow",0x80000010},{"cl3dhighlight",0x80000014},{"cl3dhilight",0x80000014},
        {"clbtnhilight",0x80000014}
    };
    auto it = names.constFind(value.toLower());
    if (it != names.cend()) return qint32(it.value());
    bool ok; const auto n = number(value, &ok);
    if (!ok || n < INT_MIN || n > UINT_MAX) invalid("无效颜色：" + value);
    return qint32(quint32(n));
}
int rgb(int raw) {
    if (quint32(raw) & 0x80000000u) {
#ifdef Q_OS_WIN
        const int index = raw & 255;
        // LCL COLOR_FORM is its sole extra Win32 color index.
        return int(GetSysColor(index == 31 ? COLOR_BTNFACE : index) & 0xffffff);
#else
        invalid("此模型的系统颜色需要当前平台的颜色映射。");
#endif
    }
    return raw & 0xffffff;
}
QJsonObject colorValue(int raw) {
    int best = INT_MAX, index = 1;
    for (int i = 0; i < int(DxfPalette.size()); ++i) {
        int distance = 0;
        for (int shift : {0,8,16}) {
            const int d = int((quint32(raw)>>shift)&255)-((DxfPalette[i]>>shift)&255);
            distance += d*d;
        }
        if (distance < best) {best = distance; index = i+1;}
    }
    return {{"value",rgb(raw)},{"raw",raw},{"dxf",index}};
}
QString component(QString name) {
    if (name == "." || name == ".." || name.contains('/') || name.contains('\\') || name.contains(':') || name.contains(QChar(0)))
        invalid("主题名包含路径：" + name);
    return name;
}
QString themeFile(const QString &root, const QString &theme) {
    const auto suffix = "/Themes/" + component(theme) + "/theme.ini";
    const auto user = root + "/.coordinate-settings" + suffix;
    return QFileInfo::exists(user) ? user : root + suffix;
}
QString quoted(QStringView value) {
    value = value.trimmed();
    if (value.size()<2 || !value.startsWith('"') || !value.endsWith('"')) invalid("翻译文件的字符串缺少引号。");
    const auto inner=value.mid(1,value.size()-2);
    if(!inner.contains('\\') && !inner.contains('"'))return inner.toString();
    QString result;result.reserve(inner.size());
    for (qsizetype i=1; i<value.size()-1; ++i) {
        auto c=value[i];
        if (c=='"') invalid("翻译文件有未转义的引号。");
        if (c=='\\') {
            if (++i>=value.size()-1) invalid("翻译文件的转义未完成。");
            c=value[i];
            switch(c.unicode()) {
            case 'n': c='\n'; break; case 'r': c='\r'; break; case 't': c='\t'; break;
            case 'b': c='\b'; break; case 'f': c='\f'; break; case 'v': c='\v'; break;
            case 'a': c=QChar(7); break; case '\\': case '"': break;
            default: invalid("翻译文件有不支持的转义。");
            }
        }
        if (c==QChar(0)) invalid("翻译文字包含 NUL。");
        result += c;
    }
    return result;
}
class Catalogue {
    QHash<QString,QString> contextual_, generic_;
public:
    explicit Catalogue(const QString &path) {
        if (!QFileInfo::exists(path)) return; // Same untranslated source fallback.
        QString context, source, target; QString *active=nullptr; bool fuzzy=false, plural=false;
        auto flush=[&] {
            if (!source.isEmpty() && !target.isEmpty() && !fuzzy && !plural) {
                if (context.isEmpty()) generic_.insert(source,target);
                else contextual_.insert(context.toLower()+QChar(4)+source,target);
            }
            context.clear();source.clear();target.clear();active=nullptr;fuzzy=false;plural=false;
        };
        const auto text=readText(path,16*1024*1024);
        for (auto line:QStringView(text).split('\n')) {
            line=line.trimmed();
            if (line.isEmpty()) {flush();continue;}
            if (line.startsWith(u"#,")) {for(const auto &flag:line.mid(2).split(','))fuzzy |= flag.trimmed()==u"fuzzy";continue;}
            if (line.startsWith('#')) continue;
            if (line.startsWith(u"msgctxt ")) {active=&context;*active=quoted(line.mid(8));}
            else if (line.startsWith(u"msgid ")) {active=&source;*active=quoted(line.mid(6));}
            else if (line.startsWith(u"msgstr ")) {active=&target;*active=quoted(line.mid(7));}
            else if (line.startsWith(u"msgid_plural ") || line.startsWith(u"msgstr[")) {plural=true;active=nullptr;}
            else if (line.startsWith('"') && active) *active+=quoted(line);
            else if (!plural) invalid("翻译文件有无法识别的行。");
        }
        flush();
    }
    QString get(const QString &key, const QString &source) const {
        return contextual_.value("freestringsunit.rs_"+key+QChar(4)+source, generic_.value(source,source));
    }
};
}

namespace {
QHash<QString,int> defaultGraphics() {
    return {
        {"pointsize",2},{"fontsize",8},{"buttockcolor",0x808040},{"waterlinecolor",0x808040},
        {"stationcolor",0x808040},{"diagonalcolor",0x808040},{"creasecolor",0},{"gridcolor",0xc0c0c0},
        {"gridfontcolor",0xffffff},{"layercolor",0x8000},{"underwatercolor",0xf0f0f0},
        {"underwatercoloralpha",255},{"markercolor",0x8000ff},{"curvatureplotcolor",0xff00ff},
        {"controlcurvecolor",0xff0080},{"hydrostaticsfontcolor",0x80},
        {"viewportcolor",0x9f9f9f},{"edgecolor",0x6f6f6f},{"creaseedgecolor",0xff},
        {"regularpointcolor",0xe1e1e1},{"creasepointcolor",0x4080},{"cornerpointcolor",0xb95c00},
        {"dartpointcolor",0xff00ff},{"leakpointcolor",0x99ff00},{"selectcolor",0xffff},{"normalcolor",0xffffff}
    };
}
}
QJsonObject preferenceValues(const QString &directory, const QJsonObject &overrides) {
    const QString root = QDir(directory).absolutePath();
    if (!QDir(root+"/Themes").exists() || !QDir(root+"/locale").exists()) invalid("缺少程序附带的主题或翻译目录："+root);
    auto graphic=defaultGraphics();
    QString theme="Default", encoding="cp1252"; bool initialized=false; int undo=32;
    const auto globalPath=root+"/FreeShip.ini", userPath=root+"/.coordinate-settings/FreeShip.ini";
    auto config=[&](const Ini &ini) {
        theme=ini.value("graphic/theme",theme); initialized=boolean(ini,"graphic/studiothemeinitialized",initialized);
        encoding=ini.value("general/fbmencoding",encoding); undo=integer(ini,"general/maxundomemory",undo);
    };
    const auto dtaPath=root+"/FreeShipGeometryBridge.dta";
    if (!overrides.contains("theme") && !QFileInfo::exists(globalPath) && !QFileInfo::exists(userPath) && QFileInfo::exists(dtaPath)) {
        auto lines=readText(dtaPath,4*1024*1024).split('\n');
        if (!lines.isEmpty() && lines.last().isEmpty()) lines.removeLast();
        qsizetype at=0;undo=20;
        auto next=[&] {if(at>=lines.size())invalid("旧 DTA 设置不完整。");return lines[at++].trimmed();};
        const QStringList fields{"pointsize","buttockcolor","waterlinecolor","stationcolor","creasecolor",
            "creaseedgecolor","gridcolor","gridfontcolor","edgecolor","creasepointcolor","regularpointcolor",
            "cornerpointcolor","dartpointcolor","selectcolor","layercolor","underwatercolor","normalcolor","viewportcolor"};
        for (const auto &key:fields) graphic[key]=color(next());
        for (int i=0;i<4 && at<lines.size();++i) next(); // directories
        if(at<lines.size())graphic["diagonalcolor"]=color(next());
        if(at<lines.size()) {bool ok;const auto count=number(next(),&ok);if(!ok || count<0 || count>100000 || count>lines.size()-at)invalid("旧 DTA 最近文件数量无效。");at+=count;}
        for(const QString key:{"leakpointcolor","markercolor","curvatureplotcolor","controlcurvecolor","hydrostaticsfontcolor","zebrastripecolor"})
            if(at<lines.size())graphic[key]=color(next());
        if(at<lines.size())next(); // window bounds
        if(at<lines.size())next(); // language does not change bridge captions
        if(at<lines.size()) {bool ok;const auto value=number(next(),&ok);if(!ok || value<INT_MIN || value>INT_MAX)invalid("旧 DTA 撤销内存无效。");undo=int(value);}
    } else {
        config(readIni(globalPath)); config(readIni(userPath));
        if (!initialized && theme.compare("Default",Qt::CaseInsensitive)==0 && QFileInfo::exists(themeFile(root,"StudioLight"))) theme="StudioLight";
        if(overrides.contains("theme")){
            if(!overrides["theme"].isString() || overrides["theme"].toString().isEmpty())invalid("主题名无效。");
            theme=overrides["theme"].toString();
            if(!QFileInfo::exists(themeFile(root,theme)))invalid("找不到主题："+theme);
        }
        const auto child=readIni(themeFile(root,theme));
        const auto parent=readIni(themeFile(root,child.value("theme/parenttheme")));
        // Deliberately one parent, as in the existing reader; no recursive cycle.
        for (const auto &ini:{parent,child}) {
            for(auto it=graphic.begin();it!=graphic.end();++it) {
                const auto key="graphic/"+it.key();
                if (!ini.contains(key)) continue;
                it.value()=it.key().endsWith("color") ? color(ini[key]) : integer(ini,key,it.value());
            }
        }
    }
    if(overrides.contains("fbm_encoding")){
        if(!overrides["fbm_encoding"].isString())invalid("FBM 编码无效。");
        encoding=overrides["fbm_encoding"].toString();
    }
    auto checkedInteger=[](QJsonValue v,qint64 low,qint64 high){
        double x=v.toDouble();if(!v.isDouble() || !std::isfinite(x) || std::floor(x)!=x || x<low || x>high)invalid("数值超出支持范围。");return int(x);
    };
    if(overrides.contains("undo_memory_mb"))undo=checkedInteger(overrides["undo_memory_mb"],0,1048576);
    if(overrides.contains("graphic")){
        if(!overrides["graphic"].isObject())invalid("图形设置无效。");
        const auto changes=overrides["graphic"].toObject();
        for(auto it=changes.begin();it!=changes.end();++it){
            if(!graphic.contains(it.key()))invalid("未知图形设置："+it.key());
            graphic[it.key()]=checkedInteger(it.value(),INT_MIN,INT_MAX);
        }
    }
    const int point=graphic["pointsize"], font=graphic["fontsize"], alpha=graphic["underwatercoloralpha"];
    if (point<1 || point>100 || font<1 || font>200 || alpha<0 || alpha>255 || undo>1048576) invalid("点大小、字号、透明度或撤销内存超出支持范围。");
    encodeFbmText("FreeShip",encoding); // Reject unsupported encodings before adopting a document.
    QJsonObject fields;for(auto it=graphic.begin();it!=graphic.end();++it)fields[it.key()]=it.value();
    return {{"theme",theme},{"fbm_encoding",encoding},{"undo_memory_mb",std::max(0,undo)},{"graphic",fields}};
}
QJsonObject importLegacyPreferences(const QString &filename, const QString &resourceDirectory) {
    const QFileInfo file(filename);
    if(!file.isFile())invalid("找不到旧设置文件："+filename);
    QJsonObject values,graphic,paths;QStringList notes,sources;
    const auto defaults=defaultGraphics();
    auto loadIni=[&](const QString& path){sources.append(QFileInfo(path).absoluteFilePath());return readIni(path);};
    auto numeric=[&](const QString& key,const QString& text,int lo,int hi,QJsonObject& target,const QString& field){
        bool ok=false;const auto n=number(text,&ok);
        if(!ok || n<lo || n>hi){notes.append(key+" 数值无效，未采用该值。");return;}
        target[field]=int(n);
    };
    auto graphicValue=[&](const QString& key,const QString& text){
        if(key.endsWith("color")){
            try{graphic[key]=color(text);}catch(const std::exception&){notes.append(key+" 颜色无效，未采用该值。");}
        }else numeric(key,text,key=="underwatercoloralpha"?0:1,key=="fontsize"?200:key=="pointsize"?100:255,graphic,key);
    };
    auto directory=[&](const QString& key,const QString& text){
        // Old relative paths depend on the process working directory, which
        // cannot be recovered from a copied INI. Never guess a different base.
        if(text.isEmpty()){paths[key]=QString{};return;}
        if(text.size()>32767 || !QFileInfo(text).isAbsolute() || !QFileInfo(text).isDir()){
            notes.append("未导入目录（不存在或不是完整路径）："+text);return;
        }
        paths[key]=QDir::cleanPath(text);
    };
    if(file.suffix().compare("dta",Qt::CaseInsensitive)==0){
        sources.append(file.absoluteFilePath());auto lines=readText(filename,4*1024*1024).split('\n');
        if(!lines.isEmpty() && lines.back().isEmpty())lines.removeLast();
        int at=0;auto next=[&](){if(at>=lines.size())invalid("旧 DTA 文件不完整。");return lines[at++].trimmed();};
        for(const QString key:{"pointsize","buttockcolor","waterlinecolor","stationcolor","creasecolor","creaseedgecolor","gridcolor","gridfontcolor","edgecolor","creasepointcolor","regularpointcolor","cornerpointcolor","dartpointcolor","selectcolor","layercolor","underwatercolor","normalcolor","viewportcolor"})graphicValue(key,next());
        for(const QString key:{"open","save","import","export"})if(at<lines.size())directory(key,next());
        if(at<lines.size())graphicValue("diagonalcolor",next());
        if(at<lines.size()){
            bool ok=false;auto count=number(next(),&ok);
            if(!ok || count<0 || count>100000 || count>lines.size()-at)invalid("旧 DTA 最近文件列表不完整。");
            at+=int(count);
        }
        for(const QString key:{"leakpointcolor","markercolor","curvatureplotcolor","controlcurvecolor","hydrostaticsfontcolor"})if(at<lines.size())graphicValue(key,next());
        if(at<lines.size()){next();notes.append("斑马纹颜色尚无对应设置，未导入。");}
        if(at<lines.size())next(); // Original window bounds: deliberately not adopted.
        if(at<lines.size())next(); // Language belongs to the translation workflow.
        if(at<lines.size())numeric("MaxUndoMemory",next(),0,1048576,values,"undo_memory_mb");
        if(at<lines.size())notes.append("DTA 文件末尾的额外字段未导入。");
    }else if(file.suffix().compare("ini",Qt::CaseInsensitive)==0){
        Ini config;QStringList themeRoots;
        const QDir selected(file.absolutePath());
        // The portable layout has an explicit global/user pair. Do not scan
        // other profiles, the current directory, or a registry installation.
        if(selected.dirName()==".coordinate-settings"){
            auto parent=selected;parent.cdUp();const auto global=parent.filePath("FreeShip.ini");
            if(QFileInfo(global).isFile())config=loadIni(global);
            themeRoots<<selected.absolutePath()<<parent.absolutePath();
        }else themeRoots<<selected.filePath(".coordinate-settings")<<selected.absolutePath();
        const auto chosen=loadIni(file.absoluteFilePath());for(auto it=chosen.begin();it!=chosen.end();++it)config[it.key()]=it.value();
        if(config.contains("general/maxundomemory"))numeric("MaxUndoMemory",config["general/maxundomemory"],0,1048576,values,"undo_memory_mb");
        if(config.contains("general/fbmencoding")){
            try{encodeFbmText("FreeShip",config["general/fbmencoding"]);values["fbm_encoding"]=config["general/fbmencoding"];}
            catch(const std::exception&){notes.append("FBM 文字编码不受支持，保留当前设置。");}
        }
        if(config.contains("graphic/tooliconsize"))numeric("ToolIconSize",config["graphic/tooliconsize"],12,128,values,"tool_icon_size");
        for(const auto& pair:{std::pair{"open","opendirectory"},{"save","savedirectory"},{"import","importdirectory"},{"export","exportdirectory"},{"solver","execdirectory"},{"temporary","tempdirectory"}}){
            const auto key=QString("directories/")+pair.second;if(config.contains(key))directory(pair.first,config[key]);
        }
        if(config.contains("graphic/theme")){
            auto findTheme=[&](const QString& name){
                if(name.isEmpty())return QString{};
                const auto suffix="/Themes/"+component(name)+"/theme.ini";
                for(const auto& root:themeRoots)if(QFileInfo(root+suffix).isFile())return root+suffix;
                return QString{};
            };
            QString theme=config["graphic/theme"];component(theme);
            if(theme.compare("Default",Qt::CaseInsensitive)==0 && !boolean(config,"graphic/studiothemeinitialized",false) && !findTheme("StudioLight").isEmpty())theme="StudioLight";
            const auto themePath=findTheme(theme);
            if(themePath.isEmpty())notes.append("未找到旧主题 "+theme+"，绘图配色保留当前设置；请连同原 Themes 文件夹一起选择配置。");
            else{
                const auto child=loadIni(themePath);const auto parentName=child.value("theme/parenttheme");
                const auto parentPath=findTheme(parentName);
                if(!parentName.isEmpty() && parentPath.isEmpty())notes.append("旧主题缺少父主题 "+parentName+"，绘图配色保留当前设置。");
                else{
                    const auto parent=parentPath.isEmpty()?Ini{}:loadIni(parentPath);
                    // Exactly one parent, matching FreePreferences.LoadTheme.
                    // Snapshot the resolved palette so a later move of the old
                    // install cannot change Qt's saved appearance.
                    for(auto it=defaults.begin();it!=defaults.end();++it)graphic[it.key()]=it.value();
                    for(const auto& ini:{parent,child})for(auto it=ini.begin();it!=ini.end();++it){
                        if(!it.key().startsWith("graphic/"))continue;
                        const auto key=it.key().mid(8);
                        if(defaults.contains(key))graphicValue(key,it.value());
                        else if(key!="theme" && key!="tooliconsize")notes.append("主题字段 "+key+" 尚无对应设置，未导入。");
                    }
                    notes.append("已读取主题 "+theme+" 的绘图配色；主窗口仍使用 StudioLight。");
                }
            }
        }
    }else invalid("请选择旧程序的 FreeShip.ini 或 FreeShip.dta。");
    if(!graphic.isEmpty())values["graphic"]=graphic;
    if(!paths.isEmpty())values["directories"]=paths;
    if(values.isEmpty())invalid("未找到可导入的设置。"+notes.join('\n'));
    // Reuse the native publisher's validation; this never writes configuration.
    QJsonObject native=values;native.remove("directories");native.remove("tool_icon_size");
    preferenceValues(resourceDirectory,native);
    notes.append("语言、最近文件和窗口布局未导入。");notes.removeDuplicates();
    int count=values.size()-(values.contains("graphic")?1:0)-(values.contains("directories")?1:0)+graphic.size()+paths.size();
    return {{"values",values},{"count",count},{"notes",QJsonArray::fromStringList(notes)},{"sources",QJsonArray::fromStringList(sources)}};
}
QJsonObject resolvedPreferenceColors(const QJsonObject &graphic){
    QJsonObject out;
    // TColor/COLORREF stores red in the low byte. Qt's QRgb stores it in
    // the high byte. Keep native publication's historical wire format intact.
    for(auto it=graphic.begin();it!=graphic.end();++it)if(it.key().endsWith("color")){
        const int bgr=rgb(it.value().toInt());
        out[it.key()]=((bgr&255)<<16)|(bgr&0xff00)|((bgr>>16)&255);
    }
    return out;
}
QJsonObject documentContext(const QString &directory, const QJsonArray &requestedColors, const QJsonObject &overrides) {
    const auto values=preferenceValues(directory,overrides);const auto fields=values["graphic"].toObject();
    QHash<QString,int> graphic;for(auto it=fields.begin();it!=fields.end();++it)graphic[it.key()]=it.value().toInt();
    const auto encoding=values["fbm_encoding"].toString();const int undo=values["undo_memory_mb"].toInt();
    const int point=graphic["pointsize"], font=graphic["fontsize"], alpha=graphic["underwatercoloralpha"];
    const QString root=QDir(directory).absolutePath();
    if(requestedColors.size()>100002)invalid("颜色数量超过限制。");
    Catalogue strings(root+"/locale/FreeShip.cn.po");
    QJsonObject colors;
    for(const QString name:{"station","buttock","waterline","diagonal","crease","grid"}) colors[name]=colorValue(graphic[name+"color"]);
    colors["red"]=colorValue(255);
    QJsonObject presentation{{"colors",colors},{"texts",QJsonObject{{"base",strings.get("base","Base")},
        {"dwl",strings.get("dwl","DWL")},{"center",strings.get("center","Center")}}},
        {"lateral",strings.get("lateral_area","Lateral area")},{"wind",strings.get("lateral_wind_area","Lateral wind area")},
        {"decimal_separator","."},{"curvature_color",rgb(graphic["curvatureplotcolor"])},
        {"control_curve_color",rgb(graphic["controlcurvecolor"])},{"marker_color",rgb(graphic["markercolor"])},
        {"normal_color",overrides.contains("theme") || overrides["graphic"].toObject().contains("normalcolor") ? rgb(graphic["normalcolor"]) : 0xc0c0c0},{"grid_font_color",rgb(graphic["gridfontcolor"])},
        {"hydro_color",rgb(graphic["hydrostaticsfontcolor"])},{"point_size",point},{"font_size",font}};
    QJsonArray palette, sections;
    for(auto c:DxfPalette)palette.append(c);
    for(const QString name:{"station","buttock","waterline","diagonal"}) {
        const auto c=colors[name].toObject();sections.append(QJsonObject{{"color",c["value"]},{"dxf",c["dxf"]}});
    }
    QJsonObject resolved;
    auto resolve=[&](int raw){resolved[QString::number(raw)]=rgb(raw);};
    for(int raw:{graphic["layercolor"],graphic["underwatercolor"],0x8000,0xf0f0f0})resolve(raw);
    for(auto value:requestedColors) {
        const auto n=value.toDouble();
        if(!value.isDouble() || !std::isfinite(n) || std::floor(n)!=n || n<INT_MIN || n>INT_MAX)invalid("模型中的颜色值无效。");
        resolve(int(n));
    }
    // TFreeShip(nil) creates these objects BEFORE preferences.Load. Keep those
    // constructor defaults distinct from theme defaults used by later layers.
    return {{"fbm_encoding",encoding},{"layer_name_prefix",strings.get("layer","Layer")},
        {"default_layer_color",graphic["layercolor"]},{"default_underwater_color",graphic["underwatercolor"]},
        {"default_underwater_alpha",alpha},{"initial_layer_color",overrides.contains("theme") || overrides["graphic"].toObject().contains("layercolor") ? graphic["layercolor"] : 0x8000},{"initial_layer_id",0},
        {"initial_underwater_color",overrides.contains("theme") || overrides["graphic"].toObject().contains("underwatercolor") ? graphic["underwatercolor"] : 0xf0f0f0},
        {"initial_underwater_alpha",overrides.contains("theme") || overrides["graphic"].toObject().contains("underwatercoloralpha") ? alpha : 255},
        {"history_limit_bytes",qint64(std::max(0,undo))*1024*1024},{"presentation_context",presentation},
        {"plate_context",QJsonObject{{"part",strings.get("part","Part")},{"palette",palette},{"sections",sections}}},
        {"resolved_colors",resolved}};
}
}
