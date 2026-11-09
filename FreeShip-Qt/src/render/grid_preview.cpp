// SPDX-License-Identifier: GPL-2.0-or-later
// A read-only wire grid. Camera operations never resample geometry or touch FBM.
#include "grid_preview.h"
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace {
[[noreturn]] void bad(){throw std::runtime_error("型值预览数据无效或不完整。");}
double number(QJsonValue v,double lo,double hi){double x=v.toDouble(-1);if(!v.isDouble()||!std::isfinite(x)||x<lo||x>hi)bad();return x;}
int count(QJsonValue v,int lo,int hi){double x=number(v,lo,hi);if(x!=std::floor(x))bad();return int(x);}
}
fs::OffsetGrid fs::OffsetGrid::read(const QJsonObject& data){
    if(data["format"]!="FreeShipOffsetPreview"||data["version"]!=1)bad();
    int nx=count(data["stations"],5,81),nz=count(data["waterlines"],5,81),nh=count(data["hulls"],1,2);
    if(nx%2!=1||nz%2!=1)bad();double length=number(data["length_m"],1e-6,1e9),draft=number(data["draft_m"],1e-6,1e9),gap=number(data["separation_m"],0,1e9);
    if((nh==1&&gap!=0)||(nh==2&&gap<=0))bad();
    auto rows=data["half_breadths_m"].toArray();if(rows.size()!=nx)bad();std::vector<std::vector<double>> table;
    for(auto r:rows){if(!r.isArray())bad();auto row=r.toArray();if(row.size()!=nz)bad();std::vector<double> v;for(auto y:row)v.push_back(number(y,0,1e9));table.push_back(std::move(v));}
    OffsetGrid result;result.minimum={1e100,1e100,1e100};result.maximum={-1e100,-1e100,-1e100};
    for(int hull=0;hull<nh;++hull)for(int side:{1,-1}){
        const size_t base=result.points.size();double centre=nh==1?0:(hull-.5)*gap;
        for(int i=0;i<nx;++i)for(int j=0;j<nz;++j){Point p{length*(nx-1-i)/(nx-1),centre+side*table[i][j],draft*j/(nz-1)};
            result.points.push_back(p);for(int k=0;k<3;++k){result.minimum[k]=std::min(result.minimum[k],p[k]);result.maximum[k]=std::max(result.maximum[k],p[k]);}}
        for(int i=0;i<nx;++i){std::vector<size_t> path;for(int j=0;j<nz;++j)path.push_back(base+i*nz+j);result.stations.push_back(std::move(path));}
        for(int j=0;j<nz;++j){std::vector<size_t> path;for(int i=0;i<nx;++i)path.push_back(base+i*nz+j);result.waterlines.push_back(std::move(path));}
    }return result;
}
GridPreviewView::GridPreviewView(QWidget* parent):QWidget(parent){setMinimumSize(280,220);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);setFocusPolicy(Qt::StrongFocus);setProperty("offset_preview_view",true);}
void GridPreviewView::setGrid(const QJsonObject& data){auto next=fs::OffsetGrid::read(data);bool boundsChanged=next.minimum!=grid_.minimum||next.maximum!=grid_.maximum;grid_=std::move(next);dataHash_=QCryptographicHash::hash(QJsonDocument(data).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex();++gridRevision_;if(boundsChanged)fit();else update();}
void GridPreviewView::setProjection(int i){projection_=std::clamp(i,0,3);if(projection_==0){yaw_=0;pitch_=0;}else if(projection_==1){yaw_=90;pitch_=0;}else if(projection_==2){yaw_=0;pitch_=90;}else{yaw_=30;pitch_=20;}fit();}
void GridPreviewView::fit(){pan_={};zoom_=1;++cameraRevision_;update();}
QPointF GridPreviewView::direction(const fs::OffsetGrid::Point& p)const{const double yaw=yaw_*std::acos(-1.)/180,pitch=pitch_*std::acos(-1.)/180;double x=p[0]*std::cos(yaw)-p[1]*std::sin(yaw),depth=p[0]*std::sin(yaw)+p[1]*std::cos(yaw);return {x,depth*std::sin(pitch)-p[2]*std::cos(pitch)};}
void GridPreviewView::paintEvent(QPaintEvent*){
    QElapsedTimer timer;timer.start();QPainter p(this);p.fillRect(rect(),palette().color(QPalette::Base));p.setRenderHint(QPainter::Antialiasing);
    if(grid_.points.empty()){p.setPen(palette().color(QPalette::Text));p.drawText(rect().adjusted(18,18,-18,-18),Qt::AlignCenter|Qt::TextWordWrap,"生成后显示实际型值网格");return;}
    double xmin=1e100,xmax=-xmin,ymin=xmin,ymax=-xmin;
    for(int mask=0;mask<8;++mask){fs::OffsetGrid::Point v;for(int i=0;i<3;++i)v[i]=(mask&(1<<i))?grid_.maximum[i]:grid_.minimum[i];auto q=direction(v);xmin=std::min(xmin,q.x());xmax=std::max(xmax,q.x());ymin=std::min(ymin,q.y());ymax=std::max(ymax,q.y());}
    pixelsPerMetre_=zoom_*std::min(std::max(1,width()-90)/std::max(1e-9,xmax-xmin),std::max(1,height()-100)/std::max(1e-9,ymax-ymin));
    // Transform each retained point once, then submit complete polylines.
    const double radians=std::acos(-1.)/180,cy=std::cos(yaw_*radians),sy=std::sin(yaw_*radians),sp=std::sin(pitch_*radians),cp=std::cos(pitch_*radians);
    fs::OffsetGrid::Point centre;for(int i=0;i<3;++i)centre[i]=(grid_.minimum[i]+grid_.maximum[i])/2;
    const auto origin2D=QPointF(width()/2.,height()/2.)+pan_;std::vector<QPointF> projected;projected.reserve(grid_.points.size());
    for(auto v:grid_.points){double x=v[0]-centre[0],y=v[1]-centre[1],z=v[2]-centre[2];projected.push_back(origin2D+QPointF(x*cy-y*sy,(x*sy+y*cy)*sp-z*cp)*pixelsPerMetre_);}
    auto draw=[&](const auto& paths,QColor color){p.setPen(QPen(color,1));for(const auto& path:paths){QPolygonF polygon;polygon.reserve(path.size());for(auto i:path)polygon.append(projected[i]);p.drawPolyline(polygon);}};
    p.save();p.setClipRect(rect());draw(grid_.waterlines,QColor(113,160,141));draw(grid_.stations,QColor(44,111,155));p.restore();
    const QPointF origin(50,height()-45);const std::array<QColor,3> colors{QColor(155,65,65),QColor(57,122,76),QColor(55,100,160)};
    for(int i=0;i<3;++i){fs::OffsetGrid::Point v{};v[i]=1;auto end=origin+direction(v)*28;p.setPen(QPen(colors[i],2));p.drawLine(origin,end);p.drawText(end+QPointF(3,-3),QString(QChar('X'+i)));}
    p.setPen(palette().color(QPalette::Text));p.drawText(rect().adjusted(12,10,-12,-10),Qt::AlignTop|Qt::AlignRight,"横剖站 / 水线 · m");lastPaintMs_=timer.nsecsElapsed()/1e6;
}
void GridPreviewView::mousePressEvent(QMouseEvent* e){if(e->button()==Qt::LeftButton||e->button()==Qt::RightButton){drag_=e->button();last_=e->position();setFocus();setCursor(Qt::ClosedHandCursor);e->accept();}else QWidget::mousePressEvent(e);}
void GridPreviewView::mouseMoveEvent(QMouseEvent* e){if(drag_==Qt::NoButton)return;auto delta=e->position()-last_;last_=e->position();if(drag_==Qt::RightButton)pan_+=delta;else{yaw_=std::remainder(yaw_+delta.x()*.4,360.);pitch_=std::clamp(pitch_+delta.y()*.4,-89.9,89.9);projection_=3;if(onOrbit)onOrbit();}++cameraRevision_;update();e->accept();}
void GridPreviewView::mouseReleaseEvent(QMouseEvent* e){if(e->button()==drag_){drag_=Qt::NoButton;unsetCursor();e->accept();}}
void GridPreviewView::mouseDoubleClickEvent(QMouseEvent* e){if(e->button()==Qt::LeftButton)fit();}
void GridPreviewView::wheelEvent(QWheelEvent* e){const double delta=e->angleDelta().y()?e->angleDelta().y()/120.:e->pixelDelta().y()/40.;double next=std::clamp(zoom_*std::pow(1.15,std::clamp(delta,-8.,8.)),.05,100.);double factor=next/zoom_;auto anchor=e->position()-QPointF(width()/2.,height()/2.);pan_=anchor-(anchor-pan_)*factor;zoom_=next;++cameraRevision_;update();e->accept();}
QJsonObject GridPreviewView::evidence()const{return {{"grid_sha256",QString::fromLatin1(dataHash_)},{"grid_revision",qint64(gridRevision_)},{"camera_revision",qint64(cameraRevision_)},{"points",qint64(grid_.points.size())},{"station_paths",qint64(grid_.stations.size())},{"waterline_paths",qint64(grid_.waterlines.size())},{"zoom",zoom_},{"pan",QJsonArray{pan_.x(),pan_.y()}},{"yaw",yaw_},{"pitch",pitch_},{"last_paint_ms",lastPaintMs_}};}
