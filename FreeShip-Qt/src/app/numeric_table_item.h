// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "table_number.h"
#include <QLocale>
#include <QTableWidgetItem>

// Store the actual editable text in the stock item. Qt aliases the default
// DisplayRole and EditRole; expose a short value only when the view paints it.
class NumericTableItem final : public QTableWidgetItem {
public:
    explicit NumericTableItem(double value):QTableWidgetItem(QString::number(value,'g',17)){
        setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
    }
    QVariant data(int role)const override{
        if(role==Qt::DisplayRole||role==Qt::ToolTipRole){
            const auto raw=QTableWidgetItem::data(Qt::EditRole);
            if(role==Qt::ToolTipRole)return raw;
            const auto text=raw.toString().trimmed();
            bool ok=false;double number=text.toDouble(&ok);
            if(!ok)number=QLocale().toDouble(text,&ok);
            return ok&&std::isfinite(number)?QVariant(tableNumber::display(number)):raw;
        }
        return QTableWidgetItem::data(role);
    }
    QTableWidgetItem* clone()const override{return new NumericTableItem(*this);}
};
