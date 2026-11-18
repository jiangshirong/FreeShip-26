// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QRegularExpression>
#include <QString>

inline QString formatNumericRanges(const QString& text){
    if(!text.contains("..")&&!text.contains(QChar(0x2026)))return text;
    static const QRegularExpression path(R"((?:[A-Za-z]:[\\/]|://|\.(?:fbm|ftm|ini|json|csv|png|dxf|obj|stl|txt)\b))",
        QRegularExpression::CaseInsensitiveOption);
    if(path.match(text).hasMatch())return text;
    static const QRegularExpression interval(
        QStringLiteral(R"((?<![\d.])([+−-]?\d+(?:[.,]\d+)?(?:\s*[%°])?)\s*(?:\.{2,3}|…)\s*([+−-]?\d+(?:[.,]\d+)?(?:\s*[%°])?)(?!\d|\.[.\d]))"));
    auto matches=interval.globalMatch(text);
    qsizetype start=0;
    QString result;
    while(matches.hasNext()){
        const auto match=matches.next();
        auto lower=match.captured(1),upper=match.captured(2);
        result+=text.mid(start,match.capturedStart()-start)+lower.replace(',','.')+QChar(0x2013)+upper.replace(',','.');
        start=match.capturedEnd();
    }
    return result+text.mid(start);
}
