// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "core/loading_cases.h"
#include "developer_loading_template.h"
#include "developer_mode.h"
#include "draft_history.h"
#include "numeric_table_item.h"
#include "studio_row_actions.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <memory>
#include <optional>
#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QFontDatabase>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace loadingEditor {
enum Table { Cases, Items, Fixed, Criteria, Count };
struct Draft {
    QJsonObject book;
    int selected = -1;
};

class CaseList final : public QListWidget {
public:
    CaseList(){
        setObjectName("load_cases");
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setUniformItemSizes(true);
        setMouseTracking(true);
        setEditTriggers(QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed);
        setItemDelegate(new NameDelegate(this));
        setStyleSheet("QListWidget#load_cases{background:transparent;}"
                      "QListWidget#load_cases::item{border-radius:5px;padding-left:9px;}"
                      "QListWidget#load_cases::item:hover{background:#eaf1ed;}"
                      "QListWidget#load_cases::item:selected{background:#deebe4;color:#176057;}");
        applyStudioScrollbars(this);
    }

    void clearCases(){
        for(auto* button:buttons_){button->hide();button->deleteLater();}
        buttons_.clear();
        clear();
    }
    void addDeleteButton(QToolButton* button){buttons_.append(button);}
    void placeButtons(){
        for(int row=0;row<buttons_.size();++row){
            const auto rect=visualItemRect(item(row));
            const QRect action(rect.right()-ButtonSize+1-Inset,rect.center().y()-ButtonSize/2,ButtonSize,ButtonSize);
            auto* button=buttons_[row];
            button->setGeometry(action);
            button->setVisible(viewport()->rect().intersects(action));
        }
    }
protected:
    void resizeEvent(QResizeEvent* event)override{QListWidget::resizeEvent(event);placeButtons();}
    void scrollContentsBy(int dx,int dy)override{QListWidget::scrollContentsBy(dx,dy);placeButtons();}
    void updateGeometries()override{QListWidget::updateGeometries();placeButtons();}
private:
    static constexpr int ButtonSize=28;
    static constexpr int Inset=4;
    class NameDelegate final : public QStyledItemDelegate {
    public:
        explicit NameDelegate(CaseList* list):QStyledItemDelegate(list){}
        QSize sizeHint(const QStyleOptionViewItem& option,const QModelIndex& index)const override{
            auto size=QStyledItemDelegate::sizeHint(option,index);
            size.setHeight(std::max(ButtonSize+2*Inset,size.height()+10));
            return size;
        }
        void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index)const override{
            auto text=option;
            initStyleOption(&text,index);
            text.state&=~QStyle::State_HasFocus;
            const auto* widget=text.widget;
            const auto* style=widget?widget->style():QApplication::style();
            text.text=text.fontMetrics.elidedText(text.text,Qt::ElideRight,std::max(0,text.rect.width()-ButtonSize-2*Inset-12));
            style->drawControl(QStyle::CE_ItemViewItem,&text,painter,widget);
        }
        void updateEditorGeometry(QWidget* editor,const QStyleOptionViewItem& option,const QModelIndex&)const override{
            editor->setGeometry(option.rect.adjusted(Inset,1,-ButtonSize-2*Inset,-1));
        }
    };
    QList<QToolButton*> buttons_;
};

class ItemKindCombo final : public QComboBox {
protected:
    void wheelEvent(QWheelEvent* event)override{event->ignore();}
};

class LoadSummary final : public QFrame {
public:
    LoadSummary(){
        setObjectName("load_summary");
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        setStyleSheet(
            "QFrame#load_summary_coordinates{border:1px solid #a9c0b6;border-radius:2px;}"
            "QLabel[loadSummaryRole='label']{color:#365952;}"
            "QLabel[loadSummaryRole='number']{color:#204840;}"
            "QLabel[loadSummaryRole='unit']{color:#52736a;}");
        layout_=new QBoxLayout(QBoxLayout::LeftToRight,this);
        layout_->setContentsMargins(0,3,0,3);
        layout_->setSpacing(20);
        layout_->setSizeConstraint(QLayout::SetNoConstraint);

        auto* mass=new QWidget;
        mass->setObjectName("load_summary_mass");
        auto* massLine=new QHBoxLayout(mass);
        massLine->setContentsMargins(0,0,0,0);
        massLine->setSpacing(5);
        auto* heading=metricLabel("label");
        bindUiText(heading,"text","排水量");
        massLine->addWidget(heading,0,Qt::AlignBaseline);
        numbers_[0]=metricLabel("number",true);
        massLine->addWidget(numbers_[0],0,Qt::AlignBaseline);
        massLine->addWidget(unitLabel("t"),0,Qt::AlignBaseline);
        massLine->addStretch();
        layout_->addWidget(mass);

        auto* gravity=new QWidget;
        gravity->setObjectName("load_summary_gravity");
        auto* gravityLine=new QHBoxLayout(gravity);
        gravityLine->setContentsMargins(0,0,0,0);
        gravityLine->setSpacing(7);
        auto* title=metricLabel("label");
        title->setObjectName("load_summary_gravity_label");
        bindUiText(title,"text","总重心");
        gravityLine->addWidget(title,0,Qt::AlignBaseline);
        auto* coordinates=new QFrame;
        coordinates->setObjectName("load_summary_coordinates");
        auto* axes=new QHBoxLayout(coordinates);
        axes->setContentsMargins(8,4,8,4);
        axes->setSpacing(15);
        constexpr std::array axisNames{"X","Y","Z"};
        for(size_t i=0;i<axisNames.size();++i){
            auto* axis=new QHBoxLayout;
            axis->setSpacing(4);
            auto* label=metricLabel("label");
            label->setText(axisNames[i]);axis->addWidget(label,0,Qt::AlignBaseline);
            auto* number=numbers_[i+1]=metricLabel("number",true);
            auto fixed=QFontDatabase::systemFont(QFontDatabase::FixedFont);
            if(number->font().pointSizeF()>0)fixed.setPointSizeF(number->font().pointSizeF());
            else fixed.setPixelSize(number->font().pixelSize());
            fixed.setWeight(QFont::Medium);
            number->setFont(fixed);
            axis->addWidget(number,0,Qt::AlignBaseline);
            axes->addLayout(axis);
        }
        gravityLine->addWidget(coordinates);
        auto* unit=unitLabel("m");
        unit->setObjectName("load_summary_gravity_unit");
        gravityLine->addWidget(unit,0,Qt::AlignBaseline);
        gravityLine->addStretch();
        layout_->addWidget(gravity);
        layout_->addStretch(1);
    }

    void setValues(double mass,fs::loading::P gravity){
        setProperty("mass",mass);
        const std::array values{mass,gravity.x,gravity.y,gravity.z};
        for(size_t i=0;i<values.size();++i)numbers_[i]->setText(tableNumber::display(values[i]));
        setVisible(true);
        updateDirection();
    }
    void clear(){
        setProperty("mass",QVariant{});
        hide();
    }
    QSize minimumSizeHint()const override{
        auto size=QFrame::minimumSizeHint();
        const auto margins=layout_->contentsMargins();
        size.setWidth(std::max(layout_->itemAt(0)->minimumSize().width(),layout_->itemAt(1)->minimumSize().width())+
                      margins.left()+margins.right());
        return size;
    }

protected:
    void resizeEvent(QResizeEvent* event)override{
        QFrame::resizeEvent(event);
        updateDirection();
    }

private:
    static QLabel* metricLabel(const char* role,bool bold=false){
        auto* label=new QLabel;
        label->setProperty("loadSummaryRole",role);
        if(bold){
            auto font=label->font();
            font.setWeight(QFont::DemiBold);
            label->setFont(font);
        }
        return label;
    }
    static QLabel* unitLabel(const char* unit){
        auto* label=metricLabel("unit");
        label->setText(unit);
        return label;
    }
    void updateDirection(){
        const auto margins=layout_->contentsMargins();
        const int inlineWidth=layout_->itemAt(0)->sizeHint().width()+layout_->itemAt(1)->sizeHint().width()+
            margins.left()+margins.right()+layout_->spacing();
        const auto direction=width()>=inlineWidth?QBoxLayout::LeftToRight:QBoxLayout::TopToBottom;
        if(direction==layout_->direction())return;
        layout_->setDirection(direction);
        updateGeometry();
    }
    QBoxLayout* layout_=nullptr;
    std::array<QLabel*,4> numbers_{};
};

class Workspace final : public QObject {
public:
    Workspace(QWidget* parent,const QJsonObject& book,QUndoStack& history,
              std::shared_ptr<const fs::ModelSnapshot> model,const QJsonObject& defaults,
              std::function<void(const std::optional<QJsonObject>&)> weightsChanged,
              std::function<void(const std::optional<QJsonArray>&)> criteriaChanged)
        :QObject(parent),history_(history),weightsChanged_(std::move(weightsChanged)),criteriaChanged_(std::move(criteriaChanged)){
        for(auto& page:pages_)page=new QWidget(parent);
        pages_[0]->setProperty("calculation_page","loading_cases");
        pages_[1]->setObjectName("HydroCriteriaPanel");
        auto* work=new QVBoxLayout(pages_[0]);
        auto* split=new QHBoxLayout;
        work->addLayout(split,1);
        caseCanvas_=canvas("LoadCaseCanvas");
        weightCanvas_=canvas("LoadWeightCanvas");
        auto* left=qobject_cast<QVBoxLayout*>(caseCanvas_->widget()->layout());
        auto* right=qobject_cast<QVBoxLayout*>(weightCanvas_->widget()->layout());
        auto* rightColumn=new QVBoxLayout;
        split->addWidget(caseCanvas_,1);split->addLayout(rightColumn,3);
        rightColumn->addWidget(weightCanvas_,1);

        auto* caseTitle=new QLabel;
        bindUiText(caseTitle,"text","工况名");left->addWidget(caseTitle);
        cases_=new CaseList;
        cases_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        bindUiText(cases_,"accessibleName","工况名");
        left->addWidget(cases_);
        auto* addCase=RowActionsTable::actionButton("AddOne","添加",pages_[0]);
        addCase->setObjectName("load_add_cases");
        left->addWidget(addCase,0,Qt::AlignLeft);
        connect(addCase,&QToolButton::clicked,this,[this]{alter(Cases,true,cases_->count());});

        QStringList fields;
        for(const auto& field:fs::loading::itemFields)fields.append(field.title);
        fields.append({QStringLiteral("X (m)"),QStringLiteral("Y (m)"),QStringLiteral("Z (m)")});
        panel(right,Items,"该工况拥有的重量项",fields);
        panel(right,Fixed,"所有工况都具有的重量项",fields);
        left->addStretch();right->addStretch();
        fitCanvasTable(grid_[Items]);
        fitCanvasTable(grid_[Fixed]);

        summary_=new LoadSummary;
        rightColumn->addWidget(summary_);
        errors_[0]=new StudioMessage(StudioMessageRole::Warning);
        errors_[0]->setObjectName("LoadBookWarning");
        rightColumn->addWidget(errors_[0]);
        auto* criteria=new QVBoxLayout(pages_[1]);
        panel(criteria,Criteria,"自定义 GZ 衡准",{"横倾角 (°)","最低 GZ (m)"});
        connect(cases_,&QListWidget::currentRowChanged,this,[this](int row){
            if(painting_)return;
            state_.selected=row;
            paintCase();
        });
        connect(cases_,&QListWidget::itemChanged,this,[this](QListWidgetItem* item){
            if(painting_)return;
            const auto before=state_;
            auto data=rows("cases");const int index=cases_->row(item);auto row=data[index].toArray();
            const auto name=item->text().trimmed();
            row[0]=name;data[index]=row;state_.book["cases"]=data;
            {QScopedValueRollback guard(painting_,true);item->setText(name);item->setToolTip(name);}
            checkpoint(before);
            refreshSummary();
        });
        errors_[0]->hide();
        errors_[1]=new StudioMessage(StudioMessageRole::Warning);
        errors_[1]->setObjectName("HydroCriteriaWarning");
        criteria->addWidget(errors_[1]);errors_[1]->hide();
        auto* sample=new QPushButton;sample->setObjectName("LoadBookTestTemplate");
        bindUiText(sample,"text","测试模板");
        work->addWidget(sample,0,Qt::AlignLeft);
        auto& mode=DeveloperMode::toggle();sample->setVisible(mode.isChecked());
        connect(&mode,&QAction::toggled,sample,&QWidget::setVisible);
        connect(sample,&QPushButton::clicked,this,[this,model,defaults,parent,&mode]{
            if(!mode.isChecked())return;
            const auto example=loadingTemplate::forHull(*model,defaults);
            if(example.book.isEmpty()){showMessage(example.description);return;}
            for(const auto* key:{"cases","shared_items","criteria"}){
                if(rows(key).isEmpty())continue;
                QMessageBox question(QMessageBox::Question,uiText("测试模板"),uiText("替换当前装载工况草稿？"),
                                     QMessageBox::Yes|QMessageBox::No,parent);
                question.setDefaultButton(QMessageBox::No);
                if(runStudioDialog(question)!=QMessageBox::Yes)return;
                break;
            }
            const auto before=state_;auto sampleBook=example.book;
            sampleBook["criteria"]=rows("criteria");load(sampleBook);checkpoint(before);render();
            showMessage(example.description);
        });
        bindUiRefresh(this,[this]{
            QScopedValueRollback guard(painting_,true);
            paintRows(Fixed,rows("shared_items"));
            paintCase();
        });
        load(book);render();
    }
    QWidget* casesPage()const{return pages_[0];}
    QWidget* criteriaPanel()const{return pages_[1];}
    void setEnabled(bool enabled){for(auto* page:pages_)page->setEnabled(enabled);}
    bool criteriaValid()const{return criteriaError_.isEmpty();}
    const QString& weightError()const{return weightError_;}
private:
    void showMessage(const QString& source){message_=source;translateMessage();}
    void translateMessage(){
        for(int i=0;i<int(errors_.size());++i){
            const auto source=i==0?(weightError_.isEmpty()?(message_.isEmpty()?liveError_:message_):weightError_):criteriaError_;
            errors_[i]->setMessage(source.isEmpty()?QString{}:uiText(source),StudioMessageRole::Warning);
            errors_[i]->setVisible(!source.isEmpty());
        }
    }
    bool painting_=false;
    Draft state_;
    QUndoStack& history_;
    std::function<void(const std::optional<QJsonObject>&)> weightsChanged_;
    std::function<void(const std::optional<QJsonArray>&)> criteriaChanged_;
    std::array<QWidget*,2> pages_{};
    std::array<StudioMessage*,2> errors_{};
    std::array<RowActionsTable*,Count> grid_{};
    std::array<QToolButton*,Count> add_{};
    CaseList* cases_=nullptr;
    LoadSummary* summary_=nullptr;
    QScrollArea* caseCanvas_=nullptr;
    QScrollArea* weightCanvas_=nullptr;
    QString message_,liveError_,weightError_,criteriaError_;

    static QScrollArea* canvas(const char* name){
        auto* scroll=new QScrollArea;
        scroll->setObjectName(name);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        applyStudioScrollbars(scroll);
        auto* content=new QWidget;
        auto* layout=new QVBoxLayout(content);
        layout->setContentsMargins(0,0,0,0);
        scroll->setWidget(content);
        return scroll;
    }
    static const char* key(Table t){
        constexpr const char* keys[]={"cases","case_items","shared_items","criteria"};
        return keys[t];
    }
    QJsonArray rows(const char* name)const{return state_.book[name].toArray();}
    QJsonArray caseRow()const{return rows("cases").at(state_.selected).toArray();}
    QJsonArray items()const{return caseRow().at(1).toArray();}
    void load(const QJsonObject& book){
        auto unified=fs::loading::unifiedBook(book,uiText("货物"));
        state_={unified,unified["cases"].toArray().isEmpty()?-1:0};
    }
    void checkpoint(const Draft& before){
        if(before.book==state_.book)return;
        const bool criteriaEdited=before.book["criteria"]!=state_.book.value("criteria");
        const bool weightsEdited=before.book["cases"]!=state_.book.value("cases")||
            before.book["shared_items"]!=state_.book.value("shared_items");
        const auto sync=[this,weightsEdited,criteriaEdited]{
            if(weightsEdited)syncWeights();
            if(criteriaEdited)syncCriteria();
            showMessage({});
        };
        const auto restore=[this,sync](const Draft& d){state_=d;render();sync();};
        const auto after=state_;
        history_.push(new AppliedDraftChange([restore,before]{restore(before);},[restore,after]{restore(after);}));
        sync();
    }
    void syncWeights(){
        auto weights=state_.book;
        weights["criteria"]=QJsonArray{};
        weightError_.clear();
        try{
            fs::loading::validateSavedBook(weights);
        }catch(const std::exception& e){weightError_=QString::fromUtf8(e.what());}
        if(weightError_.isEmpty())weightsChanged_(weights);
        else weightsChanged_(std::nullopt);
    }
    void syncCriteria(){
        QJsonArray criteria;
        criteriaError_.clear();
        for(auto entry:rows("criteria")){
            const auto row=entry.toArray();
            const auto blank=[](const QJsonValue& value){return value.isString()&&value.toString().isEmpty();};
            const bool angleBlank=blank(row.at(0)),armBlank=blank(row.at(1));
            if(angleBlank&&armBlank)continue;
            if(angleBlank||armBlank){
                criteriaError_="请填写横倾角和最低复原力臂。";
                break;
            }
            criteria.append(row);
        }
        if(criteriaError_.isEmpty())try{
            fs::loading::validateCriteria(criteria);
        }catch(const std::exception& e){criteriaError_=QString::fromUtf8(e.what());}
        if(criteriaValid())criteriaChanged_(criteria);
        else criteriaChanged_(std::nullopt);
    }
    void panel(QVBoxLayout* parent,Table t,const QString& title,QStringList headers){
        if(!title.isEmpty()){
            auto* heading=new QLabel;heading->setWordWrap(true);
            if(t==Criteria)heading->setObjectName("HydroCriteriaTitle");
            bindUiText(heading,"text",title);parent->addWidget(heading);
        }
        auto* g=grid_[t]=new RowActionsTable(headers.size(),true);
        g->setObjectName("load_"+QString(key(t)));
        const auto translateHeaders=[g,headers]{
            QStringList translated;
            for(const auto& header:headers)translated.append(header.isEmpty()?QString{}:uiText(header));
            g->setHorizontalHeaderLabels(translated);
        };
        translateHeaders();bindUiRefresh(g,translateHeaders);
        g->setMinimumWidth(0);
        g->setSelectionBehavior(QAbstractItemView::SelectItems);
        g->setSelectionMode(QAbstractItemView::SingleSelection);
        g->setEditTriggers(QAbstractItemView::EditKeyPressed|QAbstractItemView::AnyKeyPressed);
        g->verticalHeader()->setHighlightSections(false);
        g->horizontalHeader()->setHighlightSections(false);
        connect(g,&QTableWidget::cellClicked,g,[g](int row,int column){
            auto* item=g->item(row,column);
            if(item&&item->flags().testFlag(Qt::ItemIsEditable))g->editItem(item);
        });
        auto* header=g->horizontalHeader();
        if(t!=Criteria){
            using namespace fs::loading;
            for(int axis=0;axis<3;++axis)header->moveSection(header->visualIndex(ColumnCount+axis),Z+1+axis);
            const auto groups=[g]{
                const QStringList axes{QStringLiteral("X"),QStringLiteral("Y"),QStringLiteral("Z")};
                setStudioHeaderGroups(g,{
                    {QString(uiText("重心 (%1)")).arg("m"),X,axes},
                    {QString(uiText("底部中心 (%1)")).arg("m"),ColumnCount,axes}
                });
            };
            groups();bindUiRefresh(g,groups);
        }
        {
            g->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter);
            auto* add=add_[t]=g->addHeaderButton("添加");
            add->setObjectName("load_add_"+QString(key(t)));
            connect(add,&QToolButton::clicked,this,[this,t,g]{alter(t,true,g->rowCount());});
        }
        applyStudioScrollbars(g);
        configureStudioDataTable(g);
        parent->addWidget(g,t==Criteria?1:0);
        connect(g,&QTableWidget::cellChanged,this,[this,t](int r,int c){if(!painting_)edit(t,r,c);});
    }
    void fitCanvasTable(RowActionsTable* table){
        table->useCanvas(weightCanvas_);
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        const auto fit=[table]{
            const int scrollbar=table->horizontalScrollBar()->maximum()>0
                ?table->horizontalScrollBar()->sizeHint().height():0;
            const int height=table->horizontalHeader()->height()+table->verticalHeader()->length()+2*table->frameWidth()+scrollbar;
            if(table->height()!=height||table->minimumHeight()!=height)table->setFixedHeight(height);
        };
        const auto queue=[table,fit]{QTimer::singleShot(0,table,fit);};
        connect(table->verticalHeader(),&QHeaderView::geometriesChanged,this,queue);
        connect(table->verticalHeader(),&QHeaderView::sectionResized,this,queue);
        connect(table->horizontalHeader(),&QHeaderView::geometriesChanged,this,queue);
        connect(table->horizontalScrollBar(),&QScrollBar::rangeChanged,this,queue);
        queue();
    }
    void paintRows(Table t,const QJsonArray& data){
        using namespace fs::loading;
        auto* g=grid_[t];
        g->clearRowButtons();g->setRowCount(0);
        std::array<bool,ColumnCount+3> used{};
        for(auto v:data){
            const int row=g->rowCount();g->insertRow(row);
            if(t==Criteria){
                const auto values=v.toArray();
                for(int col=0;col<g->columnCount();++col)g->setItem(row,col,numericItem(values.at(col)));
            }else{
                const auto item=v.toObject();const auto kind=item["kind"].toString();
                for(int col=0;col<g->columnCount();++col){
                    const bool tank=itemShape(kind)>=0;
                    const bool bottom=col>=ColumnCount;
                    const int field=bottom?X+col-ColumnCount:col;
                    const bool position=field>=X&&field<=Z;
                    const bool active=usesItemColumn(kind,ItemColumn(field))&&(!position||tank==bottom);
                    used[col]=used[col]||active;
                    QTableWidgetItem* cell;
                    if(col==Name)cell=new QTableWidgetItem(item["name"].toString());
                    else if(col==Kind)cell=new QTableWidgetItem(uiText(itemTitle(kind)));
                    else if(active)cell=numericItem(item[itemFields[field].key]);
                    else cell=new QTableWidgetItem(QStringLiteral("—"));
                    if(col==Kind||!active)cell->setFlags(cell->flags()&~Qt::ItemIsEditable);
                    g->setItem(row,col,cell);
                }
                // A saved trapezoidal tank keeps its geometry and calculation inputs.
                if(kind!="trapezoids"){
                    auto* combo=new ItemKindCombo;
                    combo->setObjectName("LoadWeightKind");
                    combo->setFont(g->font());
                    combo->setAccessibleName(uiText("种类"));
                    combo->addItem(uiText("固定物体"),"fixed");
                    for(const auto& shape:tankTables())
                        if(shape.shape!=TankShape::Trapezoid)combo->addItem(uiText(shape.title),shape.key);
                    combo->setCurrentIndex(combo->findData(kind));
                    g->setCellWidget(row,Kind,combo);
                    connect(combo,&QComboBox::activated,this,[this,t,row,combo]{
                        const auto before=state_;
                        auto data=t==Items?items():rows("shared_items");
                        auto item=data.at(row).toObject();
                        const auto kind=combo->currentData().toString();
                        if(kind==item["kind"])return;
                        // Retain shape-specific values when a draft row is switched back.
                        const auto defaults=fs::loading::newWeightItem(kind);
                        for(auto value=defaults.begin();value!=defaults.end();++value)
                            if(!item.contains(value.key()))item[value.key()]=value.value();
                        item["kind"]=kind;
                        data[row]=item;putRows(t,data);
                        checkpoint(before);
                        QScopedValueRollback guard(painting_,true);
                        paintRows(t,data);
                        refreshSummary();
                    });
                }
            }
            auto* remove=RowActionsTable::actionButton("DeleteOne","删除",g->viewport());
            remove->setObjectName("LoadBookRemoveRow");
            connect(remove,&QToolButton::clicked,this,[this,t,row]{alter(t,false,row);});
            g->appendRowButton(remove);
        }
        if(t!=Criteria){
            int width=g->fontMetrics().horizontalAdvance(uiText("固定物体"));
            for(int row=0;row<g->rowCount();++row)
                width=std::max(width,g->fontMetrics().horizontalAdvance(g->item(row,Kind)->text()));
            g->horizontalHeaderItem(Kind)->setSizeHint({width+34,0});
            for(int col=Mass;col<g->columnCount();++col)g->setColumnHidden(col,!used[col]&&!(data.isEmpty()&&col<=Z));
        }
        g->placeButtons();
    }
    static QTableWidgetItem* numericItem(const QJsonValue& value){
        auto* item=new NumericTableItem(value.toDouble());
        if(value.isString())item->setData(Qt::EditRole,value.toString());
        return item;
    }
    void paintCase(){
        QScopedValueRollback guard(painting_,true);
        paintRows(Items,items());
        add_[Items]->setEnabled(state_.selected>=0);
        refreshSummary();
    }
    void render(){
        QScopedValueRollback guard(painting_,true);
        paintRows(Fixed,rows("shared_items"));paintRows(Criteria,rows("criteria"));
        cases_->clearCases();
        for(auto value:rows("cases")){
            const auto name=value.toArray().at(0).toString();
            auto* item=new QListWidgetItem(name,cases_);
            item->setFlags(item->flags()|Qt::ItemIsEditable);
            item->setToolTip(name);
            auto* remove=RowActionsTable::actionButton("DeleteOne","删除",cases_->viewport());
            remove->setObjectName("LoadBookRemoveRow");
            const int row=cases_->row(item);
            connect(remove,&QToolButton::clicked,this,[this,row]{alter(Cases,false,row);});
            cases_->addDeleteButton(remove);
        }
        if(state_.selected>=0)cases_->setCurrentRow(state_.selected);
        cases_->doItemsLayout();
        const int height=cases_->count()?cases_->sizeHintForRow(0)*cases_->count():0;
        cases_->setFixedHeight(height);
        cases_->placeButtons();
        paintCase();
    }
    void refreshSummary(){
        liveError_.clear();
        bool valid=false;
        if(state_.selected>=0)try{
            auto book=state_.book;book["cases"]=QJsonArray{caseRow()};book["criteria"]=QJsonArray{};
            const auto load=fs::loading::parse(book).front();const auto g=load.gravity({0,0,1});
            summary_->setValues(load.mass,g);
            valid=true;
        }catch(const std::exception& e){liveError_=QString::fromUtf8(e.what());}
        if(!valid)summary_->clear();
        translateMessage();
    }
    void putRows(Table t,const QJsonArray& data){
        if(t==Items){
            auto cases=rows("cases");auto row=caseRow();row[1]=data;
            cases[state_.selected]=row;state_.book["cases"]=cases;
        }else state_.book[key(t)]=data;
    }
    void edit(Table t,int row,int col){
        using namespace fs::loading;
        if(t!=Criteria&&col==Kind)return;
        const auto before=state_;
        const auto text=grid_[t]->item(row,col)->data(Qt::EditRole).toString().trimmed();
        QJsonValue value=text;
        if(col!=Name||t==Criteria){
            bool ok;double number=text.toDouble(&ok);if(!ok)number=QLocale().toDouble(text,&ok);
            if(ok&&std::isfinite(number))value=number;
        }
        auto data=t==Items?items():rows(key(t));
        if(t==Criteria){
            auto values=data.at(row).toArray();values[col]=value;data[row]=values;
        }else{
            const int field=col>=ColumnCount?X+col-ColumnCount:col;
            auto item=data.at(row).toObject();item[itemFields[field].key]=value;data[row]=item;
        }
        putRows(t,data);checkpoint(before);refreshSummary();
    }
    void alter(Table t,bool adding,int row){
        const auto before=state_;
        if(t==Items&&state_.selected<0)return;
        auto data=t==Items?items():rows(key(t));
        if(adding){
            const int maximum=t==Cases?100:t==Criteria?32:t==Fixed?132:133;
            if(data.size()>=maximum)return;
            if(t==Cases)data.append(QJsonArray{"",QJsonArray{}});
            else if(t==Criteria)data.append(QJsonArray{QString{},QString{}});
            else data.append(fs::loading::newWeightItem("fixed"));
        }else data.removeAt(row);
        putRows(t,data);
        if(t==Cases){
            if(adding)state_.selected=row;
            else if(state_.selected>row)--state_.selected;
            else state_.selected=std::min(state_.selected,int(data.size())-1);
        }
        checkpoint(before);render();
        if(t==Cases){
            cases_->setFocus();
            if(adding){
                caseCanvas_->ensureVisible(0,cases_->y()+cases_->visualItemRect(cases_->item(row)).bottom(),0,24);
                cases_->editItem(cases_->item(row));
            }
            return;
        }
        auto* grid=grid_[t];
        if(grid->rowCount())grid->setCurrentCell(std::min(row,grid->rowCount()-1),0);
        grid->setFocus();
        if(adding){
            grid->editItem(grid->item(row,0));
            if(t==Items||t==Fixed){
                auto* canvas=weightCanvas_;
                QTimer::singleShot(0,grid,[canvas,grid,row]{
                    canvas->widget()->layout()->activate();
                    const auto rect=grid->visualRect(grid->model()->index(row,0));
                    if(!rect.isEmpty())canvas->ensureVisible(0,grid->viewport()->mapTo(canvas->widget(),rect.center()).y(),0,rect.height());
                });
            }
        }
    }
};
}
