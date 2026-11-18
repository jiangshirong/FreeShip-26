// GPL-2.0-or-later. Arithmetic adapted from FREE!ship/Plus resistance methods.
// No widget, hidden form, or Pascal calculation is used by this workflow.
#include "resistance.h"
#include "core/platform_process.h"
#include "report_language.h"
#include "calculation_tools.h"
#include "geometry_input.h"
#include "hydrostatics.h"
#include "fbm_text.h"
#include "analysis_fields.h"
#include "calculation_parameters.h"
#include "resistance_tables.h"
#include "oortmerssen_fields.h"
#include "core/oortmerssen.h"
#include "core/resistance_domain.h"
#include "hollenbach_fields.h"
#include "core/hollenbach.h"
#include "fung_fields.h"
#include "core/fung.h"
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace fs::document {
namespace {
[[noreturn]] void bad(const QString &s){throw std::invalid_argument(s.toStdString());}
const QJsonObject &tables(){static auto t=QJsonDocument::fromJson(ResistanceTables).object();return t;}
QString tr(const QString &s){auto v=tables()["strings"].toObject()[s].toString();if(v.isEmpty())bad("Missing resistance caption: "+s);return report_language::text(v,"resistance.strings."+s);}
QString userstring(int n){return report_language::text(tables()["numbered"].toObject()[QString::number(n)].toString(),"resistance.numbered."+QString::number(n));}
// Translate owned native prose/templates before any numeric or model values
// are inserted. Completed report lines and solver output are never matched.
QString reportText(const char* source){return report_language::text(QString::fromUtf8(source),"resistance.native");}
constexpr double foot=.3048,lbs=.45359237,weightconversionfactor=1000*foot*foot*foot/lbs,pi=3.1415926535897932384626433832795;
template<class T>struct Span {T *p;int n;T &operator[](int i)const{if(i<0||i>=n)bad("阻力计算数组索引越界。");return p[i];}};
template<class T,int L,int H>struct Array {
    std::array<T,H-L+1> a{};
    Array()=default;
    Array(std::initializer_list<T> values){if(values.size()!=a.size())bad("Invalid coefficient table");std::copy(values.begin(),values.end(),a.begin());}
    T &operator[](int i){if(i<L||i>H)bad(QString("阻力计算数组索引越界：%1 [%2,%3]").arg(i).arg(L).arg(H));return a[i-L];}
    const T &operator[](int i)const{if(i<L||i>H)bad("阻力计算数组索引越界。");return a[i-L];}
    operator Span<T>(){return {a.data(),int(a.size())};}
    operator Span<const T>()const{return {a.data(),int(a.size())};}
};
template<class T> auto square(T v){return v*v;}
template<class T> auto absp(T v){return std::abs(v);}
template<class T> auto sqrtp(T v){return std::sqrt(v);}
double sinp(double v){return std::sin(v);}
double cosp(double v){return std::cos(v);}
double tanp(double v){return std::tan(v);}
double logp(double v){return std::log(v)*0.43429448190325182765;}
double lnp(double v){return std::log(v);}
double expp(double v){return std::exp(v);}
double power(double a,double b){if(b==0)return 1;if(a==0&&b>0)return 0;
    if(std::abs(b)<=2147483647&&b==std::trunc(b)){if(b<0)a=1/a;auto i=uint32_t(std::abs(b));double result=1;while(i){if(i&1)result*=a;i>>=1;if(i)a*=a;}return result;}
    return expp(b*lnp(a));}
int truncp(double n){if(!std::isfinite(n)||std::abs(n)>1e9)bad("Invalid resistance integer");return int(std::trunc(n));}
QString space(int n){if(n<0||n>10000)bad("Invalid padding");return QString(n,' ');}
QString number(double n,int fmt,int precision,int decimals){if(!std::isfinite(n))bad("阻力计算产生非有限数值。");return QString::number(n,fmt==1?'g':fmt==2?'E':'f',fmt==1?precision:decimals);}
QString formatValue(double n,int width,int decimals){return number(n,0,0,decimals).rightJustified(width,' ');}
QString pad(const QString &s,int width){return s.leftJustified(width,' ');}
QString pad(double n,int decimals,int width){if(decimals<0){float v=std::max(std::abs(float(n)),1e-6f);float logarithm=lnp(v)/2.30258;decimals=std::clamp(4-truncp(logarithm),0,3);}return formatValue(n,width,decimals);}
QString text(const QString &s){return s;}
QString text(double n){auto s=number(n,2,0,9);auto pos=s.indexOf('E');if(pos>=0){auto exponent=s.mid(pos+1).toInt();s=s.left(pos+1)+(exponent<0?"-":"+")+QString::number(std::abs(exponent)).rightJustified(3,'0');}return (n>=0?" ":"")+s;}
QString pascalFormat(QString pattern,std::initializer_list<QVariant> args){
    // Three Planing captions used a comma in the format precision. The old
    // formatter printed literal "0f"/"3f" instead of the requested numbers.
    pattern.replace(QRegularExpression("%(-?[0-9]+),([0-9]+)f"),"%\\1.\\2f");
    static const QRegularExpression token("%(-?)([0-9]*)(?:\\.([0-9]+))?([sdf])");
    for(const auto &arg:args){auto m=token.match(pattern);if(!m.hasMatch())bad("Invalid report format");
        QString v=m.captured(4)=="s"?arg.toString():m.captured(4)=="d"?QString::number(arg.toInt()):QString::number(arg.toDouble(),'f',m.captured(3).isEmpty()?2:m.captured(3).toInt());
        int width=m.captured(2).toInt();v=m.captured(1).isEmpty()?v.rightJustified(width,' '):v.leftJustified(width,' ');
        pattern.replace(m.capturedStart(),m.capturedLength(),v);}
    return pattern;
}
QString lengthstr(int u){return u?"ft":"m";}
QString areastr(int u){return u?"ft²":"m²";}
QString volstr(int u){return u?"ft³":"m³";}
QString weightstr(int u){return tr(u?"rs_tons":"rs_tonnes");}
QString densitystr(int u){return u?"lbs/ft³":"t/m³";}
float findwaterviscosity(float,int){return 1.189f;} // Temperature is not serialized; Plus reloads 15 C.
int position(const QString &a,const QString &b){return b.indexOf(a)+1;}
QString trailing(QString s){if(!s.endsWith('/'))s+='/';return s;}
QString baseName(const QString &s){return QFileInfo(s).fileName();}
struct TextFile {QString name,data;qsizetype at=0;bool writing=false;};
QByteArray readBytes(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))bad("Missing calculation output: "+baseName(path));if(f.size()<=0||f.size()>2*1024*1024)bad("Invalid calculation output size");return f.readAll();}
void saveBytes(const QString &path,const QByteArray &b){QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(b)!=b.size()||!f.commit())bad("无法写入计算文件："+baseName(path));}
void newFile(TextFile &f){f.data.clear();f.at=0;f.writing=true;}
void readFile(TextFile &f){f.data=QString::fromLatin1(readBytes(f.name));f.at=0;f.writing=false;}
void closeFile(TextFile &f){if(f.writing){saveBytes(f.name,f.data.toLatin1());f.writing=false;}}
bool atEnd(const TextFile &f){return f.at>=f.data.size();}
template<class... T>void writeText(TextFile &f,const T&...v){(f.data.append(text(v)),...);if(f.data.size()>2*1024*1024)bad("Calculation file exceeds limit");}
template<class... T>void writeLine(TextFile &f,const T&...v){writeText(f,v...);f.data+="\r\n";}
template<class T>void readOne(TextFile &f,T &n){
    while(f.at<f.data.size()&&f.data[f.at].isSpace())++f.at;
    auto at=f.at;while(f.at<f.data.size()&&!f.data[f.at].isSpace())++f.at;
    bool ok=false;double value=f.data.mid(at,f.at-at).toDouble(&ok);
    if(!ok||!std::isfinite(value)||!std::isfinite(float(value)))bad("Invalid numeric calculation output");n=T(value);
}
template<class... T>void readValues(TextFile &f,T&...v){(readOne(f,v),...);}
void readLine(TextFile &f,QString &s){if(atEnd(f))bad("Incomplete calculation output");auto end=f.data.indexOf('\n',f.at);if(end<0)end=f.data.size();s=f.data.mid(f.at,end-f.at);if(s.endsWith('\r'))s.chop(1);f.at=end+1;}
void readLine(TextFile &f){QString ignored;readLine(f,ignored);}
template<class...T>void readLine(TextFile &f,T&...v){readValues(f,v...);readLine(f);}
enum Protocol {fchollen,fcfung,fcoortmer,fcseamargin,fchship,fcpowerprediction,fcclempoup,fcclemblou,fcbunkov,fccompton,fcwolfson,fcradojcic};
struct Field {
    double value=0,minimum=-1e9,maximum=1e9;int itemindex=0;bool checked=false,enabled=true,visible=true;
    QString caption,title,text;QStringList items;
    void set(double n){if(!std::isfinite(n))bad("计算参数不是有限数值。");value=std::clamp(n,minimum,maximum);}
};
struct Settings {int projectunits=0;float projectlength=1,projectbeam=1,projectdraft=1,projectwaterdensity=1.025f,projectwatertemper=15;};
struct HydroInput {float draft=0;QJsonObject data;};
float hydValue(const HydroInput &input,const QString &key){
    static const QMap<QString,QString> keys={{"lengthwaterline","lwl"},{"beamwaterline","bwl"},{"wettedsurface","wetted_area"},{"waterplanearea","waterplane_area"},{"volume","volume"},{"displacement","displacement"},{"centerofbuoyancy.x","buoyancy.0"},{"wlmin.x","wl_min.0"},{"wlmax.x","wl_max.0"},{"submin.x","sub_min.0"},{"submax.x","sub_max.0"},{"modelmin.z","model_min.2"},{"prismcoefficient","cp"},{"blockcoefficient","cb"},{"midshipcoeff","cm"},{"midshiparea","midship_area"},{"waterplanecoeff","cwp"},{"waterplaneentranceangle","entrance_angle"},{"bulbsectionarea","bulb_area"},{"bulbsectioncog.z","bulb_cog.2"}};
    auto mapped=keys.value(key);if(mapped.isEmpty())bad("Unknown hydrostatic input: "+key);
    auto parts=mapped.split('.');auto value=input.data[parts[0]];if(parts.size()==2)value=value.toArray().at(parts[1].toInt());
    if(!value.isDouble()||!std::isfinite(value.toDouble()))bad("该模型缺少阻力计算所需的静水力参数："+key);
    return float(value.toDouble());
}
#include "resistance_interpolation.inc"

class ResistanceBase {
protected:
    const Fbm &source;
    std::shared_ptr<Hydrostatics> preparedHydro;
    QJsonObject request,schema,meta,bindings;
    QString method,output,work,solverDirectory,cacheRoot,xTitle,yTitle,rightTitle;
    QMap<QString,Field> fields;
    QMap<QString,QJsonArray> curves;
    QJsonObject nativeResult;
    QJsonArray externalTools;
    QStringList resultsmemo,resultsmemo2,warnings;
    QElapsedTimer clock;
    Settings settings;
    int checks=0;
    void check(){if(QFile::exists(output+"/cancel.flag"))bad("计算已取消。");if(clock.elapsed()>110000)bad("计算超过 110 秒，已停止。");}
    Field &field(const QString &name){return fields[name];}
    void warning(const QString &s){warnings.append(s);}
    void header(const QString &title,QStringList &rows);
    void point(const QString &name,double x,double y){if(!std::isfinite(x)||!std::isfinite(y))bad("计算产生非有限曲线。");if(curves[name].size()>10000)bad("计算点数超出范围。");curves[name].append(QJsonArray{x,y});}
    void clearSeries(const QString &name){curves[name]=QJsonArray();}
    void setAxis(const QString &name,int axis){auto rows=meta["series"].toArray();for(int i=0;i<rows.size();++i){auto s=rows[i].toObject();if(s["source"].toString().toLower()==name){s["axis"]=axis;rows[i]=s;}}meta["series"]=rows;}
    void setSeriesTitle(const QString &name,const QString &title){auto rows=meta["series"].toArray();for(int i=0;i<rows.size();++i){auto s=rows[i].toObject();if(s["source"].toString().toLower()==name){s["name"]=title;rows[i]=s;}}meta["series"]=rows;}
    void pointIndex(int i,double x,double y){point(meta["series"].toArray().at(i).toObject()["source"].toString().toLower(),x,y);}
    void clearIndex(int i){clearSeries(meta["series"].toArray().at(i).toObject()["source"].toString().toLower());}
    QString safePath(const QString &path)const{auto name=QFileInfo(path).fileName();if(name.isEmpty()||name=="."||name=="..")bad("Invalid calculation filename");return work+"/"+name;}
    bool exists(const QString &path)const{if(path.endsWith(".exe",Qt::CaseInsensitive))return QFileInfo::exists(QDir(solverDirectory).filePath(baseName(path)));return QFileInfo::exists(safePath(path));}
    QString findFile(const QString &name,const QString &)const{return exists(name)?name:QString();}
    virtual void loadProperty(const QString &name,float){bad("Unknown resistance property: "+name);}
    virtual void initialise()=0;
    virtual void runCalculation()=0;
    bool nativeOort()const{return method=="oortmerssen"&&fields.value("combobox").itemindex==0&&fields.value("oortsolver").itemindex==0;}
    bool nativeHollenbach()const{return method=="hollenbach"&&fields.value("holsolver").itemindex==0;}
    bool nativeFung()const{return method=="fung"&&fields.value("fungsolver").itemindex==0;}
    bool nativeResistance()const{return nativeOort()||nativeHollenbach()||nativeFung();}
    void calculateNativeFung();
    void extractNativeFung();
    void calculateNativeOort();
    void calculateNativeHollenbach();
    void extractNativeHollenbach();
    void writeNativeResistanceCache(const QString&,const QJsonArray&,double,double,double,double,double,double);
    virtual void choiceChanged(const QString &){}
    virtual void booleanChanged(const QString &){}
    void runSolver(const QString &executable,Protocol protocol);
    void capture();
    void publish();
    QJsonArray accelerationSamples;
    std::vector<std::array<double,5>> accelerationRows;
    void prepareAccelerationTransfer();
    bool accelerationGap=false;
    void writeAccelerationSamples(bool separate=false,double margin=1.);
    QJsonObject parameters()const;
    virtual void extract()=0;
    QJsonObject hydro(float draft){if(!preparedHydro||!preparedHydro->updateCompatibleSource(source,bindings))preparedHydro=std::make_shared<Hydrostatics>(source,bindings);auto report=preparedHydro->calculate({{"mode","single"},{"draft",draft},{"heel",0},{"trim",0}},output+"/cancel.flag");if(report["cancelled"].toBool())bad(report["message"].toString());auto row=report["rows"].toArray().at(0).toObject();if(row["status"]!="ok")bad(row["message"].toString());return row;}
public:
    ResistanceBase(const Fbm &src,const QJsonObject &req,const QString &out,const QJsonObject &nativeBindings):source(src),request(req),bindings(nativeBindings),method(req["calculator"].toString()),output(out){
        clock.start();meta=tables()[method].toObject();schema=meta["schema"].toObject();
        if(method=="oortmerssen"){auto list=schema["fields"].toArray();for(auto f:oortmerssenFields())list.append(f);schema["fields"]=list;}
        if(method=="hollenbach"){auto list=schema["fields"].toArray();for(auto f:hollenbachFields())list.append(f);schema["fields"]=list;}
        if(method=="fung"){auto list=schema["fields"].toArray();for(auto f:fungFields())list.append(f);schema["fields"]=list;}
        auto g=documentGeometrySettings(src);settings.projectunits=g.imperial;settings.projectlength=g.length;settings.projectbeam=g.beam;settings.projectdraft=g.draft;settings.projectwaterdensity=g.density;
        if(g.imperial&&method!="delft"&&method!="kaper"&&method!="oortmerssen"&&method!="hollenbach"&&method!="fung")bad("该方法仅支持公制。请先在工程设置中转换单位。");
        auto cs=meta["controls"].toObject();for(auto it=cs.begin();it!=cs.end();++it){auto o=it.value().toObject();auto &f=fields[it.key()];
            f.caption=o["Caption"].toString();if(f.caption.startsWith('\''))f.caption=f.caption.mid(1,f.caption.size()-2).replace("''","'");
            f.value=o["Value"].toString().toDouble();f.itemindex=o["ItemIndex"].toString().toInt();f.enabled=o["Enabled"]!="False";f.visible=o["Visible"]!="False";f.checked=o["Checked"]=="True";
            if(o.contains("MinValue"))f.minimum=o["MinValue"].toString().toDouble();if(o.contains("MaxValue"))f.maximum=o["MaxValue"].toString().toDouble();}
        for(auto v:schema["fields"].toArray()){auto s=v.toObject();auto &f=field(s["key"].toString().toLower());f.minimum=s["minimum"].toDouble(-1e9);f.maximum=s["maximum"].toDouble(1e9);f.enabled=s["enabled"].toBool();f.visible=s["visible"].toBool();
            if(s["key"].toString().startsWith("Oort")||s["key"].toString().startsWith("Hol")||s["key"].toString().startsWith("Fung")){f.value=s["value"].toDouble();f.itemindex=s["value"].toInt();}
            for(auto x:s["choices"].toArray())f.items.append(x.toString());
            if(!s["source_label"].toString().isEmpty())field(s["source_label"].toString().toLower()).caption=s["label"].toString();
            if(!s["source_unit"].toString().isEmpty())field(s["source_unit"].toString().toLower()).caption=s["unit"].toString();}
        if(method=="oortmerssen"||method=="hollenbach"||method=="fung")
            field("viscositybox").maximum=100; // Old widget cap 1 excludes ordinary cold water.
        if(method=="delft"&&g.imperial&&req["development_sample"]==true)
            field("viscositybox").maximum/=foot*foot;
        if(method=="fung"&&req["values"].toObject().value("FungSolver").toInt()==0&&g.imperial){
            for(const auto* key:{"lwlbox","drafttotalbox","zbulbbox"})field(key).maximum/=.3048;
            for(const auto* key:{"wettedsurfacebox","babox"})field(key).maximum/=(.3048*.3048);
            field("displacementbox").maximum/=std::pow(.3048,3);
        }
        if(method=="hollenbach"&&req["values"].toObject().value("HolSolver").toInt()==0){
            field("draftbox").minimum=-30;
            if(g.imperial){
                for(const auto* key:{"lwlbox","losbox","drafttotalbox","draftbox"}){field(key).minimum/=.3048;field(key).maximum/=.3048;}
                field("wettedsurfacebox").maximum/=(.3048*.3048);field("displacementbox").maximum/=(.3048*.3048*.3048);
            }
        }
        solverDirectory=calculationToolDirectory(req["solver_directory"].toString());
        auto id=req["expected_document_id"].toString();id.remove('-');cacheRoot=QFileInfo(out).dir().filePath("performance-cache/doc-"+id+"-r"+QString::number(qint64(req.value("calculation_cache_revision").toDouble(req["expected_revision"].toDouble()))));
        if(req["development_sample"]==true)cacheRoot=out+"/sample-cache";
    }
    void setPreparedHydro(std::shared_ptr<Hydrostatics> prepared){preparedHydro=std::move(prepared);}
    virtual ~ResistanceBase()=default;
    QJsonObject run();
};

void ResistanceBase::header(const QString &title,QStringList &rows){
    QString name,designer,comment,creator;auto bytes=source.blockBytes("project_settings");qsizetype at=0;
    auto take=[&](qsizetype n){if(n<0||n>bytes.size()-at)bad("Truncated project header");auto value=bytes.mid(at,n);at+=n;return value;};
    auto string=[&](){auto length=take(4);return decodeFbmText(take(qFromLittleEndian<qint32>(length.constData())),bindings["fbm_encoding"].toString("utf8"));};
    if(source.version()>=2){name=string();designer=string();take(30);if(source.version()>=6)take(5);if(source.version()>=7)take(1);comment=string();creator=string();}
    rows<<""<<title<<"";auto field=[&](const char *label,const QString &value){rows<<pad(space(10)+tr(label),31)+" :  "+value;};
    field("rs_project",name);field("rs_designer",designer);if(!creator.isEmpty())field("rs_created_by",creator);if(!comment.isEmpty())field("rs_comment",comment);
    field("rs_filename",QFileInfo(request["model_name"].toString()).fileName());rows<<"";
    if(source.transformFields().particularsSet){auto g=documentGeometrySettings(source);auto unit=lengthstr(g.imperial);
        field("rs_design_length",pad(g.length,-1,10)+" "+unit);field("rs_design_beam",pad(g.beam,-1,10)+" "+unit);field("rs_design_draft",pad(g.draft,-1,10)+" "+unit);
        field("rs_midship_location",pad(g.split,-1,10)+" "+unit);field("rs_water_density",pad(g.density,3,10)+" "+densitystr(g.imperial));field("rs_appendage_coefficient",pad(g.appendage,4,10));rows<<"";}
    auto now=QDateTime::currentDateTime();rows<<pad(space(10)+tr("rs_date"),19)+" : "+now.date().toString("yyyy-MM-dd")<<pad(space(10)+tr("rs_time"),19)+" : "+now.time().toString("HH:mm:ss")<<"";
}
void ResistanceBase::capture(){
    auto dest=output+"/solver-files";if(!QDir().mkpath(dest))bad("无法保存计算诊断。");qint64 total=0;
    for(auto f:QDir(work).entryInfoList(QDir::Files)){if(f.isSymLink()||f.size()<=0||f.size()>2*1024*1024||total+f.size()>16*1024*1024)continue;total+=f.size();saveBytes(dest+"/"+f.fileName(),readBytes(f.filePath()));}
}
void ResistanceBase::runSolver(const QString &executable,Protocol protocol){
    check();QString name=baseName(executable),exe=QDir(solverDirectory).filePath(name);
    externalTools.append(requireCalculationTool(solverDirectory,name));
    const QStringList files={"hollenba.res","fungleib.res","oortmers.res","OUT.TXT","ostres.tmp","OUT.","clempoup.res","clemblou.res","bunkov00.res","compton0.res","wolfson0.res","radojcic.res"};
    const QStringList inputs={"TMP5.tsk","fungdata.dat","TMP6.tsk","TMPke.txt","ostdat.tmp","IN.","clempoup.dat","clemblou.dat","bunkdata.dat","cmptdata.dat","wolfdata.dat","radodata.dat"};
    const auto inputBytes=readBytes(work+"/"+inputs[protocol]);double requestedHship=0;
    if(protocol==fchship){const auto input=QString::fromLatin1(inputBytes).trimmed().split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);
        // Pascal dat[1..30] is serialized as a zero-based Span: dat[24]
        // is the 24th field, at token index 23.
        bool valid=false;requestedHship=input.size()>23?input[23].toDouble(&valid):0;if(!valid)bad("Invalid hship input series");}
    if(QFile::exists(solverDirectory+"/CONF.PRO"))saveBytes(work+"/CONF.PRO",readBytes(solverDirectory+"/CONF.PRO"));
    if(protocol==fchship){
        // The shipped config may name the author's machine. Always point the
        // private calculation at the private copy of the supplied database.
        auto config=readBytes(solverDirectory+"/RESIST.CNF");int colon=config.indexOf(':');
        if(colon<0||colon>128)bad("RESIST.CNF 的数据库配置格式无效。");
        saveBytes(work+"/RESIST.CNF",config.left(colon+1)+QByteArray("BD\\").leftJustified(47,' ')+"\r\n");
        auto db=QDir(solverDirectory).absoluteFilePath("../BD");if(QDir(db).exists()){QDir().mkpath(work+"/BD");for(int i=1;i<=4;++i){auto f="RESIST"+QString::number(i)+".BD";saveBytes(work+"/BD/"+f,readBytes(db+"/"+f));}}
    }
    const auto result=work+"/"+files[protocol];QFile::remove(result);capture();
    QProcess p;p.setProgram(exe);p.setWorkingDirectory(work);
#ifdef Q_OS_WIN
    struct Job {HANDLE h=CreateJobObjectW(nullptr,nullptr);~Job(){if(h)CloseHandle(h);}} job;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job.h||!SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))bad("无法建立独立求解器进程组。");
    p.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a){a->flags|=CREATE_NO_WINDOW;});
#endif
    fs::platform::prepareLegacySolver(p,report_language::text(fs::platform::legacySolverWineMissing,{}));
    p.start();QByteArray log;
    try{
        while(p.state()==QProcess::Starting){p.waitForStarted(20);check();}
        if(p.error()==QProcess::FailedToStart)bad(name+" 无法启动："+p.errorString());
#ifdef Q_OS_WIN
        HANDLE h=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(p.processId()));
        if(h){DWORD code=0;GetExitCodeProcess(h,&code);bool ok=code!=STILL_ACTIVE||AssignProcessToJobObject(job.h,h);CloseHandle(h);if(!ok)bad("无法跟踪求解器进程。");}
        else if(!p.waitForFinished(0))bad("无法跟踪求解器进程。");
#endif
        if(protocol==fcpowerprediction)p.write("\r\n");p.closeWriteChannel();
        while(p.state()!=QProcess::NotRunning){p.waitForFinished(20);log+=p.readAllStandardOutput()+p.readAllStandardError();if(log.size()>2*1024*1024)bad("计算程序输出过大。");check();}
        // Also the acceleration transfer tables hship writes as resist.dat/Resistp.dat.
        fs::platform::adoptLegacyOutputs(work,{QFileInfo(result).fileName(),"RESIST.dat","RESISTp.dat"});
        log+=p.readAllStandardOutput()+p.readAllStandardError();saveBytes(output+"/"+name+".log",log);capture();
        if(p.exitStatus()!=QProcess::NormalExit||p.exitCode()!=0)bad(name+" 运行失败，退出码："+QString::number(p.exitCode()));
        auto bytes=readBytes(result);auto content=QString::fromLatin1(bytes);
        static const QRegularExpression nonfinite("(?:^|[\\s=|,()])[-+]?(?:NAN|INF(?:INITY)?)(?:$|[\\s=|,()])|#(?:IND|INF)",QRegularExpression::CaseInsensitiveOption);
        if(nonfinite.match(content).hasMatch())bad("Non-finite calculation report");
        if(protocol==fcpowerprediction){auto lines=content.trimmed().split('\n');if(lines.size()<85||lines.size()>125)bad("Incomplete calculation output");}
        else{
            auto tokens=content.trimmed().split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);std::vector<double> numbers;
            for(const auto &t:tokens){bool ok;auto v=t.toDouble(&ok);if(!ok||!std::isfinite(v)||!std::isfinite(float(v)))bad("Invalid numeric calculation output");numbers.push_back(v);}
            static const int low[]={90,80,80,1,245,0,60,12,96,66,12,56},high[]={90,80,80,1,265,0,60,48,96,66,60,63},multiple[]={1,1,1,1,1,1,1,6,1,1,6,7};
            auto n=int(numbers.size());if(n<low[protocol]||n>high[protocol]||n%multiple[protocol])bad("Incomplete calculation output");
            if(protocol==fchship){auto s=numbers[0];if(s!=std::trunc(s)||s<0||s>1000)bad("Invalid hship series");if(n!=(s<=1?245:s==2?255:265))bad("Incomplete hship output");
                // Series zero asks hship to select a method automatically.
                if(requestedHship!=0&&s!=requestedHship)bad("hship 返回的方法编号与请求不符，结果未采用。");
                if(method=="mh"&&s!=0&&(s<14||s>24))bad("Invalid multihull solver series");}
        }
    }catch(...){if(p.state()!=QProcess::NotRunning){p.kill();p.waitForFinished(3000);}capture();throw;}
}
void ResistanceBase::publish(){
    prepareAccelerationTransfer();
    QDir().mkpath(cacheRoot);QLockFile lock(cacheRoot+"/native.lock");if(!lock.tryLock(0))bad("计算结果缓存正在使用。");
    auto token="set-"+QUuid::createUuid().toString(),dest=cacheRoot+"/"+token;if(!QDir().mkdir(dest))bad("无法保存阻力结果。");
    for(const auto &name:{"RESIST.dat","RESISTp.dat","RESIST-native.json","RESIST-acceleration.json","RESIST-acceleration.dat"})if(QFile::exists(work+"/"+name))saveBytes(dest+"/"+name,readBytes(work+"/"+name));
    saveBytes(cacheRoot+"/current",(token+"\r\n").toLatin1());
}
QJsonObject ResistanceBase::parameters()const{
    auto s=schema;QJsonArray rows;for(auto v:s["fields"].toArray()){auto r=v.toObject();auto f=fields[r["key"].toString().toLower()];auto kind=r["kind"].toString();
        r["value"]=kind=="boolean"?QJsonValue(f.checked):QJsonValue(kind=="choice"?double(f.itemindex):f.value);r["enabled"]=f.enabled;r["visible"]=f.visible;
        auto label=fields[r["source_label"].toString().toLower()].caption,unit=fields[r["source_unit"].toString().toLower()].caption;
        if(!label.isEmpty())r["label"]=label.trimmed();if(!unit.isEmpty())r["unit"]=unit.trimmed();
        const auto key=r["key"].toString();
        if(method!="delft"){if(key=="EndSpeedBox")r["label"]="设计航速";if(key=="StepSpeedBox")r["label"]="最大航速";if(key=="DraftBox")r["label"]="纵倾差";}
        if(key.compare("Losbox",Qt::CaseInsensitive)==0)r["unit"]="m";
        if(key=="ViscosityBox")r["unit"]=unit.contains("ft")?"10⁻⁶ ft²/s":"10⁻⁶ m²/s";
        if(key=="ViscosityBox"&&method=="delft"&&request["development_sample"]==true)
            r["maximum"]=f.maximum;
        if(key=="A11Box"&&(nativeResistance()||
            (!fields.value("estimate2box").checked&&fields.value("kebox").value!=0))){
            r["visible"]=false;r["enabled"]=false;r["used_in_calculation"]=false;
        }
        if(method=="oortmerssen"){
            if(key=="ViscosityBox")r["maximum"]=f.maximum;
            if(key=="OortSolver"&&fields.value("combobox").itemindex!=0){
                r["visible"]=false;r["enabled"]=false;r["used_in_calculation"]=false;
            }
            if(key.startsWith("Oort")&&key!="OortSolver"){r["enabled"]=nativeOort();r["visible"]=nativeOort();}
            if(nativeOort()){
                r["double_precision"]=true;
                if(key=="ViscosityBox")r["maximum"]=f.maximum;
                if(settings.projectunits==1){
                    if(r["unit"]=="m")r["unit"]="ft";else if(r["unit"]=="m²")r["unit"]="ft²";else if(r["unit"]=="m³")r["unit"]="ft³";
                    if(key=="DensityBox")r["unit"]="lb/ft³";
                    if(key=="ViscosityBox")r["unit"]="10⁻⁶ ft²/s";
                }
                if(key=="OortLpp")r["unit"]="m";
                if(key=="CpBox")r["label"]="棱形系数 Cp（以 LWL 为基准）";
                if(key=="LCBBox")r["label"]="LCB（LWL 中点为零，向艏为正）";
                if(key=="DraftBox"){r["enabled"]=true;r["label"]="纵倾差（本方法仅支持 0）";}
                if(key=="DpBox")r["unit"]="m";
                const QStringList unused={"LoSbox","BABox","ZBulbBox","KsBox","CstrnBox","K1Box","K2Box","K3Box","K4Box","K5Box","K6Box","K7Box","KeelChordLengthbox","KeelAreaBox","RudderChordLengthbox","RudderAreaBox"};
                if(unused.contains(key)||key.startsWith("A"))r["enabled"]=false;
            }
        }
        if(method=="hollenbach"){
            if(key=="ViscosityBox")r["maximum"]=f.maximum;
            if(key.startsWith("Hol")&&key!="HolSolver"){r["enabled"]=nativeHollenbach();r["visible"]=nativeHollenbach();}
            if(nativeHollenbach()){
                r["double_precision"]=true;
                if(r["kind"]=="number"){r["minimum"]=f.minimum;r["maximum"]=f.maximum;}
                if(settings.projectunits==1){
                    if(r["unit"]=="m")r["unit"]="ft";else if(r["unit"]=="m²")r["unit"]="ft²";else if(r["unit"]=="m³")r["unit"]="ft³";
                    if(key=="DensityBox")r["unit"]="lb/ft³";if(key=="ViscosityBox")r["unit"]="10⁻⁶ ft²/s";
                }
                if(key=="HolLpp"||key=="DpBox")r["unit"]="m";
                if(key=="HolAppendageArea")r["unit"]="m²";
                if(key=="DraftBox"){r["enabled"]=true;r["label"]="纵倾差 TF−TA（首减尾）";}
                if(key=="DraftTotalBox")r["label"]="首尾平均吃水";
                if(key=="Losbox"){r["enabled"]=true;r["label"]="水下长度 LOS（按船况定义）";}
                if(key=="NpBox"){r["value"]=fields.value("holcondition").itemindex==2?2:1;r["label"]="桨数（由船况确定）";}
                const QStringList unused={"CpBox","LCBBox","BABox","KBulbBox","ZBulbBox","KsBox","CstrnBox","NpBox","K1Box","K2Box","K3Box","K4Box","K5Box","K6Box","K7Box","KeelChordLengthbox","KeelAreaBox","RudderChordLengthbox","RudderAreaBox"};
                if(unused.contains(key)||key.startsWith("A")){r["enabled"]=false;if(key!="NpBox")r["used_in_calculation"]=false;}
            }
        }
        if(method=="fung"){
            if(key=="ViscosityBox")r["maximum"]=f.maximum;
            if(key.startsWith("Fung")&&key!="FungSolver"){r["enabled"]=nativeFung();r["visible"]=nativeFung();}
            if(nativeFung()){
                r["double_precision"]=true;
                if(r["kind"]=="number"){r["minimum"]=f.minimum;r["maximum"]=f.maximum;}
                if(settings.projectunits==1){
                    if(r["unit"]=="m")r["unit"]="ft";else if(r["unit"]=="m²")r["unit"]="ft²";else if(r["unit"]=="m³")r["unit"]="ft³";
                    if(key=="DensityBox")r["unit"]="lb/ft³";if(key=="ViscosityBox")r["unit"]="10⁻⁶ ft²/s";
                }
                if(key=="CpBox")r["label"]="棱形系数 Cp（以 LWL 为基准）";
                if(key=="LCBBox")r["label"]="LCB（LWL 中点为零，向艏为正）";
                if(key=="BABox"){r["label"]="浸水艉封板面积";r["unit"]=settings.projectunits==1?"ft²":"m²";}
                if(key=="ZBulbBox"){r["label"]="艉封板水线宽度";r["unit"]=settings.projectunits==1?"ft":"m";}
                if(key=="KBulbBox"){r["label"]="水线半进流角";r["unit"]="°";}
                if(key=="DpBox")r["unit"]="m";
                if(key=="WettedSurfacebox"){r["label"]="裸船湿表面积";r["enabled"]=true;}
                if(key=="DraftBox"){r["enabled"]=true;r["label"]="纵倾差（本方法仅支持 0）";}
                // A legacy preset can carry this checked flag. Let the user
                // turn it off instead of trapping it in a disabled checkbox.
                if(key=="EstimateBox"){r["enabled"]=f.checked;r["used_in_calculation"]=false;}
                const QStringList unused={"LoSbox","KsBox","CstrnBox","K1Box","K2Box","K3Box","K4Box","K5Box","K6Box","K7Box","KeelChordLengthbox","KeelAreaBox","RudderChordLengthbox","RudderAreaBox"};
                if(unused.contains(key)||key.startsWith("A")){r["enabled"]=false;r["used_in_calculation"]=false;}
            }
        }
        if(method=="ost"||method=="rbhs"||method=="mh"){
            if(key.startsWith("Edit17_"))r["label"]="弗劳德数 Fr "+key.mid(7,1);
            if(key.startsWith("Edit18_"))r["label"]="剩余阻力系数 Cr "+key.mid(7,1);}
        rows.append(r);}
    s["fields"]=rows;return s;
}
QJsonObject ResistanceBase::run(){
    // A failed new request must not silently leave the previous parameter
    // set available to propeller extraction. Defaults/extraction is read-only.
    if(request["method"]!="performance_defaults"&&QFile::exists(cacheRoot+"/current")){
        QLockFile lock(cacheRoot+"/native.lock");if(!lock.tryLock(0)||!QFile::remove(cacheRoot+"/current"))bad("无法清除上次阻力结果。");}
    check();static auto bindings=QJsonDocument::fromJson(AnalysisFields).object();auto spec=bindings[method].toObject();auto map=spec["fields"].toObject();QByteArray record;
    if(request["development_sample"]!=true&&source.version()>=spec["version"].toInt())record=source.blockBytes(spec["block"].toString());
    std::vector<QJsonObject> entries;for(auto v:map)entries.push_back(v.toObject());std::stable_sort(entries.begin(),entries.end(),[](auto a,auto b){return a["offset"].toInt()<b["offset"].toInt();});
    for(auto b:entries){int offset=b["offset"].toInt();if(offset==-1)continue;float n=0;
        if(offset==-2)n=settings.projectwaterdensity;else if(!record.isEmpty()){if(offset+4>record.size())bad("Truncated resistance record");auto bits=qFromLittleEndian<quint32>(record.constData()+offset);std::memcpy(&n,&bits,4);if(!std::isfinite(n))bad("Invalid stored resistance value");}
        loadProperty(b["member"].toString().toLower(),n);}
    // The preserved legacy initializer exits before initializing controls in
    // imperial projects. Reuse its defaults, then publish native unit labels.
    const bool oortImperial=(method=="oortmerssen"||method=="hollenbach"||method=="fung")&&settings.projectunits==1;
    if(oortImperial)settings.projectunits=0;
    initialise();
    if(oortImperial)settings.projectunits=1;
    if(request.contains("values")&&!request["values"].isObject())bad("计算参数必须为对象。");auto patch=request["values"].toObject();
    if(((method=="oortmerssen"&&patch.value("OortSolver").toInt()==0&&patch.value("ComboBox").toInt()==0)||
        (method=="hollenbach"&&patch.value("HolSolver").toInt()==0)||(method=="fung"&&patch.value("FungSolver").toInt()==0))&&settings.projectunits==1)
        field("viscositybox").set(1.189/(.3048*.3048)); // Default 15 C water, now in displayed ft²/s units.
    for(int pass=0;pass<3;++pass)for(auto it=patch.begin();it!=patch.end();++it){QJsonObject desc;for(auto v:schema["fields"].toArray())if(v.toObject()["key"]==it.key())desc=v.toObject();if(desc.isEmpty())bad("未知计算参数："+it.key());
        auto kind=desc["kind"].toString();int order=kind=="choice"?(it.key()=="ComboBox"?0:1):2;if(order!=pass)continue;auto &f=field(it.key().toLower());
        if(kind=="boolean"){if(!it.value().isBool())bad("开关值无效。");bool changed=f.checked!=it.value().toBool();f.checked=it.value().toBool();if(changed)booleanChanged(it.key().toLower());}
        else {auto n=it.value().toDouble();if(!it.value().isDouble()||!std::isfinite(n))bad("参数不是有限数值。");if(kind=="choice"){if(n!=std::trunc(n)||n<(it.key()=="OortSolver"||it.key().startsWith("Hol")||it.key()=="FungSolver"?0:-1)||n>=desc["choices"].toArray().size())bad("选项无效。");f.itemindex=int(n);choiceChanged(it.key().toLower());}
            else{if(n<f.minimum||n>f.maximum)bad("参数超出范围："+desc["label"].toString());f.set(n);}}}
    if(request.contains("extract")&&!request["extract"].isBool())bad("Invalid extraction flag");
    if(method=="delft"&&(request["extract"].toBool()||shouldFillCalculationInputs(request))){
        // Legacy records can contain speed or density while the hull fields
        // are empty. Recover them from the model, leaving saved setups intact.
        const bool missingHull=field("lwlbox").value<=0||field("bwlbox").value<=0||
            field("draftbox").value<=0||field("drafttotalbox").value<=0||
            (!field("estimatebox").checked&&field("wettedsurfacebox").value<=0)||
            field("wlareabox").value<=0||field("displacementbox").value<=0||field("cpbox").value<=0;
        if(request["extract"].toBool()||missingHull){
            const auto original=fields;
            const auto keepManualInputs=[&]{check();fields=original;};
            try{
                if(field("drafttotalbox").value<=0)field("drafttotalbox").set(settings.projectdraft);
                if(field("draftbox").value<=0)field("draftbox").set(field("drafttotalbox").value);
                extract();
            }catch(const std::runtime_error&){
                if(request["extract"].toBool())throw;
                keepManualInputs();
            }catch(const std::invalid_argument&){
                if(request["extract"].toBool())throw;
                keepManualInputs();
            }
        }
    }else if(request["extract"].toBool()){if(nativeHollenbach())extractNativeHollenbach();else if(nativeFung())extractNativeFung();else extract();}
    const auto projectValues=request["project_values"].toObject();
    for(auto it=projectValues.begin();it!=projectValues.end();++it)if(fields.contains(it.key().toLower()))field(it.key().toLower()).set(it.value().toDouble());
    if(settings.projectunits==1&&method=="fung"&&!nativeFung())bad("旧 Fung–Leibman 外部程序仅支持公制；原生计算支持英制输入。");
    if(settings.projectunits==1&&method=="oortmerssen"&&!nativeOort())bad("此窗口的旧外部方法仅支持公制；原生 van Oortmerssen 支持英制输入。");
    if(settings.projectunits==1&&method=="hollenbach"&&!nativeHollenbach())bad("旧 Hollenbach 外部程序仅支持公制；原生计算支持英制输入。");
    if(!nativeResistance()&&request["method"]!="performance_defaults"&&!solverDirectory.isEmpty()&&!QDir(solverDirectory).exists())bad("计算程序目录不存在："+solverDirectory);
    if(request["method"]=="performance_defaults"){auto p=parameters();p["solver_directory"]=QDir(solverDirectory).exists()?solverDirectory:"";return p;}
    if(method=="planing"&&field("combobox").itemindex==9)bad("原版仅列出 Almeter 名称，未提供该方法的计算实现。");
    if(fields.contains("startspeedbox")){double a=field("startspeedbox").value,b=field("endspeedbox").value,c=field("stepspeedbox").value;
        if(method=="delft"){if(a<0||b<=a||c<=0||(b-a)/c>2000)bad("航速范围无效。");}
        else if(a<=0||b<=a||c<=b)bad("航速须满足：0 < 起始航速 < 设计航速 < 最大航速。");}
    QTemporaryDir temp(QDir::tempPath()+"/fs-res-XXXXXX");if(!temp.isValid())bad("无法创建计算目录。");work=temp.path();
#ifdef Q_OS_WIN
    if(!nativeResistance()&&work.toLatin1()!=work.toUtf8()){std::wstring path=QDir::toNativeSeparators(work).toStdWString();auto n=GetShortPathNameW(path.c_str(),nullptr,0);std::wstring shortPath(n,L'\0');if(n)n=GetShortPathNameW(path.c_str(),shortPath.data(),DWORD(shortPath.size()));if(!n)bad("无法为旧求解器创建兼容目录。");shortPath.resize(n);work=QString::fromStdWString(shortPath);if(work.toLatin1()!=work.toUtf8())bad("旧求解器需要 ASCII 临时目录。");}
#endif
    if(nativeOort())calculateNativeOort();else if(nativeHollenbach())calculateNativeHollenbach();else if(nativeFung())calculateNativeFung();else runCalculation();check();if(method=="delft"||method=="kaper")writeAccelerationSamples();capture();QJsonArray series;xTitle.replace("kN","kn");
    for(auto v:meta["series"].toArray()){auto s=v.toObject();auto name=s["source"].toString();auto points=curves[name.toLower()];if(points.isEmpty())continue;s["points"]=points;auto axis=s["axis"].toInt();s.remove("axis");s["source_x_title"]=xTitle;s["source_y_title"]=axis>=2?rightTitle:yTitle;s["x_title"]="航速 (kn)";s["y_title"]=s["source_y_title"];
        bool power=name=="Series3"||name=="Series4";QString rUnit="kN",pUnit="kW";
        if((method=="holtrop"||method=="ost"||method=="rbhs"||method=="mh")&&!yTitle.contains("kN")){rUnit="N";pUnit="W";}
        if(method=="holtrop"||method=="hollenbach"||method=="oortmerssen")s["y_title"]=power?"有效功率 ("+pUnit+")":"阻力 ("+rUnit+")";
        if((method=="fung"&&!nativeFung())||method=="ost"||method=="rbhs"||method=="mh")s["y_title"]=power?"有效功率 ("+pUnit+")":"10 × 阻力 ("+rUnit+")";
        if(method=="planing")s["y_title"]=name=="Series4"?QString("有效功率 (kW)"):name=="Series5"?s["name"].toString().trimmed()+" (°)":"10 × 阻力 (kN)";
        series.append(s);}
    if(series.isEmpty()||resultsmemo.isEmpty())bad("未得到有效计算曲线。"+warnings.join("\n")+resultsmemo.join("\n").right(1200));
    publish();QJsonObject result{{"calculator",method},{"parameters",parameters()},{"series",series},{"text",resultsmemo.join("\r\n")+"\r\n"},{"help",resultsmemo2.isEmpty()?QString():resultsmemo2.join("\r\n")+"\r\n"},{"warnings",QJsonArray::fromStringList(warnings)},{"units",settings.projectunits},{"requested",request},{"elapsed_ms",double(clock.elapsed())},{"engine","C++ resistance workflow; original external solvers"}};
    if(nativeOort()){result["engine"]="C++ van Oortmerssen 1971 / Helmore 2008";result["oortmerssen"]=nativeResult;}
    if(nativeHollenbach()){result["engine"]="C++ Hollenbach 1999";result["hollenbach"]=nativeResult;}
    if(nativeFung()){result["engine"]="C++ Fung-Leibman 1995 / 1999 erratum";result["fung"]=nativeResult;}
    QJsonArray helpParts;for(const auto& part:resultsmemo2){helpParts.append(part);helpParts.append("\r\n");}
    result["help_parts"]=helpParts;
    result["external_tools"]=externalTools;return result;
}
#include "native_resistance_cache.inc"
#include "native_oortmerssen.inc"
#include "native_hollenbach.inc"
#include "native_fung.inc"
} // runtime details
#include "resistance_delft.inc"
#include "resistance_holtrop.inc"
#include "resistance_hollenbach.inc"
#include "resistance_oortmerssen.inc"
#include "resistance_fung.inc"
#include "resistance_ost.inc"
#include "resistance_rbhs.inc"
#include "resistance_mh.inc"
#include "resistance_kaper.inc"
#include "resistance_planing.inc"
namespace {
std::unique_ptr<ResistanceBase> create(const Fbm &source,const QJsonObject &request,const QString &output,const QJsonObject &bindings){auto method=request["calculator"].toString();
#include "resistance_factory.inc"
    bad("Unknown resistance method");
}
}
bool nativeResistance(const QString &method){return tables().contains(method)&&tables()[method].toObject().contains("schema");}
QJsonObject resistance(const Fbm &source,const QJsonObject &request,const QString &output,const QJsonObject &bindings,std::shared_ptr<Hydrostatics> prepared){
    report_language::Scope locale(request);
    if(request["calculator"]=="delft"){
        auto job=create(source,request,output,bindings);
        job->setPreparedHydro(prepared);
        return job->run();
    }
    static const auto fields=QJsonDocument::fromJson(AnalysisFields).object();
    const auto spec=fields[request["calculator"].toString()].toObject();
    const auto stored=shouldFillCalculationInputs(request)&&!spec.isEmpty()&&source.version()>=spec["version"].toInt()
        ?source.blockBytes(spec["block"].toString()):QByteArray{};
    return fillCalculationInputs(request,stored,[&](const QJsonObject& input){
        auto job=create(source,input,output,bindings);
        job->setPreparedHydro(prepared);
        return job->run();
    });
}
}
