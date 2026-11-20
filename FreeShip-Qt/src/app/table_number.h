// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QChar>
#include <QString>
#include <cmath>

namespace tableNumber {
// Presentation only. Keep the underlying calculation values and exports independent.
inline QString display(double value){
    if(!std::isfinite(value))return QStringLiteral("—");
    if(value==0)return QStringLiteral("0");
    constexpr int decimals=4;
    const auto magnitude=std::abs(value);
    // Use scientific notation when fixed precision would show a nonzero value as zero.
    const bool scientific=magnitude<0.00005||magnitude>=1e12;
    auto text=QString::number(value,scientific?'e':'f',decimals);
    const auto exponent=text.indexOf(QLatin1Char('e'));
    const auto suffix=exponent<0?QString{}:text.mid(exponent);
    if(exponent>=0)text.truncate(exponent);
    while(text.endsWith(QLatin1Char('0')))text.chop(1);
    if(text.endsWith(QLatin1Char('.')))text.chop(1);
    return text+suffix;
}

// For a value already identified as numerical by a legacy report parser.
// Leave labels, missing values, formulas and other text intact.
inline QString scalar(const QString& text){
    auto source=text.trimmed();
    source.replace(QChar(0x2212),QLatin1Char('-'));
    source.replace(QLatin1Char(','),QLatin1Char('.'));
    source.replace(QLatin1Char('d'),QLatin1Char('e'),Qt::CaseInsensitive);
    bool ok=false;
    const auto value=source.toDouble(&ok);
    return ok&&std::isfinite(value)?display(value):text;
}
}
