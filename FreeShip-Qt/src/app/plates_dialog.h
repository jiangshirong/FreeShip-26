#pragma once
#include "core/developed_plates.h"
#include "core/model_snapshot.h"
#include "plate_assembly.h"
#include <QDialog>
#include <functional>
#include <memory>
class ModelSession;class QListWidget;class StudioMessage;class QTableWidget;class QDoubleSpinBox;
class QCheckBox;class QGraphicsScene;class QGraphicsView;class QGraphicsItem;class QPagedPaintDevice;
class QSpinBox;class PdfScaleInputs;
class PlatesDialog final:public QDialog {
public:
    PlatesDialog(ModelSession& session,bool hidden,QWidget* parent=nullptr);
    void generateIfNeeded();
    void receive(const QJsonObject&);void showFailure(const QString&);void refreshState();
    QString exportFile(const QString& path,const QString& format);
    QJsonObject result()const;
    QJsonObject measureDrag();
    bool hasResult()const{return data_&&!data_->plates.isEmpty();}
    QString failure()const{return failure_;}
    std::function<void(const QString&,quint64,const QVector<QVector<fs::Vec3>>&)> onDiagnosticFaces;
    void reject()override;
protected:
    void changeEvent(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;
private:
    friend class MainWindow;
    void previewAssembly();
    void buildScene();void updateItems();void positionChanged(int row);void select(int row);void placePlate(int row);
    void translatePlateNames();
    void refreshDiagnosticFaces();
    PlateEdgeHit edgeAt(QPoint pos)const;
    void highlightSeam(int index,bool unpaired=false);
    PlateEdgeHit hovered_,singleEdge_;
    QVector<PlateSeam> seams_;int seam_=-1;
    void fit();void renderPages(QPagedPaintDevice& device);void chooseExport(const QString& format);
    ModelSession& session_;bool hidden_,updating_=false;QString failure_,completionMessage_;
    std::unique_ptr<fs::DevelopedPlates> data_;
    std::shared_ptr<const fs::ModelSnapshot> resultModel_;
    QListWidget* list_;StudioMessage *status_,*plateNote_,*diagnosticNote_;QTableWidget* metrics_;
    QWidget* rotationPresets_;
    PdfScaleInputs* scale_;
    QDoubleSpinBox *gridX_,*gridY_;
    QSpinBox* fontSize_;
    QCheckBox *mirror_,*fill_,*interior_,*errors_,*dimensions_,*names_,*submerged_,*boxes_;
    std::array<QCheckBox*,4> sections_{};
    QGraphicsScene* scene_;QGraphicsView* view_;QVector<QGraphicsItem*> items_;
};
