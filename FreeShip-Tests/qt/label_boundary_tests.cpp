#include "label_layout.h"
#include <QGuiApplication>
#include <QFile>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>
using namespace fs::presentation;
int main(int argc,char** argv){
 QGuiApplication app(argc,argv);QString out=app.arguments().at(1);QDir().mkpath(out);QJsonArray cases,observations;bool failed=false;
 auto check=[&](QString id,bool ok){cases.append(QJsonObject{{"id",id},{"status",ok?"pass":"fail"}});failed|=!ok;};
 for(auto kind:{LabelKind::VerticalTick,LabelKind::HorizontalTick})for(int side=0;side<4;++side){
  QString key=QString::number(int(kind))+":"+QString::number(side);QSizeF size(40,16);QPointF preferred(180,142);
  if(side==0)preferred.setX(1);if(side==1)preferred.setX(359);if(side==2)preferred.setY(-4);if(side==3)preferred.setY(288);
  LabelLayout l;l.begin({0,0,400,300},0,false);auto p=l.place(key,"6510",preferred,preferred,size,kind);
  auto shift=p.position-preferred;double outward=side==0?-shift.x():side==1?shift.x():side==2?-shift.y():shift.y();
  check("all-grid-kinds-attracted-"+key,outward>8);
  if(side>=2)check("short-number-can-be-pulled-fully-offscreen-"+key,!p.visible);
  check("clip-is-viewport-intersection-"+key,p.clip==QRectF(p.position,size).intersected({0,0,400,300}));
  // Top/bottom numbers can be fully outside the canvas: retaining simulation
  // state must not force them back inside on the following animation frame.
  auto before=p.position;l.begin({0,0,400,300},16,true);auto next=l.place(key,"6510",preferred,preferred,size,kind);
  check("no-edge-snap-"+key,QLineF(before,next.position).length()<.01);
  bool smooth=true;QPointF last=next.position;LabelPlacement lastPlacement=next;
  QImage frames(800,300,QImage::Format_ARGB32);frames.fill(Qt::white);QPainter painter(&frames);
  for(int f=0;f<80;++f){
   double d=(side<2?1:-4)+f*.5;QPointF at(180,142);if(side==0)at.setX(d);if(side==1)at.setX(360-d);if(side==2)at.setY(d);if(side==3)at.setY(284-d);
   l.begin({0,0,400,300},32+f*16,true);auto placed=l.place(key,"6510",at,at,size,kind);
   smooth&=QLineF(last,placed.position).length()<3;last=placed.position;lastPlacement=placed;
   if(f%10==0){int x=f/10*100;painter.save();painter.translate(x,0);painter.setClipRect(0,0,100,300);painter.drawRect(0,0,99,299);painter.setPen(QColor(49,93,85));painter.drawText(QPointF(10,20),QString::number(f));
    // Crop a strip at the affected edge, preserving clipping in both directions.
    painter.translate(side==1?-300:side==0?0:-150,0);painter.setClipRect(QRectF(0,0,400,300),Qt::IntersectClip);painter.drawText(placed.position+QPointF(0,12),"6510");painter.restore();}
  }
  painter.end();frames.save(out+"/edge-"+key.replace(':','-')+".png");
  check("animated-return-is-continuous-"+key,smooth);
  check("return-restores-complete-number-"+key,lastPlacement.visible&&lastPlacement.clip.size()==size);
  observations.append(QJsonObject{{"kind",int(kind)},{"side",side},{"outward_px",outward},{"initially_visible",p.visible}});
 }
 QJsonObject r{{"status",failed?"fail":"pass"},{"cases",cases},{"observations",observations},{"scope","Production label layout, synthetic four-edge geometry; not desktop pan acceptance"}};QFile file(out+"/report.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(r).toJson());return failed?1:0;
}
