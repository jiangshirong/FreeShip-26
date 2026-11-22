// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "auxiliary_geometry.h"
#include "view_overlay.h"
#include <QColor>
#include <QHash>
#include <QTimer>
#include <map>
#include <memory>
#include <optional>

namespace fs::curveEdit {
using PointPositions=QHash<EntityId,Vec3>;

struct Key {
    // 0 is a control curve, -1 a flowline, and positive values are section kinds.
    int kind;
    EntityId id;
    bool operator<(const Key& other) const {
        return kind<other.kind||(kind==other.kind&&id<other.id);
    }
};

struct Shape {
    QVector<Vec3> path;
    QVector<presentation::Cubic> cubics;
    QColor color;
    double opacity=1;
};
using Shapes=std::map<Key,QVector<Shape>>;

bool onlyPointPositionsChanged(const ModelSnapshot& before,const ModelSnapshot& after);
bool horizontalReferencesChanged(const ModelSnapshot& before,const ModelSnapshot& after);
bool affects(const AuxiliaryCurve&,const PointPositions&);
QVector<presentation::Cubic> preview(const AuxiliaryCurve&,const ModelSnapshot&,const PointPositions&,
                                   const QVector<presentation::Cubic>& basis);

class Motion {
public:
    static constexpr int DurationMs=400;
    struct Track {Shape from,to;bool morph=false;};
    using Tracks=std::map<Key,QVector<Track>>;
    Motion();
    static std::shared_ptr<Motion> between(const ModelSnapshot& before,const ModelSnapshot& after,
        const PointPositions& pending,const std::shared_ptr<Motion>& previous,bool includeCurves=true);
    void start();
    bool active() const;
    bool contains(Key key) const;
    const Tracks& tracks()const{return tracks_;}
    double progress()const;
    std::optional<presentation::HorizontalReferences> referenceFrame(double progress)const;
    static std::optional<Track> pathTrack(QVector<Vec3> from,QVector<Vec3> to);
    static Shape sample(const Track& track,double progress);
    QMetaObject::Connection onFrame(QObject* owner,std::function<void()> repaint);
    const Shapes& frame() const;
    const Shapes& frameAt(qint64 elapsed) const;
private:
    static qint64 now();
    Tracks tracks_;
    struct ReferenceTrack {presentation::HorizontalReferences from,to;};
    std::optional<ReferenceTrack> references_;
    QTimer timer_;
    qint64 start_=-1;
    qint64 elapsed_=0;
    mutable qint64 sampled_=-1;
    mutable Shapes frame_;
};
}
