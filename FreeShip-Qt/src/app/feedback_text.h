// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ui_language.h"
#include "document/fbm_text.h"
#include <QString>
#include <QStringList>

// Translate the diagnostic template before inserting the encoding identifier.
inline QString uiDiagnosticText(const QString& source){
    const QLatin1String prefix(fs::document::FbmTextDiagnosticPrefix);
    if(source.startsWith(prefix))
        return QString(uiText("无法按 %1 编码无损转换 FBM 中的文字。")).arg(source.mid(prefix.size()));
    return uiText(source);
}

// State-dependent feedback: omit empty diagnostics, retain meaningful outcomes.
// Each optional sentence is translated as a whole, not assembled from fragments.
inline QString completionText(qsizetype count,const char* source){
    return count>0?QString(uiText(source)).arg(count):QString{};
}
inline QString hydroFeedbackText(qsizetype count,int invalid,int partial,double elapsedMs){
    QStringList lines;
    if(count>0)lines<<QString(uiText("完成 %1 个工况，耗时 %2 s。")).arg(count).arg(elapsedMs/1000,0,'f',3);
    if(invalid>0)lines<<completionText(invalid,count>0?"其中 %1 个未得到有效结果。":"%1 个工况未得到有效结果。");
    if(partial>0)lines<<completionText(partial,count>0?"其中 %1 个工况的部分量未提供（见状态列）。":"%1 个工况的部分量未提供（见状态列）。");
    return lines.join('\n');
}
inline QString performanceFeedbackText(qsizetype count,bool groups,double elapsedMs){
    if(count<=0)return {};
    return QString(groups?uiText("完成 %1 组数据，耗时 %2 s。"):uiText("完成 %1 条曲线，耗时 %2 s。"))
        .arg(count).arg(elapsedMs/1000,0,'f',3);
}
inline QString sectionsFeedbackText(int stations,int points,int capped,int empty,const QString& path){
    QStringList lines;
    if(stations>0&&points>0)lines<<QString(uiText("已导出 %1 个断面、%2 个点。")).arg(stations).arg(points);
    else if(stations>0)lines<<completionText(stations,"已导出 %1 个断面。");
    else if(points>0)lines<<completionText(points,"已导出 %1 个点。");
    if(capped>0)lines<<completionText(capped,"已水平封顶 %1 个断面。");
    if(empty>0)lines<<completionText(empty,"有 %1 个断面未与船体相交。");
    if(!path.isEmpty())lines<<path;
    return lines.join('\n');
}
