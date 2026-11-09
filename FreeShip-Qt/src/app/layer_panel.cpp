#include "layer_panel.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QDynamicPropertyChangeEvent>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QToolBar>
#include <QSpinBox>
#include <QSplitter>
#include <QJsonArray>
#include <QSet>
#include <QSlider>
#include <QSignalBlocker>
#include <cmath>
#include <limits>

namespace {
constexpr double DensityFactor = 1000.0 * 0.3048 * 0.3048 * 0.3048 / 0.45359237;
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
    explicit ColorEditor(QWidget* parent = nullptr) : QPushButton(parent) {
        connect(this, &QPushButton::clicked, this, [this] {
            const auto color = QColorDialog::getColor(displayColor(property("colorValue").toUInt()), this, "选择图层颜色");
            if (color.isValid()) setProperty("colorValue", storedColor(color));
        });
    }
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
    return {{"name", layer.name}, {"color", double(layer.color)}, {"visible", layer.visible},
        {"symmetric", layer.symmetric}, {"developable", layer.developable},
        {"use_in_hydrostatics", layer.useInHydrostatics}, {"use_for_intersections", layer.useForIntersections},
        {"show_in_linesplan", layer.showInLinesplan}, {"thickness", layer.thickness}, {"material_density", layer.materialDensity},{"alpha_blend",layer.alphaBlend}};
}
}

struct LayerPanel::State {
    struct Number { NumberEditor* widget = nullptr; double original = 0, draft = 0; bool edited = false; };
    std::shared_ptr<const fs::ModelSnapshot> snapshot;
    QHash<QString, QWidget*> editors;
    QHash<QString, Number> numbers;
    QHash<fs::EntityId, QJsonObject> drafts;
    QJsonObject original;
    QString displayedName;
    QListWidget* list = nullptr;
    QToolBar* toolbar=nullptr;
    QSpinBox* transparency=nullptr;
    QCheckBox* controlNet=nullptr;
    QSet<fs::EntityId> hiddenControl;
    QLabel* measurements=nullptr;
    fs::EntityId measuredLayer=0;double measuredArea=0;fs::Vec3 measuredCenter{};
    bool alphaEdited=false;
    QWidget* properties = nullptr;
    QLineEdit* name = nullptr;
    ColorEditor* color = nullptr;
    QCheckBox *hydro = nullptr, *symmetric = nullptr;
    QPushButton* apply = nullptr;
    QLabel *error = nullptr, *encoding = nullptr, *empty = nullptr;
    fs::EntityId selected = 0, pending = 0;
    quint64 pendingRevision = 0;
    bool loading = true, editingEnabled = true, symmetricExplicit = false;
};

LayerPanel::LayerPanel(QWidget* parent) : QWidget(parent), state_(std::make_unique<State>()) {
    auto& s = *state_;
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    s.toolbar=new QToolBar;layout->addWidget(s.toolbar);
    s.empty = new QLabel("打开模型后可编辑图层属性。");
    s.empty->setWordWrap(true); layout->addWidget(s.empty);
    s.list = new QListWidget;
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
    s.name = new QLineEdit; s.name->setMaxLength(std::numeric_limits<int>::max()); bind("name", s.name); form->addRow("名称", s.name);
    s.encoding = new QLabel; s.encoding->setWordWrap(true); s.encoding->setStyleSheet("color: #586472;"); form->addRow(s.encoding);
    s.color = new ColorEditor; bind("color", s.color); form->addRow("颜色", s.color);
    s.transparency=new QSpinBox;s.transparency->setRange(0,100);s.transparency->setSuffix(" %");bind("alpha_blend",s.transparency);
    auto* alphaRow=new QWidget;auto* alphaLayout=new QHBoxLayout(alphaRow);alphaLayout->setContentsMargins(0,0,0,0);
    auto* alphaSlider=new QSlider(Qt::Horizontal);alphaSlider->setRange(0,100);alphaSlider->setAccessibleName("透明度");
    alphaLayout->addWidget(alphaSlider,1);alphaLayout->addWidget(s.transparency);form->addRow("透明度",alphaRow);
    connect(alphaSlider,&QSlider::valueChanged,s.transparency,&QSpinBox::setValue);
    connect(s.transparency,&QSpinBox::valueChanged,alphaSlider,&QSlider::setValue);
    connect(s.transparency,&QSpinBox::valueChanged,this,[this]{if(!state_->loading){state_->alphaEdited=true;updateEnabledState();}});
    auto addCheck = [&](const QString& field, const QString& text) {
        auto* box = new QCheckBox(text); bind(field, box); form->addRow(box);
        connect(box, &QCheckBox::toggled, this, [this] { if (!state_->loading) updateEnabledState(); });
        return box;
    };
    addCheck("visible", "显示图层");
    s.symmetric = addCheck("symmetric", "关于中纵剖面对称");
    addCheck("developable", "可展开曲面");
    s.hydro = addCheck("use_in_hydrostatics", "参与静水力计算");
    addCheck("use_for_intersections", "生成交线");
    addCheck("show_in_linesplan", "在型线图中显示");
    s.controlNet=new QCheckBox("显示此图层的控制网格");bind("control_net_visible",s.controlNet);form->addRow(s.controlNet);
    connect(s.controlNet,&QCheckBox::toggled,this,[this](bool show){auto& s=*state_;if(s.loading||!s.selected)return;if(show)s.hiddenControl.remove(s.selected);else s.hiddenControl.insert(s.selected);if(onControlNetChanged)onControlNetChanged(s.selected,show);});
    for (const auto& field : {QString("thickness"), QString("material_density")}) {
        auto* number = new NumberEditor;
        number->setDecimals(12); number->setRange(0, 1e9); number->setSingleStep(0.01); number->setKeyboardTracking(false);
        s.numbers.insert(field, {number, 0, 0, false}); bind(field, number);
        form->addRow(field == "thickness" ? "板厚" : "材料密度", number);
        connect(number, &QDoubleSpinBox::valueChanged, this, [this, field](double value) {
            if (state_->loading) return;
            state_->numbers[field].edited = true; state_->numbers[field].draft = value; updateEnabledState();
        });
    }
    connect(s.name, &QLineEdit::textChanged, this, [this] { if (!state_->loading) updateEnabledState(); });
    s.color->onChanged = [this] { if (!state_->loading) updateEnabledState(); };
    connect(s.symmetric, &QCheckBox::toggled, this, [this] {
        if (state_->loading) return;
        state_->symmetricExplicit = true; updateEnabledState();
    });
    connect(s.hydro, &QCheckBox::toggled, this, [this](bool enabled) {
        auto& state = *state_;
        if (state.loading || state.symmetricExplicit) return;
        // The legacy core makes a newly hydrostatic layer symmetric. Keep the
        // draft in step, while a deliberate symmetry choice takes precedence.
        state.loading = true;
        state.symmetric->setChecked(enabled && !state.original["use_in_hydrostatics"].toBool() ? true : state.original["symmetric"].toBool());
        state.loading = false; updateEnabledState();
    });
    connect(s.list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current) {
        auto& state = *state_;
        if (state.loading) return;
        if (state.selected) state.drafts.insert(state.selected, changes());
        state.selected = current ? current->data(Qt::UserRole).toULongLong() : 0;
        loadSelected();
        if(onSelectionChanged)onSelectionChanged();
    });
    connect(s.list,&QListWidget::itemChanged,this,[this](QListWidgetItem* item){auto& s=*state_;if(s.loading)return;const auto id=item->data(Qt::UserRole).toULongLong();
        auto draft=id==s.selected?changes():s.drafts.value(id);draft["visible"]=item->checkState()==Qt::Checked;s.drafts[id]=draft;if(id==s.selected)loadSelected();});
    s.measurements=new QLabel;s.measurements->setWordWrap(true);form->addRow(s.measurements);
    s.error = new QLabel; s.error->setWordWrap(true); s.error->setStyleSheet("color: #b02020;"); s.error->hide(); layout->addWidget(s.error);
    s.apply = new QPushButton("应用"); s.apply->setObjectName("layer_apply");s.apply->setProperty("layer_action","apply"); layout->addWidget(s.apply);
    connect(s.apply, &QPushButton::clicked, this, &LayerPanel::applyChanges);
    s.loading = false; updateEnabledState();
}

LayerPanel::~LayerPanel() = default;
void LayerPanel::setManagementActions(const QList<QAction*>& actions){state_->toolbar->clear();state_->toolbar->addActions(actions);}
void LayerPanel::discardDrafts(){const auto selected=state_->selected;state_->drafts.clear();state_->selected=0;state_->pending=0;setSnapshot(state_->snapshot);setSelectedLayerId(selected);}
QWidget* LayerPanel::fieldEditor(const QString& field) const { return state_->editors.value(field, nullptr); }
fs::EntityId LayerPanel::selectedLayerId() const { return state_->selected; }

void LayerPanel::setSelectedLayerId(fs::EntityId id) {
    auto& s = *state_;
    for (int i = 0; i < s.list->count(); ++i) {
        if (s.list->item(i)->data(Qt::UserRole).toULongLong() == id) { s.list->setCurrentRow(i); return; }
    }
}

void LayerPanel::setSnapshot(std::shared_ptr<const fs::ModelSnapshot> snapshot) {
    auto& s = *state_;
    const bool sameDocument = s.snapshot && snapshot && s.snapshot->documentId == snapshot->documentId;
    if (sameDocument && s.selected) s.drafts.insert(s.selected, changes());
    if (!sameDocument) { s.drafts.clear();s.hiddenControl.clear();s.selected = 0; s.pending = 0; }
    if (sameDocument && s.pending && snapshot->revision > s.pendingRevision) {
        s.drafts.remove(s.pending); s.pending = 0;
    }
    if (sameDocument && snapshot->units != s.snapshot->units) {
        const bool imperial = snapshot->units == "imperial";
        for (auto it = s.drafts.begin(); it != s.drafts.end(); ++it) {
            if (it->contains("thickness")) it->insert("thickness", (*it)["thickness"].toDouble() * (imperial ? 1 / 25.4 : 25.4));
            if (it->contains("material_density")) it->insert("material_density", (*it)["material_density"].toDouble() * (imperial ? DensityFactor : 1 / DensityFactor));
        }
    }
    if(s.snapshot!=snapshot)s.measuredLayer=0;
    s.snapshot = std::move(snapshot);
    s.loading = true; s.list->clear();
    int selectedRow = -1;
    if (s.snapshot) {
        for (const auto& layer : s.snapshot->layers) {
            auto* item = new QListWidgetItem(layer.name, s.list);
            item->setData(Qt::UserRole, QVariant::fromValue(layer.id));
            item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(s.drafts.value(layer.id).value("visible").toBool(layer.visible)?Qt::Checked:Qt::Unchecked);
            QPixmap swatch(14, 14); swatch.fill(displayColor(layer.color)); item->setIcon(QIcon(swatch));
            if (layer.id == s.selected) selectedRow = s.list->count() - 1;
        }
    }
    if (selectedRow < 0 && s.list->count()) selectedRow = 0;
    s.list->setCurrentRow(selectedRow);
    s.selected = selectedRow < 0 ? 0 : s.list->item(selectedRow)->data(Qt::UserRole).toULongLong();
    s.loading = false; loadSelected();
}

void LayerPanel::setEditingEnabled(bool enabled) {
    state_->editingEnabled = enabled;
    // A failed request has no new snapshot; its draft stays available to fix.
    if (enabled) state_->pending = 0;
    updateEnabledState();
}

void LayerPanel::loadSelected() {
    auto& s = *state_;
    const auto* layer = s.snapshot ? s.snapshot->layer(s.selected) : nullptr;
    s.loading = true; s.original = layer ? layerValues(*layer) : QJsonObject{};
    const auto draft = s.drafts.value(s.selected);
    auto value = [&s, &draft](const QString& key) { return draft.contains(key) ? draft[key] : s.original[key]; };
    s.name->setText(s.original["name"].toString()); s.displayedName = s.name->text();
    if (draft.contains("name")) s.name->setText(draft["name"].toString());
    s.color->setProperty("colorValue", quint32(value("color").toDouble()));
    s.alphaEdited=draft.contains("alpha_blend");s.transparency->setValue(qRound((255-value("alpha_blend").toInt(255))*100./255));
    s.controlNet->setChecked(!s.hiddenControl.contains(s.selected));
    s.symmetricExplicit = draft.contains("symmetric");
    for (const auto key : {"visible", "symmetric", "developable", "use_in_hydrostatics", "use_for_intersections", "show_in_linesplan"})
        static_cast<QCheckBox*>(s.editors[key])->setChecked(value(key).toBool());
    if (layer && !s.symmetricExplicit && !layer->useInHydrostatics && s.hydro->isChecked()) s.symmetric->setChecked(true);
    const bool imperial = s.snapshot && s.snapshot->units == "imperial";
    for (auto it = s.numbers.begin(); it != s.numbers.end(); ++it) {
        it->original = s.original[it.key()].toDouble(); it->edited = draft.contains(it.key());
        // Keep the unrounded value even when the editor cannot display it.
        // In particular a unit conversion must never silently clamp a draft.
        it->draft = value(it.key()).toDouble(); it->widget->setValue(it->draft);
        it->widget->setSuffix(it.key() == "thickness" ? (imperial ? " in" : " mm") : (imperial ? " lb/ft³" : " t/m³"));
    }
    s.encoding->setText(s.snapshot ? "文件文字编码：" + s.snapshot->textEncoding : QString());
    s.encoding->setVisible(s.snapshot && !s.snapshot->textEncoding.isEmpty());
    s.error->hide(); s.loading = false; updateEnabledState();
}

QJsonObject LayerPanel::changes() const {
    const auto& s = *state_;
    if (!s.snapshot || !s.snapshot->layer(s.selected)) return {};
    QJsonObject result;
    if (s.name->text() != s.displayedName) result.insert("name", s.name->text());
    const quint32 color = s.color->property("colorValue").toUInt();
    if (color != quint32(s.original["color"].toDouble())) result.insert("color", double(color));
    if(s.alphaEdited){int alpha=qRound(255*(100-s.transparency->value())/100.);if(alpha!=s.original.value("alpha_blend").toInt(255))result["alpha_blend"]=alpha;}
    for (const auto field : {"visible", "developable", "use_in_hydrostatics", "use_for_intersections", "show_in_linesplan"}) {
        const bool value = static_cast<QCheckBox*>(s.editors[field])->isChecked();
        if (value != s.original[field].toBool()) result.insert(field, value);
    }
    const bool automaticSymmetry = !s.original["use_in_hydrostatics"].toBool() && s.hydro->isChecked();
    const bool expectedSymmetry = automaticSymmetry ? true : s.original["symmetric"].toBool();
    if (s.symmetricExplicit && s.symmetric->isChecked() != expectedSymmetry) result.insert("symmetric", s.symmetric->isChecked());
    for (auto it = s.numbers.cbegin(); it != s.numbers.cend(); ++it) {
        if (it->edited && float(it->draft) != float(it->original)) result.insert(it.key(), it->draft);
    }
    return result;
}

QString LayerPanel::validationError() const {
    const auto& s = *state_;
    if (!s.snapshot || !s.snapshot->layer(s.selected)) return "请先选择图层。";
    if (s.name->text() != s.displayedName) {
        if (s.name->text().contains(QChar(0))) return "图层名称中不能包含空字符。";
        if (s.name->text().toUtf8().size() > 65536) return "图层名称过长。";
    }
    if (s.color->property("colorValue").toULongLong() > 0xffffff) return "颜色值无效。";
    for (auto it = s.numbers.cbegin(); it != s.numbers.cend(); ++it) {
        const double value = it->edited ? it->draft : it->original;
        if (!std::isfinite(value) || value < 0 || value > 1e9) return "板厚和材料密度必须是有效的非负数。";
    }
    return {};
}

void LayerPanel::updateEnabledState() {
    auto& s = *state_;
    if (!s.apply) return;
    const bool selected = s.snapshot && s.snapshot->layer(s.selected);
    s.empty->setVisible(!selected); s.list->setVisible(s.snapshot && !s.snapshot->layers.isEmpty());
    s.properties->setEnabled(selected && s.editingEnabled && !s.pending);
    s.list->setEnabled(s.editingEnabled && !s.pending);
    s.apply->setEnabled(selected && s.editingEnabled && !s.pending && !changes().isEmpty());
    if(selected&&s.list->currentItem()){QSignalBlocker block(s.list);s.list->currentItem()->setCheckState(static_cast<QCheckBox*>(s.editors["visible"])->isChecked()?Qt::Checked:Qt::Unchecked);}
    updateMeasurements();
}

void LayerPanel::updateMeasurements(){
    auto& s=*state_;if(!s.measurements)return;
    const auto* layer=s.snapshot?s.snapshot->layer(s.selected):nullptr;
    if(!layer){s.measurements->clear();return;}
    double area=s.measuredArea;fs::Vec3 center=s.measuredCenter;
    if(s.measuredLayer!=s.selected){area=0;center={};
    for(const auto& t:s.snapshot->triangles)if(t.layer==s.selected){const auto& a=t.vertices[0];const auto& b=t.vertices[1];const auto& c=t.vertices[2];
        const fs::Vec3 u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};
        const double n=std::hypot(u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x)*.5;
        area+=n;center.x+=n*(a.x+b.x+c.x)/3;center.y+=n*(a.y+b.y+c.y)/3;center.z+=n*(a.z+b.z+c.z)/3;
    }
    if(area>0){center.x/=area;center.y/=area;center.z/=area;}
    s.measuredLayer=s.selected;s.measuredArea=area;s.measuredCenter=center;}
    const auto edits=changes();auto val=[&](const QString& k){return edits.contains(k)?edits[k]:s.original[k];};
    if(s.symmetric->isChecked()){area*=2;center.y=0;}
    const bool imperial=s.snapshot->units=="imperial";const double weight=area*val("thickness").toDouble()*val("material_density").toDouble()/(imperial?12*2240.:1000.);
    const QString unit=imperial?"ft":"m";
    s.measurements->setText(QString("面积：%1 %2²\n重量：%3 %4\n重心：%5, %6, %7 %2").arg(area,0,'f',3).arg(unit).arg(weight,0,'f',3).arg(imperial?"long ton":"t").arg(center.x,0,'f',3).arg(center.y,0,'f',3).arg(center.z,0,'f',3));
    setProperty("layer_measurements",QJsonObject{{"area",area},{"weight",weight},{"center",QJsonArray{center.x,center.y,center.z}},{"units",unit}});
}

void LayerPanel::applyChanges() {
    auto& s = *state_;
    if (!s.editingEnabled || s.pending || !onLayerChanged) return;
    for (auto it = s.numbers.begin(); it != s.numbers.end(); ++it) it->widget->interpretText();
    const auto error = validationError(); s.error->setText(error); s.error->setVisible(!error.isEmpty());
    if (!error.isEmpty()) return;
    const auto edit = changes();
    if (edit.isEmpty()) return;
    s.drafts.insert(s.selected, edit); s.pending = s.selected; s.pendingRevision = s.snapshot->revision;
    updateEnabledState(); onLayerChanged(s.selected, edit);
}
