// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ui_language.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>

class PdfScaleInputs final : public QWidget {
public:
    static constexpr int maximum=10000;
    explicit PdfScaleInputs(const QString& prefix,int limit=maximum) {
        numerator.setObjectName(prefix+QStringLiteral("Numerator"));
        denominator.setObjectName(prefix+QStringLiteral("Denominator"));
        numerator.setAccessibleName(uiText("比例分子"));
        denominator.setAccessibleName(uiText("比例分母"));
        numerator.setRange(1,limit);
        denominator.setRange(1,limit);
        auto* row=new QHBoxLayout(this);
        row->setContentsMargins(0,0,0,0);
        row->addWidget(&numerator);
        row->addWidget(new QLabel(QStringLiteral(":")));
        row->addWidget(&denominator);
        row->addStretch();
    }

    QSpinBox numerator,denominator;
};
