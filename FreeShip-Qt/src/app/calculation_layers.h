// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "layer_panel.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QCheckBox>
#include <QFrame>
#include <QHash>
#include <QJsonArray>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

class CalculationLayers final:public QFrame{
public:
    enum Purpose {Hydro, Plates, LinesPlan};
    explicit CalculationLayers(Purpose purpose,QWidget* parent=nullptr):QFrame(parent),purpose_(purpose){
        setObjectName("CalculationLayerSelection");
        setStyleSheet(QStringLiteral(
            "QFrame#CalculationLayerSelection { background:#fafcfa; border:1px solid #a8c0b5; border-radius:6px; }"
            "#CalculationLayerScroll, #CalculationLayerViewport, #CalculationLayerRows { background:transparent; }"));
        auto* column=new QVBoxLayout(this);column->setContentsMargins(12,10,12,10);
        auto* title=new QLabel(uiText("选择图层"));title->setProperty("studio_content_role","section_heading");
        bindUiText(title,"text","选择图层");column->addWidget(title);
        auto* scroller=new QScrollArea;scroller->setWidgetResizable(true);scroller->setFrameShape(QFrame::NoFrame);
        scroller->setObjectName("CalculationLayerScroll");
        scroller->viewport()->setObjectName("CalculationLayerViewport");
        scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroller->setMinimumHeight(100);
        applyStudioScrollbars(scroller);column->addWidget(scroller,1);
        auto* rows=new QWidget;rows->setObjectName("CalculationLayerRows");scroller->setWidget(rows);
        list_=new QVBoxLayout(rows);list_->setContentsMargins(0,2,6,2);list_->setSpacing(6);list_->addStretch();
        bindUiRefresh(this,[this]{updateNames();});
    }
    void setModel(std::shared_ptr<const fs::ModelSnapshot> model){
        if(model==model_)return;
        if(model&&model_&&model->documentId!=model_->documentId)overrides_.clear();
        model_=std::move(model);
        QSet<fs::EntityId> occupied;
        if(model_)for(const auto& face:model_->faces)occupied.insert(face.layer);
        occupied_=std::move(occupied);
        bool marked=false;
        if(model_&&purpose_==Plates)for(const auto& l:model_->layers)marked|=l.developable&&occupied_.contains(l.id);
        while(list_->count()>1){
            auto* item=list_->takeAt(0);delete item->widget();delete item;
        }
        rows_.clear();
        if(!model_)return;
        for(const auto& layer:model_->layers){
            auto* check=new QCheckBox;check->setProperty("calculation_layer_id",qulonglong(layer.id));
            const bool hasFaces=occupied_.contains(layer.id);
            const bool initial=purpose_==Hydro?layer.useInHydrostatics:!marked||layer.developable;
            check->setChecked(purpose_==LinesPlan?layer.showInLinesplan:overrides_.value(layer.id,hasFaces&&initial));
            rows_.append(check);list_->insertWidget(list_->count()-1,check);
            connect(check,&QCheckBox::toggled,this,[this,id=layer.id,hasFaces](bool checked){
                if(purpose_==LinesPlan){
                    if(linesPlanChanged)linesPlanChanged(id,checked);
                    return;
                }
                overrides_[id]=checked;
                if(hasFaces&&changed)changed();
            });
        }
        updateNames();
    }
    void restoreLinesPlanChecks(){
        for(qsizetype i=0;i<rows_.size();++i){
            const QSignalBlocker block(rows_[i]);
            rows_[i]->setChecked(model_->layers[i].showInLinesplan);
        }
    }
    QJsonArray selected()const{
        QJsonArray ids;
        for(const auto* row:rows_)if(row->isChecked()){
            const auto id=row->property("calculation_layer_id").toULongLong();
            if(occupied_.contains(id))ids.append(qint64(id));
        }
        return ids;
    }
    std::function<void()> changed;
    std::function<void(fs::EntityId,bool)> linesPlanChanged;
private:
    void updateNames(){
        if(!model_)return;
        for(qsizetype i=0;i<rows_.size();++i){
            const auto& layer=model_->layers[i];
            const auto name=layerDisplayName(layer);
            rows_[i]->setText(name);
            rows_[i]->setToolTip(occupied_.contains(layer.id)?name:name+QStringLiteral(" · ")+uiText("无控制面"));
        }
    }
    Purpose purpose_;
    QVBoxLayout* list_;
    QHash<fs::EntityId,bool> overrides_;
    QList<QCheckBox*> rows_;
    QSet<fs::EntityId> occupied_;
    std::shared_ptr<const fs::ModelSnapshot> model_;
};
