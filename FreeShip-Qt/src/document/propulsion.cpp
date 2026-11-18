// GPL-2.0-or-later. Protocols and postprocessing adapted from Plus
// FreePropeller_Task1..5Dlg (Timoshenko V.F., 2007-2013). No GUI or Pascal
// component participates in this workflow. See NATIVE-PROPULSION.md.
#include "propulsion.h"
#include "core/platform_process.h"
#include "report_language.h"
#include "calculation_tools.h"
#include "analysis_fields.h"
#include "calculation_parameters.h"
#include "propulsion_tables.h"
#include "geometry_input.h"
#include "core/bseries.h"
#include "core/propulsion_passport.h"
#include "core/propeller_design.h"
#include <QCoreApplication>
#include <QCryptographicHash>
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
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace fs::document {
namespace {
[[noreturn]] void bad(const QString &s){throw std::invalid_argument(s.toStdString());}
const QJsonObject &tables(){static auto t=QJsonDocument::fromJson(PropulsionTables).object();return t;}
QString r(const char *s){return report_language::text(tables()["strings"].toObject()[s].toString(),"propulsion.strings."+QString::fromUtf8(s));}
QString userstring(int i){return report_language::text(tables()["numbered"].toObject()[QString::number(i)].toString(),"propulsion.numbered."+QString::number(i));}
QString space(int n){return QString(n,' ');}
QString pad(const QString &s,int n){return s.leftJustified(n,' ');}
QString format(double v,int,int,int decimals){if(!std::isfinite(v))bad("计算产生非有限结果。");return QString::number(v,'f',decimals);}
QString fixed(double v,int width,int decimals){return format(v,0,0,decimals).rightJustified(width,' ');}
float sqr(float v){return v*v;}
QByteArray read(const QString &path,qint64 limit=2*1024*1024){
    QFile f(path);if(!f.open(QIODevice::ReadOnly))bad("Missing calculation output: "+QFileInfo(path).fileName());
    if(f.size()<=0||f.size()>limit)bad("Invalid calculation output size: "+QFileInfo(path).fileName());
    return f.readAll();
}
void write(const QString &path,const QByteArray &b){QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(b)!=b.size()||!f.commit())bad("无法写入计算文件："+QFileInfo(path).fileName());}
QStringList lines(const QString &path){auto s=QString::fromLatin1(read(path)).replace("\r\n","\n").split('\n');if(s.last().isEmpty())s.removeLast();return s;}
std::vector<float> numbers(QString s,int count=-1){
    auto tokens=s.trimmed().split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);std::vector<float> out;
    for(const auto &t:tokens){bool ok=false;double v=t.toDouble(&ok);if(!ok||!std::isfinite(v)||!std::isfinite(float(v)))bad("Invalid numeric calculation output");out.push_back(float(v));}
    if(count>=0&&out.size()!=size_t(count))bad("Incomplete calculation output");
    return out;
}
float real(const QByteArray &b,int at){if(at<0||at+4>b.size())bad("Invalid propulsion record");auto bits=qFromLittleEndian<quint32>(b.constData()+at);float x;std::memcpy(&x,&bits,4);if(!std::isfinite(x))bad("Invalid stored propulsion number");return x;}

// The same local quadratic interpolation as Plus SFINEX1. One-based report
// arrays are passed explicitly. Guard denominators before publishing a result.
template<size_t N> void interpolate(int n,const float (&inputX)[N],const float (&inputY)[N],float p,float &value){
    if(n<2||n>=int(N))bad("Invalid propulsion interpolation size");
    float x[N]{},y[N]{};const bool reversed=inputX[n]<inputX[1];
    for(int i=1;i<=n;++i){const int source=reversed?n+1-i:i;x[i]=inputX[source];y[i]=inputY[source];
        if(i>1&&x[i]<=x[i-1])bad("推进插值坐标须严格单调。");}
    if(p<=x[1])value=y[1]+(y[2]-y[1])*(p-x[1])/(x[2]-x[1]);
    else if(p>x[n])value=y[n]+(y[n]-y[n-1])*(p-x[n])/(x[n]-x[n-1]);
    else if(p<=x[2])value=(p-x[3])/(x[1]-x[2])*((p-x[2])/(x[1]-x[3])*y[1]-(p-x[1])/(x[2]-x[3])*y[2])+(p-x[1])*(p-x[2])*y[3]/((x[3]-x[1])*(x[3]-x[2]));
    else if(p>x[n-1])value=(p-x[n])/(x[n-2]-x[n-1])*((p-x[n-1])/(x[n-2]-x[n])*y[n-2]-(p-x[n-2])*y[n-1]/(x[n-1]-x[n]))+(p-x[n-2])*(p-x[n-1])/(x[n]-x[n-2])*y[n]/(x[n]-x[n-1]);
    else for(int i=3;i<n;++i)if(p>x[i-1]&&p<=x[i])value=.5*((p-x[i-1])*(p-x[i])*(y[i-2]/((x[i-2]-x[i-1])*(x[i-2]-x[i]))+y[i+1]/((x[i+1]-x[i-1])*(x[i+1]-x[i])))+(p-x[i])*((p-x[i-2])/(x[i-1]-x[i-2])+(p-x[i+1])/(x[i-1]-x[i+1]))*y[i-1]/(x[i-1]-x[i])+(p-x[i-1])*((p-x[i-2])/(x[i]-x[i-2])+(p-x[i+1])/(x[i]-x[i+1]))*y[i]/(x[i]-x[i-1]));
    if(!std::isfinite(value))bad("推进插值产生无效结果。");
}

class Calculation {
    int task;
    QJsonObject schema,request;
    QMap<QString,QJsonObject> fields;
    QMap<QString,QJsonArray> points;
    QStringList report;
    QString output,solverDirectory,cacheRoot,cacheDirectory;
    QByteArray pendingPropeller;
    QByteArray pendingPropellerMetadata;
    QJsonArray externalTools;
    QElapsedTimer timer;
    float density;
    void check() const {if(QFile::exists(output+"/cancel.flag"))bad("计算已取消。");if(timer.elapsed()>110000)bad("计算超过 110 秒，已停止。");}
    void control(const QString &key,double v){auto &f=fields[key];if(f.isEmpty())bad("未知计算参数："+key);if(f["kind"]=="number")v=std::clamp(v,f["minimum"].toDouble(),f["maximum"].toDouble());f["value"]=v;}
    void enabled(int n,bool b){fields["Edit"+QString::number(n)]["enabled"]=b;}
    void hydrate(const Fbm &source);
    void choices(bool family,bool series);
    void extract();
    void runSolver(const QString &name,const QString &work,const QStringList &outputs,bool enter=false);
    void capture(const QString &work);
    void task123(const QString &work);
    void task4(const QString &work);
    void task5(const QString &work);
    void legacy1(float z,float ae_ao,const QString &strp);
    void legacy2(float z,float ae_ao,const QString &strp);
    void legacy3(float z,float ae_ao,const QString &strp);
    std::vector<std::vector<float>> resistance(bool headerOnly=false);
    void resistance_curves(int km,int kt);
    void task3_engine_curves(int km,int kt);
    void publish_propeller(float dp,float pd,float area,float blades,float j,float kt,float kq,float eta);
    void publishCache();
    void line(const QString &s){report.append(s);}
    void point(const QString &name,double x,double y){if(!std::isfinite(x)||!std::isfinite(y))bad("计算产生非有限曲线数据。");points[name.toLower()].append(QJsonArray{x,y});}
    float d(const QString &key) const {return float(fields[key]["value"].toDouble());}
    float d(int n) const {return d("Edit"+QString::number(n));}
    void set(const QString &key,float v){control(key,v);}
    void set(int n,float v){set("Edit"+QString::number(n),v);}
    float dpr=0;
    float res[6][34]{};
    QString axisY;
    QJsonObject nativeResult;
    QJsonObject engineCurve;
    QJsonArray warnings;
    QJsonArray solverInvocations;
    bool nativeB() const {return task==5&&int(d("ComboBox"))==0;}
    bool nativePassport() const {return task==3&&d("PassportSolver")==0&&d(7)>=1&&d(7)<=4;}
    bool nativeDesign() const {const double diagram=d(task==1?14:12);return task<=2&&d("DesignSolver")==0&&diagram>=1&&diagram<=4;}
    void task5B();
    void task3B();
    void designB();
    QJsonObject parameters() const {auto s=schema;QJsonArray a;for(auto v:s["fields"].toArray())a.append(fields[v.toObject()["key"].toString()]);s["fields"]=a;return s;}
public:
    Calculation(const Fbm &source,const QJsonObject &req,const QString &out):task(req["calculator"].toString().right(1).toInt()),request(req),output(out),density(documentGeometrySettings(source).density){
        timer.start();check();if(documentGeometrySettings(source).imperial)bad("该方法仅支持公制。请先在工程设置中转换单位。");
        schema=tables()["schema"].toObject()[req["calculator"].toString()].toObject();
        if(task<=2){
            auto list=schema["fields"].toArray();
            for(int i=0;i<list.size();++i){auto f=list[i].toObject();f["double_precision"]=true;list[i]=f;}
            list.append(QJsonObject{{"key","DesignSolver"},{"label","计算方法"},{"group","计算条件"},
                {"kind","choice"},{"choices",QJsonArray{"按图谱选择（B 系列内置初选）","旧 CALCPROP（经验校核）"}},{"value",0}});
            for(const auto& key:{QString("DesignPitchLow"),QString("DesignPitchHigh"),QString("DesignDiameterLimit")}){
                if(key=="DesignDiameterLimit"&&task==1)continue;
                const bool diameter=key=="DesignDiameterLimit",low=key=="DesignPitchLow";
                list.append(QJsonObject{{"key",key},{"label",diameter?"船体直径限制（0 表示未指定）":low?"螺距比下限":"螺距比上限"},
                    {"group","内置初选范围"},{"unit",diameter?"m":""},{"kind","number"},{"minimum",diameter?0:.5},
                    {"maximum",diameter?25:1.4},{"decimals",6},{"value",diameter?0:low?.5:1.4},{"double_precision",true}});
            }
            schema["fields"]=list;
        }
        if(task==3){
            auto list=schema["fields"].toArray();
            for(int i=0;i<list.size();++i){auto f=list[i].toObject();f["double_precision"]=true;
                if(f["key"]=="Edit11")f["label"]="推力减额 t";
                if(f["key"]=="Edit12")f["label"]="相对旋转效率 ηR（旧 i1）";
                list[i]=f;}
            list.append(QJsonObject{{"key","PassportSolver"},{"label","计算方法"},{"group","计算条件"},
                {"kind","choice"},{"choices",QJsonArray{"按图谱选择（B 系列内置）","旧 CALCPROP（对照）"}},{"value",0}});
            schema["fields"]=list;
        }
        if(task==4) {
            auto list=schema["fields"].toArray();list.append(QJsonObject{
                {"key","WaterViscosity"},{"label","水的运动黏度"},{"unit","10⁻⁶ m²/s"},
                {"group","水体条件"},{"kind","number"},{"minimum",.01},{"maximum",100},
                {"decimals",6},{"value",double(1.189f)},{"enabled",true},{"visible",true}});
            schema["fields"]=list;
        }
        if(task==5){
            auto list=schema["fields"].toArray();
            list.append(QJsonObject{{"key","BSeriesReynoldsMode"},{"label","雷诺数处理"},{"group","敞水条件"},
                {"kind","choice"},{"choices",QJsonArray{"基准 Re=2×10⁶（不修正）","指定雷诺数（论文修正式）"}},{"value",0}});
            list.append(QJsonObject{{"key","BSeriesReynoldsMillions"},{"label","0.75R 处雷诺数"},{"unit","10⁶"},
                {"group","敞水条件"},{"kind","number"},{"minimum",2},{"maximum",2000},{"decimals",6},
                {"value",20},{"double_precision",true}});
            schema["fields"]=list;
        }
        for(auto v:schema["fields"].toArray()){auto f=v.toObject();fields.insert(f["key"].toString(),f);}
        if(req["development_sample"]!=true)hydrate(source);
        solverDirectory=calculationToolDirectory(req["solver_directory"].toString());
        auto id=req["expected_document_id"].toString();id.remove('-');
        cacheRoot=QFileInfo(out).dir().filePath("performance-cache/doc-"+id+"-r"+QString::number(qint64(req.value("calculation_cache_revision").toDouble(req["expected_revision"].toDouble()))));cacheDirectory=cacheRoot;
        if(req["development_sample"]==true)cacheRoot=cacheDirectory=out+"/sample-cache";
        if(task<=3&&QFile::exists(cacheRoot+"/current")){
            auto token=QString::fromLatin1(read(cacheRoot+"/current",256)).trimmed();
            if(!QRegularExpression("^set-\\{[0-9a-fA-F-]{36}\\}$").match(token).hasMatch()||QUuid(token.mid(4)).isNull())bad("Invalid calculation cache generation");
            cacheDirectory=cacheRoot+"/"+token;if(!QDir(cacheDirectory).exists())bad("Missing calculation cache generation");
        }
        if(req.contains("values")&&!req["values"].isObject())bad("计算参数必须为对象。");
        auto patch=req["values"].toObject();
        if(task==5){
            const auto family=patch.value("ComboBox");
            if(!family.isUndefined()&&(!family.isDouble()||family.toDouble()!=std::trunc(family.toDouble())||family.toInt()<-1||family.toInt()>11))bad("螺旋桨系列选项无效。");
            if(!family.isUndefined())control("ComboBox",family.toDouble());
            choices(true,false);
        }
        // Parameter inspection and dependent choices do not launch a solver.
        // A stale configured path must not prevent choosing another family.
        for(auto it=patch.begin();it!=patch.end();++it){
            if(!fields.contains(it.key()))bad("未知计算参数："+it.key());
            auto f=fields[it.key()];double v=it.value().toDouble();
            if(!it.value().isDouble()||!std::isfinite(v))bad("参数不是有限数值："+it.key());
            if(f["kind"]=="choice"){if(v!=std::trunc(v)||v<((it.key()=="BSeriesReynoldsMode"||it.key()=="PassportSolver"||it.key()=="DesignSolver")?0:-1)||v>=f["choices"].toArray().size())bad("选项无效："+f["label"].toString());}
            else if(v<f["minimum"].toDouble()||v>f["maximum"].toDouble())bad("参数超出范围："+f["label"].toString());
        }
        if(task==5){if(patch.contains("ComboBox")){control("ComboBox",patch["ComboBox"].toDouble());choices(true,false);}if(patch.contains("ComboBox1")){control("ComboBox1",patch["ComboBox1"].toDouble());choices(false,true);}}
        for(auto it=patch.begin();it!=patch.end();++it)if(!it.key().startsWith("ComboBox"))control(it.key(),it.value().toDouble());
        if(task==5){
            fields["BSeriesReynoldsMode"]["visible"]=nativeB();fields["BSeriesReynoldsMode"]["enabled"]=nativeB();
            fields["BSeriesReynoldsMillions"]["visible"]=nativeB();
            fields["BSeriesReynoldsMillions"]["enabled"]=nativeB()&&d("BSeriesReynoldsMode")==1;
        }
        if(req.contains("extract")&&!req["extract"].isBool())bad("Invalid extraction flag");
        if(req["extract"].toBool())extract();
        if(task==3){const double graph=fields["Edit7"]["value"].toDouble();
            if(graph!=std::trunc(graph)||graph<1||graph>17)bad("图谱编号须为 1–17 的整数。");
            fields["PassportSolver"]["visible"]=graph<=4;}
        if(task<=2){
            double graph=fields["Edit"+QString::number(task==1?14:12)]["value"].toDouble();
            if(graph!=std::trunc(graph)||graph<1||graph>17)bad("图谱编号须为 1–17 的整数。");
            fields["DesignSolver"]["visible"]=graph<=4;
            for(const auto& key:{"DesignPitchLow","DesignPitchHigh","DesignDiameterLimit"})if(fields.contains(key)){
                fields[key]["visible"]=nativeDesign();fields[key]["enabled"]=nativeDesign();}
            if(task==1){enabled(17,!nativeDesign());enabled(18,!nativeDesign());}
            else enabled(15,!nativeDesign());
            if(nativeDesign()){
                fields[task==1?"Edit9":"Edit7"]["label"]="推力减额 t";
                fields[task==1?"Edit10":"Edit8"]["label"]="相对旋转效率 ηR（旧 i1）";
                fields[task==1?"Edit11":"Edit9"]["label"]="旧转矩影响系数 i2（仅接受 1）";
                if(task==2)fields["Edit2"]["label"]="主机总功率，kW";
            }
        }
        if(req["method"]!="performance_defaults"&&!nativeB()&&!nativePassport()&&!nativeDesign()&&!solverDirectory.isEmpty()&&!QDir(solverDirectory).exists())bad("计算程序目录不存在："+solverDirectory);
    }
    QJsonObject run();
};

void Calculation::hydrate(const Fbm &source){
    static auto catalog=QJsonDocument::fromJson(AnalysisFields).object();auto spec=catalog[schema["calculator"].toString()].toObject();auto bindings=spec["fields"].toObject();
    QByteArray record;if(source.version()>=spec["version"].toInt())record=source.blockBytes(spec["block"].toString());
    // Original property setters run in Dat order, after the control's own
    // bounds. In particular zero blades becomes the LFM minimum (3), not 4.
    for(int n=2;n<=18;++n){
        auto key="Edit"+QString::number(n);if(!fields.contains(key))continue;auto binding=bindings[key].toObject();int at=binding["offset"].toInt(-1);if(at<0)continue;
        double v=record.isEmpty()?0:real(record,at);
        if(task==5&&n==5&&v==2)fields[key]["minimum"]=2; // Stored B2 must not hydrate as B3.
        control(key,v);v=fields[key]["value"].toDouble();
        auto zero=[&](double replacement){if(v==0)v=replacement;};
        if(task==1){
            if(n==2)v=std::min(v,50.);
            if(n==4&&v>3)v=1;
            if(n==5&&(v==0||v>25))v=1;
            if(n==6&&(v==0||v>.5))v=.1;
            if(n==7)zero(1025);
            if((n==8||n==9)&&(v<=0||v>.5))v=.1;
            if((n==10||n==11)&&(v==0||v>1.5))v=1;
            if((n==12||n==13)&&(v==0||v>1))v=.98;
            if(n==14&&(v==0||v>20))v=2;
            if(n==15&&(v==0||v>8))v=4;
            if(n==16&&(v==0||v>1.8))v=.55;
        }else if(task==2){static const double defaults[]={0,0,0,250,0,1025,.1,.1,1,1,.98,.98,2,4,.55,1.03,5};if(n<=16)zero(defaults[n]);if(n==4&&v>3)v=1;
        }else if(task==3){static const double defaults[]={0,0,1,.58,1,.1,4,2,0,100,.1,.1,1,1,.98,.98,1025,10.25};if(n<=17)zero(defaults[n]);if(n==8&&v>3)v=1;if((n==14||n==15)&&v>1)v=.98;
        }else if(task==4){
            if(n==4&&(v==0||v>2))v=1;
            if(n==5&&(v<3||v>7))v=4;
            if(n==6&&(v<=.3||v>1.5))v=.55;
            if(n==7&&(v<=.4||v>1.6))v=.9;
            if(n==8&&(v<=0||v>25))v=1;
            if(n==9&&(v<=0||v>.45))v=.2;
            if(n==10&&(v<=0||v>25))v=2.5;
            if(n==11&&(v<=0||v>1.5))v=1;
            if(n==12&&(v==0||v>10))v=5;
            if(n==13&&(v<=0||v>30))v=5;
            if(n==14&&(v==0||v>1.1))v=.99;
            if(n==15&&(v==0||v>1))v=.97;
            if(n==16&&(v==0||v>1))v=.98;
        }else{if(n==2&&v<=0)v=100;if(n==6&&(v<=0||v>15))v=1;}
        control(key,v);
    }
    if(task==2)for(int n=17;n<=18;++n)for(int i=1;i<=5;++i){auto key=QString("Edit%1_%2").arg(n).arg(i);int at=bindings[key].toObject()["offset"].toInt();control(key,record.isEmpty()?0:real(record,at));if(n==17&&i==1&&d(16)<d(key))set(16,d(key)+2.f);}
}

static const char *seriesNames[12][4]={
    {"未知","未知","未知","未知"},{"T-4-35","T-4-58","T-4-75","未知"},{"M-4-65","M-4-75","M-4-85","未知"},
    {"Z-4-85","Z-4-100","未知","未知"},{"T-6-68","T-7-70","T-8-72","未知"},{"ZM-5-70","ZM-5-85","ZM-5-100","未知"},
    {"ZV-3-50","ZV-3-80","ZV-3-110","未知"},{"E-4-57","未知","未知","未知"},{"VIT-4-35","VIT-4-58","VIT-4-75","未知"},
    {"KA-3-65 in 19a","KA-4-55 in 19a","KA-4-70 in 19a","KA-5-75 in 19a"},{"KA-4-70 in 19a","KA-4-70 in 22","KA-4-70 in 24","KA-4-70 in 37"},{"SPP-5","SPP-5+INT","保留","保留"}};
void Calculation::choices(bool family,bool series){
    int f=int(d("ComboBox")),s=int(d("ComboBox1"));
    if(family){
        fields["ComboBox1"]["visible"]=f>0;
        fields["ComboBox1"]["enabled"]=f>0;
    }
    if(f<0)return;
    if(family){QJsonArray a;for(auto name:seriesNames[f])a.append(QString::fromUtf8(name));fields["ComboBox1"]["choices"]=a;enabled(3,f==0);enabled(5,f==0);}
    if(family){
        if(f==0)fields["ComboBox1"]["choices"]=QJsonArray{"由桨叶数和面积比确定","由桨叶数和面积比确定","由桨叶数和面积比确定","由桨叶数和面积比确定"};
        fields["Edit5"]["minimum"]=f==0?2:3;
        for(int i=2;i<=7;++i)fields["Edit"+QString::number(i)]["double_precision"]=f==0;
    }
    if(!series||f==0)return;
    static const float low[]={0,.7,.6,.9,.4,.8,.6,.5,.5,.5,.5,1.3},high[]={0,1.4,1.4,1.4,1.4,1.35,1.7,1.7,1.7,1.7,1.7,1.8};
    static const float area[12][4]={{},{.35,.58,.75,0},{.65,.75,.85,0},{.85,1,1,0},{.68,.70,.72,0},{.70,.85,1,0},{.50,.80,1.1,0},{.57,.57,.57,0},{.35,.58,.75,0},{.65,.55,.70,.75},{.70,.7022,.7024,.7037},{.70,.70,.70,.70}};
    int blades=f==5||f==11?5:f==6?3:4;if(f==4){if(s>=0&&s<3)blades=6+s;else blades=int(d(5));}if(f==9){if(s==0)blades=3;if(s==3)blades=5;}
    set(5,blades);set(4,std::clamp(d(4),low[f],high[f]));if(s>=0&&area[f][s]>0)set(3,area[f][s]);if(f==11&&s==1)set(4,0);
}

std::vector<std::vector<float>> Calculation::resistance(bool headerOnly){
    if(!QFile::exists(cacheDirectory+"/RESISTp.dat"))bad("当前阻力结果未提供推进参数，请手动填写推进计算输入。");
    auto ls=lines(cacheDirectory+"/RESISTp.dat");if(ls.size()<4||ls.size()>204||!ls[0].startsWith('#')||!ls[2].startsWith('#'))bad("Invalid resistance cache headers");
    auto header=numbers(ls[1]);if(header.size()<5||header.size()>6)bad("Invalid resistance cache header");if(header.size()==6)dpr=header[5];
    std::vector<std::vector<float>> rows{header};if(!headerOnly)for(int i=3;i<ls.size();++i)rows.push_back(numbers(ls[i],5));return rows;
}
void Calculation::extract(){
    if(task<=2&&QFile::exists(cacheDirectory+"/RESIST-native.json")){
        const auto metadata=QJsonDocument::fromJson(read(cacheDirectory+"/RESIST-native.json")).object();
        auto hash=[](const QByteArray& b){return QString::fromLatin1(QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex());};
        const auto payload=metadata["payload"].toObject();
        if(metadata["version"]!=1||metadata["resist_sha256"]!=hash(read(cacheDirectory+"/RESIST.dat"))||
           metadata["resistp_sha256"]!=hash(read(cacheDirectory+"/RESISTp.dat"))||
           metadata["payload_sha256"]!=hash(QJsonDocument(payload).toJson(QJsonDocument::Compact)))bad("原生阻力缓存不完整或校验失败。");
        const auto values=payload[task==1?"task1":"task2"].toObject();
        if(values.size()!=(task==1?8:17)||values["Edit4"].toDouble()<=0)bad("阻力结果缺少推进参数，请检查螺旋桨数量并重新计算阻力。");
        QStringList keys=task==1?QStringList{"Edit2","Edit3","Edit4","Edit5","Edit7","Edit8","Edit9","Edit10"}:
            QStringList{"Edit4","Edit5","Edit6","Edit7","Edit8","Edit16","DesignDiameterLimit"};
        if(task==2)for(int i=1;i<=5;++i)keys<<QString("Edit17_%1").arg(i)<<QString("Edit18_%1").arg(i);
        for(const auto& key:keys){const auto f=fields.value(key);const auto v=values[key];
            if(f.isEmpty()||!v.isDouble()||!std::isfinite(v.toDouble())||v.toDouble()<f["minimum"].toDouble()||v.toDouble()>f["maximum"].toDouble())bad("原生阻力推进输入越界或缺失。");
            control(key,v.toDouble());}
        return;
    }
    if(task==3&&QFile::exists(cacheDirectory+"/Propres-native.json")){
        auto metadata=QJsonDocument::fromJson(read(cacheDirectory+"/Propres-native.json")).object();
        const auto hash=QCryptographicHash::hash(read(cacheDirectory+"/Propres.dat"),QCryptographicHash::Sha256).toHex();
        if(metadata["version"]!=1||metadata["legacy_sha256"].toString().toLatin1()!=hash||!metadata["inputs"].isObject())bad("原生选桨结果缓存不完整或已改变。");
        const auto values=metadata["inputs"].toObject();
        if(metadata["inputs_sha256"].toString().toLatin1()!=QCryptographicHash::hash(QJsonDocument(values).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex())bad("原生选桨输入校验失败。");
        if(values.size()!=16)bad("原生选桨参数数量不完整。");
        for(int n=2;n<=17;++n){const auto key="Edit"+QString::number(n);const auto v=values[key];const auto f=fields[key];
            if(!v.isDouble()||!std::isfinite(v.toDouble())||v.toDouble()<f["minimum"].toDouble()||v.toDouble()>f["maximum"].toDouble())bad("原生选桨参数越界或缺失。");
            control(key,v.toDouble());
        }
        return;
    }
    if(task>=4)return;
    auto rows=resistance(task!=2);auto h=rows[0];if(h[1]==0)bad("阻力计算中的螺旋桨数量为零。");
    if(task==1){auto v=numbers(QString::fromLatin1(read(cacheDirectory+"/RESIST.dat")),25);set(4,h[1]);set(7,density*1000);set(2,v[15]);set(3,v[17]);set(5,dpr);set(8,h[2]);set(9,h[3]);set(10,h[4]);}
    if(task==2){
        set(4,h[1]);set(5,density*1000);set(6,h[2]);set(7,h[3]);set(8,h[4]);if(dpr>0&&dpr<=25)control("DesignDiameterLimit",dpr);int nser=int(h[0]);
        std::vector<int> selected;
        if(nser>=52&&nser<=54)selected={2,4,6,8,9};
        else {int stride=nser==51?4:nser<51?2:nser==61||nser==62?2:nser==64||nser==65?1:3;for(int i=1;i<=5;++i)selected.push_back(i*stride);}
        for(int i=0;i<5;++i){if(selected[i]>=int(rows.size()))bad("阻力结果不足以提取五个工况。");auto row=rows[selected[i]];auto key=QString("Edit17_%1").arg(i+1);set(key,row[0]);if(i==0&&d(16)<d(key))set(16,d(key)+2);set(QString("Edit18_%1").arg(i+1),row[2]);}
    }
    if(task==3){set(8,h[1]);set(16,density*1000);set(10,h[2]);set(11,h[3]);set(12,h[4]);auto ls=lines(cacheDirectory+"/Propres.dat");if(ls.size()!=2||!ls[0].startsWith('#'))bad("Invalid propeller cache header");auto p=numbers(ls[1],15);const int to[]={2,4,3,6,5,0,0,0,12,13,14,15,7,9,17};for(int i=0;i<15;++i)if(to[i])set(to[i],p[i]);}
}
void Calculation::resistance_curves(int km,int kt){
    if(!QFile::exists(cacheDirectory+"/RESISTp.dat")){
        const QString note="未叠加船体阻力曲线：请先进行阻力计算。";
        warnings.append(note);line(note);return;
    }
    auto rows=resistance();for(size_t i=1;i<rows.size();++i){auto row=rows[i];if(row[0]>0){point("Series11",row[0],float(row[1]*km*kt));point("Series12",row[0],float(row[2]*km*kt));}}
}
void Calculation::task3_engine_curves(int km,int kt){
    // Preserve the documented diesel power law P/Pnom=(n/nnom)^2. Intersect
    // only inside each computed propeller curve, then follow the nominal-RPM
    // branch where required power does not exceed the engine limit. Never join
    // extrapolated points to arbitrary middle/end rows of the nominal curve.
    struct Point {float speed,power,thrust;};
    std::vector<Point> matches,nominal;QJsonArray observations;int omitted=0;
    for(int g=0;g<4;++g){
        float power[6]{},speed[6]{},thrust[6]{};
        for(int i=1;i<=5;++i){power[i]=res[i][13+g*5]*km;speed[i]=res[i][11+g*5];thrust[i]=res[i][12+g*5]*km*kt;}
        float target=d(17)*sqr(res[1][10+g*5]/res[1][25]);
        auto [low,high]=std::minmax_element(power+1,power+6);
        bool inside=target>=*low&&target<=*high;Point hit{};
        if(inside){
            interpolate(5,power,speed,target,hit.speed);
            inside=hit.speed>=std::min(speed[1],speed[5])&&hit.speed<=std::max(speed[1],speed[5]);
            if(inside){interpolate(5,speed,thrust,hit.speed,hit.thrust);hit.power=target;inside=hit.thrust>=0;}
        }
        QJsonObject observation{{"rpm",res[1][10+g*5]},{"target_power",target},{"power_range",QJsonArray{*low,*high}},{"inside",inside}};
        if(inside){observation["speed"]=hit.speed;observation["plotted_thrust"]=hit.thrust;}
        observations.append(observation);
        if(g<3){if(inside)matches.push_back(hit);else ++omitted;}
        else {
            if(inside)nominal.push_back(hit);
            // For very generous power the entire nominal branch is feasible.
            // Otherwise start at its real intersection, with no extrapolation.
            if(inside||target>=*high)for(int i=1;i<=5;++i)
                if(power[i]<=target&&(nominal.empty()||speed[i]>nominal.back().speed))nominal.push_back({speed[i],power[i],thrust[i]});
            if(nominal.empty())++omitted;
        }
    }
    if(!nominal.empty())matches.erase(std::remove_if(matches.begin(),matches.end(),[&](const Point &p){return p.speed>=nominal.front().speed;}),matches.end());
    matches.insert(matches.end(),nominal.begin(),nominal.end());
    bool ordered=true;for(size_t i=1;i<matches.size();++i)if(matches[i].speed<=matches[i-1].speed)ordered=false;
    if(!ordered){matches.clear();omitted=4;}
    for(const auto &p:matches){point("series_11",p.speed,-p.power);point("series_12",p.speed,p.thrust);}
    if(omitted){
        QString note=matches.empty()?"给定主机功率在本次工况范围内没有可绘制的限制曲线；未进行范围外推算。":
            "部分主机功率交点超出本次工况范围；只绘制范围内的主机限制曲线。";
        if(!ordered)note="主机交点不能组成单调航速曲线；请检查工况，未强行连接这些点。";
        warnings.append(note);line(note);
    }
    engineCurve=QJsonObject{{"power_law","Pnom*(n/nnom)^2"},{"intersections",observations},{"extrapolated",false},{"plotted_points",int(matches.size())}};
}
void Calculation::publish_propeller(float dp,float pd,float area,float blades,float j,float kt,float kq,float eta){
    if(dp<=0||eta<=0)bad("主机匹配结果没有有效工作点。");
    QString row;for(float v:{dp,pd,area,blades,j,kt,float(kq*10),eta,d(8),d(9),d(10),d(11),d(12)})row+=fixed(v,6,3);
    row+=fixed(d(3),8,2)+fixed(d(2),10,2);pendingPropeller=("# D P/D Ae/Ao Z J Kt 10*Kq Eta0 EtaR i2 EtaS EtaG Ndiag n_nom Pe\r\n"+row+"\r\n").toLatin1();
}
void Calculation::publishCache(){
    if(pendingPropeller.isEmpty())return;
    QDir().mkpath(cacheRoot);QLockFile lock(cacheRoot+"/native.lock");if(!lock.tryLock(0))bad("计算结果缓存正在使用。");
    QString token="set-"+QUuid::createUuid().toString();QString path=cacheRoot+"/"+token;if(!QDir().mkdir(path))bad("无法保存推进结果。");
    // Only names declared by the protocol are copied into a new generation.
    for(const auto &name:{"RESIST.dat","RESISTp.dat","RESIST-native.json","RESIST-acceleration.json","RESIST-acceleration.dat"})if(QFile::exists(cacheDirectory+"/"+name))write(path+"/"+name,read(cacheDirectory+"/"+name));
    write(path+"/Propres.dat",pendingPropeller);if(!pendingPropellerMetadata.isEmpty())write(path+"/Propres-native.json",pendingPropellerMetadata);
    write(cacheRoot+"/current",(token+"\r\n").toLatin1());
}

void Calculation::capture(const QString &work){
    auto dest=output+"/solver-files";if(!QDir().mkpath(dest))bad("无法保存计算诊断文件。");qint64 total=0;
    for(auto f:QDir(work).entryInfoList(QDir::Files)){auto ext=f.suffix().toLower();if(!QStringList{"tsk","dat","res","tmp","txt","","."}.contains(ext)||f.isSymLink())continue;
        if(f.size()<=0||f.size()>2*1024*1024||total+f.size()>16*1024*1024)continue;
        total+=f.size();write(dest+"/"+f.fileName(),read(f.filePath()));}
}
void Calculation::runSolver(const QString &name,const QString &work,const QStringList &outputs,bool enter){
    check();externalTools.append(requireCalculationTool(solverDirectory,name));auto exe=QDir(solverDirectory).filePath(name);
    for(const auto &n:outputs)QFile::remove(work+"/"+n);
    if(QFile::exists(solverDirectory+"/CONF.PRO"))write(work+"/CONF.PRO",read(solverDirectory+"/CONF.PRO"));
    capture(work);
    QProcess process;process.setProgram(exe);process.setWorkingDirectory(work);
    // CALCPROP reads its task number from argv[1]; a missing selector exits
    // successfully without calculating anything. File existence is not dispatch.
    if(name.compare("CALCPROP.EXE",Qt::CaseInsensitive)==0){
        if(task<1||task>3)bad("CALCPROP 任务编号无效。");
        process.setArguments({QString::number(task)});
    }
    solverInvocations.append(QJsonObject{{"program",name},{"arguments",QJsonArray::fromStringList(process.arguments())}});
    write(output+"/solver-invocations.json",QJsonDocument(solverInvocations).toJson());
#ifdef Q_OS_WIN
    // Closing the backend (including a forced parent shutdown) also closes the
    // job handle. A solver cannot be left running after its document service.
    struct Job {HANDLE handle=CreateJobObjectW(nullptr,nullptr);~Job(){if(handle)CloseHandle(handle);}} job;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job.handle||!SetInformationJobObject(job.handle,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))bad("无法建立独立求解器进程组。");
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a){a->flags|=CREATE_NO_WINDOW;});
#endif
    fs::platform::prepareLegacySolver(process,report_language::text(fs::platform::legacySolverWineMissing,{}));
    QByteArray log;process.start();
    try{
        while(process.state()==QProcess::Starting){process.waitForStarted(20);check();}
        if(process.error()==QProcess::FailedToStart)bad(name+" 无法启动："+process.errorString());
#ifdef Q_OS_WIN
        HANDLE child=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(process.processId()));
        if(child){DWORD code=0;GetExitCodeProcess(child,&code);bool joined=code!=STILL_ACTIVE||AssignProcessToJobObject(job.handle,child);CloseHandle(child);if(!joined)bad("无法跟踪求解器进程。");}
        else if(!process.waitForFinished(0))bad("无法跟踪求解器进程。");
#endif
        if(enter)process.write("\r\n");
        process.closeWriteChannel();
        while(process.state()!=QProcess::NotRunning){process.waitForFinished(20);log+=process.readAllStandardOutput()+process.readAllStandardError();if(log.size()>2*1024*1024)bad("计算程序输出过大。");check();}
        fs::platform::adoptLegacyOutputs(work,outputs);
        log+=process.readAllStandardOutput()+process.readAllStandardError();write(output+"/"+QFileInfo(name).completeBaseName()+".log",log);
        capture(work);check();if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)bad(name+" 运行失败，退出码："+QString::number(process.exitCode()));
        for(const auto &n:outputs){if(!QFile::exists(work+"/"+n))bad(name+": Missing calculation output: "+n);read(work+"/"+n);}
    }catch(...){if(process.state()!=QProcess::NotRunning){process.kill();process.waitForFinished(3000);}capture(work);throw;}
}

void Calculation::task123(const QString &work){
    for(int n=2;n<=(task==1?9:16);++n)if(d(n)<=0)bad("计算参数必须大于零，请检查输入与适用范围。");
    int diag=int(d(task==1?14:task==2?12:7)),bladeField=task==1?15:task==2?13:6;float z=d(bladeField),area=d(task==1?16:task==2?14:3);
    if(diag<1||diag>17)bad("此螺旋桨图谱未提供有效实现。");
    static const char *names[]={"","FPP B-series","FPP B-4","FPP B-5","FPP B-6-80","FPP T-4-58","FPP T-4-75","FPP M-4-65","FPP Z-4-85","FPP Z-4-100","FPP T-4-35","FPP NT-4-35","FPP NT-4-58","FPP NT-4-75","CPP NR-4-57-06","CPP NR-4-57-08","FPP NF-4-57","CRP 4-57-54"};
    static const float areas[]={0,0,0,0,.8,.58,.75,.65,.85,1,.35,.35,.58,.75,.57,.57,.57,.575};
    if(diag>=2&&diag<=4)z=diag+2;
    if(diag>=4)area=areas[diag];
    // Task3 historically overwrote the entered area with shaft efficiency.
    // Use the actual Ae/Ao for B-series 1-3 instead (recorded fix).
    enabled(bladeField,diag<=1);enabled(task==1?16:task==2?14:3,diag<=3);set(bladeField,z);
    if(task==1){
        if(d(2)>=30)bad(r("rs_limit_of_speed_in_freeware_free_shipt_is_reached___"));
        if(d(5)/d(6)<=5)bad(r("rs_step_of_propeller_diameter_is_very_big_"));
        if((diag==1&&(z<2||z>6||area<.3||area>1.05))||(diag==2&&(area<.4||area>1))||(diag==3&&(area<.449||area>1.05)))bad("桨叶数或桨盘面比超出所选图谱范围。");
        if(d(9)>=1||d(10)<=0||d(11)<=0||d(12)<=0||d(13)<=0)bad("推进效率或推力减额无效。");
        if(d(17)==0||d(17)>10)set(17,d(5)/1.4);
        if(d(18)==0||d(18)>30)set(18,20);
    }
    if(task==2){
        if(d(16)>=50||d(2)>=100000)bad("超出此版本求解器的功率或航速范围。");
        for(int i=1;i<=5;++i){float speed=d(QString("Edit17_%1").arg(i));if(speed<=0||d(QString("Edit18_%1").arg(i))<=0||(i>1&&speed<=d(QString("Edit17_%1").arg(i-1))))bad("五个阻力工况须为正值，航速须递增。");}
        if(d(16)<d("Edit17_1")||d(16)>d("Edit17_5"))bad(r("rs_attention____the_waiting_speed_is_outside_valid_domain_of_calculated_speed___"));
        float lo=d("Edit17_1")*d("Edit18_1")*.51444/2.,hi=d("Edit17_5")*d("Edit18_5")*.51444*2.5;
        if(d(2)<lo||d(2)>hi)bad("主机功率超出给定阻力工况的匹配范围。");
        set(15,d(4)==1?1.05f:1.03f);
        if(QFile::exists(cacheDirectory+"/RESISTp.dat"))resistance(true);
    }
    QString name=names[diag];line("");line(space(16)+schema["title"].toString());line("");
    for(auto v:parameters()["fields"].toArray()){auto f=v.toObject();line(space(16)+pad(f["label"].toString(),45)+" : "+format(f["value"].toDouble(),0,6,f["decimals"].toInt()));}
    std::vector<float> input;for(int i=2;i<=(task==1?16:task==2?16:16);++i)input.push_back(d(i));
    if(task==1)input[14]=area;
    if(task==2){input[11]=z;input[12]=area;for(int n=17;n<=18;++n)for(int i=1;i<=5;++i)input.push_back(d(QString("Edit%1_%2").arg(n).arg(i)));}
    if(task==3)input[1]=area;
    const int cols=task==1?9:task==2?14:33;const QString tmp=QString("TMP%1.tsk").arg(task),result=QString("RES%1.tsk").arg(task);
    for(int iteration=0;iteration<(task==2?20:1);++iteration){
        QString data;for(float v:input)data+=QString::number(v,'E',8)+"\r\n";write(work+"/"+tmp,data.toLatin1());runSolver("CALCPROP.EXE",work,{result});
        auto values=numbers(QString::fromLatin1(read(work+"/"+result)));
        // Task3 also emits a 5x5 transposed speed/thrust/negative-power table.
        // Older protocol samples contain only the leading 5x33 matrix. Accept
        // these two complete forms, and validate every redundant tail value.
        if(values.size()!=size_t(5*cols)&&!(task==3&&values.size()==240))bad("Incomplete calculation output");
        if(task==3&&values.size()==240)for(int g=0;g<5;++g)for(int i=0;i<5;++i)for(int k=0;k<3;++k){
            float expected=values[i*33+10+g*5+k]*(k==2?-1:1);
            if(values[165+g*15+i*3+k]!=expected)bad("推进特性图的附加矩阵与主表不一致。");
        }
        for(int i=1;i<=5;++i)for(int j=1;j<=cols;++j)res[i][j]=values[(i-1)*cols+j-1];
        if(task==1){if(res[5][9]==0)bad("求解器未得到有效主机功率。");for(int i=1;i<=5;++i)if(i==1||res[i][4]>res[i-1][4]){point("Series3",res[i][4],res[i][1]);point("Series1",res[i][4],res[i][8]);point("Series2",res[i][4],res[i][9]);point("Series4",res[i][4],res[i][9]/.9);}legacy1(z,area,name);}
        if(task==2){for(int i=1;i<=5;++i){point("Series1",res[i][1],res[i][4]);point("Series2",res[i][1],res[i][14]);point("Series3",res[i][1],d(2));}
            if(res[5][14]>0){if(iteration==19)bad("主机匹配在 20 次迭代内未收敛。");input[14]=(res[5][1]+res[4][1])/2;continue;}
            bool found=false;for(int i=2;i<=5;++i)if(res[i][14]==0&&res[i-1][14]>0)found=true;if(!found)bad("主机匹配结果没有有效工作点。");legacy2(z,area,name);}
        if(task==3){if(res[1][32]==0||res[1][25]==0||d(17)<=0)bad("推进特性图的功率或工作点无效。");legacy3(z,area,name);}break;
    }
}

void Calculation::task4(const QString &work){
    for(int i=2;i<=16;++i)if(d(i)<=0)bad("初步设计参数必须大于零。");
    if(d(9)>=1||d(11)>d(12))bad("伴流系数或直径范围无效。");
    float rho=density*1000,viscosity=d("WaterViscosity")/1000000;
    QString input=" \r\n"+fixed(d(5),2,0)+fixed(1,2,0)+fixed(float(d(3)*1000),16,6)+fixed(d(2),14,6)+fixed(d(9),10,6)+fixed(d(10),10,6)+fixed(d(4),2,0)+"\r\n"+
        fixed(2,2,0)+fixed(d(13),2,0)+fixed(d(6),10,6)+fixed(d(7),10,6)+fixed(d(8),10,6)+"\r\n"+fixed(d(11),10,6)+fixed(d(12),10,6)+fixed(rho,12,6)+fixed(viscosity,14,11)+"\r\n";
    write(work+"/INO.",input.toLatin1());runSolver("PropPred.EXE",work,{"OUT.","POPGR."},true);
    auto ls=lines(work+"/OUT.");if(ls.size()<59||ls.size()>70)bad("Incomplete PropPred report");
    line("");for(int i=0;i<5;++i)line(" ");for(int i=0;i<5;++i)line(space(18)+ls[i]);
    line(space(18)+r("rs_wageningen_b_screw_series_propeller_preliminary_design"));line(" ");line(space(18)+r("rs_input_verification_"));line(" ");line(space(18)+r(d(4)==1?"rs_fixed_pitch_propeller":"rs_controllable_reversible_pitch_propeller"));
    int labelIndex=851;for(int i=26;i<41;++i){if(ls[i].trimmed().isEmpty())line(space(18)+ls[i]);else {if(!ls[i].contains('='))bad("Invalid PropPred input report");line(space(18)+userstring(labelIndex++)+ls[i].mid(ls[i].indexOf('=')+1));}}
    line(" ");line(space(18)+r("rs_optimal_design_results_"));line(" ");
    float vals[11]{};for(int i=46;i<59;++i){auto value=ls[i].mid(ls[i].indexOf('=')+1);line(space(18)+userstring(865+i-46)+value);if(i<56){auto v=numbers(value,1);vals[i-45]=v[0];}}
    float mp=rho*vals[8]*vals[5]*vals[5]/3600000*std::pow(vals[1],5),nz=mp*2*3.1415926*vals[5]/60,np=vals[10]*d(2)*.51444*(1-d(9));
    line("");line(space(18)+r("rs_additional_calculations_"));line("");
    for(auto item:std::initializer_list<std::pair<const char*,double>>{{"rs_propeller_torque__knom_________________",mp},{"rs_propeller_propulsive_power__kw_________",nz},{"rs_propeller_shaft_power__kw______________",np}})line(space(18)+r(item.first)+space(8)+format(item.second,0,6,3));
    double power=nz/d(14)/d(15)/d(16);
    const auto enginePower=r("rs_estimated_power_engine___kw___bhp______").section('[',0,0).trimmed();
    line(space(18)+enginePower+" = "+format(power,0,6,3)+" kW = "+format(power/.736,0,6,3)+" bhp");
    line("");line("");line(space(18)+"Copyright (c) 1997, M. G. Parsons (POP v1.5)");line(space(18)+"Copyright (c) 2008-2010, Timoshenko V.F.");
    auto graph=lines(work+"/POPGR.");if(graph.size()<4||graph.size()>52)bad("Incomplete PropPred graph");auto first=numbers(graph[0],2);numbers(graph[1],4);
    for(int i=2;i<graph.size();++i){auto row=numbers(graph[i],4);point("Series1",row[0],row[1]);point("Series2",row[0],float(row[2]*10));point("Series3",row[0],row[3]);}point("Series4",first[0],0);point("Series4",first[0],1);
}

void Calculation::task5B(){
    // Keep input and all intermediate arithmetic in double. FBM storage is
    // still Single; complete parameter files retain the entered precision.
    auto n=[&](int i){return fields["Edit"+QString::number(i)]["value"].toDouble();};
    const double rpm=n(2),area=n(3),pitch=n(4),blades=n(5),diameter=n(6),velocity=n(7);
    if(rpm<=0||diameter<=0||blades!=std::trunc(blades))bad("B 系列转速、直径必须大于零；桨叶数必须为整数。");
    const bool corrected=d("BSeriesReynoldsMode")==1;
    const double reynolds=corrected?fields["BSeriesReynoldsMillions"]["value"].toDouble()*1e6:2e6;
    const auto result=fs::bSeries(area,pitch,int(blades),corrected?std::optional<double>(reynolds):std::nullopt);
    const auto working=result.evaluate(velocity/(rpm/60*diameter));
    auto at=[&](const auto& c){return ((c[3]*working.j+c[2])*working.j+c[1])*working.j+c[0];};
    constexpr double pi=3.1415926535897932384626433832795;
    const double scale=double(density)*std::pow(rpm/60,2)*std::pow(diameter,4);
    const double thrust=working.kt*scale,torque=working.kq*scale*diameter,power=2*pi*(rpm/60)*torque;
    if(!std::isfinite(thrust)||!std::isfinite(torque)||!std::isfinite(power))bad("螺旋桨工作点计算溢出。");
    auto optional=[](std::optional<double> v)->QJsonValue{return v?QJsonValue(*v):QJsonValue(QJsonValue::Null);};
    auto coefficients=[](const auto& values){QJsonArray a;for(double v:values)a.append(v);return a;};
    nativeResult={{"method","Oosterveld-van Oossanen 1975"},{"reference_reynolds",2000000},
        {"reynolds_correction",corrected?"Oosterveld-van Oossanen 1975 Table 6":"none; reference-Reynolds open-water prediction"},
        {"reynolds",reynolds},{"reynolds_correction_applied",corrected},{"reynolds_basis","specified constant at 0.75R"},
        {"reference_kt_polynomial",coefficients(result.referenceThrust)},{"reference_kq_polynomial",coefficients(result.referenceTorque)},
        {"delta_kt_polynomial",coefficients(result.deltaThrust)},{"delta_kq_polynomial",coefficients(result.deltaTorque)},
        {"j_interval",QJsonArray{0,1.5}},{"kt_polynomial",coefficients(result.thrust)},
        {"kq_polynomial",coefficients(result.torque)},
        {"zero_thrust_j",optional(result.zeroThrust)},{"zero_torque_j",optional(result.zeroTorque)},
        {"maximum_at_interval_boundary",result.maximumAtBoundary},{"unbounded_efficiency",result.unboundedEfficiency},
        {"maximum_j",result.maximum?QJsonValue(result.maximum->j):QJsonValue(QJsonValue::Null)},
        {"maximum_efficiency",result.maximum?optional(result.maximum->efficiency):QJsonValue(QJsonValue::Null)},
        {"working",QJsonObject{{"j",working.j},{"kt",working.kt},{"kq",working.kq},{"efficiency",optional(working.efficiency)},
            {"reference_kt",at(result.referenceThrust)},{"reference_kq",at(result.referenceTorque)},
            {"delta_kt",at(result.deltaThrust)},{"delta_kq",at(result.deltaTorque)},
            {"thrust_kn",thrust},{"torque_kn_m",torque},{"delivered_power_kw",power},{"useful_power_kw",thrust*velocity},
            {"density_t_m3",double(density)},{"propulsive",working.kt>=0&&working.kq>0}}}};
    for(const auto& p:result.curve){point("Series1",p.j,p.kt);point("Series2",p.j,10*p.kq);if(p.efficiency)point("Series3",p.j,*p.efficiency);}
    point("Series4",working.j,0);point("Series4",working.j,1);
    static const double low[]={0,0,.30,.35,.40,.45,.50,.55},high[]={0,0,.38,.80,1.00,1.05,.95,.85};
    if(area<low[int(blades)]||area>high[int(blades)])warnings.append("该桨叶数与桨盘面比组合超出原系列相应桨叶数的试验覆盖范围；仍在总体多项式范围内，结果需审慎核对。");
    if(result.unboundedEfficiency)warnings.append("转矩在推力归零前降为零，效率多项式出现奇点；不提供最大效率或最佳工况。");
    if(result.maximum&&*result.maximum->efficiency>1)warnings.append("回归多项式预测效率大于 1，属于非物理结果；保留原数值供核查，不能作为设计最优值。");
    if(result.maximumAtBoundary)warnings.append("效率最大值落在 J=1.5 的计算边界；不能据此认定已找到完整曲线的最优工况。");
    if(!working.efficiency)warnings.append("输入工作点不在正推力、正转矩的推进区间；不报告敞水推进效率。");
    line("B 系列螺旋桨敞水计算");
    line("Re = "+QString::number(reynolds,'g',17));
    line(QString("桨叶数 %1；桨盘面比 %2；螺距比 %3").arg(blades,0,'g',17).arg(area,0,'g',17).arg(pitch,0,'g',17));
    line(QString("转速 %1 r/min；直径 %2 m；桨盘流速 %3 m/s").arg(rpm,0,'g',17).arg(diameter,0,'g',17).arg(velocity,0,'g',17));
    auto resultLine=[&](const QString& label,std::optional<double> v){line(label+(v?QString::number(*v,'g',17):QStringLiteral("—")));};
    resultLine("计算范围内最大效率 η0 = ",result.maximum?result.maximum->efficiency:std::optional<double>{});
    resultLine("对应进速系数 J = ",result.maximum?std::optional<double>(result.maximum->j):std::optional<double>{});
    line("首次零推力 J = "+(result.zeroThrust?QString::number(*result.zeroThrust,'g',17):QString("0–1.5 内未找到")));
    line("首次零转矩 J = "+(result.zeroTorque?QString::number(*result.zeroTorque,'g',17):QString("0–1.5 内未找到")));
    line("");line("输入工作点：");
    resultLine("J = ",working.j);resultLine("KT = ",working.kt);resultLine("KQ = ",working.kq);resultLine("η0 = ",working.efficiency);
    if(corrected){resultLine("基准 KT = ",at(result.referenceThrust));resultLine("基准 KQ = ",at(result.referenceTorque));
        resultLine("ΔKT = ",at(result.deltaThrust));resultLine("ΔKQ = ",at(result.deltaTorque));}
    resultLine("推力 T (kN) = ",thrust);resultLine("转矩 Q (kN·m) = ",torque);
    resultLine("有效推力功率 T·Va (kW) = ",thrust*velocity);resultLine("桨轴收到功率 2πnQ (kW) = ",power);
    
    for(auto warning:warnings)line("提示："+warning.toString());
}

void Calculation::task5(const QString &work){
    if(nativeB()){task5B();return;}
    int family=int(d("ComboBox")),series=int(d("ComboBox1"));if(family<0||series<0||(family>0&&(QString::fromUtf8(seriesNames[family][series])=="未知"||QString::fromUtf8(seriesNames[family][series])=="保留")))bad("请选择已提供的螺旋桨系列。");
    for(int i=2;i<=6;++i)if(d(i)<=0)bad("敞水计算参数必须大于零。");
    if(d(3)<=.3||d(3)>1.05)set(3,.58);
    if(d(4)<.5||(d(4)>1.5&&family<11))set(4,1);
    if(d(5)<3||d(5)>7)set(5,4);
    if(d(7)==0)set(7,1);
    QString in="PROPELLER B Series\r\n"+fixed(d(3),8,4)+fixed(d(4),8,4)+fixed(d(5),4,0)+fixed(d(6),10,4)+"\r\n1 "+fixed(d(2),10,4)+"\r\n"+fixed(d(7),10,4)+"\r\n";
    static const char *codes[12][4]={{},{"T-4-35","T-4-58","T-4-75"},{"M-4-65","M-4-75","M-4-85"},{"Z-4-85","Z-4-100"},{"T-6-68","T-7-70","T-8-72"},{"ZM-5-70","ZM-5-85","ZM-5-100"},{"ZV-3-50","ZV-3-80","ZV-3-110"},{"E-4-57"},{"VIT-4-35","VIT-4-58","VIT-4-75"},{"KA-3-65","KA-4-55","KA-4-70","KA-5-75"},{"KA-4-70","KA-4-7022","KA-4-7024","KA-4-7037"},{"SPP-5","SPP-5+INT"}};
    if(family>0){if(series>0)in+=QString::fromLatin1(codes[family][series])+"\r\n";in+=QString::fromLatin1(codes[family][0])+"\r\n";}
    write(work+"/IN."," \r\nINO.\r\nOUT.\r\n");write(work+"/INO.",in.toLatin1());runSolver("FPPCALC.EXE",work,{"OUT."});
    auto ls=lines(work+"/OUT.");if(ls.size()<27||ls.size()>100)bad("Incomplete propeller report");
    line("");line(space(19)+r("rs_calculation_of_open_water_characteristics_of_a_propeller"));line(" ");line(space(18)+"-----------------------------------------------------------------------");line(" ");
    const char *kind[]={"rs_series_of_troost","rs_series_of_i_titov","rs_series_of_v_mishkevich","rs_series_of_m_zavadovsky","rs_series_of_v_turbal","rs_series_of_ksri_bihs","rs_series_of_zvezdkina"};const char *letter[]={"B","T","M","Z","T","ZM","ZV"};
    if(family<=6)line(space(34)+r("rs_propeller")+" "+letter[family]+" "+r(kind[family]));
    line(" ");
    const int field[]={3,4,5,6,2,7},dec[]={3,3,0,3,2,3};const char *label[]={"rs_aae___","rs_ppd___","rs_npb___","rs_dpr___","rs_rpm___","rs_vp____"};const QString unit[]={"","",""," m"," 1/min"," m/s"};
    for(int i=0;i<6;++i)line(space(14)+r(label[i])+" "+format(d(field[i]),0,6,dec[i])+unit[i]);
    line(" ");
    line(space(family==0?17:20)+(family==0?r("rs_j________kt_____dkt____kttdkt_______kq______dkq____kqtdkq_____eta_0"):QString("Jp       KT      dKT     KT+dKT    KQ      dKQ     KQ+dKQ     Eta0")));for(int i=25;i<ls.size();++i)line(space(14)+ls[i]);
    float optimal=0,maximum=0,zeroThrust=0,zeroTorque=0,k1=0,k2=0;
    {
        bool summary=false;int count=0;float previous=0;
        for(int i=25;i<ls.size();++i){if(ls[i].trimmed().isEmpty())continue;auto row=numbers(ls[i]);if(summary)bad("FPPcalc contains data after summary");
            if(row.size()==7){if(count<2)bad("FPPcalc needs two curve rows");summary=true;optimal=row[0];maximum=row[1];zeroThrust=row[2];zeroTorque=row[3];k1=row[5];k2=row[6];}
            else if(row.size()==8&&count<68){if(row[3]>=0&&(count==0||row[0]>previous)){point("Series1",row[0],row[3]);point("Series2",row[0],float(row[6]*10));point("Series3",row[0],row[7]);}previous=row[0];++count;}
            else bad("Invalid FPPcalc numeric row");
        }if(!summary)bad("FPPcalc summary missing");
    }
    line(" ");line(space(14)+r("rs_optimal_advance_coeffic___jp_opt__")+" "+format(optimal,0,6,4));line(space(14)+r("rs_maximum_efficiency______eta0_max__")+" "+format(maximum,0,6,4));line(space(14)+r("rs_zero_thrust_advance_coeff__jp_t0__")+" "+format(zeroThrust,0,6,4));line(space(14)+r("rs_zero_torque_advance_coeff__jp_q0__")+(zeroTorque<2.2?" "+format(zeroTorque,0,6,4):" >2.2"));
    float j=d(7)/d(6)/d(2)*60;
    if(j<zeroThrust){
        if(k2==0)bad("螺旋桨转矩系数为零。");
        float eta=k1/k2/6.2831852*j;
        k1=k1*density*d(2)*d(2)*d(6)*d(6)*d(6)*d(6)/3600;k2=k2*density*d(2)*d(2)*d(6)*d(6)*d(6)*d(6)*d(6)/3600;
        point("Series4",j,0);point("Series4",j,1);
        line(space(14)+r("rs_estimated_advance_____________jp__")+" "+format(j,0,6,4));line(space(14)+r("rs_propeller_thrust__kn___________t__")+" "+format(k1,0,6,3));line(space(14)+r("rs_propeller_torque__knom_________q__")+" "+format(k2,0,6,3));line(space(14)+r("rs_efficiency___________________eta__")+" "+format(eta,0,6,4));line(space(14)+r("rs_brake_power__kw_______________pb__")+" "+format(float(k1*d(7)),0,6,3));
        const auto shaftPower=k2*6.283*d(2)/60;
        line(space(14)+r("rs_propeller_power__kw___________pd__")+" "+format(shaftPower,0,6,3)+" kW = "+format(shaftPower/.736,0,6,3)+" bhp");
    }else line(space(14)+r("rs_estimated_advance_____________jp__")+" "+format(j,0,6,4)+" > Jp_T0");
    line(" ");if(family==0)line(space(14)+"Copyright (c) 2000, J.M.J. Journee (PROPOL 03-04-2000)");line(space(14)+"Copyright (c) 2008-2013, Timoshenko V.F.");
}

QJsonObject Calculation::run(){
    check();if(request["method"]=="performance_defaults"){auto s=parameters();s["solver_directory"]=QDir(solverDirectory).exists()?solverDirectory:"";return s;}
    if(nativeDesign()){designB();check();}else if(nativePassport()){task3B();check();}else if(nativeB()){task5B();check();}else{
    QTemporaryDir temp(QDir::tempPath()+"/fs-prop-XXXXXX");if(!temp.isValid())bad("无法创建独立计算目录。");QString work=temp.path();
#ifdef Q_OS_WIN
    if(work.toLatin1().contains('?')){std::wstring p=work.toStdWString();wchar_t shortName[32768];auto n=GetShortPathNameW(p.c_str(),shortName,32768);if(n>0&&n<32768)work=QString::fromWCharArray(shortName);if(work.toLatin1().contains('?'))bad("外部求解器需要可用的 ASCII 临时目录。");}
#endif
    if(task<=3)task123(work);else if(task==4)task4(work);else task5(work);check();}
    QJsonArray curves;const auto descriptions=tables()["series"].toObject()[QString::number(task)].toArray();
    QString x=task==1?r("rs_frequency__1_min_"):task<=3?r("rs_speed_ship__knots_"):r("rs_advance_coefficient_jp");
    QString y=task<=3?r("rs_engine_power__kw_"):r("rs_thrust_coefficient_and_torque__kt__10okq");if(task==3)y=axisY;
    for(auto value:descriptions){auto s=value.toObject();auto name=s["source"].toString();if(points[name.toLower()].isEmpty())continue;
        s["points"]=points[name.toLower()];s["x_title"]=x;s["source_x_title"]=x;s["source_y_title"]=y;s["y_title"]=y;
        if(task==1&&name=="Series3")s["y_title"]="螺旋桨直径 (m)"; // RES1 column 1, not engine power.
        if(task>=4)s["y_title"]=name=="Series1"?"推力系数 KT":name=="Series2"?"10 × 转矩系数 KQ":name=="Series3"?"敞水效率 η₀":"工作点参考线（无量纲）";
        if(nativeDesign()){
            if(task==1)s["name"]=name=="Series1"?"轴系输入功率":name=="Series2"?"主机所需总功率":name=="Series3"?"候选直径":"按 90% 负荷反算额定功率";
            else s["name"]=name=="Series1"?"逐航速重新选桨的所需总功率":name=="Series2"?"有效功率 RV":"主机可用总功率";
            s["y_title"]=(task==1&&name=="Series3")?"螺旋桨直径 (m)":"功率 (kW)";
            s["source_y_title"]=s["y_title"];
        }
        if(nativePassport()){
            const auto key=name.toLower();
            bool power=key=="series_11";
            if(key.startsWith("series_")&&key!="series_11"&&key!="series_12")power=key.mid(7).toInt()%2==0;
            if(!key.contains('_')&&key!="series11"&&key!="series12")power=key.mid(6).toInt()%2==0;
            s["y_title"]=power?"负主机功率 (kW)":"有效推力 / 船体阻力 (kN)";
            s["source_y_title"]=s["y_title"];
            const auto rows=nativeResult["points"].toArray();
            if(key=="series_11"||key=="series_12")s["name"]=power?"主机功率限制":"主机限制下有效推力";
            else if(key!="series11"&&key!="series12"){
                const bool advance=key.contains('_');int index=((advance?key.mid(7):key.mid(6)).toInt()-1)/2;
                auto p=advance?rows[0].toArray()[index].toObject():rows[index].toArray()[0].toObject();
                const QString prefix=power?"负主机功率 · ":"有效推力 · ";
                const auto suffix=QString(advance?"J=%1":"n=%1 rpm").arg(p[advance?"j":"rpm"].toDouble(),0,'g',8);
                s["name"]=prefix+suffix;
                s["name_parts"]=QJsonArray{QJsonObject{{"source",prefix}},QJsonObject{{"value",suffix}}};
            }
        }
        curves.append(s);
    }
    if(curves.isEmpty()||report.isEmpty())bad("未得到有效计算曲线。请检查参数及方法适用范围。");
    const auto helpNote=r(task==1?"rs_propeller_task1_note":task<=3?"rs_propeller_task2_note":task==4?"rs_propeller_task4_note":"rs_propeller_task5_note");
    const auto helpCredit="\r\n\r\nCopyright (c) "+QString(task<=3?"2007":task==4?"2008":"2008-2013")+", Timoshenko V.F.\r\n";
    QString help=helpNote+helpCredit;
    if(nativeB())help="B 系列采用 1975 年完整多项式，基准雷诺数 2×10^6。适用范围：2–7 片桨叶，桨盘面比 0.30–1.05，螺距比 0.50–1.40，J=0–1.5。\r\n可选择基准模式或论文 Table 6 雷诺数修正（2×10^6–2×10^9）。输入为 0.75R 处的雷诺数，整条曲线保持该值；例如输入 20 表示 2×10^7。基准模式不使用修正式；指定模式在下限处保留论文拟合残差。\r\n未施加粗糙度、空泡、桨厚变化或船后修正。";
    publishCache();QJsonObject answer{{"calculator",schema["calculator"]},{"parameters",parameters()},{"series",curves},{"text",report.join("\r\n")+"\r\n"},{"help",help},{"warnings",warnings},{"units",0},{"requested",request},{"elapsed_ms",double(timer.elapsed())},{"engine",nativeB()?"C++ Wageningen B-series":"C++ propulsion workflow; original external solver"}};
    if(nativeB())answer["b_series"]=nativeResult;
    if(nativeDesign()){
        answer["engine"]="C++ B-series preliminary design";answer["propeller_design"]=nativeResult;
        answer["help"]="在给定直径、转速和螺距范围内进行 B 系列初选。各航速独立选桨，曲线表示设计包络。载荷和主机功率为全部螺旋桨的合计值。未指定直径限制时最多搜索到 25 m，请核对安装空间。匹配只在表列航速范围内进行。";
    }
    if(nativePassport()){
        answer["engine"]="C++ B-series propulsion passport";
        answer["propulsion_passport"]=nativeResult;
        answer["help"]="图谱 1–4 使用 B 系列多项式，基准 Re=2×10^6，要求 i2 = 1。推力、转矩和主机功率均为全部螺旋桨的合计值。伴流比 w、推力减额 t 和相对旋转效率 ηR 采用输入值，ηR 为等推力下敞水转矩与船后转矩之比。Va=(1-w)Vs，Te=(1-t)Nρn²D⁴KT，Pe=2πNρn³D⁵KQ/(ηRηsηg)。参考 J 的 0.70–1.30 倍和额定转速的 0.55–1.15 倍组成 25 个工况。超出推进区的载荷仅作参考。";
    }
    if(task==3)answer["engine_curve"]=engineCurve;
    answer["external_tools"]=externalTools;
    if(!externalTools.isEmpty()&&task<=3){
        answer["numeric_conventions"]=QJsonObject{{"source","legacy-calculator"},{"knots_to_ms_power",.5144},{"ms_to_knots_task3",1.945},{"exact_knots_to_ms",1852./3600.},{"output_corrected",false}};
    }
    answer["help_parts"]=nativeB()||nativeDesign()||nativePassport()?QJsonArray{answer["help"]}:QJsonArray{helpNote,helpCredit};
    return answer;
}
#include "propulsion_passport.inc"
#include "propeller_design.inc"
#include "propulsion_reports.inc"
}
bool nativePropulsion(const QString &s){return s=="propeller1"||s=="propeller2"||s=="propeller3"||s=="propeller4"||s=="propeller5";}
QJsonObject propulsion(const Fbm &source,const QJsonObject &request,const QString &output){
    report_language::Scope locale(request);
    const auto method=request["calculator"].toString();
    if(method=="propeller4"||method=="propeller5"||!shouldFillCalculationInputs(request))
        return Calculation(source,request,output).run();
    static const auto fields=QJsonDocument::fromJson(AnalysisFields).object();
    const auto spec=fields[method].toObject();
    const auto stored=!spec.isEmpty()&&source.version()>=spec["version"].toInt()
        ?source.blockBytes(spec["block"].toString()):QByteArray{};
    return fillCalculationInputs(request,stored,[&](const QJsonObject& input){return Calculation(source,input,output).run();});
}
}
