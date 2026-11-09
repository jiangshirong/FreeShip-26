// SPDX-License-Identifier: GPL-2.0-or-later
// Protocol/formula provenance: Plus FreeEdit.ExportAddedMassInWorkspace and
// FreeAddMassOutputDlg.SaveToFile (Timoshenko V.F.). See ADDED-MASS.md.
#include "added_mass.h"
#include "geometry_input.h"
#include "hydrostatics.h"
#include "core/grid_export.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <cmath>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace fs::document {
namespace {
[[noreturn]] void bad(const QString& s){throw std::runtime_error(s.toStdString());}
void write(const QString& path,const QByteArray& bytes){QSaveFile f(path);f.setDirectWriteFallback(false);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())bad("无法保存计算文件："+path);}
QByteArray read(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()<1||f.size()>2*1024*1024)bad("计算结果缺失、为空或过大。");return f.readAll();}
QString num(double x){return QString::number(x,'g',17);}
double number(QString s){s=s.trimmed();s.replace('D','E');s.replace('d','e');bool ok=false;double x=s.toDouble(&ok);if(!ok||!std::isfinite(x))bad("AddMass 结果含非有限数值或无效数字："+s.left(30));return x;}
QJsonArray numbers(const QString& s,int count,bool allowUndefined=false){QJsonArray a;for(auto token:s.trimmed().split(QRegularExpression("\\s+"),Qt::SkipEmptyParts)){
    if(allowUndefined&&(token.compare("nan",Qt::CaseInsensitive)==0||token=="***************"))a.append(QJsonValue());else a.append(number(token));
}if(a.size()!=count)bad("AddMass 结果的数字数量不完整。");return a;}
QJsonObject table(QString name,QJsonArray columns,QJsonArray rows,QJsonArray values){return {{"name",name},{"columns",columns},{"rows",rows},{"values",values}};}
QJsonObject schema(const Fbm& source,const QJsonObject& request){
    auto settings=documentGeometrySettings(source);const double metres=settings.imperial?.3048:1.;QJsonArray fields;
    auto field=[&](QString key,QString label,QString unit,double value,double lo,double hi,int decimals){fields.append(QJsonObject{{"key",key},{"label",label},{"unit",unit},{"value",value},{"minimum",lo},{"maximum",hi},{"decimals",decimals},{"kind","number"},{"group","计算工况"}});};
    field("draft","吃水","m",settings.draft*metres,.000001,10000,6);
    field("stations","横剖站数（奇数）","",21,5,45,0);field("waterlines","水线数（奇数）","",11,5,25,0);
    field("panels","目标面元数量","",1000,400,4000,0);field("area_percent","最大面元面积比例","%",100,10,100,6);
    field("accuracy","迭代精度","",.2,.000001,1,6);field("depth","水深","m",10000,.000001,10000,6);
    field("drift","漂角","°",0,-90,90,6);field("velocity_knots","航速","kn",10,0,30,6);field("omega","转动角速度","1/s",0,-30,30,6);
    auto choice=[&](QString key,QString label,QJsonArray options){fields.append(QJsonObject{{"key",key},{"label",label},{"kind","choice"},{"group","计算工况"},{"choices",options},{"value",0},{"minimum",0},{"maximum",options.size()-1},{"decimals",0}});};
    choice("panel_type","面元类型",{"三角形","梯形"});choice("object_type","对象类型",{"一般物体或船舶","水下航行器","排水型海船","排水型河船"});
    if(request.contains("values")&&!request["values"].isObject())bad("计算参数必须为对象。");
    const auto patch=request["values"].toObject();QSet<QString> known;
    for(qsizetype i=0;i<fields.size();++i){auto f=fields[i].toObject();auto key=f["key"].toString();known.insert(key);
        if(patch.contains(key)){if(!patch[key].isDouble())bad("计算参数必须为数字："+key);f["value"]=patch[key];}
        if(request["extract"].toBool()&&key=="draft")f["value"]=settings.draft*metres;
        double v=f["value"].toDouble();if(!std::isfinite(v)||v<f["minimum"].toDouble()||v>f["maximum"].toDouble()||(f["decimals"]==0&&std::trunc(v)!=v))bad("参数超出范围："+f["label"].toString());fields[i]=f;}
    for(auto it=patch.begin();it!=patch.end();++it)if(!known.contains(it.key()))bad("未知计算参数："+it.key());
    QJsonObject values;for(auto f:fields){auto v=f.toObject();values[v["key"].toString()]=v["value"];}
    if(values["stations"].toInt()%2!=1||values["waterlines"].toInt()%2!=1)bad("横剖站数和水线数必须为奇数。");
    if(values["depth"].toDouble()<values["draft"].toDouble())bad("水深不能小于吃水。");
    return {{"calculator","addmass"},{"title","附加质量计算"},{"fields",fields},{"persistable",false},
        {"help","单体、对称船型；输入由当前控制网生成，始终使用米、千克。矩阵保留原求解器输出精度和缩放除数，不强制对称化。等效椭球与海船/河船经验公式单独列出，不替代数值矩阵。经验公式沿用上游定义，尚未取得独立工程验证；河船超出已知适用范围时不输出经验值。"}};
}
}

QJsonObject parseAddedMass(const QByteArray& bytes,const QJsonObject& values){
    if(bytes.isEmpty()||bytes.size()>2*1024*1024||bytes.contains('\0'))bad("AddMass 结果文件无效。");
    auto lines=QString::fromLatin1(bytes).replace("\r\n","\n").split('\n');while(!lines.isEmpty()&&lines.last().trimmed().isEmpty())lines.removeLast();
    if(lines.size()!=20)bad("AddMass 结果应有 20 行，当前文件残缺或版本不兼容。");
    const QString border="+---+-----------+-----------+-----------+-----------+-----------+-----------+";
    if(lines[1].trimmed()!=border||lines[3].trimmed()!=border||lines[10].trimmed()!=border)bad("AddMass 矩阵边界无效。");
    auto cells=[](QString line){auto a=line.split('|');if(a.size()!=9||!a.first().trimmed().isEmpty()||!a.last().trimmed().isEmpty())bad("AddMass 矩阵列数无效。");a.removeLast();a.removeFirst();return a;};
    auto header=cells(lines[2]);if(header[0].trimmed()!="N")bad("AddMass 矩阵表头无效。");for(int j=0;j<6;++j)if(number(header[j+1])!=j+1)bad("AddMass 矩阵列序号无效。");
    QJsonArray matrix;for(int i=0;i<6;++i){auto row=cells(lines[4+i]);if(number(row[0])!=i+1)bad("AddMass 矩阵行序号无效。");QJsonArray a;for(int j=1;j<=6;++j)a.append(number(row[j]));matrix.append(a);}
    auto meta=numbers(lines[11],2);if(meta[0].toDouble()<=0||std::trunc(meta[0].toDouble())!=meta[0].toDouble()||meta[0].toDouble()>100000||meta[1].toDouble()<=0)bad("AddMass 面元数量或缩放除数无效。");
    auto conditions=numbers(lines[19],5);const QStringList keys{"depth","drift","velocity_knots","omega","object_type"};
    for(int i=0;i<5;++i){double want=values[keys[i]].toDouble()+(i==4?1:0),got=conditions[i].toDouble();if(!values[keys[i]].isDouble()||std::abs(got-want)>std::max(1e-6,std::abs(want)*1e-6))bad("AddMass 返回工况与当前请求不一致。");}
    QJsonArray ellipsoid;QStringList labels{"L, B, T (m)","质量 (t)","Ixx, Iyy, Izz","al11, al22, al33","al44, al55, al66","k11, k22, k33","k44, k55, k66"};bool undefined=false;
    for(int i=12;i<19;++i){auto row=numbers(lines[i],i==13?1:3,true);for(auto v:row)undefined|=v.isNull();ellipsoid.append(QJsonObject{{"label",labels[i-12]},{"values",row}});}
    QJsonArray warnings;if(undefined)warnings.append("等效椭球的个别值未定义，已留空；未用零代替。");
    QJsonArray indices{1,2,3,4,5,6},ellipseRows,ellipseValues;for(auto e:ellipsoid){auto o=e.toObject();ellipseRows.append(o["label"]);ellipseValues.append(o["values"]);}
    return {{"matrix",matrix},{"panel_count",meta[0]},{"scale_divisor",meta[1]},{"ellipsoid",ellipsoid},{"warnings",warnings},
        {"tables",QJsonArray{table("附加质量矩阵（原始输出；缩放除数 "+num(meta[1].toDouble())+"）",indices,indices,matrix),table("等效椭球",{"分量 1","分量 2","分量 3"},ellipseRows,ellipseValues)}}};
}

QJsonObject addedMassEmpirical(int type,double L,double B,double T,double cb,const QJsonObject& v){
    QJsonObject result{{"status","not_applicable"}};if(type!=2&&type!=3)return result;
    const double H=v["depth"].toDouble(),beta=v["drift"].toDouble(),speed=v["velocity_knots"].toDouble(),omega=v["omega"].toDouble();
    for(double x:{L,B,T,cb,H,beta,speed,omega})if(!std::isfinite(x))bad("经验公式参数不是有限值。");
    if(L<=0||B<=0||T<=0||H<=0)bad("经验公式主尺度无效。");
    QJsonArray reasons;
    if(type==3){if(L/B<4||L/B>8.5)reasons.append("L/B 超出 4–8.5。");if(B/T<3.7||B/T>14)reasons.append("B/T 超出 3.7–14。");if(cb<.5||cb>.93)reasons.append("Cb 超出 0.5–0.93。");if(T/H>.7)reasons.append("T/H 超出 0–0.7。");}
    result["geometry"]=QJsonObject{{"length_m",L},{"beam_m",B},{"draft_m",T},{"cb",cb}};
    if(!reasons.isEmpty()){result["status"]="out_of_domain";result["reasons"]=reasons;return result;}
    double k11,k22,k66;const double bl=B/L,bt=B/T;
    if(type==2){k11=(.465*bl-.028)*2/bt;k22=(1-.5*bl)*2/bt;k66=(1.03-1.76*bl)*2/bt;}
    else{k11=(5.91*bl*bl+7.76*bl-.269)/(48.4-6.89*bt+1.47*bt*bt-.0475*bt*bt*bt);k22=(.722+.224*cb)*(1.022-bl*bl)/(.264+.386*bt);k66=2/bt*(2.59+.781*cb)*(.357-1.71*bl*bl);}
    QJsonArray rows,data;auto add=[&](QString label,double a,double b,double c){for(double x:{a,b,c})if(!std::isfinite(x))bad("经验公式产生无效结果。");rows.append(label);data.append(QJsonArray{a,b,c});};
    add(type==2?"海船经验值":"河船经验值（不计波浪及有限水深）",k11,k22,k66);
    // The upstream river branch used uninitialized sea coefficients here.
    // Only retain the defined sea-ship correction; do not invent river values.
    if(cb>=.8&&type==2)add("海船 Cb≥0.8 修正",k11*1.4,k22*1.3,k66*1.3);
    if(cb>=.8&&type==3)result["warning"]="上游 Cb≥0.8 河船附加修正使用了未初始化变量，暂不采用；下表保留明确赋值的河船公式。";
    if(type==3){const double h=T/H,frL=speed*.51444/std::sqrt(9.806*L)*std::cos(beta/57.29),frB=speed*.51444/std::sqrt(9.806*B)*std::sin(beta/57.29),frW=omega*L/std::sqrt(9.806*B);
        const double d=.918*cb*frL*(.176+.815*std::pow(h,1.416))*(1-.071*(L/B-6.8)*h*(1.09-.015*bt));
        add("河船：正加速度",k11+d,k22*(1+(.72+6.3*h*h)*std::pow(bt,.73)*frB),k66*(1+.31*(1+.312*bt)*(1+7.29*h*h)*frW));
        add("河船：负加速度",k11+1.176*d/.918,k22*(1+(.9+7.38*h*h)*std::pow(bt,.73)*frB),k66*(1+.388*(1+.312*bt)*(1+6.75*h*h)*frW));}
    result["status"]="ok";result["table"]=table("经验系数（独立于数值矩阵）",{"k11","k22","k66"},rows,data);return result;
}

QJsonObject addedMass(const Fbm& source,const QJsonObject& request,const QString& output,const QJsonObject& bindings,const ModelSnapshot* model){
    QElapsedTimer timer;timer.start();auto check=[&]{if(QFile::exists(output+"/cancel.flag"))bad("计算已取消。");if(timer.elapsed()>110000)bad("计算超过 110 秒，已停止。");};check();
    auto params=schema(source,request);if(request["method"]=="performance_defaults")return params;
    QJsonObject values;for(auto field:params["fields"].toArray()){auto f=field.toObject();values[f["key"].toString()]=f["value"];}
    auto settings=documentGeometrySettings(source);auto input=native_geometry::DocumentInput{};input.geometry=documentGeometry(source);input.imperial=settings.imperial;input.draft=settings.draft;
    input.documentId=request["expected_document_id"].toString();input.revision=quint64(request["expected_revision"].toDouble());
    if(!model)bad("当前工程几何尚未就绪，请重新打开船模。");
    auto options=values;options["panel_type"]=values["panel_type"].toInt()+1;options["object_type"]=values["object_type"].toInt()+1;
    auto grid=grid::build(input,*model,"addmass","m",options);check();
    QDir().mkpath(output+"/solver-files");write(output+"/solver-files/INA",grid.data);
    QTemporaryDir temp(QDir::tempPath()+"/fs-addmass-XXXXXX");if(!temp.isValid())bad("无法建立计算临时目录。");QString work=temp.path();
#ifdef Q_OS_WIN
    if(work.toUtf8()!=work.toLatin1()){auto w=QDir::toNativeSeparators(work).toStdWString();DWORD n=GetShortPathNameW(w.c_str(),nullptr,0);std::wstring b(n,L'\0');if(n)n=GetShortPathNameW(w.c_str(),b.data(),DWORD(b.size()));if(!n)bad("旧求解器需要 ASCII 临时目录。");b.resize(n);work=QString::fromStdWString(b);if(work.toUtf8()!=work.toLatin1())bad("旧求解器需要 ASCII 临时目录。");}
#endif
    write(work+"/INA",grid.data);QString solver=request["solver_directory"].toString();if(solver.isEmpty()){QFile config(QCoreApplication::applicationDirPath()+"/calculation-tools.json");if(config.open(QIODevice::ReadOnly)&&config.size()<65536)solver=QJsonDocument::fromJson(config.readAll()).object()["directory"].toString();}
    const auto exe=QDir(solver).absoluteFilePath("ADD_MASS.EXE");if(solver.isEmpty()||!QFileInfo(exe).isFile())bad("找不到 ADD_MASS.EXE，请选择原计算程序目录。");
    QProcess p;p.setProgram(exe);p.setWorkingDirectory(work);auto env=QProcessEnvironment::systemEnvironment();env.insert("GFORTRAN_CONVERT_UNIT","native");p.setProcessEnvironment(env);QByteArray log;
    auto capture=[&]{write(output+"/solver.log",log);for(auto f:QDir(work).entryInfoList(QDir::Files))if(!f.isSymLink()&&f.size()>0&&f.size()<=2*1024*1024)write(output+"/solver-files/"+f.fileName(),read(f.filePath()));};
#ifdef Q_OS_WIN
    struct Job{HANDLE h=CreateJobObjectW(nullptr,nullptr);~Job(){if(h)CloseHandle(h);}} job;JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job.h||!SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))bad("无法建立求解器进程组。");
    p.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* a){a->flags|=CREATE_NO_WINDOW;});
#endif
    try{p.start();while(p.state()==QProcess::Starting){p.waitForStarted(20);check();}if(p.error()==QProcess::FailedToStart)bad("AddMass 无法启动："+p.errorString());
#ifdef Q_OS_WIN
        HANDLE child=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(p.processId()));
        if(child){DWORD code=0;GetExitCodeProcess(child,&code);bool joined=code!=STILL_ACTIVE||AssignProcessToJobObject(job.h,child);CloseHandle(child);if(!joined)bad("无法跟踪求解器。");}else if(!p.waitForFinished(0))bad("无法跟踪求解器。");
#endif
        p.closeWriteChannel();while(p.state()!=QProcess::NotRunning){p.waitForFinished(20);log+=p.readAllStandardOutput()+p.readAllStandardError();if(log.size()>2*1024*1024)bad("求解器日志过大。");check();}log+=p.readAllStandardOutput()+p.readAllStandardError();check();capture();
        if(p.exitStatus()!=QProcess::NormalExit||p.exitCode()!=0)bad("ADD_MASS.EXE 运行失败，退出码："+QString::number(p.exitCode())+"。详情见本次 solver.log。");
    }catch(...){if(p.state()!=QProcess::NotRunning){p.kill();p.waitForFinished(3000);}log+=p.readAllStandardOutput()+p.readAllStandardError();capture();throw;}
    const auto raw=read(work+"/OUTA");auto parsed=parseAddedMass(raw,values);QJsonArray warnings=parsed["warnings"].toArray(),tables=parsed["tables"].toArray();
    QJsonObject empirical{{"status","not_applicable"}};
    const int type=values["object_type"].toInt();if(type==2||type==3){
        const double metres=settings.imperial?.3048:1.;Hydrostatics hydro(source,bindings);auto result=hydro.calculate({{"mode","single"},{"draft",values["draft"].toDouble()/metres},{"heel",0},{"trim",0}},output+"/cancel.flag");auto rows=result["rows"].toArray();
        if(!rows.isEmpty()&&rows[0].toObject()["status"]=="ok"){
            auto h=rows[0].toObject();const double denominator=h["lwl"].toDouble()*h["bwl"].toDouble()*(values["draft"].toDouble()/metres);
            if(denominator<=0)bad("无法取得经验公式的方形系数。");
            empirical=addedMassEmpirical(type,settings.length*metres,settings.beam*metres,values["draft"].toDouble(),h["volume"].toDouble()/denominator,values);
            if(empirical["status"]=="ok")tables.append(empirical["table"]);
            for(auto reason:empirical["reasons"].toArray())warnings.append(reason);
            if(empirical.contains("warning"))warnings.append(empirical["warning"]);
        }else{empirical={{"status","unavailable"}};warnings.append("无法提取有效静水力参数，仅显示数值矩阵，未生成经验值。");}}
    QString text=model->name+"\n附加质量矩阵（保持原求解器输出）\n"+QString::fromLatin1(raw).section('\n',1,10)+"\n实际面元数量："+num(parsed["panel_count"].toDouble())+"\n缩放除数："+num(parsed["scale_divisor"].toDouble())+"\n\n等效椭球\n";
    for(auto item:parsed["ellipsoid"].toArray()){auto e=item.toObject();QStringList vals;for(auto v:e["values"].toArray())vals<< (v.isNull()?QString("未定义"):QString::number(v.toDouble(),'g',10));text+=e["label"].toString()+" = "+vals.join(", ")+"\n";}
    if(empirical["status"]=="ok"){auto t=empirical["table"].toObject();text+="\n"+t["name"].toString()+"\nk11, k22, k66\n";auto rows=t["rows"].toArray(),vs=t["values"].toArray();for(qsizetype i=0;i<rows.size();++i){QStringList ns;for(auto v:vs[i].toArray())ns<<num(v.toDouble());text+=rows[i].toString()+": "+ns.join(", ")+"\n";}}
    check();return {{"calculator","addmass"},{"parameters",params},{"series",QJsonArray{}},{"tables",tables},{"matrix",parsed["matrix"]},{"panel_count",parsed["panel_count"]},{"scale_divisor",parsed["scale_divisor"]},{"ellipsoid",parsed["ellipsoid"]},{"empirical",empirical},{"geometry",grid.diagnostics},{"text",text},{"help",params["help"]},{"warnings",warnings},{"units",0},{"elapsed_ms",double(timer.elapsed())},{"engine","C++ AddMass workflow; original external ADD_MASS.EXE"},{"solver_sha256",QString::fromLatin1(QCryptographicHash::hash(read(exe),QCryptographicHash::Sha256).toHex())},{"solver_environment",QJsonObject{{"GFORTRAN_CONVERT_UNIT","native"}}}};
}
}
