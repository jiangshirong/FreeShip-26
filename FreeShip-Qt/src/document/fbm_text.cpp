// GPL-2.0-or-later. Single-byte mappings are from LazUtils, with its separate
// modified LGPL notice in third_party/lazarus. No best-fit substitutions.
#include "fbm_text.h"
#include "../../third_party/lazarus/fbm_codepages.h"
#include <QStringDecoder>
#include <QtEndian>
#include <algorithm>
#include <stdexcept>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#else
#include <iconv.h>
#include <cerrno>
#include <langinfo.h>
#endif

namespace fs::document {
namespace {
[[noreturn]] void invalid(const QString &encoding) {
    throw std::invalid_argument(("Text cannot round-trip through FBM encoding " + encoding).toStdString());
}
QString normalized(QString encoding) {
    encoding = encoding.toLower(); encoding.remove('-');
    if (encoding.isEmpty() || encoding.contains(QChar(0))) invalid(encoding);
    // 'Ansi' is an explicit choice in the old preferences. Resolve the same
    // process/system code page instead of treating it as an unknown encoding.
    if (encoding == "ansi") {
#ifdef Q_OS_WIN
        const auto cp = GetACP();
        return cp == CP_UTF8 ? QString("utf8") : QString("cp") + QString::number(cp);
#else
        QString locale = QString::fromLatin1(nl_langinfo(CODESET)).toLower();locale.remove('-');
        return locale;
#endif
    }
    return encoding;
}
const codepages::Table *table(const QString &encoding) {
    for (const auto &t : codepages::tables) if (encoding == QLatin1String(t.name)) return &t;
    return nullptr;
}
#ifndef Q_OS_WIN
QByteArray convert(const QByteArray &input, const QByteArray &from, const QByteArray &to) {
    auto converter = iconv_open(to.constData(), from.constData());
    if (converter == (iconv_t)-1) invalid(QString::fromLatin1(from + " -> " + to));
    QByteArray output(input.size() * 4 + 16, '\0');
    char *src = const_cast<char *>(input.constData()), *dst = output.data();
    size_t remaining = input.size(), space = output.size();
    const auto result = iconv(converter, &src, &remaining, &dst, &space);
    iconv_close(converter);
    if (result == size_t(-1) || remaining) invalid(QString::fromLatin1(from + " -> " + to));
    output.resize(output.size() - qsizetype(space)); return output;
}
#endif
int asianCodepage(const QString &encoding) {
    for (int cp : {932,936,949,950}) if (encoding == "cp" + QString::number(cp)) return cp;
    invalid(encoding);
}
QString decode(const QByteArray &bytes, const QString &encoding) {
    if (const auto *t = table(encoding)) {
        QString result; result.reserve(bytes.size());
        for (unsigned char byte : bytes) result.append(QChar(ushort(t->unicode[byte])));
        return result;
    }
    if (encoding == "utf8" || encoding == "utf8bom") {
        auto input = bytes;
        if (encoding == "utf8bom" && input.startsWith(QByteArray::fromHex("efbbbf"))) input.remove(0,3);
        QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
        QString text = decoder.decode(input);
        if (decoder.hasError()) invalid(encoding);
        return text;
    }
    if (encoding == "ucs2le" || encoding == "ucs2be") {
        if (bytes.size() % 2) invalid(encoding);
        QString text;
        for (qsizetype i=0;i<bytes.size();i+=2) {
            const ushort c = encoding == "ucs2le" ? qFromLittleEndian<quint16>(bytes.constData()+i) : qFromBigEndian<quint16>(bytes.constData()+i);
            if (i==0 && c==0xfeff) continue;
            if (QChar(c).isSurrogate()) invalid(encoding);
            text.append(QChar(c));
        }
        return text;
    }
    const int cp = asianCodepage(encoding);
    if (bytes.isEmpty()) return {};
#ifdef Q_OS_WIN
    int count = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, bytes.constData(), int(bytes.size()), nullptr, 0);
    if (!count) invalid(encoding);
    std::wstring result(size_t(count), L'\0');
    if (MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, bytes.constData(), int(bytes.size()), result.data(), count) != count) invalid(encoding);
    return QString::fromStdWString(result);
#else
    Q_UNUSED(cp);
    return QString::fromUtf8(convert(bytes, encoding.toLatin1(), "UTF-8"));
#endif
}
}

QString decodeFbmText(const QByteArray &bytes, const QString &encoding) {
    return decode(bytes, normalized(encoding));
}

QByteArray encodeFbmText(const QString &text, const QString &encoding) {
    if (!text.isValidUtf16() || text.contains(QChar(0))) invalid(encoding);
    const auto name = normalized(encoding);
    QByteArray result;
    if (const auto *t = table(name)) {
        result.reserve(text.size());
        for (auto c : text) {
            auto it = std::find(t->unicode.begin(), t->unicode.end(), char32_t(c.unicode()));
            if (it == t->unicode.end()) invalid(encoding);
            result.append(char(it-t->unicode.begin()));
        }
    } else if (name == "utf8" || name == "utf8bom") {
        if (name == "utf8bom") result.append(QByteArray::fromHex("efbbbf"));
        result.append(text.toUtf8());
    } else if (name == "ucs2le" || name == "ucs2be") {
        for (auto c : text) {
            if (c.isSurrogate()) invalid(encoding);
            char bytes[2];
            if (name == "ucs2le") qToLittleEndian<quint16>(c.unicode(), bytes);
            else qToBigEndian<quint16>(c.unicode(), bytes);
            result.append(bytes,2);
        }
    } else {
        const int cp = asianCodepage(name);
#ifdef Q_OS_WIN
        if (!text.isEmpty()) {
            BOOL substituted = FALSE;
            const auto wide = text.toStdWString();
            int count = WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, wide.data(), int(wide.size()), nullptr, 0, nullptr, &substituted);
            if (!count || substituted) invalid(encoding);
            result.resize(count);
            if (WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, wide.data(), int(wide.size()), result.data(), count, nullptr, &substituted)!=count || substituted) invalid(encoding);
        }
#else
        Q_UNUSED(cp);
        result = convert(text.toUtf8(), "UTF-8", name.toLatin1());
#endif
    }
    if (decode(result,name) != text) invalid(encoding);
    return result;
}
}
