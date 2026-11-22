// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "app/station_reference_contours.h"
#include "core/station_deviation.h"
#include "render/auxiliary_geometry.h"

template<typename Record>
void checkStationReferenceCases(Record record) {
    using namespace fs;
    using Lines = QVector<QPolygonF>;
    const auto marker = [](double x, const QPolygonF& yz, bool sharp = true) {
        ReferenceMarker result;
        for (auto p : yz) {
            result.spline.points.append({x,p.x(),p.y()});
            result.spline.knuckles.append(sharp);
        }
        return result;
    };
    auto hidden = marker(3, {{0,0},{1,2}});
    hidden.visible = false;
    auto diagonal = marker(3, {{0,0},{1,2}});
    diagonal.spline.points.back().x += 0.005;
    auto invalid = marker(3, {{0,0},{1,2}});
    invalid.spline.knuckles.clear();
    const auto partial = marker(2, {{0,0},{1,2}});
    const auto port = marker(1, {{0,0},{-1,1}});
    const auto shortRun = marker(1, {{-2,2},{-4,3}});
    const auto selected = stationReferences({partial,hidden,shortRun,diagonal,invalid,port},1000);
    record("reference.group-sort-and-filter", selected.size()==2 &&
           selected[0].xMm==1000 && selected[1].xMm==2000);
    record("reference.distinct-runs-and-negative-side", selected.size()==2 &&
           selected[0].side==ReferenceSide::Negative && selected[0].linesMm.size()==2);
    record("reference.millimetres-and-positive-side", selected.size()==2 &&
           selected[1].side==ReferenceSide::Positive &&
           selected[1].linesMm==Lines{{{0,0},{1000,2000}}});
    const auto nearPlanes = stationReferences({port,marker(1.00001, {{0,0},{1,1}})},1000);
    record("reference.preserve-nearby-distinct-stations",nearPlanes.size()==2 &&
           nearPlanes[0].side==ReferenceSide::Negative && nearPlanes[1].side==ReferenceSide::Positive);
    const auto both = stationReferences({port,marker(1, {{0,0},{1,1}})},304.8);
    record("reference.imperial-both-sides", both.size()==1 && both[0].xMm==304.8 &&
           both[0].side==ReferenceSide::Both && both[0].linesMm[0].back()==QPointF(-304.8,304.8));
    // Dense independent evaluation of the same native presentation cubic.
    const auto smooth = marker(3, {{0,0},{2,3},{4,1},{6,5}},false);
    const auto flattened = stationReferences({smooth},1000);
    QPolygonF dense;
    for (const auto& c : presentation::splineCubics(smooth.spline)) {
        for (int i=0;i<=500;++i) {
            const auto p = presentation::cubicValue(c, i/500.);
            dense.append({1000*p.y,1000*p.z});
        }
    }
    const auto curveDistance = flattened.isEmpty() ? std::nullopt :
        compareStationContours(flattened[0].linesMm,{dense},0.00005);
    record("reference.cubic-flatten-tolerance", curveDistance &&
           curveDistance->maximumMm+curveDistance->errorBoundMm<=referenceFlattenToleranceMm);
    const Lines full{{{-3,0},{1,4},{-3,8},{-3,10},{3,16}}};
    const Lines starboard{{{0,3},{1,4},{0,5}},{{0,13},{3,16}}};
    const Lines portOnly{{{-3,0},{0,3}},{{0,5},{-3,8},{-3,10},{0,13}}};
    record("reference.clip-side-no-bridges", clipStationToReferenceSide(full,ReferenceSide::Positive)==starboard);
    record("reference.clip-negative-side", clipStationToReferenceSide(full,ReferenceSide::Negative)==portOnly);
    record("reference.both-sides-preserved", clipStationToReferenceSide(full,ReferenceSide::Both)==full);
    const Lines centerline{{{0,1},{0,5},{-1,8}}};
    record("reference.keep-centerline",clipStationToReferenceSide(centerline,ReferenceSide::Positive)==
           Lines{{{0,1},{0,5}}});
    const std::atomic_bool cancel{true};
    record("reference.cancel-stops-all-phases", stationReferences({partial},1000,&cancel).isEmpty() &&
           clipStationToReferenceSide(full,ReferenceSide::Negative,&cancel).isEmpty() &&
           !compareStationContours(full,full,0.01,&cancel));
}
