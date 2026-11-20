// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model_snapshot.h"
#include <QSet>
#include <manifold/common.h>

namespace fs::printing {
struct Options {
    QSet<EntityId> layers;
    double lengthMm=250, thicknessMm=1.2;
    bool closeStemSeam=false;
};
struct Result {
    // The common STL writer takes metres in a metric snapshot.
    ModelSnapshot mesh;
    double lengthMm=0, beamMm=0, heightMm=0, seamMm=0;
    int retessellatedEnds=0;
};
struct Problem { QString reason, location; bool stemSeam=false; };
Result build(const ModelSnapshot& source,const Options&,manifold::ExecutionContext&);
}
