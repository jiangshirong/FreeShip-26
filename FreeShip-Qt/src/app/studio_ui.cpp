#include "app/ui_language.h"
#include "studio_ui.h"
#include "text_style.h"
#include "studio_numeric.h"
#include "studio_motion.h"
#include "message_motion.h"
#include "layout_motion.h"
#include <QTabWidget>
#include "math_notation.h"
#include "numeric_range_text.h"
#include <QStyleOptionHeader>
#include <QStyleOptionTab>
#include <QPushButton>
#include <QStyleOptionViewItem>
#include "studio_caption.h"
#include "studio_menu_icons.h"
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QComboBox>
#include <QCloseEvent>
#include <QDir>
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMainWindow>
#include <QLayout>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QResizeEvent>
#include <QRegion>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleOptionMenuItem>
#include <QTableView>
#include <QHeaderView>
#include <QToolButton>
#include <QToolBar>
#include <QTimer>
#include <QScreen>
#include <QSplitter>
#include <QStandardPaths>
#include <QWindow>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif
#include <algorithm>
#include <cmath>
#include <functional>

namespace {
quint64 dialogShowOrder=0;
constexpr auto waitingDialogCount="studio_waiting_dialogs";
constexpr auto deferredDialogDelete="studio_deferred_dialog_delete";
constexpr auto nativeFileDialog="studio_native_file_dialog";

// Small input forms still return a result to their caller. Closing an owner
// first ends that form and unwinds its caller before the owner can be deleted.
class DialogWait final : public QObject {
public:
    explicit DialogWait(QDialog& dialog):dialog_(dialog){
        dialog_.installEventFilter(this);
        for(auto* owner=dialog.parentWidget();owner;owner=owner->parentWidget()){
            owners_.append(owner);
            owner->setProperty(waitingDialogCount,owner->property(waitingDialogCount).toInt()+1);
            owner->installEventFilter(this);
            if(auto* parentDialog=qobject_cast<QDialog*>(owner)){
                if(parentDialog->testAttribute(Qt::WA_DeleteOnClose)){
                    parentDialog->setProperty(deferredDialogDelete,true);
                    parentDialog->setAttribute(Qt::WA_DeleteOnClose,false);
                }
                connect(parentDialog,&QDialog::finished,this,[this,parentDialog]{
                    if(parentDialog->property(deferredDialogDelete).toBool())parentDialog->setProperty("studio_waited_close",true);
                    dialog_.hide();
                });
            }
        }
        connect(&dialog_,&QDialog::finished,this,[this](int result){result_=result;finished_=true;loop_.quit();});
    }
    ~DialogWait() override {
        dialog_.removeEventFilter(this);
        for(const auto& owner:owners_)if(owner){
            owner->removeEventFilter(this);
            const int remaining=owner->property(waitingDialogCount).toInt()-1;
            owner->setProperty(waitingDialogCount,remaining);
            if(remaining==0&&owner->property(deferredDialogDelete).toBool()){
                owner->setProperty(deferredDialogDelete,false);
                owner->setAttribute(Qt::WA_DeleteOnClose,true);
                if(owner->property("studio_waited_close").toBool())owner->deleteLater();
            }
        }
        if(closingOwner_)QMetaObject::invokeMethod(closingOwner_,"close",Qt::QueuedConnection);
    }
    int run(){
        showStudioDialog(dialog_);
        if(!finished_&&dialog_.isVisible())loop_.exec(QEventLoop::DialogExec);
        return result_;
    }
private:
    bool eventFilter(QObject* watched,QEvent* event) override {
        if(watched==&dialog_){
            if(event->type()==QEvent::Hide)loop_.quit();
        }else if(event->type()==QEvent::Close){
            closingOwner_=qobject_cast<QWidget*>(watched);
            static_cast<QCloseEvent*>(event)->ignore();
            dialog_.hide();
            return true;
        }
        return false;
    }
    QDialog& dialog_;
    QList<QPointer<QWidget>> owners_;
    QPointer<QWidget> closingOwner_;
    QEventLoop loop_;
    int result_=QDialog::Rejected;
    bool finished_=false;
};

constexpr int MenuTextInset = 35;
constexpr int MenuStatusGutter = 29;
constexpr int MenuRowHeight = 32;
constexpr int MenuGroupGap = 12;
constexpr QSize FormatIconSize(44, 24);
void decorateStudioMenu(QMenu* menu){
    for(auto* action:menu->actions()){
        if(action->isSeparator()||!action->icon().isNull())continue;
        QString key;
        const auto operation=action->property("view_operation").toString(),background=action->property("background_operation").toString(),file=action->property("file_operation").toString();
        const QHash<QString,QString> viewKeys{{"wireframe","Wireframe"},{"fill","FillColor"},{"shade_1","GaussianCurvature"},{"shade_2","Developability"},{"shade_3","Zebra"},{"light_settings","Lighting"},{"print","Print"},{"save_image","ExportImage"}};
        const QHash<QString,QString> backgroundKeys{{"import","ImportFile"},{"origin","Point"},{"scale","Scale"},{"frame","CropFrame"},{"transparent_color","PickColor"},{"transparent","PickColor"},{"alpha","Blend"},{"tolerance","Tolerance"},{"visible","Observe"},{"export","SaveFile"},{"remove","DeleteOne"}};
        if(viewKeys.contains(operation))key=viewKeys[operation];
        else if(backgroundKeys.contains(background))key=backgroundKeys[background];
        else if(action->property("focal_mm").isValid())key="Camera";
        else if(action->property("coordinate_axis").isValid()){const int axis=action->property("coordinate_axis").toInt();key=QStringList{"CoordinateOff","CoordinateX","CoordinateY","CoordinateZ"}.value(axis);}
        else if(file=="recent")key="OpenFile";
        else if(action->menu()){
            const auto role=action->menu()->property("menu_role").toString();
            key=QHash<QString,QString>{{"camera","Camera"},{"render_mode","RenderMode"},{"background","BackgroundImage"},{"coordinates","Coordinates"}}.value(role);
        }
        if(!key.isEmpty())action->setIcon(studioIcon(key));
    }
}
void styleStudioScrollbar(QScrollBar* bar){
    if(bar->property("studio_scrollbar").toBool())return;
    bar->setProperty("studio_scrollbar",true);
    bar->setStyleSheet(QStringLiteral(
        "QScrollBar { background:#edf3f0; border:none; }"
        "QScrollBar:horizontal { height:20px; } QScrollBar:vertical { width:20px; }"
        "QScrollBar::handle { background:#8eaaa1; border:2px solid #edf3f0; border-radius:8px; }"
        "QScrollBar::handle:horizontal { min-width:40px; } QScrollBar::handle:vertical { min-height:40px; }"
        "QScrollBar::handle:hover { background:#527d71; } QScrollBar::handle:pressed { background:#007e74; }"
        "QScrollBar::add-line,QScrollBar::sub-line { width:0; height:0; border:none; }"
        "QScrollBar::add-page,QScrollBar::sub-page { background:transparent; }"));
}
bool dialogCaption(const QString& text) {
    const auto caption=text.section('\t',0,0).trimmed();
    return caption.endsWith(QChar(0x2026))||caption.endsWith("...");
}

// Preserve each QAction and its connections. Only its position in this popup
// changes; invisible actions stay available for later state refreshes.
void groupMenuEntries(QMenu* menu) {
    QList<QAction*> groups[4],gaps;
    const auto before=menu->actions();
    for(auto* action:before) {
        if(action->isSeparator())continue;
        groups[action->menu()?3:action->isCheckable()?2:studioMenuOpensDialog(action)?1:0].append(action);
    }
    for(auto* action:menu->findChildren<QAction*>(QString{},Qt::FindDirectChildrenOnly))
        if(action->property("studio_menu_group_gap").toBool())gaps.append(action);
    QList<QAction*> ordered;bool haveVisibleGroup=false;int usedGaps=0;
    for(const auto& group:groups) {
        const bool visible=std::any_of(group.begin(),group.end(),[](auto* a){return a->isVisible();});
        if(visible&&haveVisibleGroup) {
            if(usedGaps==gaps.size()) {
                auto* gap=new QAction(menu);gap->setSeparator(true);gap->setProperty("studio_menu_group_gap",true);gaps.append(gap);
            }
            ordered.append(gaps[usedGaps++]);
        }
        ordered.append(group);haveVisibleGroup|=visible;
    }
    if(ordered==before)return;
    for(auto* action:before)menu->removeAction(action);
    menu->addActions(ordered);
}
class KeyboardIcon final : public QIconEngine {
public:
    QIconEngine* clone()const override{return new KeyboardIcon;}
    QString iconName()override{return "studio-shortcuts";}
    QPixmap pixmap(const QSize& size,QIcon::Mode mode,QIcon::State state)override{return scaledPixmap(size,mode,state,1);}
    QPixmap scaledPixmap(const QSize& size,QIcon::Mode mode,QIcon::State state,qreal ratio)override{
        QPixmap image(size*ratio);image.setDevicePixelRatio(ratio);image.fill(Qt::transparent);
        QPainter painter(&image);paint(&painter,QRect(QPoint(),size),mode,state);return image;
    }
    void paint(QPainter* p,const QRect& r,QIcon::Mode mode,QIcon::State)override{
        p->save();p->setRenderHint(QPainter::Antialiasing);p->translate(r.center());
        const auto scale=qMin(r.width(),r.height())/24.;p->scale(scale,scale);p->translate(-12,-12);
        p->setPen(QPen(QColor(mode==QIcon::Disabled?"#88938f":"#2c6259"),1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        p->drawRoundedRect(QRectF(2,5,20,14),2,2);
        for(int y:{9,12})for(int x:{6,10,14,18})p->drawPoint(QPointF(x,y));
        p->drawLine(QPointF(7,16),QPointF(17,16));p->restore();
    }
};
class TutorialIcon final : public QIconEngine {
public:
    QIconEngine* clone()const override{return new TutorialIcon;}
    QString iconName()override{return "studio-tutorial";}
    QPixmap pixmap(const QSize& size,QIcon::Mode mode,QIcon::State state)override{return scaledPixmap(size,mode,state,1);}
    QPixmap scaledPixmap(const QSize& size,QIcon::Mode mode,QIcon::State state,qreal ratio)override{
        QPixmap image(size*ratio);image.setDevicePixelRatio(ratio);image.fill(Qt::transparent);QPainter p(&image);paint(&p,QRect(QPoint(),size),mode,state);return image;
    }
    void paint(QPainter* p,const QRect& rect,QIcon::Mode mode,QIcon::State)override{
        p->save();p->setRenderHint(QPainter::Antialiasing);const auto side=qMin(rect.width(),rect.height());
        p->translate(QRectF(rect).center()-QPointF(side/2.,side/2.));p->scale(side/24.,side/24.);
        p->setPen(QPen(QColor(mode==QIcon::Disabled?"#91a29b":"#315f56"),1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p->setBrush(Qt::NoBrush);
        QPainterPath book;book.moveTo(12,6);book.cubicTo(9,3.5,5.5,3.5,2.5,4.5);book.lineTo(2.5,19);book.cubicTo(5.5,18,9,18,12,20.5);
        book.cubicTo(15,18,18.5,18,21.5,19);book.lineTo(21.5,4.5);book.cubicTo(18.5,3.5,15,3.5,12,6);book.lineTo(12,20.5);p->drawPath(book);
        p->drawLine(QPointF(5.5,8),QPointF(9,8.8));p->drawLine(QPointF(15,8.8),QPointF(18.5,8));p->restore();
    }
};

class ImportIcon final:public QIconEngine {
public:
    QIconEngine* clone()const override{return new ImportIcon;}
    QString iconName()override{return "studio-import-file";}
    QPixmap pixmap(const QSize& size,QIcon::Mode mode,QIcon::State state)override{return scaledPixmap(size,mode,state,1);}
    QPixmap scaledPixmap(const QSize& size,QIcon::Mode mode,QIcon::State state,qreal ratio)override{
        QPixmap image(size*ratio);image.setDevicePixelRatio(ratio);image.fill(Qt::transparent);
        QPainter painter(&image);paint(&painter,QRect(QPoint(),size),mode,state);return image;
    }
    void paint(QPainter* p,const QRect& rect,QIcon::Mode mode,QIcon::State)override{
        p->save();p->setRenderHint(QPainter::Antialiasing);p->translate(QRectF(rect).center());
        const auto scale=qMin(rect.width(),rect.height())/24.;p->scale(scale,scale);p->translate(-12,-12);
        p->setPen(QPen(QColor(mode==QIcon::Disabled?"#8c9b96":"#2c6259"),1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);p->drawPolyline(QPolygonF{{4,14},{4,20},{20,20},{20,14}});
        p->drawLine(QPointF(12,3),QPointF(12,15));p->drawPolyline(QPolygonF{{7.5,10.5},{12,15},{16.5,10.5}});p->restore();
    }
};

class CountedActionIcon final:public QIconEngine {
public:
    CountedActionIcon(bool remove,bool multiple):remove_(remove),multiple_(multiple){}
    QIconEngine* clone()const override{return new CountedActionIcon(remove_,multiple_);}
    QString iconName()override{return QString("studio-%1-%2").arg(remove_?"delete":"add",multiple_?"many":"one");}
    QPixmap pixmap(const QSize& size,QIcon::Mode mode,QIcon::State state)override{return scaledPixmap(size,mode,state,1);}
    QPixmap scaledPixmap(const QSize& size,QIcon::Mode mode,QIcon::State state,qreal ratio)override{
        QPixmap image(size*ratio);image.setDevicePixelRatio(ratio);image.fill(Qt::transparent);
        QPainter p(&image);paint(&p,QRect(QPoint(),size),mode,state);return image;
    }
    void paint(QPainter* p,const QRect& rect,QIcon::Mode mode,QIcon::State)override{
        p->save();p->setRenderHint(QPainter::Antialiasing);p->translate(QRectF(rect).center());
        const auto scale=qMin(rect.width(),rect.height())/24.;p->scale(scale,scale);p->translate(-12,-12);
        p->setPen(QPen(QColor(mode==QIcon::Disabled?"#8c9b96":"#2c6259"),1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p->setBrush(Qt::NoBrush);
        const auto glyph=[&](qreal x){
            if(!remove_){const qreal arm=multiple_?4:7;p->drawLine(QPointF(x-arm,12),QPointF(x+arm,12));p->drawLine(QPointF(x,12-arm),QPointF(x,12+arm));return;}
            const qreal half=multiple_?3.4:5,top=multiple_?4:3,bottom=multiple_?20:21;
            p->drawLine(QPointF(x-half-.8,top+3),QPointF(x+half+.8,top+3));
            QPainterPath handle;handle.moveTo(x-1.7,top+3);handle.lineTo(x-1.7,top+1);handle.quadTo(x-1.7,top,x-.7,top);
            handle.lineTo(x+.7,top);handle.quadTo(x+1.7,top,x+1.7,top+1);handle.lineTo(x+1.7,top+3);p->drawPath(handle);
            QPainterPath body;body.moveTo(x-half+.5,top+3);body.lineTo(x-half+1,bottom-1);
            body.quadTo(x-half+1,bottom,x-half+2,bottom);body.lineTo(x+half-2,bottom);
            body.quadTo(x+half-1,bottom,x+half-1,bottom-1);body.lineTo(x+half-.5,top+3);p->drawPath(body);
            if(multiple_)p->drawLine(QPointF(x,top+6),QPointF(x,bottom-3));
            else for(qreal offset:{-1.7,1.7})p->drawLine(QPointF(x+offset,top+6),QPointF(x+offset,bottom-3));
        };
        if(multiple_){glyph(6);glyph(18);}else glyph(12);
        p->restore();
    }
private:
    bool remove_,multiple_;
};

// The source images have different intrinsic margins and silhouettes. Give
// their actual colored artwork equal visual weight inside the same 28 DIP slot.
class VendorIcon final:public QIconEngine {
public:
    explicit VendorIcon(QString vendor):vendor_(std::move(vendor)),image_(":/ai-vendors/"+vendor_+".png"){
        image_=image_.convertToFormat(QImage::Format_ARGB32);
        for(int y=0;y<image_.height();++y){const auto* line=reinterpret_cast<const QRgb*>(image_.constScanLine(y));
            for(int x=0;x<image_.width();++x){const auto pixel=line[x];
                if(qAlpha(pixel)>40&&qMin(qRed(pixel),qMin(qGreen(pixel),qBlue(pixel)))<238){
                    bounds_=bounds_.united(QRect(x,y,1,1));++inkArea_;
                }
            }
        }
        if(!bounds_.isEmpty())image_=image_.copy(bounds_);
    }
    QIconEngine* clone()const override{return new VendorIcon(*this);}
    QString iconName()override{return "studio-vendor:"+vendor_;}
    QPixmap pixmap(const QSize& size,QIcon::Mode mode,QIcon::State state)override{return scaledPixmap(size,mode,state,1);}
    QPixmap scaledPixmap(const QSize& size,QIcon::Mode mode,QIcon::State state,qreal ratio)override{
        QPixmap pixels(size*ratio);pixels.setDevicePixelRatio(ratio);pixels.fill(Qt::transparent);
        QPainter painter(&pixels);paint(&painter,QRect(QPoint(),size),mode,state);return pixels;
    }
    void paint(QPainter* painter,const QRect& rect,QIcon::Mode mode,QIcon::State)override{
        if(image_.isNull()||bounds_.isEmpty()||inkArea_==0)return;
        // The black OpenAI knot reads heavier after smooth minification; the
        // small optical compensation keeps it balanced with the colored marks.
        const auto optical=vendor_=="openai"?.96:1.;
        const auto scale=qMin(24./qMax(bounds_.width(),bounds_.height()),std::sqrt(225./inkArea_))*qMin(rect.width(),rect.height())/28.*optical;
        const QSizeF size(bounds_.width()*scale,bounds_.height()*scale);
        const QRectF target(QRectF(rect).center()-QPointF(size.width()/2,size.height()/2),size);
        painter->save();painter->setRenderHint(QPainter::SmoothPixmapTransform);
        if(mode==QIcon::Disabled)painter->setOpacity(.5);
        // Area-filter the source before painting. Bilinear-only minification
        // of a 1024px logo loses thin details at the 24 DIP display size.
        const auto ratio=painter->device()->devicePixelRatioF();
        const QSize physical(qMax(1,qRound(size.width()*ratio)),qMax(1,qRound(size.height()*ratio)));
        const auto reduced=image_.scaled(physical,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        painter->drawImage(target,reduced);painter->restore();
    }
private:
    QString vendor_;QImage image_;QRect bounds_;int inkArea_=0;
};

// The format badge has its own letterforms. Each polyline is one pen stroke.
const QHash<QChar, QList<QPolygonF>>& formatLetterStrokes() {
    static const QHash<QChar, QList<QPolygonF>> letters{
        {'A', {{{0,8},{2.5,0},{5,8}},{{1,5},{4,5}}}},
        {'B', {{{0,8},{0,0},{3.2,0},{4.7,1},{4.7,2.8},{3.2,4},{0,4}},{{3.2,4},{5,5},{5,6.8},{3.4,8},{0,8}}}},
        {'C', {{{5,1},{3.7,0},{1.5,0},{0,1.6},{0,6.4},{1.5,8},{3.7,8},{5,7}}}},
        {'D', {{{0,8},{0,0},{2.7,0},{5,2},{5,6},{2.7,8},{0,8}}}},
        {'E', {{{5,0},{0,0},{0,8},{5,8}},{{0,4},{4,4}}}},
        {'F', {{{5,0},{0,0},{0,8}},{{0,4},{4,4}}}},
        {'G', {{{5,1},{3.8,0},{1.5,0},{0,1.6},{0,6.4},{1.5,8},{4.9,8},{4.9,4.4},{2.8,4.4}}}},
        {'H', {{{0,0},{0,8}},{{5,0},{5,8}},{{0,4},{5,4}}}},
        {'I', {{{.7,0},{4.3,0}},{{2.5,0},{2.5,8}},{{.7,8},{4.3,8}}}},
        {'J', {{{5,0},{5,6.4},{3.5,8},{1.5,8},{0,6.6}}}},
        {'L', {{{0,0},{0,8},{5,8}}}},
        {'M', {{{0,8},{0,0},{2.5,4},{5,0},{5,8}}}},
        {'N', {{{0,8},{0,0},{5,8},{5,0}}}},
        {'O', {{{1.5,0},{3.5,0},{5,1.5},{5,6.5},{3.5,8},{1.5,8},{0,6.5},{0,1.5},{1.5,0}}}},
        {'P', {{{0,8},{0,0},{3.4,0},{5,1.3},{5,3.3},{3.4,4.5},{0,4.5}}}},
        {'R', {{{0,8},{0,0},{3.4,0},{5,1.3},{5,3},{3.4,4.3},{0,4.3}},{{2.6,4.3},{5,8}}}},
        {'S', {{{5,1},{3.8,0},{1.3,0},{0,1.2},{0,2.8},{1.3,4},{3.7,4},{5,5.2},{5,6.8},{3.7,8},{1.3,8},{0,7}}}},
        {'T', {{{0,0},{5,0}},{{2.5,0},{2.5,8}}}},
        {'U', {{{0,0},{0,6.3},{1.5,8},{3.5,8},{5,6.3},{5,0}}}},
        {'V', {{{0,0},{2.5,8},{5,0}}}},
        {'W', {{{0,0},{.9,8},{2.5,4},{4.1,8},{5,0}}}},
        {'X', {{{0,0},{5,8}},{{5,0},{0,8}}}},
        {'Y', {{{0,0},{2.5,4},{5,0}},{{2.5,4},{2.5,8}}}},
        {'Z', {{{0,0},{5,0},{0,8},{5,8}}}},
    };
    return letters;
}

class StudioFormatIcon final : public QIconEngine {
public:
    explicit StudioFormatIcon(QString format,bool title=false,bool importing=false) : format_(std::move(format)),title_(title),importing_(importing) {}
    QIconEngine* clone() const override { return new StudioFormatIcon(format_,title_,importing_); }
    QString iconName() override { return (title_?"studio-title-format:":"studio-format:") + format_; }
    QSize actualSize(const QSize& size, QIcon::Mode, QIcon::State) override {
        return (title_?QSize(24,24):FormatIconSize).scaled(size, Qt::KeepAspectRatio);
    }
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
        return scaledPixmap(size, mode, state, 1);
    }
    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal ratio) override {
        QPixmap image(size * ratio);
        image.setDevicePixelRatio(ratio);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        paint(&painter, QRect(QPoint(), size), mode, state);
        return image;
    }
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State) override {
        const auto logical=title_?QSize(24,24):FormatIconSize;
        const auto size = QSizeF(logical).scaled(rect.size(), Qt::KeepAspectRatio);
        if (size.isEmpty()) return;
        const bool enabled = mode != QIcon::Disabled;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(QRectF(rect).center() - QPointF(size.width()/2, size.height()/2));
        painter->scale(size.width()/logical.width(), size.height()/24);
        QPainterPath paper;
        if(title_){paper.moveTo(3,1.5);paper.lineTo(16,1.5);paper.lineTo(22.5,8);paper.lineTo(22.5,21);
            paper.quadTo(22.5,22.5,21,22.5);paper.lineTo(3,22.5);paper.quadTo(1.5,22.5,1.5,21);paper.lineTo(1.5,3);paper.quadTo(1.5,1.5,3,1.5);
        }else{
        paper.moveTo(6, 2.5); paper.lineTo(35, 2.5); paper.lineTo(41, 8.5);
        paper.lineTo(41, 18.5); paper.quadTo(41, 21.5, 38, 21.5); paper.lineTo(6, 21.5);
        paper.quadTo(3, 21.5, 3, 18.5); paper.lineTo(3, 5.5); paper.quadTo(3, 2.5, 6, 2.5);
        }
        paper.closeSubpath();
        const QColor outline=importing_?QColor(enabled?"#7197b3":"#a8b7c4"):QColor(enabled?"#559578":"#a5b7ae");
        const QColor paperFill=importing_?QColor(enabled?"#e5eff8":"#f0f3f6"):QColor(enabled?"#e1f3e9":"#eff3f0");
        const QColor lettering=importing_?QColor(enabled?"#245878":"#788895"):QColor(enabled?"#006447":"#71847b");
        painter->setPen(QPen(outline, 1.15, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(paperFill);
        painter->drawPath(paper);
        painter->drawPolyline(title_?QPolygonF{{16,1.5},{16,8},{22.5,8}}:QPolygonF{{35,2.5},{35,8.5},{41,8.5}});
        painter->setPen(QPen(lettering, 1.45, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        constexpr qreal advance = 7.5;
        const qreal width=5+advance*(format_.size()-1),scale=title_?qMin(1.,20./width):1.;
        painter->translate((logical.width()-width*scale)/2,title_?10.:8.5);painter->scale(scale,1);
        const auto& letters = formatLetterStrokes();
        for (const auto letter : format_) {
            const auto strokes = letters.constFind(letter);
            if (strokes != letters.cend())
                for (const auto& stroke : *strokes) painter->drawPolyline(stroke);
            painter->translate(advance, 0);
        }
        painter->restore();
    }
private:
    QString format_;
    bool title_,importing_;
};

bool studioWindow(const QWidget* widget){
    return widget&&widget->isWindow()&&(qobject_cast<const QDialog*>(widget)||qobject_cast<const QMainWindow*>(widget));
}
void relaxWindowLimits(QWidget* widget){
    if(!studioWindow(widget))return;
    if(auto* layout=widget->layout())if(layout->sizeConstraint()==QLayout::SetFixedSize||layout->sizeConstraint()==QLayout::SetMinAndMaxSize)
        layout->setSizeConstraint(QLayout::SetDefaultConstraint);
    if(widget->maximumSize()!=QSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX))widget->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
}
void centerStudioMessage(QMessageBox* message){
    if(message->isMaximized()||message->isMinimized())return;
    auto* owner=message->parentWidget()?message->parentWidget()->window():nullptr;
    auto* screen=owner?owner->screen():message->screen();
    if(!screen)screen=QGuiApplication::primaryScreen();
    if(!screen)return;
    const auto available=screen->availableGeometry();auto frame=message->frameGeometry();
    frame.moveCenter(available.center());message->move(frame.topLeft());
}
void prepareStudioWindow(QWidget* widget){
    if(!studioWindow(widget)||widget->property(nativeFileDialog).toBool()
        ||widget->property("studio_window_prepared").toBool())return;
    widget->setProperty("studio_window_prepared",true);
    if(qobject_cast<QDialog*>(widget))widget->setWindowModality(Qt::NonModal);
    auto flags=widget->windowFlags();flags|=Qt::CustomizeWindowHint|Qt::WindowTitleHint|Qt::WindowSystemMenuHint|Qt::WindowCloseButtonHint;
    flags&=~(Qt::MSWindowsFixedSizeDialogHint|Qt::WindowContextHelpButtonHint);
    // The custom main caption paints its own controls, but Windows must know
    // minimize/maximize are supported BEFORE the initial show/maximize call.
    if(widget->property("studio_caption_minimize").toBool())flags|=Qt::WindowMinMaxButtonsHint;
    else {
        flags&=~Qt::WindowMinMaxButtonsHint;
        if(widget->property("studio_caption_maximize").toBool())flags|=Qt::WindowMaximizeButtonHint;
    }
#ifdef Q_OS_LINUX
    // X11 treats explicit title/system menu hints as decoration requests.
    if(widget->property("studio_caption_minimize").toBool())flags=(flags & Qt::WindowType_Mask) | Qt::FramelessWindowHint;
#endif
    if(flags!=widget->windowFlags()&&!widget->isVisible())widget->setWindowFlags(flags);
    relaxWindowLimits(widget);
    if(qobject_cast<QDialog*>(widget))layout_motion::installFeatureWindow(widget);
    if(widget->property("studio_window_icon").isValid())return;
    if(auto* message=qobject_cast<QMessageBox*>(widget)){
        // Windows task dialogs have their own fixed-size HWND. Keep message
        // windows under the same resizable title-bar policy as other dialogs.
        message->setOption(QMessageBox::Option::DontUseNativeDialog);
        const auto kind=message->icon();
        message->setProperty("studio_message_kind",int(kind));
        if(kind!=QMessageBox::NoIcon){
            const QString key=kind==QMessageBox::Warning?"MessageWarning":kind==QMessageBox::Critical?"MessageCritical":kind==QMessageBox::Question?"MessageQuestion":"MessageInformation";
            const auto icon=studioIcon(key);message->setIconPixmap(icon.pixmap(QSize(40,40),message->devicePixelRatioF()));
            widget->setWindowIcon(icon);widget->setProperty("studio_window_icon",key);
        }else setStudioWindowIcon(widget,"Info");
        if(auto* owner=widget->parentWidget();owner&&owner->window()->property("studio_probe_transparent_dialogs").toBool()){
            widget->setWindowOpacity(0);widget->setAttribute(Qt::WA_ShowWithoutActivating);
        }
    }else if(auto* file=qobject_cast<QFileDialog*>(widget))setStudioWindowIcon(file,file->acceptMode()==QFileDialog::AcceptSave?"SaveAs":"OpenFile");
    else if(widget->parentWidget()&&!widget->parentWidget()->windowIcon().isNull()){
        widget->setWindowIcon(widget->parentWidget()->windowIcon());widget->setProperty("studio_window_icon","parent");
    }else setStudioWindowIcon(widget,"Info");
}

int menuTextInset(const QWidget* widget) {
    return widget->property("studio_format_icons").toBool() ? 60 : MenuTextInset;
}

bool isMenuRow(const QStyleOptionMenuItem* item, const QWidget* widget) {
    return item && qobject_cast<const QMenu*>(widget) &&
        (item->menuItemType == QStyleOptionMenuItem::Normal ||
         item->menuItemType == QStyleOptionMenuItem::DefaultItem ||
         item->menuItemType == QStyleOptionMenuItem::SubMenu);
}

class StudioMenuStyle final : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;
    int pixelMetric(PixelMetric metric,const QStyleOption* option=nullptr,const QWidget* widget=nullptr)const override{
        if((metric==PM_ToolBarFrameWidth||metric==PM_ToolBarItemMargin)&&widget&&widget->objectName()=="StudioSideToolbar")return 0;
        return QProxyStyle::pixelMetric(metric,option,widget);
    }
    int styleHint(StyleHint hint,const QStyleOption* option=nullptr,const QWidget* widget=nullptr,QStyleHintReturn* result=nullptr)const override{
        if(hint==SH_ToolTip_WakeUpDelay||hint==SH_TabBar_AllowWheelScrolling)return 0;
        if(qobject_cast<const QMenu*>(widget)){
            if(hint==SH_Menu_SubMenuPopupDelay)return 0;
            if(hint==SH_Menu_MouseTracking)return 1;
        }
        return QProxyStyle::styleHint(hint,option,widget,result);
    }

    void drawPrimitive(PrimitiveElement element,const QStyleOption* option,QPainter* painter,const QWidget* widget)const override{
        if(element==PE_FrameFocusRect||element==PE_FrameDefaultButton)return;
        QProxyStyle::drawPrimitive(element,option,painter,widget);
        if(element!=PE_PanelMenu||!qobject_cast<const QMenu*>(widget))return;
        const QColor background(widget->property("studio_menu_background").toString());
        if(!background.isValid())return;
        // Windows' native menu panel can ignore palette colors. Paint the
        // interior here while retaining the native frame and shadow.
        painter->save();painter->setRenderHint(QPainter::Antialiasing);painter->setPen(Qt::NoPen);painter->setBrush(background);
        painter->drawRoundedRect(QRectF(option->rect).adjusted(1,1,-1,-1),5,5);painter->restore();
    }

    void polish(QWidget* widget)override {
        QProxyStyle::polish(widget);
        if(widget->windowType()==Qt::Popup&&qobject_cast<QComboBox*>(widget->parentWidget()))
            widget->installEventFilter(this);
        if(auto* tabs=qobject_cast<QTabWidget*>(widget))studio_motion::installTabs(tabs);
        if(auto* table=qobject_cast<QTableView*>(widget)){
            table->setAlternatingRowColors(false);
            table->setProperty("studio_table_surface","uniform");
            table->verticalHeader()->setDefaultSectionSize(std::max(34,widget->fontMetrics().height()+14));
        }
        if(auto* bar=qobject_cast<QScrollBar*>(widget))styleStudioScrollbar(bar);
        if(auto* split=qobject_cast<QSplitter*>(widget))layout_motion::installSplitter(split);
        if(studioWindow(widget)){prepareStudioWindow(widget);widget->installEventFilter(this);}
        if(auto* menu=qobject_cast<QMenu*>(widget)){menu->setMouseTracking(true);menu->installEventFilter(this);decorateStudioMenu(menu);groupMenuEntries(menu);}
    }
    void unpolish(QWidget* widget)override {
        widget->removeEventFilter(this);
        QProxyStyle::unpolish(widget);
    }
    bool eventFilter(QObject* watched,QEvent* event)override {
        if(event->type()==QEvent::Show){
            auto* popup=qobject_cast<QWidget*>(watched);
            if(popup&&popup->windowType()==Qt::Popup)
                if(auto* combo=qobject_cast<QComboBox*>(popup->parentWidget());combo&&combo->view()->window()==popup){
                    auto* view=combo->view();
                    const QRect screen=combo->screen()->availableGeometry();
                    const auto geometry=popup->geometry();
                    const int decor=std::max(0,geometry.width()-view->viewport()->width());
                    const int scrollbar=view->style()->pixelMetric(QStyle::PM_ScrollBarExtent,nullptr,view);
                    const int required=view->sizeHintForColumn(combo->modelColumn())+
                        std::max(decor,scrollbar+2*view->frameWidth())+8;
                    const int width=std::min(screen.width(),std::max(geometry.width(),required));
                    const int anchor=combo->layoutDirection()==Qt::RightToLeft?geometry.right()-width+1:geometry.left();
                    const int left=std::clamp(anchor,screen.left(),screen.right()-width+1);
                    view->setTextElideMode(Qt::ElideNone);
                    view->setHorizontalScrollBarPolicy(required>screen.width()?Qt::ScrollBarAsNeeded:Qt::ScrollBarAlwaysOff);
                    popup->setGeometry(left,geometry.top(),width,geometry.height());
                }
        }
        if(auto* menu=qobject_cast<QMenu*>(watched);menu&&
           (event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseButtonRelease)){
            const auto* mouse=static_cast<QMouseEvent*>(event);
            auto* action=menu->actionAt(mouse->position().toPoint());
            const bool keep=mouse->button()==Qt::LeftButton&&mouse->modifiers().testFlag(Qt::ControlModifier)&&
                action&&action->isEnabled()&&action->isVisible()&&!action->isSeparator()&&!action->menu()&&!studioMenuOpensDialog(action);
            if(keep){
                menu->setActiveAction(action);
                if(event->type()==QEvent::MouseButtonRelease){
                    QPointer<QMenu> guard(menu);action->trigger();
                    if(guard)guard->update();
                }
                return true;
            }
        }
        if(event->type()==QEvent::LayoutRequest)if(auto* message=qobject_cast<QMessageBox*>(watched);message&&message->isVisible()){
            // QMessageBox handles LayoutRequest by calling setFixedSize again,
            // which cancels the Qt maximized state after a native title double click.
            // Its initial show still chooses a content-sized normal window.
            relaxWindowLimits(message);
            if(message->layout())message->layout()->activate();
            return true;
        }
        if(event->type()==QEvent::Show)if(auto* window=qobject_cast<QWidget*>(watched);studioWindow(window)){
            if(window->property(nativeFileDialog).toBool())return QProxyStyle::eventFilter(watched,event);
            if(qobject_cast<QDialog*>(window)){
                // Qt's widget file chooser also creates its own warning boxes.
                // The Show event arrives before their native window is shown.
                if(window->isModal())window->setWindowModality(Qt::NonModal);
                window->setProperty("studio_dialog_show_order",++dialogShowOrder);
            }
            configureStudioNumericForms(window);
            studio_motion::dismissMenuPresentations();
            relaxWindowLimits(window);
            if(auto* message=qobject_cast<QMessageBox*>(window);message&&!message->property("studio_message_positioned").toBool()){
                // Never use an iconic/hidden owner's frame rectangle (or Qt's
                // initial 0,0 placement) as the modal window's anchor.
                centerStudioMessage(message);message->setProperty("studio_message_positioned",true);
                QTimer::singleShot(0,message,[message]{centerStudioMessage(message);});
            }
            // QMessageBox can impose a fixed size during its own show handler.
            QTimer::singleShot(0,window,[window]{relaxWindowLimits(window);});
        }
        // Show follows aboutToShow, so lazy submenus and refreshed check states
        // are already current. No sorting is done during hover or activation.
        if(event->type()==QEvent::Show)if(auto* menu=qobject_cast<QMenu*>(watched)){
            decorateStudioMenu(menu);groupMenuEntries(menu);menu->resize(menu->sizeHint());
        }
        return QProxyStyle::eventFilter(watched,event);
    }

    QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& contents,
                           const QWidget* widget) const override {
        const auto size = QProxyStyle::sizeFromContents(type, option, contents, widget);
        if(type==CT_LineEdit||type==CT_ComboBox||type==CT_SpinBox||type==CT_PushButton)
            return {size.width(),std::max(size.height(),(widget?widget->fontMetrics().height():QFontMetrics(QApplication::font()).height())+14)};
        if(type==CT_CheckBox||type==CT_RadioButton)
            return {size.width(),std::max(size.height(),(widget?widget->fontMetrics().height():QFontMetrics(QApplication::font()).height())+10)};
        const auto* item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
        if(type==CT_MenuItem&&item&&qobject_cast<const QMenu*>(widget)&&item->menuItemType==QStyleOptionMenuItem::Separator)
            return {size.width(),MenuGroupGap};
        if (type != CT_MenuItem || !isMenuRow(item, widget)) return size;
        return {std::max(140, item->fontMetrics.horizontalAdvance(studioMenuCaption(item->text)) + menuTextInset(widget) + MenuStatusGutter),
                std::max(size.height(), MenuRowHeight)};
    }

    void drawItemText(QPainter* painter,const QRect& rect,int flags,const QPalette& palette,bool enabled,const QString& text,QPalette::ColorRole role=QPalette::NoRole)const override {
        if(!notation::contains(text)){QProxyStyle::drawItemText(painter,rect,flags,palette,enabled,text,role);return;}
        painter->save();if(role!=QPalette::NoRole)painter->setPen(palette.color(enabled?QPalette::Active:QPalette::Disabled,role));
        notation::draw(*painter,rect,flags,text);painter->restore();
    }
    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget) const override {
        if(element==CE_PushButton||element==CE_PushButtonBevel||element==CE_CheckBox||element==CE_RadioButton)
            if(const auto* button=qstyleoption_cast<const QStyleOptionButton*>(option)){
                // Native Windows styles can embed focus and default frames in their control painting.
                auto base=*button;base.state&=~State_HasFocus;
                base.features&=~QStyleOptionButton::DefaultButton;
                QProxyStyle::drawControl(element,&base,painter,widget);
                return;
            }
        if(element==CE_TabBarTab)
            if(const auto* tab=qstyleoption_cast<const QStyleOptionTab*>(option)){
                // Native tabs can embed the focus frame in the tab control.
                auto base=*tab;base.state&=~State_HasFocus;
                QProxyStyle::drawControl(element,&base,painter,widget);
                return;
            }
        if(element==CE_HeaderLabel&&widget&&widget->property("studio_table_navigation_header").toBool())
            if(const auto* header=qstyleoption_cast<const QStyleOptionHeader*>(option)){
                // Native Windows header themes can retain the system text size.
                // These data headers share the table's native scaled font; draw
                // their text directly, keeping the normal section background.
                auto base=*header;base.text.clear();QProxyStyle::drawControl(element,&base,painter,widget);
                painter->save();painter->setFont(widget->font());
                painter->setPen(header->palette.color(header->state.testFlag(State_Enabled)?QPalette::Active:QPalette::Disabled,QPalette::ButtonText));
                const int pad=qMax(1,qRound(6*widget->property("studio_table_scale").toDouble()));
                // CE_HeaderLabel can already have native padding removed.
                // Use the actual section once, so scaling never double-insets
                // the text and clips a closing parenthesis or superscript.
                QRect section=header->rect;
                if(const auto* view=qobject_cast<const QHeaderView*>(widget)){
                    const int position=view->sectionViewportPosition(header->section);
                    const int size=view->sectionSize(header->section);
                    section=view->orientation()==Qt::Horizontal
                        ?QRect(position,0,size,view->viewport()->height())
                        :QRect(0,position,view->viewport()->width(),size);
                }
                painter->setClipRect(section,Qt::IntersectClip);
                const auto textRect=section.adjusted(pad,0,-pad,0);
                if(notation::contains(header->text))notation::draw(*painter,textRect,header->textAlignment,header->text);
                else painter->drawText(textRect,header->textAlignment,header->text);
                painter->restore();return;
            }
        if(element==CE_ItemViewItem)if(const auto* cell=qstyleoption_cast<const QStyleOptionViewItem*>(option);cell&&notation::contains(cell->text)){
            auto base=*cell;base.text.clear();QProxyStyle::drawControl(element,&base,painter,widget);
            painter->save();painter->setFont(cell->font);const auto group=cell->state.testFlag(State_Enabled)?(cell->state.testFlag(State_Active)?QPalette::Active:QPalette::Inactive):QPalette::Disabled;
            painter->setPen(cell->palette.color(group,cell->state.testFlag(State_Selected)?QPalette::HighlightedText:QPalette::Text));
            notation::draw(*painter,subElementRect(SE_ItemViewItemText,cell,widget),int(cell->displayAlignment)|(cell->features.testFlag(QStyleOptionViewItem::WrapText)?Qt::TextWordWrap:0),cell->text);painter->restore();return;
        }
        if(element==CE_HeaderLabel)if(const auto* header=qstyleoption_cast<const QStyleOptionHeader*>(option);header&&notation::contains(header->text)){
            auto base=*header;base.text.clear();QProxyStyle::drawControl(element,&base,painter,widget);
            painter->save();if(widget)painter->setFont(widget->font());painter->setPen(header->palette.color(header->state.testFlag(State_Enabled)?QPalette::Active:QPalette::Disabled,QPalette::ButtonText));
            notation::draw(*painter,subElementRect(SE_HeaderLabel,header,widget),header->textAlignment,header->text);painter->restore();return;
        }
        const auto* item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
        if(element==CE_MenuItem&&item&&qobject_cast<const QMenu*>(widget)&&item->menuItemType==QStyleOptionMenuItem::Separator){
            painter->save();painter->setPen(QColor("#dde7e2"));
            painter->drawLine(item->rect.left()+8,item->rect.center().y(),item->rect.right()-8,item->rect.center().y());painter->restore();return;
        }
        if (element != CE_MenuItem || !isMenuRow(item, widget)) {
            QProxyStyle::drawControl(element, option, painter, widget);
            return;
        }

        const bool enabled = item->state.testFlag(State_Enabled);
        const bool active = enabled && item->state.testFlag(State_Selected);
        const bool submenu = item->menuItemType == QStyleOptionMenuItem::SubMenu;
        const bool checkable = item->checkType != QStyleOptionMenuItem::NotCheckable;
        const QRect row = item->rect;
        const auto* menu=qobject_cast<const QMenu*>(widget);
        const auto* action=menu?menu->actionAt(row.center()):nullptr;
        const bool opensDialog=!submenu&&!checkable&&(action?studioMenuOpensDialog(action):dialogCaption(item->text));
        const QColor rowTint(action?action->property("studio_menu_tint").toString():QString{});
        const auto visual = [&](QRect rect) { return visualRect(item->direction, row, rect); };
        const QColor ink(enabled ? "#143d34" : "#929b96");

        painter->save();
        // Full native popup geometry and input area exist from the first frame.
        // Only row ink settles; never use QWidget::setMask/windowOpacity here.
        painter->setOpacity(painter->opacity()*(.45+.55*studio_motion::menuProgress(menu)));
        painter->setRenderHint(QPainter::Antialiasing);
        if (active || rowTint.isValid()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(rowTint.isValid()?(active?QColor("#d2e4ee"):rowTint):QColor("#e0efe9"));
            painter->drawRoundedRect(row.adjusted(4, 1, -4, -1), 5, 5);
        }

        if (!item->icon.isNull()) {
            const QSize size = item->icon.name().startsWith("studio-format:") ? FormatIconSize : QSize(16,16);
            const QRect icon(row.left() + 9, row.center().y() - size.height()/2, size.width(), size.height());
            painter->save();if(!enabled)painter->setOpacity(painter->opacity()*.72);
            item->icon.paint(painter, visual(icon), Qt::AlignCenter, enabled ? QIcon::Normal : QIcon::Disabled);painter->restore();
        }
        painter->setPen(ink);
        auto font = item->font;
        if (item->menuItemType == QStyleOptionMenuItem::DefaultItem) font.setBold(true);
        painter->setFont(font);
        const auto caption=studioMenuCaption(item->text);
        auto label = row.adjusted(menuTextInset(widget), 0, -MenuStatusGutter, 0);
        const auto alignment = visualAlignment(item->direction, Qt::AlignLeft | Qt::AlignVCenter);
        const int mnemonic = styleHint(SH_UnderlineShortcut, item, widget) ? Qt::TextShowMnemonic : Qt::TextHideMnemonic;
        notation::draw(*painter,visual(label),int(alignment)|mnemonic,caption);

        const QPointF mark = visual(QRect(row.right() - 25, row.top(), 23, row.height())).center();
        if (submenu) {
            const qreal direction = item->direction == Qt::RightToLeft ? -1 : 1;
            painter->setPen(QPen(ink, 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPolyline(QPolygonF{mark + QPointF(-2 * direction, -3.4), mark + QPointF(1.5 * direction, 0),
                                            mark + QPointF(-2 * direction, 3.4)});
        } else if(opensDialog) {
            painter->setPen(Qt::NoPen);painter->setBrush(ink);
            for(qreal offset:{-4.,0.,4.})painter->drawEllipse(mark+QPointF(offset,0),1.25,1.25);
        } else if (checkable) {
            const double progress=studio_motion::checkProgress(menu,action,item->checked?1:0);
            const qreal opacity=painter->opacity();
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(enabled ? "#d6ebe2" : "#eef1ee"));
            painter->setOpacity(opacity*progress);
            painter->drawEllipse(mark, 8, 8);
            painter->setPen(QPen(QColor(enabled ? "#00675d" : "#a4ada7"), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPolyline(QPolygonF{mark + QPointF(-3.4, 0), mark + QPointF(-1, 2.4), mark + QPointF(3.6, -2.5)});
            painter->setOpacity(opacity*(1-progress));
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(QColor(enabled?"#6f8b81":"#b0b8b2"),1.4,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            painter->drawEllipse(mark,6.5,6.5);
            painter->setOpacity(opacity);
        } else {
            // Direct action: a single solid dot; state toggles retain check/circle.
            painter->setPen(Qt::NoPen);
            painter->setBrush(ink);
            painter->drawEllipse(mark,2.8,2.8);
        }
        painter->restore();
    }
};
}

QString studioMenuCaption(const QString& text) {
    auto caption=text.section('\t',0,0).trimmed();
    if(caption.endsWith(QChar(0x2026)))caption.chop(1);
    else if(caption.endsWith("..."))caption.chop(3);
    return caption.trimmed();
}
bool studioMenuOpensDialog(const QAction* action) {
    if(!action||action->menu()||action->isCheckable())return false;
    const auto explicitKind=action->property("studio_opens_dialog");
    return explicitKind.isValid()?explicitKind.toBool():dialogCaption(action->text());
}

void applyStudioToolTipPolicy() {
    if (!dynamic_cast<StudioMenuStyle*>(QApplication::style()))
        QApplication::setStyle(new StudioMenuStyle(QApplication::style()->objectName()));
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip,false);
    QApplication::setEffectEnabled(Qt::UI_FadeTooltip,false);
}

void applyStudioTheme() {
    applyStudioToolTipPolicy();
    QApplication::setEffectEnabled(Qt::UI_AnimateMenu,false);
    QApplication::setEffectEnabled(Qt::UI_FadeMenu,false);
    installStudioCaptionEvents();
    const QIcon brand(":/brand/freeship26.png");if(!brand.isNull())QApplication::setWindowIcon(brand);
    QFont face("Microsoft YaHei UI");face.setPointSizeF(10.5);
    face.setStyleStrategy(QFont::StyleStrategy(QFont::PreferAntialias|QFont::PreferQuality));
    QApplication::setFont(studioTextFont(face));
    auto palette=QApplication::palette();
    palette.setColor(QPalette::Window,QColor(246,249,247));
    palette.setColor(QPalette::WindowText,QColor(30,53,50));
    palette.setColor(QPalette::Base,QColor(250,252,250));
    // A common fallback also prevents new views that enable alternation after polish
    // from reintroducing green/white data stripes. Selection/header colors remain distinct.
    palette.setColor(QPalette::AlternateBase,palette.color(QPalette::Base));
    palette.setColor(QPalette::Button,QColor(237,244,240));
    palette.setColor(QPalette::Text,QColor(30,53,50));
    palette.setColor(QPalette::Highlight,QColor(0,126,116));
    palette.setColor(QPalette::HighlightedText,Qt::white);
    QApplication::setPalette(palette);
    qApp->setStyleSheet(qApp->styleSheet()+QStringLiteral(
        // Stylesheet-painted controls also share the focus-frame policy.
        "QWidget {outline:none;}"
        "QComboBox QAbstractItemView {selection-background-color:#dcebe3;selection-color:#1e3532;}"
        "QPushButton[studio_format_button=true] {background:#edf3ef;border:1px solid #d5e2db;"
        "border-radius:5px;padding:3px 7px;}"
        "QPushButton[studio_format_button=true]:enabled:hover,"
        "QPushButton[studio_format_button=true]:enabled:focus {background:#e0ede5;}"
        "QPushButton[studio_format_button=true]:pressed,"
        "QPushButton[studio_format_button=true]:open {background:#d5e7dd;}"
        "QPushButton[studio_format_button=true]:disabled {background:#f1f5f2;}"
        "QPushButton[studio_format_button=true]::menu-indicator {subcontrol-origin:padding;"
        "subcontrol-position:right center;right:5px;}"
        "QTabWidget::pane {border:0;background:transparent;}"
        "QTabWidget::tab-bar {left:0px;}"
        "QTabBar::tab {background:transparent;color:#526f66;border:0;border-bottom:2px solid transparent;"
        "padding:9px 16px;margin:0 4px 8px 0;min-height:20px;}"
        "QTabBar::tab:hover {background:#edf3ef;color:#245b51;}"
        "QTabBar::tab:selected {background:#e2eee7;color:#007e74;border-bottom-color:#007e74;}"
        "QTabBar[studio_tab_motion=true]::tab:selected {border-bottom-color:transparent;}"
        "QTabBar::tab:disabled {color:#929b96;}"));
}
void showStudioDialog(QDialog& dialog){
    bool nativeFile=false;
    if(auto* file=qobject_cast<QFileDialog*>(&dialog)){
#ifdef Q_OS_WIN
        // The Windows Shell picker requires a modal owner.
        nativeFile=QGuiApplication::platformName()=="windows"
            && !QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs)
            && !file->testAttribute(Qt::WA_DontShowOnScreen)
            && !file->testOption(QFileDialog::DontUseNativeDialog);
#endif
        if(!nativeFile)file->setOption(QFileDialog::DontUseNativeDialog);
        file->setProperty(nativeFileDialog,nativeFile);
    }
    dialog.setWindowModality(nativeFile?Qt::WindowModal:Qt::NonModal);
    if(auto* color=qobject_cast<QColorDialog*>(&dialog))color->setOption(QColorDialog::DontUseNativeDialog);
    if(auto* message=qobject_cast<QMessageBox*>(&dialog))message->setOption(QMessageBox::Option::DontUseNativeDialog);
    dialog.setProperty("studio_dialog_show_order",++dialogShowOrder);
    dialog.show();
}
int runStudioDialog(QDialog& dialog){
    for(auto* widget:QApplication::topLevelWidgets())if(auto* other=qobject_cast<QDialog*>(widget);
        other&&other!=&dialog&&other->isVisible()&&other->parentWidget()==dialog.parentWidget()&&
        other->property("studio_dialog_waiting").toBool()&&other->windowTitle()==dialog.windowTitle()&&
        other->objectName()==dialog.objectName()&&other->metaObject()==dialog.metaObject()){
        other->raise();other->activateWindow();return QDialog::Rejected;
    }
    dialog.setProperty("studio_dialog_waiting",true);
    auto* file=qobject_cast<QFileDialog*>(&dialog);
    const bool confirmOverwrite=file&&file->acceptMode()==QFileDialog::AcceptSave&&!file->testOption(QFileDialog::DontConfirmOverwrite);
    if(confirmOverwrite)file->setOption(QFileDialog::DontConfirmOverwrite);
    const auto result=DialogWait(dialog).run();
    dialog.setProperty("studio_dialog_waiting",false);
    if(confirmOverwrite){
        file->setOption(QFileDialog::DontConfirmOverwrite,false);
        const auto path=file->selectedFiles().value(0);
        if(result==QDialog::Accepted&&QFileInfo::exists(path)&&
           !confirmStudioAction(dialog.parentWidget(),uiText("替换文件"),uiText("文件已存在，是否替换？\n")+path))
            return QDialog::Rejected;
    }
    return result;
}
bool hasPendingStudioDialog(const QWidget* owner){return owner&&owner->property(waitingDialogCount).toInt()>0;}
QDialog* frontStudioDialog(){
    QDialog* front=nullptr;
    quint64 order=0;
    for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QDialog*>(widget);dialog&&dialog->isVisible()){
        const auto shown=dialog->property("studio_dialog_show_order").toULongLong();
        if(!front||shown>order){front=dialog;order=shown;}
    }
    return front;
}
namespace {
QStringList studioFileSelection(QWidget* parent,const QString& title,const QString& path,const QString& filter,
                               QFileDialog::AcceptMode accept,QFileDialog::FileMode mode){
    QFileDialog picker(parent,title,{},filter);
    picker.setAcceptMode(accept);picker.setFileMode(mode);
    picker.setOption(QFileDialog::ShowDirsOnly,mode==QFileDialog::Directory);
    const QFileInfo initial(path);
    if(initial.isDir())picker.setDirectory(initial.absoluteFilePath());
    else if(!path.isEmpty()){
        picker.setDirectory(initial.absolutePath());picker.selectFile(initial.fileName());
    }
    return runStudioDialog(picker)==QDialog::Accepted?picker.selectedFiles():QStringList{};
}
}
QString studioOpenFile(QWidget* parent,const QString& title,const QString& path,const QString& filter){
    return studioFileSelection(parent,title,path,filter,QFileDialog::AcceptOpen,QFileDialog::ExistingFile).value(0);
}
QStringList studioOpenFiles(QWidget* parent,const QString& title,const QString& path,const QString& filter){
    return studioFileSelection(parent,title,path,filter,QFileDialog::AcceptOpen,QFileDialog::ExistingFiles);
}
QString studioSaveFile(QWidget* parent,const QString& title,const QString& path,const QString& filter){
    return studioFileSelection(parent,title,path,filter,QFileDialog::AcceptSave,QFileDialog::AnyFile).value(0);
}
QString studioChooseDirectory(QWidget* parent,const QString& title,const QString& path){
    return studioFileSelection(parent,title,path,{},QFileDialog::AcceptOpen,QFileDialog::Directory).value(0);
}
QColor studioChooseColor(const QColor& initial,QWidget* parent,const QString& title,QColorDialog::ColorDialogOptions options){
    QColorDialog dialog(initial,parent);
    dialog.setWindowTitle(title);dialog.setOptions(options|QColorDialog::DontUseNativeDialog);
    return runStudioDialog(dialog)==QDialog::Accepted?dialog.selectedColor():QColor{};
}
bool confirmStudioAction(QWidget* parent,const QString& title,const QString& question){
    QMessageBox dialog(QMessageBox::Question,title,question,QMessageBox::Yes|QMessageBox::No,parent);
    dialog.setAttribute(Qt::WA_DontShowOnScreen,parent&&parent->testAttribute(Qt::WA_DontShowOnScreen));
    dialog.button(QMessageBox::Yes)->setText(uiText("确认"));dialog.button(QMessageBox::No)->setText(uiText("取消"));
    dialog.setDefaultButton(QMessageBox::No);return runStudioDialog(dialog)==QMessageBox::Yes;
}
void applyStudioScrollbars(QAbstractScrollArea* area) {
    if(auto* table=qobject_cast<QTableView*>(area)) {
        table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        table->horizontalScrollBar()->setSingleStep(12);
    }
    styleStudioScrollbar(area->horizontalScrollBar());styleStudioScrollbar(area->verticalScrollBar());
}
QPushButton* studioFolderButton(const QString& label){
    auto* button=new QPushButton;
    button->setObjectName("StudioFolderButton");
    button->setIcon(studioIcon("OpenFile"));
    button->setIconSize(QSize(20,20));
    button->setToolTip(label);
    button->setAccessibleName(label);
    button->setAutoDefault(false);
    return button;
}

QIcon studioIcon(const QString& key) {
    if(auto icon=studioSupplementIcon(key);!icon.isNull())return icon;
    if(key=="Tutorial")return QIcon(new TutorialIcon);
    if(key=="ImportFile")return QIcon(new ImportIcon);
    if(key=="Shortcuts")return QIcon(new KeyboardIcon);
    if(key=="AddOne"||key=="AddRange"||key=="DeleteOne"||key=="DeleteMany")
        return QIcon(new CountedActionIcon(key.startsWith("Delete"),key=="AddRange"||key=="DeleteMany"));
    const auto root=QCoreApplication::applicationDirPath()+"/bridge/Themes/StudioLight/icons/";
    QFile file(root+"manifest.json"); QString resolved=key=="Open"?"OpenFile":key=="Preferences"?"ProjectSettings":key=="Develop"?"Developability":key;
    if(resolved=="Developability")return studioSupplementIcon(resolved);
    if(file.open(QIODevice::ReadOnly)) {
        const auto aliases=QJsonDocument::fromJson(file.readAll()).object()["aliases"].toObject();
        if(aliases.contains(key))resolved=aliases[key].toString();
    }
    QIcon icon;
    for(int size:{16,20,22,24,25,30,32,40,48,64,96,128}) {
        const auto path=root+QString::number(size)+"/"+resolved+".png";
        if(QFile::exists(path))icon.addFile(path,QSize(size,size));
    }
    return icon;
}
QIcon studioVendorIcon(const QString& vendor){
    static QHash<QString,QIcon> cache;
    if(!cache.contains(vendor))cache.insert(vendor,QIcon(new VendorIcon(vendor)));
    return cache.value(vendor);
}
QIcon studioFormatIcon(const QString& format,bool importing) {
    const auto name = format.toUpper();
    const auto& letters = formatLetterStrokes();
    if (name.isEmpty() || name.size() > 4 || std::any_of(name.begin(), name.end(), [&](QChar letter) { return !letters.contains(letter); }))
        return {};
    return QIcon(new StudioFormatIcon(name,false,importing));
}
QIcon studioFormatWindowIcon(const QString& format,bool importing){
    if(studioFormatIcon(format).isNull())return {};
    return QIcon(new StudioFormatIcon(format.toUpper(),true,importing));
}
void setStudioWindowIcon(QWidget* window,const QString& key){
    const auto icon=key=="brand"?QIcon(":/brand/freeship26.png"):studioIcon(key);
    if(icon.isNull())return;
    window->setWindowIcon(icon);window->setProperty("studio_window_icon",key);
}
void setStudioFormatButton(QAbstractButton* button, const QString& format, const QString& description) {
    const auto icon = studioFormatIcon(format);
    if (icon.isNull()) return;
    button->setProperty("studio_format_button",true);
    button->setText({});
    button->setIcon(icon);
    button->setIconSize(FormatIconSize);
    button->setMinimumSize(52, 32);
    button->setToolTip(description);
    button->setAccessibleName(description);
}
void setStudioFormatMenuAction(QMenu* menu, QAction* action, const QString& format,bool importing) {
    action->setProperty("studio_opens_dialog",true);
    const auto icon = studioFormatIcon(format,importing);
    if (icon.isNull()) return;
    menu->setProperty("studio_format_icons", true);
    action->setIcon(icon);
}
StudioMessage::StudioMessage(StudioMessageRole role, const QString& text, QWidget* parent)
    : QLabel(parent), role_(role) {
    setWordWrap(true);
    setTextFormat(Qt::PlainText);
    setTextInteractionFlags(Qt::TextSelectableByMouse);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    // QSS owns the content insets. QWidget margins are reset by the style's
    // polish pass, which previously let the first text glyph cover the icon.
    setMinimumHeight(40);
    auto policy = sizePolicy();
    policy.setHorizontalPolicy(QSizePolicy::Preferred);
    policy.setVerticalPolicy(QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    auto face = font();
    face.setPointSizeF(qMax(9.5, face.pointSizeF()));
    setFont(face);
    setMessage(text, role);
}
void StudioMessage::setMessage(const QString& text, StudioMessageRole role, bool working) {
    role_ = role;
    working_ = working;
    const auto style=studioMessageStyle(role);ink_=style.foreground;
    setProperty("studio_message_role",style.name);
    setProperty("studio_content_role","message");
    setProperty("studio_message_busy",working);
    setProperty("studio_message_foreground",style.foreground.name());
    setProperty("studio_message_background",style.background.name());
    setStyleSheet(QString("QLabel { color:%1; background:%2; border:1px solid %3; border-radius:6px; padding:10px 12px 10px 38px; }")
        .arg(style.foreground.name(),style.background.name(),style.border.name()));
    setText(text);
    message_motion::updated(this,working);
}
void StudioMessage::setText(const QString& text) {
    const auto display=mathNotation_?formatNumericRanges(text):text;
    const bool math=mathNotation_&&notation::contains(display);
    setTextFormat(math?Qt::RichText:Qt::PlainText);
    if(mathNotation_)setAccessibleName(display);
    QLabel::setText(math?notation::html(display,font().pointSizeF()).replace('\n',"<br>"):display);
    setVisible(!text.isEmpty());
}
void StudioMessage::setVisible(bool visible) {
    // Both text updates and conditional show() calls can happen before layout
    // adoption. Such a label is not an independent application window.
    if(visible&&!parentWidget()){setAttribute(Qt::WA_WState_ExplicitShowHide,false);return;}
    if(parentWidget()&&message_motion::visibility(this,visible))return;
    QLabel::setVisible(visible);
}
void StudioMessage::clear() { setText({}); }
void StudioMessage::paintEvent(QPaintEvent* event) {
    QLabel::paintEvent(event);
    if (text().isEmpty()) return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(12, 12);
    if(working_){painter.translate(8,8);painter.rotate(property("studio_message_busy_phase").toDouble()*360);painter.translate(-8,-8);}
    if(!working_&&role_==StudioMessageRole::Feedback&&property("studio_message_confirmation_phase").isValid())painter.setClipRect(QRectF(0,0,17*property("studio_message_confirmation_phase").toDouble(),17));
    paintStudioMessageMark(painter,role_,working_);
}

StudioBar::StudioBar(QWidget* parent):QWidget(parent) {
    hoverMenuTimer_=new QTimer(this);hoverMenuTimer_->setSingleShot(true);hoverMenuTimer_->setInterval(90);
    connect(hoverMenuTimer_,&QTimer::timeout,this,[this]{showHoverMenu();});
    closeMenuTimer_=new QTimer(this);closeMenuTimer_->setSingleShot(true);closeMenuTimer_->setInterval(220);
    connect(closeMenuTimer_,&QTimer::timeout,this,[this]{
        if(openMenu_&&isVisible()&&hasMenuPointer_&&!insideHoverMenu(lastMenuPointer_))openMenu_->close();
    });
    setObjectName("StudioToolbar");setFixedHeight(58);
    const auto buttonsStyle=QStringLiteral(
        "QToolButton {color:#173b37;background:transparent;border:1px solid transparent;border-radius:2px;padding:2px;}"
        "QToolButton:hover {background:#e7f2fc;border-color:#abd5f5;}"
        "QToolButton:checked,QToolButton:pressed {background:#cce8ff;border-color:#8dcaff;}"
        "QToolButton:disabled {color:#88938f;} QToolButton::menu-indicator {image:none;}"
        "QToolButton[studio_motion_button=true],QToolButton[studio_motion_button=true]:hover,QToolButton[studio_motion_button=true]:checked,QToolButton[studio_motion_button=true]:pressed {background:transparent;border-color:transparent;}");
    setStyleSheet("#StudioToolbar {background:#e0ece8;} "+buttonsStyle);
    sideBar_=new QToolBar(parent);
    sideBar_->setObjectName("StudioSideToolbar");
    sideBar_->setAllowedAreas(Qt::RightToolBarArea);
    sideBar_->setMovable(false);
    sideBar_->setFloatable(false);
    sideBar_->setStyleSheet("QToolBar#StudioSideToolbar {background:#e0ece8;border:0;margin:0;padding:0;spacing:0;} "
        "QScrollArea#StudioSideScroll {background:#e0ece8;border:0;} "+buttonsStyle);
    sideScroll_=new QScrollArea(sideBar_);
    sideScroll_->setObjectName("StudioSideScroll");
    sideScroll_->setFrameShape(QFrame::NoFrame);
    sideScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideScroll_->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Expanding);
    sideScroll_->setWidgetResizable(true);
    sideContents_=new QWidget;
    sideContents_->setObjectName("StudioSideContents");
    sideContents_->setAttribute(Qt::WA_StyledBackground);
    sideContents_->setStyleSheet("#StudioSideContents {background:#e0ece8;}");
    sideScroll_->setWidget(sideContents_);
    sideBar_->addWidget(sideScroll_);
    sideBar_->hide();
    qApp->installEventFilter(this);
}
namespace {
QMenu* studioButtonMenu(QToolButton* button){
    if(!button)return nullptr;
    if(auto* menu=button->menu())return menu;
    return button->defaultAction()?button->defaultAction()->menu():nullptr;
}
bool toolbarPointerReceiver(QObject* receiver,QWidget* main,QMenu* openMenu){
    auto* widget=qobject_cast<QWidget*>(receiver);
    auto* surface=widget?widget->window()->windowHandle():qobject_cast<QWindow*>(receiver);
    if(widget&&widget->window()==main)return true;
    if(!surface)return false;
    if(surface==main->windowHandle())return true;
    // A popup can own mouse delivery when the pointer is over its toolbar.
    // Only this toolbar's visible menu chain shares that behavior.
    QList<QMenu*> menus;
    if(openMenu)menus.append(openMenu);
    while(!menus.isEmpty()){
        auto* menu=menus.takeLast();
        if(!menu->isVisible())continue;
        if(menu->windowHandle()==surface)return true;
        for(auto* action:menu->actions())if(action->menu())menus.append(action->menu());
    }
    return false;
}
void traceMenuSurface(QMenu* menu,const char* phase){
    // Bounded local diagnostics for an ordinary launch only. No model names,
    // captions, paths, mouse movement or user preferences are recorded.
    if(!qApp||qApp->applicationName()!="FreeShip_26 Qt")return;
    for(const auto& argument:qApp->arguments())if(argument.startsWith("--verify-"))return;
    static int remaining=128;if(remaining<=0)return;--remaining;
    QJsonObject row{{"utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"pid",double(QCoreApplication::applicationPid())},{"phase",phase},
        {"qt_visible",menu->isVisible()},{"opacity",menu->windowOpacity()},
        {"mask_empty",menu->mask().isEmpty()},{"width",menu->width()},{"height",menu->height()},
        {"progress",menu->property("studio_motion_progress").toDouble()},
        {"active_popup_is_menu",QApplication::activePopupWidget()==menu},
        {"active_modal_present",QApplication::activeModalWidget()!=nullptr}};
#ifdef Q_OS_WIN
    if(menu->internalWinId()){
        const auto hwnd=reinterpret_cast<HWND>(menu->internalWinId());
        row["native_visible"]=bool(IsWindowVisible(hwnd));
        HRGN region=CreateRectRgn(0,0,0,0);row["native_region_kind"]=GetWindowRgn(hwnd,region);DeleteObject(region);
    }
#endif
    const auto folder=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/FreeShip26/Diagnostics";
    if(!QDir().mkpath(folder))return;
    QFile file(folder+"/menu-surface.jsonl");
    const auto mode=QIODevice::WriteOnly|(file.size()>256*1024?QIODevice::Truncate:QIODevice::Append);
    if(file.open(mode))file.write(QJsonDocument(row).toJson(QJsonDocument::Compact)+'\n');
}
}
void StudioBar::watchMenuButton(QToolButton* button){
    studio_motion::installMenu(studioButtonMenu(button));
    auto* menu=studioButtonMenu(button);if(!menu)return;
    button->setMouseTracking(true);button->setProperty("studio_hover_popup",true);
    button->setAttribute(Qt::WA_Hover,true);button->installEventFilter(this);
    connect(menu,&QMenu::aboutToShow,this,[this,button,menu]{
        closeMenuTimer_->stop();
        if(openMenuButton_&&openMenuButton_!=button)openMenuButton_->setDown(false);
        openMenu_=menu;openMenuButton_=button;button->setDown(true);
        traceMenuSurface(menu,"about-to-show");
        for(int delay:{0,80,250})QTimer::singleShot(delay,menu,[menu,delay]{traceMenuSurface(menu,delay==0?"shown-turn":delay==80?"shown-80ms":"shown-250ms");});
    });
    connect(menu,&QMenu::aboutToHide,this,[this,button,menu]{
        traceMenuSurface(menu,"about-to-hide");
        button->setDown(false);
        if(openMenu_==menu){openMenu_.clear();openMenuButton_.clear();closeMenuTimer_->stop();keepMenuRelease_=false;}
    });
}
void StudioBar::setHoveredMenuButton(QToolButton* button){
    if(button&&(!button->property("studio_hover_popup").toBool()||!button->isEnabled()))button=nullptr;
    if(hoveredMenuButton_==button)return;
    if(hoveredMenuButton_)studio_motion::setMenuButtonHovered(hoveredMenuButton_,false);
    hoveredMenuButton_=button;
    if(button)studio_motion::setMenuButtonHovered(button,true);
}
void StudioBar::queueHoverMenu(QToolButton* button){
    if(!button||!button->isVisible()||!button->isEnabled()||!button->property("studio_hover_popup").toBool()){
        hoverMenuTimer_->stop();pendingMenuButton_.clear();return;
    }
    closeMenuTimer_->stop();
    if(openMenuButton_==button&&openMenu_&&openMenu_->isVisible()){
        hoverMenuTimer_->stop();pendingMenuButton_.clear();return;
    }
    if(pendingMenuButton_==button&&hoverMenuTimer_->isActive())return;
    pendingMenuButton_=button;hoverMenuTimer_->start();
}
void StudioBar::showHoverMenu(){
    const auto button=pendingMenuButton_;pendingMenuButton_.clear();
    // Dragging cancels the pending request from its own input events. Do not
    // gate Enter-only hover on QApplication's possibly stale native-drag state.
    // Keep normal menu dismissal, keyboard use and click behavior. Close only
    // this toolbar's previous popup; unrelated context menus are left alone.
    if(auto* popup=QApplication::activePopupWidget();popup&&!openMenu_){
        if(auto* menu=studioButtonMenu(button))traceMenuSurface(menu,"hover-blocked-by-other-popup");
        return;
    }
    openButtonMenu(button);
}
void StudioBar::openButtonMenu(QToolButton* button){
    if(!button||!isVisible()||!button->isVisible()||!button->isEnabled())return;
    auto* menu=studioButtonMenu(button);if(!menu)return;
    if(QApplication::activeModalWidget()){traceMenuSurface(menu,"open-blocked-by-modal");return;}
    hoverMenuTimer_->stop();pendingMenuButton_.clear();closeMenuTimer_->stop();
    lastMenuPointer_=button->mapToGlobal(button->rect().center());hasMenuPointer_=true;
    if(openMenu_==menu&&menu->isVisible())return;
    if(openMenu_)openMenu_->close();
    if(sideContents_->isAncestorOf(button))
        menu->popup(button->mapToGlobal(QPoint(-menu->sizeHint().width(),0)));
    else
        menu->popup(button->mapToGlobal(QPoint(0,button->height())));
}
bool StudioBar::insideHoverMenu(const QPoint& global) const{
    if(!openMenu_||!openMenu_->isVisible())return false;
    QRegion region;
    if(openMenuButton_)region+=QRect(openMenuButton_->mapToGlobal(QPoint()),openMenuButton_->size()).adjusted(-12,-10,12,10);
    QSet<QMenu*> seen;
    std::function<void(QMenu*)> add=[&](QMenu* menu){
        if(!menu||seen.contains(menu))return;seen.insert(menu);
        if(menu->isVisible())region+=menu->frameGeometry().adjusted(-12,-10,12,10);
        for(auto* action:menu->actions())if(action->menu())add(action->menu());
    };
    add(openMenu_);return region.contains(global);
}
void StudioBar::trackHoverMenu(const QPoint& global,bool dragging){
    lastMenuPointer_=global;hasMenuPointer_=true;
    if(!isVisible()||!openMenu_||dragging||hoverMenuTimer_->isActive()||insideHoverMenu(global))closeMenuTimer_->stop();
    else if(!closeMenuTimer_->isActive())closeMenuTimer_->start();
}
bool StudioBar::eventFilter(QObject* watched,QEvent* event){
    if(watched==window()&&event->type()==QEvent::LanguageChange)
        QTimer::singleShot(0,this,[this]{layoutButtons();});
    const auto type=event->type();
    // Popup grabs can route pointer motion to another widget without sending
    // Leave to the tool button. Keyboard use also supersedes a pending hover.
    if(type==QEvent::KeyPress||type==QEvent::Shortcut){
        setHoveredMenuButton(nullptr);
        hoverMenuTimer_->stop();pendingMenuButton_.clear();
    }
    const bool click=type==QEvent::MouseButtonPress||type==QEvent::MouseButtonDblClick||type==QEvent::MouseButtonRelease;
    const bool tracking=click||type==QEvent::MouseMove||type==QEvent::WindowActivate;
    if(tracking&&!toolbarPointerReceiver(watched,window(),openMenu_)){
        setHoveredMenuButton(nullptr);
        hoverMenuTimer_->stop();pendingMenuButton_.clear();keepMenuRelease_=false;
        return QWidget::eventFilter(watched,event);
    }
    if(auto* modal=QApplication::activeModalWidget();modal&&modal!=window())
        return QWidget::eventFilter(watched,event);
    if(click){
        const auto* mouse=static_cast<QMouseEvent*>(event);
        if(event->type()!=QEvent::MouseButtonRelease){hoverMenuTimer_->stop();pendingMenuButton_.clear();}
        if(mouse->button()==Qt::LeftButton){
            if(event->type()==QEvent::MouseButtonRelease&&keepMenuRelease_){
                keepMenuRelease_=false;trackHoverMenu(mouse->globalPosition().toPoint(),false);return true;
            }
            if(event->type()!=QEvent::MouseButtonRelease&&isVisible()){
                // Clicking and hovering use one non-blocking popup path. Do not
                // enter QToolButton's nested exec() loop and then race its replay
                // of the same outside press/release against the hover controller.
                const auto global=mouse->globalPosition().toPoint();
                if(auto* button=buttonAt(global);button&&button->property("studio_hover_popup").toBool()&&button->isEnabled()){
                    openButtonMenu(button);keepMenuRelease_=true;return true;
                }
            }
        }
    }
    if(event->type()==QEvent::Enter||event->type()==QEvent::HoverEnter||event->type()==QEvent::HoverMove){
        if(auto* button=qobject_cast<QToolButton*>(watched);button&&(isAncestorOf(button)||sideContents_->isAncestorOf(button))){
            setHoveredMenuButton(button);
            queueHoverMenu(button);
        }
    }else if(event->type()==QEvent::FocusIn){
        if(auto* button=qobject_cast<QToolButton*>(watched);button&&sideContents_->isAncestorOf(button))
            sideScroll_->ensureWidgetVisible(button,0,2);
    }else if(event->type()==QEvent::Leave||event->type()==QEvent::Hide){
        if(watched==hoveredMenuButton_)setHoveredMenuButton(nullptr);
        if(watched==pendingMenuButton_){hoverMenuTimer_->stop();pendingMenuButton_.clear();}
    }else if(event->type()==QEvent::MouseMove){
        // A popup grabs mouse delivery. Use that event's position to switch
        // toolbar menus even while the receiving widget is the previous menu.
        const auto* mouse=static_cast<QMouseEvent*>(event);
        auto* button=mouse->buttons()==Qt::NoButton?buttonAt(mouse->globalPosition().toPoint()):nullptr;
        setHoveredMenuButton(button);
        queueHoverMenu(button);
        trackHoverMenu(mouse->globalPosition().toPoint(),mouse->buttons()!=Qt::NoButton);
    }else if(event->type()==QEvent::ApplicationDeactivate){
        setHoveredMenuButton(nullptr);
        hoverMenuTimer_->stop();closeMenuTimer_->stop();pendingMenuButton_.clear();
        if(openMenu_)openMenu_->close();
    }
    return QWidget::eventFilter(watched,event);
}
QList<QToolButton*> StudioBar::buttons() const{
    QList<QToolButton*> result;
    for(const auto& group:groups_)result.append(group.buttons);
    return result;
}
QToolButton* StudioBar::buttonAt(const QPoint& global) const{
    const auto* viewport=sideScroll_->viewport();
    if(!isVisible()||(!rect().contains(mapFromGlobal(global))&&
                      (!sideBar_->isVisible()||!viewport->rect().contains(viewport->mapFromGlobal(global)))))return nullptr;
    for(const auto& group:groups_)
        for(auto* button:group.buttons)
            if(button->isVisible()&&button->rect().contains(button->mapFromGlobal(global)))return button;
    return nullptr;
}
void StudioBar::addGroup(const QList<QAction*>& actions,QColor background) {
    Group group{new QWidget(this),new QWidget(sideContents_),{}};
    for(auto* panel:{group.top,group.side}){
        panel->setProperty("studio_toolbar_group",groups_.size());
        panel->setObjectName("StudioToolbarGroup");
        panel->setAttribute(Qt::WA_StyledBackground);
        panel->setStyleSheet("#StudioToolbarGroup {background:"+background.name()+";}");
    }
    for(auto* action:actions) {
        auto* button=studio_motion::toolButton(group.top);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize({iconSize_,iconSize_});
        button->setAutoRaise(true);
        if(action->menu()){button->setPopupMode(QToolButton::InstantPopup);watchMenuButton(button);}
        group.buttons.append(button);
    }
    groups_.append(group);
}
void StudioBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);layoutButtons();
}
void StudioBar::setIconSize(int size){
    iconSize_=std::clamp(size,12,128);setFixedHeight(iconSize_+38);
    for(auto* button:buttons())button->setIconSize({iconSize_,iconSize_});
    layoutButtons();
}
void StudioBar::setStartupLayoutWidth(int width){
    startupLayoutWidth_=width;
    layoutButtons();
}
void StudioBar::layoutButtons(){
    if(groups_.isEmpty())return;
    constexpr int inset=8, minGap=24;
    const int available=(startupLayoutWidth_>0?startupLayoutWidth_:width())-2*inset;
    const int buttonHeight=iconSize_+28;
    const auto textWidth=[this](const QToolButton* button){
        return std::max({60,iconSize_+20,QFontMetrics(button->font()).horizontalAdvance(button->text())+20});
    };

    // Keep a prefix in its original order across the top, then continue down the right.
    QVector<int> topCounts(groups_.size()),topWidths(groups_.size());
    int used=0,topGroups=0,lastTop=-1;
    for(int g=0;g<groups_.size();++g){
        for(auto* button:groups_[g].buttons){
            const int gap=g>0&&topCounts[g]==0?minGap:0;
            const int w=textWidth(button);
            if(used+gap+w>available)break;
            used+=gap+w;
            topWidths[g]+=w;
            if(topCounts[g]++==0){++topGroups;lastTop=g;}
        }
        if(topCounts[g]<groups_[g].buttons.size())break;
    }

    int sideWidth=0;
    for(int g=0;g<groups_.size();++g)
        for(int b=topCounts[g];b<groups_[g].buttons.size();++b)
            sideWidth=std::max(sideWidth,studio_motion::verticalToolButtonSize(*groups_[g].buttons[b]).width()+4);
    int free=std::max(0,available-used);
    if(lastTop>=0&&topCounts[lastTop]<groups_[lastTop].buttons.size()){
        const int cornerPadding=std::min(free,std::max(0,sideWidth-topWidths[lastTop]));
        topWidths[lastTop]+=cornerPadding;
        free-=cornerPadding;
    }

    int sideY=0,topX=inset;
    QToolButton* movedFocus=nullptr;
    for(int g=0;g<groups_.size();++g){
        auto& group=groups_[g];
        const int topCount=topCounts[g],sideCount=group.buttons.size()-topCount;
        const bool turns=topCount&&sideCount;
        const int spread=topGroups>1&&g<topGroups?int(std::lround(double(g)*free/(topGroups-1))):0;
        const int endPadding=g==lastTop&&topGroups==1?free:0;
        group.top->setGeometry(topX+spread,3,topWidths[g]+endPadding,height()-(turns?3:6));
        group.top->setVisible(topCount>0);
        if(topCount)topX+=topWidths[g]+minGap;

        int buttonX=0,buttonY=2;
        for(int b=0;b<group.buttons.size();++b){
            auto* button=group.buttons[b];
            const bool onTop=b<topCount;
            auto* panel=onTop?group.top:group.side;
            if(button->parentWidget()!=panel){
                if(openMenuButton_==button&&openMenu_)openMenu_->close();
                if(pendingMenuButton_==button){hoverMenuTimer_->stop();pendingMenuButton_.clear();}
                if(button->hasFocus())movedFocus=button;
                button->setParent(panel);
                button->setProperty("studio_vertical_toolbar_label",!onTop);
                button->show();
            }
            if(onTop){
                const int w=textWidth(button);
                button->setGeometry(buttonX,2,w-2,buttonHeight);
                buttonX+=w;
            }else{
                const int h=std::max(buttonHeight,studio_motion::verticalToolButtonSize(*button).height());
                button->setGeometry(2,buttonY,sideWidth-4,h);
                buttonY+=h+4;
            }
        }
        if(sideCount){
            if(sideY)sideY+=8;
            const int groupHeight=buttonY-2;
            group.side->setGeometry(inset,sideY,sideWidth,groupHeight);
            sideY+=groupHeight;
        }
        group.side->setVisible(sideCount>0);
    }
    sideContents_->setMinimumHeight(sideY?sideY+inset:0);
    if(sideY){
        sideScroll_->setFixedWidth(sideWidth+2*inset);
        sideBar_->setFixedWidth(sideScroll_->width());
    }
    if(sideBar_->isHidden()==bool(sideY))sideBar_->setVisible(sideY>0);
    if(movedFocus)movedFocus->setFocus(Qt::OtherFocusReason);
}
