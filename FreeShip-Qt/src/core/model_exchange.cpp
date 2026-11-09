// SPDX-License-Identifier: GPL-2.0-or-later
#include "model_exchange.h"
#include "curve_import.h"
#include "document/fbm_text.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QJsonArray>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QStringDecoder>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>
#include <tuple>
#include <functional>

namespace fs::exchange {
namespace {
constexpr int maxPoints=100000,maxFaces=100000,maxCorners=4096;
constexpr qint64 maxBytes=128LL*1024*1024;
[[noreturn]] void bad(const QString& text){throw std::runtime_error(("模型交接："+text).toUtf8().constData());}
double number(const QString& s){bool ok=false;double n=s.toDouble(&ok);if(!ok||!std::isfinite(n)||std::abs(n)>1e9)bad("无效或超出范围的数字："+s.left(40));return n;}
int integer(const QString& s,int lo,int hi){bool ok=false;auto n=s.toLongLong(&ok);if(!ok||n<lo||n>hi)bad("无效的索引或数量："+s.left(40));return int(n);}
bool flag(const QString& s){if(s=="1"||s=="-1"||s.compare("true",Qt::CaseInsensitive)==0)return true;if(s=="0"||s.compare("false",Qt::CaseInsensitive)==0)return false;bad("无效的布尔值："+s);}
QStringList words(QString s){s.replace('\t',' ');return s.simplified().split(' ',Qt::SkipEmptyParts);}
void sizeIs(const QStringList& row,int n){if(row.size()!=n)bad("记录的字段数不正确。");}
QString text(const QByteArray& bytes){QStringDecoder decode(QStringDecoder::Utf8);QString s=decode(bytes);if(decode.hasError()||s.contains(QChar(0)))bad("文本须为 UTF-8，且不能含空字符。");return s;}
double metres(const QString& unit){if(unit=="m")return 1;if(unit=="mm")return .001;if(unit=="cm")return .01;if(unit=="ft")return .3048;if(unit=="in")return .0254;bad("不支持的长度单位。");}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 unit(Vec3 a){double n=std::sqrt(dot(a,a));return n?Vec3{a.x/n,a.y/n,a.z/n}:Vec3{};}
double coreFloat(double value){volatile float stored=static_cast<float>(value);return stored;}
Vec3 imported(Vec3 p,const Options& o){
    if(o.legacyAxes)p={p.z,p.x,p.y};
    const double scale=metres(o.units)/(o.units=="ft"?.3048:1.);
    p={p.x*scale,p.y*scale,p.z*scale};
    for(double n:{p.x,p.y,p.z})if(!std::isfinite(n)||std::abs(n)>1e9)bad("换算后的坐标超出范围。");
    // The editing core is Single. Weld/degeneracy checks must use exactly the
    // coordinates it will receive, not a higher precision temporary shape.
    return {coreFloat(p.x),coreFloat(p.y),coreFloat(p.z)};
}
quint64 edgeKey(EntityId a,EntityId b){if(a>b)std::swap(a,b);return (a<<32)|b;}
Layer layer(QString name,EntityId id,bool symmetric=false){Layer l;l.id=id;l.name=name;l.color=0xb4b4b4;l.symmetric=symmetric;return l;}
ControlPoint point(Vec3 p,EntityId id,int type=3){ControlPoint v;v.id=id;v.position=p;v.vertexType=type;return v;}
void addPoint(Document& d,Vec3 p,int type){if(d.model.points.size()>=maxPoints)bad("导入控制点超过 100000 个。");d.model.points.append(point(p,d.model.points.size()+1,type));}
void addFace(Document& d,QVector<EntityId> ids,EntityId l){if(d.model.faces.size()>=maxFaces)bad("导入面超过 100000 个。");d.model.faces.append({EntityId(d.model.faces.size()+1),l,std::move(ids)});}
void edges(Document& d,bool crease){
    QSet<quint64> seen;
    for(const auto& f:d.model.faces)for(int i=0;i<f.points.size();++i){auto a=f.points[i],b=f.points[(i+1)%f.points.size()];auto k=edgeKey(a,b);if(!seen.contains(k)){seen.insert(k);d.model.edges.append({EntityId(d.model.edges.size()+1),a,b,crease});}}
}
// Ear clipping preserves a planar concave polygon. Self intersections and
// zero-area ears are rejected; no convex fan that fills an absent region.
QVector<QVector<EntityId>> triangulate(const QVector<EntityId>& ids,const ModelSnapshot& m){
    if(ids.size()<3||ids.size()>maxCorners)bad("面的顶点数须为 3–4096。");
    QSet<EntityId> seen;Vec3 normal{};
    for(int i=0;i<ids.size();++i){auto id=ids[i];if(id<1||id>EntityId(m.points.size())||seen.contains(id))bad("面含重复或越界的顶点。");seen.insert(id);}
    const auto origin=m.points[ids[0]-1].position;double scale=0;
    for(int i=1;i+1<ids.size();++i){auto n=cross(sub(m.points[ids[i]-1].position,origin),sub(m.points[ids[i+1]-1].position,origin));normal.x+=n.x;normal.y+=n.y;normal.z+=n.z;}
    if(dot(normal,normal)==0)bad("面退化或自相交。");
    normal=unit(normal);
    for(auto id:ids){auto v=sub(m.points[id-1].position,origin);scale=std::max(scale,std::sqrt(dot(v,v)));}
    for(auto id:ids)if(std::abs(dot(sub(m.points[id-1].position,origin),normal))>scale*2e-6)bad("多边形不共面；请先在来源软件中三角化。");
    int drop=std::abs(normal.x)>std::abs(normal.y)?0:1;if(std::abs(normal.z)>(drop==0?std::abs(normal.x):std::abs(normal.y)))drop=2;
    QVector<std::array<double,2>> p;for(auto id:ids){auto v=sub(m.points[id-1].position,origin);p.append(drop==0?std::array<double,2>{v.y,v.z}:drop==1?std::array<double,2>{v.z,v.x}:std::array<double,2>{v.x,v.y});}
    auto area=[&](int a,int b,int c){return (p[b][0]-p[a][0])*(p[c][1]-p[a][1])-(p[b][1]-p[a][1])*(p[c][0]-p[a][0]);};
    double total=0;for(int i=0;i<p.size();++i)total+=p[i][0]*p[(i+1)%p.size()][1]-p[(i+1)%p.size()][0]*p[i][1];double sign=total>0?1:-1;
    auto on=[&](int a,int b,int c){return area(a,b,c)==0&&p[c][0]>=std::min(p[a][0],p[b][0])&&p[c][0]<=std::max(p[a][0],p[b][0])&&p[c][1]>=std::min(p[a][1],p[b][1])&&p[c][1]<=std::max(p[a][1],p[b][1]);};
    for(int i=0;i<p.size();++i)for(int j=i+2;j<p.size();++j){int b=(i+1)%p.size(),e=(j+1)%p.size();if(e==i)continue;
        double a=area(i,b,j),c=area(i,b,e),f=area(j,e,i),g=area(j,e,b);
        if(((a<0)!=(c<0)&&(f<0)!=(g<0)&&a&&c&&f&&g)||on(i,b,j)||on(i,b,e)||on(j,e,i)||on(j,e,b))bad("多边形边界自相交。");}
    QVector<int> ring;for(int i=0;i<ids.size();++i)ring.append(i);QVector<QVector<EntityId>> out;
    while(ring.size()>3){bool clipped=false;for(int i=0;i<ring.size();++i){int a=ring[(i+ring.size()-1)%ring.size()],b=ring[i],c=ring[(i+1)%ring.size()];if(sign*area(a,b,c)<=0)continue;
            bool occupied=false;for(int v:ring)if(v!=a&&v!=b&&v!=c&&sign*area(a,b,v)>=0&&sign*area(b,c,v)>=0&&sign*area(c,a,v)>=0){occupied=true;break;}
            if(occupied)continue;
            out.append({ids[a],ids[b],ids[c]});ring.removeAt(i);clipped=true;break;}
        if(!clipped)bad("多边形无法无损三角化。");}
    if(area(ring[0],ring[1],ring[2])==0)bad("多边形含退化三角形。");
    out.append({ids[ring[0]],ids[ring[1]],ids[ring[2]]});return out;
}
void finishMesh(Document& d,const Options& o){
    auto& m=d.model;m.units=o.units=="ft"?"imperial":"metric";m.precision=0;
    // Same seawater default as the editing core. Imperial density is lb/ft3,
    // while metric density is t/m3; changing only the length label is wrong.
    m.waterDensity=coreFloat(1.025);
    if(m.units=="imperial")m.waterDensity=coreFloat(m.waterDensity*1000*.3048*.3048*.3048/.45359237);
    edges(d,!o.controlNet);d.notes<<"OBJ/STL 不保存船舶主尺度、剖面线、折痕属性和背景图；保存 FBM 可保留本次导入后的工程。";
    d.validate();auto lo=m.points[0].position,hi=lo;for(const auto& p:m.points){lo.x=std::min(lo.x,p.position.x);lo.y=std::min(lo.y,p.position.y);lo.z=std::min(lo.z,p.position.z);hi.x=std::max(hi.x,p.position.x);hi.y=std::max(hi.y,p.position.y);hi.z=std::max(hi.z,p.position.z);}
    m.length=hi.x-lo.x;m.beam=o.symmetric?2*std::max(std::abs(lo.y),std::abs(hi.y)):hi.y-lo.y;m.draft=0;
}
Document obj(const QByteArray& bytes,const Options& o){
    Document d;auto& m=d.model;QString group="OBJ",object;QHash<QString,EntityId> groups;int normals=0,uvs=0;bool appearance=false;QSet<QVector<EntityId>> polygons;
    const auto lines=text(bytes).split('\n');
    for(int row=0;row<lines.size();++row){QString line=lines[row].section('#',0,0).trimmed();if(line.isEmpty())continue;auto w=words(line);QString k=w.takeFirst();
        try{
            if(k=="v"){if(w.size()!=3&&w.size()!=4)bad("顶点须有三个坐标及可选齐次分量。");double div=w.size()==4?number(w[3]):1;if(div==0)bad("齐次分量不能为零。");addPoint(d,imported({number(w[0])/div,number(w[1])/div,number(w[2])/div},o),o.controlNet?0:3);}
            else if(k=="vn"){sizeIs(w,3);for(const auto& n:w)number(n);++normals;}
            else if(k=="vt"){if(w.isEmpty()||w.size()>3)bad("纹理坐标字段数不正确。");for(const auto& n:w)number(n);++uvs;appearance=true;}
            else if(k=="g")group=w.join(' ');
            else if(k=="o")object=w.join(' ');
            else if(k=="usemtl"||k=="mtllib"||k=="s")appearance=true;
            else if(k=="f"){
                if(w.size()<3||w.size()>maxCorners)bad("面的顶点数须为 3–4096。");
                QVector<EntityId> ids;
                for(const auto& v:w){auto f=v.split('/');if(f.isEmpty()||f.size()>3)bad("无效的面索引。");int counts[]={int(m.points.size()),uvs,normals};
                    for(int i=0;i<f.size();++i){if(i>0&&f[i].isEmpty())continue;int n=integer(f[i],-counts[i],counts[i]);if(n==0)bad("OBJ 索引不能为零。");if(i==0)ids.append(n<0?counts[i]+n+1:n);}}
                QString name=object.isEmpty()?group:(group.isEmpty()||group=="OBJ"?object:object+" / "+group);if(name.isEmpty())name="OBJ";
                auto sorted=ids;std::sort(sorted.begin(),sorted.end());if(polygons.contains(sorted))bad("存在重复的面。");polygons.insert(sorted);
                if(!groups.contains(name)){if(m.layers.size()>=1000)bad("图层过多。");groups[name]=m.layers.size()+1;m.layers.append(layer(name,groups[name],o.symmetric));}
                if(o.controlNet)addFace(d,ids,groups[name]);else for(auto tri:triangulate(ids,m))addFace(d,tri,groups[name]);
            }else bad("暂不支持 OBJ 记录："+k+"。请导出多边形网格。");
        }catch(const std::exception& e){bad(QString("OBJ 第 %1 行：%2").arg(row+1).arg(QString::fromUtf8(e.what())));}
    }
    if(appearance)d.notes<<"只导入几何与分组，不读取外部材质、纹理文件或平滑组。";
    finishMesh(d,o);return d;
}
float littleFloat(const char* p){quint32 bits=qFromLittleEndian<quint32>(p);float n;std::memcpy(&n,&bits,4);if(!std::isfinite(n))bad("STL 含非有限数值。");return n;}
Document stl(const QByteArray& bytes,const Options& o){
    Document d;d.model.layers.append(layer("STL",1,o.symmetric));std::map<std::tuple<double,double,double>,EntityId> ids;
    auto vertex=[&](Vec3 p){p=imported(p,o);auto key=std::make_tuple(p.x,p.y,p.z);auto it=ids.find(key);if(it!=ids.end())return it->second;addPoint(d,p,3);return ids[key]=d.model.points.size();};
    auto facet=[&](Vec3 a,Vec3 b,Vec3 c,EntityId l){auto x=vertex(a),y=vertex(b),z=vertex(c);addFace(d,{x,y,z},l);};
    quint32 count=bytes.size()>=84?qFromLittleEndian<quint32>(bytes.constData()+80):0;
    if(bytes.size()>=84&&quint64(bytes.size())==84ULL+50ULL*count){
        if(count>maxFaces)bad("STL 三角形超过 100000 个。");
        for(quint32 i=0;i<count;++i){const char* p=bytes.constData()+84+50*i;for(int j=0;j<3;++j)littleFloat(p+4*j);Vec3 v[3];for(int j=0;j<3;++j)v[j]={littleFloat(p+12+j*12),littleFloat(p+16+j*12),littleFloat(p+20+j*12)};facet(v[0],v[1],v[2],1);}
        d.notes<<"二进制 STL 的材质/颜色扩展未导入。";
    }else{
        auto lines=text(bytes).split('\n');int pos=0;auto next=[&](){while(pos<lines.size()&&lines[pos].trimmed().isEmpty())++pos;if(pos==lines.size())bad("STL 文件不完整。");return words(lines[pos++]);};
        bool any=false;while(true){while(pos<lines.size()&&lines[pos].trimmed().isEmpty())++pos;if(pos==lines.size())break;
            auto head=next();if(head.isEmpty()||head[0]!="solid")bad("STL 头或二进制长度不正确。");QString name=head.mid(1).join(' ');EntityId l=1;
            if(!any)d.model.layers[0].name=name.isEmpty()?"STL":name;else{l=d.model.layers.size()+1;d.model.layers.append(layer(name.isEmpty()?"STL":name,l,o.symmetric));}any=true;
            while(true){auto w=next();if(w[0]=="endsolid")break;if(w.size()!=5||w[0]!="facet"||w[1]!="normal")bad("STL facet 记录不正确。");for(int j=2;j<5;++j)number(w[j]);
                if(next()!=QStringList{"outer","loop"})bad("STL 缺少 outer loop。");
                Vec3 v[3];for(int j=0;j<3;++j){auto p=next();if(p.size()!=4||p[0]!="vertex")bad("STL 顶点记录不正确。");v[j]={number(p[1]),number(p[2]),number(p[3])};}
                if(next()!=QStringList{"endloop"}||next()!=QStringList{"endfacet"})bad("STL 面未闭合。");
                facet(v[0],v[1],v[2],l);}
        }
    }
    Options flat=o;flat.controlNet=false;finishMesh(d,flat);return d;
}
Document fef(const QByteArray& bytes){
    Document d;auto& m=d.model;auto lines=text(bytes).split('\n');int i=0;
    auto next=[&](){if(i>=lines.size())bad("FEF 文件不完整。");QString s=lines[i++];if(s.endsWith('\r'))s.chop(1);return s;};
    auto row=[&](){return words(next());};auto count=[&](int max){auto r=row();sizeIs(r,1);return integer(r[0],0,max);};
    m.name=next();m.designer=next();d.creator=next();m.comment=next();auto settings=row();sizeIs(settings,8);
    m.length=number(settings[0]);m.beam=number(settings[1]);m.draft=number(settings[2]);m.waterDensity=number(settings[3]);m.appendageCoefficient=number(settings[4]);m.units=integer(settings[5],0,1)?"imperial":"metric";d.particularsSet=flag(settings[6]);m.precision=integer(settings[7],0,3);
    int n=count(1000);QSet<int> layerIds;for(int j=0;j<n;++j){Layer l=layer(next(),j+1);auto r=row();sizeIs(r,10);int id=integer(r[0],0,2147483647);if(layerIds.contains(id))bad("FEF 图层编号重复。");layerIds.insert(id);l.color=integer(r[1],0,0xffffff);l.visible=flag(r[2]);l.developable=flag(r[3]);l.symmetric=flag(r[4]);l.useForIntersections=flag(r[5]);l.useInHydrostatics=flag(r[6]);l.showInLinesplan=flag(r[7]);l.materialDensity=number(r[8]);l.thickness=number(r[9]);m.layers.append(l);}
    n=count(maxPoints);for(int j=0;j<n;++j){auto r=row();if(r.size()<3||r.size()>5)bad("FEF 顶点记录不正确。");Vec3 p{number(r[0]),number(r[1]),number(r[2])};p={coreFloat(p.x),coreFloat(p.y),coreFloat(p.z)};addPoint(d,p,r.size()>3?integer(r[3],0,3):0);if(r.size()>4)flag(r[4]);}
    n=count(300000);for(int j=0;j<n;++j){auto r=row();if(r.size()!=3&&r.size()!=4)bad("FEF 边记录不正确。");m.edges.append({EntityId(j+1),EntityId(integer(r[0],0,m.points.size()-1)+1),EntityId(integer(r[1],0,m.points.size()-1)+1),bool(integer(r[2],0,1))});if(r.size()==4)flag(r[3]);}
    n=count(maxFaces);for(int j=0;j<n;++j){auto r=row();if(r.isEmpty())bad("FEF 面记录为空。");int k=integer(r[0],3,maxCorners);if(r.size()!=k+2&&r.size()!=k+3)bad("FEF 面记录不正确。");QVector<EntityId> ids;for(int x=1;x<=k;++x)ids.append(integer(r[x],0,m.points.size()-1)+1);addFace(d,ids,integer(r[k+1],0,m.layers.size()-1)+1);if(r.size()==k+3)flag(r[k+2]);}
    while(i<lines.size())if(!next().trimmed().isEmpty())bad("FEF 末尾有未识别的数据。");
    d.validate();return d;
}
// Polycad grid contract from Plus FreeEdit.File_ImportPolycad and
// FreeSubdivisionSurface.ImportControlPointGrid (GPL-2.0-or-later).
// This imports a subdivision control net, not the original NURBS surface.
Document polycad(const QByteArray& bytes,const Options& options){
    Document d;auto& m=d.model;Options o=options;o.legacyAxes=false;o.controlNet=true;
    const auto input=text(bytes).split('\n');QString kind,name;int columns=0,rows=0,expected=-1,grids=0,total=0;
    bool pendingShape=false;QMap<QString,double> dimensions;
    std::map<std::tuple<double,double,double>,EntityId> vertices;
    const QSet<QString> types{"TBSplineSurface","TBSplineSurfaceList","TYachtLines","TShipLines"};
    const QSet<QString> parameters{
        "KnotType","KnotU","KnotV","MinUParameter","MinVParameter","MaxUParameter","MaxVParameter","OrderU","OrderV",
        "Loa","Lwl","Ovh","BMax","Bwl","BTransom","Tc","Cp","Cm","LCB","Cwp","Lcf","FreeBow","FreeMid","FreeAft",
        "BowCurve","ForeFoot","TransomAngle","FlareAngle","BowShape","Lbp","Beam","Depth","Draught","ParallelDeckF",
        "ParallelDeckA","FOSBow","FOSAft","BowRadiusAtDeck","BilgeRadius","TransomBehindAP","TransomImmersion",
        "TransomBeam","TransomCurvature","BowCurvature","BulbRadius","ForeOverHang","BulbLength","ForeFootRadius",
        "StemRadius","StRootHeight","StShaftHeight","StBossToRoot","StBossRadius","StBossStart","StFlatOfBottomStart",
        "DrawBoundaries","DrawParameters","DesignDraught"};
    auto failure=[&](int line,const QString& why){bad(QString("Polycad 第 %1 行：%2").arg(line+1).arg(why));};
    auto finishSection=[&](int line){
        if(kind.isEmpty())return;
        if(pendingShape||grids==0||(kind=="TBSplineSurfaceList"?(expected<1||grids!=expected):grids!=1))
            failure(line,"曲面数量或坐标记录不完整。");
    };
    auto clean=[](QString s){s=s.trimmed();if(s.startsWith('#')||s.startsWith(';')||s.startsWith("//"))return QString{};return s;};
    for(int i=0;i<input.size();++i){
        auto line=clean(input[i]);if(line.isEmpty())continue;
        if(line.size()>16384)failure(i,"记录过长。");
        if(line.startsWith("$POLYCADVERSION"))continue;
        if(line.startsWith('[')){
            finishSection(i);if(!line.endsWith(']'))failure(i,"段落标题不完整。");
            kind=line.mid(1,line.size()-2).trimmed();if(!types.contains(kind))failure(i,"不支持的对象类型："+kind);
            columns=rows=grids=0;expected=-1;name.clear();pendingShape=false;continue;
        }
        if(kind.isEmpty())failure(i,"文件不是支持的 Polycad 坐标网格（同名 .geo 也可能是 Gmsh 文件）。");
        const auto equal=line.indexOf('=');
        if(equal>=0){
            const auto key=line.left(equal).trimmed(),value=line.mid(equal+1).trimmed();
            if(key=="NumU"||key=="NumV"){
                if(grids&&kind!="TBSplineSurfaceList")failure(i,"单曲面段落出现重复定义。");
                if(key=="NumU")columns=integer(value,2,maxPoints);else rows=integer(value,2,maxPoints);pendingShape=true;
            }else if(key=="NumSurfaces"){
                if(kind!="TBSplineSurfaceList"||expected!=-1||grids)failure(i,"曲面总数重复或位置错误。");expected=integer(value,1,1000);
            }else if(key=="Name"){
                if(value.size()>256)failure(i,"曲面名称过长。");name=value;
            }else if(!parameters.contains(key))failure(i,"不支持的参数："+key);
            else if((kind=="TYachtLines"&&(key=="Loa"||key=="BMax"||key=="Tc"))||
                    (kind=="TShipLines"&&(key=="Lbp"||key=="Beam"||key=="Draught"))){
                const QString field=key=="Loa"||key=="Lbp"?"length":key=="BMax"||key=="Beam"?"beam":"draft";
                const double n=number(value);if(n<=0)failure(i,"主尺度必须大于零。");
                const double scaled=imported({n,0,0},o).x;
                if(dimensions.contains(field)&&dimensions[field]!=scaled)failure(i,"多个船型的主尺度相互冲突。");
                dimensions[field]=scaled;
            }else if(key=="OrderU"||key=="OrderV")integer(value,2,32);
            continue;
        }
        if(columns<2||rows<2||qint64(columns)*rows>maxPoints||qint64(columns)*rows+total>maxPoints)
            failure(i,"必须先给出有效 NumU/NumV，总坐标数不能超过 100000。");
        if(grids&&(kind!="TBSplineSurfaceList"||grids>=expected))failure(i,"出现多余坐标或曲面。");
        if(m.layers.size()>=1000)failure(i,"曲面数量超过 1000。");
        QVector<Vec3> grid;grid.reserve(columns*rows);
        while(grid.size()<columns*rows){
            if(i>=input.size())failure(i,"坐标记录被截断。");
            auto row=clean(input[i]);if(row.isEmpty()){++i;continue;}
            const auto values=words(row);if(values.size()!=3)failure(i,"每个坐标必须有 X Y Z 三个数字。");
            grid.append(imported({number(values[0]),number(values[1]),number(values[2])},o));++i;
        }
        --i;total+=grid.size();++grids;pendingShape=false;
        const EntityId layerId=m.layers.size()+1;
        m.layers.append(layer(name.isEmpty()?"Polycad "+QString::number(layerId):name,layerId,o.symmetric));name.clear();
        QVector<EntityId> ids(columns*rows);
        for(int v=0;v<rows;++v)for(int u=0;u<columns;++u){
            auto p=grid[u*rows+v];auto key=std::make_tuple(p.x,p.y,p.z);auto it=vertices.find(key);
            if(it==vertices.end()){addPoint(d,p,0);it=vertices.emplace(key,m.points.size()).first;}
            ids[v*columns+u]=it->second;
        }
        const auto before=m.faces.size();
        for(int v=1;v<rows;++v)for(int u=1;u<columns;++u){
            QVector<EntityId> face;for(auto id:{ids[(v-1)*columns+u-1],ids[(v-1)*columns+u],ids[v*columns+u],ids[v*columns+u-1]})
                if(!face.contains(id))face.append(id);
            if(face.size()>=3)addFace(d,face,layerId);
        }
        if(m.faces.size()==before)failure(i,"曲面完全退化。");
    }
    finishSection(input.size());finishMesh(d,o);
    QHash<EntityId,int> faceUses;QHash<quint64,int> edgeUses;
    for(const auto& f:m.faces){for(auto id:f.points)++faceUses[id];for(int i=0;i<f.points.size();++i)++edgeUses[edgeKey(f.points[i],f.points[(i+1)%f.points.size()])];}
    for(auto& p:m.points)p.vertexType=faceUses[p.id]<2?3:0;
    for(auto& e:m.edges)e.crease=edgeUses[edgeKey(e.a,e.b)]!=2;
    if(dimensions.contains("length"))m.length=dimensions["length"];
    if(dimensions.contains("beam"))m.beam=dimensions["beam"];
    if(dimensions.contains("draft"))m.draft=dimensions["draft"];
    d.particularsSet=dimensions.size()==3;
    d.notes={"已读取 Polycad 坐标网格作为细分控制网；不是原 B 样条/NURBS 曲面的无损转换。阶次、节点和船型生成参数不参与细分计算。"};
    // FBM stores text in the user's selected legacy code page. Keep generated
    // provenance representable there; the import explanation above is UTF-8 UI.
    m.comment="Polycad subdivision control net; original B-spline orders, knots and parametric shape are not preserved.";
    d.validate();return d;
}
#include "legacy_exchange.inc"
#include "vrml_import.inc"
QJsonArray xyz(Vec3 v){return {v.x,v.y,v.z};}
QString num(double n){if(!std::isfinite(n))bad("不能导出非有限数值。");return QString::number(n,'g',17);}
QString lineText(QString s){if(s.contains('\n')||s.contains('\r')||s.contains(QChar(0)))bad("FEF 的单行文本字段包含换行符。");return s;}
}

void Document::validate()const{
    const auto& m=model;if(m.points.isEmpty()||m.faces.isEmpty()||m.layers.isEmpty())bad("文件不包含完整的面模型。");
    if(m.points.size()>maxPoints||m.faces.size()>maxFaces)bad("导入几何数量超出限制。");
    for(double x:{m.length,m.beam,m.draft,m.waterDensity,m.appendageCoefficient})if(!std::isfinite(x)||x<0||x>1e9)bad("工程参数超出范围。");
    QHash<quint64,int> edgeUses;for(const auto& e:m.edges){if(e.a<1||e.b<1||e.a>EntityId(m.points.size())||e.b>EntityId(m.points.size())||e.a==e.b||edgeUses.contains(edgeKey(e.a,e.b)))bad("存在无效或重复的边。");edgeUses.insert(edgeKey(e.a,e.b),0);}
    for(const auto& l:m.layers)if(l.thickness<0||l.materialDensity<0)bad("材料参数不能为负。");
    QSet<QVector<EntityId>> duplicates;qint64 corners=0;
    for(const auto& f:m.faces){if(f.layer<1||f.layer>EntityId(m.layers.size())||f.points.size()<3||f.points.size()>maxCorners)bad("面的图层或顶点数不正确。");QSet<EntityId> found;
        corners+=f.points.size();if(corners*2*(qint64(1)<<(2*m.precision))>1000000)bad("按文件精度生成的曲面超过 100 万三角形。请在来源软件中拆分模型。");
        for(auto id:f.points)if(id<1||id>EntityId(m.points.size())||found.contains(id))bad("面含越界或重复顶点。");else found.insert(id);
        auto sorted=f.points;std::sort(sorted.begin(),sorted.end());if(duplicates.contains(sorted))bad("存在重复的面。");duplicates.insert(sorted);
        // Control polygons may be nonplanar (a valid subdivision input). Check
        // nonzero area without forcing them into a planar mesh representation.
        const auto a=m.points[f.points[0]-1].position;bool area=false;
        for(int i=1;i+1<f.points.size();++i){auto n=cross(sub(m.points[f.points[i]-1].position,a),sub(m.points[f.points[i+1]-1].position,a));area|=dot(n,n)>0;}
        if(!area)bad("面在原计算精度下退化。");
        for(int i=0;i<f.points.size();++i){auto key=edgeKey(f.points[i],f.points[(i+1)%f.points.size()]);if(!edgeUses.contains(key))bad("面引用了不存在的边。");if(++edgeUses[key]>2)bad("同一条边连接了两个以上的面，请先修复非流形网格。");}
    }
    QSet<quint64> assigned;
    if(controlCurves.size()>4096||markers.size()>4096||stations.size()>10000)bad("导入附属数据数量超限。");
    for(const auto& curve:controlCurves){if(curve.size()<2||curve.size()>100000)bad("控制曲线点数不正确。");
        QSet<EntityId> visited;for(int i=0;i<curve.size();++i){const auto id=curve[i];if(id<1||id>EntityId(m.points.size())||visited.contains(id))bad("控制曲线引用无效或重复控制点。");visited.insert(id);
            if(i){const auto key=edgeKey(curve[i-1],id);if(!edgeUses.contains(key)||assigned.contains(key))bad("控制曲线引用了缺失或重复分配的边。");assigned.insert(key);}}
    }
    int markerPoints=0;for(const auto& marker:markers){markerPoints+=marker.size();if(marker.size()<2||markerPoints>100000)bad("标记点数量超限。");for(auto p:marker)for(double v:{p.x,p.y,p.z})if(!std::isfinite(v)||std::abs(v)>1e9)bad("标记坐标无效。");}
    for(double station:stations)if(!std::isfinite(station)||std::abs(station)>1e9)bad("站位无效。");
}
QJsonObject Document::bridgeData()const{
    validate();const auto& m=model;QJsonArray points,edges,faces,layers;
    for(const auto& p:m.points)points.append(QJsonObject{{"p",xyz(p.position)},{"vertex_type",p.vertexType},{"locked",p.locked}});
    for(const auto& e:m.edges)edges.append(QJsonArray{double(e.a-1),double(e.b-1),e.crease});
    for(const auto& f:m.faces){QJsonArray ids;for(auto id:f.points)ids.append(double(id-1));faces.append(QJsonObject{{"points",ids},{"layer",double(f.layer-1)}});}
    for(const auto& l:m.layers)layers.append(QJsonObject{{"name",l.name},{"color",double(l.color)},{"visible",l.visible},{"symmetric",l.symmetric},{"developable",l.developable},{"use_in_hydrostatics",l.useInHydrostatics},{"use_for_intersections",l.useForIntersections},{"show_in_linesplan",l.showInLinesplan},{"alpha_blend",l.alphaBlend},{"thickness",l.thickness},{"material_density",l.materialDensity}});
    QJsonObject project{{"name",m.name},{"designer",m.designer},{"comment",m.comment},{"units",m.units},{"length",m.length},{"beam",m.beam},{"draft",m.draft},{"water_density",m.waterDensity},{"appendage_coefficient",m.appendageCoefficient},{"precision",m.precision}};
    QJsonObject result{{"format","FreeShipExchange"},{"version",1},{"project",project},{"creator",creator},{"particulars_set",particularsSet},{"points",points},{"edges",edges},{"faces",faces},{"layers",layers}};
    if(!controlCurves.isEmpty()){QJsonArray curves;for(const auto& curve:controlCurves){QJsonArray ids;for(auto id:curve)ids.append(double(id-1));curves.append(ids);}result["control_curves"]=curves;}
    if(!markers.isEmpty()){QJsonArray paths;for(const auto& marker:markers){QJsonArray path;for(auto p:marker)path.append(xyz(p));paths.append(path);}result["markers"]=paths;}
    if(!stations.isEmpty()){QJsonArray positions;for(double v:stations)positions.append(v);result["stations"]=positions;}
    return result;
}
Document read(const QString& path,const QString& format,const Options& options){
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()<1||file.size()>maxBytes)bad("无法读取文件，或文件大于 128 MiB。");auto bytes=file.readAll();if(file.error()!=QFile::NoError)bad("文件读取不完整。");
    Document d;if(format=="obj")d=obj(bytes,options);else if(format=="stl")d=stl(bytes,options);else if(format=="fef")d=fef(bytes);else if(format=="polycad")d=polycad(bytes,options);else if(format=="part")d=part(bytes,options);else if(format=="vrml")d=vrml(bytes,options);else if(format=="waves")d=waves(bytes);
    else if(format=="chines"||format=="carene"||format=="hull"||format=="surface")d=readCurveFile(bytes,format,options);else bad("不支持的导入格式。");return d;
}

void write(const QString& path,const QString& format,const ModelSnapshot& m,const Options& o){
    if(format!="obj"&&format!="stl"&&format!="fef"&&format!="part")bad("不支持的导出格式。");
    QSaveFile file(path);file.setDirectWriteFallback(false);if(!file.open(QIODevice::WriteOnly))bad("无法创建目标文件："+file.errorString());
    QTextStream s(&file);s.setEncoding(QStringConverter::Utf8);s.setLocale(QLocale::c());
    if(format=="part"){writePart(file,m,o);}
    else if(format=="fef"){
        QHash<EntityId,int> pts,ls;for(int i=0;i<m.points.size();++i)pts[m.points[i].id]=i;for(int i=0;i<m.layers.size();++i)ls[m.layers[i].id]=i;
        s<<lineText(m.name)<<'\n'<<lineText(m.designer)<<'\n'<<lineText(m.createdBy)<<'\n'<<lineText(m.comment)<<'\n';
        s<<num(m.length)<<' '<<num(m.beam)<<' '<<num(m.draft)<<' '<<num(m.waterDensity)<<' '<<num(m.appendageCoefficient)<<' '<<(m.units=="imperial"?1:0)<<' '<<(m.particularsSet?-1:0)<<' '<<m.precision<<'\n'<<m.layers.size()<<'\n';
        for(int i=0;i<m.layers.size();++i){const auto& l=m.layers[i];s<<lineText(l.name)<<'\n'<<i+1<<' '<<l.color;for(bool b:{l.visible,l.developable,l.symmetric,l.useForIntersections,l.useInHydrostatics,l.showInLinesplan})s<<' '<<(b?-1:0);s<<' '<<num(l.materialDensity)<<' '<<num(l.thickness)<<'\n';}
        s<<m.points.size()<<'\n';for(const auto& p:m.points)s<<num(p.position.x)<<' '<<num(p.position.y)<<' '<<num(p.position.z)<<' '<<p.vertexType<<" 0\n";
        s<<m.edges.size()<<'\n';for(const auto& e:m.edges)s<<pts[e.a]<<' '<<pts[e.b]<<' '<<int(e.crease)<<" 0\n";
        s<<m.faces.size()<<'\n';for(const auto& f:m.faces){s<<f.points.size();for(auto p:f.points)s<<' '<<pts[p];s<<' '<<ls[f.layer]<<" 0\n";}
    }else{
        double scale=(m.units=="imperial"?.3048:1)/metres(o.units);auto transform=[&](Vec3 p){p={p.x*scale,p.y*scale,p.z*scale};return o.legacyAxes?Vec3{p.y,p.z,p.x}:p;};
        QHash<EntityId,const Layer*> ls;for(const auto& l:m.layers)ls[l.id]=&l;
        auto selected=[&](EntityId l){return ls.contains(l)&&(o.includeHidden||ls[l]->visible);};
        auto emitTriangles=[&](auto consume){for(const auto& t:m.triangles)if(selected(t.layer)){auto a=t.vertices[0],b=t.vertices[1],c=t.vertices[2];consume(t.layer,transform(a),transform(b),transform(c));if(o.fullHull&&ls[t.layer]->symmetric&&(a.y!=0||b.y!=0||c.y!=0))consume(t.layer,transform({c.x,-c.y,c.z}),transform({b.x,-b.y,b.z}),transform({a.x,-a.y,a.z}));}};
        if(format=="obj"){
            s<<"# FreeShip_26 units="<<o.units<<" axes="<<(o.legacyAxes?"yzx":"xyz")<<'\n';
            QHash<EntityId,Vec3> points;for(const auto& p:m.points)points[p.id]=p.position;
            std::map<std::tuple<EntityId,EntityId,bool>,quint64> controlIds;std::map<std::tuple<EntityId,double,double,double>,quint64> meshIds;quint64 next=1,lastLayer=0;int faceCount=0;
            auto group=[&](EntityId l){if(lastLayer==l)return;lastLayer=l;QString name=ls[l]->name;name.replace('\n',' ');name.replace('\r',' ');name.replace('#','_');s<<"g "<<name<<'\n';};
            auto vertex=[&](Vec3 v){s<<"v "<<num(v.x)<<' '<<num(v.y)<<' '<<num(v.z)<<'\n';return next++;};
            auto meshVertex=[&](EntityId l,Vec3 p){auto key=std::make_tuple(l,p.x,p.y,p.z);auto it=meshIds.find(key);if(it!=meshIds.end())return it->second;return meshIds[key]=vertex(p);};
            if(o.controlNet){for(const auto& f:m.faces)if(selected(f.layer)){group(f.layer);bool off=false;for(auto id:f.points)off|=points[id].y!=0;
                for(int side=0;side<(o.fullHull&&ls[f.layer]->symmetric&&off?2:1);++side){QVector<quint64> indices;auto ids=f.points;if(side)std::reverse(ids.begin(),ids.end());for(auto id:ids){auto p=points[id];bool reflected=side&&p.y!=0;auto key=std::make_tuple(EntityId(0),id,reflected);auto it=controlIds.find(key);quint64 idx;if(it!=controlIds.end())idx=it->second;else{if(reflected)p.y=-p.y;idx=controlIds[key]=vertex(transform(p));}indices.append(idx);}s<<'f';for(auto id:indices)s<<' '<<id;s<<'\n';++faceCount;}}}
            else emitTriangles([&](EntityId l,Vec3 a,Vec3 b,Vec3 c){group(l);auto ia=meshVertex(l,a),ib=meshVertex(l,b),ic=meshVertex(l,c);s<<"f "<<ia<<' '<<ib<<' '<<ic<<'\n';++faceCount;});
            if(!faceCount)bad("所选范围没有可导出的面。");
        }else{
            quint64 count=0;emitTriangles([&](EntityId,Vec3,Vec3,Vec3){++count;});if(!count||count>0xffffffffULL)bad("没有三角形，或数量超出 STL 范围。");
            if(o.binaryStl){s.flush();QByteArray header(84,0);auto label=("FreeShip_26 units="+o.units+" axes="+(o.legacyAxes?"yzx":"xyz")).toLatin1();std::memcpy(header.data(),label.constData(),std::min(qsizetype(80),label.size()));qToLittleEndian<quint32>(quint32(count),header.data()+80);if(file.write(header)!=header.size())bad("STL 写入失败。");
                emitTriangles([&](EntityId,Vec3 a,Vec3 b,Vec3 c){auto n=unit(cross(sub(b,a),sub(c,a)));QByteArray record(50,0);int offset=0;for(auto v:{n,a,b,c})for(double value:{v.x,v.y,v.z}){float single=float(value);if(!std::isfinite(single))bad("坐标超出二进制 STL 范围。");quint32 bits;std::memcpy(&bits,&single,4);qToLittleEndian<quint32>(bits,record.data()+offset);offset+=4;}if(file.write(record)!=50)bad("STL 写入失败。");});
            }else{s<<"solid FreeShip_26\n";emitTriangles([&](EntityId,Vec3 a,Vec3 b,Vec3 c){auto n=unit(cross(sub(b,a),sub(c,a)));s<<"  facet normal "<<num(n.x)<<' '<<num(n.y)<<' '<<num(n.z)<<"\n    outer loop\n";for(auto p:{a,b,c})s<<"      vertex "<<num(p.x)<<' '<<num(p.y)<<' '<<num(p.z)<<'\n';s<<"    endloop\n  endfacet\n";});s<<"endsolid FreeShip_26\n";}
        }
    }
    s.flush();if(s.status()!=QTextStream::Ok||file.error()!=QFile::NoError||!file.commit())bad("写入未完成，原文件未被替换。");
}
}
