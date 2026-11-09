// Presentation-only adaptation of FreeOverlayReveal / FreeViewport.
// GPL-2.0-or-later; original FREE!ship / Plus copyright in COPYING/NOTICE.
#pragma once
#include <QColor>
#include <QHash>
#include <QLineF>
#include <QVector>
#include <QtGlobal>
namespace fs::presentation {
struct Stroke {QLineF line;QColor color;double width=1;};
class RevealGraph {
public:
    void prepare(const QVector<Stroke>& lines,QPointF center);
    QVector<Stroke> visible(double fraction)const;
    int buildCount()const{return builds_;}
private:
    struct Node {QPointF point;double distance=0;};
    struct Edge {int a,b;Stroke stroke;double length;};
    QVector<Stroke> source_;QPointF center_;QVector<Node> nodes_;QVector<Edge> edges_;
    double maximum_=0;int builds_=0;
};
class Entrance {
public:
    void restart();void finish();void pause(qint64 now);void resume(qint64 now);
    void beginFrame(qint64 now,double width);void endFrame();
    bool active()const{return phase_!=0;}
    bool preview()const{return phase_==1;}
    bool frozen()const{return paused_>=0;}
    double previewFraction()const;
    double lineFraction()const;
    bool pointVisible(double x)const;
    QColor pointColor(quint64 id,QColor base);
    double textOpacity(bool numeric)const;
    double textFraction(bool numeric)const;
private:
    int phase_=0;qint64 start_=-1,frame_=0,paused_=-1,overlayStart_=0,lastPulse_=0;
    double width_=1;QHash<quint64,qint64> pulses_;
};
}
