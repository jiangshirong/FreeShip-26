// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "hollenbach_input_feedback.h"
#include <QStringList>

namespace calculationRanges {
using hollenbachFeedback::EditorRange;
using hollenbachFeedback::InputRange;
using hollenbachFeedback::infinity;

class Ranges {
    struct Field {
        QList<InputRange> choices;
        QString unavailable;
        const char* unit="";
        bool invalid=false;
    };
    const QMap<QString,EditorRange>& editors;
    QMap<QString,Field> fields;

    static bool hasEntry(const InputRange& range,const EditorRange& editor){
        using hollenbachFeedback::entryBound;
        return entryBound(range.lower,1.,editor.decimals,true)<=entryBound(range.upper,1.,editor.decimals,false);
    }
    static QString message(const InputRange& range,const EditorRange& editor,const char* unit){
        if(!hasEntry(range,editor)&&QString(range.lower.rule)==range.upper.rule)
            return QString(uiText("无可用范围。%1与%2冲突。")).arg(uiText(range.lower.rule),uiText("输入框精度"));
        auto result=hollenbachFeedback::rangeMessage(range,editor,1.,unit,"此项","允许填写 %1–%2 %3。",editor.decimals);
        if(!*unit)result.replace(" 。","。").replace(" .",".").replace(" （","（").replace("  ("," (");
        return result;
    }
public:
    explicit Ranges(const QMap<QString,EditorRange>& editors):editors(editors){}

    void unavailable(const QString& key,const char* unit,const char* first,const char* second,bool invalid){
        if(!editors.contains(key)||!editors.value(key).parsed||!invalid)return;
        auto& field=fields[key];
        field.unit=unit;
        field.invalid=true;
        if(field.unavailable.isEmpty())
            field.unavailable=QString(uiText("无可用范围。%1与%2冲突。")).arg(uiText(first),uiText(second));
    }

    void constrain(const QString& key,const char* unit,const QList<InputRange>& limits,bool invalid){
        if(!editors.contains(key))return;
        const auto editor=editors.value(key);
        auto& field=fields[key];
        field.unit=unit;
        field.invalid|=invalid&&editor.parsed;
        if(field.choices.isEmpty()){
            field.choices.append(InputRange{{editor.minimum,"输入框范围"},{editor.maximum,"输入框范围"}});
        }
        QList<InputRange> intersection;
        for(auto existing:field.choices){
            for(const auto& limit:limits){
                auto candidate=existing;
                candidate.intersect(limit.lower,limit.upper);
                intersection.append(candidate);
            }
        }
        field.choices=intersection;
    }

    void show(QMap<QString,QString>& errors) const{
        for(auto it=fields.cbegin();it!=fields.cend();++it){
            const auto& key=it.key();
            const auto& field=it.value();
            if(!field.invalid)continue;
            const auto editor=editors.value(key);
            QString description;
            QList<InputRange> choices;
            for(const auto& choice:field.choices)if(hasEntry(choice,editor))choices.append(choice);
            if(!field.unavailable.isEmpty())description=field.unavailable;
            else if(choices.isEmpty())description=message(field.choices.front(),editor,field.unit);
            else if(choices.size()==1)description=message(choices.front(),editor,field.unit);
            else{
                QStringList ranges;
                std::sort(choices.begin(),choices.end(),[](const InputRange& a,const InputRange& b){
                    return a.lower.value<b.lower.value;
                });
                for(const auto& choice:choices){
                    using namespace hollenbachFeedback;
                    ranges.append(entryText(entryBound(choice.lower,1.,editor.decimals,true),editor.decimals)
                        +"–"+entryText(entryBound(choice.upper,1.,editor.decimals,false),editor.decimals));
                }
                description=QString(uiText("允许填写 %1。")).arg(
                    (ranges.join(uiText(" 或 "))+QStringLiteral(" ")+field.unit).trimmed());
            }
            if(!errors.contains(key))errors[key]=description;
            else if(!description.isEmpty()&&!errors.value(key).contains(description))errors[key]+=QLatin1Char('\n')+description;
        }
    }
};
}
