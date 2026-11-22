// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "station_templates.h"
class QPainter;
namespace fs::frames {
inline constexpr int maximumPdfPages=500;
enum class PdfLayout { A4Tiles, A4EdgeToEdge, WholeStation };
struct PdfScale { int numerator=1, denominator=1; };
QRectF plotBounds(const Station& station);
int pdfPageCount(const QList<Station>& stations,PdfScale scale,PdfLayout layout); // saturates at maximumPdfPages + 1
struct SheetLabels {
    QString heading;
    QString station;
    QString guideTitle,cut,align,datum,contour,joinEdges,spine,centreline,templateCut,shellReference;
    bool interlocking=false;
    QString title(const Station& sheet) const;
};
void writePdf(const QString& path,const QList<Station>& stations,const SheetLabels& labels,
              PdfLayout layout,PdfScale scale,const std::atomic_bool& cancelled);
void writeDxf(const QString& path,const QList<Station>& stations,double thickness,const std::atomic_bool& cancelled);
}

