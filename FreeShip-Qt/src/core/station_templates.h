// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native/document.h"
#include "station_shell.h"
#include <QList>
#include <QPolygonF>
#include <QSet>
#include <atomic>
#include <memory>
#include <optional>
#include <stdexcept>

namespace fs::frames {
// All exported coordinates are millimetres. A closed outline repeats its first point.
struct Station {
    int number = 0;
    double x = 0;
    QList<QPolygonF> outlines;
    QString error;
    QList<QPair<int,QPointF>> slotLabels; // Station number and label anchor on the spine.
    QList<QPolygonF> diagnosticOutlines; // Raw station geometry available after a failed validation.
    QList<QPolygonF> shellReference; // Original outer section at the same station.
    std::optional<ShellContourProfile> shellProfile; // Isolated performance probe only.
};
struct Options {
    QSet<int> indices;          // Revision-local, zero-based station numbers.
    int layer = -1;             // -1 uses all layers participating in intersections.
    double thickness = 0;       // Zero requests the original outer lines.
    bool closeSheer = false;    // Explicit straight top for an open mould.
    bool measureShell = false;
    bool operator==(const Options& other) const {
        return indices==other.indices&&layer==other.layer&&thickness==other.thickness&&
               closeSheer==other.closeSheer&&measureShell==other.measureShell;
    }
};

// Shared by draws on one immutable model revision. Thickness and station planes
// change the cut, but subdivisions and layer filtering stay the same.
class StationGeometry {
public:
    StationGeometry(const native_geometry::DocumentInput&, int layer, bool shell);
    ~StationGeometry();
    QList<Station> cut(const std::vector<native_geometry::SectionInput>&, const Options&,
                       const std::atomic_bool& cancelled) const;
private:
    struct Prepared;
    std::unique_ptr<Prepared> prepared_;
};
QList<Station> interlockFrames(QList<Station> frames, double stock);
struct StationAssemblyError : std::runtime_error {
    StationAssemblyError(int number,const QString& reason,int adjacent=0);
    int number,adjacent;
    QString reason;
};
QList<Station> sectionTemplates(const native_geometry::DocumentInput& input, const Options& options,
                                const std::atomic_bool& cancelled);
QList<Station> sectionTemplatesAt(const native_geometry::DocumentInput& input,
                                const std::vector<native_geometry::SectionInput>& sections,
                                const Options& options,const std::atomic_bool& cancelled);
}

