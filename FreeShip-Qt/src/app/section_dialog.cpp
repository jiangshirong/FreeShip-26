#include "app/ui_language.h"
#include "section_dialog.h"
#include "studio_ui.h"
#include "data_motion.h"
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
#include <QToolButton>
#include <QHBoxLayout>
#include <QScreen>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
class SectionHeader final:public QHeaderView {
public:
    SectionHeader(QTableWidget* parent,const QList<QAction*>& actions):QHeaderView(Qt::Horizontal,parent){
        setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);setMinimumHeight(44);
        for(auto* action:actions){auto* button=new QToolButton(viewport());button->setDefaultAction(action);
            button->setAutoRaise(true);button->setIconSize({20,20});buttons_.append(button);}
        connect(this,&QHeaderView::sectionResized,this,[this]{positionButtons();});
        connect(this,&QHeaderView::geometriesChanged,this,[this]{positionButtons();});
    }
protected:
    void resizeEvent(QResizeEvent* event)override{QHeaderView::resizeEvent(event);positionButtons();}
private:
    void positionButtons(){
        int x=sectionViewportPosition(0)+sectionSize(0)-int(buttons_.size())*30-4;
        for(auto* button:buttons_){button->setGeometry(x,(height()-28)/2,28,28);x+=30;}
    }
    QList<QToolButton*> buttons_;
};
}

SectionDialog::SectionDialog(QWidget* parent):QDialog(parent) {
    setWindowTitle(uiText("剖面线"));setStudioWindowIcon(this,"Sections");setWindowModality(Qt::WindowModal);resize(760,680);setMinimumSize(620,480);
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(10,10,10,10);
    auto action=[&](const QString& name,const QString& icon,const QString& operation){
        auto* a=new QAction(studioIcon(icon),name,this);a->setProperty("section_operation",operation);return a;
    };
    addOne_=action(uiText("新增剖面线"),"AddOne","add");addRange_=action(uiText("批量新增"),"AddRange","add_range");
    clear_=action(uiText("清空当前类别"),"DeleteMany","clear");
    connect(addOne_,&QAction::triggered,this,[this]{add(false);});connect(addRange_,&QAction::triggered,this,[this]{add(true);});
    connect(clear_,&QAction::triggered,this,&SectionDialog::clearCurrent);
    tabs_=new QTabWidget;for(const auto* name:{uiText("横剖线"),uiText("纵剖线"),uiText("水线"),uiText("斜剖线")})tabs_->addTab(new QWidget,QString::fromUtf8(name));
    // A fixed pixel cap clips the tab labels when the dialog font grows.
    tabs_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);layout->addWidget(tabs_);
    table_=new QTableWidget(0,3);table_->setObjectName("SectionDefinitions");
    table_->setHorizontalHeader(new SectionHeader(table_,{addOne_,addRange_,clear_}));
    table_->setHorizontalHeaderLabels({uiText("编号"),uiText("距离"),uiText("曲率")});
    applyStudioScrollbars(table_);
    table_->verticalHeader()->hide();table_->setSelectionBehavior(QAbstractItemView::SelectRows);table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setMinimumSectionSize(55);
    table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Fixed);table_->setColumnWidth(0,180);
    table_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);layout->addWidget(table_,1);
    error_=new StudioMessage(StudioMessageRole::Warning);error_->setObjectName("SectionWarning");layout->addWidget(error_);
    connect(tabs_,&QTabWidget::currentChanged,this,[this]{refill();});
    connect(table_,&QTableWidget::itemSelectionChanged,this,&SectionDialog::selectionChanged);
    connect(table_,&QTableWidget::cellChanged,this,[this](int row,int column){
        if(filling_||!enabled_||!model_||!onEdit)return;
        if(column==2){submit("update",kind(),{{"index",table_->item(row,0)->data(Qt::UserRole).toInt()},{"show_curvature",table_->item(row,2)->checkState()==Qt::Checked}});return;}
        if(column!=1)return;
        bool ok=false;auto distance=table_->item(row,1)->text().toDouble(&ok);
        if(!ok||!std::isfinite(distance)||std::abs(distance)>1e9){refill();showError(uiText("请输入有效距离。"));return;}
        auto index=table_->item(row,0)->data(Qt::UserRole).toInt();
        for(const auto& s:draft_)if(s.kind==kind()&&s.index==index&&float(distance)==float(s.distance)){refill();return;}
        submit("update",kind(),{{"index",index},{"distance",distance}});
    });
    updateActions();
}
int SectionDialog::kind()const{return tabs_->currentIndex()+1;}
QSet<int> SectionDialog::selectedIndices()const{
    QSet<int> ids;for(auto index:table_->selectionModel()->selectedRows())ids.insert(table_->item(index.row(),0)->data(Qt::UserRole).toInt());return ids;
}
void SectionDialog::setSnapshot(std::shared_ptr<const fs::ModelSnapshot> model){
    model_=std::move(model);if(pending_>0)--pending_;
    setProperty("section_pending_count",pending_);
    if(!pending_){draft_=model_?model_->sections:QVector<fs::Section>{};refill();}
}
void SectionDialog::discardPending(){pending_=0;setProperty("section_pending_count",0);draft_=model_?model_->sections:QVector<fs::Section>{};refill();}
void SectionDialog::submit(const QString& operation,int sectionKind,QJsonObject fields){
    if(!enabled_||!model_||!onEdit)return;
    const auto previous=draft_;QVector<fs::Section> list;
    for(const auto& section:draft_)if(section.kind==sectionKind)list.append(section);
    auto exists=[&](float position,int except=-1){for(int i=0;i<list.size();++i)if(i!=except&&std::abs(list[i].distance-position)<1e-7)return true;return false;};
    auto position=[](fs::Section& section,float value){section.plane[3]=double(section.kind==4?-value*float(section.plane[2]):-value);section.distance=section.kind==4?double(-float(section.plane[3])/float(section.plane[2])):double(-float(section.plane[3]));};
    bool changed=false;
    if(operation=="add"){
        const auto positions=fields["positions"].toArray();
        if(positions.isEmpty()||positions.size()>512||draft_.size()+positions.size()>4096){showError(uiText("剖面线数量超过限制。"));return;}
        for(auto value:positions){const float x=float(value.toDouble());if(exists(x))continue;
            fs::Section section;section.kind=sectionKind;
            if(sectionKind==4)section.plane[1]=section.plane[2]=float(1/std::sqrt(2.0));else section.plane[sectionKind-1]=1;
            position(section,x);list.append(section);changed=true;
        }
    }else if(operation=="delete"||operation=="clear"){
        QSet<int> selected;for(auto value:fields["indices"].toArray())selected.insert(value.toInt());
        for(int i=list.size()-1;i>=0;--i)if(operation=="clear"||selected.contains(i)){list.removeAt(i);changed=true;}
    }else if(operation=="update"){
        const int index=fields["index"].toInt(-1);if(index<0||index>=list.size())return;
        if(fields.contains("distance")){
            const float x=float(fields["distance"].toDouble());
            if(exists(x,index)){refill();showError(uiText("该位置已有剖面线。"));return;}
            changed|=list[index].distance!=x;position(list[index],x);
        }
        if(fields.contains("show_curvature")){const bool value=fields["show_curvature"].toBool();changed|=list[index].showCurvature!=value;list[index].showCurvature=value;}
    }
    // Match the backend's exchange ordering, including legacy equal-plane ties.
    std::function<void(int,int)> sort=[&](int left,int right){int a=left,b=right;const double pivot=-list[(left+right)/2].plane[3];do{while(-list[a].plane[3]<pivot)++a;while(-list[b].plane[3]>pivot)--b;if(a<=b){std::swap(list[a],list[b]);++a;--b;}}while(a<=b);if(left<b)sort(left,b);if(a<right)sort(a,right);};
    if(changed&&list.size()>1)sort(0,list.size()-1);
    for(int i=0;i<list.size();++i)list[i].index=i;
    draft_.erase(std::remove_if(draft_.begin(),draft_.end(),[&](const auto& section){return section.kind==sectionKind;}),draft_.end());
    draft_+=list;++pending_;
    if(!onEdit(operation,sectionKind,fields)&&pending_>0){--pending_;draft_=previous;}
    setProperty("section_pending_count",pending_);refill();
}
void SectionDialog::setEditingEnabled(bool enabled){enabled_=enabled;table_->setEnabled(enabled);tabs_->setEnabled(enabled);updateActions();}
void SectionDialog::updateActions(){addOne_->setEnabled(enabled_);addRange_->setEnabled(enabled_);clear_->setEnabled(enabled_&&table_->rowCount()>0);}
void SectionDialog::selectionChanged(){updateActions();if(onSelection)onSelection(kind(),pending_?QSet<int>{}:selectedIndices());}
void SectionDialog::showError(const QString& message){error_->setText(message);error_->setVisible(!message.isEmpty());}
void SectionDialog::refill(){
    data_motion::beginRowUpdate(table_);QStringList rowIdentities;
    filling_=true;table_->clearSelection();table_->setRowCount(0);showError({});
    table_->horizontalHeaderItem(1)->setText(uiText("距离 (")+(model_&&model_->units=="imperial"?QString("ft"):QString("m"))+")");
    if(model_)for(const auto& s:draft_)if(s.kind==kind()){
        rowIdentities.append(QString::number(s.kind)+":"+QString::number(s.distance,'g',17));
        int row=table_->rowCount();table_->insertRow(row);
        auto* number=new QTableWidgetItem(QString::number(s.index+1));number->setData(Qt::UserRole,s.index);number->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);table_->setItem(row,0,number);
        auto* cell=new QWidget;auto* controls=new QHBoxLayout(cell);controls->setContentsMargins(4,0,4,0);
        auto* caption=new QLabel(number->text());caption->setObjectName("SectionNumber");caption->setAttribute(Qt::WA_TransparentForMouseEvents);
        controls->addWidget(caption);controls->addStretch();
        auto* remove=new QToolButton;remove->setIcon(studioIcon("DeleteOne"));remove->setIconSize({18,18});remove->setAutoRaise(true);remove->setFixedSize(28,28);
        remove->setToolTip(QString(uiText("删除第 %1 条剖面线")).arg(s.index+1));remove->setAccessibleName(remove->toolTip());
        remove->setProperty("section_operation","delete");remove->setProperty("section_index",s.index);remove->setProperty("section_kind",s.kind);
        controls->addWidget(remove);table_->setCellWidget(row,0,cell);table_->setRowHeight(row,36);
        connect(remove,&QToolButton::clicked,this,[this,index=s.index,sectionKind=s.kind]{
            if(enabled_&&model_&&kind()==sectionKind&&onEdit)submit("delete",sectionKind,{{"indices",QJsonArray{index}}});
        });
        table_->setItem(row,1,new QTableWidgetItem(QString::number(s.distance,'g',9)));
        auto* curvature=new QTableWidgetItem;curvature->setCheckState(s.showCurvature?Qt::Checked:Qt::Unchecked);
        curvature->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);curvature->setData(Qt::UserRole,QString("show_curvature"));curvature->setToolTip(uiText("显示这条剖面线的曲率梳"));table_->setItem(row,2,curvature);
    }
    filling_=false;selectionChanged();
    data_motion::finishRowUpdate(table_,QString::number(kind()),rowIdentities);
    const int limit=screen()?screen()->availableGeometry().height()-80:900;
    const int desired=qMin(qMax(480,limit),qMax(680,table_->rowCount()*36+110));
    if(height()<desired)resize(width(),desired);
}
void SectionDialog::add(bool range){
    if(!enabled_||!model_)return;
    QDialog dialog(this);setStudioWindowIcon(&dialog,range?"AddRange":"AddOne");dialog.setAttribute(Qt::WA_DontShowOnScreen,testAttribute(Qt::WA_DontShowOnScreen));dialog.setWindowTitle(range?uiText("批量新增剖面线"):uiText("新增剖面线"));
    auto* layout=new QVBoxLayout(&dialog);auto* form=new QFormLayout;layout->addLayout(form);
    auto field=[&](const QString& name,const QString& key,double value){auto* box=new QDoubleSpinBox;box->setProperty("section_field",key);box->setRange(-1e9,1e9);box->setDecimals(9);box->setValue(value);box->setSuffix(model_->units=="imperial"?" ft":" m");form->addRow(name,box);return box;};
    double lo=kind()==1?model_->minimum.x:kind()==2?model_->minimum.y:kind()==3?model_->minimum.z:model_->minimum.y+model_->minimum.z;
    double hi=kind()==1?model_->maximum.x:kind()==2?model_->maximum.y:kind()==3?model_->maximum.z:model_->maximum.y+model_->maximum.z;
    auto* first=field(range?uiText("起始位置"):uiText("距离"),"start",range?lo:0);QDoubleSpinBox *last=nullptr,*step=nullptr;
    if(range){last=field(uiText("结束位置"),"end",hi);step=field(uiText("间距"),"spacing",std::max((hi-lo)/10,0.001));step->setMinimum(0.000000001);}
    auto* warning=new StudioMessage(StudioMessageRole::Warning);layout->addWidget(warning);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);QJsonArray positions;
    buttons->button(QDialogButtonBox::Ok)->setText(uiText("确定"));buttons->button(QDialogButtonBox::Cancel)->setText(uiText("取消"));
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        first->interpretText();positions={};if(!range){positions.append(first->value());dialog.accept();return;}
        last->interpretText();step->interpretText();const auto n=std::floor((last->value()-first->value())/step->value()+1e-9)+1;
        if(n<1||n>512){warning->setText(uiText("结束位置不能小于起始位置，每批最多 512 条。"));return;}
        for(int i=0;i<int(n);++i)positions.append(first->value()+i*step->value());
        dialog.accept();
    });
    if(dialog.exec()==QDialog::Accepted&&onEdit)submit("add",kind(),{{"positions",positions}});
}
void SectionDialog::clearCurrent(){
    if(!enabled_)return;
    if(!model_||table_->rowCount()==0)return;
    QDialog dialog(this);setStudioWindowIcon(&dialog,"DeleteMany");dialog.setAttribute(Qt::WA_DontShowOnScreen,testAttribute(Qt::WA_DontShowOnScreen));dialog.setWindowTitle(uiText("删除剖面线"));
    auto* layout=new QVBoxLayout(&dialog);layout->addWidget(new StudioMessage(StudioMessageRole::Warning,uiText("清空当前类别的全部剖面线？")));
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Yes|QDialogButtonBox::No);buttons->button(QDialogButtonBox::No)->setDefault(true);buttons->button(QDialogButtonBox::Yes)->setAutoDefault(false);
    buttons->button(QDialogButtonBox::Yes)->setText(uiText("确定"));buttons->button(QDialogButtonBox::No)->setText(uiText("取消"));
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(buttons);
    if(dialog.exec()==QDialog::Accepted&&onEdit)submit("clear",kind(),{});
}
