// SPDX-License-Identifier: GPL-2.0-or-later
#include "data_motion.h"
#include <climits>
#include "studio_motion.h"
#include <QAbstractAnimation>
#include <QApplication>
#include <QEvent>
#include <QHash>
#include <QHeaderView>
#include <QPainter>
#include <QPointer>
#include <QSet>
#include <QTableView>
#include <QTableWidget>
#include <QTimer>
#include <QVariant>
#include <functional>
#include <algorithm>
#include <cmath>

namespace data_motion { namespace {
class Tween final:public QAbstractAnimation {
public:
 explicit Tween(QObject* parent,std::function<void(double)> paint):QAbstractAnimation(parent),paint_(std::move(paint)){}
 int duration()const override{return duration_;}
 void begin(int ms){stop();duration_=ms;start();}
 void finish(){stop();paint_(1);}
protected:
 void updateCurrentTime(int elapsed)override{const double x=std::clamp(double(elapsed)/duration_,0.,1.);paint_(1-std::pow(1-x,3));}
private:int duration_=180;std::function<void(double)> paint_;
};
struct Cell {QString key;QVariant value;QRect rect;};
QVector<Cell> visibleCells(QTableView* table){
 QVector<Cell> result;if(!table||!table->model()||!table->isVisible())return result;
 auto* model=table->model();const QRect viewport=table->viewport()->rect();
 int first=table->rowAt(0);if(first<0)first=0;int last=table->rowAt(viewport.bottom());if(last<0)last=model->rowCount()-1;
 for(int row=first;row<=last&&result.size()<4096;++row){
  if(table->isRowHidden(row))continue;
  for(int col=0;col<model->columnCount();++col){
   if(table->isColumnHidden(col))continue;const auto index=model->index(row,col);const auto rect=table->visualRect(index);
   if(!rect.intersects(viewport))continue;
   const auto role=model->index(row,0).data(Qt::UserRole);QString key;
   if(role.toString()=="section")continue;
   if(role.metaType().id()==QMetaType::QString&&!role.toString().isEmpty())key=role.toString()+":"+QString::number(col);
   else key=QString::number(row)+":"+model->headerData(col,Qt::Horizontal).toString();
   result.append({key,index.data(Qt::DisplayRole),rect});
  }
 }
 return result;
}
class Highlight final:public QWidget {
public:
 explicit Highlight(QWidget* parent):QWidget(parent){setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_NoSystemBackground);setAttribute(Qt::WA_TranslucentBackground);setFocusPolicy(Qt::NoFocus);hide();}
 QVector<QRect> cells;double alpha=0;bool frame=false,wash=false;QColor paper;
 struct Ghost{QRect rect;QPixmap pixels;};QVector<Ghost> ghosts;double ghostProgress=1;
protected:
 void paintEvent(QPaintEvent*)override{
  QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
  if(wash&&alpha>0){p.save();p.setOpacity(alpha);p.fillRect(rect(),paper);p.restore();}
  if(alpha>0){p.setPen(Qt::NoPen);p.setBrush(QColor(61,151,124,qRound(alpha*65)));for(const auto& cell:cells)p.drawRect(cell.adjusted(1,1,-1,-1));
   if(frame){p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor(0,126,116,qRound(alpha*190)),2));p.drawRoundedRect(QRectF(rect()).adjusted(2,2,-2,-2),4,4);}}
  for(const auto& ghost:ghosts){p.save();p.setClipRect(QRect(ghost.rect.topLeft(),QSize(ghost.rect.width(),qRound(ghost.rect.height()*(1-ghostProgress)))));p.drawPixmap(ghost.rect.topLeft(),ghost.pixels);p.restore();}
 }
};
class TableMotion final:public QObject {
public:
 explicit TableMotion(QTableView* table):QObject(table),table_(table),overlay_(new Highlight(table->viewport())),flash_(this,[this](double p){overlay_->alpha=1-p;table_->setProperty("studio_motion_result_progress",p);overlay_->update();if(p>=1){overlay_->hide();table_->setProperty("studio_motion_result_active",false);}}),rows_(this,[this](double p){advanceRows(p);}){
  setObjectName("StudioDataMotion");table_->viewport()->installEventFilter(this);qApp->installEventFilter(this);
 }
 ~TableMotion()override{if(qApp)qApp->removeEventFilter(this);}
 void beforeCells(){++cellGeneration_;flash_.finish();before_.clear();for(const auto& cell:visibleCells(table_))before_.insert(cell.key,cell.value);}
 void afterCells(const QString& domain){
  const bool same=domain_==domain;domain_=domain;overlay_->cells.clear();
  if(same&&!before_.isEmpty())for(const auto& cell:visibleCells(table_))if(before_.contains(cell.key)&&before_[cell.key]!=cell.value)overlay_->cells.append(cell.rect);
  table_->setProperty("studio_motion_result_changed_cells",overlay_->cells.size());
  if(!studio_motion::enabled()||overlay_->cells.isEmpty()||!table_->isVisible())return;
  overlay_->setGeometry(table_->viewport()->rect());overlay_->ghosts.clear();overlay_->frame=false;overlay_->wash=false;overlay_->alpha=1;overlay_->show();overlay_->raise();
  table_->setProperty("studio_motion_result_active",true);flash_.begin(420);
 }
 void scheduleCells(const QString& domain){const quint64 token=++cellGeneration_;QTimer::singleShot(0,this,[this,domain,token]{if(token==cellGeneration_)afterCells(domain);});}
 void beforeRows(int first=0,int count=INT_MAX){
  rows_.finish();oldRows_.clear();auto* widget=qobject_cast<QTableWidget*>(table_);if(!widget)return;
  if(studio_motion::enabled()&&table_->isVisible())for(int row=first;row-first<count&&row<rowIds_.size()&&row<widget->rowCount();++row){
   const QRect rect(0,table_->rowViewportPosition(row),table_->viewport()->width(),widget->rowHeight(row));
   if(rect.intersects(table_->viewport()->rect())&&oldRows_.size()<12)oldRows_.insert(rowIds_[row],{rect,table_->viewport()->grab(rect)});
  }
 }
 void afterRows(const QString& domain,const QStringList& identities){
  auto* widget=qobject_cast<QTableWidget*>(table_);if(!widget)return;
  const bool same=rowDomain_==domain;rowDomain_=domain;const auto oldIds=rowIds_;rowIds_=identities;growing_.clear();overlay_->ghosts.clear();
  if(!same||oldIds.isEmpty()||!studio_motion::enabled()||!table_->isVisible())return;
  const QSet<QString> previous(oldIds.cbegin(),oldIds.cend()),current(identities.cbegin(),identities.cend());
  for(int row=0;row<identities.size();++row)if(!previous.contains(identities[row])){
   const QRect rect=table_->visualRect(table_->model()->index(row,0));if(rect.intersects(table_->viewport()->rect()))growing_.append({row,widget->rowHeight(row)});
  }
  for(auto it=oldRows_.cbegin();it!=oldRows_.cend();++it)if(!current.contains(it.key()))overlay_->ghosts.append(it.value());
  table_->setProperty("studio_motion_rows_inserted",growing_.size());table_->setProperty("studio_motion_rows_removed",overlay_->ghosts.size());
  if(growing_.isEmpty()&&overlay_->ghosts.isEmpty())return;
  rowMinimum_=table_->verticalHeader()->minimumSectionSize();table_->verticalHeader()->setMinimumSectionSize(1);
  overlay_->cells.clear();overlay_->alpha=0;overlay_->frame=false;overlay_->wash=false;overlay_->setGeometry(table_->viewport()->rect());overlay_->ghostProgress=0;overlay_->show();overlay_->raise();
  table_->setProperty("studio_motion_rows_active",true);advanceRows(0);rows_.begin(growing_.isEmpty()?140:180);
 }
protected:
 bool eventFilter(QObject* object,QEvent* event)override{
  const auto type=event->type();
  if(object==table_->viewport()&&(type==QEvent::Resize||type==QEvent::Hide)){flash_.finish();rows_.finish();}
  if(type==QEvent::MouseButtonPress||type==QEvent::KeyPress||type==QEvent::Wheel){auto* w=qobject_cast<QWidget*>(object);if(w&&(w==table_||table_->isAncestorOf(w))){rows_.finish();flash_.finish();}}
  return false;
 }
private:
 void advanceRows(double p){auto* widget=qobject_cast<QTableWidget*>(table_);if(!widget)return;
  for(const auto& row:growing_)if(row.first<widget->rowCount())widget->setRowHeight(row.first,std::max(table_->verticalHeader()->minimumSectionSize(),qRound(row.second*p)));
  overlay_->ghostProgress=std::min(1.,p*180/140);table_->setProperty("studio_motion_rows_progress",p);overlay_->update();
  if(p>=1){overlay_->ghosts.clear();overlay_->hide();growing_.clear();if(rowMinimum_>=0){table_->verticalHeader()->setMinimumSectionSize(rowMinimum_);rowMinimum_=-1;}table_->setProperty("studio_motion_rows_active",false);}
 }
 QTableView* table_;Highlight* overlay_;Tween flash_,rows_;QHash<QString,QVariant> before_;QString domain_,rowDomain_;QStringList rowIds_;
 QHash<QString,Highlight::Ghost> oldRows_;QVector<QPair<int,int>> growing_;int rowMinimum_=-1;quint64 cellGeneration_=0;
};
TableMotion* tableMotion(QTableView* table){auto* existing=table->findChild<QObject*>("StudioDataMotion",Qt::FindDirectChildrenOnly);return existing?static_cast<TableMotion*>(existing):new TableMotion(table);}
class Properties final:public QObject {
public:
 explicit Properties(QWidget* panel):QObject(panel),panel_(panel),overlay_(new Highlight(panel)),phase_(this,[this](double p){
  panel_->setProperty("studio_motion_property_progress",p);
  overlay_->alpha=1-p;overlay_->update();
  if(p>=1){overlay_->hide();panel_->setProperty("studio_motion_property_active",false);}
 }){setObjectName("StudioPropertyMotion");panel_->installEventFilter(this);qApp->installEventFilter(this);}
 ~Properties()override{if(qApp)qApp->removeEventFilter(this);}
 void pulse(const QString& identity){if(identity_==identity)return;identity_=identity;panel_->setProperty("studio_motion_property_identity",identity);
  if(!studio_motion::enabled()||!panel_->isVisible())return;phase_.finish();overlay_->wash=false;overlay_->frame=true;overlay_->alpha=1;overlay_->setGeometry(panel_->rect());overlay_->show();overlay_->raise();panel_->setProperty("studio_motion_property_active",true);phase_.begin(240);}
protected:
 bool eventFilter(QObject* object,QEvent* event)override{
  const auto type=event->type();if(object==panel_){
   if(type==QEvent::Show&&studio_motion::enabled()){
    phase_.finish();overlay_->wash=true;overlay_->frame=false;overlay_->cells.clear();overlay_->paper=panel_->palette().color(QPalette::Window);overlay_->alpha=1;
    overlay_->setGeometry(panel_->rect());overlay_->show();overlay_->raise();panel_->setProperty("studio_motion_property_active",true);phase_.begin(160);
   }
   if(type==QEvent::Hide)phase_.finish();
   if(type==QEvent::Resize)overlay_->setGeometry(panel_->rect());
  }
  if(type==QEvent::MouseButtonPress||type==QEvent::KeyPress||type==QEvent::InputMethod){auto* w=qobject_cast<QWidget*>(object);if(w&&(w==panel_||panel_->isAncestorOf(w)))phase_.finish();}
  return false;
 }
private:QWidget* panel_;Highlight* overlay_;Tween phase_;QString identity_;
};
Properties* properties(QWidget* panel){auto* existing=panel->findChild<QObject*>("StudioPropertyMotion",Qt::FindDirectChildrenOnly);return existing?static_cast<Properties*>(existing):new Properties(panel);}
}
void beginTableUpdate(QTableView* table){if(table)tableMotion(table)->beforeCells();}
void finishTableUpdate(QTableView* table,const QString& domain){if(table)tableMotion(table)->scheduleCells(domain);}
void beginRowSplice(QTableWidget* table,int first,int count){if(table)tableMotion(table)->beforeRows(first,count);}
void beginRowUpdate(QTableWidget* table){if(table)tableMotion(table)->beforeRows();}
void finishRowUpdate(QTableWidget* table,const QString& domain,const QStringList& identities){if(table)tableMotion(table)->afterRows(domain,identities);}
void installProperties(QWidget* panel){if(panel)properties(panel);}
void pulseProperties(QWidget* panel,const QString& identity){if(panel)properties(panel)->pulse(identity);}
}
