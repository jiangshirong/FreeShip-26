// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/model_snapshot.h"
#include "core/station_sheets.h"
#include <QDialog>
class QListWidget;
class QAction;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;
class QRadioButton;
class QPushButton;
class QTimer;
class StudioMessage;
class StationPreview;

class StationTemplateDialog final : public QDialog {
public:
    StationTemplateDialog(std::shared_ptr<const fs::ModelSnapshot> model,QString inputPath,
                          QString fileBase,const QSet<int>& selected,QWidget* parent);
    ~StationTemplateDialog() override;
private:
    void schedule();
    void generate();
    void updateStationWarnings();
    void refresh();
    void save(bool pdf,fs::frames::PdfLayout layout=fs::frames::PdfLayout::A4Tiles);
    void writeSelection(bool pdf,fs::frames::PdfLayout layout,const QString& path);
    std::shared_ptr<const fs::ModelSnapshot> model_;
    QString inputPath_,fileBase_,error_;
    QListWidget* stations_;
    QComboBox *type_,*layer_;
    QDoubleSpinBox* thickness_;
    QSpinBox* scale_;
    QRadioButton* noTrim_;
    fs::frames::PdfLayout tileLayout() const;
    QCheckBox* cap_;
    StationPreview* preview_;
    StudioMessage* message_;
    QPushButton *pdf_,*dxf_;
    QAction *tiledPdf_,*wholePdf_;
    QTimer* timer_;
    std::shared_ptr<std::atomic_bool> cancel_;
    QList<fs::frames::Station> sheets_;
    int pageCount_=0;
    bool pending_=true,writing_=false;
};

