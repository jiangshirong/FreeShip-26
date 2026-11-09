#include "main_window.h"
#include "layer_panel.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {
class TransformNumber final : public QDoubleSpinBox {
protected:
    QString textFromValue(double value)const override {
        QString text=QDoubleSpinBox::textFromValue(value);
        if(text.contains(locale().decimalPoint())) {
            while(text.endsWith('0'))text.chop(1);
            if(text.endsWith(locale().decimalPoint()))text.chop(locale().decimalPoint().size());
        }
        return text;
    }
};
}

QSet<fs::EntityId> MainWindow::transformPoints()const{
    auto m=session_.model();if(!m)return {};
    if(selectionKind_==HullEntity::Point)return selection_;
    QSet<fs::EntityId> result;
    if(selectionKind_==HullEntity::Edge)for(const auto& e:m->edges)if(entitySelection_.contains(e.id)){result.insert(e.a);result.insert(e.b);}
    if(selectionKind_==HullEntity::Face)for(const auto& f:m->faces)if(entitySelection_.contains(f.id))for(auto p:f.points)result.insert(p);
    if(selectionKind_==HullEntity::Curve)for(const auto& c:m->controlCurves)if(entitySelection_.contains(c.id))for(auto p:c.controls)result.insert(p);
    return result;
}
void MainWindow::advancedDialog(const QString& operation){
    if(!session_.ready()||session_.busy()||transformPoints().isEmpty())return;
    endFlowline();const auto m=session_.model();
    const bool extrusion=operation=="extrude",mirror=operation=="mirror",scale=operation=="scale",rotation=operation=="rotate"||operation=="rotate_midship";
    const auto selected=selectionKind_==HullEntity::Point?selection_:entitySelection_;
    const auto kind=selectionKind_==HullEntity::Point?"point":selectionKind_==HullEntity::Edge?"edge":selectionKind_==HullEntity::Face?"face":"curve";
    auto ordered=selected.values();std::sort(ordered.begin(),ordered.end());QJsonArray ids;for(auto id:ordered)ids.append(double(id));
    QJsonObject fields{{"kind",kind},{"ids",ids}};
    QDialog dialog(this);dialog.setAttribute(Qt::WA_DontShowOnScreen,workflow_);dialog.setProperty("advanced_operation",operation);
    dialog.setWindowTitle(extrusion?"边挤出":mirror?"面镜像":scale?"缩放":rotation?"旋转":"移动");
    auto* layout=new QVBoxLayout(&dialog);auto* form=new QFormLayout;
    const QString units=m->units=="metric"?"m":"ft";
    std::array<QDoubleSpinBox*,3> vector{};QComboBox *axis=nullptr,*layer=nullptr;QDoubleSpinBox* distance=nullptr;QCheckBox *weld=nullptr,*markers=nullptr;
    auto number=[&](const QString& role,double initial,double minimum,double maximum){auto* value=new TransformNumber;value->setProperty("field",role);value->setDecimals(12);value->setRange(minimum,maximum);value->setValue(initial);value->setMinimumWidth(170);value->setKeyboardTracking(false);return value;};
    if(mirror){
        axis=new QComboBox;axis->setProperty("field","axis");axis->addItems({"X 方向（横剖平面）","Y 方向（纵剖平面）","Z 方向（水平面）"});axis->setCurrentIndex(1);form->addRow("镜像平面",axis);
        distance=number("distance",0,-1e9,1e9);form->addRow("平面坐标 ("+units+")",distance);
        weld=new QCheckBox("合并重合的控制点");weld->setProperty("field","weld");weld->setChecked(true);form->addRow(weld);
    }else{
        if(scale||rotation)layout->addWidget(new QLabel(operation=="rotate_midship"?"以船中位置为旋转基准。":"以模型原点为基准。"));
        for(int i=0;i<3;++i){vector[i]=number(QString(QChar('x'+i)),scale?1:0,scale?1e-6:rotation?-360000:-1e9,scale?1e6:rotation?360000:1e9);
            form->addRow(QString(QChar('X'+i))+(scale?" 倍率":rotation?" (°)":" ("+units+")"),vector[i]);}
        if(extrusion){layer=new QComboBox;layer->setProperty("field","layer");for(const auto& l:m->layers)layer->addItem(l.name,QVariant::fromValue(l.id));
            auto chosen=layers_->selectedLayerId();int index=layer->findData(QVariant::fromValue(chosen));if(index>=0)layer->setCurrentIndex(index);form->addRow("新面的图层",layer);
        }else if(transformPoints().size()==m->points.size()){
            markers=new QCheckBox("同步变换标记");markers->setProperty("field","adjust_markers");markers->setChecked(true);form->addRow(markers);
            if(scale)layout->addWidget(new QLabel("整船缩放将同时更新主尺度等数据。"));
        }
    }
    layout->addLayout(form);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("确定");buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(buttons);
    if(dialog.exec()!=QDialog::Accepted)return;
    if(mirror){fields["axis"]=axis->currentIndex();fields["distance"]=distance->value();fields["weld"]=weld->isChecked();}
    else{fields["vector"]=QJsonArray{vector[0]->value(),vector[1]->value(),vector[2]->value()};
        if(extrusion)fields["layer_id"]=double(layer->currentData().toULongLong());else fields["adjust_markers"]=markers&&markers->isChecked();}
    if(operation=="rotate_midship")fields["pivot"]=QJsonArray{m->splitSectionLocation,0,0};
    session_.editAdvanced(operation=="rotate_midship"?"rotate":operation,fields);
}
