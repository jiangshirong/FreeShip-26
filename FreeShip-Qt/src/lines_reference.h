#pragma once
#include <QString>

inline QString linesReferenceText(const QString& text,const QString& baseline,const QString& centerline){
    if(!baseline.isEmpty()&&(text.startsWith(QStringLiteral("基线 "))||text.startsWith(QStringLiteral("Base "))))
        return baseline.arg(text.mid(text.indexOf(' ')+1));
    if(!centerline.isEmpty()&&(text==QStringLiteral("中心")||text==QStringLiteral("Center")))
        return centerline;
    return text;
}
