#pragma once
#include <QColor>
#include <QPainterPath>
#include <QVector>
#include <QRectF>
#include <QString>

struct Primitive {
    quint32 kind=0, color=0;
    QVector<QPointF> points;
    quint32 pixels=0;
    QString text;
};
struct Batch { QPainterPath path; QRectF bounds; QColor color; bool fill=false; int primitives=0; };
class Scene {
public:
    QRectF bounds;
    QVector<Primitive> primitives;
    QVector<Batch> batches;
    QVector<int> labels;
    QByteArray digest;
    int triangles=0, lineVertices=0;
    double loadMs=0, compileMs=0;
    // Validation-only consumers need the same parser/hash, not painter paths.
    static Scene load(const QString &filename,bool compilePaths=true);
    void save(const QString &filename) const;
    void compile();
};
QColor legacyColor(quint32 color);
