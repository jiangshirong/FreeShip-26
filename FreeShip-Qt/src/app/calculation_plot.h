// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <functional>
#include <memory>
class QCheckBox;class QToolButton;class QScrollArea;class QGridLayout;class QBoxLayout;
struct CalculationCurve {
    QString source,name,xTitle,yTitle,axis;
    QColor color;Qt::PenStyle style=Qt::SolidLine;double width=1;bool markers=false;
    QVector<QPointF> points; // Order, duplicate X values and gaps are preserved.
    QRectF bounds;qsizetype finitePoints=0,segments=0;
};
struct CalculationChartData {
    QString method;QVector<CalculationCurve> curves;bool legendRight=false;double preparationMs=0,decodeMs=0;
    QString numericFingerprint()const;
    static std::shared_ptr<const CalculationChartData> read(const QString&,const QJsonObject&);
private:
    mutable QString numericSha256_; // Diagnostic only; normal drawing does not hash the full result.
};
struct CalculationChartState {
    std::shared_ptr<const CalculationChartData> data;QVector<bool> visible;
    int selected=0;bool selectedOnly=false;
};
struct CalculationAxis {
    double minimum=0,maximum=1;QVector<double> ticks;
    static CalculationAxis fit(double minimum,double maximum,int intervals=5);
};
class CalculationCanvas;
class CalculationPlot final:public QWidget {
public:
    explicit CalculationPlot(QWidget* parent=nullptr);
    static void prepareRenderer(); // Nonblocking shader/context preparation while entering a calculation.
    static void shutdownRenderer(); // Also usable by hosts which do not enter QApplication::exec().
    void setState(std::shared_ptr<CalculationChartState> state);
    void setSelected(int index);
    void synchronize();
    QJsonObject evidence()const;
    std::function<void()> onDisplayChanged;
protected:
    void resizeEvent(QResizeEvent*)override;
    void changeEvent(QEvent*)override;
private:
    void arrangeLegend();
    std::shared_ptr<CalculationChartState> state_;
    CalculationCanvas* canvas_;QCheckBox* single_;QScrollArea* legend_;
    QGridLayout* legendLayout_=nullptr;QBoxLayout* content_;
    QVector<QToolButton*> buttons_;int legendColumns_=0;bool right_=false;
};
