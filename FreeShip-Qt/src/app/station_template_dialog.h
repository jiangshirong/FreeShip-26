// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/model_snapshot.h"
#include "core/station_sheets.h"
#include <QDialog>
class QListWidget;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
class QTimer;
class RowActionsTable;
class QToolButton;
class StationStatus;
class StationPreview;

class StationTemplateDialog final : public QDialog {
public:
    StationTemplateDialog(std::shared_ptr<const fs::ModelSnapshot> model,QString inputPath,
                          QString fileBase,const QSet<int>& selected,QWidget* parent);
    ~StationTemplateDialog() override;
private:
    void schedule(bool animate=false);
    void generate();
    void insertKitStation(int row,double x,int order,bool fromModel);
    void updateKitStationActions();
    void updateStationWarnings();
    void refresh(bool animate=false);
    void configurePdf();
    void save(bool pdf,fs::frames::PdfLayout layout=fs::frames::PdfLayout::A4Tiles,
              fs::frames::PdfScale scale={});
    void writeSelection(bool pdf,fs::frames::PdfLayout layout,fs::frames::PdfScale scale,const QString& path);
    std::shared_ptr<const fs::ModelSnapshot> model_;
    QString inputPath_,fileBase_,error_;
    QListWidget* stations_;
    QComboBox *type_,*layer_;
    QDoubleSpinBox* thickness_;
    QCheckBox* cap_;
    QWidget *kitSettings_,*kitStationPanel_;
    RowActionsTable* kitStations_;
    QDoubleSpinBox* stock_;
    QToolButton* addKitStation_;
    int nextKitSectionOrder_=0;
    StationPreview* preview_;
    StationStatus* message_;
    QPushButton *pdf_,*dxf_;
    fs::frames::PdfLayout pdfLayout_=fs::frames::PdfLayout::A4Tiles;
    fs::frames::PdfScale pdfScale_;
    QTimer* timer_;
    std::shared_ptr<std::atomic_bool> cancel_;
    QList<fs::frames::Station> sheets_;
    bool pending_=true,writing_=false,animatePreview_=false;
};

