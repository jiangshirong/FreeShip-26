// GPL-2.0-or-later. Generated from FreeResistance_KaperDlg.pas.
namespace {
const Array<Array<double,0,13>,0,16> table1a = {{1.00, 1.00, 1.00, 1.00, 1.01, 1.05, 1.05, 1.13, 1.62, 1.61,
    1.55, 1.54, 1.52, 1.52},
    {1.06, 1.06, 1.06, 1.05,
    1.00, 1.03, 1.02, 1.15, 1.56, 1.51, 1.49, 1.47, 1.45, 1.45},
    {1.10, 1.10, 1.10, 1.10,
    1.04, 1.04, 1.03, 1.14, 1.45, 1.41, 1.40, 1.41, 1.41, 1.41},
    {1.15, 1.15, 1.15, 1.16,
    1.06, 1.02, 1.07, 1.07, 1.36, 1.35, 1.36, 1.36, 1.34, 1.34},
    {1.17, 1.17, 1.17, 1.15,
    1.05, 1.00, 1.00, 1.04, 1.25, 1.28, 1.32, 1.30, 1.29, 1.29},
    {1.25, 1.25, 1.25, 1.25,
    1.05, 1.01, 1.01, 1.06, 1.23, 1.22, 1.24, 1.24, 1.23, 1.23},
    {1.23, 1.23, 1.23, 1.23,
    1.00, 1.02, 1.02, 1.01, 1.16, 1.16, 1.19, 1.19, 1.18, 1.18},
    {1.27, 1.27, 1.27, 1.25,
    1.01, 1.02, 1.03, 1.00, 1.00, 1.09, 1.13, 1.13, 1.13, 1.13},
    {1.32, 1.32, 1.32, 1.31,
    1.05, 1.03, 1.10, 1.03, 1.10, 1.07, 1.09, 1.09, 1.09, 1.09},
    {1.35, 1.35, 1.35, 1.33,
    1.05, 1.08, 1.15, 1.06, 1.11, 1.06, 1.07, 1.06, 1.06, 1.06},
    {1.33, 1.33, 1.33, 1.33,
    1.02, 1.06, 1.20, 1.10, 1.11, 1.05, 1.05, 1.04, 1.05, 1.05},
    {1.38, 1.38, 1.38, 1.36,
    1.06, 1.09, 1.30, 1.21, 1.12, 1.03, 1.03, 1.03, 1.04, 1.04},
    {1.40, 1.40, 1.40, 1.34,
    1.08, 1.13, 1.38, 1.28, 1.09, 1.02, 1.00, 1.00, 1.02, 1.02},
    {1.42, 1.42, 1.42, 1.38,
    1.12, 1.18, 1.48, 1.39, 1.19, 1.02, 1.01, 1.03, 1.03, 1.03},
    {1.42, 1.42, 1.42, 1.39,
    1.07, 1.17, 1.55, 1.43, 1.20, 1.00, 1.00, 1.02, 1.03, 1.03},
    {1.53, 1.53, 1.53, 1.51,
    1.18, 1.26, 1.72, 1.54, 1.27, 1.01, 1.00, 1.01, 1.02, 1.02},
    {1.48, 1.48, 1.48, 1.49,
    1.22, 1.27, 1.80, 1.63, 1.32, 1.02, 1.00, 1.00, 1.00, 1.00}};
const Array<Array<double,0,13>,0,16> table1b = {{1.00, 1.00, 1.00, 1.00, 1.00, 1.08, 1.05, 1.20, 1.45, 1.44,
    1.42, 1.43, 1.40, 1.40},
    {1.05, 1.05, 1.05, 1.04,
    1.00, 1.07, 1.01, 1.18, 1.34, 1.38, 1.36, 1.37, 1.32, 1.32},
    {1.09, 1.09, 1.09, 1.08,
    1.01, 1.06, 1.02, 1.11, 1.26, 1.29, 1.30, 1.30, 1.28, 1.28},
    {1.14, 1.14, 1.14, 1.11,
    1.03, 1.03, 1.02, 1.09, 1.17, 1.24, 1.24, 1.25, 1.24, 1.24},
    {1.15, 1.15, 1.15, 1.13,
    1.02, 1.00, 1.00, 1.01, 1.12, 1.19, 1.22, 1.22, 1.20, 1.20},
    {1.21, 1.21, 1.21, 1.17,
    1.01, 1.02, 1.03, 1.01, 1.06, 1.14, 1.16, 1.16, 1.15, 1.15},
    {1.23, 1.23, 1.23, 1.17,
    1.00, 1.05, 1.07, 1.01, 1.01, 1.11, 1.14, 1.13, 1.13, 1.13},
    {1.25, 1.25, 1.25, 1.19,
    1.00, 1.06, 1.12, 1.00, 1.00, 1.08, 1.08, 1.09, 1.10, 1.10},
    {1.29, 1.29, 1.29, 1.21,
    1.01, 1.07, 1.21, 1.07, 1.00, 1.06, 1.07, 1.07, 1.07, 1.07},
    {1.30, 1.30, 1.30, 1.24,
    1.03, 1.13, 1.29, 1.11, 1.01, 1.05, 1.06, 1.06, 1.06, 1.06},
    {1.29, 1.29, 1.29, 1.24,
    1.02, 1.14, 1.35, 1.22, 1.02, 1.02, 1.04, 1.04, 1.05, 1.05},
    {1.32, 1.32, 1.32, 1.27,
    1.02, 1.17, 1.50, 1.31, 1.04, 1.00, 1.03, 1.04, 1.04, 1.04},
    {1.38, 1.38, 1.38, 1.31,
    1.05, 1.24, 1.59, 1.39, 1.07, 1.02, 1.03, 1.03, 1.03, 1.03},
    {1.38, 1.38, 1.38, 1.33,
    1.09, 1.28, 1.74, 1.58, 1.12, 1.02, 1.02, 1.03, 1.02, 1.02},
    {1.36, 1.36, 1.36, 1.33,
    1.05, 1.30, 1.87, 1.64, 1.15, 1.01, 1.01, 1.01, 1.02, 1.02},
    {1.48, 1.48, 1.48, 1.41,
    1.13, 1.39, 2.06, 1.74, 1.20, 1.02, 1.00, 1.01, 1.01, 1.01},
    {1.45, 1.45, 1.45, 1.40,
    1.18, 1.43, 2.17, 1.89, 1.29, 1.04, 1.00, 1.00, 1.00, 1.00}};
const Array<Array<double,0,13>,0,16> table1c = {{1.00, 1.00, 1.00, 1.00, 1.01, 1.10, 1.07, 1.27, 1.43, 1.44,
    1.38, 1.39, 1.41, 1.41},
    {1.06, 1.06, 1.06, 1.01,
    1.00, 1.05, 1.02, 1.20, 1.31, 1.34, 1.34, 1.33, 1.31, 1.31},
    {1.09, 1.09, 1.09, 1.06,
    1.02, 1.05, 1.02, 1.13, 1.23, 1.31, 1.29, 1.28, 1.29, 1.29},
    {1.14, 1.14, 1.14, 1.06,
    1.03, 1.02, 1.00, 1.08, 1.16, 1.25, 1.23, 1.23, 1.23, 1.23},
    {1.18, 1.18, 1.18, 1.09,
    1.01, 1.00, 1.02, 1.00, 1.11, 1.19, 1.20, 1.19, 1.18, 1.18},
    {1.20, 1.20, 1.20, 1.10,
    1.00, 1.01, 1.07, 1.01, 1.06, 1.14, 1.15, 1.15, 1.14, 1.14},
    {1.23, 1.23, 1.23, 1.11,
    1.00, 1.04, 1.14, 1.02, 1.05, 1.12, 1.12, 1.12, 1.12, 1.12},
    {1.23, 1.23, 1.23, 1.15,
    1.00, 1.07, 1.20, 1.04, 1.00, 1.08, 1.08, 1.09, 1.11, 1.11},
    {1.29, 1.29, 1.29, 1.15,
    1.00, 1.11, 1.32, 1.09, 1.01, 1.05, 1.07, 1.07, 1.10, 1.10},
    {1.30, 1.30, 1.30, 1.18,
    1.03, 1.15, 1.41, 1.18, 1.01, 1.03, 1.04, 1.06, 1.08, 1.08},
    {1.26, 1.26, 1.26, 1.19,
    1.00, 1.18, 1.48, 1.29, 1.00, 1.01, 1.04, 1.05, 1.06, 1.06},
    {1.36, 1.36, 1.36, 1.22,
    1.02, 1.21, 1.66, 1.36, 1.06, 1.00, 1.02, 1.04, 1.04, 1.04},
    {1.37, 1.37, 1.37, 1.27,
    1.05, 1.29, 1.79, 1.52, 1.11, 1.02, 1.02, 1.03, 1.03, 1.03},
    {1.40, 1.40, 1.40, 1.28,
    1.09, 1.34, 1.97, 1.65, 1.14, 1.01, 1.00, 1.02, 1.02, 1.02},
    {1.38, 1.38, 1.38, 1.25,
    1.06, 1.38, 2.13, 1.67, 1.20, 1.01, 1.00, 1.01, 1.01, 1.01},
    {1.48, 1.48, 1.48, 1.34,
    1.12, 1.46, 2.34, 1.91, 1.25, 1.03, 1.00, 1.00, 1.01, 1.01},
    {1.47, 1.47, 1.47, 1.32,
    1.17, 1.51, 2.47, 2.08, 1.33, 1.07, 1.00, 1.00, 1.00, 1.00}};
const Array<Array<double,0,14>,0,4> table4 = {{1.00000, 1.00000, 1.00000, 1.0000, 1.0000, 1.0000, 1.0000,
    1.000, 1.0000, 1.0000, 1.0000, 1.0000, 1.000, 1.00, 1.00},
    {1.01590, 1.01590,
    1.01627, 1.0170, 1.0175, 1.0179, 1.0176, 1.017, 1.0155, 1.0126,
    1.0000, 0.9752, 0.950, 0.93, 0.93},
    {1.01775, 1.01775,
    1.01800, 1.0185, 1.0190, 1.0190, 1.0190, 1.019, 1.0180, 1.0150,
    1.0011, 0.9784, 0.950, 0.93, 0.93},
    {1.02750, 1.02750,
    1.02800, 1.0290, 1.0300, 1.0300, 1.0300, 1.030, 1.0270, 1.0230,
    1.0075, 0.9821, 0.965, 0.95, 0.95},
    {1.04562, 1.04562,
    1.04591, 1.0465, 1.0471, 1.0470, 1.0460, 1.044, 1.0380, 1.0310,
    1.0080, 0.9875, 0.970, 0.96, 0.96}};
struct Resistance_kaper final: ResistanceBase {
using ResistanceBase::ResistanceBase;

float fgetlcb();
void fsetlcb(float val);
float fgetle();
void fsetle(float val);
float fgetewl();
void fsetewl(float val);
float fgetbwl();
void fsetbwl(float val);
float fgetat_ax();
void fsetat_ax(float val);
float fgeth();
void fseth(float val);
float fgetws();
void fsetws(float val);
float fgetcp();
void fsetcp(float val);
float fgetdispl();
void fsetdispl(float val);
void checkbox2click(void* sender);
void calculate();
bool execute();
void loadProperty(const QString &member,float value) override {
if (member==QStringLiteral("draft")) { fseth(value); return; }
if (member==QStringLiteral("lwl")) { fsetewl(value); return; }
if (member==QStringLiteral("displacement")) { fsetdispl(value); return; }
if (member==QStringLiteral("wettedsurface")) { fsetws(value); return; }
if (member==QStringLiteral("entranceangle")) { fsetle(value); return; }
if (member==QStringLiteral("at_ax")) { fsetat_ax(value); return; }
if (member==QStringLiteral("bwl")) { fsetbwl(value); return; }
if (member==QStringLiteral("cp")) { fsetcp(value); return; }
if (member==QStringLiteral("displ")) { fsetdispl(value); return; }
if (member==QStringLiteral("ewl")) { fsetewl(value); return; }
if (member==QStringLiteral("h")) { fseth(value); return; }
if (member==QStringLiteral("lcb")) { fsetlcb(value); return; }
if (member==QStringLiteral("ie")) { fsetle(value); return; }
if (member==QStringLiteral("ws")) { fsetws(value); return; }
ResistanceBase::loadProperty(member,value);
}
void booleanChanged(const QString &key) override {  }
void extract() override { field("checkbox2").checked=true;checkbox2click(nullptr);field("checkbox2").checked=false;checkbox2click(nullptr); }
void initialise() override { execute(); }
void choiceChanged(const QString &key) override {  }
void runCalculation() override { calculate(); }
};
float Resistance_kaper::fgetlcb() {
float result{};

    result = field(QStringLiteral("edit9")) . value;
    return result;
}

void Resistance_kaper::fsetlcb(float val) {

    field(QStringLiteral("edit9")).set(val);
}

float Resistance_kaper::fgetle() {
float result{};

    result = field(QStringLiteral("edit8")) . value;
    return result;
}

void Resistance_kaper::fsetle(float val) {

    field(QStringLiteral("edit8")).set(val);
}

float Resistance_kaper::fgetewl() {
float result{};

    result = field(QStringLiteral("edit2")) . value;
    return result;
}

void Resistance_kaper::fsetewl(float val) {

    field(QStringLiteral("edit2")).set(val);
}

float Resistance_kaper::fgetbwl() {
float result{};

    result = field(QStringLiteral("edit3")) . value;
    return result;
}

void Resistance_kaper::fsetbwl(float val) {

    field(QStringLiteral("edit3")).set(val);
}

float Resistance_kaper::fgetat_ax() {
float result{};

    result = field(QStringLiteral("edit10")) . value;
    return result;
}

void Resistance_kaper::fsetat_ax(float val) {

    field(QStringLiteral("edit10")).set(val);
    if ((val < 0.0f) && (val > 0.04))
        ; // presentation only
    else
        ; // presentation only
}

float Resistance_kaper::fgeth() {
float result{};

    result = field(QStringLiteral("edit4")) . value;
    return result;
}

void Resistance_kaper::fseth(float val) {

    field(QStringLiteral("edit4")).set(val);
}

float Resistance_kaper::fgetws() {
float result{};

    result = field(QStringLiteral("edit5")) . value;
    return result;
}

void Resistance_kaper::fsetws(float val) {

    field(QStringLiteral("edit5")).set(val);
}

float Resistance_kaper::fgetcp() {
float result{};

    result = field(QStringLiteral("edit6")) . value;
    return result;
}

void Resistance_kaper::fsetcp(float val) {

    field(QStringLiteral("edit6")).set(val);
    if ((val < 0.48) && (val > 0.64))
        ; // presentation only
    else
        ; // presentation only
}

float Resistance_kaper::fgetdispl() {
float result{};

    result = field(QStringLiteral("edit7")) . value;
    return result;
}

void Resistance_kaper::fsetdispl(float val) {

    field(QStringLiteral("edit7")).set(val);
}

void Resistance_kaper::checkbox2click(void* sender) {
HydroInput hydobject{};
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    if (field(QStringLiteral("checkbox2")) . checked)
        {
            ; // presentation only
            ; // presentation only
            ; // presentation only
            ; // presentation only
            ; // presentation only
            ; // presentation only
            ; // presentation only
        }
    else
        {
            ; // presentation only
            ; // presentation only
            ; // presentation only
            ; // presentation only
            ; // presentation only
            ; // presentation only
            ; // presentation only
        }
    if (field(QStringLiteral("checkbox2")) . checked)
        {
            if (fgeth() == 0.0f)
                fseth(settings. projectdraft);
            ; // local immutable hydrostatic request
            hydobject . draft = fgeth();
            hydobject.data = hydro(hydobject.draft);
            fsetewl(hydValue(hydobject, QStringLiteral("lengthwaterline")));
            fsetbwl(hydValue(hydobject, QStringLiteral("beamwaterline")));
            fsetws(hydValue(hydobject, QStringLiteral("wettedsurface")));
            fsetcp(hydValue(hydobject, QStringLiteral("prismcoefficient")));
            fsetdispl(hydValue(hydobject, QStringLiteral("displacement")));
            if (fgetewl() != 0)
                fsetlcb((hydValue(hydobject, QStringLiteral("wlmax.x")) - hydValue(hydobject, QStringLiteral("centerofbuoyancy.x"))) / fgetewl());
            else
                fsetlcb(0);
            fsetle(hydValue(hydobject, QStringLiteral("waterplaneentranceangle")));
            check();
        }
}

void Resistance_kaper::calculate() {
const double e20 = 0.8;
const double f20 = 3.65;
const double g20 = 0.07;
const double h20 = 1.2;
const double n27 = 0.85;
const double o27 = 0.75f;
int i{};
int j{};
int ind1{};
int ind2{};
float speed{};
float speed_ft{};
Array<Array<float,0,13>,0,16> table2{};
Array<float,0,16> table3{};
Array<float,0,4> table5{};
float tmp{};
float fact{};
float reynoldsnumber{};
float cv{};
float cp_intfactor{};
float c1{};
float c2{};
float c3{};
float c4{};
float c5{};
float c6{};
float displ_longtonnes{};
float cf{};
float cf_p{};
float rresidual{};
float rfrictional{};
float rfrictional_poly{};
float rtotal{};
float rtotal_poly{};
float rspilman{};
bool validdata{};
float effectivelengthwaterline{};
float speedlengthratio{};
float beamwaterline{};
float draft{};
float wetarea{};
float displacement{};
float maxspeed{};
    clearIndex(0);
    clearIndex(1);
    clearIndex(2);
    clearIndex(3);
    if (settings. projectunits == 1)
        yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral(", ") + tr(QStringLiteral("rs_lbs"));
    else
        yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral(", ") + tr(QStringLiteral("rs_n"));
    resultsmemo.clear();
    ; // presentation only
    if (settings. projectunits == 1)
        {
            effectivelengthwaterline = fgetewl();
            beamwaterline = fgetbwl();
            draft = fgeth();
            wetarea = fgetws();
            displacement = fgetdispl() * 2240;
        }
    else
        {
            effectivelengthwaterline = fgetewl() / foot;
            beamwaterline = fgetbwl() / foot;
            draft = fgeth() / foot;
            wetarea = fgetws() / (foot * foot);
            displacement = fgetdispl() * 2240;
        }
    if ((effectivelengthwaterline == 0) || (beamwaterline == 0) || (draft == 0) || (wetarea == 0) || (fgetcp() == 0) || (displacement == 0) || (fgetlcb() <= 0))
        return;
    cv = (displacement / 64) / power (effectivelengthwaterline , 3);
    displ_longtonnes = displacement / 2240;
    maxspeed = 1.4 * power (effectivelengthwaterline , 0.5f);
    for (i=0; i<=16; ++i)
    {
        check();
        {
            for (j=0; j<=13; ++j)
            {
                check();
                {
                    if ((1000 * cv > 1.0f) && (1000 * cv < 1.5f))
                        {
                            table2 [i] [j] = (table1b [i] [j] - table1a [i] [j]) * (1000 * cv - 1) + table1a [i] [j];
                        }
                    else
                        if ((1000 * cv >= 1.5f) && (1000 * cv < 2.0f))
                            table2 [i] [j] = (table1c [i] [j] - table1b [i] [j]) * (1000 * cv - 1) + table1b [i] [j];
                        else
                            if (1000 * cv >= 2.0f)
                                table2 [i] [j] = table1c [i] [j];
                            else
                                table2 [i] [j] = table1a [i] [j];
                }
            }
        }
    }
    header (space (10) + tr(QStringLiteral("rs_kaper_resistance_for_canoes_and_kayaks")) + QStringLiteral(", ") + tr(QStringLiteral("rs_according_to_john_winters")) + QStringLiteral(".") , resultsmemo);
    resultsmemo.append(QStringLiteral("          ----------------------------------------------------------------------------------"));
    resultsmemo.append(QStringLiteral(""));
    resultsmemo.append(space (10) + tr(QStringLiteral("rs_input_variables")));
    resultsmemo.append(QStringLiteral(""));
    resultsmemo.append(space (10) + tr(QStringLiteral("rs_hull")));
    resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_effective_waterline_length")) , 40) + QStringLiteral(" : ") + number (fgetewl() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (settings. projectunits));
    resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_beam_on_waterline")) , 40) + QStringLiteral(" : ") + number (fgetbwl() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (settings. projectunits));
    if (settings. projectunits == 1)
        resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_draft_hull")) , 40) + QStringLiteral(" : ") + number (draft , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (settings. projectunits));
    else
        resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_draft_hull")) , 40) + QStringLiteral(" : ") + number (draft * foot , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (settings. projectunits));
    resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_wetted_surface_area")) , 40) + QStringLiteral(" : ") + number (fgetws() , 0 , 6 , 2) + QString(QChar(32)) + areastr (settings. projectunits));
    resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_prismatic_coefficient")) , 40) + QStringLiteral(" : ") + number (fgetcp() , 0 , 6 , 4));
    resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_displacement")) , 40) + QStringLiteral(" : ") + number (fgetdispl() , 0 , 6 , 3) + QString(QChar(32)) + weightstr (settings. projectunits));
    resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_half_entrance_angle_of_dwl")) , 40) + QStringLiteral(" : ") + number (fgetle() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_degr")));
    resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_longitudinal_center_of_buoyancy")) , 40) + QStringLiteral(" : ") + number (fgetlcb() , 0 , 6 , 3));
    resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_submerged_transom_area_ratio")) , 40) + QStringLiteral(" : ") + number (fgetat_ax() , 0 , 6 , 4));
    resultsmemo.append(QStringLiteral(""));
    resultsmemo.append(QStringLiteral("          ----------------------------------------------------------------------------------"));
    resultsmemo.append(QStringLiteral(""));
    resultsmemo.append(QStringLiteral(""));
    validdata = true;
    if ((fgetcp() < 0.48) || (fgetcp() > 0.64))
        {
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_cp_is_outside_valid_domain")) + QStringLiteral(" of 0.48 .. 0.64"));
            validdata = false;
            ; // presentation only
        }
    if ((fgetat_ax() < 0.0f) || (fgetat_ax() > 0.04))
        {
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_submerged_transom_area_ratio")) + QStringLiteral(" of 0.00 .. 0.04"));
            validdata = false;
            ; // presentation only
        }
    if (! validdata)
        {
            ; // presentation only
            return;
        }
    resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_by")) + QStringLiteral(" KAPER:"));
    resultsmemo.append(QStringLiteral(""));
    resultsmemo.append(space (10) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
    resultsmemo.append(space (10) + tr(QStringLiteral("rs_l_speed_l_speed_l__s_l__l_r_frict_l_r_resid_l_r_total_l_spilman_l")));
    if (settings. projectunits == 1)
        resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn__l__ft_s_l_ratio_l___lbs___l___lbs___l___lbs___l___lbs___l")));
    else
        resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn__l__m_s__l_ratio_l____n____l____n____l____n____l____n____l")));
    resultsmemo.append(space (10) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
    speed = 0.4 * power (effectivelengthwaterline , 0.5f);
    while (speed <= maxspeed) {
        check();
        {
            speed_ft = speed * 1.6889;
            speedlengthratio = speed / power (effectivelengthwaterline , 0.5f);
            c1 = 0.002 * power (beamwaterline / effectivelengthwaterline , 0.5f) * power (4 * speedlengthratio , 4);
            c2 = 0.005 * (sinp (fgetle() * 0.01745)) * power (4 * speedlengthratio , 2);
            if (speedlengthratio < 1.5f)
                c3 = e20 * cosp (f20 * speedlengthratio + g20) + h20;
            else
                c3 = 0.7 * e20 * cosp (f20 * speedlengthratio + g20) + 0.96 * h20;
            ind1 = truncp (speedlengthratio / 0.1) - 4;
            if (ind1 < 0)
                {
                    resultsmemo.append(QStringLiteral("Error: SpeedLengthRatio is ") + number (speedlengthratio , 0 , 6 , 3) + QStringLiteral(", must be greater than 0.4"));
                    ; // presentation only
                    continue;
                }
            if (ind1 >= 13)
                {
                    for (j=0; j<=16; ++j)
                    {
                        check();
                        {
                            table3 [j] = table2 [j] [13];
                        }
                    }
                    ind1 = 12;
                    ind2 = 13;
                    fact = (speedlengthratio - (ind1 + 4) * 0.1) / 0.1;
                    for (j=0; j<=16; ++j)
                    {
                        check();
                        {
                            table3 [j] = table2 [j] [ind1] + fact * (table2 [j] [ind2] - table2 [j] [ind1]);
                        }
                    }
                }
            else
                {
                    ind2 = ind1 + 1;
                    fact = (speedlengthratio - (ind1 + 4) * 0.1) / 0.1;
                    for (j=0; j<=16; ++j)
                    {
                        check();
                        {
                            table3 [j] = table2 [j] [ind1] + fact * (table2 [j] [ind2] - table2 [j] [ind1]);
                        }
                    }
                }
            c4 = 0.0f;
            if ((fgetcp() >= 0.48))
                {
                    ind1 = truncp ((fgetcp() - 0.48) / 0.01);
                    ind2 = ind1 + 1;
                    fact = (fgetcp() - (0.48 + ind1 * 0.01)) / 0.01;
                    c4 = table3 [ind1] + fact * (table3 [ind2] - table3 [ind1]);
                }
            c5 = power (0.5f / fgetlcb() , 0.35);
            ind1 = truncp (speedlengthratio / 0.1) - 3;
            if (ind1 > 13)
                ind1 = 13;
            ind2 = ind1 + 1;
            fact = (speedlengthratio - (ind1 + 3) * 0.1) / 0.1;
            for (j=0; j<=4; ++j)
            {
                check();
                {
                    table5 [j] = table4 [j] [ind1] + fact * (table4 [j] [ind1] - table4 [j] [ind2]);
                }
            }
            ind1 = truncp (fgetat_ax() / 0.01);
            ind2 = ind1 + 1;
            fact = (fgetat_ax() - (ind1 * 0.01)) / 0.01;
            c6 = table5 [ind1] + fact * (table5 [ind2] - table5 [ind1]);
            if ((speedlengthratio > 1.4) && (speedlengthratio < 1.6))
                tmp = n27;
            else
                if ((speedlengthratio >= 1.6) && (speedlengthratio < 3))
                    tmp = o27;
                else
                    tmp = 1;
            rresidual = ((4 * c5 * displ_longtonnes * power (speed , 4) / square (effectivelengthwaterline)) + c1 + c2) * c3 * c4 * c6 * tmp;
            reynoldsnumber = speed_ft * (effectivelengthwaterline / 1.2791) * 1e5f;
            cf = 0.075 / square (logp (reynoldsnumber) - 2);
            cf_p = cf + 0.0001;
            rfrictional = 0.99525 * cf * wetarea * speed_ft * speed_ft;
            rfrictional_poly = cf_p * wetarea * speed_ft * speed_ft;
            rtotal = rresidual + rfrictional;
            rtotal_poly = rresidual + rfrictional_poly;
            rspilman = 21.541 * power (speedlengthratio , 4) - 58.373 * power (speedlengthratio , 3) + 59.124 * power (speedlengthratio , 2) - 25.828 * speedlengthratio + 4.126;
            fact = (0.00871 + 0.053 / (8.8 + effectivelengthwaterline)) * wetarea * power (speed , 1.852) + 0.04;
            rspilman = rspilman + fact;
            if (speed <= maxspeed)
                {
                    if (settings. projectunits == 1)
                        {
                            resultsmemo.append(space (10) + QStringLiteral("| ") + pad (speed , 2 , 5) + QStringLiteral(" | ") + pad (speed * 1.6878099 , 2 , 5) + QStringLiteral(" | ") + pad (speedlengthratio , 3 , 5) + QStringLiteral(" | ") + pad (rfrictional , 3 , 7) + QStringLiteral(" | ") + pad (rresidual , 3 , 7) + QStringLiteral(" | ") + pad (rtotal , 3 , 7) + QStringLiteral(" | ") + pad (rspilman , 3 , 7) + QStringLiteral(" |"));
                            pointIndex(0, speed , rfrictional);
                            pointIndex(1, speed , rresidual);
                            pointIndex(2, speed , rtotal);
                            pointIndex(3, speed , rspilman);
                        }
                    else
                        {
                            resultsmemo.append(space (10) + QStringLiteral("| ") + pad (speed , 2 , 5) + QStringLiteral(" | ") + pad (speed * 1852 / 3600 , 2 , 5) + QStringLiteral(" | ") + pad (speedlengthratio , 3 , 5) + QStringLiteral(" | ") + pad (4.44822 * rfrictional , 3 , 7) + QStringLiteral(" | ") + pad (4.44822 * rresidual , 3 , 7) + QStringLiteral(" | ") + pad (4.44822 * rtotal , 3 , 7) + QStringLiteral(" | ") + pad (4.44822 * rspilman , 3 , 7) + QStringLiteral(" |"));
                            pointIndex(0, speed , 4.44822 * rfrictional);
                            pointIndex(1, speed , 4.44822 * rresidual);
                            pointIndex(2, speed , 4.44822 * rtotal);
                            pointIndex(3, speed , 4.44822 * rspilman);
                        }
                }
            speed = speed + 0.1;
        }
    }
    resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+---------+---------+---------+---------+"));
    ; // presentation only
}

bool Resistance_kaper::execute() {
bool result{};

    ; // presentation only
    yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral(" ,") + tr(QStringLiteral("rs_n"));
    xTitle = tr(QStringLiteral("rs_speed")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kn"));
    field(QStringLiteral("checkbox2")) . checked = false;
    field(QStringLiteral("label9")) . caption = lengthstr (settings. projectunits);
    field(QStringLiteral("label10")) . caption = lengthstr (settings. projectunits);
    field(QStringLiteral("label11")) . caption = lengthstr (settings. projectunits);
    field(QStringLiteral("label7")) . caption = areastr (settings. projectunits);
    field(QStringLiteral("label14")) . caption = weightstr (settings. projectunits);
    field(QStringLiteral("label16")) . caption = tr(QStringLiteral("rs_degr"));
    result = false;
    return result;
}
}
