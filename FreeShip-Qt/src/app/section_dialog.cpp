#include "section_dialog.h"
#include "studio_ui.h"
#include <QAction>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

SectionDialog::SectionDialog(QWidget* parent):QDialog(parent) {
    setWindowTitle("剖面线");setWindowModality(Qt::WindowModal);resize(390,440);
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(10,10,10,10);
    auto* bar=new QToolBar;bar->setIconSize({20,20});layout->addWidget(bar);
    auto action=[&](const QString& name,const QString& icon,const QString& operation){
        auto* a=bar->addAction(studioIcon(icon),name);a->setProperty("section_operation",operation);return a;
    };
    addOne_=action("新增剖面线","AddOne","add");addRange_=action("批量新增","AddRange","add_range");
    remove_=action("删除所选剖面线","DeleteSelected","delete");clear_=action("清空当前类别","DeleteSelected","clear");
    connect(addOne_,&QAction::triggered,this,[this]{add(false);});connect(addRange_,&QAction::triggered,this,[this]{add(true);});
    connect(remove_,&QAction::triggered,this,[this]{remove(false);});connect(clear_,&QAction::triggered,this,[this]{remove(true);});
    tabs_=new QTabWidget;for(const auto* name:{"横剖线","纵剖线","水线","斜剖线"})tabs_->addTab(new QWidget,QString::fromUtf8(name));
    // A fixed pixel cap clips the tab labels when the dialog font grows.
    tabs_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);layout->addWidget(tabs_);
    table_=new QTableWidget(0,3);table_->setObjectName("SectionDefinitions");table_->setHorizontalHeaderLabels({"编号","距离","曲率"});
    table_->verticalHeader()->hide();table_->setSelectionBehavior(QAbstractItemView::SelectRows);table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    table_->horizontalHeader()->setMinimumSectionSize(55);
    table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);layout->addWidget(table_,1);
    error_=new QLabel;error_->setWordWrap(true);error_->setStyleSheet("color:#9b3528");error_->hide();layout->addWidget(error_);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close);buttons->button(QDialogButtonBox::Close)->setText("关闭");connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);layout->addWidget(buttons);
    connect(tabs_,&QTabWidget::currentChanged,this,[this]{refill();});
    connect(table_,&QTableWidget::itemSelectionChanged,this,&SectionDialog::selectionChanged);
    connect(table_,&QTableWidget::cellChanged,this,[this](int row,int column){
        if(filling_||!enabled_||!model_||!onEdit)return;
        if(column==2){onEdit("update",kind(),{{"index",table_->item(row,0)->data(Qt::UserRole).toInt()},{"show_curvature",table_->item(row,2)->checkState()==Qt::Checked}});return;}
        if(column!=1)return;
        bool ok=false;auto distance=table_->item(row,1)->text().toDouble(&ok);
        if(!ok||!std::isfinite(distance)||std::abs(distance)>1e9){refill();showError("请输入有效距离。");return;}
        auto index=table_->item(row,0)->data(Qt::UserRole).toInt();
        for(const auto& s:model_->sections)if(s.kind==kind()&&s.index==index&&float(distance)==float(s.distance)){refill();return;}
        onEdit("update",kind(),{{"index",index},{"distance",distance}});
    });
    updateActions();
}
int SectionDialog::kind()const{return tabs_->currentIndex()+1;}
QSet<int> SectionDialog::selectedIndices()const{
    QSet<int> ids;for(auto index:table_->selectionModel()->selectedRows())ids.insert(table_->item(index.row(),0)->data(Qt::UserRole).toInt());return ids;
}
void SectionDialog::setSnapshot(std::shared_ptr<const fs::ModelSnapshot> model){model_=std::move(model);refill();}
void SectionDialog::setEditingEnabled(bool enabled){enabled_=enabled;table_->setEnabled(enabled);tabs_->setEnabled(enabled);updateActions();}
void SectionDialog::updateActions(){addOne_->setEnabled(enabled_);addRange_->setEnabled(enabled_);remove_->setEnabled(enabled_&&!selectedIndices().isEmpty());clear_->setEnabled(enabled_&&table_->rowCount()>0);}
void SectionDialog::selectionChanged(){updateActions();if(onSelection)onSelection(kind(),selectedIndices());}
void SectionDialog::showError(const QString& message){error_->setText(message);error_->setVisible(!message.isEmpty());}
void SectionDialog::refill(){
    filling_=true;table_->clearSelection();table_->setRowCount(0);showError({});
    table_->horizontalHeaderItem(1)->setText("距离 ("+(model_&&model_->units=="imperial"?QString("ft"):QString("m"))+")");
    if(model_)for(const auto& s:model_->sections)if(s.kind==kind()){
        int row=table_->rowCount();table_->insertRow(row);
        auto* number=new QTableWidgetItem(QString::number(s.index+1));number->setData(Qt::UserRole,s.index);number->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);table_->setItem(row,0,number);
        table_->setItem(row,1,new QTableWidgetItem(QString::number(s.distance,'g',9)));
        auto* curvature=new QTableWidgetItem;curvature->setCheckState(s.showCurvature?Qt::Checked:Qt::Unchecked);
        curvature->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);curvature->setData(Qt::UserRole,QString("show_curvature"));curvature->setToolTip("显示这条剖面线的曲率梳");table_->setItem(row,2,curvature);
    }
    filling_=false;selectionChanged();
}
void SectionDialog::add(bool range){
    if(!enabled_||!model_)return;
    QDialog dialog(this);dialog.setAttribute(Qt::WA_DontShowOnScreen,testAttribute(Qt::WA_DontShowOnScreen));dialog.setWindowTitle(range?"批量新增剖面线":"新增剖面线");
    auto* layout=new QVBoxLayout(&dialog);auto* form=new QFormLayout;layout->addLayout(form);
    auto field=[&](const QString& name,const QString& key,double value){auto* box=new QDoubleSpinBox;box->setProperty("section_field",key);box->setRange(-1e9,1e9);box->setDecimals(9);box->setValue(value);box->setSuffix(model_->units=="imperial"?" ft":" m");form->addRow(name,box);return box;};
    double lo=kind()==1?model_->minimum.x:kind()==2?model_->minimum.y:kind()==3?model_->minimum.z:model_->minimum.y+model_->minimum.z;
    double hi=kind()==1?model_->maximum.x:kind()==2?model_->maximum.y:kind()==3?model_->maximum.z:model_->maximum.y+model_->maximum.z;
    auto* first=field(range?"起始位置":"距离","start",range?lo:0);QDoubleSpinBox *last=nullptr,*step=nullptr;
    if(range){last=field("结束位置","end",hi);step=field("间距","spacing",std::max((hi-lo)/10,0.001));step->setMinimum(0.000000001);}
    auto* warning=new QLabel;warning->setWordWrap(true);layout->addWidget(warning);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);QJsonArray positions;
    buttons->button(QDialogButtonBox::Ok)->setText("确定");buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        first->interpretText();positions={};if(!range){positions.append(first->value());dialog.accept();return;}
        last->interpretText();step->interpretText();const auto n=std::floor((last->value()-first->value())/step->value()+1e-9)+1;
        if(n<1||n>512){warning->setText("结束位置不能小于起始位置，每批最多 512 条。");return;}
        for(int i=0;i<int(n);++i)positions.append(first->value()+i*step->value());
        dialog.accept();
    });
    if(dialog.exec()==QDialog::Accepted&&onEdit)onEdit("add",kind(),{{"positions",positions}});
}
void SectionDialog::remove(bool all){
    if(!enabled_)return;
    auto selected=selectedIndices();if(!all&&selected.isEmpty())return;
    QDialog dialog(this);dialog.setAttribute(Qt::WA_DontShowOnScreen,testAttribute(Qt::WA_DontShowOnScreen));dialog.setWindowTitle("删除剖面线");
    auto* layout=new QVBoxLayout(&dialog);layout->addWidget(new QLabel(all?"清空当前类别的全部剖面线？可以撤销。":"删除所选剖面线？可以撤销。"));
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Yes|QDialogButtonBox::No);buttons->button(QDialogButtonBox::No)->setDefault(true);buttons->button(QDialogButtonBox::Yes)->setAutoDefault(false);
    buttons->button(QDialogButtonBox::Yes)->setText("删除");buttons->button(QDialogButtonBox::No)->setText("取消");
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(buttons);
    if(dialog.exec()==QDialog::Accepted&&onEdit){QJsonArray ids;auto sorted=selected.values();std::sort(sorted.begin(),sorted.end());for(int id:sorted)ids.append(id);onEdit(all?"clear":"delete",kind(),{{"indices",ids}});}
}
