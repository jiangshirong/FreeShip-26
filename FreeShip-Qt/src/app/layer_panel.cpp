#include "studio_ui.h"
#include "app/ui_language.h"
#include "layer_panel.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QDynamicPropertyChangeEvent>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QKeyEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QStyleOptionToolButton>
#include <QTimer>
#include <QToolButton>
#include <QSpinBox>
#include <QSplitter>
#include <QJsonArray>
#include <QSet>
#include <QSlider>
#include <QSignalBlocker>
#include <cmath>
#include <limits>

QDialog* createLayerPropertiesDialog(QWidget* parent,LayerPanel*& panel){
    auto* dialog=new QDialog(parent);
    setStudioWindowIcon(dialog,"Layers");
    dialog->setWindowTitle(uiText("图层属性"));
    dialog->resize(800,727);
    dialog->setMinimumSize(650,480);
    auto* layout=new QVBoxLayout(dialog);
    layout->setContentsMargins(0,0,0,0);
    panel=new LayerPanel(dialog);
    layout->addWidget(panel,1);
    return dialog;
}

QString layerDisplayName(const fs::Layer& layer){
    return layer.defaultNameNumber?uiText(QStringLiteral("图层"))+QLatin1Char(' ')+QString::number(*layer.defaultNameNumber):layer.name;
}

namespace {
constexpr int LayerButtonWidth=28;
constexpr int LayerColorRole=Qt::UserRole+1;
class LayerVisibilityButton final : public QToolButton {
public:
    LayerVisibilityButton(){setFocusPolicy(Qt::TabFocus);setAttribute(Qt::WA_Hover);}
protected:
    void paintEvent(QPaintEvent*)override{
        // The eye alone carries the state. Native and stylesheet button panels
        // would paint a checked or disabled rectangle over the list row.
        QStyleOptionToolButton option;initStyleOption(&option);
        const auto mode=!isEnabled()?QIcon::Disabled:isDown()?QIcon::Selected:underMouse()?QIcon::Active:QIcon::Normal;
        QPainter painter(this);
        icon().paint(&painter,QStyle::alignedRect(layoutDirection(),Qt::AlignCenter,iconSize(),rect()),
                     Qt::AlignCenter,mode,isChecked()?QIcon::On:QIcon::Off);
        if(isEnabled()&&hasFocus()&&option.state.testFlag(QStyle::State_KeyboardFocusChange)){
            painter.setPen(QColor("#315f56"));
            painter.drawLine(rect().center().x()-5,height()-3,rect().center().x()+5,height()-3);
        }
    }
};
class LayerNameEdit final : public QLineEdit {
public:
    using QLineEdit::QLineEdit;
    std::function<void()> cancel, submit;
protected:
    void keyPressEvent(QKeyEvent* event) override {
        if(event->key()==Qt::Key_Escape){event->accept();cancel();return;}
        if(event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter){event->accept();submit();return;}
        QLineEdit::keyPressEvent(event);
    }
    void focusOutEvent(QFocusEvent* event)override{
        QLineEdit::focusOutEvent(event);
        if(event->reason()==Qt::MouseFocusReason||event->reason()==Qt::TabFocusReason||event->reason()==Qt::BacktabFocusReason)cancel();
    }
};
class LayerRowDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    static QRect nameRect(const QRect& rect){return rect.adjusted(LayerButtonWidth+20,0,-LayerButtonWidth,0);}
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index)const override{
        if(!index.data(Qt::UserRole).toULongLong())return;
        auto content=option;
        initStyleOption(&content,index);
        const auto* style=option.widget?option.widget->style():QApplication::style();
        const auto label=nameRect(option.rect);
        const auto text=content.fontMetrics.elidedText(content.text,Qt::ElideRight,qMax(0,label.width()));
        content.text.clear();content.icon=QIcon();
        content.state&=~QStyle::State_Editing;
        style->drawControl(QStyle::CE_ItemViewItem,&content,painter,option.widget);
        painter->save();painter->setFont(content.font);
        painter->fillRect(QRect(option.rect.left()+LayerButtonWidth,option.rect.center().y()-6,14,14),index.data(LayerColorRole).value<QColor>());
        const auto group=content.state.testFlag(QStyle::State_Enabled)?QPalette::Active:QPalette::Disabled;
        painter->setPen(content.palette.color(group,QPalette::Text));
        painter->drawText(label,Qt::AlignVCenter|Qt::AlignLeft,text);
        painter->restore();
    }
};

class LayerList final : public QListWidget {
public:
    explicit LayerList(QWidget* parent=nullptr):QListWidget(parent),delegate_(new LayerRowDelegate(this)){
        setItemDelegate(delegate_);
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll_.setInterval(30);
        connect(&scroll_,&QTimer::timeout,this,[this]{
            auto* bar=verticalScrollBar();
            constexpr int margin=24;
            const int y=pointer_.y();
            if(y<margin)bar->setValue(bar->value()-10);
            else if(y>=viewport()->height()-margin)bar->setValue(bar->value()+10);
            viewport()->update();
        });
    }
    std::function<bool()> canReorder;
    std::function<void(fs::EntityId,int)> onReorder;
    std::function<void(fs::EntityId,const QString&)> onRename;
    std::function<void()> onNameEditingFinished;
    void editName(QListWidgetItem* item){
        stopNameEditing();
        editingId_=item->data(Qt::UserRole).toULongLong();
        scrollToItem(item);
        name_=new LayerNameEdit(viewport());
        name_->setObjectName("layer_inline_name");
        name_->setMaxLength(std::numeric_limits<int>::max());
        bindUiText(name_,"accessibleName","名称");
        originalName_=item->text();name_->setText(originalName_);
        name_->cancel=[this]{stopNameEditing(name_&&name_->hasFocus());};
        name_->submit=[this]{
            if(!name_)return;
            const auto text=name_->text();
            if(text==originalName_){stopNameEditing(true);return;}
            if(onRename)onRename(editingId_,text);
        };
        positionName();name_->show();name_->setFocus(Qt::OtherFocusReason);name_->selectAll();
    }
    void refreshName(){
        if(!name_||!currentItem()||currentItem()->data(Qt::UserRole).toULongLong()!=editingId_)return;
        const bool unchanged=name_->text()==originalName_;
        originalName_=currentItem()->text();
        if(unchanged&&name_->text()!=originalName_){name_->setText(originalName_);name_->selectAll();}
    }
    void stopNameEditing(bool refocus=false){
        if(!name_)return;
        auto* editor=name_;name_=nullptr;editingId_=0;editor->hide();editor->setObjectName(QString());editor->deleteLater();
        if(refocus)setFocus(Qt::OtherFocusReason);
        if(onNameEditingFinished)onNameEditingFinished();
    }
    void stopReorder(){
        scroll_.stop();source_=0;
        if(dragging_){dragging_=false;viewport()->unsetCursor();viewport()->update();}
    }
protected:
    void resizeEvent(QResizeEvent* e)override{QListWidget::resizeEvent(e);positionName();}
    void scrollContentsBy(int dx,int dy)override{QListWidget::scrollContentsBy(dx,dy);positionName();}
    void mousePressEvent(QMouseEvent* e)override{
        const auto point=e->position().toPoint();
        stopReorder();
        if(e->button()==Qt::LeftButton)if(auto* rowItem=itemAt(point)){
            source_=rowItem->data(Qt::UserRole).toULongLong();sourceRow_=row(rowItem);start_=point;
        }
        QListWidget::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent* e)override{
        if(source_&&e->buttons().testFlag(Qt::LeftButton)){
            pointer_=e->position().toPoint();
            if(canReorder&&!canReorder()){stopReorder();QListWidget::mouseMoveEvent(e);return;}
            if(!dragging_&&(pointer_-start_).manhattanLength()>=QApplication::startDragDistance()){
                dragging_=true;viewport()->setCursor(Qt::ClosedHandCursor);scroll_.start();
            }
            if(dragging_){viewport()->update();e->accept();return;}
        }
        QListWidget::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e)override{
        const auto id=source_;const bool dragging=dragging_;
        const int slot=dragging?insertionSlot(e->position().toPoint()):-1;
        const int destination=slot>sourceRow_?slot-1:slot;
        const bool changed=destination>=0&&destination!=sourceRow_;
        stopReorder();
        if(dragging&&e->button()==Qt::LeftButton){
            // Reset the view's press state without treating the drop row as a click.
            QMouseEvent outside(QEvent::MouseButtonRelease,QPointF(-1,-1),e->globalPosition(),e->button(),e->buttons(),e->modifiers());
            QListWidget::mouseReleaseEvent(&outside);
            if(changed&&onReorder)onReorder(id,destination);
            e->accept();return;
        }
        QListWidget::mouseReleaseEvent(e);
    }
    void paintEvent(QPaintEvent* e)override{
        QListWidget::paintEvent(e);
        if(!dragging_)return;
        const int slot=insertionSlot(pointer_);
        if(slot<0||slot==sourceRow_||slot==sourceRow_+1)return;
        QPainter painter(viewport());painter.setPen(QPen(palette().color(QPalette::Highlight),2));
        const int y=qMax(1,visualItemRect(item(slot)).top());
        painter.drawLine(1,y,viewport()->width()-2,y);
    }
    void keyPressEvent(QKeyEvent* e)override{
        if(e->key()==Qt::Key_Escape&&source_){stopReorder();e->accept();return;}
        QListWidget::keyPressEvent(e);
    }
private:
    int insertionSlot(const QPoint& point)const{
        if(count()<2||!viewport()->rect().contains(point))return -1;
        if(auto* hit=itemAt(point)){
            const auto index=row(hit);
            if(index==count()-1)return index; // The add button is always the final row.
            return index+(point.y()>visualItemRect(hit).center().y()?1:0);
        }
        return count()-1;
    }
    void positionName(){
        if(name_&&currentItem()&&currentItem()->data(Qt::UserRole).toULongLong()==editingId_)
            name_->setGeometry(LayerRowDelegate::nameRect(visualItemRect(currentItem())));
    }
    LayerRowDelegate* delegate_;
    LayerNameEdit* name_=nullptr;
    fs::EntityId editingId_=0;
    QString originalName_;
    QTimer scroll_;
    fs::EntityId source_=0;
    int sourceRow_=0;
    bool dragging_=false;
    QPoint start_,pointer_;
};

QColor displayColor(quint32 color) { return QColor(color & 255, (color >> 8) & 255, (color >> 16) & 255); }
quint32 storedColor(QColor color) { return quint32(color.red()) | quint32(color.green()) << 8 | quint32(color.blue()) << 16; }
class NumberEditor final : public QDoubleSpinBox {
protected:
    QString textFromValue(double value) const override {
        QString text = QDoubleSpinBox::textFromValue(value);
        if (text.contains(locale().decimalPoint())) {
            while (text.endsWith('0')) text.chop(1);
            if (text.endsWith(locale().decimalPoint())) text.chop(locale().decimalPoint().size());
        }
        return text;
    }
};
class ColorEditor final : public QPushButton {
public:
    std::function<void()> onChanged;
protected:
    bool event(QEvent* event) override {
        if (event->type() == QEvent::DynamicPropertyChange &&
            static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName() == "colorValue") {
            const auto color = displayColor(property("colorValue").toUInt());
            setText(color.name().toUpper());
            setStyleSheet(QString("QPushButton { background-color: %1; color: %2; padding: 5px; }")
                .arg(color.name(), color.lightness() > 140 ? "#18212b" : "white"));
            if (onChanged) onChanged();
        }
        return QPushButton::event(event);
    }
};
QJsonObject layerValues(const fs::Layer& layer) {
    return {{"color", double(layer.color)},
        {"symmetric", layer.symmetric}, {"developable", layer.developable},
        {"use_in_hydrostatics", layer.useInHydrostatics}, {"use_for_intersections", layer.useForIntersections},
        {"show_in_linesplan", layer.showInLinesplan}, {"thickness", layer.thickness}, {"material_density", layer.materialDensity},{"alpha_blend",layer.alphaBlend}};
}
}

struct LayerPanel::State {
    std::shared_ptr<const fs::ModelSnapshot> snapshot;
    enum Kind { Insert, Remove, Replace };
    struct Change {
        fs::Layer layer; int index; Kind kind; fs::Layer previous{};
        Change inverse()const{return kind==Replace?Change{previous,index,Replace,layer}:Change{layer,index,kind==Insert?Remove:Insert};}
    };
    int committedHigh=0, projectedHigh=0, projectedNumber=-1;
    QVector<fs::Layer> rows;
    QVector<Change> before,after,expected;
    bool listAllowed=false;
    const fs::Layer* layer(fs::EntityId id)const{for(const auto& l:rows)if(l.id==id)return &l;return nullptr;}
    void projectRow(const Change& change){
        if(change.kind==Remove)rows.removeAt(change.index);
        else if(change.kind==Insert)rows.insert(change.index,change.layer);
        else rows[change.index]=change.layer;
        expected.append(change);
    }
    void record(const Change& op){
        // Keep a run of trash actions scoped to those deletions, as in the section editor.
        if(op.kind==Remove&&!before.isEmpty()&&before.last().kind!=Remove)before.clear();
        before.append(op);after.clear();projectRow(op);
    }
    QHash<QString, QWidget*> editors;
    QHash<QString, NumberEditor*> numbers;
    LayerList* list = nullptr;
    QToolButton* add=nullptr;
    QHash<fs::EntityId,QToolButton*> remove, visibility;
    QSet<fs::EntityId> occupied;
    fs::EntityId focusName=0;
    QSpinBox* transparency=nullptr;
    QSlider* alphaSlider=nullptr;
    QCheckBox* controlNet=nullptr;
    QSet<fs::EntityId> hiddenControl;
    QLabel* measurements=nullptr;
    QHash<fs::EntityId,QPair<double,fs::Vec3>> measurementsByLayer;
    QWidget* properties = nullptr;
    ColorEditor* color = nullptr;
    QCheckBox* symmetric = nullptr;
    StudioMessage *error = nullptr, *empty = nullptr;
    fs::EntityId selected = 0;
    bool loading = true, editingEnabled = true;
};

LayerPanel::LayerPanel(QWidget* parent) : QWidget(parent), state_(std::make_unique<State>()) {
    auto& s = *state_;
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    s.empty = new StudioMessage(StudioMessageRole::Explanation,uiText("未打开船模"));
    s.empty->setWordWrap(true); layout->addWidget(s.empty);
    s.list = new LayerList;
    s.list->canReorder=[this]{return state_->editingEnabled;};
    s.list->onReorder=[this](fs::EntityId id,int index){if(state_->editingEnabled&&onMoveLayer)onMoveLayer(id,index);};
    s.list->setObjectName("layer_list");s.list->setMinimumWidth(180);
    auto* split=new QSplitter;split->addWidget(s.list);layout->addWidget(split,1);
    auto* scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame); scroll->setWidgetResizable(true);
    s.properties = new QWidget;
    auto* form = new QFormLayout(s.properties);
    form->setContentsMargins(0, 5, 4, 4); form->setVerticalSpacing(9);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    scroll->setWidget(s.properties);split->addWidget(scroll);split->setStretchFactor(0,1);split->setStretchFactor(1,1);split->setSizes({380,380});
    auto bind = [&s](const QString& field, QWidget* editor) {
        editor->setProperty("modelField", field); editor->setObjectName("layer_" + field); s.editors.insert(field, editor);
    };
    s.color = new ColorEditor; bind("color", s.color); form->addRow(uiText("颜色"), s.color);
    s.color->setAutoDefault(false);
    connect(s.color,&QPushButton::clicked,this,[this]{
        auto& state=*state_;
        if(!state.snapshot||!state.selected)return;
        const auto document=state.snapshot->documentId;const auto layer=state.selected;
        const auto color=studioChooseColor(displayColor(state.color->property("colorValue").toUInt()),state.color,uiText("选择图层颜色"),QColorDialog::DontUseNativeDialog);
        if(color.isValid()&&state.snapshot&&state.snapshot->documentId==document&&state.selected==layer)
            state.color->setProperty("colorValue",storedColor(color));
    });
    s.transparency=new QSpinBox;s.transparency->setRange(0,100);s.transparency->setSuffix(" %");s.transparency->setKeyboardTracking(false);bind("alpha_blend",s.transparency);
    auto* alphaRow=new QWidget;auto* alphaLayout=new QHBoxLayout(alphaRow);alphaLayout->setContentsMargins(0,0,0,0);
    auto* alphaSlider=s.alphaSlider=new QSlider(Qt::Horizontal);alphaSlider->setRange(0,100);alphaSlider->setAccessibleName(uiText("透明度"));
    alphaLayout->addWidget(alphaSlider,1);alphaLayout->addWidget(s.transparency);form->addRow(uiText("透明度"),alphaRow);
    connect(alphaSlider,&QSlider::valueChanged,s.transparency,&QSpinBox::setValue);
    connect(s.transparency,&QSpinBox::valueChanged,alphaSlider,&QSlider::setValue);
    connect(s.transparency,&QSpinBox::valueChanged,this,[this]{if(!state_->alphaSlider->isSliderDown())submitTransparency();});
    connect(alphaSlider,&QSlider::sliderReleased,this,&LayerPanel::submitTransparency);
    auto addCheck = [&](const QString& field, const QString& text) {
        auto* box = new QCheckBox(text); bind(field, box); form->addRow(box);
        connect(box, &QCheckBox::toggled, this, [this,field](bool checked) { submitProperty(field,checked); });
        return box;
    };
    s.symmetric = addCheck("symmetric", uiText("关于中纵剖面对称"));
    addCheck("developable", uiText("可展开曲面"));
    addCheck("use_in_hydrostatics", uiText("参与静水力计算"));
    addCheck("use_for_intersections", uiText("生成交线"));
    addCheck("show_in_linesplan", uiText("在型线图中显示"));
    s.controlNet=new QCheckBox(uiText("显示此图层的控制网格"));bind("control_net_visible",s.controlNet);form->addRow(s.controlNet);
    connect(s.controlNet,&QCheckBox::toggled,this,[this](bool show){auto& s=*state_;if(s.loading||!s.selected)return;if(show)s.hiddenControl.remove(s.selected);else s.hiddenControl.insert(s.selected);if(onControlNetChanged)onControlNetChanged(s.selected,show);});
    for (const auto& field : {QString("thickness"), QString("material_density")}) {
        auto* number = new NumberEditor;
        number->setDecimals(12); number->setRange(0, 1e9); number->setSingleStep(0.01); number->setKeyboardTracking(false);
        s.numbers.insert(field, number); bind(field, number);
        form->addRow(field == "thickness" ? uiText("板厚") : uiText("材料密度"), number);
        connect(number, &QDoubleSpinBox::valueChanged, this, [this, field](double value) {
            submitProperty(field,value);
        });
    }
    s.color->onChanged = [this] { submitProperty("color",double(state_->color->property("colorValue").toUInt())); };
    connect(s.list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current) {
        auto& state = *state_;
        if (state.loading) return;
        state.list->stopNameEditing();
        commitText();
        state.selected = current ? current->data(Qt::UserRole).toULongLong() : 0;
        loadSelected();
        if(onSelectionChanged)onSelectionChanged();
    });
    connect(s.list,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem* item){
        if(state_->editingEnabled||state_->listAllowed)if(const auto id=item->data(Qt::UserRole).toULongLong())editLayerName(id);
    });
    s.list->onRename=[this](fs::EntityId id,const QString& text){
        auto& s=*state_;
        if(!(s.editingEnabled||s.listAllowed)||!s.layer(id))return;
        const char* warning=text.contains(QChar(0))?"图层名称中不能包含空字符。":text.toUtf8().size()>65536?"图层名称过长。":nullptr;
        if(warning){s.error->setText(uiText(warning));s.error->show();return;}
        s.list->stopNameEditing(true);
        previewValue(id,{{"name",text}});
    };
    s.measurements=new QLabel;s.measurements->setProperty("studio_content_role","result_data");s.measurements->setWordWrap(true);form->addRow(s.measurements);
    s.error = new StudioMessage(StudioMessageRole::Warning);s.error->setObjectName("LayerWarning");layout->addWidget(s.error);
    s.list->onNameEditingFinished=[this]{state_->error->hide();};
    bindUiRefresh(this,[this]{
        auto& s=*state_;
        if(s.snapshot){
            const QSignalBlocker block(s.list);
            for(int i=0;i<s.rows.size();++i)s.list->item(i)->setText(layerDisplayName(s.rows[i]));
            s.list->refreshName();
        }
        updateMeasurements();
    });
    s.loading = false; updateEnabledState();
}

LayerPanel::~LayerPanel() = default;
void LayerPanel::editLayerName(fs::EntityId id){
    setSelectedLayerId(id);
    state_->focusName=id;
    updateEnabledState();
}
void LayerPanel::finishEditing(){
    commitText();
    state_->focusName=0;
    state_->list->stopNameEditing();
}
void LayerPanel::commitText(){
    auto& s=*state_;
    if(s.loading||!(s.editingEnabled||s.listAllowed))return;
    s.transparency->interpretText();
    for(auto* number:s.numbers)number->interpretText();
}
QWidget* LayerPanel::fieldEditor(const QString& field) const { return state_->editors.value(field, nullptr); }
fs::EntityId LayerPanel::selectedLayerId() const { return state_->selected; }

void LayerPanel::setSelectedLayerId(fs::EntityId id) {
    auto& s = *state_;
    for (int i = 0; i < s.list->count(); ++i) {
        if (s.list->item(i)->data(Qt::UserRole).toULongLong() == id) { s.list->setCurrentRow(i); return; }
    }
}

void LayerPanel::setSnapshot(std::shared_ptr<const fs::ModelSnapshot> snapshot,int historyStates,int highWater) {
    auto& s = *state_;
    const bool sameDocument = s.snapshot && snapshot && s.snapshot->documentId == snapshot->documentId;
    if (!sameDocument) { s.hiddenControl.clear();s.selected = 0;s.focusName=0; }
    if(snapshot!=s.snapshot){
        s.committedHigh=highWater;
        bool matched=false;
        if(sameDocument&&!s.expected.isEmpty()&&snapshot->revision==s.snapshot->revision+1){
            const auto& op=s.expected.front();const auto& old=s.snapshot->layers;const auto& now=snapshot->layers;
            const bool removing=op.kind==State::Remove, replacing=op.kind==State::Replace;
            const auto& target=removing?old:now;
            matched=op.index>=0&&op.index<target.size()&&target[op.index].id==op.layer.id
                &&now.size()==old.size()+(replacing?0:removing?-1:1);
            for(int i=0;matched&&i<qMin(now.size(),old.size());++i)
                matched=replacing?now[i].id==old[i].id:removing?now[i].id==old[i+(i>=op.index)].id:old[i].id==now[i+(i>=op.index)].id;
            if(matched&&!removing){const auto& actual=now[op.index];
                matched=actual.visible==op.layer.visible
                    &&(op.layer.defaultNameNumber||op.layer.name.isEmpty()?actual.defaultNameNumber.has_value():actual.name==op.layer.name)
                    &&(!replacing||layerValues(actual)==layerValues(op.layer));
            }
        }
        if(matched){s.expected.removeFirst();s.snapshot=snapshot;
            if(!s.expected.isEmpty())return;
            if(historyStates>0&&s.before.size()>historyStates-1-s.after.size())s.before.remove(0,s.before.size()-qMax<qsizetype>(0,historyStates-1-s.after.size()));
            s.projectedHigh=highWater;s.projectedNumber=snapshot->lastLayerNumber;
            s.rows=snapshot->layers;s.measurementsByLayer.clear();
            for(int i=0;i<s.rows.size();++i)if(s.list->item(i)->text()!=layerDisplayName(s.rows[i]))s.list->item(i)->setText(layerDisplayName(s.rows[i]));
            s.list->refreshName();updateEnabledState();return;
        }
        if(!s.expected.isEmpty()&&onMismatch)onMismatch();
        s.before.clear();s.after.clear();s.expected.clear();s.measurementsByLayer.clear();
        s.snapshot=std::move(snapshot);s.occupied.clear();
        if(s.snapshot)for(const auto& face:s.snapshot->faces)s.occupied.insert(face.layer);
        s.projectedHigh=highWater;s.projectedNumber=s.snapshot?s.snapshot->lastLayerNumber:-1;
        s.rows=s.snapshot?s.snapshot->layers:QVector<fs::Layer>{};
    }
    rebuildRows();
}

bool LayerPanel::canDelete(fs::EntityId id)const{
    const auto& s=*state_;return (s.editingEnabled||s.listAllowed)&&s.rows.size()>1&&s.layer(id)&&!s.occupied.contains(id);
}
void LayerPanel::previewCreate(){
    auto& s=*state_;
    if(!(s.editingEnabled||s.listAllowed)||!onListCommand||!s.snapshot||!s.projectedHigh||s.projectedHigh>=2000000||s.projectedNumber==std::numeric_limits<int>::max())return;
    fs::Layer layer;layer.id=s.projectedHigh+1;layer.defaultNameNumber=s.projectedNumber+1;layer.color=s.snapshot->newLayerColor;
    if(!onListCommand("manage_layers",{{"operation","create"},{"changes",QJsonObject{}},{"requested_new_id",double(layer.id)}}))return;
    ++s.projectedHigh;++s.projectedNumber;
    const auto index=int(s.rows.size());s.record({layer,index,State::Insert});refreshRow(index,false);editLayerName(layer.id);
}
void LayerPanel::updateValueRow(int index){
    auto& s=*state_;const auto& layer=s.rows[index];auto* eye=s.visibility.value(layer.id);
    eye->setChecked(layer.visible);eye->setProperty("layer_visible",layer.visible);eye->setIcon(studioIcon(layer.visible?"LayerVisible":"LayerHidden"));
    s.list->item(index)->setText(layerDisplayName(layer));
    s.list->item(index)->setData(LayerColorRole,displayColor(layer.color));s.list->refreshName();
}
bool LayerPanel::previewValue(fs::EntityId id,const QJsonObject& value){
    auto& s=*state_;const auto* current=s.layer(id);if(!current)return false;
    const int index=int(current-s.rows.constData());auto next=*current;
    if(value.contains("visible"))next.visible=value["visible"].toBool();
    if(value.contains("name")){
        const auto name=value["name"].toString();
        if(name==layerDisplayName(*current))return true;
        next.name=name;next.defaultNameNumber.reset();
    }
    if(value.contains("color"))next.color=quint32(value["color"].toDouble());
    if(value.contains("alpha_blend"))next.alphaBlend=value["alpha_blend"].toInt();
    if(value.contains("developable"))next.developable=value["developable"].toBool();
    if(value.contains("use_for_intersections"))next.useForIntersections=value["use_for_intersections"].toBool();
    if(value.contains("show_in_linesplan"))next.showInLinesplan=value["show_in_linesplan"].toBool();
    if(value.contains("use_in_hydrostatics")){
        next.useInHydrostatics=value["use_in_hydrostatics"].toBool();
        // Match the document's default when a layer starts contributing to hydrostatics.
        if(next.useInHydrostatics&&!current->useInHydrostatics)next.symmetric=true;
    }
    if(value.contains("symmetric"))next.symmetric=value["symmetric"].toBool();
    if(value.contains("thickness"))next.thickness=float(value["thickness"].toDouble());
    if(value.contains("material_density"))next.materialDensity=float(value["material_density"].toDouble());
    if(!value.contains("name")&&next.visible==current->visible&&layerValues(next)==layerValues(*current))return true;
    const State::Change op{next,index,State::Replace,*current};
    const bool sent=(s.editingEnabled||s.listAllowed)&&onListCommand
        &&onListCommand("update_layer",{{"layer_id",double(id)},{"changes",value}});
    if(sent)s.record(op);
    updateValueRow(index);
    if(sent&&value.contains("visible")&&onVisibilityPreview)onVisibilityPreview();
    return sent;
}
void LayerPanel::submitProperty(const QString& field,const QJsonValue& value){
    auto& s=*state_;
    if(s.loading||!s.selected)return;
    if(!previewValue(s.selected,{{field,value}})){loadSelected();return;}
    if(field=="use_in_hydrostatics"){
        const QSignalBlocker block(s.symmetric);
        s.symmetric->setChecked(s.layer(s.selected)->symmetric);
    }
    updateMeasurements();
}
void LayerPanel::submitTransparency(){
    const auto& s=*state_;
    if(s.loading)return;
    const auto* layer=s.layer(s.selected);
    if(!layer||s.transparency->value()==qRound((255-layer->alphaBlend)*100./255))return;
    submitProperty("alpha_blend",qRound(255*(100-s.transparency->value())/100.));
}
void LayerPanel::previewDelete(fs::EntityId id){
    auto& s=*state_;if(!canDelete(id)||!onListCommand)return;
    int index=int(s.layer(id)-s.rows.constData());State::Change op{s.rows.at(index),index,State::Remove};
    if(!onListCommand("manage_layers",{{"operation","delete"},{"layer_id",double(id)}}))return;
    s.record(op);refreshRow(index,true);
}
bool LayerPanel::previewHistory(bool redo){
    auto& s=*state_;if(s.before.isEmpty()&&s.after.isEmpty()&&s.expected.isEmpty())return false;
    auto& from=redo?s.after:s.before;auto& to=redo?s.before:s.after;
    if(from.isEmpty()||!onListCommand||!onListCommand(redo?"redo":"undo",{}))return true;
    const auto original=from.takeLast();to.append(original);const auto op=redo?original:original.inverse();
    s.projectRow(op);if(op.kind==State::Replace)updateValueRow(op.index);else refreshRow(op.index,op.kind==State::Remove);
    if(op.kind==State::Replace){
        if(op.layer.id==s.selected&&layerValues(op.layer)!=layerValues(op.previous))loadSelected();
        if(op.previous.visible!=op.layer.visible&&onVisibilityPreview)onVisibilityPreview();
    }
    return true;
}
void LayerPanel::discardPredictions(){
    auto& s=*state_;if(s.expected.isEmpty())return;
    s.expected.clear();s.before.clear();s.after.clear();s.projectedHigh=s.committedHigh;s.projectedNumber=s.snapshot->lastLayerNumber;s.rows=s.snapshot->layers;rebuildRows();
    if(onVisibilityPreview)onVisibilityPreview();
}
QSet<fs::EntityId> LayerPanel::projectedVisibilityChanges() const {
    const auto& s=*state_;QSet<fs::EntityId> result;
    // Only occupied layers can change the existing hull drawing. Collapse pending
    // toggles by parity, relative to the most recently acknowledged snapshot.
    for(const auto& op:s.expected)if(op.kind==State::Replace&&op.layer.visible!=op.previous.visible&&s.occupied.contains(op.layer.id)){
        if(!result.remove(op.layer.id))result.insert(op.layer.id);
    }
    return result;
}
namespace {
    auto rowContainer=[](QListWidgetItem* item){
        auto* row=new QHBoxLayout(new QWidget);
        row->setContentsMargins(0,0,2,0);row->setSpacing(0);
        item->setSizeHint(QSize(0,24));
        return row;
    };
    auto rowButton=[](QHBoxLayout* row,const QString& icon,const char* label,QToolButton* button=nullptr){
        if(!button)button=new QToolButton;
        button->setAutoRaise(true);button->setFixedSize(24,24);button->setIconSize(QSize(17,17));
        button->setIcon(studioIcon(icon));
        bindUiText(button,"toolTip",label);bindUiText(button,"accessibleName",label);
        row->addWidget(button);
        return button;
    };
}
void LayerPanel::insertRow(int index,const fs::Layer& layer){
    auto& s=*state_;
    auto* item = new QListWidgetItem(layerDisplayName(layer));
    s.list->insertItem(index,item);
    item->setData(Qt::UserRole, QVariant::fromValue(layer.id));
    item->setData(LayerColorRole,displayColor(layer.color));
    auto* row=rowContainer(item);
    auto* visibility=rowButton(row,layer.visible?"LayerVisible":"LayerHidden","显示图层",new LayerVisibilityButton);
    visibility->setCheckable(true);visibility->setChecked(layer.visible);
    visibility->setProperty("layer_action","visibility");visibility->setProperty("layer_id",QVariant::fromValue(layer.id));
    s.visibility.insert(layer.id,visibility);
    visibility->setProperty("layer_visible",layer.visible);
    connect(visibility,&QToolButton::clicked,this,[this,id=layer.id](bool next){
        previewValue(id,{{"visible",next}});
    });
    row->addStretch();
    auto* remove=rowButton(row,"DeleteOne","删除空图层");
    remove->setProperty("layer_action","delete");remove->setProperty("layer_id",QVariant::fromValue(layer.id));
    s.list->setItemWidget(item,row->parentWidget());
    s.remove.insert(layer.id,remove);
    connect(remove,&QToolButton::clicked,this,[this,id=layer.id]{if(onDeleteLayer)onDeleteLayer(id);});
}
void LayerPanel::refreshRow(int index,bool remove){
    auto& s=*state_;s.loading=true;s.list->stopReorder();s.list->stopNameEditing();
    if(remove){
        auto* item=s.list->item(index);const auto id=item->data(Qt::UserRole).toULongLong();
        s.visibility.remove(id);s.remove.remove(id);delete s.list->takeItem(index);
    }else insertRow(index,s.rows.at(index));
    if(!s.layer(s.selected))s.list->setCurrentRow(qMin(index,int(s.rows.size())-1));
    s.selected=s.list->currentItem()?s.list->currentItem()->data(Qt::UserRole).toULongLong():0;
    s.loading=false;loadSelected();
}
void LayerPanel::rebuildRows(){
    auto& s=*state_;
    auto* focus=QApplication::focusWidget();
    const bool restoreListFocus=focus&&(focus==s.list||s.list->isAncestorOf(focus));
    s.loading = true;s.list->stopReorder();s.list->stopNameEditing();s.add=nullptr;s.remove.clear();s.visibility.clear();s.list->clear();
    int selectedRow = -1;
    if (s.snapshot) {
        for (const auto& layer : s.rows) {
            const int index=s.list->count();insertRow(index,layer);
            if(layer.id==s.selected)selectedRow=index;
        }
        auto* footer=new QListWidgetItem(s.list);footer->setFlags(Qt::ItemIsEnabled);
        auto* row=rowContainer(footer);s.add=rowButton(row,"AddOne","新建图层");row->addStretch();s.list->setItemWidget(footer,row->parentWidget());s.add->setProperty("layer_action","create");
        connect(s.add,&QToolButton::clicked,this,[this]{if(onCreateLayer)onCreateLayer();});
    }
    if (selectedRow < 0 && s.snapshot && !s.rows.isEmpty()) selectedRow = 0;
    s.list->setCurrentRow(selectedRow);
    s.selected = selectedRow < 0 ? 0 : s.list->item(selectedRow)->data(Qt::UserRole).toULongLong();
    s.loading = false; loadSelected();
    if(restoreListFocus&&s.list->isEnabled())s.list->setFocus(Qt::OtherFocusReason);
}

void LayerPanel::setEditingEnabled(bool enabled,bool allowList) {
    state_->listAllowed=allowList;
    state_->editingEnabled = enabled;
    if(enabled){
        // A failed request keeps the old snapshot and the old visibility.
        for(auto* button:state_->visibility){
            const bool committed=button->property("layer_visible").toBool();
            if(button->isChecked()!=committed){button->setChecked(committed);button->setIcon(studioIcon(committed?"LayerVisible":"LayerHidden"));}
        }
    }else if(!allowList)state_->list->stopNameEditing();
    updateEnabledState();
}

void LayerPanel::loadSelected() {
    auto& s = *state_;
    const auto* layer = s.snapshot ? s.layer(s.selected) : nullptr;
    s.loading = true;
    const auto values = layer ? layerValues(*layer) : QJsonObject{};
    s.color->setProperty("colorValue", quint32(values["color"].toDouble()));
    const auto percent=qRound((255-values["alpha_blend"].toInt(255))*100./255);
    s.transparency->setValue(percent);s.alphaSlider->setValue(percent);
    s.controlNet->setChecked(!s.hiddenControl.contains(s.selected));
    for (const auto key : {"symmetric", "developable", "use_in_hydrostatics", "use_for_intersections", "show_in_linesplan"})
        static_cast<QCheckBox*>(s.editors[key])->setChecked(values[key].toBool());
    const bool imperial = s.snapshot && s.snapshot->units == "imperial";
    for (auto it = s.numbers.begin(); it != s.numbers.end(); ++it) {
        it.value()->setValue(values[it.key()].toDouble());
        it.value()->setSuffix(it.key() == "thickness" ? (imperial ? " in" : " mm") : (imperial ? " lb/ft³" : " t/m³"));
    }
    s.error->hide(); s.loading = false; updateEnabledState();
}

void LayerPanel::updateEnabledState() {
    auto& s = *state_;
    const bool selected = s.snapshot && s.layer(s.selected);
    s.empty->setVisible(!selected); s.list->setVisible(s.snapshot && !s.rows.isEmpty());
    s.properties->setEnabled(selected && (s.editingEnabled||s.listAllowed));
    const bool editable=s.editingEnabled;
    s.list->setEnabled(editable||s.listAllowed);
    if(s.add)s.add->setEnabled((editable||s.listAllowed)&&s.projectedHigh>0&&s.projectedHigh<2000000&&s.projectedNumber<std::numeric_limits<int>::max()&&bool(onCreateLayer));
    for(auto it=s.remove.cbegin();it!=s.remove.cend();++it)
        it.value()->setEnabled((editable||s.listAllowed)&&onDeleteLayer&&s.remove.size()>1&&!s.occupied.contains(it.key()));
    for(auto* button:s.visibility)button->setEnabled((editable||s.listAllowed)&&bool(onListCommand));
    if((editable||s.listAllowed)&&s.focusName){
        if(s.selected==s.focusName&&s.list->currentItem()&&isVisible())s.list->editName(s.list->currentItem());
        s.focusName=0;
    }
    updateMeasurements();
}

void LayerPanel::updateMeasurements(){
    auto& s=*state_;if(!s.measurements)return;
    const auto* layer=s.snapshot?s.layer(s.selected):nullptr;
    if(!layer){s.measurements->clear();return;}
    const auto found=s.measurementsByLayer.constFind(s.selected);double area=0;fs::Vec3 center{};
    if(found!=s.measurementsByLayer.cend()){area=found->first;center=found->second;}else{
    if(s.occupied.contains(s.selected))for(const auto& t:s.snapshot->triangles)if(t.layer==s.selected){const auto& a=t.vertices[0];const auto& b=t.vertices[1];const auto& c=t.vertices[2];
        const fs::Vec3 u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};
        const double n=std::hypot(u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x)*.5;
        area+=n;center.x+=n*(a.x+b.x+c.x)/3;center.y+=n*(a.y+b.y+c.y)/3;center.z+=n*(a.z+b.z+c.z)/3;
    }
    if(area>0){center.x/=area;center.y/=area;center.z/=area;}
    s.measurementsByLayer.insert(s.selected,{area,center});}
    if(layer->symmetric){area*=2;center.y=0;}
    const bool imperial=s.snapshot->units=="imperial";const double weight=area*layer->thickness*layer->materialDensity/(imperial?12*2240.:1000.);
    const QString unit=imperial?"ft":"m";
    s.measurements->setText(QString(uiText("面积：%1 %2²\n重量：%3 %4\n重心：%5, %6, %7 %2")).arg(area,0,'f',3).arg(unit).arg(weight,0,'f',3).arg(imperial?"long ton":"t").arg(center.x,0,'f',3).arg(center.y,0,'f',3).arg(center.z,0,'f',3));
    setProperty("layer_measurements",QJsonObject{{"area",area},{"weight",weight},{"center",QJsonArray{center.x,center.y,center.z}},{"units",unit}});
}
