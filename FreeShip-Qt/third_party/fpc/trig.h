#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace fs::document::fpc {
[[noreturn]] inline void invalid(const char *message) { throw std::invalid_argument(message); }
/* The bounded trigonometric implementation below is an altered C++ translation
 * of Free Pascal 3.2.2 rtl/inc/genmath.inc (rem_pio2, k_rem_pio2, polevl and
 * Sin/Cos), not a new mathematical approximation. Only precision=2 and finite
 * arguments within our validated +/-360000 degree limit are supported.
 * The original operation order, coefficients and argument reduction are kept.
 * The Win64 RTL uses Double; MinGW's x87 libm gives different quadrant residues.
 *
 * Original Free Pascal run time library notice:
 * Copyright (c) 1999-2007 by Several contributors.
 * The source code of the Free Pascal Runtime Libraries and packages are
 * distributed under the Library GNU General Public License (see COPYING)
 * with the following modification (COPYING.FPC):
 * As a special exception, the copyright holders of this library give you
 * permission to link this library with independent modules to produce an
 * executable, regardless of the license terms of these independent modules,
 * and to copy and distribute the resulting executable under terms of your
 * choice, provided that you also meet, for each linked independent module,
 * the terms and conditions of the license of that module. An independent
 * module is a module which is not derived from or based on this library.
 * If you modify this library, you may extend this exception to your version
 * of the library, but you are not obligated to do so. If you do not wish to
 * do so, delete this exception statement from your version.
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Original genmath credits:
 * Copyright Abandoned, 1987, Fred Fish.
 * This previously copyrighted work has been placed into the public domain
 * by the author (Fred Fish) and may be freely used for any purpose, private
 * or commercial. I would appreciate it, as a courtesy, if this notice is
 * left in all copies and derivative works. Thank you, and enjoy...
 * The author makes no warranty of any kind with respect to this product and
 * explicitly disclaims any implied warranties of merchantability or fitness
 * for any particular purpose.
 * Copyright (c) 1992 Odent Jean Philippe. The source can be modified as long
 * as my name appears and some notes explaining the modifications done are
 * included in the file. Copyright (c) 1997 Carl Eric Codere.
 *
 * AMath/DAMath portions: (C) Copyright 2009-2013 Wolfgang Ehrhardt.
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software in
 *    a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Argument-reduction portions:
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this software is freely
 * granted, provided that this notice is preserved.
 * Pascal port from DAMath library (C) Copyright 2013 Wolfgang Ehrhardt.
 */
inline constexpr double Two24 = 16777216.0, TwoMinus24 = 5.9604644775390625e-08;
inline int reducePayneHanek(const std::array<double,3> &x, std::array<double,2> &y, int e0, int nx) {
    constexpr std::array<double,8> piOverTwo{
        1.57079625129699707031e+00,7.54978941586159635335e-08,5.39030252995776476554e-15,
        3.28200341580791294123e-22,1.27065575308067607349e-29,1.22933308981111328932e-36,
        2.73370053816464559624e-44,2.16741683877804819444e-51};
    constexpr std::array<int,20> inversePi{
        0xA2F983,0x6E4E44,0x1529FC,0x2757D1,0xF534DD,0xC0DB62,0x95993C,0x439041,0xFE5163,0xABDEBB,
        0xC561B7,0x246E3A,0x424DD2,0xE00649,0x2EEA09,0xD1921C,0xFE1DEB,0x1CB129,0xA73EE8,0x8235F5};
    // The angle bound makes jv=0; 20 limbs exceed the RTL's required work
    // range here. Recomputations are guarded instead of permitting overflow.
    constexpr int jk=4,jp=4;
    const int jx=nx-1,jv=std::max(0,(e0-3)/24); int q0=e0-24*(jv+1),jz=jk,n=0,ih=0;
    std::array<int,20> iq{}; std::array<double,20> f{},fq{},q{}; double z=0,fw=0;
    for (int i=0,j=jv-jx;i<=jx+jk;++i,++j) f.at(i)=j<0 ? 0 : inversePi.at(j);
    for (int i=0;i<=jk;++i) {
        fw=0; for (int j=0;j<=jx;++j) fw=fw+x[j]*f[jx+i-j]; q[i]=fw;
    }
    for (;;) {
        z=q[jz];
        for (int i=0,j=jz;j>=1;--j,++i) {
            fw=std::trunc(TwoMinus24*z); iq[i]=int(std::trunc(z-Two24*fw)); z=q[j-1]+fw;
        }
        z=std::ldexp(z,q0); z=z-8.0*std::floor(z*.125); n=int(std::trunc(z)); z=z-n; ih=0;
        if (q0>0) {
            const int t=iq[jz-1]>>(24-q0); n+=t; iq[jz-1]-=t<<(24-q0); ih=iq[jz-1]>>(23-q0);
        } else if (q0==0) ih=iq[jz-1]>>23;
        else if (z>=.5) ih=2;
        if (ih>0) {
            ++n; int carry=0;
            for (int i=0;i<jz;++i) {
                const int t=iq[i];
                if (!carry) { if (t) { carry=1; iq[i]=0x1000000-t; } }
                else iq[i]=0xffffff-t;
            }
            if (q0==1) iq[jz-1]&=0x7fffff;
            else if (q0==2) iq[jz-1]&=0x3fffff;
            if (ih==2) { z=1.0-z; if (carry) z=z-std::ldexp(1.0,q0); }
        }
        if (z!=0) break;
        int t=0; for (int i=jz-1;i>=jk;--i) t|=iq[i]; if (t) break;
        int k=1; while (k<=jk && iq[jk-k]==0) ++k;
        if (k>jk || jx+jz+k>=int(f.size()) || jv+jz+k>=int(inversePi.size())) invalid("Rotation argument reduction work limit exceeded");
        for (int i=jz+1;i<=jz+k;++i) {
            f[jx+i]=inversePi[jv+i]; fw=0;
            for (int j=0;j<=jx;++j) fw=fw+x[j]*f[jx+i-j];
            q[i]=fw;
        }
        jz+=k;
    }
    if (z==0) {
        do { --jz; q0-=24; if (jz<0) invalid("Invalid rotation reduction"); } while (iq[jz]==0);
    } else {
        z=std::ldexp(z,-q0);
        if (z>=Two24) {
            fw=std::trunc(TwoMinus24*z); iq[jz]=int(std::trunc(z-Two24*fw)); ++jz; q0+=24; iq.at(jz)=int(std::trunc(fw));
        } else iq[jz]=int(std::trunc(z));
    }
    fw=std::ldexp(1.0,q0);
    for (int i=jz;i>=0;--i) { q[i]=fw*iq[i]; fw=fw*TwoMinus24; }
    for (int i=jz;i>=0;--i) {
        fw=0; for (int k=0;k<=jp && k<=jz-i;++k) fw=fw+piOverTwo[k]*q[i+k]; fq[jz-i]=fw;
    }
    fw=0; for (int i=jz;i>=0;--i) fw=fw+fq[i]; y[0]=ih==0 ? fw : -fw;
    fw=fq[0]-fw; for (int i=1;i<=jz;++i) fw=fw+fq[i]; y[1]=ih==0 ? fw : -fw;
    return n&7;
}
inline int reduceAngle(double x, double &z) {
    constexpr double piOverFour=7.85398163397448309616E-1;
    if (std::abs(x)<piOverFour) { z=x; return 0; }
    double y=std::floor(x/piOverFour); int i=int(std::trunc(y-16.0*std::floor(y*.0625)));
    if (i&1) { ++i; y=y+1.0; }
    z=((x-y*7.85398125648498535156E-1)-y*3.77489470793079817668E-8)-y*2.69515142907905952645E-15;
    if (std::abs(z)>2.384185791015625E-7) return (i>>1)&7;
    z=std::abs(x); int exponent=0; std::frexp(z,&exponent); const int e0=exponent-24;
    z=std::ldexp(z,-e0); std::array<double,3> tx{}; std::array<double,2> ty{};
    tx[0]=std::trunc(z); z=(z-tx[0])*Two24; tx[1]=std::trunc(z); tx[2]=(z-tx[1])*Two24;
    int nx=3; while (nx>1 && tx[nx-1]==0) --nx;
    const int result=reducePayneHanek(tx,ty,e0,nx);
    if (x<0) { z=-ty[0]-ty[1]; return (-result)&7; }
    z=ty[0]+ty[1]; return result;
}
inline double radians(double angle, bool cosine) {
    if (!std::isfinite(angle) || std::abs(angle)>6283.185307179587) invalid("FPC trigonometric argument out of range");
    if (!cosine && angle==0) return angle;
    constexpr std::array<double,6> sine{1.58962301576546568060E-10,-2.50507477628578072866E-8,2.75573136213857245213E-6,
        -1.98412698295895385996E-4,8.33333333332211858878E-3,-1.66666666666666307295E-1};
    constexpr std::array<double,6> cosines{-1.13585365213876817300E-11,2.08757008419747316778E-9,-2.75573141792967388112E-7,
        2.48015872888517045348E-5,-1.38888888888730564116E-3,4.16666666666665929218E-2};
    double z=0; const int quadrant=reduceAngle(angle,z)&3; const double zz=z*z;
    const bool useSine=cosine ? (quadrant==1 || quadrant==3) : (quadrant!=1 && quadrant!=3);
    const auto &coefficients=useSine ? sine : cosines;
    double polynomial=coefficients[0]; for (std::size_t i=1;i<coefficients.size();++i) polynomial=polynomial*zz+coefficients[i];
    const double result=useSine ? z+z*z*z*polynomial : 1.0-std::ldexp(zz,-1)+zz*zz*polynomial;
    const bool negative=cosine ? (quadrant==1 || quadrant==2) : quadrant>1;
    return negative ? -result : result;
}
inline float trig(float degrees, bool cosine) {
    if (!std::isfinite(degrees) || std::abs(degrees)>360000) invalid("FPC rotation angle out of range");
    constexpr double factor = 3.141592653589793238462643383279502884 / 180.0;
    return float(radians(double(degrees)*factor,cosine));
}
} // namespace fs::document::fpc

