// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ui_language.h"
#include "report_layout.h"
#include "table_number.h"
#include <QFile>
#include <QJsonDocument>

// Reviewed method metadata, independent of translated punctuation and legacy
// manual headings. No inference of engineering limits from free-form prose.
namespace methodHelp {
inline QString localized(const QJsonValue& item){
    if(item.isString())return uiText(item.toString());
    const auto pair=item.toObject();const auto zh=pair["zh"].toString();const auto translated=uiText(zh);
    return uiLanguage().startsWith("zh")||translated!=zh?translated:pair["en"].toString(zh);
}
inline QString parameterLabel(const QString& method,const QJsonObject& field,const QJsonObject& values){
    const auto key=field["key"].toString();
    if(method=="wind"){
        if(key=="Edit4")return QString(uiText("侧向受风投影面积 AL"))+" [m²]";
        if(key=="Edit5")return QString(uiText("上层建筑侧向投影面积 AS"))+" [m²]";
        if(key=="Edit6")return QString(uiText("横向受风投影面积 AT"))+" [m²]";
        if(key=="Edit9"){
            bool type=values["Edit9"].toDouble()>=12;
            for(int n=4;n<=8;++n)type|=values["Edit"+QString::number(n)].toDouble()==0;
            return type?uiText("船型编号 M（1–15）"):uiText("桅杆／起货柱组数 M（1–7）");
        }
    }
    return uiText(field["label"].toString(),"performance."+method+"."+key+".label");
}
inline QJsonObject notes(const QString& method,const QJsonObject& result={}){
    static const auto catalog=[] {QFile f(":/calculation-methods/calculation_method_notes.json");
        return f.open(QIODevice::ReadOnly)?QJsonDocument::fromJson(f.readAll()).object():QJsonObject{};}();
    auto entry=catalog.value(method).toObject();
    const auto variant=result.contains("propeller_design")?"native_design":result.contains("propulsion_passport")?"native_passport":result.contains("b_series")?"native_b":"";
    const auto selected=entry["variants"].toObject().value(variant).toObject();
    for(auto i=selected.begin();i!=selected.end();++i)entry[i.key()]=i.value();
    return entry;
}
inline QStringList lines(const QJsonObject& entry,const QString& category){
    QStringList output;
    for(const auto& item:entry.value(category).toArray()){
        output.append(localized(item));
    }
    return output;
}
inline QString limitContent(const QJsonArray& items,bool html){
    QString output;
    for(const auto& item:items){
        output+=report::metadata(localized(item),html);
        const auto data=item.toObject()["table"].toObject();
        if(data.isEmpty())continue;
        report::Table table{{},{},{},{0}};
        for(const auto& header:data["headers"].toArray())table.headers.append(localized(header));
        if(!table.headers.isEmpty()&&table.headers.last()==uiText("单位"))table.leftColumns.append(int(table.headers.size()-1));
        for(const auto& entry:data["rows"].toArray()){
            QStringList row;
            for(const auto& cell:entry.toArray()){
                const auto text=cell.isDouble()?tableNumber::display(cell.toDouble()):localized(cell);
                row.append(table.leftColumns.contains(int(row.size()))?text:tableNumber::scalar(text));
            }
            table.rows.append(row);
        }
        output+=report::render(table,html);
        output+=report::metadata(localized(item.toObject()["after"]),html);
    }
    return output;
}
inline QString limits(const QString& method,const QJsonObject& result={},bool html=true){
    const auto entry=notes(method,result);
    if(!entry["original_limits_in_report"].toBool())return {};
    const auto items=entry["limits"].toArray();
    if(std::any_of(items.begin(),items.end(),[](const QJsonValue& item){return item.toObject()["table"].isObject();}))
        return report::heading(uiText("适用限制"),html)+limitContent(items,html);
    return report::message(lines(entry,"limits").join('\n'),html,StudioMessageRole::Explanation,uiText("适用限制"));
}
inline QString document(const QString& method,const QJsonObject& result={}){
    const auto entry=notes(method,result);QString output;
    for(const auto& section:QList<QPair<QString,QString>>{{"limits",uiText("适用限制")},{"parameters",uiText("参数说明")},{"results",uiText("结果定义")},{"sources",uiText("方法来源")}}){
        const auto content=lines(entry,section.first);
        if(!content.isEmpty()){
            output+=report::heading(section.second,true);
            if(section.first=="limits")output+=limitContent(entry["limits"].toArray(),true);
            else for(const auto& paragraph:content)
                output+=section.first=="results"&&paragraph.contains("\\frac")
                    ?notation::displayHtml(paragraph):report::metadata(paragraph,true);
        }
    }
    return output;
}
}
