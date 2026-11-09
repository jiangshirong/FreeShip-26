#include "view_overlay.h"
#include <QFontMetricsF>
#include <algorithm>
#include <cmath>
namespace fs::presentation {
namespace {QColor color(quint32 c){return QColor(c&255,(c>>8)&255,(c>>16)&255);}}
QString dimension(double value,bool imperial){
    float stored=float(value);
    if(!imperial)return QString::number(qint64(std::nearbyint(double(stored*1000.f))));
    auto feet=qint64(std::trunc(stored));auto inches=std::abs(stored-float(feet))*12.f;
    return (stored<0&&feet==0?QString("-"):QString())+QString::number(feet)+"' "+QString::number(inches,'f',1)+"''";
}
ViewOverlay makeOverlay(const ModelSnapshot& m,int view,bool grid,bool hydro,const QFont& font,const std::function<QPointF(Vec3)>& project,const std::array<bool,4>& kinds){
    ViewOverlay result;if(!m.overlay.present)return result;QFontMetricsF fm(font);double height=fm.height();
    auto lo=m.minimum,hi=m.maximum;if(m.bothSides||view==2){lo.y=-std::max(std::abs(lo.y),std::abs(hi.y));hi.y=-lo.y;}
    auto expand=[](double& a,double& b){auto d=(b-a)*.03;a-=d;b+=d;};expand(lo.x,hi.x);expand(lo.y,hi.y);expand(lo.z,hi.z);
    auto label=[&](QString key,QString text,QPointF a,QPointF preferred,QColor c,LabelKind kind,bool symbol=false){result.labels.append({key,text,a,preferred,c,kind,symbol});};
    auto line=[&](QString key,QString text,Vec3 a,Vec3 b,bool named,QColor c,double width=1){
        auto p=project(a),q=project(b);auto kind=std::abs(q.x()-p.x())<std::abs(q.y()-p.y())?LabelKind::VerticalTick:LabelKind::HorizontalTick;
        (named?result.wave:result.grid).append({{p,q},c,width});
        auto w=fm.horizontalAdvance(text);if(named)kind=LabelKind::Named;
        QPointF start(p.x(),p.y()-height),end(q.x()-w,q.y()-height);
        if(kind==LabelKind::VerticalTick){start={p.x(),p.y()};end={q.x(),q.y()-height};}
        if(key.startsWith("grid:2:")){
            if(view==2){start={p.x()-(a.y<0?w:0),p.y()};end={q.x()-(a.y<0?w:0),q.y()-height};}
            else if(a.y<0){start={p.x(),p.y()};end={q.x()-w,q.y()};}
        }
        if(key=="reference:center"){
            if(view==2){start={p.x()-w/2,p.y()};end={q.x()-w/2,q.y()-height};}
            else {start={p.x()-w,p.y()-height};end={q.x(),q.y()-height};}
        }
        if(key=="reference:dwl")start.setX(p.x()-w/2);
        label(key+":start",text,p,start,named?c:color(m.overlay.gridFontColor),kind);
        if(key!="reference:base"&&key!="reference:dwl")
            label(key+":end",text,q,end,named?c:color(m.overlay.gridFontColor),kind);
    };
    if(grid&&view!=3){
        for(const auto& s:m.sections){
            if(!kinds[s.kind-1])continue;
            auto a=lo,b=hi;QString key="grid:"+QString::number(s.kind)+':'+QString::number(s.index);
            if(s.kind==1&&view!=2){a.x=b.x=s.distance;line(key,dimension(s.distance,m.units=="imperial"),a,b,false,color(m.overlay.gridColor));}
            if(s.kind==2&&view!=0){for(int sign:{1,-1}){if(sign<0&&(s.distance==0||(!m.bothSides&&view!=2)))continue;a.y=b.y=sign*s.distance;line(key+':'+QString::number(sign),dimension(sign*s.distance,m.units=="imperial"),a,b,false,color(m.overlay.gridColor));}}
            if(s.kind==3&&view!=1){a.z=b.z=s.distance;line(key,dimension(s.distance,m.units=="imperial"),a,b,false,color(m.overlay.gridColor));}
        }
        if(view!=0){auto a=lo,b=hi;a.y=b.y=0;line("reference:center","中心线",a,b,true,Qt::blue,2);}
        if(view!=1){
            auto a=lo,b=hi;a.z=b.z=m.minimum.z;line("reference:base","基线 "+dimension(a.z,m.units=="imperial"),a,b,true,Qt::blue,2);
            if(m.overlay.mainParticulars){a.z=b.z=m.waterlineZ;line("reference:dwl","DWL "+dimension(a.z,m.units=="imperial"),a,b,true,Qt::blue,2);}
        }
        if(view==0||view==1){auto a=lo,b=hi;a.x=b.x=m.useDefaultSplitSectionLocation?float(m.length*.5):m.splitSectionLocation;
            auto p=project(a)+QPointF(0,30),q=project(b)-QPointF(0,20);auto text="分割剖面:"+dimension(a.x,m.units=="imperial");
            result.wave.append({{p,q},Qt::blue,1});label("reference:split:lower",text,p,p-QPointF(0,height),Qt::blue,LabelKind::Named);
            label("reference:split:upper",text,q,q-QPointF(fm.horizontalAdvance(text),height),Qt::blue,LabelKind::Named);
        }
    }
    if(hydro&&m.overlay.hydroAvailable){
        auto c=color(m.overlay.hydroColor);
        for(const auto& marker:m.overlay.markers){auto p=project(marker.position);label("hydro:"+marker.key,marker.text,p,p+QPointF(8,0),c,LabelKind::Point,true);}
        if(view==0){
            for(int i=1;i<m.overlay.areaPath.size();++i)result.wave.append({{project(m.overlay.areaPath[i-1]),project(m.overlay.areaPath[i])},c,1});
            for(int i=0;i<m.overlay.areas.size();++i){const auto& area=m.overlay.areas[i];auto p=project(area.position);auto text=QString::number(area.value,'f',2);
                label("area:"+QString::number(i),text,p,p-QPointF(area.position.x<m.splitSectionLocation?fm.horizontalAdvance(text):0,height),c,LabelKind::Point);
                result.grid.append({{p-QPointF(3,0),p+QPointF(3,0)},c,1});result.grid.append({{p-QPointF(0,3),p+QPointF(0,3)},c,1});
            }
        }
    }
    return result;
}
}
