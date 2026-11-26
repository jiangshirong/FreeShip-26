// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <QVector>
#include <memory>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class SolveStatusTable;
class QTemporaryDir;
namespace fs::document { class Fbm; }
struct ComparisonHull {
    QString label;
    std::shared_ptr<const fs::document::Fbm> model;
    QString error;
};
class ComparisonParameters final : public QWidget {
public:
    ComparisonParameters(QString currentName,double designDraftM,QWidget* parent=nullptr);
    ~ComparisonParameters() override;
    void setModels(QVector<ComparisonHull> hulls,bool pending=false);
private:
    void invalidate();
    void calculate();
    QVector<ComparisonHull> hulls_;
    struct Solvers;
    std::shared_ptr<Solvers> solvers_;
    std::shared_ptr<QTemporaryDir> job_;
    QComboBox* mode_;
    QDoubleSpinBox *draft_,*trim_,*density_,*mass_,*lcg_,*tcg_,*kg_;
    QPushButton* calculate_;
    QLabel* status_;
    SolveStatusTable* table_;
    bool pending_=false;
    bool seedLoad_=true;
};
