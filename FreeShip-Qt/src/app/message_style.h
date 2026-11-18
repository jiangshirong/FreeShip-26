// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QColor>
#include <QPainter>
#include <QPolygonF>
#include <QFontMetricsF>

enum class StudioMessageRole { Explanation, Feedback, Warning };
struct StudioMessageStyle {const char* name;QColor foreground,background,border;};
inline StudioMessageStyle studioMessageStyle(StudioMessageRole role){
    switch(role){
    case StudioMessageRole::Explanation:return {"explanation",QColor("#4b625b"),QColor("#eef3f0"),QColor("#d8e2dc")};
    case StudioMessageRole::Feedback:return {"feedback",QColor("#1e6255"),QColor("#e7f3ed"),QColor("#c8e1d4")};
    case StudioMessageRole::Warning:return {"warning",QColor("#77501f"),QColor("#fbf1df"),QColor("#ead6b4")};
    }
    return studioMessageStyle(StudioMessageRole::Explanation);
}
inline void paintStudioMessageMark(QPainter& painter,StudioMessageRole role,bool working=false){
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(studioMessageStyle(role).foreground,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    if(role==StudioMessageRole::Warning){
        painter.drawPolygon(QPolygonF{{8,.5},{15.5,14.5},{.5,14.5}});
        painter.drawLine(QPointF(8,5),QPointF(8,9));painter.drawPoint(QPointF(8,11.5));
    }else{
        painter.drawEllipse(QRectF(.5,.5,15,15));
        if(role==StudioMessageRole::Feedback){
            if(working)painter.drawPolyline(QPolygonF{{8,4},{8,8},{11.5,9.5}});
            else painter.drawPolyline(QPolygonF{{4,8},{7,11},{12,5}});
        }else{painter.drawPoint(QPointF(8,4.5));painter.drawLine(QPointF(8,7.5),QPointF(8,11.5));}
    }
}
// Painter-only canvases and printed pages share the widget/report palette.
inline qreal studioMessageHeight(const QPainter& painter,const QString& text,qreal width,qreal scale=1){
    const auto body=QFontMetricsF(painter.font(),painter.device()).boundingRect(
        QRectF(0,0,qMax(1.,width-50*scale),100000),Qt::TextWordWrap,text);
    return qMax(40*scale,body.height()+20*scale);
}
inline void paintStudioMessage(QPainter& painter,const QRectF& box,const QString& text,StudioMessageRole role,qreal scale=1){
    if(text.isEmpty())return;
    painter.save();const auto style=studioMessageStyle(role);painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(style.border,scale));painter.setBrush(style.background);painter.drawRoundedRect(box,6*scale,6*scale);
    painter.save();painter.translate(box.topLeft()+QPointF(12,12)*scale);painter.scale(scale,scale);paintStudioMessageMark(painter,role);painter.restore();
    painter.setPen(style.foreground);painter.drawText(box.adjusted(38*scale,10*scale,-12*scale,-10*scale),Qt::AlignLeft|Qt::AlignVCenter|Qt::TextWordWrap,text);painter.restore();
}
