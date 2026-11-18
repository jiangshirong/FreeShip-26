// GPL-2.0-or-later. FBM fields from Plus FreeProjectSettings/FreeTypes.
// Hydro range constraints match HydrostaticsEdits.inc. Performance settings are
// explicit patches: omitted fields and record padding survive unchanged. Saving
// input no longer constructs a hidden calculator form or mutates getter state.
#include "analysis_edit.h"
#include "calculation_parameters.h"
#include "analysis_fields.h"
#include "hydrodynamics.h"
#include "document_create.h"
#include "oortmerssen_fields.h"
#include "hollenbach_fields.h"
#include "fung_fields.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace fs::document {
namespace {
[[noreturn]] void bad(const char *message) {throw std::invalid_argument(message);}
double number(QJsonValue v) {
    const double n=v.toDouble();if (!v.isDouble() || !std::isfinite(n) || std::abs(n)>1e9) bad("Invalid bounded analysis number");return n;
}
bool boolean(QJsonValue v) {if(!v.isBool())bad("Invalid analysis flag");return v.toBool();}
QByteArray integer(int v) {QByteArray b(4,0);qToLittleEndian<qint32>(v,b.data());return b;}
QByteArray real(float v) {
    if (!std::isfinite(v)||std::abs(v)>1e9f) bad("Invalid stored analysis number");
    quint32 bits;std::memcpy(&bits,&v,4);QByteArray b(4,0);qToLittleEndian(bits,b.data());return b;
}
struct Read {
    QByteArray data;qsizetype at=0;
    QByteArray take(qsizetype n) {if(n<0||n>data.size()-at)bad("Truncated analysis field");auto b=data.mid(at,n);at+=n;return b;}
    int integer(){auto b=take(4);return qFromLittleEndian<qint32>(b.constData());}
    float real(){auto bits=quint32(integer());float n;std::memcpy(&n,&bits,4);return n;}
    bool flag(){return take(1)[0]!=0;}
    void string(){take(integer());}
    std::vector<float> list(){int n=integer();if(n<0||n>2000000)bad("Invalid analysis list");std::vector<float> a;for(int i=0;i<n;++i)a.push_back(real());return a;}
};
QByteArray list(const std::vector<float> &v) {auto b=integer(v.size());for(float n:v)b+=real(n);return b;}
struct Project {
    QByteArray raw;float length=1;int units=0;qsizetype density=-1,hydro=-1,hydroEnd=-1;
    std::vector<float> table,masses,angles,trims;QByteArray increments;bool useIncrements=true,freeTrim=true;float vcg=1;
    explicit Project(const Fbm &source):raw(source.blockBytes("project_settings")) {
        Read r{raw};const int v=source.version();
        if(v>=2) {r.string();r.string();length=r.real();r.take(9);density=r.at;r.take(13);units=r.integer();
            if(v>=6)r.take(5);if(v>=7)r.take(1);r.string();r.string();}
        if(v>=16){r.take(4);if(r.flag()){r.take(8);r.take(r.integer());}}
        if(v>=18)r.take(1);
        if(v>=20){hydro=r.at;for(int i=0;i<4;++i)table.push_back(r.real());masses=r.list();increments=r.take(12);useIncrements=r.flag();angles=r.list();trims=r.list();freeTrim=r.flag();vcg=r.real();hydroEnd=r.at;}
    }
    QByteArray hydroBytes() const {QByteArray b;for(float n:table)b+=real(n);return b+list(masses)+increments+char(useIncrements)+list(angles)+list(trims)+char(freeTrim)+real(vcg);}
};
TopologyEdit hydro(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    if(request["complete_snapshot"].toBool()){
        const auto edited=storeCompleteCalculationParameters(source,request);
        return {edited,bindings,edited.bytes()!=source.bytes()};
    }
    auto working=source.version()<20?promoteDocument(source,20,bindings):source;Project p(working);
    const auto before=p.hydroBytes();const auto mode=request["mode"].toString();
    QJsonObject exactRange;
    if(mode=="table") {
        const double start=number(request["start"]),end=number(request["end"]),step=number(request["step"]),trim=number(request["trim"]);
        if(start<0||end<start||end>1e6||step<1e-6||step>1e6||(end-start)/step>199+1e-8||std::abs(trim)>p.length)bad("无效的吃水表范围。");
        p.table={float(start),float(end),float(step),float(trim)};
        exactRange={{"start",start},{"end",end},{"step",step},{"trim",trim}};
    }else if(mode=="cross") {
        const double trim=number(request["trim"]);p.vcg=float(number(request["vcg"]));p.freeTrim=boolean(request["free_trim"]);
        if(std::abs(trim)>p.length)bad("纵倾超出范围。");p.trims={float(trim)};
        auto values=[&](const char *key,int limit,bool angle){
            if(!request[key].isArray())bad("Expected analysis list");const auto a=request[key].toArray();
            if(a.isEmpty()||a.size()>limit)bad("保存列表长度超出范围。");
            std::vector<float> out;double previous=0;
            for(auto v:a){const double n=number(v);
                if((angle&&std::abs(n)>180)||(!angle&&(n<1e-6||n>1e9)))bad("列表值超出范围。");
                if(!out.empty()&&(n<=previous||float(n)<=out.back()))bad("列表必须递增，且间隔不能小于 FBM 能保存的精度。");
                out.push_back(float(n));previous=n;
            }return out;
        };
        p.angles=values("angles",181,true);p.masses=values("displacements",100,false);p.useIncrements=false;
        if(p.angles.size()*p.masses.size()>500)bad("稳性表最多包含 500 个工况。");
    }else bad("只有吃水表和稳性曲线的范围可保存到 FBM。");
    const auto after=p.hydroBytes();
    const bool keepExact=!exactRange.isEmpty()&&(after!=before||request["exact_table_range"].toBool());
    if(after==before&&!keepExact)return {source,bindings,false};
    if(after!=before){
        p.raw.replace(p.hydro,p.hydroEnd-p.hydro,after);
        working=working.withBlocks({{"project_settings",p.raw}});
    }
    // Keep the legacy fields readable and store the evaluated values at full precision.
    if(keepExact)
        working=storeCompleteCalculationParameters(working,{{"calculator","hydro_table"},{"values",exactRange}});
    return {working,bindings,working.bytes()!=source.bytes()};
}
TopologyEdit performance(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    if(request["complete_snapshot"].toBool()){
        const auto edited=storeCompleteCalculationParameters(source,request);
        return {edited,bindings,edited.bytes()!=source.bytes()};
    }
    static const auto catalog=[] {auto c=QJsonDocument::fromJson(AnalysisFields).object();auto h=hydrodynamicsBindings();for(auto i=h.begin();i!=h.end();++i)c[i.key()]=i.value();
        auto prop=c["propeller4"].toObject();auto fields=prop["fields"].toObject();
        fields["WaterViscosity"]=QJsonObject{{"kind","number"},{"minimum",.01},{"maximum",100},{"offset",-1}};
        prop["fields"]=fields;c["propeller4"]=prop;
        prop=c["propeller5"].toObject();fields=prop["fields"].toObject();auto blades=fields["Edit5"].toObject();
        blades["minimum"]=2;fields["Edit5"]=blades;
        fields["BSeriesReynoldsMode"]=QJsonObject{{"kind","choice"},{"count",2},{"offset",-1}};
        fields["BSeriesReynoldsMillions"]=QJsonObject{{"kind","number"},{"minimum",2},{"maximum",2000},{"offset",-1}};
        prop["fields"]=fields;c["propeller5"]=prop;
        prop=c["propeller3"].toObject();fields=prop["fields"].toObject();
        fields["PassportSolver"]=QJsonObject{{"kind","choice"},{"count",2},{"offset",-1}};
        prop["fields"]=fields;c["propeller3"]=prop;
        for(const auto& method:{"propeller1","propeller2"}){
            prop=c[method].toObject();fields=prop["fields"].toObject();
            fields["DesignSolver"]=QJsonObject{{"kind","choice"},{"count",2},{"offset",-1}};
            for(const auto& key:{"DesignPitchLow","DesignPitchHigh"})fields[key]=QJsonObject{{"kind","number"},{"minimum",.5},{"maximum",1.4},{"offset",-1}};
            if(QString(method)=="propeller2")fields["DesignDiameterLimit"]=QJsonObject{{"kind","number"},{"minimum",0},{"maximum",25},{"offset",-1}};
            prop["fields"]=fields;c[method]=prop;
        }
        prop=c["oortmerssen"].toObject();fields=prop["fields"].toObject();
        auto viscosity=fields["ViscosityBox"].toObject();viscosity["maximum"]=100;fields["ViscosityBox"]=viscosity;
        for(auto entry:oortmerssenFields()){auto f=entry.toObject();f["offset"]=-1;if(f["kind"]=="choice")f["count"]=f["choices"].toArray().size();fields[f["key"].toString()]=f;}
        prop["fields"]=fields;c["oortmerssen"]=prop;
        prop=c["hollenbach"].toObject();fields=prop["fields"].toObject();
        viscosity=fields["ViscosityBox"].toObject();viscosity["maximum"]=100;fields["ViscosityBox"]=viscosity;
        for(auto entry:hollenbachFields()){auto f=entry.toObject();f["offset"]=-1;if(f["kind"]=="choice")f["count"]=f["choices"].toArray().size();fields[f["key"].toString()]=f;}
        prop["fields"]=fields;c["hollenbach"]=prop;
        prop=c["fung"].toObject();fields=prop["fields"].toObject();
        viscosity=fields["ViscosityBox"].toObject();viscosity["maximum"]=100;fields["ViscosityBox"]=viscosity;
        for(auto entry:fungFields()){auto f=entry.toObject();f["offset"]=-1;if(f["kind"]=="choice")f["count"]=f["choices"].toArray().size();fields[f["key"].toString()]=f;}
        prop["fields"]=fields;c["fung"]=prop;return c;}();
    if(!request["calculator"].isString() || !catalog.contains(request["calculator"].toString()))bad("未知计算方法。");
    const auto method=request["calculator"].toString();const auto spec=catalog[method].toObject(),fields=spec["fields"].toObject();
    if(request.contains("values")&&!request["values"].isObject())bad("计算参数必须为对象。");
    const auto patch=request["values"].toObject();
    if(method=="propeller5"&&patch.contains("Edit5")&&patch["Edit5"].toDouble()!=std::trunc(patch["Edit5"].toDouble()))bad("桨叶数必须为整数。");
    Project original(source);
    if(original.units==1 && method!="delft" && method!="kaper" &&
        !(method=="oortmerssen"&&patch["OortSolver"].toInt()==0&&patch["ComboBox"].toInt()==0)&&!(method=="hollenbach"&&patch["HolSolver"].toInt()==0)&&!(method=="fung"&&patch["FungSolver"].toInt()==0))bad("该方法仅支持公制。请先在工程设置中转换单位。");
    if(patch.isEmpty())return {source,bindings,false};
    auto working=promoteDocument(source,std::max(source.version(),spec["version"].toInt()),bindings);
    Project project(working);const auto block=spec["block"].toString();auto bytes=working.blockBytes(block);const auto before=bytes;
    QJsonObject validated;bool hasPersistedInput=false;
    // Validate the whole patch before changing even the temporary record.
    for(auto it=patch.begin();it!=patch.end();++it) {
        if(!fields.contains(it.key()))bad("未知计算参数。");const auto f=fields[it.key()].toObject();const auto kind=f["kind"].toString();
        if(kind=="boolean")validated[it.key()]=boolean(it.value());
        else {
            double n=number(it.value());
            if(kind=="choice") {if(std::floor(n)!=n||n<((it.key()=="BSeriesReynoldsMode"||it.key()=="PassportSolver"||it.key()=="DesignSolver"||it.key()=="OortSolver"||it.key().startsWith("Hol")||it.key()=="FungSolver")?0:-1)||n>=f["count"].toInt())bad("计算选项无效。");}
            else {
                double low=f["minimum"].toDouble(),high=f["maximum"].toDouble();
                if(method=="fung"&&patch.value("FungSolver").toInt()==0&&original.units==1){
                    if(QStringList{"LwlBox","DraftTotalBox","ZBulbBox"}.contains(it.key()))high/=.3048;
                    if(it.key()=="WettedSurfacebox"||it.key()=="BABox")high/=(.3048*.3048);
                    if(it.key()=="DisplacementBox")high/=std::pow(.3048,3);
                }
                if(method=="hollenbach"&&patch.value("HolSolver").toInt()==0){
                    if(it.key()=="DraftBox")low=-30;
                    if(original.units==1){
                        if(QStringList{"LwlBox","Losbox","DraftTotalBox","DraftBox"}.contains(it.key())){low/=.3048;high/=.3048;}
                        if(it.key()=="WettedSurfacebox")high/=(.3048*.3048);
                        if(it.key()=="DisplacementBox")high/=(.3048*.3048*.3048);
                    }
                }
                if(n<low||n>high)bad("计算参数超出范围。");if(f["integer"].toBool())n=std::nearbyint(n);
            }
            validated[it.key()]=n;
        }
    }
    auto put=[&](int offset,float n){if(offset<0||offset+4>bytes.size())bad("Invalid calculator offset");bytes.replace(offset,4,real(n));};
    for(auto it=validated.begin();it!=validated.end();++it) {
        const auto f=fields[it.key()].toObject();const int offset=f["offset"].toInt(-1);if(offset==-1)continue;
        const auto value=real(float(it.value().toDouble()+f["add"].toInt()));
        if(offset==-2) {if(project.density<0)bad("Missing water density field");project.raw.replace(project.density,4,value);}
        else {if(offset<0||offset+4>bytes.size())bad("Invalid calculator offset");bytes.replace(offset,4,value);}
        hasPersistedInput=true;
    }
    if(validated.value("EstimateBox").toBool()) {
        if(method=="fung"&&patch.value("FungSolver").toInt()==0)bad("Fung–Leibman 请填写或提取裸船湿表面积。");
        // Explicit estimate action. Use the entered draft, not uninitialized
        // hidden-form Tc/WlArea fields. All intermediate storage remains Single.
        auto fieldKey=[&](const char *key) {
            for(auto it=fields.begin();it!=fields.end();++it)if(it.key().compare(key,Qt::CaseInsensitive)==0)return it.key();
            return QString(key);
        };
        auto value=[&](const char *name)->float {
            const auto key=fieldKey(name);
            if(validated.contains(key))return float(validated[key].toDouble());
            auto f=fields[key].toObject();int offset=f["offset"].toInt(-1);
            if(offset<0)return 0;Read r{bytes,offset};return r.real();
        };
        const float length=value("LwlBox"),beam=value("BwlBox"),cp=value("CpBox"),displacement=value("DisplacementBox"),
            draft=value(method=="delft"?"DraftBox":"DraftTotalBox"),area=value("WlAreaBox");
        float wetted=0;
        if(length>0&&beam>0&&cp>0&&displacement>0&&draft>0) {
            const float am=displacement/(length*cp),cm=am/(beam*draft),cwp=area/(length*beam);
            if(cwp==0 && (method=="ost"||method=="rbhs"||method=="mh"))
                wetted=float(length*(1.36*draft+1.13*cp*cm*beam));
            else {
                const float c23=float(.453+.443*(cp*cm)-.286*cm-.00347*(beam/draft)+.37*cwp);
                const float factor=float(.616*c23+.111*cm*cm*cm+.245*(c23/cm)-.0228);
                wetted=factor*length*(2*draft+beam)*std::sqrt(cm);
            }
        }
        put(fields[fieldKey("WettedSurfaceBox")].toObject()["offset"].toInt(-1),wetted);hasPersistedInput=true;
    }
    // Manual parameter saves must not be overwritten by a persisted auto-extract
    // switch on the next defaults request. It is not a UI checkbox in Qt.
    if(hasPersistedInput){const int offset=spec["extract_offset"].toInt();if(offset<0||offset>=bytes.size())bad("Invalid extraction flag");bytes[offset]=0;}
    if(bytes==before && project.raw==working.blockBytes("project_settings"))return {source,bindings,false};
    return {working.withBlocks({{block,bytes},{"project_settings",project.raw}}),bindings,true};
}
}
bool nativeAnalysisCommand(const QJsonObject &r) {return r["method"]=="hydro_settings"||r["method"]=="performance_settings";}
TopologyEdit editAnalysis(const Fbm &source,const QJsonObject &bindings,const QJsonObject &request) {
    if(request["method"]=="hydro_settings")return hydro(source,bindings,request);
    if(request["method"]=="performance_settings")return performance(source,bindings,request);
    bad("Unsupported analysis edit");
}
}
