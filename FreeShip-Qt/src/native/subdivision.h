/*
 * Calculation-only adaptation of current FREE!ship Plus subdivision rules.
 * Original FREE!ship copyright 2005 Martijn van Engeland <Info@FREEship.org>.
 * Plus credits Timoshenko V.F. (2006-2011) and Mark Malakanov (2015).
 * ShipCAD by Greg Green (2015) was consulted as a comparison; see
 * SUBDIVISION-PROVENANCE.md for the exact sources and retained behavior.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version. It is distributed WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See ../../COPYING for the complete license.
 */
#pragma once
#include "geometry.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace fs::native_geometry {

enum class VertexType { Regular = 0, Crease = 1, Dart = 2, Corner = 3 };
enum class SubdivisionMode { QuadTriangle = 0, CatmullClark = 1 };

struct ControlPointInput {
    Vec3 position{};
    VertexType type = VertexType::Regular;
};
struct ControlEdgeInput {
    std::array<std::size_t, 2> points{};
    bool crease = false;
    int curve = -1;
};
struct ControlFaceInput {
    std::vector<std::size_t> points;
    std::size_t layer = 0;
};
struct ControlCurveInput {
    std::vector<std::size_t> points;
    bool visible = true;
};
struct LayerInput {
    bool symmetric = true;
    bool surfaceVisible = true;
    bool controlNetVisible = true;
    bool useForIntersections = true;
    bool useInHydrostatics = true;
    bool showInLinesplan = true;
    bool developable = false;
    std::int32_t color = 0;
    unsigned alphaBlend = 255;
    float materialDensity = 0;
    float thickness = 0;
    std::string name;
    std::int32_t layerId = 0;
};
struct GeometryInput {
    SubdivisionMode subdivisionMode = SubdivisionMode::QuadTriangle;
    int precision = 0;               // Source metadata, not a second level selector.
    unsigned desiredLevel = 0;
    std::vector<ControlPointInput> points;
    std::vector<ControlEdgeInput> edges; // Complete unique control edges, in source order.
    std::vector<ControlFaceInput> faces;
    std::vector<LayerInput> layers;
    std::vector<ControlCurveInput> curves;
};

struct SubdivisionPoint {
    Vec3 position{};
    VertexType type = VertexType::Regular;
};
struct SubdivisionFace {
    std::vector<std::size_t> points;
    std::size_t rootControlFace = 0;
    int childIndex = -1;             // -1 for the original control face at level 0.
    std::size_t layer = 0;
};
struct SubdivisionEdge {
    std::array<std::size_t, 2> points{}; // Ascending canonical point IDs.
    bool crease = false;
    int curve = -1;
    bool controlDescendant = false;
    std::vector<std::size_t> faces;
    bool reversed = false; // Source construction direction relative to canonical IDs.
};
struct SubdivisionCurve {
    std::vector<std::size_t> points;
    bool visible = true;
    bool freeStanding = false;
    std::vector<bool> knuckles;
};
struct LooseControlPoint {
    std::size_t controlPoint = 0;
    Vec3 position{};
    VertexType type = VertexType::Regular;
};
struct LooseControlEdge {
    std::size_t controlEdge = 0;
    std::array<std::size_t, 2> points{}; // Original control-point IDs.
    bool crease = false;
    int curve = -1;
};
struct SubdivisionLevel {
    unsigned level = 0;
    // Level 0 preserves all input points and IDs. Higher levels use the first
    // reference during control-face/child/point traversal, then curve chains.
    // IDs are never ordered by coordinate/address.
    std::vector<SubdivisionPoint> points;
    std::vector<SubdivisionFace> faces;
    std::vector<SubdivisionEdge> edges;
    std::vector<SubdivisionCurve> curves;
    std::vector<LooseControlPoint> looseControlPoints;
    std::vector<LooseControlEdge> looseControlEdges;
    std::vector<LayerInput> layers;
    Vec3 minimum{}, maximum{}; // Bounds of referenced face/curve points only.
    bool empty = true;
};

class SubdivisionSurface {
public:
    explicit SubdivisionSurface(GeometryInput input);
    ~SubdivisionSurface();
    SubdivisionSurface(SubdivisionSurface&&) noexcept;
    SubdivisionSurface& operator=(SubdivisionSurface&&) noexcept;
    SubdivisionSurface(const SubdivisionSurface&) = delete;
    SubdivisionSurface& operator=(const SubdivisionSurface&) = delete;
    const GeometryInput& input() const noexcept;
    // Current Plus supports levels 0..4. Invalid topology and unsupported
    // curve mixtures are rejected explicitly, never silently dropped.
    const SubdivisionLevel& level(unsigned level) const;
    void setPoint(std::size_t index, Vec3 position);
    bool built(unsigned level) const noexcept;
    std::size_t rebuildCount() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// An exact arithmetic trace of the existing subdivision, not a flattened
// weighted stencil: float rounding and evaluation order remain unchanged.
struct SubdivisionOperation {
    enum Code : std::uint32_t { Input, Zero, Scale, AddScaled, Divide, Relax, Midpoint, Add };
    std::uint32_t code=Zero,a=0,b=0;
    float factor=0;
    Vec3 value{};
};
struct SubdivisionProgram {
    std::vector<SubdivisionOperation> operations;
    std::vector<std::uint32_t> controls,outputs,boundOrder;
    SubdivisionLevel mesh;
    std::vector<bool> changedRoots;
    std::size_t evaluated=0,changedPoints=0;
    // Caller verifies topology/settings before reusing this program.
    void update(const GeometryInput& input);
};
SubdivisionProgram compileSubdivision(const GeometryInput& input,unsigned level);

// Design-waterline datum uses the hull, not non-hydrostatic decorations/waves.
// With no hydrostatic faces, retain the referenced-geometry fallback.
float hydrostaticKeel(const GeometryInput& input);

} // namespace fs::native_geometry
