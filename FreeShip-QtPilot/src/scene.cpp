#include "scene.h"
#include <QFile>
#include <QSaveFile>
#include <QDataStream>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <cmath>
#include <stdexcept>

namespace {
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
void configure(QDataStream &s){s.setByteOrder(QDataStream::LittleEndian);s.setFloatingPointPrecision(QDataStream::DoublePrecision);}
bool validCoordinate(double x){return std::isfinite(x)&&std::abs(x)<1e12;}
}
QColor legacyColor(quint32 c){return QColor(c&255,(c>>8)&255,(c>>16)&255);}
Scene Scene::load(const QString &filename){
    QElapsedTimer timer;timer.start();QFile f(filename);
    require(f.open(QIODevice::ReadOnly),"Cannot open scene");
    require(f.size()>=40&&f.size()<=256*1024*1024,"Invalid scene size");
    QByteArray data=f.readAll();require(data.size()==f.size(),"Short read");
    require(data.first(8)=="FSLP0001","Unsupported scene version");
    QDataStream s(data);configure(s);s.skipRawData(8);Scene result;
    double x0,y0,x1,y1;s>>x0>>y0>>x1>>y1;
    require(validCoordinate(x0)&&validCoordinate(y0)&&validCoordinate(x1)&&validCoordinate(y1)&&x1>x0&&y1>y0,"Invalid scene bounds");
    result.bounds=QRectF(QPointF(x0,y0),QPointF(x1,y1));
    qint64 totalPoints=0;
    while(!s.atEnd()){
        Primitive p;quint32 count;s>>p.kind>>p.color>>count;
        require(s.status()==QDataStream::Ok&&p.kind>=1&&p.kind<=3,"Invalid primitive header");
        require((p.kind==1&&count==3)||(p.kind==2&&count>=2&&count<=1000000)||(p.kind==3&&count==1),"Invalid point count");
        totalPoints+=count;require(totalPoints<=12000000&&qint64(count)*16<=data.size()-s.device()->pos(),"Truncated or excessive points");
        p.points.reserve(count);
        for(quint32 i=0;i<count;++i){double x,y;s>>x>>y;require(validCoordinate(x)&&validCoordinate(y),"Non-finite coordinate");p.points.append(QPointF(x,y));}
        if(p.kind==3){
            quint32 len;s>>p.pixels>>len;
            require(p.pixels>=1&&p.pixels<=512&&len<=65536&&len<=data.size()-s.device()->pos(),"Invalid label");
            QByteArray text(len,Qt::Uninitialized);require(s.readRawData(text.data(),len)==int(len),"Truncated label");
            p.text=QString::fromUtf8(text);require(p.text.toUtf8()==text,"Invalid UTF-8 label");
        }
        require(s.status()==QDataStream::Ok,"Truncated scene");result.primitives.append(std::move(p));
    }
    require(!result.primitives.isEmpty(),"Empty scene");
    result.digest=QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex();
    result.loadMs=timer.nsecsElapsed()/1e6;result.compile();return result;
}
void Scene::save(const QString &filename)const{
    QSaveFile f(filename);require(f.open(QIODevice::WriteOnly),"Cannot save scene");
    QDataStream s(&f);configure(s);s.writeRawData("FSLP0001",8);
    s<<bounds.left()<<bounds.top()<<bounds.right()<<bounds.bottom();
    for(const auto&p:primitives){
        s<<p.kind<<p.color<<quint32(p.points.size());for(const auto&v:p.points)s<<v.x()<<v.y();
        if(p.kind==3){auto bytes=p.text.toUtf8();s<<p.pixels<<quint32(bytes.size());s.writeRawData(bytes.data(),bytes.size());}
    }
    require(s.status()==QDataStream::Ok&&f.commit(),"Scene save failed");
}
void Scene::compile(){
    QElapsedTimer timer;timer.start();batches.clear();labels.clear();triangles=0;lineVertices=0;
    for(int i=0;i<primitives.size();++i){
        const auto&p=primitives[i];
        if(p.kind==3){labels.append(i);continue;}
        bool fill=p.kind==1;QColor color=legacyColor(p.color);
        if(batches.isEmpty()||batches.last().fill!=fill||batches.last().color!=color||batches.last().primitives>=256){
            Batch b;b.fill=fill;b.color=color;b.path.setFillRule(Qt::WindingFill);batches.append(b);
        }
        auto&b=batches.last();++b.primitives;
        b.path.moveTo(p.points[0]);
        if(fill){
            // Normalize orientation so overlapping same-color triangles add, not cancel.
            const auto a=p.points[1]-p.points[0],c=p.points[2]-p.points[0];
            bool reverse=a.x()*c.y()-a.y()*c.x()<0;
            b.path.lineTo(p.points[reverse?2:1]);b.path.lineTo(p.points[reverse?1:2]);b.path.closeSubpath();++triangles;
        }else{for(int n=1;n<p.points.size();++n)b.path.lineTo(p.points[n]);lineVertices+=p.points.size();}
    }
    for(auto&b:batches)b.bounds=b.path.boundingRect();
    compileMs=timer.nsecsElapsed()/1e6;
}
