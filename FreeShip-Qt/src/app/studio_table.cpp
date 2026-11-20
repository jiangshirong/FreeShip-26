// SPDX-License-Identifier: GPL-2.0-or-later
#include "studio_table.h"
#include "math_notation.h"
#include <QAbstractItemModel>
#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPointer>
#include <QScrollBar>
#include <QTableView>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
constexpr double LowestScale=.2,HighestScale=2.5;
class TableNavigation final:public QObject {
public:
    explicit TableNavigation(QTableView* table,bool matchHeaderFont,int wrapColumn,bool fitRowHeaders):QObject(table),table_(table),font_(table->font()),
        headerFont_(table->horizontalHeader()->font()),matchHeaderFont_(matchHeaderFont),wrapColumn_(wrapColumn),fitRowHeaders_(fitRowHeaders){
        setObjectName("StudioTableNavigation");table_->setProperty("studio_table_navigation",true);
        table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        table_->horizontalScrollBar()->setSingleStep(12);table_->verticalScrollBar()->setSingleStep(12);
        table_->installEventFilter(this);table_->viewport()->installEventFilter(this);
        connect(table_->horizontalHeader(),&QHeaderView::sectionResized,this,[this](int column,int,int width){
            if(!applying_&&column>=0&&column<widths_.size()){
                if(column==wrapColumn_){
                    const int minimum=wrapWidth(scale_,0);
                    if(width<minimum){
                        applying_=true;table_->setColumnWidth(column,minimum);applying_=false;
                        width=table_->columnWidth(column);
                    }
                }
                widths_[column]=width/scale_;
                if(column==wrapColumn_)resizeWrappedRows();
                publish();
            }
        });
        queue();
    }
    void refresh(){++measureGeneration_;dirty_=true;queue();}
protected:
    bool eventFilter(QObject* watched,QEvent* event)override{
        if(applying_)return false;
        if(watched==table_){
            if(event->type()==QEvent::Show||event->type()==QEvent::LayoutRequest)queue();
            if(event->type()==QEvent::Resize){
                publish();
            }
            if(event->type()==QEvent::FontChange){font_=table_->font();scale_=1;refresh();}
        }
        if(watched!=table_->viewport())return false;
        if(event->type()==QEvent::Wheel){
            auto* wheel=static_cast<QWheelEvent*>(event);
            if(!wheel->modifiers().testFlag(Qt::ControlModifier))return false;
            if(!table_->model()||table_->model()->rowCount()==0||table_->model()->columnCount()==0)return false;
            if(dirty_||model_!=table_->model())measure();
            const double delta=wheel->pixelDelta().isNull()?wheel->angleDelta().y()/120.:wheel->pixelDelta().y()/80.;
            if(delta==0)return false;
            applyScale(std::clamp(scale_*std::pow(1.12,delta),std::min(scale_,minimumScale()),HighestScale),wheel->position());
            wheel->accept();return true;
        }
        if(event->type()==QEvent::MouseButtonPress){
            const auto* mouse=static_cast<QMouseEvent*>(event);
            if(mouse->button()==Qt::RightButton){pressed_=true;dragging_=false;press_=mouse->position();
                scroll_=QPoint(table_->horizontalScrollBar()->value(),table_->verticalScrollBar()->value());}
        }
        if(event->type()==QEvent::MouseMove&&pressed_){
            const auto* mouse=static_cast<QMouseEvent*>(event);
            if(!mouse->buttons().testFlag(Qt::RightButton)){finishDrag();return false;}
            const auto delta=mouse->position()-press_;
            if(!dragging_&&delta.manhattanLength()>=QApplication::startDragDistance()&&
               (table_->horizontalScrollBar()->maximum()>0||table_->verticalScrollBar()->maximum()>0)){
                dragging_=true;savedCursor_=table_->viewport()->cursor();table_->viewport()->setCursor(Qt::ClosedHandCursor);
            }
            if(dragging_){table_->horizontalScrollBar()->setValue(scroll_.x()-qRound(delta.x()));
                table_->verticalScrollBar()->setValue(scroll_.y()-qRound(delta.y()));return true;}
        }
        if(event->type()==QEvent::MouseButtonRelease&&static_cast<QMouseEvent*>(event)->button()==Qt::RightButton){
            const bool consumed=dragging_;finishDrag();
            if(consumed){suppressContext_=true;QTimer::singleShot(300,this,[this]{suppressContext_=false;});return true;}
        }
        if(event->type()==QEvent::ContextMenu&&suppressContext_)return true;
        if(event->type()==QEvent::Hide)finishDrag();
        return false;
    }
private:
    void queue(){
        if(queued_)return;queued_=true;
        QTimer::singleShot(0,this,[this]{queued_=false;if(!table_->isVisible())return;if(model_!=table_->model())bindModel();if(dirty_)measure();});
    }
    void bindModel(){
        for(const auto& connection:connections_)disconnect(connection);connections_.clear();
        model_=table_->model();dirty_=true;
        if(!model_)return;
        const auto changed=[this]{refresh();};
        connections_<<connect(model_,&QAbstractItemModel::modelReset,this,changed)
            <<connect(model_,&QAbstractItemModel::dataChanged,this,changed)
            <<connect(model_,&QAbstractItemModel::headerDataChanged,this,changed)
            <<connect(model_,&QAbstractItemModel::rowsInserted,this,changed)
            <<connect(model_,&QAbstractItemModel::rowsRemoved,this,changed)
            <<connect(model_,&QAbstractItemModel::columnsInserted,this,changed)
            <<connect(model_,&QAbstractItemModel::columnsRemoved,this,changed)
            <<connect(model_,&QAbstractItemModel::layoutChanged,this,changed);
    }
    static double textWidth(const QString& text,const QFont& font){
        const bool numeric=!text.isEmpty()&&std::all_of(text.cbegin(),text.cend(),[](QChar c){return (c>='0'&&c<='9')||c=='.'||c=='-'||c=='+';});
        if(!numeric&&notation::contains(text))return notation::size(text,font).width();
        double width=0;const QFontMetricsF metrics(font);
        for(const auto& line:text.split('\n'))width=std::max(width,metrics.horizontalAdvance(line));
        return width;
    }
    void measure(){
        if(model_!=table_->model())bindModel();
        if(!model_)return;
        dirty_=false;widths_.fill(0,model_->columnCount());
        const QFontMetricsF metrics(font_);
        rowHeight_=std::max(28.,metrics.height()+12.);visibleRows_=0;
        headingTextHeight_=0;
        rowHeaders_.clear();
        if(model_->rowCount()>512&&wrapColumn_<0&&!fitRowHeaders_){
            visibleRows_=model_->rowCount()-table_->verticalHeader()->hiddenSectionCount();
            for(int column=0;column<widths_.size();++column){
                if(table_->isColumnHidden(column))continue;
                const auto size=notation::size(model_->headerData(column,Qt::Horizontal).toString(),headerFont());
                widths_[column]=std::max(int(std::ceil(size.width()))+14+headerControlWidth(column),headerWidthHint(column));
                headingTextHeight_=std::max(double(size.height()),headingTextHeight_);
            }
            fitHeadingGroup();
            measureRow_=0;table_->setProperty("studio_table_measure_pending",true);
            applyScale(std::clamp(scale_,LowestScale,HighestScale),QPointF{},true);
            measureChunk(++measureGeneration_);return;
        }
        for(int row=0;row<model_->rowCount();++row)if(!table_->isRowHidden(row)){
            ++visibleRows_;
            if(fitRowHeaders_)rowHeaders_.append(model_->headerData(row,Qt::Vertical).toString());
        }
        for(int column=0;column<widths_.size();++column){
            if(table_->isColumnHidden(column))continue;
            const auto caption=model_->headerData(column,Qt::Horizontal).toString();
            const auto headingSize=notation::size(caption,headerFont());
            double width=headingSize.width()+headerControlWidth(column);
            headingTextHeight_=std::max(double(headingSize.height()),headingTextHeight_);
            for(int row=0;row<model_->rowCount();++row){
                if(table_->isRowHidden(row)||table_->columnSpan(row,column)>1)continue;
                const auto index=model_->index(row,column);
                const auto ownFont=index.data(Qt::FontRole);
                width=std::max(width,textWidth(index.data().toString(),ownFont.isValid()?qvariant_cast<QFont>(ownFont).resolve(font_):font_)
                                     +cellControlWidth(column));
            }
            // Six logical pixels on each side plus the grid stroke. No fixed
            // minimum such as 112 px and no stretching into unused canvas.
            widths_[column]=std::max(int(std::ceil(width))+14,headerWidthHint(column));
        }
        fitHeadingGroup();
        applyScale(std::clamp(scale_,LowestScale,HighestScale),QPointF{},true);
    }
    void fitHeadingGroup(){
        const auto* header=table_->horizontalHeader();
        const auto title=header->property("studio_header_group_title").toString();
        if(title.isEmpty())return;
        const int first=header->property("studio_header_group_first").toInt();
        const int last=std::min(int(widths_.size()),first+header->property("studio_header_group_count").toInt());
        if(first<0||first>=last)return;
        double available=0;
        for(int col=first;col<last;++col)available+=widths_[col];
        const double extra=(textWidth(title,headerFont())+14-available)/(last-first);
        if(extra>0)for(int col=first;col<last;++col)widths_[col]+=extra;
    }
    int headerControlWidth(int column)const{
        return column==0?table_->horizontalHeader()->property("studio_first_header_control_width").toInt():0;
    }
    int cellControlWidth(int column)const{
        return column==0?table_->property("studio_first_column_control_width").toInt():0;
    }
    int headerWidthHint(int column)const{
        return model_->headerData(column,Qt::Horizontal,Qt::SizeHintRole).toSize().width();
    }
    void measureChunk(quint64 generation){
        if(generation!=measureGeneration_||!model_)return;
        if(!table_->isVisible()){dirty_=true;table_->setProperty("studio_table_measure_pending",false);return;}
        QElapsedTimer budget;budget.start();bool changed=false;
        while(measureRow_<model_->rowCount()){
            const int row=measureRow_++;
            if(!table_->isRowHidden(row))for(int column=0;column<widths_.size();++column){
                if(table_->isColumnHidden(column)||table_->columnSpan(row,column)>1)continue;
                const auto index=model_->index(row,column);const auto font=index.data(Qt::FontRole);
                const auto width=std::ceil(textWidth(index.data().toString(),font.isValid()?qvariant_cast<QFont>(font).resolve(font_):font_))
                                 +14+cellControlWidth(column);
                if(width>widths_[column]){widths_[column]=width;changed=true;}
            }
            if(budget.elapsed()>=4)break;
        }
        if(changed)applyScale(scale_,QPointF{},true);
        table_->setProperty("studio_table_measured_rows",measureRow_);
        table_->setProperty("studio_table_measure_max_ms",std::max(table_->property("studio_table_measure_max_ms").toDouble(),budget.nsecsElapsed()/1e6));
        const bool pending=measureRow_<model_->rowCount();table_->setProperty("studio_table_measure_pending",pending);
        if(pending)QTimer::singleShot(8,this,[this,generation]{measureChunk(generation);});
    }
    double contentWidth()const{double width=0;for(const auto value:widths_)width+=value;return width;}
    int headerHeight(double scale)const{
        const double fontHeight=QFontMetricsF(scaled(headerFont(),scale)).height();
        const double padding=std::max(4.,12*scale);
        const bool grouped=!table_->horizontalHeader()->property("studio_header_group_title").toString().isEmpty();
        return qCeil(std::max(headingTextHeight_*scale,fontHeight)+padding+(grouped?fontHeight+padding:0));
    }
    int rowHeaderWidth(double scale)const{
        const auto font=scaled(font_,scale);
        double width=0;
        for(const auto& label:rowHeaders_)width=std::max(width,textWidth(label,font));
        return qCeil(width)+2*qMax(1,qRound(6*scale))+2;
    }
    double contentHeight()const{
        return heightAt(scale_)/scale_;
    }
    int wrapWidth(double scale,double width)const{
        const auto caption=model_->headerData(wrapColumn_,Qt::Horizontal).toString();
        return std::max(qCeil(width*scale),qCeil(textWidth(caption,scaled(headerFont(),scale))+12*scale));
    }
    int wrappedRowHeight(int row,double scale,int width,const QFont& font)const{
        const int base=qCeil(rowHeight_*scale);
        const auto index=model_->index(row,wrapColumn_);
        if(table_->columnSpan(row,wrapColumn_)>1)return base;
        const auto text=index.data().toString();
        const int padding=std::max(4,qCeil(10*scale));
        if(textWidth(text,font)+padding<=width)return base;
        return std::max(base,qCeil(notation::size(text,font,std::max(1,width-padding)).height())+padding);
    }
    int heightAt(double scale)const{
        if(!model_||wrapColumn_<0||wrapColumn_>=widths_.size())return qCeil(rowHeight_*scale)*visibleRows_;
        const int width=wrapWidth(scale,widths_[wrapColumn_]);const auto font=scaled(font_,scale);
        int height=0;
        for(int row=0;row<model_->rowCount();++row)
            if(!table_->isRowHidden(row))height+=wrappedRowHeight(row,scale,width,font);
        return height;
    }
    void resizeWrappedRows(){
        if(wrapColumn_<0||wrapColumn_>=widths_.size())return;
        const int width=table_->columnWidth(wrapColumn_);const auto font=table_->font();
        for(int row=0;row<model_->rowCount();++row)
            if(!table_->isRowHidden(row))table_->setRowHeight(row,wrappedRowHeight(row,scale_,width,font));
    }
    double minimumScale()const{
        if(widths_.isEmpty()||!model_)return 1;
        const auto space=table_->maximumViewportSize();
        const int fullHeight=space.height()+table_->horizontalHeader()->height();
        const bool hasFittedHeader=fitRowHeaders_&&!table_->verticalHeader()->isHidden();
        const int fullWidth=space.width()+(hasFittedHeader?table_->verticalHeader()->width():0);
        const auto fits=[&](double scale){
            double width=0;
            for(int c=0;c<widths_.size();++c)if(widths_[c]>0)
                width+=c==wrapColumn_?wrapWidth(scale,widths_[c]):std::ceil(widths_[c]*scale);
            const int heading=headerHeight(scale);
            if(hasFittedHeader)width+=rowHeaderWidth(scale);
            return width<=fullWidth-2&&heightAt(scale)<=fullHeight-heading-2;
        };
        if(fits(1))return 1;
        double low=LowestScale,high=1;
        for(int iteration=0;iteration<16;++iteration){const double middle=(low+high)*.5;if(fits(middle))low=middle;else high=middle;}
        return low;
    }
    static QFont scaled(QFont font,double scale){
        if(font.pointSizeF()>0)font.setPointSizeF(font.pointSizeF()*scale);
        else font.setPixelSize(std::max(2,qRound(font.pixelSize()*scale)));
        font.setStyleStrategy(QFont::StyleStrategy(QFont::PreferAntialias|QFont::PreferQuality));return font;
    }
    void applyScale(double next,QPointF anchor,bool force=false){
        if(!force&&std::abs(next-scale_)<.00001){publish();return;}
        applying_=true;
        auto* horizontal=table_->horizontalScrollBar();auto* vertical=table_->verticalScrollBar();
        const auto content=(QPointF(horizontal->value(),vertical->value())+anchor)/scale_;
        scale_=next;table_->setUpdatesEnabled(false);table_->setFont(scaled(font_,scale_));
        auto* header=table_->horizontalHeader();header->setFont(scaled(headerFont(),scale_));
        header->setProperty("studio_table_navigation_header",true);header->setProperty("studio_table_scale",scale_);
        header->setStretchLastSection(false);header->setSectionResizeMode(QHeaderView::Interactive);
        header->setMinimumSectionSize(8);
        header->setFixedHeight(headerHeight(scale_));
        auto* rowHeader=table_->verticalHeader();
        rowHeader->setFont(scaled(font_,scale_));rowHeader->setMinimumSectionSize(4);
        rowHeader->setDefaultSectionSize(qCeil(rowHeight_*scale_));
        if(fitRowHeaders_){
            rowHeader->setProperty("studio_table_navigation_header",true);
            rowHeader->setProperty("studio_table_scale",scale_);
            rowHeader->setFixedWidth(rowHeaderWidth(scale_));
        }
        for(int column=0;column<widths_.size();++column)if(widths_[column]>0)
            table_->setColumnWidth(column,column==wrapColumn_?wrapWidth(scale_,widths_[column]):qCeil(widths_[column]*scale_));
        resizeWrappedRows();
        table_->doItemsLayout();
        horizontal->setValue(qRound(content.x()*scale_-anchor.x()));vertical->setValue(qRound(content.y()*scale_-anchor.y()));
        horizontal->setSingleStep(12);vertical->setSingleStep(12);
        table_->setUpdatesEnabled(true);applying_=false;publish();
    }
    void publish(){
        table_->setProperty("studio_table_scale",scale_);table_->setProperty("studio_table_minimum_scale",minimumScale());
        table_->setProperty("studio_table_maximum_scale",HighestScale);
        table_->setProperty("studio_table_base_width",contentWidth());table_->setProperty("studio_table_base_height",contentHeight());
    }
    void finishDrag(){if(dragging_)table_->viewport()->setCursor(savedCursor_);pressed_=dragging_=false;}
    const QFont& headerFont()const{return matchHeaderFont_?font_:headerFont_;}
    QTableView* table_;QPointer<QAbstractItemModel> model_;QFont font_,headerFont_;bool matchHeaderFont_;int wrapColumn_;bool fitRowHeaders_;
    QList<QMetaObject::Connection> connections_;QVector<double> widths_;QStringList rowHeaders_;
    quint64 measureGeneration_=0;int measureRow_=0;
    double scale_=1,rowHeight_=32,headingTextHeight_=0;int visibleRows_=0;bool dirty_=true,queued_=false,applying_=false,pressed_=false,dragging_=false,suppressContext_=false;
    QPointF press_;QPoint scroll_;QCursor savedCursor_;
};
TableNavigation* controller(QTableView* table){
    return table?dynamic_cast<TableNavigation*>(table->findChild<QObject*>("StudioTableNavigation",Qt::FindDirectChildrenOnly)):nullptr;
}
}
void configureStudioDataTable(QTableView* table,bool matchHeaderFont,int wrapColumn){configureStudioDataTable(table,matchHeaderFont,wrapColumn,false);}
void configureStudioDataTable(QTableView* table,bool matchHeaderFont,int wrapColumn,bool fitRowHeaders){if(table&&!controller(table))new TableNavigation(table,matchHeaderFont,wrapColumn,fitRowHeaders);}
void refreshStudioDataTable(QTableView* table){if(auto* navigation=controller(table))navigation->refresh();}
