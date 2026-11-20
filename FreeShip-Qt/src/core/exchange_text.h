// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "document/fbm_text.h"
#include <QByteArrayView>
#include <stdexcept>

namespace fs::exchange {
// Decode a complete text stream, never individual fields or binary FBM data.
inline QString decodeExchangeText(const QByteArray& bytes, bool allowWesternLegacy = true) {
    const QByteArrayView input(bytes);
    const bool bom = input.startsWith("\xEF\xBB\xBF");
    if (input.startsWith("\xFF\xFE") || input.startsWith("\xFE\xFF"))
        throw std::invalid_argument("请将 UTF-16 文件转换为 UTF-8。");
    if (bytes.contains('\0')) throw std::invalid_argument("文本不能含空字符。");
    if (input.isValidUtf8()) return QString::fromUtf8(input);
    // A BOM declares UTF-8: do not reinterpret a damaged marked file as ANSI.
    if (bom || !allowWesternLegacy) throw std::invalid_argument("文件不是有效的 UTF-8 文本。");
    for (unsigned char c : bytes) {
        if (c==0x81 || c==0x8d || c==0x8f || c==0x90 || c==0x9d)
            throw std::invalid_argument("无法识别文本编码，请将文件转换为 UTF-8。");
    }
    return fs::document::decodeFbmText(bytes, "cp1252");
}
}
