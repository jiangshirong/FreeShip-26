// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QByteArray>
#include <QImage>
#include <QJsonObject>
#include <QPoint>
#include <QRectF>
namespace fs {
// Original FBM view numbers: body=0, profile=1, plan=2. No perspective image.
struct BackgroundImage {
    int view=1,alpha=255,tolerance=3,quality=100;
    bool visible=true,transparent=false;
    quint32 transparentColor=0;
    QPoint origin;
    double scale=1;
    QByteArray jpeg,digest;
    QImage pixels;
    static BackgroundImage fromJson(const QJsonObject&);
    static QImage decode(const QByteArray& jpeg);
    static QByteArray importFile(const QString& filename);
    void validate()const;
    QJsonObject metadata()const;
    QImage maskedPixels()const;
    QRectF targetRect(QPointF projectedZero,double pixelsPerUnit)const;
    QPointF imagePoint(QPointF screen,QPointF projectedZero,double pixelsPerUnit)const;
};
}
