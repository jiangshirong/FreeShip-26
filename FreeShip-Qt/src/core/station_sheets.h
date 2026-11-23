// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "station_templates.h"
class QPainter;
namespace fs::frames {
inline constexpr int maximumPdfPages=500;
enum class PdfLayout { A4Tiles, A4EdgeToEdge, WholeStation };
struct PdfScale { double numerator=1, denominator=1; bool fit=false; };
QRectF plotBounds(const Station& station);
double pdfDivisor(const QList<Station>& stations,PdfScale scale,PdfLayout layout);
QSizeF firstPdfPaper(const Station& station,PdfLayout layout,double divisor,bool fit);
bool firstPdfIsGuide(const Station& station,PdfLayout layout,double divisor);
int pdfPageCount(const QList<Station>& stations,PdfScale scale,PdfLayout layout); // saturates at maximumPdfPages + 1
struct SheetLabels {
    QString heading;
    QString station;
    QString guideTitle,cut,align,datum,contour,joinEdges,spine,centreline,templateCut,shellReference;
    bool interlocking=false;
    QString baseline;
    QString fitScale;
    QString title(const Station& sheet) const;
};
void paintFirstPdfPage(QPainter& p,const Station& station,const SheetLabels& labels,
                       PdfLayout layout,double divisor,QSizeF paper,const QString& caption,bool guide);
void writePdf(const QString& path,const QList<Station>& stations,const SheetLabels& labels,
              PdfLayout layout,PdfScale scale,const std::atomic_bool& cancelled);
void writeDxf(const QString& path,const QList<Station>& stations,double thickness,const std::atomic_bool& cancelled);
}

