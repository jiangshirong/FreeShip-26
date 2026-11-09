// Pixel-space label placement adapted from current FreeLabelLayout.pas.
// GPL-2.0-or-later; see NOTICE.md. Text and model values belong to the caller.
#pragma once
#include <QHash>
#include <QLineF>
#include <QRectF>
#include <QString>
#include <QVector>
namespace fs::presentation {
enum class LabelKind {Point,Named,VerticalTick,HorizontalTick};
struct LabelPlacement {QPointF position;QRectF clip;bool visible=false;};
class LabelLayout {
public:
    void clear();void translate(QPointF delta);
    void begin(QRectF bounds,qint64 time,bool animate);
    void reserve(QRectF rect);void segment(QLineF line);
    bool claim(QPointF anchor,const QString& value);
    LabelPlacement place(const QString& key,const QString& text,QPointF anchor,QPointF preferred,QSizeF size,LabelKind kind,int outward=0);
    bool animating()const{return moving_;}
    static bool segmentHits(QRectF rect,QLineF line);
    int segmentCrossings(QRectF rect);
private:
    struct Entry {QPointF offset,target,velocity,edge,position,preferred;QRectF rect;QString text;QSizeF size;LabelKind kind=LabelKind::Point;int outward=0;quint64 seen=0;};
    struct NamedValue {QString value;QPointF anchor;QSizeF size;};
    QVector<NamedValue> namedValues_;
    QHash<QString,Entry> history_;QHash<QString,bool> claims_;
    QVector<QRectF> occupied_;QVector<QLineF> lines_;
    QHash<QPair<int,int>,QVector<int>> lineCells_;
    QVector<quint32> lineSeen_;quint32 query_=0;
    QRectF bounds_;quint64 frame_=0;qint64 last_=-1;double dt_=0.016;bool animate_=false,moving_=false;
};
}
