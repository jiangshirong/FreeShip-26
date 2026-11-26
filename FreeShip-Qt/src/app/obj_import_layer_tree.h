// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "studio_ui.h"
#include "core/model_exchange.h"
#include "layer_panel.h"
#include "ui_language.h"
#include <QDrag>
#include <QDropEvent>
#include <QPersistentModelIndex>
#include <QSignalBlocker>
#include <QTreeWidget>

class ObjImportLayerTree final : public QTreeWidget {
public:
    explicit ObjImportLayerTree(QWidget* parent=nullptr) : QTreeWidget(parent) {
        setObjectName("ObjImportLayers");
        setHeaderHidden(true);
        setMinimumWidth(180);
        setUniformRowHeights(true);
        setDragDropMode(QAbstractItemView::DragDrop);
        setDefaultDropAction(Qt::CopyAction);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        setExpandsOnDoubleClick(false);
        connect(this,&QTreeWidget::itemClicked,this,[](QTreeWidgetItem* item){
            if(!item->parent())item->setExpanded(!item->isExpanded());
        });
        connect(this,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem* item){
            if(!item->parent())editItem(item);
        });
        connect(this,&QTreeWidget::itemChanged,this,[this](QTreeWidgetItem* item){
            if(item->parent())return;
            const QSignalBlocker block(this);
            const auto name=item->text(0).trimmed();
            if(!name.isEmpty()&&name!=displayName(item)&&!nameExists(name,item))
                item->setData(0,Qt::UserRole,name);
            item->setText(0,displayName(item));
        });
        bindUiRefresh(this,[this]{refreshLayerNames();});
    }

    void setSources(const QStringList& sources){
        clear();
        if(sources.isEmpty())return;
        auto* initial=appendLayer();
        for(const auto& source:sources){
            auto* item=new QTreeWidgetItem(initial,{source});
            item->setData(0,Qt::UserRole,source);
            item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsDragEnabled);
        }
        initial->setExpanded(true);
    }

    QVector<fs::exchange::ObjAssignedLayer> assignments() const {
        QVector<fs::exchange::ObjAssignedLayer> result;
        for(int i=0;i<topLevelItemCount();++i){
            const auto* layer=topLevelItem(i);
            QStringList groups;
            for(int j=0;j<layer->childCount();++j)
                groups.append(layer->child(j)->data(0,Qt::UserRole).toString());
            result.append({layer->data(0,Qt::UserRole).toString(),std::move(groups)});
        }
        return result;
    }

    void moveGroup(QTreeWidgetItem* group,QTreeWidgetItem* target=nullptr){
        auto* source=group?group->parent():nullptr;
        if(!source||source==target||(!target&&topLevelItemCount()>=1000))return;
        if(!target)target=appendLayer();
        target->addChild(source->takeChild(source->indexOfChild(group)));
        target->setExpanded(true);
        setCurrentItem(group);
        if(source!=topLevelItem(0)&&!source->childCount()){
            delete source;
            refreshLayerNames();
        }
    }

protected:
    void startDrag(Qt::DropActions) override {
        auto* item=currentItem();
        if(!item||!item->parent())return;
        dragged_=indexFromItem(item);
        auto* drag=new QDrag(this);
        drag->setMimeData(mimeData({item}));
        drag->setPixmap(viewport()->grab(visualItemRect(item)));
        drag->exec(Qt::CopyAction);
        dragged_=QPersistentModelIndex();
    }

    void dropEvent(QDropEvent* event) override {
        if(event->source()!=this||!dragged_.isValid()){event->ignore();return;}
        auto* target=itemAt(event->position().toPoint());
        if(target&&!target->parent()&&dropIndicatorPosition()!=OnItem)target=nullptr;
        if(target&&target->parent())target=target->parent();
        moveGroup(itemFromIndex(dragged_),target);
        event->acceptProposedAction();
    }

    void dragMoveEvent(QDragMoveEvent* event) override {
        QTreeWidget::dragMoveEvent(event);
        if(event->source()==this&&dragged_.isValid())event->acceptProposedAction();
    }

private:
    QTreeWidgetItem* appendLayer(){
        const QSignalBlocker block(this);
        auto* item=new QTreeWidgetItem(this);
        item->setText(0,displayName(item));
        item->setIcon(0,studioIcon("Layers"));
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsEditable|Qt::ItemIsDropEnabled);
        return item;
    }
    QString displayName(QTreeWidgetItem* item)const{
        fs::Layer layer;
        layer.name=item->data(0,Qt::UserRole).toString();
        if(layer.name.isEmpty())layer.defaultNameNumber=indexOfTopLevelItem(item);
        return layerDisplayName(layer);
    }
    void refreshLayerNames(){
        const QSignalBlocker block(this);
        for(int i=0;i<topLevelItemCount();++i){
            auto* layer=topLevelItem(i);
            layer->setText(0,displayName(layer));
        }
    }
    bool nameExists(const QString& name,const QTreeWidgetItem* except=nullptr)const{
        for(int i=0;i<topLevelItemCount();++i)
            if(topLevelItem(i)!=except&&topLevelItem(i)->text(0)==name)return true;
        return false;
    }
    QPersistentModelIndex dragged_;
};
