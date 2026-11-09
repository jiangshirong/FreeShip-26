// SPDX-License-Identifier: GPL-2.0-or-later
#include "cad_export.h"
#include "native/geometry.h"
#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QLocale>
#include <QSaveFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <tuple>

namespace fs::cad {
namespace {
[[noreturn]] void bad(const QString& text){throw std::runtime_error(text.toStdString());}
QString n(double v){if(!std::isfinite(v))bad("导出数据包含无效数值。");return QString::number(v,'g',17);}
double unit(const QString& u){if(u=="m")return 1;if(u=="mm")return .001;if(u=="cm")return .01;if(u=="ft")return .3048;if(u=="in")return .0254;bad("未知长度单位。");}
Vec3 scaled(Vec3 p,double s){return {p.x*s,p.y*s,p.z*s};}
QString clean(QString s){s.replace('\r',' ');s.replace('\n',' ');return s;}
QString hollerith(QString s){auto a=clean(s).toLatin1();for(auto& c:a)if(uchar(c)<32||uchar(c)>126)c='?';return QString::number(a.size())+'H'+QString::fromLatin1(a);}
int integer(const QJsonValue& v,int lo,int hi){double d=v.toDouble(-1);if(!v.isDouble()||!std::isfinite(d)||d!=std::floor(d)||d<lo||d>hi)bad("曲面整数参数无效。");return int(d);}
double number(const QJsonValue& v){if(!v.isDouble()||!std::isfinite(v.toDouble()))bad("曲面坐标或节点无效。");return v.toDouble();}
struct Patch {const Layer* layer=nullptr;int cols=0,rows=0,du=0,dv=0;QVector<double> u,v;QVector<Vec3> points;};
QVector<Patch> decode(const QJsonObject& data,const ModelSnapshot& m){
    if(data["schema"]!=1||data["document_id"]!=m.documentId||data["revision"].toDouble(-1)!=double(m.revision)||!data["patches"].isArray())bad("曲面数据与当前模型版本不一致。");
    const auto list=data["patches"].toArray();if(list.size()>100000)bad("曲面数量过多。");QVector<Patch> patches;qint64 total=0;
    for(auto value:list){auto o=value.toObject();Patch p;
        auto id=number(o["layer"]);if(id<1||id>9007199254740991.0||id!=std::floor(id))bad("曲面图层身份无效。");p.layer=m.layer(EntityId(id));if(!p.layer)bad("曲面图层不存在。");
        p.cols=integer(o["cols"],2,100000);p.rows=integer(o["rows"],2,100000);p.du=integer(o["degree_u"],1,5);p.dv=integer(o["degree_v"],1,5);
        total+=qint64(p.cols)*p.rows;if(total>2000000||p.cols<=p.du||p.rows<=p.dv)bad("曲面控制网大小无效。");
        auto knots=[&](QString key,int count,int degree){if(!o[key].isArray())bad("曲面缺少节点向量。");auto a=o[key].toArray();if(a.size()!=count+degree+1)bad("曲面节点数不一致。");QVector<double> v;for(auto item:a)v.append(number(item));if(!std::is_sorted(v.begin(),v.end())||v[degree]>=v[count])bad("曲面节点顺序或定义域无效。");return v;};
        p.u=knots("u",p.cols,p.du);p.v=knots("v",p.rows,p.dv);
        if(!o["points"].isArray())bad("曲面缺少控制点。");auto points=o["points"].toArray();if(points.size()!=qint64(p.cols)*p.rows)bad("曲面控制点数不一致。");
        for(auto item:points){if(!item.isArray()||item.toArray().size()!=3)bad("曲面控制点无效。");auto a=item.toArray();p.points.append({number(a[0]),number(a[1]),number(a[2])});}
        patches.append(std::move(p));
    }return patches;
}
struct Iges {
    QStringList directories,parameters;
    static QStringList pack(const QStringList& values,int width){
        QStringList lines;QString current;
        for(int i=0;i<values.size();++i){auto token=values[i]+(i+1==values.size()?';':',');
            // Numeric fields must remain in a single record. Only Hollerith
            // strings may span records; never insert padding inside a string.
            if(!current.isEmpty()&&current.size()+token.size()>width){lines<<current.leftJustified(width,' ');current.clear();}
            while(token.size()>width){lines<<token.left(width);token.remove(0,width);}
            current+=token;
        }
        if(!current.isEmpty())lines<<current.leftJustified(width,' ');return lines;
    }
    static QString idx(qint64 value){auto s=QString::number(value);if(s.size()>8)bad("IGES 字段超出范围。");return s.rightJustified(8,' ');}
    static QString line(QString text,QChar section,int index){if(text.size()>72||index>9999999)bad("IGES 记录超出范围。");return text.leftJustified(72,' ')+section+QString::number(index).rightJustified(7,' ');}
    int add(int type,QStringList values,int color,int layer){
        int de=directories.size()+1,first=parameters.size()+1;
        for(const auto& line:pack(values,64))parameters.append(line+idx(de));
        directories.append(idx(type)+idx(first)+idx(0)+idx(1)+idx(layer)+idx(0)+idx(0)+idx(0)+"00000000");
        directories.append(idx(type)+idx(0)+idx(color)+idx(parameters.size()-first+1)+idx(0)+idx(0)+idx(0)+QString(8,' ')+idx(0));return de;
    }
};
void iges(QTextStream& s,const QString& path,const ModelSnapshot& m,const exchange::Options& o,const QJsonObject& data,double scale){
    auto patches=decode(data,m);Iges file;QHash<EntityId,int> colors,levels;int count=0;double maximum=0;
    for(const auto& l:m.layers)if(o.includeHidden||l.visible){
        levels[l.id]=levels.size()+1;
        colors[l.id]=-file.add(314,{"314",n((l.color&255)*100.0/255),n(((l.color>>8)&255)*100.0/255),n(((l.color>>16)&255)*100.0/255),hollerith(l.name)},0,levels[l.id]);
    }
    for(const auto& p:patches)if(colors.contains(p.layer->id)){
        bool off=std::any_of(p.points.begin(),p.points.end(),[](Vec3 v){return v.y!=0;});
        for(int side=0;side<(o.fullHull&&p.layer->symmetric&&off?2:1);++side){
            QStringList v{"128",QString::number(p.cols-1),QString::number(p.rows-1),QString::number(p.du),QString::number(p.dv),"0","0","1","0","0"};
            if(side){for(auto it=p.u.crbegin();it!=p.u.crend();++it)v<<n(p.u.first()+p.u.last()-*it);}else for(double t:p.u)v<<n(t);
            for(double t:p.v)v<<n(t);for(int i=0;i<p.points.size();++i)v<<"1";
            for(int row=0;row<p.rows;++row)for(int col=0;col<p.cols;++col){auto point=p.points[row*p.cols+(side?p.cols-1-col:col)];if(side)point.y=-point.y;point=scaled(point,scale);for(double t:{point.x,point.y,point.z}){v<<n(t);maximum=std::max(maximum,std::abs(t));}}
            v<<n(p.u[p.du])<<n(p.u[p.cols])<<n(p.v[p.dv])<<n(p.v[p.rows]);file.add(128,v,colors[p.layer->id],levels[p.layer->id]);++count;
        }
    }
    if(!count)bad("所选范围没有可导出的曲面。");
    const int code=o.units=="m"?6:o.units=="mm"?2:o.units=="cm"?10:o.units=="ft"?4:1;
    auto date=hollerith(QDateTime::currentDateTimeUtc().toString("yyyyMMdd.HHmmss"));
    QStringList global{hollerith(","),hollerith(";"),hollerith("FreeShip_26"),hollerith(QFileInfo(path).fileName()),hollerith("FreeShip_26"),hollerith("FreeShip Qt"),"32","38","9","308","17",hollerith(""),"1",QString::number(code),hollerith(o.units.toUpper()),"1","1",date,n(std::max(1.0,maximum)*1e-7),n(maximum),hollerith(m.designer),hollerith(""),"9","1",date};
    const auto g=Iges::pack(global,72);int gc=0;
    s<<Iges::line("FreeShip_26 NURBS surface exchange",'S',1)<<'\n';
    for(const auto& line:g)s<<Iges::line(line,'G',++gc)<<'\n';
    for(int i=0;i<file.directories.size();++i)s<<Iges::line(file.directories[i],'D',i+1)<<'\n';
    for(int i=0;i<file.parameters.size();++i)s<<Iges::line(file.parameters[i],'P',i+1)<<'\n';
    s<<Iges::line("S"+QString::number(1).rightJustified(7,' ')+"G"+QString::number(gc).rightJustified(7,' ')+"D"+QString::number(file.directories.size()).rightJustified(7,' ')+"P"+QString::number(file.parameters.size()).rightJustified(7,' '),'T',1)<<'\n';
}
struct Curve {QString name;quint32 color;SectionSpline spline;};
QVector<Curve> curves(const ModelSnapshot& m){
    QVector<Curve> result;
    for(const auto& sec:m.sections){if(!sec.paths.isEmpty()&&sec.splines.size()!=sec.paths.size())bad("剖面缺少完整样条数据。");int segment=0;for(const auto& spline:sec.splines)result.append({QString("Section_%1_%2_%3").arg(sec.kind).arg(sec.index).arg(++segment),sec.color,spline});}
    for(const auto& c:m.controlCurves)result.append({"ControlCurve_"+QString::number(c.id),c.color,c.spline});
    for(const auto& c:result)if(c.spline.points.size()<2||c.spline.points.size()!=c.spline.knuckles.size())bad("曲线数据不完整。");
    return result;
}
}
bool supports(const QString& f){return f=="iges"||f=="dxf3d"||f=="dxfmesh"||f=="offsets"||f=="coordinates";}
QString title(const QString& f){if(f=="michlet")return "Michlet 计算输入";if(f=="addmass")return "AddMass 计算输入";if(f=="offset_table")return "矩形型值表";if(f=="dxfcrease")return "DXF 折痕与边界线";if(f=="part")return "FreeShip 几何部件";if(f=="waves")return "Michlet 波高网格";if(f=="vrml")return "VRML 静态网格";if(f=="hull")return "Carlson Hull 船型";if(f=="carene")return "Carene 折角线";if(f=="chines")return "折角线文本";if(f=="surface")return "曲线拟合曲面";if(f=="polycad")return "Polycad 控制网";if(f=="arch_hll")return "ArchimedesMB 船体";if(f=="arch_app")return "Archimedes 单体船";if(f=="pam")return "PAM 旧版几何模板";if(f=="ghs")return "GHS 船体几何";if(f=="iges")return "IGES 曲面";if(f=="dxf3d")return "DXF 三维剖面与控制曲线";if(f=="dxfmesh")return "DXF 三维面";if(f=="offsets")return "型值（剖面及控制曲线）";if(f=="coordinates")return "坐标点";return f.toUpper();}
QString extension(const QString& f){if(f=="michlet")return "mlt";if(f=="addmass")return "txt";if(f=="offset_table")return "csv";if(f=="waves")return "mlt";if(f=="vrml")return "wrl";if(f=="hull")return "hul";if(f=="carene")return "xyz";if(f=="chines"||f=="surface")return "txt";if(f=="polycad")return "geo";if(f=="arch_hll")return "hll";if(f=="arch_app")return "app";if(f=="ghs")return "gf";if(f=="iges")return "igs";if(f.startsWith("dxf"))return "dxf";if(f=="offsets"||f=="coordinates")return "txt";return f;}
bool acceptsExtension(const QString& f,const QString& e){return e.compare(extension(f),Qt::CaseInsensitive)==0||(f=="iges"&&e.compare("iges",Qt::CaseInsensitive)==0);}
void write(const QString& path,const QString& format,const ModelSnapshot& m,const exchange::Options& o,const QJsonObject& patches){
    if(!supports(format))bad("未知 CAD 导出格式。");const double scale=(m.units=="imperial"?.3048:1)/unit(o.units);
    QSaveFile file(path);file.setDirectWriteFallback(false);if(!file.open(QIODevice::WriteOnly))bad("无法创建目标文件："+file.errorString());
    QTextStream s(&file);s.setEncoding(QStringConverter::Utf8);s.setLocale(QLocale::c());
    if(format=="iges")iges(s,path,m,o,patches,scale);
    else if(format=="coordinates"){
        std::set<std::tuple<double,double,double>> seen;int count=0;
        auto point=[&](Vec3 p){auto key=std::make_tuple(p.x,p.y,p.z);if(!seen.insert(key).second)return;p=scaled(p,scale);s<<n(p.x)<<' '<<n(p.y)<<' '<<n(p.z)<<'\n';++count;};
        if(o.controlNet){for(const auto& p:m.points)point(p.position);}
        else for(const auto& t:m.triangles){auto* l=m.layer(t.layer);if(!l)bad("曲面图层丢失。");if(!o.includeHidden&&!l->visible)continue;for(auto p:t.vertices){point(p);if(o.fullHull&&l->symmetric&&p.y!=0)point({p.x,-p.y,p.z});}}
        if(!count)bad("所选范围没有坐标点。");
    }else if(format=="offsets"){
        auto list=curves(m);if(list.isEmpty())bad("没有剖面线或控制曲线，请先添加剖面。");
        s<<"# FreeShip_26 offsets units="<<o.units<<" axes=XYZ original_side\n";
        for(const auto& c:list){s<<c.name<<'\n';for(int i=0;i<c.spline.points.size();++i){auto p=scaled(c.spline.points[i],scale);s<<n(p.x)<<' '<<n(p.y)<<' '<<n(p.z);if(c.spline.knuckles[i])s<<" KNUCKLE";s<<'\n';}}
    }else{
        auto pair=[&](int code,const QString& v){s<<code<<'\n'<<v<<'\n';};auto point=[&](Vec3 p,int offset){p=scaled(p,scale);pair(10+offset,n(p.x));pair(20+offset,n(p.y));pair(30+offset,n(p.z));};
        auto color=[&](quint32 c){pair(420,QString::number(((c&255)<<16)|(c&0xff00)|((c>>16)&255)));};
        pair(0,"SECTION");pair(2,"HEADER");pair(9,"$ACADVER");pair(1,"AC1018");pair(9,"$INSUNITS");pair(70,QString::number(o.units=="m"?6:o.units=="mm"?4:o.units=="cm"?5:o.units=="ft"?2:1));pair(0,"ENDSEC");pair(0,"SECTION");pair(2,"ENTITIES");int count=0;
        if(format=="dxfmesh"){
            for(const auto& t:m.triangles){auto* l=m.layer(t.layer);if(!l)bad("曲面图层丢失。");if(!o.includeHidden&&!l->visible)continue;
                const bool off=std::any_of(t.vertices.begin(),t.vertices.end(),[](Vec3 p){return p.y!=0;});
                for(int side=0;side<(o.fullHull&&l->symmetric&&off?2:1);++side){pair(0,"3DFACE");pair(100,"AcDbEntity");pair(8,"Layer_"+QString::number(l->id));color(l->color);pair(100,"AcDbFace");for(int i=0;i<4;++i){int j=std::min(i,2);auto p=t.vertices[side?2-j:j];if(side)p.y=-p.y;point(p,i);}++count;}
            }
        }else for(const auto& c:curves(m)){
            std::vector<native_geometry::Vec3> pts;std::vector<bool> flags;
            for(int i=0;i<c.spline.points.size();++i){auto p=c.spline.points[i];pts.push_back({float(p.x),float(p.y),float(p.z)});flags.push_back(c.spline.knuckles[i]);}
            native_geometry::Spline spline(pts,flags);const auto& params=spline.parameters();const auto& deriv=spline.secondDerivatives();
            for(size_t i=1;i<pts.size();++i){auto a=pts[i-1],b=pts[i];double h=float(params[i]-params[i-1]);
                if(h<1e-6){if(a.x==b.x&&a.y==b.y&&a.z==b.z)continue;bad("曲线包含过短参数段，请先检查重复或极近的型值点。");}
                pair(0,"SPLINE");pair(100,"AcDbEntity");pair(8,c.name);color(c.color);pair(100,"AcDbSpline");pair(70,"0");pair(71,"3");pair(72,"8");pair(73,"4");pair(74,"0");for(int k=0;k<8;++k)pair(40,k<4?"0":"1");
                Vec3 p0{a.x,a.y,a.z},p3{b.x,b.y,b.z},p1,p2;
                auto controls=[&](double x,double y,double d0,double d1){if(flags[i-1]&&flags[i])d0=d1=0;double h2=float(float(h)*float(h)/6.0f);return std::pair{x+(y-x-h2*(2*d0+d1))/3,y-(y-x+h2*(d0+2*d1))/3};};
                auto x=controls(a.x,b.x,deriv[i-1].x,deriv[i].x),y=controls(a.y,b.y,deriv[i-1].y,deriv[i].y),z=controls(a.z,b.z,deriv[i-1].z,deriv[i].z);p1={x.first,y.first,z.first};p2={x.second,y.second,z.second};for(auto p:{p0,p1,p2,p3})point(p,0);++count;
            }
        }
        if(!count)bad("所选范围没有可导出的几何。");pair(0,"ENDSEC");pair(0,"EOF");
    }
    s.flush();if(s.status()!=QTextStream::Ok||file.error()!=QFile::NoError||!file.commit())bad("写入失败，原文件未被替换。");
}
}
