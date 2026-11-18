// SPDX-License-Identifier: GPL-2.0-or-later
// Adapted from Plus FreeHydrodyn_{Task1,Maneuv,RVRS}Dlg (Timoshenko V.F.).
// Native orchestration; original external solver licenses remain separate.
#include "hydrodynamics.h"
#include "core/development_calculation_samples.h"
#include "core/platform_process.h"
#include "calculation_parameters.h"
#include "report_language.h"
#include "calculation_tools.h"
#include "hydrodynamics_tables.h"
#include "geometry_input.h"
#include "hydrostatics.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
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
const QJsonObject &tables(){static auto t=QJsonDocument::fromJson(HydrodynamicsTables).object();return t;}
QByteArray read(const QString &path){const qint64 limit=QFileInfo(path).fileName().compare("RVRSRES.dat",Qt::CaseInsensitive)==0?32*1024*1024:2*1024*1024;QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()<=0||f.size()>limit)bad("缺少或无效的计算文件："+QFileInfo(path).fileName());return f.readAll();}
void write(const QString &path,const QByteArray &b){QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(b)!=b.size()||!f.commit())bad("无法保存计算文件："+path);}
std::vector<float> numbers(const QString &text,int count=-1){
    std::vector<float> result;for(auto token:text.trimmed().split(QRegularExpression("\\s+"),Qt::SkipEmptyParts)){
        bool ok=false;double n=token.toDouble(&ok);if(!ok||!std::isfinite(n)||!std::isfinite(float(n)))bad("计算文件包含无效数字。");result.push_back(float(n));
    }if(count>=0&&int(result.size())!=count)bad("计算文件的数字数量不完整。");return result;
}
QString fixed(float n,int width,int decimals){return QString::number(n,'f',decimals).rightJustified(width,' ');}
QStringList lines(const QByteArray &b){auto a=QString::fromLatin1(b).replace("\r\n","\n").split('\n');while(!a.isEmpty()&&a.last().isEmpty())a.removeLast();return a;}

class Calculation {
    std::shared_ptr<Hydrostatics> preparedHydro;
    const Fbm &source;QJsonObject request,bindings,schema;QMap<QString,QJsonObject> fields;
    QString method,output,solver,cache;QStringList warnings;QElapsedTimer timer;
    bool automaticInputsUnavailable=false;
    double stopSpeed=0;
    bool solverStoppedAtTarget=false;
    QJsonArray series,externalTools;QJsonObject scalars;QString report;
    float d(int n)const{return float(fields["Edit"+QString::number(n)]["value"].toDouble());}
    bool flag(int n)const{return fields["CheckBox"+QString::number(n)]["value"].toBool();}
    void set(int n,double v){if(!std::isfinite(v))bad("提取参数不是有限数值。");fields["Edit"+QString::number(n)]["value"]=double(float(v));}
    void check()const{if(QFile::exists(output+"/cancel.flag"))bad("计算已取消。");if(timer.elapsed()>110000)bad("计算超过 110 秒，已停止。");}
    QJsonObject parameters()const{auto s=schema;QJsonArray a;for(auto v:s["fields"].toArray())a.append(fields[v.toObject()["key"].toString()]);s["fields"]=a;
        if(automaticInputsUnavailable)s["auto_extract_unavailable"]=true;
        return s;
    }
    void validate()const{
        for(auto it=fields.begin();it!=fields.end();++it){const auto f=it.value();if(f["kind"]=="boolean")continue;
            double v=f["value"].toDouble();if(!std::isfinite(v)||v<f["minimum"].toDouble()||v>f["maximum"].toDouble()||(f["decimals"]==0&&std::trunc(v)!=v))bad("参数超出范围："+f["label"].toString());}
    }
    QString cachePath(){
        if(!cache.isEmpty())return cache;
        auto id=request["expected_document_id"].toString();id.remove('-');
        auto root=QFileInfo(output).dir().filePath("performance-cache/doc-"+id+"-r"+QString::number(qint64(request.value("calculation_cache_revision").toDouble(request["expected_revision"].toDouble()))));
        QFile tokenFile(root+"/current");if(!tokenFile.open(QIODevice::ReadOnly)||tokenFile.size()>256)bad("当前船模没有可用的阻力结果；尚未计算、计算失败或模型修改后均需重新计算阻力。");
        auto token=QString::fromLatin1(tokenFile.readAll()).trimmed();
        if(!QRegularExpression("^set-\\{[0-9a-fA-F-]{36}\\}$").match(token).hasMatch()||QUuid(token.mid(4)).isNull())bad("计算缓存版本无效。");
        cache=root+"/"+token;return cache;
    }
    void extract(bool automatic=false);
    void wind(const QString &work);
    void maneuver(const QString &work);
    void acceleration(const QString &work);
    void solverRun(const QString &name,const QString &work,const QString &result);
    void capture(const QString &work){
        QDir().mkpath(output+"/solver-files");qint64 total=0;
        for(auto f:QDir(work).entryInfoList(QDir::Files)){const qint64 limit=f.fileName().compare("RVRSRES.dat",Qt::CaseInsensitive)==0?32*1024*1024:2*1024*1024;
            if(!f.isSymLink()&&f.size()>0&&f.size()<=limit&&total+f.size()<=40*1024*1024){total+=f.size();write(output+"/solver-files/"+f.fileName(),read(f.filePath()));}}
    }
    void curve(const QString &name,const QString &x,const QString &y,const QJsonArray &points){series.append(QJsonObject{{"name",name},{"x_title",x},{"y_title",y},{"points",points}});}
public:
    Calculation(const Fbm &s,const QJsonObject &r,const QString &out,const QJsonObject &b,std::shared_ptr<Hydrostatics> prepared):preparedHydro(std::move(prepared)),source(s),request(r),bindings(b),method(r["calculator"].toString()),output(out){
        timer.start();check();if(documentGeometrySettings(source).imperial)bad("该方法仅支持公制。请先在工程设置中转换单位。");
        schema=tables()["schema"].toObject()[method].toObject();
        if(method=="acceleration"){
            auto a=schema["fields"].toArray();
            a.prepend(QJsonObject{{"key","TargetSpeed"},{"label","目标航速"},{"group","停止条件"},{"kind","number"},{"unit","kn"},{"minimum",0},{"maximum",1000},{"decimals",3},{"double_precision",true},{"value",8.0}});
            a.prepend(QJsonObject{{"key","StopAtSpeed"},{"label","加速到指定航速后停止"},{"group","停止条件"},{"kind","boolean"},{"value",false}});
            schema["fields"]=a;
        }
        for(auto v:schema["fields"].toArray()){auto f=v.toObject();fields[f["key"].toString()]=f;}
        bool hasLegacyParameters=false;
        auto spec=hydrodynamicsBindings()[method].toObject();
        if(request["development_sample"]!=true&&!spec.isEmpty()&&source.version()>=spec["version"].toInt()){auto bytes=source.blockBytes(spec["block"].toString());
            const int size=spec["extract_offset"].toInt();if(bytes.size()<size+1)bad("计算参数记录不完整。");bool nonzero=false;
            for(int at=0;at<size;++at)nonzero|=bytes.at(at)!=0;
            hasLegacyParameters=nonzero;
            if(nonzero)for(int at=0;at<size;at+=4){auto bits=qFromLittleEndian<quint32>(bytes.constData()+at);float n;std::memcpy(&n,&bits,4);set(at/4+2,n);}
        }
        if(request.contains("values")&&!request["values"].isObject())bad("计算参数必须为对象。");
        auto patch=request["values"].toObject();
        for(auto it=patch.begin();it!=patch.end();++it){if(!fields.contains(it.key()))bad("未知计算参数："+it.key());auto &f=fields[it.key()];
            if(f["kind"]=="boolean"){if(!it.value().isBool())bad("计算选项必须是布尔值。");f["value"]=it.value();}
            else{if(!it.value().isDouble())bad("计算参数必须是数字。");f["value"]=it.value();}}
        solver=calculationToolDirectory(request["solver_directory"].toString());
        if(request["extract"].toBool())extract();
        else if(!hasLegacyParameters&&shouldFillCalculationInputs(request)){
            const auto defaults=fields;
            try{extract(true);validate();}
            catch(const std::runtime_error&){fields=defaults;automaticInputsUnavailable=true;}
            catch(const std::invalid_argument&){fields=defaults;automaticInputsUnavailable=true;}
        }
        // A state-variable evaluation implies linear evaluation in MPP.
        const auto projectValues=request["project_values"].toObject();
        for(auto it=projectValues.begin();it!=projectValues.end();++it)if(fields.contains(it.key()))fields[it.key()]["value"]=it.value();
        if(method=="maneuver"&&flag(5))fields["CheckBox3"]["value"]=true;
        validate();
        if(method=="acceleration"){
            fields["StopAtSpeed"]["enabled"]=d(18)==1;
            fields["TargetSpeed"]["enabled"]=d(18)==1&&fields["StopAtSpeed"]["value"].toBool();
        }
    }
    QJsonObject run(){
        if(request["method"]=="performance_defaults")return parameters();
        QTemporaryDir temp(QDir::tempPath()+"/fs-hydrodyn-XXXXXX");if(!temp.isValid())bad("无法创建计算临时目录。");QString work=temp.path();
#ifdef Q_OS_WIN
        if(work.toLatin1()!=work.toUtf8()){auto path=QDir::toNativeSeparators(work).toStdWString();auto n=GetShortPathNameW(path.c_str(),nullptr,0);std::wstring shortPath(n,L'\0');if(n)n=GetShortPathNameW(path.c_str(),shortPath.data(),DWORD(shortPath.size()));if(!n)bad("旧求解器需要 ASCII 临时目录。");shortPath.resize(n);work=QString::fromStdWString(shortPath);if(work.toLatin1()!=work.toUtf8())bad("旧求解器需要 ASCII 临时目录。");}
#endif
        try{if(method=="wind")wind(work);else if(method=="maneuver")maneuver(work);else acceleration(work);check();capture(work);}catch(...){capture(work);throw;}
        QJsonArray limits;
        QJsonObject conventions;
        if(method=="acceleration"){
            const QString verifiedHash="50b9b1c79d27fb05a12902181b0c23d5b98050c764bcfcdfb3351d16336ec2e6";
            bool verified=false;
            for(auto tool:externalTools)for(auto file:tool.toObject()["files"].toArray())
                if(file.toObject()["file"].toString().compare("RVRSSHIP.EXE",Qt::CaseInsensitive)==0&&file.toObject()["sha256"]==verifiedHash)verified=true;
            conventions={{"engine_inertia_unit","kg*m^2"},{"engine_inertia_basis","per_propeller_shaft"},{"input_forwarded_without_conversion",true},{"automatic_propeller_inertia_added",true},{"unit_verified_for_executable",verified},{"verified_solver_sha256",verifiedHash},{"evidence","docs/migration/INERTIA-WIND-REVIEW.md"}};
            if(!verified){limits.append(QJsonObject{{"code","inertia-unverified-executable"},{"independently_verified",false}});warnings<<"请核对当前求解器要求的惯性矩单位。";}
        }
        return {{"calculator",method},{"parameters",parameters()},{"series",series},{"scalars",scalars},{"text",report},{"help",schema["help"]},{"warnings",QJsonArray::fromStringList(warnings)},{"elapsed_ms",double(timer.elapsed())},{"units",0},{"engine","C++ hydrodynamics workflow; original external solvers"},{"external_tools",externalTools},{"verification_limits",limits},{"numeric_conventions",conventions}};
    }
};

void Calculation::solverRun(const QString &name,const QString &work,const QString &result){
    check();externalTools.append(requireCalculationTool(solver,name));const auto exe=QDir(solver).filePath(name);
    QProcess p;p.setProgram(exe);p.setWorkingDirectory(work);QByteArray log;
    capture(work); // Legacy solvers delete their task file after consuming it.
#ifdef Q_OS_WIN
    struct Job{HANDLE h=CreateJobObjectW(nullptr,nullptr);~Job(){if(h)CloseHandle(h);}}job;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job.h||!SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limit,sizeof(limit)))bad("无法建立求解器进程组。");
    p.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a){a->flags|=CREATE_NO_WINDOW;});
#endif
    fs::platform::prepareLegacySolver(p,report_language::text(fs::platform::legacySolverWineMissing,{}));
    p.start();try{
        while(p.state()==QProcess::Starting){p.waitForStarted(20);check();}
        if(p.error()==QProcess::FailedToStart)bad(name+" 无法启动："+p.errorString());
#ifdef Q_OS_WIN
        HANDLE child=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(p.processId()));
        if(child){DWORD code=0;GetExitCodeProcess(child,&code);bool joined=code!=STILL_ACTIVE||AssignProcessToJobObject(job.h,child);CloseHandle(child);if(!joined)bad("无法跟踪求解器。");}else if(!p.waitForFinished(0))bad("无法跟踪求解器。");
#endif
        if(name=="ManeuvPP.EXE")p.write("\r\n"); // MPP pauses after writing OUT.
        qint64 consumed=0;QByteArray pending;
        p.closeWriteChannel();while(p.state()!=QProcess::NotRunning){
            p.waitForFinished(20);log+=p.readAllStandardOutput()+p.readAllStandardError();if(log.size()>2*1024*1024)bad("求解器日志过大。");check();
            // Observe complete solver records only. Some runtimes buffer their output;
            // the same endpoint clipping is also applied after normal completion.
            if(stopSpeed>0&&name=="RVRSSHIP.EXE"&&p.state()!=QProcess::NotRunning){
                QFile history(work+"/"+result);
                if(history.open(QIODevice::ReadOnly)&&history.seek(consumed)){
                    if(history.size()>32*1024*1024)bad("加速／制动输出文件过大。");
                    auto chunk=history.readAll();consumed+=chunk.size();pending+=chunk;
                    int end;while((end=pending.indexOf('\n'))>=0){
                        const auto line=pending.left(end);pending.remove(0,end+1);
                        if(line.trimmed().isEmpty())continue;
                        const auto row=numbers(QString::fromLatin1(line),6);
                        if(row[0]>0&&row[2]>=stopSpeed){
                            const auto prefix=consumed-pending.size();history.close();
                            p.kill();if(!p.waitForFinished(3000))bad("无法停止加速求解器。");
                            QFile completed(work+"/"+result);
                            if(!completed.open(QIODevice::ReadWrite)||!completed.resize(prefix))bad("无法保留目标航速计算结果。");
                            solverStoppedAtTarget=true;break;
                        }
                    }
                }
            }
        }
        fs::platform::adoptLegacyOutputs(work,{result});
        log+=p.readAllStandardOutput()+p.readAllStandardError();write(output+"/solver.log",log);check();
        if(!solverStoppedAtTarget&&(p.exitStatus()!=QProcess::NormalExit||p.exitCode()!=0))bad(name+" 运行失败，退出码："+QString::number(p.exitCode()));
        read(work+"/"+result);
    }catch(...){if(p.state()!=QProcess::NotRunning){p.kill();p.waitForFinished(3000);}throw;}
}

void Calculation::extract(bool automatic){
    auto settings=documentGeometrySettings(source);
    if(!preparedHydro||!preparedHydro->updateCompatibleSource(source,bindings))preparedHydro=std::make_shared<Hydrostatics>(source,bindings);
    auto analysis=preparedHydro->calculate({{"mode","single"},{"draft",settings.draft},{"heel",0},{"trim",0}},output+"/cancel.flag");
    auto rows=analysis["rows"].toArray();if(rows.isEmpty()||rows[0].toObject()["status"]!="ok")bad("当前船模无法提取静水力参数。");auto h=rows[0].toObject();
    auto value=[&](QString key){if(!h[key].isDouble())bad("缺少静水力参数："+key);return h[key].toDouble();};
    if(method=="wind"){
        const auto length=preparedHydro->longitudinalSpan(),width=value("bwl");
        if(!(length>0&&length<=fields["Edit2"]["maximum"].toDouble()&&width>0&&width<=fields["Edit3"]["maximum"].toDouble()))
            bad("当前船模无法提取静水力参数。");
        set(2,length);set(3,width);
        // Above-water geometry cannot supply the missing superstructure/mast data.
        // The user's wind areas remain manual; never substitute underwater area.
    }else if(method=="maneuver"){
        set(2,value("lwl"));set(3,value("bwl"));set(4,settings.draft);set(5,settings.draft);set(6,value("cb"));
        // The hull geometry does not establish the center of gravity.
        // Leave LCG at its saved or entered value.
        set(10,settings.density*1000);
        // Geometry has no temperature. Retain the entered/persisted viscosity;
        // extracting hull particulars must not reset it to a hidden 15 C value.
    }else{
        set(2,settings.length);set(3,value("bwl"));set(4,settings.draft);set(5,value("volume"));set(6,value("wetted_area"));
        const auto fillPropulsion=[&] {
            validateCalculationCache(cachePath(),true);
            if(!QFile::exists(cachePath()+"/RESISTp.dat")||!QFile::exists(cachePath()+"/Propres.dat"))bad("当前结果未提供推进参数，请手动填写螺旋桨和主机参数。");
            auto resistance=lines(read(cachePath()+"/RESISTp.dat"));if(resistance.size()<2)bad("阻力缓存不完整。");auto h=numbers(resistance[1]);if(h.size()<5)bad("阻力缓存不完整。");
            auto propulsion=lines(read(cachePath()+"/Propres.dat"));if(propulsion.size()!=2)bad("请先完成当前版本的推进计算，或手动填写推进参数。");auto p=numbers(propulsion[1],15);
            set(7,h[1]);set(12,h[2]);set(13,h[3]);set(8,p[0]);set(9,p[1]);set(10,p[2]);set(11,p[3]);set(14,p[14]);set(15,p[13]);set(16,p[10]*p[11]);
        };
        if(!automatic){fillPropulsion();return;}
        const auto geometry=fields;
        try{fillPropulsion();validate();}
        catch(const std::runtime_error&){fields=geometry;automaticInputsUnavailable=true;}
        catch(const std::invalid_argument&){fields=geometry;automaticInputsUnavailable=true;}
    }
}

// Local quadratic SFINEX1 interpolation, retained from Plus for coefficient
// samples. Endpoint samples are exact; no geometry or drawing data participates.
float interpolate(const std::vector<float>&x,const std::vector<float>&y,float p){
    const int n=int(x.size());if(n<3||y.size()!=x.size())bad("气动力系数表不完整。");
    if(p<=x.front())return y.front();if(p>=x.back())return y.back();
    auto quadratic=[&](int i){return (p-x[i+1])*(p-x[i+2])*y[i]/((x[i]-x[i+1])*(x[i]-x[i+2]))+(p-x[i])*(p-x[i+2])*y[i+1]/((x[i+1]-x[i])*(x[i+1]-x[i+2]))+(p-x[i])*(p-x[i+1])*y[i+2]/((x[i+2]-x[i])*(x[i+2]-x[i+1]));};
    int hi=int(std::lower_bound(x.begin(),x.end(),p)-x.begin());if(x[hi]==p)return y[hi];
    if(hi==1)return quadratic(0);if(hi==n-1)return quadratic(n-3);return float(.5*(quadratic(hi-2)+quadratic(hi-1)));
}
void Calculation::wind(const QString &work){
    if(d(2)<=0||d(3)<=0||d(8)>d(2))bad("总长、船宽必须为正，形心位置不能超过总长。");
    std::vector<float> angle,cx,cy,cm;
    if(d(9)<12){
        QString input="Merchant ship\r\n";const int width[]={8,8,10,10,10,10,10,8},decimals[]={4,4,3,3,3,3,3,0};
        for(int i=0;i<8;++i)input+=fixed(d(i+2),width[i],decimals[i]);input+="\r\n*** End of file ***\r\n";
        write(work+"/INO",input.toLatin1());solverRun("Ishercof.exe",work,"OUT");auto ls=lines(read(work+"/OUT"));
        if(ls.size()<54)bad("Ishercof 输出不足 19 个角度。");report=ls.join('\n');
        for(int i=0;i<19;++i){auto row=numbers(ls[35+i],7);if(row[0]!=i*10)bad("Ishercof 输出角度不完整。");angle.push_back(row[0]);cx.push_back(row[1]);cy.push_back(row[3]);cm.push_back(row[5]);}
    }else{
        // Type 13 uses 0.91, matching executable source (its comment says .091).
        static const float coeff[4][9]={{.42,.05,.0097,.042,1.18,-.13,-.104,.0894,.00845},{.91,.047,-.0144,.0063,1.08,.14,.025,.1,.0158},{.13,.025,.0204,.028,1.053,.023,0,.0646,-.00686},{.38,.014,.0878,.0554,1.767,.0167,0,.081,.0115}};
        auto c=coeff[int(d(9))-12];for(int i=0;i<37;++i){float a=i*5,y=float(a/180*3.1415926);angle.push_back(a);cx.push_back(float(c[0]*std::cos(double(y))+c[1]*std::sin(double(y))+c[2]*std::sin(2.*y)+c[3]*std::sin(4.*y)));cy.push_back(float(c[4]*std::sin(double(y))+c[5]*std::sin(3.*y)));cm.push_back(float(c[6]*std::sin(double(y))+c[7]*std::sin(2.*y)+c[8]*std::sin(4.*y)));}
        report=report_language::text("角度 / Cx / Cy / Cmz","wind.results")+'\n';
        for(size_t i=0;i<angle.size();++i)report+=QString("%1  %2  %3  %4\n").arg(angle[i]).arg(cx[i],0,'g',9).arg(cy[i],0,'g',9).arg(cm[i],0,'g',9);
    }
    QJsonArray a,b,c;for(int i=0;i<=36;++i){float x=i*5;a.append(QJsonArray{x,interpolate(angle,cx,x)});b.append(QJsonArray{x,interpolate(angle,cy,x)});c.append(QJsonArray{x,double(float(interpolate(angle,cm,x)*10))});}
    curve("Cx","相对风角 (°)","Cx",a);curve("Cy","相对风角 (°)","Cy",b);curve("10 × Cmz","相对风角 (°)","10 × Cmz",c);
    const double rad=d(12)*std::acos(-1.)/180.,vx=d(10)+d(11)*std::cos(rad),vy=d(11)*std::sin(rad),speed=std::hypot(vx,vy);
    const double signedAngle=speed>1e-12?std::atan2(vy,vx)*180/std::acos(-1.):0,angleAbs=std::abs(signedAngle),sign=signedAngle<0?-1.:1.;
    // The original Isherwood help specifies AT and AT*LOA. For the separate
    // OIIMF table this remains a compatibility assumption, not verified physics.
    const double scale=.5*1.2*speed*speed*d(6)/1000;
    scalars={{"relative_wind_m_s",speed},{"relative_wind_angle_deg",signedAngle},{"longitudinal_force_kN",interpolate(angle,cx,float(angleAbs))*scale},{"lateral_force_kN",interpolate(angle,cy,float(angleAbs))*scale*sign},{"yaw_moment_kNm",interpolate(angle,cm,float(angleAbs))*scale*d(2)*sign}};
    const auto appendWindValue=[&](const char* label,const char* unit,double value){
        report+="\n"+report_language::text(QString::fromUtf8(label),"wind.results")+" ("+
            QString::fromUtf8(unit)+"): "+QString::number(value,'g',10);
    };
    appendWindValue("相对风速","m/s",speed);
    appendWindValue("相对风角","°",signedAngle);
    appendWindValue("纵向力","kN",scalars["longitudinal_force_kN"].toDouble());
    appendWindValue("横向力","kN",scalars["lateral_force_kN"].toDouble());
    appendWindValue("艏摇力矩","kN·m",scalars["yaw_moment_kNm"].toDouble());
}
void Calculation::maneuver(const QString &work){
    for(int n:{2,3,4,5,6,8,9,10,11,12,13,14})if(d(n)<=0)bad("操纵性参数必须大于零。");
    const float trim=(d(4)-d(5))/d(2),lb=d(2)/d(3);const double fr=d(9)*.51444/std::sqrt(9.81*d(2));bool valid;
    if(d(17)<=1)valid=d(2)>=50&&d(2)<=330&&d(6)>=.5&&d(6)<=.9&&trim>=0&&trim<=.05&&lb>=5&&lb<=10&&d(20)<=45&&d(15)<=.041&&d(16)>=-.109&&d(16)<=.041&&fr>=.04&&fr<=.35;
    else valid=d(2)>=70&&d(2)<=230&&d(6)>=.4&&d(6)<=.7&&trim>=-.01&&trim<=.03&&lb>=5&&lb<=17&&d(20)<=35&&d(15)<=.041&&d(16)>=-.1&&d(16)<=.001&&fr>=.06&&fr<=.7;
    if(!valid)bad("操纵性参数超出原方法有效域，请检查 L、Cb、纵倾、L/B、舵角、舵面积、艏面积和 Fr。");
    if(flag(4)&&d(20)<=0)bad("回转预估需要大于零的舵角。");
    float close=d(18),rudders=d(19);if(d(17)==0){close=0;rudders=0;}if(d(17)==1)rudders=d(18)==0?1:2;if(d(17)==2)close=1;
    QString input=fixed(d(2),11,6)+fixed(d(3),10,6)+fixed(d(4),10,6)+fixed(d(5),10,6)+fixed(d(6),10,6)+fixed(d(7),12,6)+fixed(d(15),10,6)+fixed(close,2,0)+"\r\n";
    input+=fixed(d(8),10,6)+fixed(d(9),10,6)+fixed(d(12),10,6)+fixed(d(13),12,6)+fixed(d(14),10,6)+fixed(d(16),10,5)+fixed(d(17),2,0)+fixed(rudders,2,0)+"\r\n";
    input+=fixed(flag(4)?d(20):0,10,6)+fixed(d(10),12,6)+fixed(float(d(11)/1000000),14,11)+fixed(flag(3),2,0)+fixed(flag(4),2,0)+fixed(flag(5),2,0)+"\r\n";
    write(work+"/INM",input.toLatin1());solverRun("ManeuvPP.EXE",work,"OUT");auto ls=lines(read(work+"/OUT"));
    report=ls.join('\n');
    if(ls.size()<57||ls.size()>300||!report.contains("Maneuvering Prediction Program")||!report.contains("Submerged Bow Area")||
       (flag(3)&&!report.contains("Closed Loop Phase Margin with Steering Engine"))||
       (flag(4)&&!report.contains("Steady Speed in Turn"))||
       (flag(5)&&!report.contains("gamma 32")))bad("操纵性求解器没有返回所选项目的完整报告。");
    if(QRegularExpression("\\b(?:nan|inf(?:inity)?)\\b|=\\s*\\*{2,}",QRegularExpression::CaseInsensitiveOption).match(report).hasMatch())bad("操纵性结果包含非有限数值，请检查输入和方法适用范围。");
    if(!flag(3)&&!flag(4)&&!flag(5))warnings<<"未选择分析选项；报告仅包含输入参数。";
}
void Calculation::acceleration(const QString &work){
    double sampledMaximum=0;
    for(int n=2;n<=18;++n)if(d(n)<=0&&n!=12&&n!=13)bad("加速／制动参数必须为正。");
    const bool sample=request["development_sample"]==true;
    QByteArray resistance;
    if(sample){
        resistance=fs::developmentCalculation::accelerationResistance();
        sampledMaximum=fs::developmentCalculation::accelerationMaximumKn*1852./3600.;
    }else{
        validateCalculationCache(cachePath());
        QString resistanceFile=cachePath()+"/RESIST.dat";
        QJsonObject state;
        if(QFile::exists(cachePath()+"/RESIST-acceleration.json")){
            state=QJsonDocument::fromJson(read(cachePath()+"/RESIST-acceleration.json")).object();
            if(state["version"]!=1||!state["available"].isBool())bad("阻力衔接状态损坏，请重新计算阻力。");
            if(!state["available"].toBool())bad(state["reason"].toString("本次阻力结果不能用于加速计算，请重新计算阻力。"));
            sampledMaximum=state["speed_max_kn"].toDouble()*1852./3600.;
            if(state["separate_table"].toBool())resistanceFile=cachePath()+"/RESIST-acceleration.dat";
        }
        if(!QFile::exists(resistanceFile))bad("当前阻力结果缺少加速所需的数据，请重新计算阻力。");
        resistance=read(resistanceFile);
        if(state.contains("resist_sha256")&&state["resist_sha256"]!=QString::fromLatin1(QCryptographicHash::hash(resistance,QCryptographicHash::Sha256).toHex()))bad("阻力衔接数据校验失败，请重新计算阻力。");
    }
    const auto r=numbers(QString::fromLatin1(resistance));
    if(r.size()!=25)bad("阻力交接表应有五个航速点，每个航速点有五个数值。请重新计算阻力。");
    for(int i=0;i<5;++i){if(r[i*5]<=0||r[i*5+1]<0)bad("阻力衔接表含非正航速或负阻力，请重新计算阻力。");
        if(i>0&&r[i*5]<=r[(i-1)*5])bad("阻力衔接表存在重复或倒序航速，请重新计算阻力。");}
    const bool targetEnabled=d(18)==1&&fields["StopAtSpeed"]["value"].toBool();
    const double targetKn=fields["TargetSpeed"]["value"].toDouble();
    const double maximumKn=sampledMaximum>0?sampledMaximum*3600./1852.:double(r[20]);
    if(targetEnabled){
        if(targetKn<=0)bad("目标航速必须大于 0 kn。");
        if(targetKn>maximumKn+1e-6)bad(report_language::text("目标航速 %1 kn 超出阻力曲线范围；最高有效航速为 %2 kn。请降低目标航速或扩大阻力计算的航速范围。","acceleration").arg(targetKn,0,'f',3).arg(maximumKn,0,'f',3));
        stopSpeed=targetKn*1852./3600.;
    }
    write(work+"/resist.dat",resistance);
    const float density=sample?1.025f:documentGeometrySettings(source).density;
    std::vector<float> data{d(2),d(3),d(4),d(5),d(6),density,d(7),d(8),d(9),d(10),d(11),d(12),d(13),d(14),d(15),d(16),d(17),d(18),.001f,2.f,.1f,d(18)==1?0:r[15],d(15)};
    QString input;for(float n:data)input+=QString::number(n,'E',8)+"\r\n";write(work+"/TMP4.tsk",input.toLatin1());
    solverRun("RVRSSHIP.EXE",work,"RVRSRES.dat");if(!solverStoppedAtTarget&&QFile::exists(work+"/TMP4.tsk"))bad("加速／制动求解器未完成计算。");
    auto ls=lines(read(work+"/RVRSRES.dat"));QJsonArray points[5];double last=-1,highestSpeed=0;std::vector<double> final,previous;bool reached=false;
    // A 32 MiB RVRS-specific limit bounds the full time history. An instantaneous
    // shaft-speed change may publish two states at the same timestamp.
    int rowIndex=0;for(auto line:ls){if((rowIndex++&255)==0)check();if(line.trimmed().isEmpty())continue;const auto raw=numbers(line,6);std::vector<double> row(raw.begin(),raw.end());if(row[0]<0||row[0]<last)bad("加速／制动输出时间发生倒退。");last=row[0];highestSpeed=std::max(highestSpeed,row[2]);
        if(targetEnabled&&row[2]>=stopSpeed){
            if(previous.empty()||previous[2]>=stopSpeed||row[0]<=previous[0])bad("缺少目标航速附近的有效时间步，无法确定停止时刻。");
            const double fraction=(stopSpeed-previous[2])/(row[2]-previous[2]);
            for(int i=0;i<6;++i)row[i]=previous[i]+fraction*(row[i]-previous[i]);
            row[2]=stopSpeed;reached=true;
        }
        final=row;previous=row;if(row[0]==0)continue;
        if(sampledMaximum>0&&(row[2]>sampledMaximum+1e-4||row[2]<-1e-4))bad(report_language::text("计算航速超出当前阻力曲线的正向范围。最高有效航速：%1 kn；实际越界航速：%2 kn。请扩大阻力航速范围，或启用“加速到指定航速后停止”并降低目标航速；同时核对主机及螺旋桨参数。","acceleration").arg(maximumKn,0,'f',3).arg(row[2]*3600./1852.,0,'f',6));
        // Keep the solver's raw values; presentation uses the exact nautical
        // mile definition and does not round the converted curve to float again.
        const double y[]={row[1]/1852.,row[2]*3600./1852.,row[3]/10.,row[4],row[5]};for(int i=0;i<5;++i)points[i].append(QJsonArray{row[0],y[i]});if(reached)break;}
    if(targetEnabled&&!reached)bad(report_language::text("未达到目标航速 %1 kn；本次最高计算航速为 %2 kn。请核对主机和螺旋桨参数，或降低目标航速。","acceleration").arg(targetKn,0,'f',3).arg(highestSpeed*3600./1852.,0,'f',3));
    if(final.empty()||points[0].isEmpty())bad("没有有效的加速／制动时间历程。");
    const QString names[]={"距离","航速","桨转速 × 0.1","推力","转矩"},units[]={"n mile","kn","0.1 × r/min","kN","kN·m"};for(int i=0;i<5;++i)curve(names[i],"时间 (s)",units[i],points[i]);
    scalars={{"time_s",final[0]},{"distance_m",final[1]},{"speed_m_s",final[2]},{"shaft_rpm",std::abs(final[3])}};
    if(targetEnabled){scalars["target_speed_kn"]=targetKn;scalars["target_reached"]=reached;scalars["solver_stopped_at_target"]=solverStoppedAtTarget;scalars["endpoint_interpolated"]=true;}
    report=QString("任务 %1\n时间 (s)：%2\n距离 (m)：%3\n末速 (m/s)：%4\n末转速 (r/min)：%5\n").arg(int(d(18))).arg(final[0]).arg(final[1]).arg(final[2]).arg(std::abs(final[3]));
    if(targetEnabled)report+=QString("已达到目标航速：%1 kn\n").arg(targetKn,0,'f',3);
    scalars["engine_inertia_kg_m2"]=double(d(17));
    report+=QString("主机等效惯性矩 (kg·m²，单根桨轴基准)：%1\n求解器另加桨参数相关惯性项。原输入数值保持不变。\n").arg(d(17));
}
}
bool nativeHydrodynamics(const QString &m){return tables()["schema"].toObject().contains(m);}
QJsonObject hydrodynamicsBindings(){return tables()["bindings"].toObject();}
QJsonObject hydrodynamics(const Fbm &source,const QJsonObject &request,const QString &output,const QJsonObject &bindings,std::shared_ptr<Hydrostatics> prepared){
    report_language::Scope locale(request);
    return Calculation(source,request,output,bindings,std::move(prepared)).run();
}
}
