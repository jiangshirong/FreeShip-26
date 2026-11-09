// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "fbm_codec.h"
namespace fs::document {
// Refresh only an enabled, supported thumbnail. Never promote an old format,
// enable a user's disabled preview, or rewrite opaque extensions.
Fbm withPreviewImage(const Fbm& source, const QByteArray& jpeg);
// Enabling initializes a valid neutral image until the next GUI save renders
// the current model. Disabling removes the complete JPEG envelope.
Fbm withPreviewEnabled(const Fbm& source, bool enabled);
}
