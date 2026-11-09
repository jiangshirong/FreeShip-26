// SPDX-License-Identifier: GPL-2.0-or-later
#include "background_geometry.h"
#include <cmath>
#include <algorithm>
namespace fs {
namespace {
bool calibration(QPointF origin,double scale,BackgroundCalibration& out){
    if(!std::isfinite(scale)||scale<1e-9||scale>1e9||!std::isfinite(origin.x())||!std::isfinite(origin.y())||std::abs(origin.x())>1e9||std::abs(origin.y())>1e9)return false;
    out={QPoint(int(std::nearbyint(origin.x())),int(std::nearbyint(origin.y()))),double(float(scale))};return out.scale>=1e-9;
}
}
int backgroundSide(QRectF r,QPointF p,double m){
    if(!r.adjusted(-m,-m,m,m).contains(p))return -1;
    const double d[]{std::abs(p.x()-r.left()),std::abs(p.x()-r.right()),std::abs(p.y()-r.top()),std::abs(p.y()-r.bottom())};
    auto at=std::min_element(std::begin(d),std::end(d));return *at<=m?int(at-d)+1:(r.contains(p)?0:-1);
}
bool moveBackground(const BackgroundImage& b,QPointF delta,double pixelsPerUnit,BackgroundCalibration& out){
    if(!std::isfinite(pixelsPerUnit)||pixelsPerUnit<=0)return false;
    return calibration(QPointF(b.origin)-delta/(pixelsPerUnit*b.scale),b.scale,out);
}
bool resizeBackground(const BackgroundImage& b,QPointF zero,double pixelsPerUnit,int side,QPointF delta,BackgroundCalibration& out){
    if(side==0)return moveBackground(b,delta,pixelsPerUnit,out);
    (void)zero;
    if(!std::isfinite(pixelsPerUnit)||pixelsPerUnit<=0)return false;
    const auto x=std::nearbyint(delta.x()/(pixelsPerUnit*b.scale)),y=std::nearbyint(delta.y()/(pixelsPerUnit*b.scale));
    auto origin=QPointF(b.origin);double scale=b.scale;
    // Retain the original W/E/N/S calibration rules and 16-pixel minimum.
    // Use the gesture start instead of accumulating rounded mouse messages.
    if(side==1){origin.rx()-=x;scale*=1-x/b.pixels.width();}
    else if(side==2)scale*=1+x/b.pixels.width();
    else if(side==3)scale*=1-y/b.pixels.height();
    else if(side==4){origin.ry()-=y;scale*=1+y/b.pixels.height();}else return false;
    if(scale*pixelsPerUnit*std::min(b.pixels.width(),b.pixels.height())<16)return false;
    return calibration(origin,scale,out);
}
bool calibrateBackground(const BackgroundImage& b,QPointF offset,QPointF desired,double& scale){
    // Original Set scale chooses the coordinate with the larger pixel offset.
    const auto x=std::nearbyint(offset.x()),y=std::nearbyint(offset.y());
    if(x==0&&y==0)return false;
    auto value=std::abs(x)>std::abs(y)?desired.x()/x:desired.y()/-y;
    BackgroundCalibration result;if(!calibration(b.origin,value,result))return false;scale=result.scale;return true;
}
}
