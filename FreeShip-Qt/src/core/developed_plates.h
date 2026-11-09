#pragma once
#include <QJsonObject>
#include <QPolygonF>
#include <QColor>
#include <QTransform>
#include <QVector3D>
#include <array>

namespace fs {
struct PlateEdge { int a,b; double error; bool boundary; };
struct PlateCurve { int kind,dxfColor; QColor color; QPolygonF points; };
struct DevelopedPlate {
    QString name; QColor color; int dxfColor=7;
    QPolygonF points; QVector<QVector3D> original;
    QVector<PlateEdge> edges; QVector<QVector<int>> faces,loops;
    QVector<int> corners; QVector<PlateCurve> curves;
    QJsonObject metrics; bool joinedMirror=false;
    QRectF bounds; QPointF offset; double rotation=0; bool mirrored=false,visible=true;
    QTransform placement()const;
};
struct DevelopedPlates {
    QJsonObject source; QVector<DevelopedPlate> plates; int units=0; double waterline=0;
    QColor underwaterColor;bool shadeSubmerged=true;
    QString lengthUnit()const{return units==0?"m":"ft";}
    static DevelopedPlates parse(const QJsonObject& object);
    void arrange();
    QByteArray dxf(const std::array<bool,4>& sections)const;
    QByteArray text()const;
    QJsonObject json()const;
};
}
