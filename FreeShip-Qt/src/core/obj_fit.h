// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "model_exchange.h"
#include <array>
#include <vector>

namespace fs::exchange {
struct ObjTriangle {
    std::array<size_t,3> points;
    size_t face; // source polygon index
};

void reconstructObj(Document& document, const std::vector<ObjTriangle>& reference,
                    const Options& options, const std::atomic_bool* cancel);
}
