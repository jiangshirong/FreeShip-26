// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/model_snapshot.h"
#include "station_reference_contours.h"
#include <QDialog>
#include <QHash>
#include <atomic>
#include <memory>

namespace fs::native_geometry { struct DocumentInput; }
class QComboBox;
class QLabel;
class QPushButton;
class StudioFlowLayout;
class ComparisonCanvas;
class ComparisonParameters;
namespace fs::document { class Fbm; }
struct ComparisonSource {
    QString label,path;
    std::shared_ptr<const fs::ModelSnapshot> snapshot; // Open tab, including unsaved edits.
    QString stateFile;
};

class StationComparisonDialog final : public QDialog {
public:
    StationComparisonDialog(std::shared_ptr<const fs::ModelSnapshot> model,QString inputPath,
                            int preferredStation,QWidget* parent,QVector<ComparisonSource> sources={},
                            bool compareModels=false,QString stateFile={},QString currentName={});
    ~StationComparisonDialog() override;
    void updateCurrentHull(std::shared_ptr<const fs::ModelSnapshot> model,const QString& path,const QString& stateFile);
private:
    void compareSelected();
    void changeSource(int row);
    void refreshStations();
    void readCurrentHull(std::shared_ptr<const fs::ModelSnapshot> model,QString path,QString stateFile);
    void setStations(const fs::ModelSnapshot& model);
    void updateParameterModels();
    void rebuildSourceChips();
    void loadModelSources();
    void showStatus(const QString& text,bool error=false);
    QComboBox* station_;
    QComboBox* sources_=nullptr;
    QPushButton* browse_=nullptr;
    StudioFlowLayout* chips_=nullptr;
    QString currentName_;
    std::shared_ptr<const fs::ModelSnapshot> requestedModel_,referenceSnapshot_;
    int preferredStation_=-1;
    QVariant preferredX_;
    QVector<ComparisonSource> models_;
    struct LoadedSource {
        std::shared_ptr<const fs::native_geometry::DocumentInput> document;
        std::shared_ptr<const fs::document::Fbm> model;
        QString error;
    };
    QVector<LoadedSource> loadedSources_;
    int activeSource_=0;
    std::shared_ptr<const fs::native_geometry::DocumentInput> referenceDocument_;
    std::shared_ptr<std::atomic_bool> loadingSource_;
    QLabel* status_;
    ComparisonCanvas* canvas_;
    ComparisonParameters* parameters_=nullptr;
    std::shared_ptr<const fs::document::Fbm> currentFbm_;
    QVector<QPair<QString,int>> stationItems_;
    QVector<double> stationPositions_;
    QHash<int,fs::StationReference> references_;
    std::shared_ptr<const fs::native_geometry::DocumentInput> document_;
    std::shared_ptr<std::atomic_bool> preparing_,comparing_;
};
