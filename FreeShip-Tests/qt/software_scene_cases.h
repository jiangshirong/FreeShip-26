// Independent pixel-centre containment oracle for the CPU screen renderer.
#pragma once
#include "software_scene.h"
template<class Check> void softwareSceneCases(Check check){
    Scene scene;scene.bounds={-10,-10,20,20};
    auto add=[&](quint32 color,std::initializer_list<QPointF> points){Primitive p;p.kind=1;p.color=color;p.points=QVector<QPointF>(points);scene.primitives.append(p);};
    add(0x0000ff,{{-8,-7},{8,-7},{-8,7}});add(0x0000ff,{{8,-7},{8,7},{-8,7}});
    add(0x00ff00,{{-4,-9},{2,8},{7,-1}});add(0xff0000,{{-3,2},{5,2},{1,-6}});
    add(0x00ffff,{{-9,4},{9,4.0001},{8,4.0002}});
    add(0xff00ff,{{-3,-3},{0,0},{3,3}}); // zero area must never cover a pixel
    scene.compile();SoftwareScene renderer(scene);
    auto cross=[](QPointF a,QPointF b){return a.x()*b.y()-a.y()*b.x();};
    for(qreal dpr:{1.,1.5,2.})for(double zoom:{.01,3.7,19.,1e8}){
        const QSize size(137,99);const QTransform camera(zoom,0,0,-zoom,67.17,48.23);
        const auto& image=renderer.render(size,dpr,camera);bool equal=true;int tested=0;
        const auto inverse=camera.inverted();
        for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){
            const QPointF sample=inverse.map(QPointF((x+.5)/dpr,(y+.5)/dpr));QRgb expected=qRgb(255,255,255);bool boundary=false;
            for(const auto& p:scene.primitives){const double area=cross(p.points[1]-p.points[0],p.points[2]-p.points[0]);if(area==0)continue;
                double a=cross(p.points[1]-p.points[0],sample-p.points[0])/area,b=cross(p.points[2]-p.points[1],sample-p.points[1])/area,c=cross(p.points[0]-p.points[2],sample-p.points[2])/area;
                if(std::min({std::abs(a),std::abs(b),std::abs(c)})<1e-10){boundary=true;break;}
                if(a>0&&b>0&&c>0)expected=legacyColor(p.color).rgb();
            }
            if(!boundary){++tested;equal&=image.pixel(x,y)==expected;}
        }
        check(QString("software-pixel-oracle-%1-%2").arg(dpr).arg(zoom),equal&&tested>100);
    }
    const auto empty=renderer.render({83,41},1,QTransform(2,0,0,-2,10000,10000));bool white=true;
    for(int y=0;y<empty.height();++y)for(int x=0;x<empty.width();++x)white&=empty.pixel(x,y)==qRgb(255,255,255);
    check("software-pan-outside-clears-buffer",white);
    // A line drawn before a later face must be covered, not moved on top.
    Scene ordered;ordered.bounds={0,0,10,10};Primitive line;line.kind=2;line.color=0;line.points={{0,5},{10,5}};ordered.primitives.append(line);
    Primitive fill;fill.kind=1;fill.color=0xff;fill.points={{0,0},{10,0},{0,10}};ordered.primitives.append(fill);ordered.compile();SoftwareScene interleaved(ordered);
    const auto image=interleaved.render({100,100},1,QTransform(10,0,0,-10,0,100));
    check("software-preserves-fill-stroke-order",image.pixel(20,50)==qRgb(255,0,0)&&image.pixelColor(80,50).red()<255);
    Scene border;border.bounds={0,0,10,10};fill.points={{0,-9.5},{10,-9.5},{0,-20}};border.primitives.append(fill);border.compile();SoftwareScene edge(border);
    check("software-last-pixel-row",edge.render({10,10},1,QTransform(1,0,0,-1,0,0)).pixel(1,9)==qRgb(255,0,0));
}
