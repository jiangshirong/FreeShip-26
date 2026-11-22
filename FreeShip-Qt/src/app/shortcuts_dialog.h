#pragma once
#include "studio_ui.h"
#include "ui_language.h"
#include "studio_read_ui.h"
#include <QAction>
#include <QDialog>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QPainterPath>
#include <QShortcut>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <array>
#include <functional>

enum class ShortcutGesture { None, DoubleClick, Pan, Rotate, Box, Wheel };
enum class ShortcutViewScope { None, All, Current, CurrentOrLines, Perspective };

inline QWidget* shortcutViewScopeIcons(ShortcutViewScope scope,const QString& description,QWidget* owner){
    auto* cell=new QWidget;
    cell->setObjectName("ShortcutViewScope");
    cell->setAccessibleName(description);
    cell->setToolTip(description);
    auto* row=new QHBoxLayout(cell);
    row->setContentsMargins(2,0,8,0);
    row->setSpacing(6);
    auto* views=new QWidget;
    auto* viewRow=new QHBoxLayout(views);
    viewRow->setContentsMargins(0,0,0,0);
    viewRow->setSpacing(6);
    const auto add=[&](QHBoxLayout* layout,const QString& icon,const QString& caption){
        auto* label=new QLabel;
        label->setObjectName("ShortcutViewIcon");
        label->setProperty("shortcut_view_icon",icon);
        label->setAccessibleName(caption);
        label->setToolTip(description+" · "+caption);
        label->setPixmap(studioIcon(icon).pixmap(QSize(20,20),owner->devicePixelRatioF()));
        layout->addWidget(label);
    };
    if(scope!=ShortcutViewScope::Perspective){
        add(viewRow,"ViewBody",uiText("横剖面"));
        add(viewRow,"ViewProfile",uiText("纵剖面"));
        add(viewRow,"ViewPlan",uiText("俯视图"));
    }
    add(viewRow,"ViewPerspective",uiText("透视图"));
    if(scope==ShortcutViewScope::All){
        views->setObjectName("ShortcutAllViews");
        views->setStyleSheet("QWidget#ShortcutAllViews{background:#e7f0ea;border-radius:4px}");
    }
    row->addWidget(views);
    if(scope==ShortcutViewScope::CurrentOrLines){
        auto* separator=new QFrame;
        separator->setFixedSize(1,17);
        separator->setStyleSheet("background:#cbded3");
        row->addWidget(separator);
        add(row,"LinesPlan",uiText("型线图"));
    }
    row->addStretch();
    return cell;
}

class ShortcutGesturePreview final : public QWidget {
public:
    ShortcutGesturePreview(ShortcutGesture gesture,const QString& label)
        : gesture_(gesture){
        setAccessibleName(label);
        setObjectName("ShortcutGesturePreview");
        setFixedSize(sizeHint());
    }
    QSize sizeHint()const override{return {gesture_==ShortcutGesture::Wheel?62:84,43};}
    void setElapsed(qint64 ms){
        phase_=(ms%1800)/1800.;
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QColor ink("#315b55"), accent("#007e74"), fill("#cae8da");
        if(gesture_==ShortcutGesture::Wheel){paintWheel(painter,ink,accent,fill);return;}
        const bool dragging=gesture_==ShortcutGesture::Pan||gesture_==ShortcutGesture::Rotate||gesture_==ShortcutGesture::Box;
        const bool right=gesture_==ShortcutGesture::Pan;
        const qreal progress=qBound(0.,(phase_-.15)/.58,1.);
        const bool active=dragging?(phase_>=.15&&phase_<.74):
            (phase_>.2&&phase_<.30)||(gesture_==ShortcutGesture::DoubleClick&&phase_>.39&&phase_<.49);
        const qreal x=dragging?4+51*progress:4;
        const qreal y=gesture_==ShortcutGesture::Rotate?8+6*std::sin(progress*3.14159):8;
        if(dragging){
            painter.setPen(QPen(QColor("#9dbdb1"),1,Qt::DashLine));
            if(gesture_==ShortcutGesture::Box){
                painter.setBrush(QColor(0,126,116,28));painter.drawRect(QRectF(12,14,51*progress,24*progress));
            }else painter.drawLine(QPointF(12,37),QPointF(66,37));
        }
        painter.setPen(QPen(ink,1.5));
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(QRectF(x,y,18,26),8,8);
        if(active){
            QPainterPath button;
            const qreal edge=right?x+18:x, middle=x+9;
            button.moveTo(middle,y+1);
            button.quadTo(edge,y+1,edge,y+9);
            button.lineTo(middle,y+9);
            painter.fillPath(button,fill);
        }
        painter.drawLine(QPointF(x+1,y+10),QPointF(x+17,y+10));
        painter.drawLine(QPointF(x+9,y+1),QPointF(x+9,y+10));
        if(active&&!dragging){
            painter.setPen(QPen(accent,1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(QPointF(x+4,y+4),8,8);
        }
    }
private:
    void paintWheel(QPainter& p,const QColor& ink,const QColor& accent,const QColor& fill)const{
        p.setPen(QPen(ink,1.6));p.setBrush(Qt::white);p.drawRoundedRect(QRectF(3,3,26,36),11,11);
        p.drawLine(QPointF(4,17),QPointF(28,17));
        p.setPen(QPen(accent,1.2));p.setBrush(fill);
        const QRectF wheel(12.5,6,7,13);
        p.drawRoundedRect(wheel,3,3);
        const int direction=phase_<.5?-1:1;
        const qreal cycle=std::fmod(phase_*2,1.);
        const qreal travel=qBound(0.,(cycle-.08)/.76,1.);
        p.save();QPainterPath clip;clip.addRoundedRect(wheel.adjusted(1,1,-1,-1),2,2);p.setClipPath(clip);
        p.setPen(QPen(accent,1));
        const qreal offset=direction*travel*15;
        for(int i=-4;i<=8;++i)p.drawLine(QPointF(13,7+3*i+offset),QPointF(19,7+3*i+offset));
        p.restore();
        const qreal tip=21+direction*(5+5*travel);
        p.setPen(QPen(accent,2,Qt::SolidLine,Qt::RoundCap));
        p.drawLine(QPointF(46,tip-direction*14),QPointF(46,tip));
        p.drawLine(QPointF(41,tip-direction*5),QPointF(46,tip));
        p.drawLine(QPointF(51,tip-direction*5),QPointF(46,tip));
    }
    ShortcutGesture gesture_;
    qreal phase_=0;
};

inline bool showShortcutGuide(QWidget* owner,QShortcut* languageShortcut=nullptr,
                              const std::function<void()>& switchLanguage={}){
    QDialog dialog(owner);dialog.setAttribute(Qt::WA_DontShowOnScreen,owner&&owner->testAttribute(Qt::WA_DontShowOnScreen));setStudioWindowIcon(&dialog,"Shortcuts");dialog.setObjectName("ShortcutGuide");dialog.setWindowTitle(uiText("快捷方式"));
    dialog.resize(980,740);dialog.setMinimumSize(620,440);styleStudioReadDialog(&dialog);
    auto* layout=new QVBoxLayout(&dialog);layout->setContentsMargins(24,24,24,24);layout->setSpacing(20);
    layout->addWidget(studioReadHeading(uiText("快捷方式"),"Shortcuts",&dialog));
    auto* table=new StudioShortcutTable(0,3);table->setObjectName("ShortcutGuideTable");
    table->setItemDelegate(new StudioWidgetCellDelegate(table));
    table->horizontalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setAlternatingRowColors(false);table->setShowGrid(false);table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Fixed);table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);table->setItemDelegateForColumn(1,new StudioKeycapDelegate(table));
    table->setWordWrap(true);table->setTextElideMode(Qt::ElideNone);applyStudioScrollbars(table);layout->addWidget(table);
    enum Group { Model, View, Window, Groups };
    struct Entry { QString description,key,scope; ShortcutGesture gesture; ShortcutViewScope viewScope; bool ctrl; };
    std::array<QList<Entry>,Groups> entries;
    const auto add=[&](Group group,const QString& description,const QString& key,const QString& scope,
                       ShortcutGesture gesture=ShortcutGesture::None,ShortcutViewScope viewScope=ShortcutViewScope::None,bool ctrl=false){
        if(key.isEmpty()||description.isEmpty())return;
        for(auto& entry:entries[group]){
            if(entry.description!=description||entry.scope!=scope||entry.gesture!=gesture||entry.viewScope!=viewScope||entry.ctrl!=ctrl)continue;
            if(!entry.key.split(" / ").contains(key))entry.key+=" / "+key;
            return;
        }
        entries[group].append({description,key,scope,gesture,viewScope,ctrl});
    };
    const auto target=[](const QObject* command){
        const auto source=command->property("shortcut_target").toString();
        return source.isEmpty()?QString(uiText("当前工作区")):uiText(source);
    };
    for(const auto* action:owner->findChildren<QAction*>()){
        if(action->property("coordinate_axis").isValid())continue;
        const auto fileOperation=action->property("file_operation").toString();
        if(fileOperation=="save"||action->property("operation")=="delete")continue;
        const auto caption=fileOperation=="open"?QString(uiText("选择模型打开")):studioMenuCaption(action->text().remove('&'));
        for(const auto& key:action->shortcuts())add(Model,caption,key.toString(QKeySequence::NativeText),target(action));
    }
    for(const auto* shortcut:owner->findChildren<QShortcut*>()){
        if(shortcut==languageShortcut||shortcut->property("shortcut_source")=="切换语言 / Language")continue;
        auto* action=qobject_cast<QAction*>(shortcut->property("shortcut_action").value<QObject*>());
        const auto source=shortcut->property("shortcut_source").toString();
        const int axis=shortcut->property("coordinate_shortcut_axis").toInt();
        auto description=action?action->text():QString(uiText(source));
        const auto scope=target(shortcut);
        auto group=Model;
        auto viewScope=ShortcutViewScope::None;
        if(axis>=1&&axis<=3)description=QString(uiText("显示或隐藏 %1 坐标")).arg(QString("XYZ").at(axis-1));
        if(axis){group=View;viewScope=ShortcutViewScope::Current;}
        if(source=="缩放至全图"){group=View;viewScope=ShortcutViewScope::All;}
        if(source=="退出程序")group=Window;
        for(const auto& key:shortcut->keys())add(group,description,key.toString(QKeySequence::NativeText),scope,ShortcutGesture::None,viewScope);
    }
    add(View,uiText("平移视图"),uiText("右键拖动"),uiText("当前视图或型线图窗口"),ShortcutGesture::Pan,ShortcutViewScope::CurrentOrLines);
    add(View,uiText("框选控制点"),uiText("空白处左键拖动"),uiText("当前视图"),ShortcutGesture::Box,ShortcutViewScope::Current);
    add(View,uiText("旋转视角"),uiText("Ctrl + 左键拖动"),uiText("透视图"),ShortcutGesture::Rotate,ShortcutViewScope::Perspective,true);
    add(View,uiText("旋转板片"),uiText("Ctrl + 左键拖动"),uiText("板材展开"),ShortcutGesture::Rotate,ShortcutViewScope::None,true);
    add(View,uiText("缩放视图"),uiText("鼠标滚轮"),uiText("当前视图或型线图窗口"),ShortcutGesture::Wheel,ShortcutViewScope::CurrentOrLines);
    add(View,uiText("缩放表格"),uiText("Ctrl + 鼠标滚轮"),uiText("计算结果表格"),ShortcutGesture::Wheel,ShortcutViewScope::None,true);
    add(View,uiText("缩放至全图"),uiText("空白处双击左键"),uiText("当前视图"),ShortcutGesture::DoubleClick,ShortcutViewScope::Current);
    if(languageShortcut)
        for(const auto& key:languageShortcut->keys())
            add(Window,uiText("切换语言"),key.toString(QKeySequence::NativeText),target(languageShortcut));
    add(Window,uiText("关闭当前窗口"),"Esc",uiText("支持 Esc 关闭的窗口"));
    add(Window,uiText("最大化或还原窗口"),uiText("双击标题栏"),uiText("可最大化的窗口"));

    const std::array<QString,Groups> headings{uiText("船模文件"),uiText("视图"),uiText("窗口")};
    QList<QPair<int,ShortcutGesturePreview*>> previews;
    for(int group=0;group<Groups;++group){
        if(entries[group].isEmpty())continue;
        int row=table->rowCount();
        table->insertRow(row);
        table->setSpan(row,0,1,3);
        auto* heading=new QTableWidgetItem(headings[group]);
        heading->setData(Qt::UserRole,"group");
        table->setItem(row,0,heading);
        auto* groupLabel=new QLabel(headings[group]);
        groupLabel->setContentsMargins(10,5,10,5);
        groupLabel->setStyleSheet("background:#e4eee7;color:#315d50;border-top:1px solid #d2e2d8");
        auto font=table->font();
        font.setWeight(QFont::DemiBold);
        groupLabel->setFont(font);
        table->setCellWidget(row,0,groupLabel);
        for(const auto& entry:entries[group]){
            row=table->rowCount();
            table->insertRow(row);
            const QStringList cells{entry.description,entry.key,entry.scope};
            for(int col=0;col<3;++col)table->setItem(row,col,new QTableWidgetItem(cells[col]));
            if(entry.viewScope!=ShortcutViewScope::None)
                table->setCellWidget(row,2,shortcutViewScopeIcons(entry.viewScope,entry.scope,&dialog));
            if(entry.gesture==ShortcutGesture::None){
                if(group==Window&&entry.key==uiText("双击标题栏")){
                    auto* label=new QLabel(entry.key);
                    label->setContentsMargins(12,0,8,0);label->setWordWrap(true);
                    table->setCellWidget(row,1,label);
                    table->setItemDelegateForRow(row,new StudioWidgetCellDelegate(table));
                }
                continue;
            }
            auto* gestureCell=new QWidget;
            auto* gestureLayout=new QHBoxLayout(gestureCell);
            gestureLayout->setContentsMargins(12,0,8,0);
            gestureLayout->setSpacing(6);
            if(entry.ctrl){
                gestureLayout->addWidget(new StudioKeycap("Ctrl",gestureCell));
                auto* plus=new QLabel("+",gestureCell);
                plus->setStyleSheet("color:#688378");gestureLayout->addWidget(plus);
            }
            auto* preview=new ShortcutGesturePreview(entry.gesture,entry.key);
            gestureLayout->addWidget(preview,0,Qt::AlignVCenter);
            gestureCell->setMinimumHeight(preview->height());
            if(entry.gesture!=ShortcutGesture::Wheel){
                auto* label=new QLabel(entry.ctrl?uiText("左键拖动"):entry.key,gestureCell);
                label->setWordWrap(true);gestureLayout->addWidget(label,1);
            }else gestureLayout->addStretch();
            table->setCellWidget(row,1,gestureCell);
            table->setItemDelegateForRow(row,new StudioWidgetCellDelegate(table));
            previews.append({row,preview});
        }
    }
    QElapsedTimer elapsed;
    elapsed.start();
    QTimer timer(&dialog);
    QObject::connect(&timer,&QTimer::timeout,&dialog,[&]{
        for(const auto& [row,preview]:previews)
            if(table->visualItemRect(table->item(row,1)).intersects(table->viewport()->rect()))
                preview->setElapsed(elapsed.elapsed());
    });
    timer.start(40);
    table->verticalHeader()->setMinimumSectionSize(36);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    const bool localLanguage=languageShortcut&&languageShortcut->isEnabled()&&switchLanguage;
    constexpr int languageChanged=2;
    if(localLanguage){
        auto* binding=new QShortcut(&dialog);
        binding->setKeys(languageShortcut->keys());
        binding->setAutoRepeat(false);
        QObject::connect(binding,&QShortcut::activated,&dialog,[&]{
            const auto before=uiLanguage();
            switchLanguage();
            if(uiLanguage()!=before)dialog.done(languageChanged);
        });
        languageShortcut->setEnabled(false);
    }
    const int result=runStudioDialog(dialog);
    if(localLanguage)languageShortcut->setEnabled(true);
    return result==languageChanged;
}
