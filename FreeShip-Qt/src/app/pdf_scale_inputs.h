// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ui_language.h"
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedLayout>
#include <QWheelEvent>
#include <cmath>

class PdfRatioValue final : public QDoubleSpinBox {
public:
    PdfRatioValue() {
        setDecimals(8);
        setButtonSymbols(UpDownArrows);
        setSingleStep(1.);
        setMinimumWidth(98);
    }
protected:
    StepEnabled stepEnabled() const override {
        auto enabled=QDoubleSpinBox::stepEnabled();
        if(value()-singleStep()<minimum())enabled.setFlag(StepDownEnabled,false);
        if(value()+singleStep()>maximum())enabled.setFlag(StepUpEnabled,false);
        return enabled;
    }
    void wheelEvent(QWheelEvent* e) override{e->ignore();}
    QString textFromValue(double value) const override {
        auto loc=locale();loc.setNumberOptions(loc.numberOptions()|QLocale::OmitGroupSeparator);
        auto text=loc.toString(value,'f',decimals());
        const auto decimal=locale().decimalPoint();
        if(text.contains(decimal)){
            const auto zero=locale().zeroDigit();
            while(text.endsWith(zero))text.chop(zero.size());
            if(text.endsWith(decimal))text.chop(decimal.size());
        }
        return text;
    }
};


class PdfScaleInputs final : public QWidget {
public:
    static constexpr int maximum=10000;
    explicit PdfScaleInputs(const QString& prefix,int limit=maximum) {
        numerator.setObjectName(prefix+QStringLiteral("Numerator"));
        denominator.setObjectName(prefix+QStringLiteral("Denominator"));
        numerator.setAccessibleName(uiText("比例分子"));
        denominator.setAccessibleName(uiText("比例分母"));
        numerator.setRange(.00000001,limit);
        denominator.setRange(.00000001,limit);
        numerator.setValue(1);
        denominator.setValue(1);
        setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
        auto* stack=new QStackedLayout(this);
        stack->setContentsMargins(0,0,0,0);
        auto* editable=new QWidget;
        auto* row=new QHBoxLayout(editable);
        row->setContentsMargins(0,0,0,0);
        row->addWidget(&numerator);
        row->addWidget(new QLabel(QStringLiteral(":")));
        row->addWidget(&denominator);
        row->addStretch();
        computed_.setObjectName(prefix+QStringLiteral("ComputedRatio"));
        computed_.setTextFormat(Qt::PlainText);
        stack->addWidget(editable);
        stack->addWidget(&computed_);
        pages_=stack;
    }
    // Fit is rendered as plain text; toggling it never changes the manual pair.
    void showFitRatio(bool fit,double divisor) {
        if(fit){
            auto loc=locale();loc.setNumberOptions(loc.numberOptions()|QLocale::OmitGroupSeparator);
            computed_.setText(std::isfinite(divisor)&&divisor>0?
                QStringLiteral("%1 : %2").arg(loc.toString(1),loc.toString(divisor,'g',6)):QStringLiteral("—"));
        }
        pages_->setCurrentIndex(fit?1:0);
    }

    PdfRatioValue numerator,denominator;
private:
    QLabel computed_;
    QStackedLayout* pages_;
};
