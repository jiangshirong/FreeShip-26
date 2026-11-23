#include "app/ui_language.h"
#include "project_dialog.h"
#include "studio_ui.h"
#include "core/principal_dimensions.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDynamicPropertyChangeEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

namespace {
constexpr double Foot = 0.3048;
constexpr double DensityFactor = 1000.0 * Foot * Foot * Foot / 0.45359237;
enum class Quantity { None, Length, Density };
const std::array<const char*,5> HydroFields{"hydro_displacement","hydro_lateral_area",
    "hydro_sectional_areas","hydro_metacentric_height","hydro_lcf"};
QJsonObject projectValues(const fs::ModelSnapshot& snapshot){
    QJsonObject values{{"name",snapshot.name},{"designer",snapshot.designer},{"comment",snapshot.comment},{"created_by",snapshot.createdBy},
        {"units",snapshot.units},{"length",snapshot.length},{"beam",snapshot.beam},{"draft",snapshot.draft},{"water_density",snapshot.waterDensity},
        {"appendage_coefficient",snapshot.appendageCoefficient},{"split_section_location",snapshot.splitSectionLocation},
        {"use_default_split_section_location",snapshot.useDefaultSplitSectionLocation},{"simplify_intersections",snapshot.simplifyIntersections},
        {"precision",snapshot.precision},{"save_preview",snapshot.savePreview},{"shade_underwater",snapshot.shadeUnderwater},
        {"underwater_color",double(snapshot.underwaterColor)},{"coefficient_basis",snapshot.coefficientBasis},{"underwater_color_alpha",snapshot.underwaterColorAlpha}};
    for(size_t i=0;i<HydroFields.size();++i)values[HydroFields[i]]=snapshot.hydroFeatures[i];
    values["principal_dimensions"]=snapshot.principalDimensions;
    values["has_perpendiculars"]=snapshot.principalDimensions.contains("aft_perpendicular_m");
    values["has_moulded_depth"]=snapshot.principalDimensions.contains("moulded_depth_m");
    for(const auto* key:{"aft_perpendicular","forward_perpendicular","length_between_perpendiculars","moulded_depth"})
        values[key]=snapshot.principalDimensions[QString(key)+"_m"].toDouble()/(snapshot.units=="imperial"?Foot:1.);
    return values;
}
double factor(Quantity quantity, const QString& units) {
    if (units != "imperial") return 1;
    return quantity == Quantity::Length ? 1 / Foot : quantity == Quantity::Density ? DensityFactor : 1;
}
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
    explicit ColorEditor(QWidget* parent = nullptr) : QPushButton(parent) {
        connect(this, &QPushButton::clicked, this, [this] {
            const auto color = studioChooseColor(displayColor(property("colorValue").toUInt()), this, uiText("选择颜色"), QColorDialog::DontUseNativeDialog);
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
        }
        return QPushButton::event(event);
    }
};
}

struct ProjectDialog::State {
    struct Number {
        NumberEditor* widget = nullptr;
        Quantity quantity = Quantity::None;
        double original = 0, metricDraft = 0;
        bool edited = false;
    };
    QJsonObject original;
    QHash<QString, QWidget*> editors;
    QHash<QString, QString> displayedText;
    QHash<QString, Number> numbers;
    QHash<QString, QWidget*> numberLabels;
    QString initialUnits, units;
    QComboBox *unitBox = nullptr, *precision = nullptr, *coefficientBasis = nullptr;
    QCheckBox *defaultSplit = nullptr, *shade = nullptr;
    QCheckBox *perpendiculars = nullptr, *depth = nullptr;
    ColorEditor* color = nullptr;
    QSpinBox* underwaterOpacity = nullptr;
    StudioMessage* error = nullptr;
    QTabWidget* tabs = nullptr;
    QDialogButtonBox* buttons = nullptr;
    StudioMessage* waiting = nullptr;
    bool editingEnabled = true;
    bool loading = true, splitWasDefault = true, manualSplitInitialized = false;
};

ProjectDialog::ProjectDialog(const fs::ModelSnapshot& snapshot, QWidget* parent)
    : QDialog(parent), state_(std::make_unique<State>()) {
    setWindowTitle(uiText("工程设置"));
    setStudioWindowIcon(this,"ProjectSettings");
    resize(960, 720);
    setMinimumSize(500, 360);

    auto& s = *state_;
    s.initialUnits = s.units = snapshot.units;
    s.original=projectValues(snapshot);
    auto* layout = new QVBoxLayout(this);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    s.buttons=buttons;
    buttons->button(QDialogButtonBox::Ok)->setText(uiText("确定"));
    buttons->button(QDialogButtonBox::Ok)->setIcon(studioIcon("Confirm"));
    buttons->button(QDialogButtonBox::Ok)->setDefault(true);
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText("取消"));
    buttons->button(QDialogButtonBox::Cancel)->setIcon(studioIcon("Cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &ProjectDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* tabs = new QTabWidget;
    s.tabs=tabs;
    layout->addWidget(tabs, 1);
    auto page = [tabs](const QString& title) {
        auto* widget = new QWidget;
        auto* column = new QVBoxLayout(widget);
        column->setContentsMargins(0, 0, 0, 0);
        auto* form = new QFormLayout;
        column->addLayout(form);
        column->addStretch();
        form->setContentsMargins(18, 18, 18, 18);
        form->setVerticalSpacing(14);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        auto* scroll = new QScrollArea;
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setWidget(widget);
        tabs->addTab(scroll, title);
        return form;
    };
    auto bind = [&s](const QString& field, QWidget* editor) {
        editor->setProperty("modelField", field);
        editor->setObjectName("project_" + field);
        s.editors.insert(field, editor);
    };
    auto* general = page(uiText("常规"));
    for (const auto& item : {qMakePair(QString("name"), QString(uiText("工程名称"))), qMakePair(QString("designer"), QString(uiText("设计者"))), qMakePair(QString("created_by"), QString(uiText("文件创建者")))}) {
        auto* editor = new QLineEdit;
        editor->setMaxLength(std::numeric_limits<int>::max()); editor->setText(s.original[item.first].toString());
        bind(item.first, editor); general->addRow(item.second, editor);
        s.displayedText.insert(item.first, editor->text());
    }
    auto* comment = new QPlainTextEdit(snapshot.comment);
    comment->setMinimumHeight(96); comment->setMaximumHeight(150);
    bind("comment", comment); general->addRow(uiText("备注"), comment);
    // Qt normalizes line endings and paragraph separators for display. An
    // untouched text control must not rewrite the original file's text.
    s.displayedText.insert("comment", comment->toPlainText());
    auto* dimensions = page(uiText("主尺度"));
    auto* hydrostatics = page(uiText("静水力"));
    s.unitBox = new QComboBox;
    addUiItem(s.unitBox,"公制", "metric"); addUiItem(s.unitBox,"英制", "imperial");
    s.unitBox->setCurrentIndex(snapshot.units == "imperial" ? 1 : 0);
    bind("units", s.unitBox); general->addRow(uiText("单位"), s.unitBox);
    auto addNumber = [&](QFormLayout* form, const QString& field, const QString& label, Quantity quantity, double minimum) {
        auto* editor = new NumberEditor;
        editor->setRange(minimum, 1e9); editor->setDecimals(12); editor->setSingleStep(0.01); editor->setKeyboardTracking(false);
        const double value = s.original[field].toDouble();
        s.numbers.insert(field, {editor, quantity, value, value / factor(quantity, s.units), false});
        editor->setValue(value); bind(field, editor); form->addRow(label, editor);
        s.numberLabels.insert(field,form->labelForField(editor));
        const bool optionalDimension = field=="aft_perpendicular"||field=="forward_perpendicular"||
            field=="length_between_perpendiculars"||field=="moulded_depth";
        if (optionalDimension || field=="split_section_location")
            editor->setStyleSheet("QDoubleSpinBox:disabled {background-color:#e3e8e6;color:#7b8581;border:1px solid #cbd3cf;border-radius:4px;} QLineEdit:disabled {color:#7b8581;background:transparent;}");
        if (optionalDimension)
            s.numberLabels[field]->setStyleSheet("QLabel:disabled {color:#7b8581;}");
        connect(editor, &QDoubleSpinBox::valueChanged, this, [this, field](double value) {
            auto& state = *state_;
            if (state.loading) return;
            auto& number = state.numbers[field];
            number.edited = true;
            number.metricDraft = value / factor(number.quantity, state.units);
            if (field == "split_section_location") state.manualSplitInitialized = true;
            if (field == "length") updateSplitState();
        });
    };
    addNumber(dimensions, "length", uiText("船长"), Quantity::Length, 0);
    addNumber(dimensions, "beam", uiText("型宽"), Quantity::Length, 0);
    addNumber(dimensions, "draft", uiText("设计吃水"), Quantity::Length, 0);
    s.perpendiculars=new QCheckBox(uiText("设置艏艉垂线"));bind("has_perpendiculars",s.perpendiculars);
    s.perpendiculars->setChecked(s.original["has_perpendiculars"].toBool());dimensions->addRow(s.perpendiculars);
    addNumber(dimensions,"aft_perpendicular",uiText("艉垂线 X"),Quantity::Length,-1e9);
    addNumber(dimensions,"forward_perpendicular",uiText("艏垂线 X"),Quantity::Length,-1e9);
    addNumber(dimensions,"length_between_perpendiculars",uiText("垂线间长 LPP"),Quantity::Length,0);
    s.numbers["length_between_perpendiculars"].widget->setReadOnly(true);
    s.numbers["length_between_perpendiculars"].widget->setButtonSymbols(QAbstractSpinBox::NoButtons);
    s.depth=new QCheckBox(uiText("设置型深"));bind("has_moulded_depth",s.depth);
    s.depth->setChecked(s.original["has_moulded_depth"].toBool());dimensions->addRow(s.depth);
    addNumber(dimensions,"moulded_depth",uiText("型深 D"),Quantity::Length,0);
    for(const auto* key:{"aft_perpendicular","forward_perpendicular"})
        connect(s.numbers[key].widget,&QDoubleSpinBox::valueChanged,this,[this]{updatePrincipalState();});
    connect(s.perpendiculars,&QCheckBox::toggled,this,[this]{updatePrincipalState();});
    connect(s.depth,&QCheckBox::toggled,this,[this]{updatePrincipalState();});
    addNumber(hydrostatics, "water_density", uiText("水密度"), Quantity::Density, 0);
    addNumber(hydrostatics, "appendage_coefficient", uiText("附体系数"), Quantity::None, 0);

    s.precision = new QComboBox;
    addUiItems(s.precision,{"低", "中", "高", "很高"}); s.precision->setCurrentIndex(snapshot.precision);
    bind("precision", s.precision); general->addRow(uiText("细分精度"), s.precision);
    auto addCheck = [&](QFormLayout* form, const QString& field, const QString& label) {
        auto* box = new QCheckBox(label);
        box->setChecked(s.original[field].toBool()); bind(field, box); form->addRow(box); return box;
    };
    s.coefficientBasis=new QComboBox;
    addUiItems(s.coefficientBasis,{"工程设置中的主尺度","实际水下船体尺度"});
    s.coefficientBasis->setCurrentIndex(snapshot.coefficientBasis);
    bind("coefficient_basis",s.coefficientBasis);hydrostatics->addRow(uiText("系数计算依据"),s.coefficientBasis);
    auto* features=new QGroupBox(uiText("显示静水力特征"));
    auto* featureLayout=new QFormLayout(features);
    featureLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    const std::array<QString,5> labels{uiText("排水量和浮心"),uiText("侧投影面积和形心"),uiText("横剖面面积曲线"),uiText("稳心高度"),uiText("纵向漂心")};
    for(size_t i:{size_t(0),size_t(2),size_t(3),size_t(4),size_t(1)})addCheck(featureLayout,HydroFields[i],labels[i]);
    hydrostatics->addRow(features);
    s.defaultSplit = addCheck(dimensions, "use_default_split_section_location", uiText("横剖视图分界取船长中点"));
    s.splitWasDefault = snapshot.useDefaultSplitSectionLocation;
    s.manualSplitInitialized = !snapshot.useDefaultSplitSectionLocation;
    addNumber(dimensions, "split_section_location", uiText("横剖视图分界位置"), Quantity::Length, -1e9);
    auto* alignBaseline=addCheck(dimensions,"align_to_baseline",uiText("确认时将船底移至基线（Z = 0）"));
    auto* baselineNote=new StudioMessage(StudioMessageRole::Explanation,uiText("整体平移船体、参考曲线和剖面，吃水保持不变。含锁定点或约束点时须先解除限制。"));
    baselineNote->setWordWrap(true);baselineNote->setObjectName("BaselineOperationNotice");
    baselineNote->setVisible(alignBaseline->isChecked());
    connect(alignBaseline,&QCheckBox::toggled,baselineNote,&QWidget::setVisible);dimensions->addRow(baselineNote);
    addCheck(general, "simplify_intersections", uiText("简化交线"));
    addCheck(general, "save_preview", uiText("保存预览图片"));
    s.shade = addCheck(general, "shade_underwater", uiText("水下部分着色"));
    s.color = new ColorEditor; s.color->setProperty("colorValue", snapshot.underwaterColor);
    bind("underwater_color", s.color); general->addRow(uiText("水下颜色"), s.color);
    connect(s.shade, &QCheckBox::toggled, s.color, &QWidget::setEnabled);
    s.color->setEnabled(snapshot.shadeUnderwater);
    s.underwaterOpacity=new QSpinBox;
    s.underwaterOpacity->setRange(0,100);s.underwaterOpacity->setSuffix(" %");
    s.underwaterOpacity->setValue(snapshot.underwaterColorAlpha*100/255);
    s.underwaterOpacity->setToolTip(uiText("0% 使用图层颜色，100% 使用水下颜色。船体透明度由图层设置控制。"));
    bind("underwater_opacity_percent",s.underwaterOpacity);general->addRow(uiText("水下不透明度"),s.underwaterOpacity);
    connect(s.shade,&QCheckBox::toggled,s.underwaterOpacity,&QWidget::setEnabled);
    s.underwaterOpacity->setEnabled(snapshot.shadeUnderwater);
    connect(s.unitBox, &QComboBox::currentIndexChanged, this, [this] { switchUnits(state_->unitBox->currentData().toString()); });
    connect(s.defaultSplit, &QCheckBox::toggled, this, &ProjectDialog::updateSplitState);
    s.error = new StudioMessage(StudioMessageRole::Warning);s.error->setObjectName("ProjectWarning");layout->addWidget(s.error);
    s.waiting=new StudioMessage(StudioMessageRole::Feedback,uiText("正在应用上一项修改"));s.waiting->hide();layout->addWidget(s.waiting);
    layout->addWidget(buttons);
    s.loading = false;
    switchUnits(s.units);
}

ProjectDialog::~ProjectDialog() = default;
void ProjectDialog::changeEvent(QEvent* event) {
    QDialog::changeEvent(event);
    if(event->type()!=QEvent::FontChange && event->type()!=QEvent::StyleChange)return;
    // The Windows button-box style can retain its initial width after an
    // inherited font change. Keep the original icons and make room for text.
    if(auto* buttons=findChild<QDialogButtonBox*>())for(auto* button:buttons->buttons()) {
        const int icon=button->icon().isNull()?0:button->iconSize().width()+6;
        button->setMinimumSize(button->fontMetrics().horizontalAdvance(button->text())+icon+20,
            std::max(button->fontMetrics().height(),button->iconSize().height())+10);
    }
}
QWidget* ProjectDialog::fieldEditor(const QString& field) const { return state_->editors.value(field, nullptr); }
void ProjectDialog::setEditingEnabled(bool enabled){
    auto& s=*state_;s.editingEnabled=enabled;s.tabs->setEnabled(enabled);s.buttons->button(QDialogButtonBox::Ok)->setEnabled(enabled);
    s.waiting->setVisible(!enabled);setProperty("project_waiting",!enabled);
    updatePrincipalState();
}
void ProjectDialog::refreshSnapshot(const fs::ModelSnapshot& snapshot){
    auto& s=*state_;s.loading=true;s.original=projectValues(snapshot);s.initialUnits=s.units=snapshot.units;
    for(const auto& key:QStringList{"name","designer","created_by","comment"}){
        auto* editor=s.editors[key];const QSignalBlocker block(editor);const auto value=s.original[key].toString();
        if(auto* line=qobject_cast<QLineEdit*>(editor))line->setText(value);
        else if(auto* text=qobject_cast<QPlainTextEdit*>(editor))text->setPlainText(value);
        s.displayedText[key]=key=="comment"?static_cast<QPlainTextEdit*>(editor)->toPlainText():value;
    }
    for(auto it=s.numbers.begin();it!=s.numbers.end();++it){it->original=s.original[it.key()].toDouble();it->metricDraft=it->original/factor(it->quantity,s.units);it->edited=false;}
    for(auto it=s.editors.begin();it!=s.editors.end();++it)if(auto* box=qobject_cast<QCheckBox*>(it.value())){const QSignalBlocker block(box);box->setChecked(s.original[it.key()].toBool());}
    {const QSignalBlocker block(s.unitBox);s.unitBox->setCurrentIndex(s.unitBox->findData(s.units));}
    {const QSignalBlocker block(s.precision);s.precision->setCurrentIndex(snapshot.precision);}
    {const QSignalBlocker block(s.coefficientBasis);s.coefficientBasis->setCurrentIndex(snapshot.coefficientBasis);}
    s.color->setProperty("colorValue",snapshot.underwaterColor);s.underwaterOpacity->setValue(snapshot.underwaterColorAlpha*100/255);
    s.color->setEnabled(snapshot.shadeUnderwater);s.underwaterOpacity->setEnabled(snapshot.shadeUnderwater);
    s.splitWasDefault=snapshot.useDefaultSplitSectionLocation;s.manualSplitInitialized=!s.splitWasDefault;s.error->hide();
    s.loading=false;switchUnits(s.units);
}

void ProjectDialog::switchUnits(const QString& units) {
    auto& s = *state_;
    if (units != "metric" && units != "imperial") return;
    // A shortcut can change units before a numeric editor loses focus.
    // Commit its text using the old units before replacing the display.
    if (units != s.units)
        for (auto it = s.numbers.begin(); it != s.numbers.end(); ++it) it->widget->interpretText();
    s.loading = true;
    s.units = units;
    for (auto it = s.numbers.begin(); it != s.numbers.end(); ++it) {
        auto& n = it.value();
        double value = n.edited ? n.metricDraft * factor(n.quantity, units) :
            n.original * factor(n.quantity, units) / factor(n.quantity, s.initialUnits);
        // The service stores converted quantities in the legacy Single type.
        // This is a display preview only; untouched values are never submitted.
        if (!n.edited && units != s.initialUnits && it.key()!="aft_perpendicular"&&it.key()!="forward_perpendicular"&&it.key()!="length_between_perpendiculars"&&it.key()!="moulded_depth") value = double(float(value));
        n.widget->setValue(value);
        if (n.quantity == Quantity::Length) n.widget->setSuffix(units == "imperial" ? " ft" : " m");
        else if (n.quantity == Quantity::Density) n.widget->setSuffix(units == "imperial" ? " lb/ft³" : " t/m³");
    }
    s.loading = false;
    updateSplitState();
    updatePrincipalState();
}

void ProjectDialog::updatePrincipalState(){
    auto& s=*state_;if(!s.perpendiculars||!s.depth)return;
    const auto enable=[&](const char* key,bool selected){
        const bool enabled=s.editingEnabled&&selected;
        s.numbers[key].widget->setEnabled(enabled);
        s.numberLabels[key]->setEnabled(enabled);
    };
    for(const auto* key:{"aft_perpendicular","forward_perpendicular","length_between_perpendiculars"})
        enable(key,s.perpendiculars->isChecked());
    enable("moulded_depth",s.depth->isChecked());
    auto* length=s.numbers["length_between_perpendiculars"].widget;const QSignalBlocker block(length);
    length->setValue(std::max(0.,s.numbers["forward_perpendicular"].widget->value()-s.numbers["aft_perpendicular"].widget->value()));
}

QJsonObject ProjectDialog::principalDimensions()const{
    const auto& s=*state_;QJsonObject result;
    const auto metres=[&](const char* key){const auto& n=s.numbers[key];return n.edited?n.metricDraft:n.original/factor(n.quantity,s.initialUnits);};
    if(s.perpendiculars->isChecked()){
        const double aft=metres("aft_perpendicular"),fore=metres("forward_perpendicular");
        result["aft_perpendicular_m"]=aft;result["forward_perpendicular_m"]=fore;result["length_between_perpendiculars_m"]=fore-aft;
    }
    if(s.depth->isChecked())result["moulded_depth_m"]=metres("moulded_depth");
    return result;
}

void ProjectDialog::updateSplitState() {
    auto& s = *state_;
    if (!s.defaultSplit || !s.numbers.contains("split_section_location")) return;
    auto& split = s.numbers["split_section_location"];
    const bool automatic = s.defaultSplit->isChecked();
    const bool priorLoading = s.loading;
    s.loading = true;
    if (automatic) {
        const auto& length = s.numbers["length"];
        const double value = length.edited ? length.metricDraft * factor(length.quantity, s.units) :
            length.original * factor(length.quantity, s.units) / factor(length.quantity, s.initialUnits);
        split.widget->setValue(value / 2);
    } else {
        if (s.splitWasDefault && !s.manualSplitInitialized) {
            split.metricDraft = split.widget->value() / factor(Quantity::Length, s.units);
            s.manualSplitInitialized = true;
        }
        split.widget->setValue(split.metricDraft * factor(Quantity::Length, s.units));
    }
    split.widget->setEnabled(!automatic);
    split.widget->setButtonSymbols(automatic ? QAbstractSpinBox::NoButtons : QAbstractSpinBox::UpDownArrows);
    s.splitWasDefault = automatic;
    s.loading = priorLoading;
}

QJsonObject ProjectDialog::changes() const {
    const auto& s = *state_;
    QJsonObject result;
    for (const auto field : {"name", "designer", "created_by"}) {
        const QString value = static_cast<QLineEdit*>(s.editors[field])->text();
        if (value != s.displayedText[field]) result.insert(field, value);
    }
    const auto comment = static_cast<QPlainTextEdit*>(s.editors["comment"])->toPlainText();
    if (comment != s.displayedText["comment"]) result.insert("comment", comment);
    if (s.units != s.initialUnits) result.insert("units", s.units);
    if(static_cast<QCheckBox*>(s.editors["align_to_baseline"])->isChecked())result["align_to_baseline"]=true;
    for (auto it = s.numbers.cbegin(); it != s.numbers.cend(); ++it) {
        const auto& n = it.value();
        if(it.key()=="aft_perpendicular"||it.key()=="forward_perpendicular"||it.key()=="length_between_perpendiculars"||it.key()=="moulded_depth")continue;
        if (it.key() == "split_section_location" && s.defaultSplit->isChecked()) continue;
        const bool needsManualSplit = it.key() == "split_section_location" &&
            s.original["use_default_split_section_location"].toBool() && !s.defaultSplit->isChecked();
        if (!n.edited && !needsManualSplit) continue;
        const double value = n.metricDraft * factor(n.quantity, s.units);
        const double expected = n.original * factor(n.quantity, s.units) / factor(n.quantity, s.initialUnits);
        if (needsManualSplit || float(value) != float(expected)) result.insert(it.key(), value);
    }
    for (const auto field : {"use_default_split_section_location", "simplify_intersections", "shade_underwater", "save_preview"}) {
        const bool value = static_cast<QCheckBox*>(s.editors[field])->isChecked();
        if (value != s.original[field].toBool()) result.insert(field, value);
    }
    if (s.precision->currentIndex() != s.original["precision"].toInt()) result.insert("precision", s.precision->currentIndex());
    if(s.coefficientBasis->currentIndex()!=s.original["coefficient_basis"].toInt())result["coefficient_basis"]=s.coefficientBasis->currentIndex();
    for(const auto* field:HydroFields){
        const bool value=static_cast<QCheckBox*>(s.editors[field])->isChecked();
        if(value!=s.original[field].toBool())result[field]=value;
    }
    const quint32 color = s.color->property("colorValue").toUInt();
    // Display uses integer percentages, but an untouched value must retain all
    // eight bits of the FBM value (e.g. 234 must not become 232).
    if(s.underwaterOpacity->value()!=s.original["underwater_color_alpha"].toInt()*100/255)
        result["underwater_color_alpha"]=s.underwaterOpacity->value()*255/100;
    if (color != quint32(s.original["underwater_color"].toDouble())) result.insert("underwater_color", double(color));
    const auto principal=principalDimensions();if(principal!=s.original["principal_dimensions"].toObject())result["principal_dimensions"]=principal;
    return result;
}

QString ProjectDialog::validationError() const {
    const auto& s = *state_;
    const auto edits = changes();
    try{fs::validatePrincipalDimensions(principalDimensions());}
    catch(const std::exception& ex){return uiText(QString::fromUtf8(ex.what()));}
    for (const auto field : {"name", "designer", "comment", "created_by"}) {
        if (!edits.contains(field)) continue;
        const QString text = edits[field].toString();
        if (text.contains(QChar(0))) return uiText("文字中不能包含空字符。");
        if (text.toUtf8().size() > 65536) return uiText("文字过长，请缩短名称或备注。");
    }
    for (auto it = s.numbers.cbegin(); it != s.numbers.cend(); ++it) {
        const auto& n = it.value();
        if(it.key()=="aft_perpendicular"||it.key()=="forward_perpendicular"||it.key()=="length_between_perpendiculars"||it.key()=="moulded_depth")continue;
        if (it.key() == "split_section_location" && s.defaultSplit->isChecked()) continue;
        if (!edits.contains(it.key()) && (s.units == s.initialUnits || n.quantity == Quantity::None)) continue;
        const double value = edits.contains(it.key()) ? edits[it.key()].toDouble() :
            n.original * factor(n.quantity, s.units) / factor(n.quantity, s.initialUnits);
        if (!std::isfinite(value) || std::abs(value) > 1e9 || (it.key() != "split_section_location" && value < 0)) return uiText("数值超出可用范围。");
    }
    if (s.color->property("colorValue").toULongLong() > 0xffffff) return uiText("颜色值无效。");
    return {};
}

void ProjectDialog::accept() {
    if(!state_->editingEnabled)return;
    for (auto it = state_->numbers.begin(); it != state_->numbers.end(); ++it) it->widget->interpretText();
    const auto error = validationError();
    state_->error->setText(error); state_->error->setVisible(!error.isEmpty());
    if (error.isEmpty()) QDialog::accept();
}
