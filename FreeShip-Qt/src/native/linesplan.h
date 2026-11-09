/*
 * Calculation-only adaptation of current FREE!ship Plus lines-plan rules.
 * Original FREE!ship copyright 2005 Martijn van Engeland; Plus contributions
 * by Timoshenko V.F. and Mark Malakanov. See LINESPLAN-PROVENANCE.md.
 * GNU General Public License, version 2 or (at your option) any later version.
 * WITHOUT ANY WARRANTY. See ../../COPYING for the complete license.
 */
#pragma once
#include "geometry.h"

namespace fs::native_geometry {

enum class LinesplanView { Profile, AftBody, FrontBody, Plan };
enum class CurvePurpose { Display, DXF };
struct LinesplanPlacement {
    Vec3 origin{};
    float splitLocation = 0;
    bool mirrorPlan = false;
    bool hasDiagonals = false;
};
using CurvePath = std::vector<Vec3>; // Layout coordinates; Z is zero.

// Current GetValues sampling and knuckle insertion order. Its fragment field
// counts samples here, unlike intersectPlane's interval count. No simplification.
// At least two samples are required for a curve; empty/single-point input is empty.
// Sparse sampling can skip knuckles under the inherited rule; see provenance.
CurvePath samplePolyline(const Spline& spline, bool preserveAllKnuckles = false);

// One curve in one view. The caller supplies layout origin and split location.
// Display uses the existing density and body bounds; DXF uses 100 samples and
// leaves body selection to its caller, as does the original AddSpline routine.
// Neither route changes the input's sample density, points or knuckles.
// Body paths: negative then positive Y. Plan: positive, then optional negative Y.
std::vector<CurvePath> linesplanCurve(const Spline& spline, LinesplanView view,
                                    const LinesplanPlacement& placement,
                                    CurvePurpose purpose = CurvePurpose::Display);

} // namespace fs::native_geometry
