// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "scene.h"
#include <QImage>
#include <QPainter>
#include <algorithm>
#include <array>
#include <cmath>

// Screen-only opaque triangle rasterization. No GPU, mesh simplification or
// scaled bitmap reuse. Original primitive order is retained, including strokes
// between fills. Coordinates and edge intersections remain double precision.
class SoftwareScene {
    struct Triangle {std::array<QPointF,3> p;QRgb color;double longSlope=0,upperSlope=0,lowerSlope=0;};
    struct Command {bool fill;int begin=0,end=0;QColor color;QPainterPath path;QRectF bounds;};
    QVector<Triangle> triangles_;
    QVector<Command> commands_;
    QImage image_;
    QTransform lastCamera_;QSize lastSize_;qreal lastDpr_=0;
    QRgb* pixels_=nullptr;int width_=0,height_=0,stride_=0;

    void triangle(const Triangle& t,const QTransform& transform){
        // Cached points are ordered by decreasing world Y; the lines-plan
        // camera has positive X scale and negative Y scale.
        const double top=t.p[0].y()*transform.m22()+transform.dy(),middle=t.p[1].y()*transform.m22()+transform.dy(),bottom=t.p[2].y()*transform.m22()+transform.dy();
        const int width=width_,height=height_;
        if(bottom<=.5||top>height-.5||bottom<=top)return;
        // Most small projected triangles cover no scanline at fit-to-window.
        // Reject those before X transforms or edge intersection arithmetic.
        const int first=int(std::clamp(std::ceil(top-.5),0.,double(height)));
        const int end=int(std::clamp(std::ceil(bottom-.5),0.,double(height)));
        if(first==end)return;
        const double x0=t.p[0].x()*transform.m11()+transform.dx(),x1=t.p[1].x()*transform.m11()+transform.dx(),x2=t.p[2].x()*transform.m11()+transform.dx();
        if(std::max({x0,x1,x2})<=.5||std::min({x0,x1,x2})>width-.5)return;
        const int split=int(std::clamp(std::ceil(middle-.5),double(first),double(end)));
        auto half=[&](double shortX,double shortY,double shortSlope,int first,int end){
            for(int y=first;y<end;++y){
                const double row=y+.5;
                const double a=x0+(row-top)*t.longSlope,b=shortX+(row-shortY)*shortSlope;
                const int left=int(std::clamp(std::ceil(std::min(a,b)-.5),0.,double(width)));
                const int right=int(std::clamp(std::ceil(std::max(a,b)-.5),0.,double(width)));
                auto* pixels=pixels_+y*stride_;std::fill(pixels+left,pixels+right,t.color);
            }
        };
        half(x0,top,t.upperSlope,first,split);
        half(x1,middle,t.lowerSlope,split,end);
    }
public:
    explicit SoftwareScene(const Scene& scene){
        triangles_.reserve(scene.triangles);
        for(const auto& primitive:scene.primitives){
            if(primitive.kind==3)continue;
            const bool fill=primitive.kind==1;const auto color=legacyColor(primitive.color);
            if(commands_.isEmpty()||commands_.last().fill!=fill||(!fill&&commands_.last().color!=color)){
                Command c;c.fill=fill;c.color=color;c.begin=c.end=triangles_.size();commands_.append(c);
            }
            auto& command=commands_.last();
            if(fill){Triangle t{{primitive.points[0],primitive.points[1],primitive.points[2]},color.rgb()};
                const auto a=t.p[1]-t.p[0],b=t.p[2]-t.p[0];if(a.x()*b.y()==a.y()*b.x())continue;
                std::sort(t.p.begin(),t.p.end(),[](QPointF a,QPointF b){return a.y()>b.y();});
                auto slope=[](QPointF a,QPointF b){return a.y()==b.y()?0.:(b.x()-a.x())/(a.y()-b.y());};
                t.longSlope=slope(t.p[0],t.p[2]);t.upperSlope=slope(t.p[0],t.p[1]);t.lowerSlope=slope(t.p[1],t.p[2]);
                triangles_.append(t);command.end=triangles_.size();
            }else{command.path.moveTo(primitive.points[0]);for(int i=1;i<primitive.points.size();++i)command.path.lineTo(primitive.points[i]);}
        }
        for(auto& c:commands_)if(!c.fill)c.bounds=c.path.boundingRect();
    }
    const QImage& render(QSize logical,qreal dpr,const QTransform& camera){
        // Label animation and expose events do not invalidate static geometry.
        if(!image_.isNull()&&lastSize_==logical&&lastDpr_==dpr&&lastCamera_==camera)return image_;
        const QSize pixels(qRound(logical.width()*dpr),qRound(logical.height()*dpr));
        if(image_.size()!=pixels)image_=QImage(pixels,QImage::Format_RGB32);
        if(image_.isNull())return image_;
        image_.setDevicePixelRatio(dpr);image_.fill(Qt::white);
        width_=image_.width();height_=image_.height();stride_=image_.bytesPerLine()/sizeof(QRgb);pixels_=reinterpret_cast<QRgb*>(image_.bits());
        const QTransform physical(camera.m11()*dpr,0,0,camera.m22()*dpr,camera.dx()*dpr,camera.dy()*dpr);
        const auto visible=camera.inverted().mapRect(QRectF(QPointF{},logical)).adjusted(-1e-8,-1e-8,1e-8,1e-8);
        QPainter painter;
        for(const auto& c:commands_){
            if(c.fill){if(painter.isActive()){painter.end();pixels_=reinterpret_cast<QRgb*>(image_.bits());}for(int i=c.begin;i<c.end;++i)triangle(triangles_[i],physical);}
            else{if(!visible.intersects(c.bounds.adjusted(-1e-8,-1e-8,1e-8,1e-8)))continue;
                if(!painter.isActive()){painter.begin(&image_);painter.setTransform(camera);painter.setRenderHint(QPainter::Antialiasing);painter.setBrush(Qt::NoBrush);}
                QPen pen(c.color,1);pen.setCosmetic(true);painter.setPen(pen);painter.drawPath(c.path);
            }
        }
        if(painter.isActive())painter.end();lastSize_=logical;lastDpr_=dpr;lastCamera_=camera;return image_;
    }
};
