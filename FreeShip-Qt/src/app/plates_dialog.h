#pragma once
#include "core/developed_plates.h"
#include <QDialog>
#include <memory>
class ModelSession;class QListWidget;class StudioMessage;class QTableWidget;class QPushButton;class QDoubleSpinBox;
class QCheckBox;class QGraphicsScene;class QGraphicsView;class QGraphicsItem;class QPagedPaintDevice;
class QSpinBox;
class PlatesDialog final:public QDialog {
public:
    PlatesDialog(ModelSession& session,bool hidden,QWidget* parent=nullptr);
    void receive(const QJsonObject&);void showFailure(const QString&);void refreshState();
    QString exportFile(const QString& path,const QString& format);
    QJsonObject result()const;
    QJsonObject measureDrag();
    bool hasResult()const{return data_&&!data_->plates.isEmpty();}
    QString failure()const{return failure_;}
    void reject()override;
protected:
    void changeEvent(QEvent* event) override;
private:
    void buildScene();void updateItems();void positionChanged(int row);void select(int row);void transformActive();
    void translatePlateNames();
    void fit();void renderPages(QPagedPaintDevice& device);void chooseExport(const QString& format);
    ModelSession& session_;bool hidden_,updating_=false;QString failure_,completionMessage_;
    std::unique_ptr<fs::DevelopedPlates> data_;
    QListWidget* list_;StudioMessage *status_,*plateNote_;QTableWidget* metrics_;QPushButton *calculate_,*cancel_;
    QDoubleSpinBox *rotation_,*x_,*y_,*scale_,*gridX_,*gridY_;
    QSpinBox* fontSize_;
    QCheckBox *mirror_,*fill_,*interior_,*errors_,*dimensions_,*names_,*submerged_,*boxes_;
    std::array<QCheckBox*,4> sections_{};
    QGraphicsScene* scene_;QGraphicsView* view_;QVector<QGraphicsItem*> items_;
};
