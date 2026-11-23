// SPDX-License-Identifier: GPL-2.0-or-later
// Small, round-ended vector masters matching the existing StudioLight family.
#include "studio_menu_icons.h"
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

QStringList studioSupplementIconKeys(){return {"History","Tools","Wireframe","InspectControlFaces","InspectLineTypes","GaussianCurvature","Developability","Zebra",
    "BackgroundImage","CropFrame","PickColor","Blend","Tolerance","Coordinates","CoordinateX","CoordinateY","CoordinateZ","CoordinateOff","Resistance","Propeller","Person","Copy",
    "MessageQuestion","MessageInformation","MessageWarning","MessageCritical","MessageSuccess",
    "LayerVisible","LayerHidden",
    "PrintPreviewFitWidth","PrintPreviewFitPage","PrintPreviewZoomIn","PrintPreviewZoomOut",
    "PrintPreviewPrevious","PrintPreviewNext","PrintPreviewOverview","PrintPreviewSetup","PrintPreviewPrint"};}
namespace {
class MenuIcon final:public QIconEngine {
public:
    explicit MenuIcon(QString key):key_(std::move(key)){}
    QIconEngine* clone()const override{return new MenuIcon(key_);}
    QString iconName()override{return "studio-menu:"+key_;}
    QPixmap pixmap(const QSize& size,QIcon::Mode mode,QIcon::State state)override{return scaledPixmap(size,mode,state,1);}
    QPixmap scaledPixmap(const QSize& size,QIcon::Mode mode,QIcon::State state,qreal ratio)override{
        QPixmap image(size*ratio);image.setDevicePixelRatio(ratio);image.fill(Qt::transparent);QPainter painter(&image);paint(&painter,QRect(QPoint(),size),mode,state);return image;
    }
    void paint(QPainter* p,const QRect& rect,QIcon::Mode mode,QIcon::State)override{
        p->save();p->setRenderHint(QPainter::Antialiasing);const qreal side=qMin(rect.width(),rect.height());
        p->translate(QRectF(rect).center()-QPointF(side/2,side/2));p->scale(side/24,side/24);
        const QColor ink(mode==QIcon::Disabled?"#91a29b":"#315f56");p->setPen(QPen(ink,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p->setBrush(Qt::NoBrush);
        auto line=[&](qreal x,qreal y,qreal xx,qreal yy){p->drawLine(QPointF(x,y),QPointF(xx,yy));};
        if(key_.startsWith("PrintPreview")){
            const auto role=key_.mid(12);
            const auto page=[&](QRectF r){p->drawRoundedRect(r,1.4,1.4);};
            if(role=="FitWidth"){
                page({6,3,12,18});line(2,8,2,16);line(22,8,22,16);line(3.5,12,20.5,12);
                p->drawPolyline(QPolygonF{{6,9.5},{3.5,12},{6,14.5}});p->drawPolyline(QPolygonF{{18,9.5},{20.5,12},{18,14.5}});
            }else if(role=="FitPage"){
                page({7,5,10,14});p->drawPolyline(QPolygonF{{3,8},{3,2},{9,2}});p->drawPolyline(QPolygonF{{15,2},{21,2},{21,8}});
                p->drawPolyline(QPolygonF{{3,16},{3,22},{9,22}});p->drawPolyline(QPolygonF{{15,22},{21,22},{21,16}});
            }else if(role=="ZoomIn"||role=="ZoomOut"){
                p->drawEllipse(QPointF(10,10),6.8,6.8);line(15,15,21.5,21.5);line(6.5,10,13.5,10);if(role=="ZoomIn")line(10,6.5,10,13.5);
            }else if(role=="Previous"||role=="Next"){
                const bool forward=role=="Next";
                if(forward)p->drawPolyline(QPolygonF{{8,5},{15,12},{8,19}});else p->drawPolyline(QPolygonF{{16,5},{9,12},{16,19}});
            }else if(role=="Overview"){
                for(int x:{3,14})for(int y:{2,13})page({qreal(x),qreal(y),7,9});
            }else if(role=="Setup"){
                // Page + gear: configuration, rather than a crop/margin tool.
                // Leave space around the gear instead of painting an opaque
                // badge that would mismatch hover/disabled backgrounds.
                p->drawPolyline(QPolygonF{{9.5,21.5},{3,21.5},{3,2.5},{11,2.5},{14.5,6},{14.5,9}});
                p->drawPolyline(QPolygonF{{11,2.5},{11,6},{14.5,6}});
                line(6,10,10,10);line(6,13,8.5,13);
                const QPointF center(16.4,16.5);QPolygonF gear;
                for(int i=0;i<32;++i){
                    const double angle=-std::acos(-1.)/2+i*std::acos(-1.)/16;
                    const double radius=i%4==1||i%4==2?5.35:4.1;
                    gear.append(center+QPointF(std::cos(angle)*radius,std::sin(angle)*radius));
                }
                p->setPen(QPen(ink,1.45,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                p->drawPolygon(gear);p->drawEllipse(center,1.7,1.7);
            }else if(role=="Print"){
                page({6,2,12,7});page({3,8,18,10});p->fillRect(QRectF(6,14,12,8),mode==QIcon::Disabled?QColor("#f1f4f2"):QColor("#f6f9f7"));
                page({6,14,12,8});line(9,18,15,18);p->drawPoint(QPointF(17.5,11.5));
            }
        }else if(key_=="LayerVisible"||key_=="LayerHidden"){
            const bool hidden=key_=="LayerHidden";
            const QColor stroke(mode==QIcon::Disabled?"#888888":mode==QIcon::Selected?"#173b34":
                                mode==QIcon::Active?"#234b42":"#315f56");
            p->setPen(QPen(stroke,1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            // Leave a gap either side of the slash so the icon works on hover and selection backgrounds.
            if(hidden){
                QPainterPath gap;gap.addPolygon(QPolygonF{{0,1.6},{1.6,0},{24,22.4},{22.4,24}});
                QPainterPath canvas;canvas.addRect(0,0,24,24);
                p->save();p->setClipPath(canvas.subtracted(gap),Qt::IntersectClip);
            }
            QPainterPath eye;eye.moveTo(2,12);eye.cubicTo(7.5,4.5,16.5,4.5,22,12);eye.cubicTo(16.5,19.5,7.5,19.5,2,12);
            p->drawPath(eye);p->drawEllipse(QPointF(12,12),2.6,2.6);
            if(hidden){p->restore();line(3,3,21,21);}
        }else if(key_.startsWith("Message")){
            const bool warning=key_=="MessageWarning",critical=key_=="MessageCritical";
            const QColor color(mode==QIcon::Disabled?"#91a29b":warning?"#966321":critical?"#a34438":"#24675b");
            p->setPen(QPen(color,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            p->setBrush(QColor(mode==QIcon::Disabled?"#edf1ee":warning?"#fbf0dc":critical?"#f8e8e3":"#e4f0e9"));
            if(warning){
                QPainterPath triangle;triangle.moveTo(10.3,3.6);triangle.quadTo(12,.8,13.7,3.6);triangle.lineTo(22,18.2);triangle.quadTo(23.5,21,20.2,21);
                triangle.lineTo(3.8,21);triangle.quadTo(.5,21,2,18.2);triangle.closeSubpath();p->drawPath(triangle);
                line(12,8,12,13);p->drawPoint(QPointF(12,17));
            }else{
                p->drawEllipse(QRectF(2,2,20,20));p->setBrush(Qt::NoBrush);
                if(critical){line(8.5,8.5,15.5,15.5);line(15.5,8.5,8.5,15.5);}
                else if(key_=="MessageSuccess")p->drawPolyline(QPolygonF{{6.8,12},{10.4,15.5},{17.4,8.8}});
                else if(key_=="MessageQuestion"){
                    QPainterPath question;question.moveTo(8.8,8.7);question.cubicTo(8.8,4.6,16,4.6,16,8.5);
                    question.cubicTo(16,11.2,12,11,12,14);p->drawPath(question);p->drawPoint(QPointF(12,17.5));
                }else{p->drawPoint(QPointF(12,6.8));line(12,10.5,12,17.5);}
            }
        }else if(key_=="Person"){
            p->drawEllipse(QPointF(12,7),3.5,3.5);QPainterPath shoulders;shoulders.moveTo(4,21);shoulders.lineTo(4,19);
            shoulders.cubicTo(4,11.5,20,11.5,20,19);shoulders.lineTo(20,21);p->drawPath(shoulders);
        }else if(key_=="Copy"){
            p->drawRoundedRect(QRectF(8,7,12,14),2,2);QPainterPath back;back.moveTo(15,4);back.lineTo(6,4);back.quadTo(3,4,3,7);back.lineTo(3,16);p->drawPath(back);
        }else if(key_=="History"){
            QPainterPath path;path.moveTo(4,8);path.cubicTo(6,2,15,1,19,7);path.cubicTo(25,16,14,25,7,19);p->drawPath(path);
            p->drawPolyline(QPolygonF{{3,3},{3,9},{9,9}});line(12,7,12,12);line(12,12,16,14);
        }else if(key_=="Tools"){
            p->drawRoundedRect(QRectF(2.5,7.5,19,12.5),2,2);
            p->drawPolyline(QPolygonF{{8.5,7.5},{8.5,4},{15.5,4},{15.5,7.5}});
            line(2.5,12.5,9.5,12.5);line(14.5,12.5,21.5,12.5);
            p->drawRoundedRect(QRectF(9.5,11,5,4.5),.75,.75);
        }else if(key_=="Wireframe"){
            p->drawPolygon(QPolygonF{{12,2.5},{21,7.5},{21,17},{12,22},{3,17},{3,7.5}});
            p->drawPolyline(QPolygonF{{3,7.5},{12,12.5},{21,7.5}});line(12,12.5,12,22);
        }else if(key_=="InspectControlFaces"){
            const bool disabled=mode==QIcon::Disabled;
            p->setPen(QPen(disabled?ink:QColor("#355865"),1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            p->setBrush(QColor(disabled?"#d8e1dc":"#b5d7dd"));
            p->drawPolygon(QPolygonF{{2.5,9},{21.5,3.8},{21.5,15},{2.5,20.2}});
            line(11.5,6.54,11.5,17.74);line(2.5,14.6,21.5,9.4);
        }else if(key_=="InspectLineTypes"){
            const bool disabled=mode==QIcon::Disabled;
            p->setPen(QPen(disabled?ink:QColor("#14699e"),1.8,Qt::SolidLine,Qt::RoundCap));line(3,5,21,5);
            p->setPen(QPen(disabled?ink:QColor("#ac4a00"),1.8,Qt::SolidLine,Qt::RoundCap));
            for(int x:{3,10,17})line(x,12,x+4,12);
            p->setPen(QPen(disabled?ink:QColor("#00a9bc"),1.8,Qt::SolidLine,Qt::RoundCap));
            line(3,19,7,19);p->drawPoint(QPointF(10.5,19));line(14,19,18,19);p->drawPoint(QPointF(21,19));
        }else if(key_=="BackgroundImage"){
            p->drawRoundedRect(QRectF(2.5,3.5,19,17),2,2);p->drawEllipse(QPointF(8,8),1.5,1.5);
            p->drawPolyline(QPolygonF{{3,17},{9,12},{13,15},{17,10},{21,15}});
        }else if(key_=="CropFrame"){
            p->drawRect(QRectF(5,5,14,14));p->setBrush(Qt::white);
            for(const auto& point:QPolygonF{{4,4},{20,4},{4,20},{20,20}})p->drawRoundedRect(QRectF(point-QPointF(2,2),QSizeF(4,4)),.6,.6);
        }else if(key_=="PickColor"){
            p->drawPolyline(QPolygonF{{5,16},{3,21},{8,19},{18,9},{15,6},{5,16}});line(13,4,20,11);
            QPainterPath path;path.moveTo(16,6);path.cubicTo(21,0,25,4,19,9);p->drawPath(path);
        }else if(key_=="Blend"){
            p->drawEllipse(QPointF(8.5,12),6,6);p->drawEllipse(QPointF(15.5,12),6,6);
            line(12,7.5,12,16.5);
        }else if(key_=="Tolerance"){
            line(4,12,20,12);line(6,5,6,19);line(18,5,18,19);
            line(3,5,9,5);line(3,19,9,19);line(15,5,21,5);line(15,19,21,19);p->drawEllipse(QPointF(12,12),1.3,1.3);
        }else if(key_=="Coordinates"){
            p->drawPolyline(QPolygonF{{5,3},{5,19},{21,19}});line(5,19,17,7);
            p->drawPolyline(QPolygonF{{2.5,5.5},{5,3},{7.5,5.5}});p->drawPolyline(QPolygonF{{18.5,16.5},{21,19},{18.5,21.5}});
        }else if(key_=="CoordinateX"){
            line(6,5,18,19);line(18,5,6,19);
        }else if(key_=="CoordinateY"){
            p->drawPolyline(QPolygonF{{6,5},{12,12},{18,5}});line(12,12,12,19);
        }else if(key_=="CoordinateZ"){
            p->drawPolyline(QPolygonF{{6,5},{18,5},{6,19},{18,19}});
        }else if(key_=="CoordinateOff"){
            p->drawEllipse(QRectF(4,4,16,16));line(6.5,17.5,17.5,6.5);
        }else if(key_=="Resistance"){
            for(int y:{7,13,19}){QPainterPath path;path.moveTo(3,y);path.cubicTo(7,y-3,9,y+3,13,y);path.cubicTo(16,y-2,18,y+1,21,y);p->drawPath(path);}
        }else if(key_=="Propeller"){
            p->translate(12,12);p->setBrush(QColor(mode==QIcon::Disabled?"#d8e1dc":"#dcece4"));
            for(int i=0;i<3;++i){QPainterPath blade;blade.moveTo(0,-2);blade.cubicTo(-4,-4,-6,-10,-1,-10);blade.cubicTo(4,-10,4,-4,1,-2);p->drawPath(blade);p->rotate(120);}
            p->setBrush(ink);p->drawEllipse(QPointF(0,0),2.2,2.2);
        }else if(key_=="GaussianCurvature"){
            for(int y:{5,12,19}){QPainterPath path;path.moveTo(3,y);path.cubicTo(8,y-6,16,y+6,21,y);p->drawPath(path);}
        }else if(key_=="Developability"){
            p->drawPolygon(QPolygonF{{3,6},{12,3},{21,6},{21,19},{12,22},{3,19}});
            line(12,3,12,22);line(3,6,21,19);
        }else if(key_=="Zebra"){
            for(int x:{4,10,16}){QPainterPath path;path.moveTo(x+3,3);path.cubicTo(x-5,9,x+9,15,x+1,21);p->drawPath(path);}
        }
        p->restore();
    }
private:QString key_;
};
}
QIcon studioSupplementIcon(const QString& key){
    if(!studioSupplementIconKeys().contains(key))return {};
    return QIcon(new MenuIcon(key));
}
