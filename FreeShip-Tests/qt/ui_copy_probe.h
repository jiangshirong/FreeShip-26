// Read-only UI text inventory. Visibility is recorded, never inferred from text.
#pragma once
#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include "report_message_probe.h"
#include "app/math_notation.h"
#include <QWidget>
#include <QGroupBox>
#include <QTabWidget>
#include <QAbstractItemView>

inline QJsonArray uiCopyProbe(QWidget* window) {
    QJsonArray rows;
    auto record=[&](QWidget* widget,const QString& role,const QString& text){
        if(text.trimmed().isEmpty())return;
        rows.append(QJsonObject{{"class",widget->metaObject()->className()},
            {"name",widget->objectName()},{"role",role},{"text",text},
            {"math_symbols",notation::symbols(text)},{"math_subscripts",qobject_cast<QTextEdit*>(widget)?reportSubscriptProbe(qobject_cast<QTextEdit*>(widget)):QJsonArray{}},{"semantic_role",widget->property("studio_message_role").toString()},{"content_role",widget->property("studio_content_role").toString()},
            {"report_messages",qobject_cast<QTextEdit*>(widget)?reportMessageProbe(qobject_cast<QTextEdit*>(widget)):QJsonArray{}},
            {"visible",widget->isVisibleTo(window)},{"enabled",widget->isEnabled()}});
    };
    for(auto* widget:window->findChildren<QWidget*>()) {
        if(widget->window()!=window)continue;
        if(auto* label=qobject_cast<QLabel*>(widget))record(widget,"label",label->text());
        else if(auto* text=qobject_cast<QPlainTextEdit*>(widget);text&&text->isReadOnly())record(widget,"help",text->toPlainText());
        else if(auto* text=qobject_cast<QTextEdit*>(widget);text&&text->isReadOnly())record(widget,"report",text->toPlainText());
        else if(auto* button=qobject_cast<QAbstractButton*>(widget))record(widget,"button",button->text());
        if(auto* combo=qobject_cast<QComboBox*>(widget))for(int i=0;i<combo->count();++i)record(widget,"option",combo->itemText(i));
        if(auto* group=qobject_cast<QGroupBox*>(widget))record(widget,"group",group->title());
        if(auto* tabs=qobject_cast<QTabWidget*>(widget))for(int i=0;i<tabs->count();++i)record(widget,"tab",tabs->tabText(i));
        if(auto* view=qobject_cast<QAbstractItemView*>(widget);view&&view->model()){
            for(int i=0;i<view->model()->columnCount();++i)record(widget,"column",view->model()->headerData(i,Qt::Horizontal).toString());
        }
        if(auto* input=qobject_cast<QLineEdit*>(widget))record(widget,"placeholder",input->placeholderText());
        record(widget,"tooltip",widget->toolTip());
        record(widget,"whats_this",widget->whatsThis());
        record(widget,"status_tip",widget->statusTip());
    }
    return rows;
}
