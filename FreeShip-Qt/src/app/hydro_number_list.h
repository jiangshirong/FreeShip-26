// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "draft_history.h"
#include "studio_ui.h"
#include "ui_language.h"
#include <QApplication>
#include <QFocusEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMap>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSpinBox>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <functional>
#include <limits>

class HydroValueBlock final : public QToolButton {
public:
    explicit HydroValueBlock(QWidget* parent):QToolButton(parent){}
    QString previewText()const{
        const auto full=text();
        int digits=0;
        qsizetype end=0;
        for(qsizetype i=0;i<full.size();++i){
            if(!full[i].isDigit())continue;
            if(++digits>3)return full.left(end)+QStringLiteral("...");
            end=i+1;
        }
        return full;
    }
    QSize sizeHint()const override{
        const auto metrics=fontMetrics();
        const int textWidth=metrics.horizontalAdvance(previewText());
        return {qMax(24,textWidth+12),30};
    }
    void setDragging(bool dragging){dragging_=dragging;update();}
protected:
    void paintEvent(QPaintEvent*)override{
        QStylePainter painter(this);
        if(dragging_){
            painter.setPen(QPen(QColor("#bfd4cb"),1,Qt::DashLine));
            painter.drawRoundedRect(rect().adjusted(0,0,-1,-1),4,4);
            return;
        }
        QStyleOptionToolButton option;
        initStyleOption(&option);
        const auto metrics=fontMetrics();
        const int available=qMax(0,width()-10);
        const auto preview=previewText();
        const auto label=metrics.horizontalAdvance(preview)<=available?preview:
            metrics.elidedText(preview,Qt::ElideRight,available);
        option.text.clear();
        painter.drawComplexControl(QStyle::CC_ToolButton,option);
        painter.setPen(QColor(isEnabled()?"#225e55":"#7b8582"));
        painter.drawText(rect().adjusted(5,0,-5,0),Qt::AlignCenter|Qt::TextSingleLine,label);
    }
private:
    bool dragging_=false;
};

class HydroInlineValueEdit final : public QLineEdit {
public:
    explicit HydroInlineValueEdit(QWidget* parent):QLineEdit(parent){}
    QSize sizeHint()const override{return {qMax(46,fontMetrics().horizontalAdvance(text())+18),30};}
};

class HydroValueFlow final : public QLayout {
public:
    HydroValueFlow(){setContentsMargins(0,0,0,0);setSpacing(4);}
    ~HydroValueFlow()override{while(auto* item=takeAt(0))delete item;}
    void addItem(QLayoutItem* item)override{items_.append(item);invalidate();}
    int count()const override{return items_.size();}
    QLayoutItem* itemAt(int index)const override{return items_.value(index);}
    QLayoutItem* takeAt(int index)override{
        if(index<0||index>=items_.size())return nullptr;
        auto* item=items_.takeAt(index);invalidate();return item;
    }
    bool hasHeightForWidth()const override{return true;}
    int heightForWidth(int width)const override{return arrange(QRect(0,0,width,0),false);}
    QSize minimumSize()const override{return items_.isEmpty()?QSize(0,0):QSize(24,30);}
    QSize sizeHint()const override{return {240,heightForWidth(240)};}
    void setGeometry(const QRect& rect)override{
        QLayout::setGeometry(rect);arrange(rect,true);
        if(onLayout)onLayout();
    }
    std::function<void()> onLayout;
private:
    int arrange(const QRect& rect,bool place)const{
        int x=0,y=0,rowHeight=0;
        const int available=qMax(1,rect.width());
        for(auto* item:items_){
            const auto size=item->widget()->sizeHint();
            const int width=qMin(available,size.width());
            const int height=size.height();
            if(x>0&&x+width>available){x=0;y+=rowHeight+spacing();rowHeight=0;}
            if(place)item->setGeometry(QRect(rect.x()+x,rect.y()+y,width,height));
            x+=width+spacing();rowHeight=qMax(rowHeight,height);
        }
        return y+rowHeight;
    }
    QList<QLayoutItem*> items_;
};

class HydroNumberList final : public QWidget {
public:
    HydroNumberList(int limit,double minimum,double maximum,QUndoStack& history,QWidget* parent=nullptr)
        :QWidget(parent),limit_(limit),minimum_(minimum),maximum_(maximum),history_(history){
        QSizePolicy policy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        policy.setHeightForWidth(true);setSizePolicy(policy);
        setStyleSheet(
            "QToolButton#HydroValueBlock {"
            " background:#e5efea; border:1px solid #bfd4cb; border-radius:4px; color:#225e55; padding:0 5px; }"
            "QToolButton#HydroValueBlock:hover, QToolButton#HydroValueBlock:focus {"
            " background:#dcebe4; border-color:#6c9d8c; }"
            "QToolButton#HydroValueBlock:pressed { background:#c8dfd5; }"
            "QToolButton#HydroValueBlock:disabled { color:#7b8582; }"
            "QLineEdit#HydroInlineValueEdit { background:#f6faf7; border:1px solid #6c9d8c;"
            " border-radius:4px; color:#225e55; padding:0 5px; }"
            "QWidget#HydroValueDrop { border:none; border-radius:4px; background:transparent; }"
            "QWidget#HydroValueDrop[dragOver=\"true\"] { background:#f9eae5; }"
            "QLabel#HydroValueDropLabel { color:#466960; }"
            "QLabel#HydroValueDropLabel[dragOver=\"true\"] { color:#a02d1e; }");
        auto* root=new QVBoxLayout(this);
        root->setContentsMargins(0,0,0,0);root->setSpacing(5);
        header_=new QHBoxLayout;
        header_->setSpacing(4);
        root->addLayout(header_);
        add_=actionButton(header_,"add",studioIcon("AddOne"));
        range_=actionButton(header_,"range",studioIcon("AddRange"));
        flow_=new HydroValueFlow;
        root->addLayout(flow_);

        edit_=new HydroInlineValueEdit(this);
        edit_->setObjectName("HydroInlineValueEdit");
        edit_->setProperty("hydro_list_value",true);edit_->setProperty("ui_user_text",true);
        edit_->setFixedHeight(30);edit_->setMaxLength(80);edit_->installEventFilter(this);edit_->hide();
        flow_->onLayout=[this]{placeEditor();};
        createRangeEditor(root);

        drop_=new QWidget;
        drop_->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Preferred);
        drop_->setObjectName("HydroValueDrop");drop_->setProperty("hydro_list_action","drop");
        auto* dropLine=new QHBoxLayout(drop_);
        dropLine->setContentsMargins(6,3,6,3);dropLine->setSpacing(5);
        auto* trash=new QLabel;trash->setPixmap(studioIcon("DeleteOne").pixmap(14,14));
        dropLabel_=new QLabel;
        dropLabel_->setObjectName("HydroValueDropLabel");dropLabel_->setWordWrap(true);dropLabel_->setMinimumWidth(0);
        dropLine->addWidget(trash,0,Qt::AlignVCenter);
        dropLine->addWidget(dropLabel_,1);
        header_->addSpacing(6);
        header_->addWidget(drop_,0,Qt::AlignVCenter);
        header_->addStretch();

        ghost_=new QLabel(this);ghost_->setObjectName("HydroValueGhost");
        ghost_->setAttribute(Qt::WA_TransparentForMouseEvents);ghost_->setAlignment(Qt::AlignCenter);ghost_->hide();
        dragScroll_.setInterval(30);
        connect(&dragScroll_,&QTimer::timeout,this,[this]{scrollAtDragEdge();});
        connect(add_,&QToolButton::clicked,this,[this]{startEdit(entries_.size());});
        connect(range_,&QToolButton::clicked,this,[this]{showRangeEditor();});
        connect(edit_,&QLineEdit::textChanged,this,[this]{
            if(!isEditing())return;
            if(editIndex_==entries_.size()){edit_->updateGeometry();flow_->invalidate();}
            else placeEditor();
            notify(false);
        });
        translate();bindUiRefresh(this,[this]{translate();});
        setFocusProxy(add_);
        updateControls();
    }

    void setValues(const QJsonValue& source){
        history_.clear();
        cancelDrag();
        closeEditor(false);
        rangePanel_->hide();
        entries_=source.toArray();
        if(source.isString()){
            auto text=source.toString();text.replace(QChar(0xff0c),',');text.replace(';',',');
            for(const auto& part:text.split(',',Qt::SkipEmptyParts))entries_.append(parse(part.trimmed()));
        }
        rebuild();
        notify();
    }

    QJsonArray values()const{return entries_;}
    bool isEditing()const{return editIndex_>=0;}
    QJsonArray validationValues()const{
        auto result=entries_;
        if(isEditing()){
            const auto value=editedValue();
            if(editIndex_==result.size())result.append(value);
            else result[editIndex_]=value;
        }
        return result;
    }

    void setAccessibleLabel(const QString& label){
        setAccessibleName(label);
        updateValueNames();
    }
    void setCaption(QWidget* caption){
        caption->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Preferred);
        header_->insertWidget(0,caption,0,Qt::AlignVCenter);
    }
    std::function<void(bool)> onChanged;

protected:
    bool eventFilter(QObject* watched,QEvent* event)override{
        if(watched==edit_){
            if(event->type()==QEvent::KeyPress){
                const int key=static_cast<QKeyEvent*>(event)->key();
                if(key==Qt::Key_Return||key==Qt::Key_Enter||key==Qt::Key_Escape){
                    endEdit(key!=Qt::Key_Escape,true);return true;
                }
            }
            if(event->type()==QEvent::FocusOut&&static_cast<QFocusEvent*>(event)->reason()!=Qt::PopupFocusReason)
                endEdit(false,false);
            return QWidget::eventFilter(watched,event);
        }
        if(watched==rangeStart_||watched==rangeEnd_||watched==rangeCount_){
            if(event->type()==QEvent::KeyPress){
                const int key=static_cast<QKeyEvent*>(event)->key();
                if(key==Qt::Key_Escape){closeRangeEditor(false);return true;}
                if(key==Qt::Key_Return||key==Qt::Key_Enter){
                    if(rangeApply_->isEnabled())closeRangeEditor(true);
                    return true;
                }
            }
            return QWidget::eventFilter(watched,event);
        }
        auto* block=dynamic_cast<HydroValueBlock*>(watched);
        if(!block)return QWidget::eventFilter(watched,event);
        if(event->type()==QEvent::KeyPress&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Delete){
            removeValue(block->property("hydro_value_index").toInt());return true;
        }
        if(event->type()==QEvent::MouseButtonPress){
            const auto* mouse=static_cast<QMouseEvent*>(event);
            if(mouse->button()==Qt::LeftButton){
                pressed_=block;pressPosition_=mouse->globalPosition().toPoint();
            }
        }else if(event->type()==QEvent::MouseMove&&pressed_==block){
            const auto* mouse=static_cast<QMouseEvent*>(event);
            const auto global=mouse->globalPosition().toPoint();
            if(mouse->buttons().testFlag(Qt::LeftButton)&&
               (dragging_||(global-pressPosition_).manhattanLength()>=QApplication::startDragDistance())){
                if(!dragging_){
                    dragging_=true;
                    block->setDown(false);
                    block->setCursor(Qt::ClosedHandCursor);
                    ghost_->setPixmap(block->grab());ghost_->resize(block->size());
                    dragOffset_=block->mapFromGlobal(pressPosition_);
                    block->setDragging(true);
                    ghost_->raise();ghost_->show();
                    dragScroll_.start();
                }
                dragPosition_=global;
                updateDragPosition();
                return true;
            }
        }else if(event->type()==QEvent::MouseButtonRelease&&pressed_==block){
            const auto* mouse=static_cast<QMouseEvent*>(event);
            if(mouse->button()==Qt::LeftButton&&dragging_){
                const bool remove=overDrop(mouse->globalPosition().toPoint());
                const int index=block->property("hydro_value_index").toInt();
                cancelDrag();
                if(remove)removeValue(index);
                return true;
            }
            cancelDrag();
        }else if(event->type()==QEvent::UngrabMouse||event->type()==QEvent::Hide){
            if(pressed_==block)cancelDrag();
        }
        return QWidget::eventFilter(watched,event);
    }

private:
    static QToolButton* actionButton(QHBoxLayout* line,const char* action,const QIcon& icon={}){
        auto* button=new QToolButton;
        button->setProperty("hydro_list_action",action);
        button->setIcon(icon);button->setIconSize({17,17});button->setFixedSize(28,30);
        line->addWidget(button,0,Qt::AlignTop);
        return button;
    }
    static QJsonValue parse(const QString& text){
        bool ok=false;const auto value=QLocale::c().toDouble(text,&ok);
        return ok&&std::isfinite(value)&&!text.contains(',')?QJsonValue(value):QJsonValue(text);
    }
    template<class T> static QString shortest(T number){
        char buffer[64];const auto r=std::to_chars(buffer,buffer+sizeof(buffer),number);
        return QString::fromLatin1(buffer,int(r.ptr-buffer));
    }
    static QString display(const QJsonValue& value){
        if(!value.isDouble())return value.toString();
        const auto v=value.toDouble();
        const auto text=shortest(v);
        const auto single=static_cast<float>(v);
        if(double(single)==v){
            const auto compact=shortest(single);
            if(compact.size()+4<text.size())return compact;
        }
        return text;
    }
    void createRangeEditor(QVBoxLayout* root){
        rangePanel_=new QWidget;
        auto* form=new QFormLayout(rangePanel_);
        form->setContentsMargins(0,3,0,3);form->setSpacing(4);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        const auto label=[]{
            auto* widget=new QLabel;widget->setWordWrap(true);return widget;
        };
        const auto input=[](const char* action){
            auto* widget=new QLineEdit;
            widget->setProperty("hydro_list_action",action);
            widget->setProperty("ui_user_text",true);widget->setMaxLength(80);
            return widget;
        };
        rangeStartLabel_=label();rangeEndLabel_=label();rangeCountLabel_=label();rangeStepLabel_=label();
        rangeStart_=input("range_start");rangeEnd_=input("range_end");
        rangeCount_=new QSpinBox;rangeCount_->setRange(2,limit_);
        rangeCount_->setProperty("hydro_list_action","range_count");
        rangeStep_=label();rangeStep_->setProperty("hydro_list_action","range_step");
        rangeError_=label();rangeError_->setStyleSheet("color:#a02d1e;");
        rangeStartLabel_->setBuddy(rangeStart_);rangeEndLabel_->setBuddy(rangeEnd_);
        rangeCountLabel_->setBuddy(rangeCount_);
        form->addRow(rangeStartLabel_,rangeStart_);form->addRow(rangeEndLabel_,rangeEnd_);
        form->addRow(rangeCountLabel_,rangeCount_);form->addRow(rangeStepLabel_,rangeStep_);
        form->addRow(rangeError_);
        auto* actions=new QHBoxLayout;
        actions->addStretch();
        rangeApply_=actionButton(actions,"range_apply",studioIcon("MessageSuccess"));
        rangeCancel_=actionButton(actions,"range_cancel");rangeCancel_->setText(QStringLiteral("×"));
        form->addRow(actions);
        for(QWidget* widget:{static_cast<QWidget*>(rangeStart_),static_cast<QWidget*>(rangeEnd_),static_cast<QWidget*>(rangeCount_)})
            widget->installEventFilter(this);
        connect(rangeStart_,&QLineEdit::textChanged,this,[this]{updateRangePreview();});
        connect(rangeEnd_,&QLineEdit::textChanged,this,[this]{updateRangePreview();});
        connect(rangeCount_,qOverload<int>(&QSpinBox::valueChanged),this,[this]{updateRangePreview();});
        connect(rangeApply_,&QToolButton::clicked,this,[this]{closeRangeEditor(true);});
        connect(rangeCancel_,&QToolButton::clicked,this,[this]{closeRangeEditor(false);});
        root->addWidget(rangePanel_);rangePanel_->hide();
    }
    void showRangeEditor(){
        rangeStart_->setText(entries_.size()>1?display(entries_.first()):QString{});
        rangeEnd_->setText(entries_.size()>1?display(entries_.last()):QString{});
        rangeCount_->setValue(qMin(11,limit_));
        rangePanel_->show();updateRangePreview();updateControls();
        focusValue(rangeStart_);rangeStart_->selectAll();
    }
    void updateRangePreview(){
        rangePreview_={};
        const auto endpoint=[this](const QLineEdit* field,bool first){
            const auto text=field->text().trimmed();
            if(entries_.size()>1){
                const QJsonValue original=first?entries_.first():entries_.last();
                if(original.isDouble()&&display(original)==text)return original;
            }
            return parse(text);
        };
        const auto first=endpoint(rangeStart_,true),last=endpoint(rangeEnd_,false);
        const double start=first.toDouble(),end=last.toDouble();
        const int count=rangeCount_->value();
        QString issue;
        double step=0;
        if(!rangeStart_->text().isEmpty()&&!rangeEnd_->text().isEmpty()){
            if(!first.isDouble()||!last.isDouble()||start<minimum_||end>maximum_||start>=end)
                issue=QString(uiText("数值须在 %1 至 %2 之间，并严格递增。")).arg(shortest(minimum_),shortest(maximum_));
            else{
                step=(end-start)/(count-1);
                QList<double> numbers;
                QJsonArray drafts;
                for(const auto& entry:entries_){
                    if(entry.isDouble())numbers.append(entry.toDouble());
                    else drafts.append(entry);
                }
                const double tolerance=qMin(step*.25,4*std::numeric_limits<double>::epsilon()*qMax(std::abs(start),std::abs(end)));
                double previous=start;
                for(int i=0;i<count;++i){
                    const double value=i==count-1?end:start+step*i;
                    if(i>0&&value<=previous){issue=uiText("间隔过小，无法生成不同的数值。");break;}
                    previous=value;
                    if(std::none_of(numbers.cbegin(),numbers.cend(),[&](double current){return std::abs(current-value)<=tolerance;}))
                        numbers.append(value);
                }
                if(issue.isEmpty()){
                    std::sort(numbers.begin(),numbers.end());
                    numbers.erase(std::unique(numbers.begin(),numbers.end()),numbers.end());
                    if(numbers.size()+drafts.size()>limit_)issue=QString(uiText("此列表最多可填 %1 项。")).arg(limit_);
                    else{
                        for(double value:numbers)rangePreview_.append(value);
                        for(const auto& draft:drafts)rangePreview_.append(draft);
                    }
                }
            }
        }
        rangeStep_->setText(step>0?QString::number(step,'g',10):QStringLiteral("—"));
        rangeStep_->setToolTip(step>0?shortest(step):QString{});
        rangeError_->setText(issue);rangeError_->setVisible(!issue.isEmpty());
        rangeApply_->setEnabled(!rangePreview_.isEmpty());
    }
    void closeRangeEditor(bool apply){
        if(apply&&rangePreview_.isEmpty())return;
        rangePanel_->hide();
        if(apply){
            const auto before=entries_;
            entries_=rangePreview_;
            rebuild();recordChange(before,-1);notify();
        }else updateControls();
        focusValue(range_);
    }
    void rebuild(){
        for(auto* block:blocks_){flow_->removeWidget(block);block->hide();block->deleteLater();}
        blocks_.clear();
        // Keep close but unequal legacy values distinguishable.
        QMap<QString,double> seen;
        QSet<QString> ambiguous;
        for(const auto& value:entries_)if(value.isDouble()){
            const auto text=display(value);
            if(seen.contains(text)&&seen.value(text)!=value.toDouble())ambiguous.insert(text);
            seen[text]=value.toDouble();
        }
        for(int i=0;i<entries_.size();++i){
            const auto value=entries_[i];
            auto label=display(value);
            if(value.isDouble()&&ambiguous.contains(label))label=shortest(value.toDouble());
            auto* block=new HydroValueBlock(this);
            block->setObjectName("HydroValueBlock");block->setProperty("hydro_value_index",i);
            block->setText(label);block->setToolTip(value.isDouble()?shortest(value.toDouble()):label);
            block->setCursor(Qt::OpenHandCursor);
            block->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);block->setMinimumWidth(20);
            block->setFixedHeight(30);block->installEventFilter(this);
            flow_->addWidget(block);
            blocks_.append(block);
            connect(block,&QToolButton::clicked,this,[this,i]{startEdit(i);});
        }
        updateValueNames();updateControls();
    }
    void updateControls(){
        drop_->setVisible(!blocks_.isEmpty());
        const bool editingRange=!rangePanel_->isHidden();
        add_->setEnabled(entries_.size()<limit_&&editIndex_<0&&!editingRange);
        range_->setEnabled(editIndex_<0&&!editingRange);
    }
    void translate(){
        const auto caption=[](QToolButton* button,const QString& text){button->setToolTip(text);button->setAccessibleName(text);};
        caption(add_,uiText("添加数值"));
        caption(range_,uiText("批量添加数值"));
        dropLabel_->setText(uiText("拖动到此区域删除"));
        rangeStartLabel_->setText(uiText("起始值"));
        rangeEndLabel_->setText(uiText("终止值"));
        rangeCountLabel_->setText(uiText("数量（含起点和终点）"));
        rangeStepLabel_->setText(uiText("等分间隔"));
        caption(rangeApply_,uiText("应用"));caption(rangeCancel_,uiText("取消"));
        updateRangePreview();
    }
    void updateValueNames(){
        for(int i=0;i<blocks_.size();++i)blocks_[i]->setAccessibleName(accessibleName()+" "+blocks_[i]->text());
        edit_->setAccessibleName(accessibleName());
    }
    QJsonValue editedValue()const{return parse(edit_->text().trimmed());}
    void placeEditor(){
        if(!isEditing()||editIndex_>=blocks_.size())return;
        auto* block=blocks_[editIndex_];
        const auto area=flow_->geometry();
        if(area.width()<=0)return;
        const int width=qMin(area.width(),qMax(block->width(),edit_->sizeHint().width()));
        const int left=qBound(area.left(),block->x(),area.right()+1-width);
        edit_->setGeometry(left,block->y(),width,block->height());
    }
    void startEdit(int index){
        if(index<0||index>entries_.size()||(index==entries_.size()&&entries_.size()>=limit_))return;
        if(!rangePanel_->isHidden())closeRangeEditor(false);
        if(editIndex_==index){focusValue(edit_);return;}
        closeEditor(false);
        edit_->setText(index<blocks_.size()?blocks_[index]->toolTip():QString{});
        editIndex_=index;
        if(index==entries_.size())flow_->addWidget(edit_);
        else placeEditor();
        edit_->show();edit_->raise();updateControls();
        focusValue(edit_);edit_->selectAll();notify(false);
    }
    void closeEditor(bool accept){
        if(!isEditing())return;
        const int index=std::exchange(editIndex_,-1);
        const bool adding=index==entries_.size();
        if(adding)flow_->removeWidget(edit_);
        edit_->hide();
        if(!accept)return;
        const auto value=editedValue();
        if(adding)entries_.append(value);
        else entries_[index]=value;
    }
    void endEdit(bool accept,bool restoreFocus){
        if(!isEditing()||(accept&&!editedValue().isDouble()))return;
        const auto before=entries_;
        const int index=editIndex_;
        closeEditor(accept);
        const bool changed=before!=entries_;
        if(changed){rebuild();recordChange(before,index);}
        else updateControls();
        if(restoreFocus)focusValue(index<blocks_.size()?blocks_[index]:add_);
        notify(changed);
    }
    void removeValue(int index){
        if(index<0||index>=entries_.size())return;
        closeEditor(false);
        const auto before=entries_;
        entries_.removeAt(index);
        rebuild();
        recordChange(before,index);
        if(!rangePanel_->isHidden())updateRangePreview();
        focusValue(entries_.isEmpty()?add_:blocks_[qMin(index,int(entries_.size())-1)]);
        notify();
    }
    void recordChange(const QJsonArray& before,int index){
        if(before==entries_)return;
        const auto restore=[this,index](const QJsonArray& values){
            cancelDrag();closeEditor(false);rangePanel_->hide();
            entries_=values;rebuild();
            if(isVisible())
                focusValue(index>=0&&!blocks_.isEmpty()?blocks_[qMin(index,int(blocks_.size())-1)]:add_);
            notify();
        };
        history_.push(new AppliedDraftChange([restore,before]{restore(before);},
                                             [restore,after=entries_]{restore(after);}));
    }
    QScrollArea* parameterScroll()const{
        for(auto* parent=parentWidget();parent;parent=parent->parentWidget())
            if(auto* scroll=qobject_cast<QScrollArea*>(parent))return scroll;
        return nullptr;
    }
    void focusValue(QWidget* widget){
        widget->setFocus();
        QTimer::singleShot(0,widget,[this,widget]{
            if(auto* scroll=parameterScroll();scroll&&widget->hasFocus())scroll->ensureWidgetVisible(widget,0,0);
        });
    }
    void updateDragPosition(){
        ghost_->move(mapFromGlobal(dragPosition_)-dragOffset_);
        setDropHover(overDrop(dragPosition_));
    }
    void scrollAtDragEdge(){
        auto* scroll=parameterScroll();
        if(!scroll)return;
        const auto rect=scroll->viewport()->rect();
        const auto position=scroll->viewport()->mapFromGlobal(dragPosition_);
        if(!rect.contains(position))return;
        constexpr int edge=24;
        auto* bar=scroll->verticalScrollBar();
        if(position.y()<edge)bar->setValue(bar->value()-12);
        else if(position.y()>rect.bottom()-edge)bar->setValue(bar->value()+12);
        updateDragPosition();
    }
    bool overDrop(QPoint global)const{
        if(const auto* scroll=parameterScroll();scroll&&
           !scroll->viewport()->rect().contains(scroll->viewport()->mapFromGlobal(global)))return false;
        return drop_->isVisible()&&drop_->rect().contains(drop_->mapFromGlobal(global));
    }
    void setDropHover(bool hover){
        if(hover==dropHovered_)return;
        dropHovered_=hover;
        for(QWidget* widget:{drop_,static_cast<QWidget*>(dropLabel_)}){
            widget->setProperty("dragOver",hover);
            widget->style()->unpolish(widget);widget->style()->polish(widget);widget->update();
        }
    }
    void cancelDrag(){
        if(pressed_){
            pressed_->setCursor(Qt::OpenHandCursor);
            pressed_->setDragging(false);
        }
        pressed_=nullptr;dragging_=false;
        dragScroll_.stop();
        ghost_->hide();setDropHover(false);
    }
    void notify(bool committed=true){if(onChanged)onChanged(committed);}
    int limit_,editIndex_=-1;
    double minimum_,maximum_;
    QUndoStack& history_;
    QJsonArray entries_;
    QList<QToolButton*> blocks_;
    QWidget *rangePanel_,*drop_;
    QHBoxLayout* header_;
    HydroValueFlow* flow_;
    QToolButton *add_,*range_,*rangeApply_,*rangeCancel_;
    HydroValueBlock* pressed_=nullptr;
    QLabel *dropLabel_,*ghost_,*rangeStartLabel_,*rangeEndLabel_,*rangeCountLabel_,*rangeStepLabel_,*rangeStep_,*rangeError_;
    HydroInlineValueEdit* edit_;
    QLineEdit *rangeStart_,*rangeEnd_;
    QSpinBox* rangeCount_;
    QJsonArray rangePreview_;
    QPoint pressPosition_,dragPosition_,dragOffset_;
    QTimer dragScroll_;
    bool dragging_=false,dropHovered_=false;
};
