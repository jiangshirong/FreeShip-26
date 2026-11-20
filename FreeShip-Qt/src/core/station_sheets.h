// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "station_templates.h"
class QPainter;
namespace fs::frames {
inline constexpr int maximumPdfPages=500;
enum class PdfLayout { A4Tiles, A4EdgeToEdge, WholeStation };
QRectF plotBounds(const Station& station);
int pdfPageCount(const QList<Station>& stations,int denominator,PdfLayout layout=PdfLayout::A4Tiles); // saturates at maximumPdfPages + 1
struct SheetLabels {
    QString heading; // selected contour type and thickness
    QString station;
    QString calibration;
    QString guideTitle, overview, cut, align, datum, contour, joinEdges;
    QString title(const Station& sheet) const;
};
void writePdf(const QString& path,const QList<Station>& stations,const SheetLabels& labels,
              PdfLayout layout,int denominator,const std::atomic_bool& cancelled);
void writeDxf(const QString& path,const QList<Station>& stations,double thickness,const std::atomic_bool& cancelled);
}

