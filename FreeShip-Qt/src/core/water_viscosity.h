// GPL-2.0-or-later. Table from Plus FreeShipUnit_Functions.inc,
// FindWaterViscosity. Values are in 10^-6 m^2/s; do not extrapolate beyond it.
#pragma once
#include <array>
#include <cmath>
#include <stdexcept>

namespace fs {
inline float waterViscosity(float celsius) {
    constexpr std::array<float,12> t{0,3.8f,5,7.2f,10,12.2f,15,17.2f,20,22.2f,25,30};
    constexpr std::array<float,12> v{1.82f,1.61f,1.56f,1.462f,1.352f,1.274f,1.189f,1.125f,1.020f,.95f,.910f,.817f};
    if(!std::isfinite(celsius)||celsius<t.front()||celsius>t.back())
        throw std::invalid_argument("Water temperature is outside the 0-30 C table");
    for(size_t i=0;i<t.size();++i) {
        if(celsius==t[i])return v[i];
        if(celsius<t[i])return v[i-1]+(v[i-1]-v[i])/(t[i-1]-t[i])*(celsius-t[i-1]);
    }
    return v.back();
}
}
