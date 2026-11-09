// GPL-2.0-or-later. File interpretation and construction adapted from Plus
// FreeEdit.File_ImportCarene/Chines/Hull/Surface and FreeShip.ImportChines.
// Original FREE!ship/Plus authorship applies; see NOTICE.md.
#include "curve_import.h"
#include "native/geometry.h"
#include <QHash>
#include <QSet>
#include <QStringDecoder>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <tuple>

namespace fs::exchange {
namespace {
using Curve=QVector<Vec3>;
[[noreturn]] void bad(const QString& why){throw std::invalid_argument(("曲线导入："+why).toUtf8().constData());}
double stored(double x){volatile float f=float(x);if(!std::isfinite(f)||std::abs(f)>1e9)bad("坐标超出范围。");return f;}
double value(QString text){bool ok=false;double n=text.trimmed().toDouble(&ok);if(!ok||!std::isfinite(n)||std::abs(n)>1e9)bad("无效数字："+text.left(40));return n;}
int count(QString text,int low,int high){bool ok=false;int n=text.trimmed().toInt(&ok);if(!ok||n<low||n>high)bad("数量或单位标识无效："+text.left(40));return n;}
double scale(const QString& u){if(u=="m"||u=="ft")return 1;if(u=="mm")return .001;if(u=="cm")return .01;if(u=="in")return .0254;bad("未知单位。");}
Vec3 point(QString line,double factor){
    auto fields=line.simplified().split(' ',Qt::SkipEmptyParts);if(fields.size()!=3)bad("每行必须有 X Y Z 三个坐标。");
    return {stored(value(fields[0])*factor),stored(value(fields[1])*factor),stored(value(fields[2])*factor)};
}
struct Reader {
    QStringList lines;int at=0;
    explicit Reader(const QByteArray& bytes){
        if(bytes.count('\n')>400000)bad("文件行数超过 400000。");
        QStringDecoder decode(QStringDecoder::Utf8);QString text=decode(bytes);
        if(decode.hasError()||text.contains(QChar(0)))bad("文件须为 UTF-8，不能含空字符。");
        lines=text.split('\n');for(auto& s:lines){if(s.endsWith('\r'))s.chop(1);if(s.size()>16384)bad("单行记录过长。");}
    }
    QString next(){if(at>=lines.size())bad("文件被截断。");return lines[at++];}
    void end(){while(at<lines.size())if(!next().trimmed().isEmpty())bad("文件末尾有多余数据。");}
};
struct Parsed {QVector<Curve> curves;QVector<double> stations;QString units="m",name,designer;int columns=8;double density=1.025;};
void appendCurve(Parsed& p,Curve& curve){
    if(curve.isEmpty())return;
    if(curve.size()<2)bad("每条曲线至少需要两个点。");
    bool distinct=false;for(int i=1;i<curve.size();++i){
        const auto a=curve[i-1],b=curve[i];
        if(a.x==b.x&&a.y==b.y&&a.z==b.z)bad("曲线中有连续重复点。");
        distinct|=a.x!=b.x||a.y!=b.y||a.z!=b.z;
    }
    if(!distinct||p.curves.size()>=256)bad("曲线退化或数量超过 256。");
    p.curves.append(std::move(curve));curve.clear();
}
Parsed parse(Reader& r,const QString& format,const Options& o){
    Parsed p;p.columns=o.columns?o.columns:8;int total=0;
    auto add=[&](Curve& curve,QString line,double factor){if(++total>100000)bad("输入坐标超过 100000 个。");curve.append(point(line,factor));};
    if(format=="hull"){
        p.units="ft";p.density=1.0;
        const int chines=count(r.next(),2,256);QVector<Curve> basic(chines),intermediate(chines);
        auto block=[&](QVector<Curve>& curves,int rows){
            for(int row=0;row<rows;++row)for(auto& curve:curves){
                const auto y=stored(stored(value(r.next()))/12),z=stored(stored(value(r.next()))/12),x=stored(stored(value(r.next()))/12);
                curve.append({x,y,z});
            }
        };
        block(basic,5);r.next(); // HUL separates the two fixed station tables by one record.
        block(intermediate,13);p.curves=o.intermediateBulkheads?intermediate:basic;p.columns=o.intermediateBulkheads?13:5;
        double maxX=p.curves[0][0].x;for(const auto& curve:p.curves)for(auto v:curve)maxX=std::max(maxX,v.x);
        for(auto& curve:p.curves)for(auto& v:curve)v.x=stored(maxX-v.x);
        // Both tables and all station offsets are inches. Preserve the old
        // final imperial project, converting directly to feet instead of first
        // converting to metres and scaling the whole project again. The old
        // 0.024 station multiplier differed from the correct point multiplier.
        for(int i=0;i<8;++i)p.stations.append(stored(maxX-stored(stored(value(r.next()))/12)));
        for(int i=0;i<9;++i)r.next(); // Rig data is not part of the hull geometry.
        p.designer=r.next().trimmed();p.name=r.next().trimmed();r.end();
    }else if(format=="carene"){
        p.name=r.next().left(18).trimmed();p.units=o.units;const auto factor=scale(o.units);
        Curve curve;bool active=false;
        while(r.at<r.lines.size()){
            auto line=r.next().trimmed();
            if(line.startsWith("LINE")){
                appendCurve(p,curve);active=true;r.next(); // Carene's line heading record.
            }else if(line.isEmpty()){appendCurve(p,curve);active=false;}
            else {if(!active)bad("坐标须在 LINE 段落中，不能跳过未知记录。");add(curve,line,factor);}
        }
        appendCurve(p,curve);
    }else{
        p.units=count(r.next(),0,1)?"ft":"m";Curve curve;
        while(r.at<r.lines.size()){
            auto line=r.next().trimmed();
            if(line.compare("EOF",Qt::CaseInsensitive)==0){appendCurve(p,curve);r.end();break;}
            if(line.isEmpty())appendCurve(p,curve);else add(curve,line,1);
        }
        appendCurve(p,curve);
    }
    if(p.curves.size()<2)bad("至少需要两条曲线。");
    // HUL tables also need the same zero-length/repeated-sample checks.
    if(format=="hull"){Parsed checked;for(auto curve:p.curves)appendCurve(checked,curve);}
    if(p.name.size()>256||p.designer.size()>256)bad("名称或设计者字段过长。");
    return p;
}
Curve sample(const Curve& curve,int n){
    std::vector<native_geometry::Vec3> points;for(auto p:curve)points.push_back({float(p.x),float(p.y),float(p.z)});
    native_geometry::Spline spline(std::move(points));Curve result;result.reserve(n);
    for(int i=0;i<n;++i){auto p=spline.value(double(i)/(n-1));result.append({stored(p.x),stored(p.y),stored(p.z)});}return result;
}
Curve fit(const Curve& samples,int& clipped){
    // Solve C[i-1] + 4*C[i] + C[i+1] = 6*S[i], with fixed endpoints.
    // The system is strictly diagonally dominant. O(n) elimination avoids the
    // old dense inverse and Single inverse coefficients; store only the final
    // control coordinates as Single, as required by the model format.
    const int n=samples.size();Curve rhs=samples;QVector<double> diagonal(n,4);diagonal[0]=diagonal[n-1]=1;
    for(int i=1;i<n-1;++i){rhs[i].x*=6;rhs[i].y*=6;rhs[i].z*=6;}
    for(int i=1;i<n-1;++i){const double f=1/diagonal[i-1];if(i>1)diagonal[i]-=f;rhs[i].x-=f*rhs[i-1].x;rhs[i].y-=f*rhs[i-1].y;rhs[i].z-=f*rhs[i-1].z;}
    for(int i=n-2;i>0;--i){rhs[i].x=(rhs[i].x-rhs[i+1].x)/diagonal[i];rhs[i].y=(rhs[i].y-rhs[i+1].y)/diagonal[i];rhs[i].z=(rhs[i].z-rhs[i+1].z)/diagonal[i];}
    for(int i=0;i<n;++i){auto& p=rhs[i];p={stored(p.x),stored(p.y),stored(p.z)};
        if(samples[i].y>=0&&p.y<0){p.y=0;++clipped;}
    }
    return rhs;
}
using Edge=std::pair<EntityId,EntityId>;
Edge edge(EntityId a,EntityId b){return std::minmax(a,b);}
struct Builder {
    Document d;std::map<std::tuple<double,double,double>,EntityId> points;
    std::map<Edge,int> uses;QSet<Edge> creases;
    EntityId add(Vec3 p){
        const auto key=std::make_tuple(p.x,p.y,p.z);auto i=points.find(key);if(i!=points.end())return i->second;
        if(d.model.points.size()>=100000)bad("控制点数量超限。");
        ControlPoint c;c.id=d.model.points.size()+1;c.position=p;d.model.points.append(c);points.emplace(key,c.id);return c.id;
    }
    EntityId layer(QString name,bool symmetric,bool developable){Layer l;l.id=d.model.layers.size()+1;l.name=name;l.color=0xb4b4b4;l.symmetric=symmetric;l.developable=developable;d.model.layers.append(l);return l.id;}
    void face(QVector<EntityId> ids,EntityId layerId){
        QVector<EntityId> unique;for(auto id:ids)if(!unique.contains(id))unique.append(id);if(unique.size()<3)return;
        for(int i=0;i<unique.size();++i)++uses[edge(unique[i],unique[(i+1)%unique.size()])];
        d.model.faces.append({EntityId(d.model.faces.size()+1),layerId,std::move(unique)});
    }
    void finish(const Parsed& p,int precision,bool symmetric,int clipped){
        auto& m=d.model;QHash<EntityId,int> incidentFaces,incidentCreases;
        for(auto f:m.faces)for(auto id:f.points)++incidentFaces[id];
        // Remove only construction points/layers with no face, then remap all
        // retained curve and face references. Source markers remain untouched.
        QHash<EntityId,EntityId> ids,layers;QVector<ControlPoint> kept;
        for(auto point:m.points)if(incidentFaces[point.id]){ids[point.id]=kept.size()+1;point.id=kept.size()+1;kept.append(point);}
        m.points=std::move(kept);QSet<EntityId> usedLayers;for(auto f:m.faces)usedLayers.insert(f.layer);
        QVector<Layer> keptLayers;for(auto layer:m.layers)if(usedLayers.contains(layer.id)){layers[layer.id]=keptLayers.size()+1;layer.id=keptLayers.size()+1;keptLayers.append(layer);}m.layers=std::move(keptLayers);
        for(auto& f:m.faces){f.layer=layers[f.layer];for(auto& id:f.points)id=ids[id];}
        for(auto& curve:d.controlCurves){for(auto& id:curve){if(!ids.contains(id))bad("控制曲线含无面点。");id=ids[id];}}
        for(auto [key,n]:uses){const bool crease=n!=2||creases.contains(key);const auto a=ids[key.first],b=ids[key.second];
            m.edges.append({EntityId(m.edges.size()+1),a,b,crease});if(crease){++incidentCreases[a];++incidentCreases[b];}}
        incidentFaces.clear();for(auto f:m.faces)for(auto id:f.points)++incidentFaces[id];
        for(auto& point:m.points){int n=incidentCreases[point.id];point.vertexType=incidentFaces[point.id]<2?3:std::min(n,3);}
        m.units=p.units=="ft"?"imperial":"metric";m.precision=precision;m.name=p.name;m.designer=p.designer;
        m.waterDensity=stored(p.density);if(p.units=="ft")m.waterDensity=stored(m.waterDensity*1000*.3048*.3048*.3048/.45359237);
        if(m.points.isEmpty())bad("曲线没有生成有效曲面。");
        auto lo=m.points[0].position,hi=lo;
        for(auto point:m.points){const auto v=point.position;lo.x=std::min(lo.x,v.x);lo.y=std::min(lo.y,v.y);lo.z=std::min(lo.z,v.z);hi.x=std::max(hi.x,v.x);hi.y=std::max(hi.y,v.y);hi.z=std::max(hi.z,v.z);}
        m.length=stored(hi.x-lo.x);m.beam=stored(symmetric?2*std::max(std::abs(lo.y),std::abs(hi.y)):hi.y-lo.y);m.draft=0;
        d.markers=p.curves;d.stations=p.stations;m.comment="Surface fitted from source curves; original samples retained as markers. Set the design draft before hydrostatics.";
        d.notes<<"已保留原始曲线标记；拟合网格不是原始点列的无损曲面交换。请在工程设置中填写设计吃水。";
        if(clipped)d.notes<<QString("沿用原半船拟合规则，将 %1 个越过中心面的拟合坐标限制到 Y=0；请对照原曲线标记检查。").arg(clipped);
        d.validate();
    }
};
}
Document readCurveFile(const QByteArray& bytes,const QString& format,const Options& options){
    Reader r(bytes);auto parsed=parse(r,format,options);Builder b;int clipped=0;
    if(format=="surface"){
        const int rows=options.rows,columns=options.columns?options.columns:std::max(3,int(parsed.curves.size()));
        if(rows<3||rows>100||columns<3||columns>100)bad("曲面网格的行数、列数须在 3–100 之间。");
        QVector<Curve> sampled;for(const auto& curve:parsed.curves)sampled.append(sample(curve,rows));
        QVector<Curve> grid;
        for(int row=0;row<rows;++row){Curve across;for(const auto& curve:sampled)across.append(curve[row]);grid.append(fit(sample(across,columns),clipped));}
        for(int column=0;column<columns;++column){Curve vertical;for(const auto& row:grid)vertical.append(row[column]);vertical=fit(vertical,clipped);for(int row=0;row<rows;++row)grid[row][column]=vertical[row];}
        const auto layer=b.layer("Surface",options.symmetric,false);QVector<EntityId> ids;
        for(const auto& row:grid)for(auto p:row)ids.append(b.add(p));
        for(int row=1;row<rows;++row)for(int col=1;col<columns;++col)b.face({ids[(row-1)*columns+col-1],ids[(row-1)*columns+col],ids[row*columns+col],ids[row*columns+col-1]},layer);
        b.finish(parsed,1,options.symmetric,clipped);
    }else{
        const int n=parsed.columns,c=parsed.curves.size();if(n<2||n>100)bad("每条折角线的拟合点数须在 2–100 之间。");
        QVector<QVector<EntityId>> ids(c);QVector<EntityId> layers;
        for(int i=0;i<c;++i){
            for(auto p:parsed.curves[i])if(p.y<0)bad("折角线输入应为 Y≥0 的半船，请先核对坐标方向。");
            for(auto p:fit(sample(parsed.curves[i],n),clipped))ids[i].append(b.add(p));
            QVector<EntityId> curve;for(auto id:ids[i])if(curve.isEmpty()||curve.back()!=id)curve.append(id);
            if(curve.size()>1)b.d.controlCurves.append(curve);
            if(i<c-1)layers.append(b.layer("Strake "+QString::number(i+1),true,true));
        }
        for(int j=1;j<n;++j)for(int i=1;i<c;++i)b.face({ids[i][j],ids[i][j-1],ids[i-1][j-1],ids[i-1][j]},layers[i-1]);
        for(const auto& curve:ids)for(int j=1;j<n;++j)if(curve[j]!=curve[j-1])b.creases.insert(edge(curve[j-1],curve[j]));
        for(int i=1;i<c;++i)b.creases.insert(edge(ids[i-1].back(),ids[i].back()));
        QVector<EntityId> boundary,centre;
        for(int i=c-1;i>0;--i)boundary.append(ids[i].back());
        for(int j=n-1;j>=0;--j)boundary.append(ids[0][j]);
        for(int i=1;i<c;++i)boundary.append(ids[i][0]);
        for(auto id:boundary){auto p=b.d.model.points[id-1].position;p.y=0;centre.append(b.add(p));}
        auto closing=b.layer("Close hull",true,false);
        for(int i=1;i<boundary.size();++i)b.face({centre[i],centre[i-1],boundary[i-1],boundary[i]},closing);
        b.finish(parsed,2,true,clipped);
    }
    return std::move(b.d);
}
}
