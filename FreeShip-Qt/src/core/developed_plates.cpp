#include "developed_plates.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>
#include <QSet>
#include <cmath>
#include <stdexcept>

namespace fs {
namespace {
[[noreturn]] void invalid(){throw std::runtime_error("板材展开数据无效或不完整。");}
double number(QJsonValue v){double d=v.toDouble(qQNaN());if(!std::isfinite(d)||std::abs(d)>1e15)invalid();return d;}
int index(QJsonValue v,int size){double d=number(v);if(d<0||d>=size||std::floor(d)!=d)invalid();return int(d);}
QPointF point(QJsonValue v){auto a=v.toArray();if(a.size()!=2)invalid();return {number(a[0]),number(a[1])};}
QColor color(QJsonValue v){int c=index(v,0x1000000);return {c&255,(c>>8)&255,(c>>16)&255};}
QString numeric(double d){return QString::number(d,'g',17);}
// Walk the edge graph without changing the source edges. Each closed boundary
// component (including holes and the reflected half) keeps all of its vertices.
QVector<QVector<int>> boundaries(const DevelopedPlate& p){
    QVector<QVector<int>> adjacent(p.points.size());QSet<quint64> unique;
    for(int i=0;i<p.edges.size();++i){const auto&e=p.edges[i];if(!e.boundary)continue;
        quint64 key=(quint64(qMin(e.a,e.b))<<32)|quint32(qMax(e.a,e.b));if(unique.contains(key))invalid();unique.insert(key);
        adjacent[e.a].append(i);adjacent[e.b].append(i);}
    for(const auto& a:adjacent)if(!a.isEmpty()&&a.size()!=2)invalid();
    QSet<int> used;QVector<QVector<int>> result;
    for(int i=0;i<p.edges.size();++i){if(!p.edges[i].boundary||used.contains(i))continue;
        QVector<int> loop;int start=p.edges[i].a,current=start,edge=i;
        do {loop.append(current);if(used.contains(edge))invalid();used.insert(edge);
            const auto&e=p.edges[edge];current=e.a==current?e.b:e.a;
            const auto&a=adjacent[current];edge=a[0]==edge?a[1]:a[0];
        }while(current!=start);
        if(loop.size()<3)invalid();
        result.append(loop);
    }
    if(result.isEmpty())invalid();
    return result;
}
QString layerName(QString s){for(int i=0;i<s.size();++i)if(s[i].unicode()<32||QString("<>/\\\":;?*|=,").contains(s[i]))s[i]='_';return s.left(220);}
}
QTransform DevelopedPlate::placement()const{
    auto c=bounds.center();QTransform t;t.translate(offset.x()+c.x(),offset.y()+c.y());
    t.rotate(rotation);t.scale(1,mirrored?-1:1);t.translate(-c.x(),-c.y());return t;
}
DevelopedPlates DevelopedPlates::parse(const QJsonObject& root){
    DevelopedPlates out;out.source=root;
    if(root["schema"]!=1||!root["plates"].isArray()||root["plates"].toArray().isEmpty())invalid();
    out.units=index(root["units"],2);out.waterline=number(root["waterline"]);
    out.underwaterColor=color(root["underwater_color"]);out.shadeSubmerged=root["shade_submerged"].toBool();
    qsizetype total=0;
    for(auto value:root["plates"].toArray()){
        auto o=value.toObject();DevelopedPlate p;p.name=o["name"].toString();if(p.name.isEmpty())invalid();
        p.color=color(o["color"]);p.dxfColor=index(o["dxf_color"],256);p.joinedMirror=o["joined_mirror"].toBool();
        for(auto v:o["points"].toArray())p.points.append(point(v));
        if(p.points.size()<3||(total+=p.points.size())>2000000)invalid();
        for(auto v:o["original"].toArray()){auto a=v.toArray();if(a.size()!=3)invalid();p.original.append(QVector3D(number(a[0]),number(a[1]),number(a[2])));}
        if(p.original.size()!=p.points.size())invalid();
        p.bounds=p.points.boundingRect();
        for(auto v:o["edges"].toArray()){auto a=v.toArray();if(a.size()!=4||!a[3].isBool())invalid();
            PlateEdge e{index(a[0],p.points.size()),index(a[1],p.points.size()),number(a[2]),a[3].toBool()};if(e.a==e.b)invalid();p.edges.append(e);}
        for(auto v:o["faces"].toArray()){QVector<int> face;for(auto n:v.toArray())face.append(index(n,p.points.size()));if(face.size()<3)invalid();p.faces.append(face);}
        if(p.faces.isEmpty()||p.edges.isEmpty())invalid();
        for(auto v:o["corners"].toArray())p.corners.append(index(v,p.points.size()));
        for(auto v:o["curves"].toArray()){auto c=v.toObject();PlateCurve curve;curve.kind=index(c["kind"],5);if(!curve.kind)invalid();
            curve.color=color(c["color"]);curve.dxfColor=index(c["dxf_color"],256);
            for(auto n:c["points"].toArray())curve.points.append(point(n));
            if(curve.points.size()<2)invalid();
            p.curves.append(curve);}
        for(const auto* key:{"reference_area","signed_area_error","absolute_area_error","max_face_area_error","min_edge_error","max_edge_error","normalized_error","iterations"})p.metrics[key]=number(o[key]);
        if(p.metrics["reference_area"].toDouble()<=0||p.metrics["absolute_area_error"].toDouble()<0)invalid();
        p.loops=boundaries(p);out.plates.append(p);
    }
    out.arrange();return out;
}
void DevelopedPlates::arrange(){
    double y=0;for(auto& p:plates){p.rotation=0;p.mirrored=false;p.offset={-p.bounds.left(),y-p.bounds.bottom()};
        y-=p.bounds.height()+.025*std::hypot(p.bounds.width(),p.bounds.height());}
}
QByteArray DevelopedPlates::dxf(const std::array<bool,4>& sections)const{
    QString s;QTextStream t(&s);t.setLocale(QLocale::c());
    t<<"0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1021\n9\n$INSUNITS\n70\n"<<(units==0?6:2)<<"\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n";
    auto poly=[&](const QPolygonF& pts,const QString& name,int col,bool closed){
        t<<"0\nPOLYLINE\n8\n"<<layerName(name)<<"\n62\n"<<col<<"\n66\n1\n70\n"<<(closed?1:0)<<'\n';
        for(auto pt:pts)t<<"0\nVERTEX\n8\n"<<layerName(name)<<"\n10\n"<<numeric(pt.x())<<"\n20\n"<<numeric(pt.y())<<"\n30\n0\n";
        t<<"0\nSEQEND\n";
    };
    for(const auto&p:plates)if(p.visible){auto tr=p.placement();for(const auto& loop:p.loops){QPolygonF pts;for(int i:loop)pts.append(tr.map(p.points[i]));poly(pts,p.name,p.dxfColor,true);}
        for(const auto& c:p.curves)if(sections[c.kind-1])poly(tr.map(c.points),QStringList{"stations","buttocks","waterlines","diagonals"}[c.kind-1],c.dxfColor,false);}
    t<<"0\nENDSEC\n0\nEOF\n";return s.toUtf8();
}
QByteArray DevelopedPlates::text()const{
    QString s;QTextStream t(&s);t<<"FreeShip_26 板材展开边界坐标\n单位: "<<lengthUnit()<<"\n每块板片以其边界最小 X、Y 为原点；保留全部顶点。\n";
    for(const auto&p:plates)if(p.visible){auto tr=p.placement();QPolygonF all;for(const auto&loop:p.loops)for(int i:loop)all.append(tr.map(p.points[i]));auto origin=all.boundingRect().topLeft();
        t<<"\nBoundary coordinates for: "<<p.name<<"\n";int n=0;
        for(const auto&loop:p.loops){t<<"\nBoundary "<<++n<<" (closed)\nX Y\n";for(int i:loop){auto v=tr.map(p.points[i])-origin;t<<numeric(v.x())<<' '<<numeric(v.y())<<'\n';}}
    }return s.toUtf8();
}
QJsonObject DevelopedPlates::json()const{
    auto o=source;QJsonArray layout;for(const auto&p:plates)layout.append(QJsonObject{{"name",p.name},{"x",p.offset.x()},{"y",p.offset.y()},{"rotation",p.rotation},{"mirrored",p.mirrored},{"visible",p.visible}});o["layout"]=layout;return o;
}
}
