// SPDX-License-Identifier: GPL-2.0-or-later
#include "tutorial_features.h"
#include "ui_language.h"
#include "studio_ui.h"
#include <QApplication>
#include <QBuffer>
#include <QElapsedTimer>
#include <QFile>
#include <QHBoxLayout>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTabBar>
#include <QToolButton>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <QtEndian>
#include <cmath>
#include <algorithm>
#include <functional>
#include <memory>
#include <string_view>

namespace {
struct Chapter {
    const char* name;
    const char* clip;
    const char* icon;
    const char* introduction;
    QList<const char*> steps;
};
constexpr int framesPerSecond=30;
constexpr int framesPerStep=3*framesPerSecond;
const QList<Chapter> chapters{
    {"自动识别折痕","creases","CreaseEdge","导入 OBJ 时，勾选「自动识别折痕」，就能自动识别折痕边。",
     {"导入船体网格","看两个面交界的折角","辨认会保留的尖角"}},
    {"拟合船体曲面","rebuild","Wireframe","从 OBJ 网格建立控制网格，调整控制点，让细分后的船面贴合原网格。",
     {"在原网格上取样","建立初始控制网","细分并拟合船面"}},
    {"比较船模","compare","LinesPlan","用「比较船模」查看微调船型后，吃水和排水量变了多少。",
     {"复制船模","只修改副本","固定吃水","固定装载","比较吃水变化"}},
    {"导出横剖面图","templates","ExportFile","在「剖面线」窗口可导出船体横剖线、造模样板或插接模架。",
     {"船体横剖线","造模样板","插接模架","按站号插合","沿横板铺外板","A4 拼页",
      "无需裁剪","剪下第 2 页的裁切边","对齐十字并贴合"}},
    {"横剖线对照","reference","Sections","导入参考曲线，与当前船模的横剖线对照。",
     {"导入参考线","从船模剖出横剖线","查看最大偏差"}},
    {"3D 打印船壳（敞口）","print-shell","STL","导出 STL 时，勾选「3D 打印船壳（敞口）」即可单独导出船壳。",
     {"从整船选出船壳","设定成品尺寸","生成 STL","在切片软件中检查"}}
};

constexpr auto artworkSize=QSize(640,272);
constexpr int actionMilliseconds=2200;
constexpr int maxFramesPerBlock=24;

// Each block starts with a complete image, so dragging can seek directly into it.
class TutorialClip final {
public:
    bool open(const char* name,int expectedFrames) {
        reader_.reset();buffer_.close();images_.clear();block_=-1;offsets_.clear();
        QFile file(QStringLiteral(":/tutorial-features/%1.clip").arg(QString::fromLatin1(name)));
        if(!file.open(QIODevice::ReadOnly))return false;
        bytes_=file.readAll();
        if(bytes_.size()<20||!bytes_.startsWith("FST1")||readWord(4)!=quint32(expectedFrames))return false;
        const auto blockSize=readWord(8);
        if(blockSize==0||blockSize>maxFramesPerBlock)return false;
        blockSize_=int(blockSize);
        frameCount_=expectedFrames;
        const int blocks=(frameCount_+blockSize_-1)/blockSize_;
        if(12+4*(blocks+1)>bytes_.size())return false;
        for(int i=0;i<=blocks;++i){
            const auto offset=readWord(12+4*i);
            if(offset>quint64(bytes_.size())||
               (i==0?offset!=quint32(12+4*(blocks+1)):offset<=offsets_.last())){
                offsets_.clear();return false;
            }
            offsets_.append(offset);
        }
        return true;
    }
    QImage frame(int index) {
        if(offsets_.isEmpty()||index<0||index>=frameCount_)return {};
        const int block=index/blockSize_;
        const int inBlock=index%blockSize_;
        if(block!=block_){
            reader_.reset();images_.clear();buffer_.close();
            const auto from=offsets_[block],size=offsets_[block+1]-from;
            buffer_.setData(QByteArray::fromRawData(bytes_.constData()+from,size));
            if(!buffer_.open(QIODevice::ReadOnly))return {};
            reader_=std::make_unique<QImageReader>(&buffer_,QByteArrayLiteral("gif"));
            block_=block;
        }
        while(images_.size()<=inBlock){
            auto image=reader_->read();
            if(image.isNull())return {};
            images_.append(std::move(image));
        }
        return images_.at(inBlock);
    }
private:
    quint32 readWord(int offset) const { return qFromLittleEndian<quint32>(bytes_.constData()+offset); }
    QByteArray bytes_;
    QBuffer buffer_;
    std::unique_ptr<QImageReader> reader_;
    QList<QImage> images_;
    QList<quint32> offsets_;
    int blockSize_=0;
    int block_=-1;
    int frameCount_=0;
};

enum class PlaybackIcon { Play, Pause, Replay };
QIcon playbackIcon(PlaybackIcon kind,qreal ratio) {
    QPixmap bitmap(QSize(24,24)*ratio);
    bitmap.setDevicePixelRatio(ratio);
    bitmap.fill(Qt::transparent);
    QPainter p(&bitmap);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor ink("#26746a");
    if(kind==PlaybackIcon::Play){
        p.setPen(Qt::NoPen);p.setBrush(ink);
        p.drawPolygon(QPolygonF(QList<QPointF>{QPointF(8,4.5),QPointF(20,12),QPointF(8,19.5)}));
    }else if(kind==PlaybackIcon::Pause){
        p.setPen(Qt::NoPen);p.setBrush(ink);
        p.drawRoundedRect(QRectF(7,5,3.5,14),.8,.8);
        p.drawRoundedRect(QRectF(14,5,3.5,14),.8,.8);
    }else{
        p.setPen(QPen(ink,1.9,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        const QRectF circle(4.5,4.5,15,15);
        QPainterPath replay;
        replay.arcMoveTo(circle,-47);
        replay.arcTo(circle,-47,-268);
        // Extend the 45-degree tangent so the arrow wings clear the arc.
        const auto tip=replay.currentPosition()+QPointF(1.15,1.15);
        replay.lineTo(tip);
        replay.moveTo(tip-QPointF(4.3,0));
        replay.lineTo(tip);
        replay.lineTo(tip-QPointF(0,4.3));
        p.drawPath(replay);
    }
    return QIcon(bitmap);
}

qreal arrangeCaption(QTextLayout& layout,qreal width) {
    QTextOption options(Qt::AlignHCenter);
    options.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(options);
    layout.beginLayout();
    qreal height=0;
    for(auto line=layout.createLine();line.isValid();line=layout.createLine()){
        line.setLineWidth(width);
        line.setPosition(QPointF(0,height));
        height+=line.height();
    }
    layout.endLayout();
    return height;
}

void drawCaption(QPainter& painter,const QRectF& area,const QString& text,QFont font) {
    QTextLayout layout(text,font);
    auto height=arrangeCaption(layout,area.width());
    while(height>area.height()&&font.pixelSize()>1){
        font.setPixelSize(font.pixelSize()-1);
        layout.setFont(font);
        height=arrangeCaption(layout,area.width());
    }
    layout.draw(&painter,QPointF(area.left(),area.center().y()-height/2));
}

struct LegendEntry {
    QString text;
    QColor ink;
    QPen sample;
};

void drawLegend(QPainter& painter,const QRectF& area,const QList<LegendEntry>& entries) {
    constexpr qreal sampleWidth=26, textGap=9, entryGap=24;
    const auto font=painter.font();
    const QFontMetricsF metrics(font);
    const qreal maxWidth=(area.width()-entryGap*(entries.size()-1))/entries.size();
    QList<qreal> widths;
    qreal totalWidth=entryGap*(entries.size()-1);
    for(const auto& entry:entries){
        const auto width=std::min(maxWidth, sampleWidth+textGap+std::ceil(metrics.horizontalAdvance(entry.text))+2);
        widths.append(width);
        totalWidth+=width;
    }
    qreal x=area.center().x()-totalWidth/2;
    for(qsizetype i=0;i<entries.size();++i){
        const auto& entry=entries.at(i);
        painter.setPen(entry.sample);
        const auto y=area.center().y();
        painter.drawLine(QPointF(x,y),QPointF(x+sampleWidth,y));
        painter.setPen(entry.ink);
        drawCaption(painter,QRectF(x+sampleWidth+textGap,area.top(),widths[i]-sampleWidth-textGap,area.height()),
                    entry.text,font);
        x+=widths[i]+entryGap;
    }
}

class TutorialFrame final : public QWidget {
public:
    TutorialFrame() {
        setObjectName("TutorialFeatureFrame");
        setMinimumHeight(216);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    }
    void showFrame(QPixmap frame,int index) {
        frame_=std::move(frame);index_=index;setProperty("frameNumber",index);update();
    }
    void setClip(const char* clip) { clip_=QString::fromLatin1(clip); }
    void setIntroduction(const QString& text) {
        introduction_=text;
        setAccessibleDescription(text);
        setProperty("introduction",text);
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(),QColor(247,250,249));
        if(frame_.isNull())return;
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const qreal textWidth=std::max(1.,width()-24.);
        QTextLayout layout(introduction_,font());
        const qreal textHeight=arrangeCaption(layout,textWidth);
        const int lane=std::max(2*fontMetrics().lineSpacing(),qCeil(textHeight))+14;
        QRect pictureArea=rect().adjusted(4,lane,-4,-4);
        if(clip_=="reference"){
            // Reserve space across every step so revealing the legend does not move the artwork.
            const auto fit=artworkSize.scaled(pictureArea.size(),Qt::KeepAspectRatio);
            pictureArea.adjust(0,qCeil(36.*fit.width()/artworkSize.width()),0,0);
        }
        const auto size=frame_.size().scaled(pictureArea.size(),Qt::KeepAspectRatio);
        QRect viewport(QPoint{},size);
        viewport.moveCenter(pictureArea.center());
        painter.drawPixmap(viewport,frame_);
        setProperty("pictureRect",viewport);
        drawArtworkLabels(painter,viewport);
        const QRectF textArea(12,(lane-textHeight)/2,textWidth,textHeight);
        setProperty("introductionRect",textArea.toAlignedRect());
        painter.setPen(QColor("#24433e"));
        layout.draw(&painter,textArea.topLeft());
    }
private:
    void drawArtworkLabels(QPainter& painter,const QRect& viewport) const {
        const int step=index_/framesPerStep;
        const auto ease=[](double t){
            t=std::clamp(t,0.,1.);return t*t*(3.-2.*t);
        };
        const double t=ease((double(index_%framesPerStep)/framesPerSecond-.16)/(clip_=="compare"&&step==0?2.13:2.45));
        painter.save();
        painter.translate(viewport.topLeft());
        painter.scale(double(viewport.width())/artworkSize.width(),double(viewport.height())/artworkSize.height());
        auto face=font();face.setPixelSize(12);face.setWeight(QFont::Medium);
        painter.setFont(face);painter.setPen(QColor("#274e49"));
        const auto label=[&](qreal x,qreal y,qreal width,qreal height,const QString& text){
            drawCaption(painter,QRectF(x-width/2,y-height/2,width,height),text,face);
        };
        if(clip_=="templates"){
            drawTemplateLabels(painter,step);
        }
        if(clip_=="compare"&&step>=2){
            painter.setOpacity(step==3?1-ease(t/.25):ease((t-.38)/.43));
            label(320,123,450,24,uiText(step==4?"固定装载":"固定吃水"));
            // Centers match the numeric columns in the artwork.
            const QRectF cells[]={{101,138,106,27},{220,138,114,27},{341,138,114,27},{481,138,96,27}};
            const QString headings[]={QString(uiText("吃水"))+" (m)",QString(uiText("排水质量"))+" (t)",
                QString(uiText("水线长"))+" (m)",QString(uiText("湿表面积"))+" (m²)"};
            auto headerFont=face;
            // Use one size for the whole row, allowing the longer headings to wrap.
            for(int i=0;i<4;++i){
                QTextLayout heading(headings[i],headerFont);
                while(arrangeCaption(heading,cells[i].width())>cells[i].height()&&headerFont.pixelSize()>1){
                    headerFont.setPixelSize(headerFont.pixelSize()-1);
                    heading.setFont(headerFont);
                }
            }
            for(int i=0;i<4;++i)drawCaption(painter,cells[i],headings[i],headerFont);
        }
        if(clip_=="creases"&&step==2){
            painter.setOpacity(ease((t-.28)/.55));
            drawLegend(painter,QRectF(24,221,592,30),
                {{uiText("已设为折痕"),QColor("#734a24"),QPen(QColor("#efad54"),2.2)}});
        }
        if(clip_=="rebuild"){
            const char* const stages[]={"原始 OBJ 网格","建立初始控制网","细分并拟合船面"};
            painter.setPen(QColor("#08796f"));
            label(216,232,280,34,uiText(stages[step]));
            painter.setOpacity(step==0?ease(t/.45):1.);
            painter.setPen(QColor("#315e7b"));
            label(527,232,180,34,uiText(step==2?"重建前后剖面对照":"剖面取样示意"));
        }
        if(clip_=="reference"){
            const auto section=step==1?ease((t-.12)/.53):0.;
            const auto overlay=step==1?ease((t-.61)/.39):1.;
            if(step<2){
                painter.setOpacity(step==0?1.:1-ease(section/.62));
                painter.setPen(QColor("#276f9e"));
                label(138,206,150,22,uiText("参考曲线"));
            }
            if(step>0){
                painter.setOpacity(overlay);
                const QColor reference("#276f9e"),current("#08796f");
                drawLegend(painter,QRectF(24,-36,592,32),
                    {{uiText("参考曲线"),reference,QPen(reference,1.8,Qt::DashLine)},
                     {uiText("当前船模"),current,QPen(current,1.8)}});
            }
        }
        painter.restore();
    }
    void drawTemplateLabels(QPainter& painter,int step) const {
        const double elapsed=double(index_%framesPerStep)/framesPerSecond;
        const double blend=step==0||step==1||step==2||step==4||step==5||step==6
            ?std::clamp((elapsed-2.40)/.55,0.,1.):0.;
        const auto annotate=[&](int part,double opacity){
            if(opacity<=0)return;
            painter.setOpacity(opacity);
            const auto caption=[&](int x,int y,int width,const QString& text){
                drawCaption(painter,QRectF(x-width/2,y-17,width,34),text,painter.font());
            };
            switch(part){
                case 0: caption(516,25,230,uiText("横剖线","tutorial.section"));break;
                case 1: caption(320,26,500,uiText("造模样板"));break;
                case 2: caption(320,25,500,uiText("插接模架"));break;
                case 5:
                    caption(320,26,610,uiText("A4 拼页有「无需裁剪」和「需裁剪」两种方式。"));
                    caption(166,211,240,uiText("无需裁剪"));
                    caption(476,211,240,uiText("需裁剪"));
                    break;
                case 6:
                    caption(320,25,610,uiText("「无需裁剪」更方便，但要求打印机同时支持 A4 无边距和 100% 原尺寸打印。"));
                    break;
                case 7:
                case 8:
                    caption(320,25,610,uiText("「需裁剪」多一步裁纸，对打印机的要求较低。"));
                    break;
            }
        };
        annotate(step,std::max(0.,1.-2.*blend));
        annotate(step+1,std::max(0.,2.*blend-1.));
    }
    QString clip_;
    int index_=0;
    QPixmap frame_;
    QString introduction_;
};

class StepNode final : public QToolButton {
public:
    explicit StepNode(const char* source,QWidget* parent):QToolButton(parent) {
        setObjectName("TutorialFeatureNode");
        const auto label=QString::fromUtf8(source);
        bindUiText(this,"text",label);
        bindUiText(this,"accessibleName",label);
        bindUiText(this,"toolTip",label);
        setFixedSize(28,28);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setCheckable(true);
    }
    void setReached(bool reached) { reached_=reached;update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QPointF center(width()/2.,height()/2.);
        const QColor accent("#007d71");
        if(underMouse()||(hasFocus()&&testAttribute(Qt::WA_KeyboardFocusChange))){
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#e1f0e8"));
            painter.drawEllipse(center,11,11);
        }
        painter.setPen(QPen(isChecked()||reached_?accent:QColor("#64877e"),1.55));
        painter.setBrush(reached_?accent:QColor("#f7faf9"));
        painter.drawEllipse(center,isChecked()?6.5:4.5,isChecked()?6.5:4.5);
        if(isChecked()){
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::white);
            painter.drawEllipse(center,2.4,2.4);
        }
    }
private:
    bool reached_=false;
};

enum class ScrubEvent { Begin, Move, End };

class StepTimeline final : public QWidget {
public:
    explicit StepTimeline(QWidget* parent):QWidget(parent) {
        setObjectName("TutorialFeatureSteps");
        bindUiText(this,"accessibleName",QStringLiteral("步骤"));
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::SizeHorCursor);
        nodesLayout_=new QHBoxLayout(this);
        nodesLayout_->setContentsMargins(4,0,4,0);
        nodesLayout_->setSpacing(0);
    }
    std::function<void(double,ScrubEvent)> seek;
    void setSteps(const QList<const char*>& steps,const std::function<void(int)>& jump) {
        while(auto* item=nodesLayout_->takeAt(0)){
            delete item->widget();
            delete item;
        }
        nodes_.clear();
        for(int i=0;i<steps.size();++i){
            if(i)nodesLayout_->addStretch(1);
            auto* button=new StepNode(steps.at(i),this);
            nodes_.append(button);
            button->installEventFilter(this);
            nodesLayout_->addWidget(button);
            connect(button,&QToolButton::clicked,this,[jump,i]{jump(i);});
        }
        nodesLayout_->addStretch(1);
        nodesLayout_->addSpacing(14);
        setStep(0);
    }
    void setStep(int step) {
        step_=step;
        for(int i=0;i<nodes_.size();++i){
            nodes_[i]->setChecked(i==step);
            nodes_[i]->setReached(i<=step);
        }
        setProgress(0.);
    }
    void setProgress(double progress) {
        progress_=progress;
        setProperty("stepProgress",progress);
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        if(nodes_.isEmpty())return;
        const QPointF start=center(0),end=center(int(nodes_.size()));
        const auto current=center(step_);
        const auto next=center(step_+1);
        const auto handle=current+(next-current)*progress_;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#cadbd4"),2.,Qt::SolidLine,Qt::RoundCap));
        painter.drawLine(start,end);
        painter.setPen(QPen(QColor("#007d71"),2.,Qt::SolidLine,Qt::RoundCap));
        painter.drawLine(start,handle);
        if(progress_>.015){
            painter.setBrush(QColor("#007d71"));
            painter.setPen(QPen(QColor("#f7faf9"),1.4));
            painter.drawEllipse(handle,5.5,5.5);
        }
    }
    void mousePressEvent(QMouseEvent* event) override {
        if(event->button()!=Qt::LeftButton){QWidget::mousePressEvent(event);return;}
        press(event->position(),-1);event->accept();
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if(!pointerDown_){QWidget::mouseMoveEvent(event);return;}
        move(event->position());event->accept();
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if(!pointerDown_||event->button()!=Qt::LeftButton){QWidget::mouseReleaseEvent(event);return;}
        release(event->position());event->accept();
    }
    bool eventFilter(QObject* watched,QEvent* event) override {
        const int node=nodes_.indexOf(static_cast<StepNode*>(watched));
        if(node>=0&&event->type()==QEvent::MouseButtonPress){
            auto* mouse=static_cast<QMouseEvent*>(event);
            if(mouse->button()==Qt::LeftButton){
                press(nodes_[node]->mapTo(this,mouse->position()),node);
                return true;
            }
        }
        return QWidget::eventFilter(watched,event);
    }
    void keyPressEvent(QKeyEvent* event) override {
        const double from=step_+progress_;
        double target=from;
        switch(event->key()){
            case Qt::Key_Left:target-=.10;break;
            case Qt::Key_Right:target+=.10;break;
            case Qt::Key_Home:target=0;break;
            case Qt::Key_End:target=double(nodes_.size());break;
            default:QWidget::keyPressEvent(event);return;
        }
        if(seek){
            seek(from,ScrubEvent::Begin);
            seek(std::clamp(target,0.,double(nodes_.size())),ScrubEvent::End);
        }
        event->accept();
    }
private:
    QPointF center(int node) const {
        const auto* button=nodes_.at(std::min(node,int(nodes_.size())-1));
        const auto point=QPointF(button->pos())+QPointF(button->width()/2.,button->height()/2.);
        return node==nodes_.size()?QPointF(width()-4.,point.y()):point;
    }
    double position(double x) const {
        if(nodes_.isEmpty())return 0.;
        for(int i=0;i<nodes_.size();++i){
            const double a=center(i).x(),b=center(i+1).x();
            if(x<=b)return i+std::clamp((x-a)/std::max(1.,b-a),0.,1.);
        }
        return double(nodes_.size());
    }
    void press(QPointF point,int node) {
        pointerDown_=true;
        pressedNode_=node;
        pressPoint_=point;
        dragging_=node<0;
        grabMouse();
        if(dragging_&&seek)seek(position(point.x()),ScrubEvent::Begin);
    }
    void move(QPointF point) {
        if(!dragging_&&std::abs(point.x()-pressPoint_.x())<QApplication::startDragDistance())return;
        const auto state=dragging_?ScrubEvent::Move:ScrubEvent::Begin;
        dragging_=true;
        if(seek)seek(position(point.x()),state);
    }
    void release(QPointF point) {
        pointerDown_=false;
        if(mouseGrabber()==this)releaseMouse();
        if(dragging_){
            if(seek)seek(position(point.x()),ScrubEvent::End);
        }else if(pressedNode_>=0)nodes_.at(pressedNode_)->click();
        dragging_=false;
    }
    QHBoxLayout* nodesLayout_;
    QList<StepNode*> nodes_;
    int step_=0;
    double progress_=0.;
    QPointF pressPoint_;
    int pressedNode_=-1;
    bool pointerDown_=false;
    bool dragging_=false;
};

class FeatureTutorial final : public QWidget {
public:
    explicit FeatureTutorial(QWidget* parent):QWidget(parent) {
        setObjectName("TutorialFeatures");
        auto* layout=new QVBoxLayout(this);
        layout->setContentsMargins(0,4,0,0);
        layout->setSpacing(7);
        subjects_=new QTabBar;
        subjects_->setObjectName("TutorialFeatureSubjects");
        bindUiText(subjects_,"accessibleName",QStringLiteral("新功能"));
        subjects_->setExpanding(false);
        subjects_->setUsesScrollButtons(true);
        subjects_->setElideMode(Qt::ElideNone);
        subjects_->setIconSize(QSize(19,19));
        subjects_->setStyleSheet("QTabBar#TutorialFeatureSubjects::tab {padding:7px 8px;margin:0 2px 2px 0;}");
        for(const auto& chapter:chapters){
            const auto icon=chapter.icon==std::string_view("STL")?studioFormatIcon("STL"):studioIcon(chapter.icon);
            subjects_->addTab(icon,uiText(chapter.name));
        }
        layout->addWidget(subjects_);
        frame_=new TutorialFrame;
        layout->addWidget(frame_,1);
        auto* controls=new QHBoxLayout;
        controls->setSpacing(8);
        const auto button=[this](const char* name,const QString& label,PlaybackIcon kind){
            auto* result=new QToolButton(this);
            result->setObjectName(name);
            result->setAccessibleName(label);
            result->setToolTip(label);
            result->setIcon(playbackIcon(kind,devicePixelRatioF()));
            result->setIconSize(QSize(22,22));
            result->setFixedSize(34,32);
            result->setAutoRaise(true);
            return result;
        };
        play_=button("TutorialFeaturePlay",uiText("播放"),PlaybackIcon::Play);
        auto* replay=button("TutorialFeatureReplay",uiText("重播演示"),PlaybackIcon::Replay);
        bindUiText(replay,"toolTip",QStringLiteral("重播演示"));
        bindUiText(replay,"accessibleName",QStringLiteral("重播演示"));
        controls->addWidget(play_);
        controls->addWidget(replay);
        steps_=new StepTimeline(this);
        steps_->seek=[this](double position,ScrubEvent event){scrub(position,event);};
        controls->addWidget(steps_,1);
        layout->addLayout(controls);
        connect(subjects_,&QTabBar::currentChanged,this,[this]{loadChapter();});
        timer_.setInterval(16);
        timer_.setTimerType(Qt::PreciseTimer);
        connect(&timer_,&QTimer::timeout,this,[this]{advance();});
        connect(play_,&QToolButton::clicked,this,[this]{
            if(sequencePlaying_){pause();return;}
            if(currentFrame_+1==chapter().steps.size()*framesPerStep && phaseElapsed_>=actionMilliseconds)
                showStep(0);
            play();
        });
        connect(replay,&QToolButton::clicked,this,[this]{
            showStep(0);
            play();
        });
        loadChapter();
        refreshLanguage();
        bindUiRefresh(this,[this]{refreshLanguage();});
    }
protected:
    void hideEvent(QHideEvent* event) override {
        pause();
        QWidget::hideEvent(event);
    }
private:
    const Chapter& chapter() const { return chapters.at(subjects_->currentIndex()); }
    void refreshChapterText(){
        frame_->setAccessibleName(uiText(chapter().name));
        frame_->setIntroduction(uiText(chapter().introduction));
        frame_->update();
    }
    void refreshLanguage(){
        for(int i=0;i<chapters.size();++i){
            const auto title=uiText(chapters[i].name);
            subjects_->setTabText(i,title);
            subjects_->setTabToolTip(i,title);
            subjects_->setAccessibleTabName(i,title);
        }
        refreshChapterText();
        updatePlaybackButton();
    }
    void startClock(){
        if(timer_.isActive())return;
        phaseClock_.restart();
        timer_.start();
    }
    void stopClock(){
        if(timer_.isActive())phaseElapsed_+=phaseClock_.elapsed();
        timer_.stop();
    }
    void play(){
        if(currentFrame_<0)return;
        sequencePlaying_=true;
        startClock();
        updatePlaybackButton();
    }
    void pause(){
        sequencePlaying_=false;
        stopClock();
        updatePlaybackButton();
    }
    void updatePlaybackButton() {
        const QString label=sequencePlaying_?uiText("暂停"):uiText("播放");
        play_->setToolTip(label);
        play_->setAccessibleName(label);
        play_->setIcon(playbackIcon(sequencePlaying_?PlaybackIcon::Pause:PlaybackIcon::Play,devicePixelRatioF()));
    }
    static int actionFrame(qint64 elapsed) {
        return int(std::min<qint64>(framesPerStep-1,elapsed*framesPerStep/actionMilliseconds));
    }
    void advance(){
        const qint64 elapsed=phaseElapsed_+phaseClock_.elapsed();
        if(!seek(step_*framesPerStep+actionFrame(elapsed))){pause();return;}
        steps_->setProgress(std::min(1.,double(elapsed)/actionMilliseconds));
        if(elapsed<actionMilliseconds)return;
        if(step_+1==chapter().steps.size())pause();
        else showStep(step_+1);
    }
    bool seek(int frameNumber){
        if(frameNumber==currentFrame_)return true;
        auto image=clip_.frame(frameNumber);
        if(image.isNull())return false;
        frame_->showFrame(QPixmap::fromImage(std::move(image)),frameNumber);
        currentFrame_=frameNumber;
        return true;
    }
    void loadChapter(){
        const auto& current=chapter();
        pause();
        steps_->setSteps(current.steps,[this](int step){showStep(step);});
        frame_->setClip(current.clip);
        refreshChapterText();
        currentFrame_=-1;
        if(!clip_.open(current.clip,int(current.steps.size())*framesPerStep))
            qWarning("Cannot open tutorial clip: %s",current.clip);
        showStep(0);
    }
    void scrub(double position,ScrubEvent event) {
        if(event==ScrubEvent::Begin)stopClock();
        const int step=std::min(int(chapter().steps.size())-1,int(position));
        showStep(step,position-step);
        if(event==ScrubEvent::End&&sequencePlaying_)startClock();
    }
    void showStep(int step,double progress=0.){
        step_=step;
        if(timer_.isActive())phaseClock_.restart();
        steps_->setStep(step);
        steps_->setProgress(progress);
        frame_->setProperty("tutorialStep",step);
        phaseElapsed_=qint64(progress*actionMilliseconds);
        if(!seek(step*framesPerStep+actionFrame(phaseElapsed_)))pause();
    }
    QTabBar* subjects_;
    StepTimeline* steps_;
    TutorialFrame* frame_;
    QToolButton* play_;
    TutorialClip clip_;
    QTimer timer_;
    QElapsedTimer phaseClock_;
    int currentFrame_=-1;
    int step_=0;
    qint64 phaseElapsed_=0;
    bool sequencePlaying_=false;
};
}

QWidget* createFeatureTutorial(QWidget* parent) {
    return new FeatureTutorial(parent);
}
