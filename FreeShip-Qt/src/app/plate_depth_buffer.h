// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QImage>
#include <QPointF>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <limits>

struct ProjectedVertex { QPointF screen;float depth;QVector3D view; };

// depth is reciprocal camera distance, affine across a projected triangle.
class PlateDepthBuffer {
public:
    QImage image;
    void clear(QSize size,QColor background) {
        if(image.size()!=size)image=QImage(size,QImage::Format_RGB32);
        image.fill(background);depth.fill(-std::numeric_limits<float>::infinity(),size.width()*size.height());
    }
    bool visible(QPointF pt,float z)const {
        const int x=int(std::floor(pt.x())),y=int(std::floor(pt.y()));
        return image.rect().contains(x,y)&&z+.0007f>=depth[y*image.width()+x];
    }
    void triangle(const ProjectedVertex& a,const ProjectedVertex& b,const ProjectedVertex& c,QRgb color,int coverage=16) {
        if(coverage==0)return;
        static constexpr unsigned char coverageOrder[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
        const QPointF u=b.screen-a.screen,v=c.screen-a.screen;
        const double area=u.x()*v.y()-u.y()*v.x();if(std::abs(area)<1e-9)return;
        const int left=std::max(0,int(std::floor(std::min({a.screen.x(),b.screen.x(),c.screen.x()}))));
        const int right=std::min(image.width()-1,int(std::ceil(std::max({a.screen.x(),b.screen.x(),c.screen.x()}))));
        const int top=std::max(0,int(std::floor(std::min({a.screen.y(),b.screen.y(),c.screen.y()}))));
        const int bottom=std::min(image.height()-1,int(std::ceil(std::max({a.screen.y(),b.screen.y(),c.screen.y()}))));
        const double du=v.y()/area,dv=-u.y()/area;
        for(int y=top;y<=bottom;++y){
            auto* pixels=reinterpret_cast<QRgb*>(image.scanLine(y));auto* row=depth.data()+y*image.width();
            const double x0=left+.5-a.screen.x(),y0=y+.5-a.screen.y();
            double s=(x0*v.y()-y0*v.x())/area,t=(u.x()*y0-u.y()*x0)/area;
            for(int x=left;x<=right;++x,s+=du,t+=dv){
                if(s<0||t<0||s+t>1)continue;
                if(coverage<16&&coverageOrder[y&3][x&3]>=coverage)continue;
                const float z=float(a.depth+s*(b.depth-a.depth)+t*(c.depth-a.depth));
                if(z>row[x]){row[x]=z;pixels[x]=color;}
            }
        }
    }
private:
    QVector<float> depth;
};
