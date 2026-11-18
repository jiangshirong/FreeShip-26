// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <optional>
#include <vector>

namespace fs {
// Oosterveld/van Oossanen B-series, reference Reynolds number 2e6.
// Optional Table 6 correction uses a specified, constant Reynolds number at
// 0.75R. No implicit roughness, cavitation, hull or wake correction.
struct BSeriesValue { double j,kt,kq; std::optional<double> efficiency; };
struct BSeriesResult {
    std::array<double,4> thrust{},torque{};
    std::array<double,4> referenceThrust{},referenceTorque{},deltaThrust{},deltaTorque{};
    std::optional<double> zeroThrust,zeroTorque;
    std::optional<BSeriesValue> maximum;
    std::vector<BSeriesValue> curve;
    bool maximumAtBoundary=false,unboundedEfficiency=false;
    BSeriesValue evaluate(double j) const;
    // All intersections, including tangent roots; never infer from plot samples.
    std::vector<double> torqueIntersections(double kq,double low,double high) const;
    bool propulsiveInterval(double low,double high) const;
    std::vector<double> thrustIdentity(double coefficient,int power) const;
};
BSeriesResult bSeries(double areaRatio,double pitchRatio,int blades,
                      std::optional<double> reynolds=std::nullopt,bool sampleCurve=true);
}
