// GPL-2.0-or-later. Generated from FreeResistance_PlaningDlg.pas.
namespace {

struct Resistance_planing final: ResistanceBase {
using ResistanceBase::ResistanceBase;
float i3{};
float i4{};
float i5{};
float i6{};
float ie_{};
float cm{};
QString pathfile{};
QString pathfileold{};
QString filetofind{};
QString resultfilename{};
QString fexecdirectory{};
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
float fgetsa();
void fsetsa(float val);
float fgetcaa();
void fsetcaa(float val);
float fgetangle();
void fsetangle(float val);
float fgetk();
void fsetk(float val);
float fgetdp();
void fsetdp(float val);
float fgetbetat();
void fsetbetat(float val);
float fgetbetar();
void fsetbetar(float val);
float fgetcp();
void fsetcp(float val);
float fgetdispl();
void fsetdispl(float val);
void checkbox2click(void* sender);
void checkbox3click(void* sender);
void checkbox4click(void* sender);
void checkbox5click(void* sender);
void checkbox6click(void* sender);
void comboboxclick(void* sender);
void comboboxchange(void* sender);
void selectcalcmethod();
void combobox1click(void* sender);
void selectpropulsortype();
void file_exportdatacp(Span<float> dat);
void combobox1change(void* sender);
void file_exportdatacb(Span<float> dat);
void file_exportdatabu(Span<float> dat);
void file_exportdatacompton(Span<float> dat);
void file_exportdatawolfson(Span<float> dat);
void file_exportdataradojcic(Span<float> dat);
void calculateinworkspace();
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
if (member==QStringLiteral("sa")) { fsetsa(value); return; }
if (member==QStringLiteral("caa")) { fsetcaa(value); return; }
if (member==QStringLiteral("angle")) { fsetangle(value); return; }
if (member==QStringLiteral("k")) { fsetk(value); return; }
if (member==QStringLiteral("dp")) { fsetdp(value); return; }
if (member==QStringLiteral("betat")) { fsetbetat(value); return; }
if (member==QStringLiteral("betar")) { fsetbetar(value); return; }
ResistanceBase::loadProperty(member,value);
}
void booleanChanged(const QString &key) override { if (key==QStringLiteral("checkbox3")) checkbox3click(nullptr);if (key==QStringLiteral("checkbox4")) checkbox4click(nullptr);if (key==QStringLiteral("checkbox5")) checkbox5click(nullptr);if (key==QStringLiteral("checkbox6")) checkbox6click(nullptr); }
void extract() override { field("checkbox2").checked=true;checkbox2click(nullptr);field("checkbox2").checked=false;checkbox2click(nullptr); }
void initialise() override { execute(); }
void choiceChanged(const QString &key) override { if (key==QStringLiteral("combobox")) comboboxclick(nullptr);if (key==QStringLiteral("combobox1")) combobox1click(nullptr); }
void runCalculation() override { calculateinworkspace(); }
};
float Resistance_planing::fgetlcb() {
float result{};

    result = field(QStringLiteral("edit9")) . value;
    return result;
}

void Resistance_planing::fsetlcb(float val) {

    field(QStringLiteral("edit9")).set(val);
    if (field(QStringLiteral("edit9")) . value < 0.1)
        field(QStringLiteral("edit9")).set(1.0f);
}

float Resistance_planing::fgetle() {
float result{};

    result = field(QStringLiteral("edit8")) . value;
    return result;
}

void Resistance_planing::fsetle(float val) {

    field(QStringLiteral("edit8")).set(val);
}

float Resistance_planing::fgetewl() {
float result{};

    result = field(QStringLiteral("edit2")) . value;
    return result;
}

void Resistance_planing::fsetewl(float val) {

    field(QStringLiteral("edit2")).set(val);
}

float Resistance_planing::fgetbwl() {
float result{};

    result = field(QStringLiteral("edit3")) . value;
    return result;
}

void Resistance_planing::fsetbwl(float val) {

    field(QStringLiteral("edit3")).set(val);
}

float Resistance_planing::fgetat_ax() {
float result{};

    result = field(QStringLiteral("edit12")) . value;
    return result;
}

void Resistance_planing::fsetat_ax(float val) {

    field(QStringLiteral("edit12")).set(val);
    if (field(QStringLiteral("edit12")) . value < 1)
        field(QStringLiteral("edit12")).set(14.f);
}

float Resistance_planing::fgeth() {
float result{};

    result = field(QStringLiteral("edit4")) . value;
    return result;
}

void Resistance_planing::fseth(float val) {

    field(QStringLiteral("edit4")).set(val);
}

float Resistance_planing::fgetws() {
float result{};

    result = field(QStringLiteral("edit5")) . value;
    return result;
}

void Resistance_planing::fsetws(float val) {

    field(QStringLiteral("edit5")).set(val);
}

float Resistance_planing::fgetsa() {
float result{};

    result = field(QStringLiteral("edit10")) . value;
    return result;
}

void Resistance_planing::fsetsa(float val) {

    field(QStringLiteral("edit10")).set(val);
}

float Resistance_planing::fgetcaa() {
float result{};

    result = field(QStringLiteral("edit11")) . value;
    return result;
}

void Resistance_planing::fsetcaa(float val) {

    field(QStringLiteral("edit11")).set(val);
    if (field(QStringLiteral("edit11")) . value < 0.2)
        field(QStringLiteral("edit11")).set(0.4);
}

float Resistance_planing::fgetangle() {
float result{};

    result = field(QStringLiteral("edit13")) . value;
    return result;
}

void Resistance_planing::fsetangle(float val) {

    field(QStringLiteral("edit13")).set(val);
    if (field(QStringLiteral("edit13")) . value < 0.1)
        field(QStringLiteral("edit13")).set(4.f);
}

float Resistance_planing::fgetk() {
float result{};

    result = field(QStringLiteral("edit14")) . value;
    return result;
}

void Resistance_planing::fsetk(float val) {

    field(QStringLiteral("edit14")).set(val);
    if (field(QStringLiteral("edit14")) . value < 0.05)
        field(QStringLiteral("edit14")).set(0.15);
}

float Resistance_planing::fgetdp() {
float result{};

    result = field(QStringLiteral("edit15")) . value;
    return result;
}

void Resistance_planing::fsetdp(float val) {

    field(QStringLiteral("edit15")).set(val);
    if (field(QStringLiteral("edit15")) . value < 0.1)
        field(QStringLiteral("edit15")).set(0.1);
}

float Resistance_planing::fgetbetat() {
float result{};

    result = field(QStringLiteral("edit16")) . value;
    return result;
}

void Resistance_planing::fsetbetat(float val) {

    field(QStringLiteral("edit16")).set(val);
    if (field(QStringLiteral("edit16")) . value < 0.1)
        field(QStringLiteral("edit16")).set(0.1);
}

float Resistance_planing::fgetbetar() {
float result{};

    result = field(QStringLiteral("edit17")) . value;
    return result;
}

void Resistance_planing::fsetbetar(float val) {

    field(QStringLiteral("edit17")).set(val);
    if (field(QStringLiteral("edit17")) . value < 0.1)
        field(QStringLiteral("edit17")).set(0.1);
}

float Resistance_planing::fgetcp() {
float result{};

    result = field(QStringLiteral("edit6")) . value;
    return result;
}

void Resistance_planing::fsetcp(float val) {

    field(QStringLiteral("edit6")).set(val);
    if (field(QStringLiteral("edit6")) . value == 0)
        field(QStringLiteral("edit6")).set(0.6);
    if (val > 0.9)
        ; // presentation only
    else
        ; // presentation only
}

float Resistance_planing::fgetdispl() {
float result{};

    result = field(QStringLiteral("edit7")) . value;
    return result;
}

void Resistance_planing::fsetdispl(float val) {

    field(QStringLiteral("edit7")).set(val);
}

void Resistance_planing::checkbox2click(void* sender) {
HydroInput hydobject{};
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
        }
    else
        {
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
            cm = hydValue(hydobject, QStringLiteral("midshipcoeff"));
            fsetdispl(hydValue(hydobject, QStringLiteral("displacement")));
            if (fgetewl() != 0)
                fsetlcb(hydValue(hydobject, QStringLiteral("centerofbuoyancy.x")));
            else
                fsetlcb(0);
            ie_ = hydValue(hydobject, QStringLiteral("waterplaneentranceangle"));
            check();
            if (settings. projectunits != 1)
                {
                    fsetk(fgetk());
                }
            else
                {
                    fsetk(fgetk() / foot);
                }
        }
}

void Resistance_planing::checkbox3click(void* sender) {
HydroInput hydobject{};
    if (field(QStringLiteral("checkbox3")) . checked)
        i3 = 1;
    else
        i3 = 0;
}

void Resistance_planing::checkbox4click(void* sender) {
HydroInput hydobject{};
    if (field(QStringLiteral("checkbox4")) . checked)
        i4 = 1;
    else
        i4 = 0;
}

void Resistance_planing::checkbox5click(void* sender) {
HydroInput hydobject{};
    if (field(QStringLiteral("checkbox5")) . checked)
        i5 = 1;
    else
        i5 = 0;
}

void Resistance_planing::checkbox6click(void* sender) {
HydroInput hydobject{};
    if (field(QStringLiteral("checkbox6")) . checked)
        i6 = 1;
    else
        i6 = 0;
}

void Resistance_planing::comboboxclick(void* sender) {

    selectcalcmethod();
}

void Resistance_planing::comboboxchange(void* sender) {

    selectcalcmethod();
}

void Resistance_planing::selectcalcmethod() {

    if (field(QStringLiteral("combobox")) . itemindex == 0)
        {
            ; // presentation only
            field(QStringLiteral("checkbox3")) . enabled = true;
            field(QStringLiteral("checkbox4")) . enabled = true;
            field(QStringLiteral("checkbox5")) . enabled = true;
            field(QStringLiteral("checkbox6")) . enabled = false;
            field(QStringLiteral("edit8")) . enabled = true;
            field(QStringLiteral("edit9")) . enabled = true;
            field(QStringLiteral("edit10")) . enabled = true;
            field(QStringLiteral("edit13")) . enabled = false;
            field(QStringLiteral("edit14")) . enabled = false;
            field(QStringLiteral("edit15")) . enabled = false;
            field(QStringLiteral("edit16")) . enabled = false;
            field(QStringLiteral("edit17")) . enabled = false;
        }
    if (field(QStringLiteral("combobox")) . itemindex == 1)
        {
            field(QStringLiteral("label23")) . caption = tr(QStringLiteral("rs_angle_between_thrust_and_keel_lines"));
            field(QStringLiteral("label24")) . caption = tr(QStringLiteral("rs_distance_between_thrust_line_and_cog"));
            if (settings. projectunits != 1)
                {
                    field(QStringLiteral("label23_")) . caption = tr(QStringLiteral("rs_degr"));
                    field(QStringLiteral("label24_")) . caption = tr(QStringLiteral("rs_m"));
                    field(QStringLiteral("label19_")) . caption = tr(QStringLiteral("rs_m_2"));
                }
            else
                {
                    field(QStringLiteral("label23_")) . caption = tr(QStringLiteral("rs_degr"));
                    field(QStringLiteral("label24_")) . caption = tr(QStringLiteral("rs_ft"));
                    field(QStringLiteral("label19_")) . caption = tr(QStringLiteral("rs_ft_2"));
                }
            ; // presentation only
            field(QStringLiteral("checkbox3")) . enabled = false;
            field(QStringLiteral("checkbox4")) . enabled = false;
            field(QStringLiteral("checkbox5")) . enabled = false;
            field(QStringLiteral("checkbox6")) . enabled = false;
            field(QStringLiteral("edit8")) . enabled = true;
            field(QStringLiteral("edit9")) . enabled = true;
            field(QStringLiteral("edit10")) . enabled = true;
            field(QStringLiteral("edit13")) . enabled = true;
            field(QStringLiteral("edit14")) . enabled = true;
            field(QStringLiteral("edit15")) . enabled = false;
            field(QStringLiteral("edit16")) . enabled = false;
            field(QStringLiteral("edit17")) . enabled = false;
        }
    if (field(QStringLiteral("combobox")) . itemindex == 2)
        {
            field(QStringLiteral("label23")) . caption = tr(QStringLiteral("rs_midship_area"));
            field(QStringLiteral("label24")) . caption = tr(QStringLiteral("rs_transom_area"));
            if (settings. projectunits != 1)
                {
                    field(QStringLiteral("label23_")) . caption = tr(QStringLiteral("rs_m_2"));
                    field(QStringLiteral("label24_")) . caption = tr(QStringLiteral("rs_m_2"));
                    field(QStringLiteral("label19_")) . caption = tr(QStringLiteral("rs_m_2"));
                }
            else
                {
                    field(QStringLiteral("label23_")) . caption = tr(QStringLiteral("rs_ft_2"));
                    field(QStringLiteral("label24_")) . caption = tr(QStringLiteral("rs_ft_2"));
                    field(QStringLiteral("label19_")) . caption = tr(QStringLiteral("rs_ft_2"));
                }
            ; // presentation only
            field(QStringLiteral("checkbox3")) . enabled = false;
            field(QStringLiteral("checkbox4")) . enabled = false;
            field(QStringLiteral("checkbox5")) . enabled = false;
            field(QStringLiteral("checkbox6")) . enabled = false;
            field(QStringLiteral("edit8")) . enabled = false;
            field(QStringLiteral("edit9")) . enabled = false;
            field(QStringLiteral("edit10")) . enabled = false;
            field(QStringLiteral("edit13")) . enabled = true;
            field(QStringLiteral("edit14")) . enabled = true;
            field(QStringLiteral("edit15")) . enabled = false;
            field(QStringLiteral("edit16")) . enabled = false;
            field(QStringLiteral("edit17")) . enabled = false;
        }
    if ((field(QStringLiteral("combobox")) . itemindex > 2) && (field(QStringLiteral("combobox")) . itemindex < 5))
        {
            ; // presentation only
            field(QStringLiteral("checkbox3")) . enabled = false;
            field(QStringLiteral("checkbox4")) . enabled = false;
            field(QStringLiteral("checkbox5")) . enabled = false;
            field(QStringLiteral("checkbox6")) . enabled = false;
            field(QStringLiteral("edit8")) . enabled = true;
            field(QStringLiteral("edit9")) . enabled = true;
            field(QStringLiteral("edit10")) . enabled = true;
            field(QStringLiteral("edit13")) . enabled = false;
            field(QStringLiteral("edit14")) . enabled = false;
            field(QStringLiteral("edit15")) . enabled = false;
            field(QStringLiteral("edit16")) . enabled = false;
            field(QStringLiteral("edit17")) . enabled = false;
        }
    if (field(QStringLiteral("combobox")) . itemindex == 5)
        {
            ; // presentation only
            field(QStringLiteral("checkbox3")) . enabled = false;
            field(QStringLiteral("checkbox4")) . enabled = false;
            field(QStringLiteral("checkbox5")) . enabled = false;
            field(QStringLiteral("checkbox6")) . enabled = false;
            field(QStringLiteral("edit8")) . enabled = true;
            field(QStringLiteral("edit9")) . enabled = true;
            field(QStringLiteral("edit10")) . enabled = true;
            field(QStringLiteral("edit13")) . enabled = false;
            field(QStringLiteral("edit14")) . enabled = false;
            field(QStringLiteral("edit15")) . enabled = false;
            field(QStringLiteral("edit16")) . enabled = true;
            field(QStringLiteral("edit17")) . enabled = true;
        }
    if (field(QStringLiteral("combobox")) . itemindex > 5)
        {
            ; // presentation only
            field(QStringLiteral("checkbox3")) . enabled = false;
            field(QStringLiteral("checkbox4")) . enabled = false;
            field(QStringLiteral("checkbox5")) . enabled = false;
            field(QStringLiteral("checkbox6")) . enabled = true;
            field(QStringLiteral("edit8")) . enabled = true;
            field(QStringLiteral("edit9")) . enabled = true;
            field(QStringLiteral("edit10")) . enabled = true;
            field(QStringLiteral("edit13")) . enabled = false;
            field(QStringLiteral("edit14")) . enabled = false;
            field(QStringLiteral("edit15")) . enabled = false;
            field(QStringLiteral("edit16")) . enabled = false;
            field(QStringLiteral("edit17")) . enabled = false;
        }
}

void Resistance_planing::combobox1click(void* sender) {

    selectpropulsortype();
}

void Resistance_planing::selectpropulsortype() {

    if (field(QStringLiteral("combobox1")) . itemindex == 0)
        {
            field(QStringLiteral("edit15")) . enabled = false;
        }
    if (field(QStringLiteral("combobox1")) . itemindex == 1)
        {
            field(QStringLiteral("edit15")) . enabled = true;
        }
    if (field(QStringLiteral("combobox1")) . itemindex == 2)
        {
            field(QStringLiteral("edit15")) . enabled = true;
        }
    if (field(QStringLiteral("combobox1")) . itemindex > 2)
        {
            warning(tr(QStringLiteral("rslocfeatureunavailable")));
        }
}

void Resistance_planing::file_exportdatacp(Span<float> dat) {
int i{};
TextFile ffile{};
    if (exists (QStringLiteral("clempoup.dat")))
        check();
    ffile.name = safePath(QStringLiteral("clempoup.dat"));
    newFile(ffile);
    for (i=0; i<=9; ++i)
    {
        check();
        {
            writeLine(ffile, dat [i]);
        }
    }
    closeFile(ffile);
}

void Resistance_planing::combobox1change(void* sender) {

    selectpropulsortype();
}

void Resistance_planing::file_exportdatacb(Span<float> dat) {
int i{};
TextFile ffile{};
    if (exists (QStringLiteral("clemblou.dat")))
        check();
    ffile.name = safePath(QStringLiteral("clemblou.dat"));
    newFile(ffile);
    for (i=0; i<=9; ++i)
    {
        check();
        {
            writeLine(ffile, dat [i]);
        }
    }
    closeFile(ffile);
}

void Resistance_planing::file_exportdatabu(Span<float> dat) {
int i{};
TextFile ffile{};
    if (exists (QStringLiteral("bunkdata.dat")))
        check();
    ffile.name = safePath(QStringLiteral("bunkdata.dat"));
    newFile(ffile);
    for (i=0; i<=9; ++i)
    {
        check();
        {
            writeLine(ffile, dat [i]);
        }
    }
    closeFile(ffile);
}

void Resistance_planing::file_exportdatacompton(Span<float> dat) {
int i{};
TextFile ffile{};
    if (exists (QStringLiteral("cmptdata.dat")))
        check();
    ffile.name = safePath(QStringLiteral("cmptdata.dat"));
    newFile(ffile);
    for (i=0; i<=9; ++i)
    {
        check();
        {
            writeLine(ffile, dat [i]);
        }
    }
    closeFile(ffile);
}

void Resistance_planing::file_exportdatawolfson(Span<float> dat) {
int i{};
TextFile ffile{};
    if (exists (QStringLiteral("wolfdata.dat")))
        check();
    ffile.name = safePath(QStringLiteral("wolfdata.dat"));
    newFile(ffile);
    for (i=0; i<=9; ++i)
    {
        check();
        {
            writeLine(ffile, dat [i]);
        }
    }
    closeFile(ffile);
}

void Resistance_planing::file_exportdataradojcic(Span<float> dat) {
int i{};
TextFile ffile{};
    if (exists (QStringLiteral("radodata.dat")))
        check();
    ffile.name = safePath(QStringLiteral("radodata.dat"));
    newFile(ffile);
    for (i=0; i<=9; ++i)
    {
        check();
        {
            writeLine(ffile, dat [i]);
        }
    }
    closeFile(ffile);
}

void Resistance_planing::calculateinworkspace() {
QString processcallerdirectory{};
int processcallercursor{};
int i{};
int j{};
int n1{};
int n2{};
int n3{};
int n4{};
int ii{};
int ki{};
int nwj{};
int n{};
int imax{};
float mdelta{};
float lambda{};
float nu{};
float mdeltaopt{};
float mdelta_{};
float dv{};
float vo{};
float vk{};
float frb{};
float v_{};
float ro{};
float v{};
float g{};
float vms{};
float b{};
float xg{};
float x{};
float x2{};
float x3{};
float x4{};
float x5{};
float beta{};
float beta0{};
float cb{};
float cb_a{};
float alfa{};
float alfab{};
float omega{};
float alcm{};
float re{};
float cf0{};
float dcf0{};
float rf{};
float rd{};
float rt{};
float pe{};
float y{};
float ak{};
float akm{};
float eps{};
float a1{};
float a2{};
float cb_a_{};
float rap{};
float kap{};
float frv_{};
float raa{};
float k_red{};
float rtt_{};
float be{};
float fd{};
float z{};
float u{};
float w{};
float at{};
float axx{};
float fnv{};
float rn{};
float rn_{};
float cf0_{};
float n9{};
float n10{};
float n12{};
float n13{};
float n14{};
float n15{};
float nser{};
float np{};
float wt{};
float t0{};
float nr{};
float speed{};
float rt_max{};
float temper{};
int units{};
TextFile ffile{};
bool validdata{};
float effectivelengthwaterline{};
float speedlengthratio{};
float beamwaterline{};
float draft{};
float wetarea{};
float displacement{};
float maxspeed{};
float ke{};
float fr_v{};
float vfoot{};
float cv{};
float clbeta{};
float slr{};
float g_imp{};
float ro_imp{};
float clo{};
float clon{};
float dvs{};
float tau{};
float vm_v{};
float dtau{};
float dtau_f{};
float v_l{};
float f{};
float epsilon{};
float bwl_imp{};
float nu_imp{};
float ct{};
float cpw{};
float ne{};
float etaw{};
float iee{};
float roi{};
float bwli{};
float frvr{};
float l_opt{};
float cg_opt{};
float be_opt{};
float cg{};
float dp{};
float vsr{};
float frl{};
float xg_opt{};
float lk{};
float lc{};
float ld{};
float be2{};
Array<float,1,15> dat{};
Array<float,1,11> rtt{};
Array<float,1,11> fnv_{};
Array<float,1,10> ax{};
Array<float,1,10> bx{};
Array<float,1,10> cba{};
Array<float,1,10> lamb{};
Array<float,1,50> tau_{};
Array<float,1,50> m12{};
Array<float,1,16> resvs{};
Array<float,1,16> resfd{};
Array<float,1,16> resbe{};
Array<float,1,16> resrf{};
Array<float,1,16> resrd{};
Array<float,1,16> resrt{};
Array<float,1,16> resvs_{};
Array<float,1,16> resfd_{};
Array<float,1,16> resbe_{};
Array<float,1,16> resrf_{};
Array<float,1,16> resrd_{};
Array<float,1,16> resrt_{};
Array<float,1,16> resbe2{};
QString finitdirectory{};
QString tmpdir{};
QString outputstr{};
QString errorstr{};
QString execfullname{};
int exitstatus{};
QString unt{};
const Array<float,1,10> frbc = {2.3, 2.9, 3.5, 4.0, 4.5, 5.5, 6.0, 7.0, 9.2, 111};
const Array<float,1,10> frbl = {2.3, 2.9, 3.5, 4.0, 4.5, 5.5, 6.0, 11.0, 24, 30};
const Array<float,1,7> frv = {0.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0};
const Array<float,1,7> k_ap = {0.015, 0.03, 0.04, 0.07, 0.11, 0.15, 0.19};
const Array<Array<double,0,13>,0,10> aa = {
    {0.06473, -0.48680, -0.01030, -0.06490, 0.0, 0.10628, 0.97310,
    -0.00272, 0.01089, 0.0, -1.40962, 0.29136, 0.02971, -0.00150},
    {0.10776, -0.88787, -0.01634, -0.13444, 0.0, 0.18186, 1.83080,
    -0.00389, 0.01467, 0.0, -2.46696, 0.47305, 0.05877, -0.00356},
    {0.09483, -0.63720, -0.01540, -0.13580, -0.16046, 0.16803, 1.55972,
    -0.00309, 0.03481, 0.0, -2.15556, 1.02992, 0.05198, -0.00303},
    {0.03475, 0.0, -0.00978, -0.05097, -0.21880, 0.10434, 0.43510,
    -0.00198, 0.04113, 0.0, -0.92663, 1.06392, 0.02209, -0.00105},
    {0.03031, 0.0, -0.00664, -0.05540, -0.19359, 0.09612, 0.51820,
    -0.00215, 0.03901, 0.0, -0.95276, 0.97757, 0.02413, -0.00140},
    {0.03163, 0.0, 0.0, -0.10543, -0.20540, 0.06007, 0.58230,
    -0.00372, 0.04794, 0.08317, -0.70895, 1.19737, 0.0, 0.0},

    {0.03194, 0.0, 0.0, -0.08599, -0.19442, 0.06191, 0.52049, -0.00360,
    0.04436, 0.07366, -0.72057, 1.18119, 0.0, 0.0},
    {0.04343, 0.0, 0.0, -0.13289, -0.18062, 0.05487, 0.78195, -0.00332,
    0.04187, 0.12147, -0.95929, 1.01562, 0.0, 0.0},
    {0.05036, 0.0, 0.0, -0.15597, -0.17813, 0.05099, 0.92859, -0.00308,
    0.04111, 0.14928, -1.12178, 0.93144, 0.0, 0.0},
    {0.05612, 0.0, 0.0, -0.18661, -0.18288, 0.04744, 1.18569, -0.00244,
    0.04124, 0.18090, -1.38644, 0.78414, 0.0, 0.0},
    {0.05967, 0.0, 0.0, -0.19758, 0.20152, 0.04645, 1.30026, -0.00212,
    0.04343, 0.19769, -1.55127, 0.78282, 0.0, 0.0}};
const Array<float,1,10> frvl = {2.4965398, 2.5865052, 2.7422145, 2.9844291, 3.3269896, 3.5276817, 3.8356401,
    4.0570934, 4.2854671, 4.482699};
const Array<float,1,10> lopt = {7.982558, 7.005814, 6.0116279, 5.3313953, 5.02,
    5.0872093, 5.4883721, 5.994186, 6.7616279, 7.6162791};
const Array<float,1,8> frvcg = {2.6749571, 2.8670669, 3.0660377, 3.2855918, 3.5017153, 3.7830189, 4.0643225, 4.4931389};
const Array<float,1,8> cgopt = {0.4612766, 0.4987234, 0.5429787, 0.6008511, 0.653617, 0.7012766, 0.7319149, 0.7591489};
const Array<float,1,7> frvb = {2.4304029, 2.7380952, 2.9981685, 3.4340659, 3.7857143, 4.2142857, 4.496337};
const Array<float,1,7> betao = {16.46789, 15.844037, 15.366973, 14.724771, 14.266055, 13.807339, 13.605505};
const Array<float,1,18> frvxg = {2.4963031, 2.6182994, 2.75878,
    2.8807763, 2.9953789, 3.0988909, 3.1617375, 3.2837338, 3.3872458, 3.512939,
    3.642329, 3.7421442, 3.8715342, 4.0157116, 4.1414048, 4.2707948, 4.3780037, 4.4963031};
const Array<float,1,18> xgopt = {0.3825926, 0.3903704, 0.4003704, 0.4088889, 0.41,
    0.4007407, 0.3896296, 0.3822222, 0.3825926, 0.3874074, 0.3948148,
    0.4003704, 0.4037037, 0.4007407, 0.3918519, 0.3792593, 0.3707407, 0.3640741};
    clearIndex(0);
    clearIndex(1);
    clearIndex(2);
    clearIndex(3);
    clearIndex(4);
    if (settings. projectunits == 1)
        yTitle = tr(QStringLiteral("rs_resistanceo0_1__lbf___power__ehp_"));
    else
        yTitle = tr(QStringLiteral("rs_resistanceo10__kn___power__kw_"));
    resultsmemo.clear();
    ; // presentation only
    units = settings. projectunits;
    temper = settings. projectwatertemper;
    ro = settings. projectwaterdensity * 1000.f;
    nu = findwaterviscosity (temper , units) / 1000000.f;
    g = 9.80665;
    if (fgetlcb() < 0)
        fsetlcb(1);
    if (settings. projectunits == 1)
        {
            effectivelengthwaterline = fgetewl() * foot;
            beamwaterline = fgetbwl() * foot;
            draft = fgeth() * foot;
            xg = fgetlcb() * foot;
            wetarea = fgetws() * foot * foot;
            displacement = fgetdispl() * 1000;
            ke = fgetk();
            ro = ro / 63.4297;
        }
    else
        {
            effectivelengthwaterline = fgetewl();
            beamwaterline = fgetbwl();
            xg = fgetlcb();
            draft = fgeth();
            wetarea = fgetws();
            ke = fgetk() / foot;
            displacement = fgetdispl() * 1000.f;
        }
    if ((effectivelengthwaterline == 0) || (beamwaterline == 0) || (draft <= 0) || (displacement <= 0) || (fgetlcb() <= 0))
        return;
    maxspeed = fgetat_ax();
    resultsmemo.append(QStringLiteral(""));
    resultsmemo.append(QStringLiteral(""));
    resultsmemo.append(QStringLiteral(""));
    resultsmemo.append(QStringLiteral(""));
    if ((field(QStringLiteral("combobox")) . itemindex == 0) && (fgetle() > 0))
        header (space (10) + tr(QStringLiteral("rs_calculations_of_resistance_and_power_for_planing_ships_by_lyubomirov_i_p__formulaes")) , resultsmemo);
    if ((field(QStringLiteral("combobox")) . itemindex == 0) && (fgetle() == 0))
        header (space (10) + tr(QStringLiteral("rs_calculations_of_resistance_and_power_for_planing_ships_by_sedov_l_i__method")) , resultsmemo);
    if (field(QStringLiteral("combobox")) . itemindex == 1)
        header (space (10) + tr(QStringLiteral("rs_resistance_and_power_planing_ships_by_d_savitsky_s_method__1964_")) , resultsmemo);
    if (field(QStringLiteral("combobox")) . itemindex == 2)
        header (space (10) + tr(QStringLiteral("rs_program_of_resistance_prediction_planing_and_pre_planing_ships")) , resultsmemo);
    if (field(QStringLiteral("combobox")) . itemindex == 3)
        header (space (10) + tr(QStringLiteral("rs_resistance_and_power_planing_ships_by_clement_poup_method")) , resultsmemo);
    if (field(QStringLiteral("combobox")) . itemindex == 4)
        header (space (10) + tr(QStringLiteral("rs_resistance_and_power_planing_ships_by_clement_blount_method")) , resultsmemo);
    if (field(QStringLiteral("combobox")) . itemindex == 5)
        header (space (10) + tr(QStringLiteral("rs_resistance_and_power_pre_planing_ships_by_m_bunkov_method")) , resultsmemo);
    if (field(QStringLiteral("combobox")) . itemindex == 6)
        header (space (10) + tr(QStringLiteral("rs_resistance_and_power_planing_ships_by_compton_s_method")) , resultsmemo);
    if (field(QStringLiteral("combobox")) . itemindex == 7)
        header (space (10) + tr(QStringLiteral("rs_resistance_and_power_planing_ships_by_robinson_s_method__wolfson_1999_")) , resultsmemo);
    if (field(QStringLiteral("combobox")) . itemindex == 8)
        header (space (10) + tr(QStringLiteral("rs_resistance_and_power_planing_ships_by_radojcic_method")) , resultsmemo);
    resultsmemo.append(space (10) + QStringLiteral("----------------------------------------------------------------------------------"));
    resultsmemo.append(QStringLiteral(""));
    resultsmemo.append(space (10) + tr(QStringLiteral("rs_input_variables")));
    resultsmemo.append(QStringLiteral(""));
    resultsmemo.append(space (10) + tr(QStringLiteral("rs_hull")));
    if (settings. projectunits != 1)
        {
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_effective_waterline_length")) , 36) + QStringLiteral(" : ") + number (fgetewl() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_m")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_beam_on_waterline")) , 36) + QStringLiteral(" : ") + number (fgetbwl() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_m")));
            if (fgeth() > 0)
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_draft_hull")) , 36) + QStringLiteral(" : ") + number (fgeth() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_m")));
            if (fgetws() > 0)
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_wetted_surface_area")) , 36) + QStringLiteral(" : ") + number (fgetws() , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_m_2")));
            if (fgetcp() > 0)
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_prismatic_coefficient")) , 36) + QStringLiteral(" : ") + number (fgetcp() , 0 , 6 , 4));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_displacement")) , 36) + QStringLiteral(" : ") + number (displacement / 1000.f , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_t")));
            if (field(QStringLiteral("combobox")) . itemindex != 4)
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_deadrise_amidships")) , 36) + QStringLiteral(" : ") + number (fgetle() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_degr")));
            else
                {
                    iee = 12.5f;
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_deadrise_amidships")) , 36) + QStringLiteral(" : ") + number (iee , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_degr")));
                }
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_x_of_cog")) , 36) + QStringLiteral(" : ") + number (fgetlcb() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_m")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_midship_area_above_waterline")) , 36) + QStringLiteral(" : ") + number (fgetsa() , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_m_2")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_roughness_coefficient_o10_3")) , 36) + QStringLiteral(" : ") + number (fgetcaa() , 0 , 6 , 3));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_maximal_speed")) , 36) + QStringLiteral(" : ") + number (fgetat_ax() , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_knots")));
            if (field(QStringLiteral("combobox")) . itemindex == 1)
                {
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_angle_between_thrust_and_keel_lines")) , 36) + QStringLiteral(" : ") + number (fgetangle() , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_degr")));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_distance_between_thrust_line_and_cog")) , 36) + QStringLiteral(" : ") + number (fgetk() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_m")));
                }
            if (field(QStringLiteral("combobox")) . itemindex == 2)
                {
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_midship_area")) , 36) + QStringLiteral(" : ") + number (fgetangle() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_m_2")));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_transom_area")) , 36) + QStringLiteral(" : ") + number (fgetk() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_m_2")));
                }
        }
    else
        {
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_effective_waterline_length")) , 36) + QStringLiteral(" : ") + number (fgetewl() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_ft")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_beam_on_waterline")) , 36) + QStringLiteral(" : ") + number (fgetbwl() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_ft")));
            if (fgeth() > 0)
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_draft_hull")) , 36) + QStringLiteral(" : ") + number (fgeth() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_ft")));
            if (fgetws() > 0)
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_wetted_surface_area")) , 36) + QStringLiteral(" : ") + number (fgetws() , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_ft")) + QStringLiteral("^2"));
            if (fgetcp() > 0)
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_prismatic_coefficient")) , 36) + QStringLiteral(" : ") + number (fgetcp() , 0 , 6 , 4));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_displacement")) , 36) + QStringLiteral(" : ") + number (displacement / 1000.f , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_t")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_deadrise_amidships")) , 36) + QStringLiteral(" : ") + number (fgetle() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_degr")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_x_of_cog")) , 36) + QStringLiteral(" : ") + number (fgetlcb() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_ft")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_midship_area_above_waterline")) , 36) + QStringLiteral(" : ") + number (fgetsa() , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_ft")) + QStringLiteral("^2"));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_roughness_coefficient_o10_3")) , 36) + QStringLiteral(" : ") + number (fgetcaa() , 0 , 6 , 3));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_maximal_speed")) , 36) + QStringLiteral(" : ") + number (fgetat_ax() , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_knots")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_angle_between_thrust_and_keel_lines")) , 36) + QStringLiteral(" : ") + number (fgetangle() , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_degr")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_distance_between_thrust_line_and_cog")) , 36) + QStringLiteral(" : ") + number (ke , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_ft")));
        }
    if (field(QStringLiteral("combobox")) . itemindex == 2)
        resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_half_entrance_angle_of_dwl")) , 36) + QStringLiteral(" : ") + number (ie_ , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_degr")));
    resultsmemo.append(QStringLiteral(""));
    resultsmemo.append(space (10) + QStringLiteral("----------------------------------------------------------------------------"));
    resultsmemo.append(QStringLiteral(""));
    resultsmemo.append(QStringLiteral(""));
    validdata = true;
    if ((fgetcp() < 0.4) || (fgetcp() > 0.9))
        {
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_cp_is_outside_valid_domain")) + QStringLiteral(" of 0.40 .. 0.90"));
            validdata = false;
            ; // presentation only
        }
    if (! validdata)
        {
            ; // presentation only
            return;
        }
    ii = 0;
    beta = fgetle();
    vo = 1.7 * sqrtp (g * beamwaterline) / 0.51444;
    vk = maxspeed;
    dv = (vk - vo) / 10;
    for (i=1; i<=11; ++i)
    {
        check();
        {
            v = vo + dv * (i - 1);
            speed = v;
            frb = v * 0.51444 / sqrtp (g * beamwaterline);
            v_ = displacement / ro;
            vms = v * 0.51444;
            if (field(QStringLiteral("combobox")) . itemindex < 2)
                {
                    if ((frb < 1.69) || (frb > 120))
                        {
                            resultsmemo.append(space (10) + QStringLiteral("FrB ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 1,7...120"));
                            return;
                        }
                }
            if (field(QStringLiteral("combobox")) . itemindex == 0)
                {
                    if (i == 1)
                        {
                            resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_for_planing_ships")));
                            resultsmemo.append(QStringLiteral(""));
                            resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
                            resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_l___vs__l__frb__l__tau__l___r_f___l___r_p___l___r_t___l___pe____l")));
                            if (settings. projectunits == 1)
                                resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_l___kn__l_______l__degr_l___lbf___l___lbf___l___lbf___l___ehp___l")));
                            else
                                resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_l___kn__l_______l__degr_l___kn____l___kn____l___kn____l___kw____l")));
                            resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
                        }
                    mdelta = xg / beamwaterline;
                    if ((mdelta < 0) || (mdelta > 6))
                        {
                            resultsmemo.append(space (10) + QStringLiteral("Mdelta ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0...6"));
                            return;
                        }
                    ; // presentation only
                    x = mdelta;
                    x2 = x * x;
                    x3 = x2 * x;
                    x4 = x3 * x;
                    lamb [1] = 1.3147917 * x + 0.0223824;
                    lamb [2] = 8.0300346e-4 * x4 + 0.0402645 * x3 - 0.1871798 * x2 + 1.5191684 * x - 0.0016619;
                    lamb [3] = 0.045736 * x3 - 0.1649088 * x2 + 1.4600864 * x + 0.0060418;
                    lamb [4] = 0.0767865 * x3 - 0.2522323 * x2 + 1.5197702 * x + 0.0040581;
                    lamb [5] = 0.0956775 * x3 - 0.2631637 * x2 + 1.5225045 * x + 0.0020694;
                    lamb [6] = 0.15136 * x3 - 0.3950466 * x2 + 1.5961602 * x + 0.0018206;
                    lamb [7] = 0.1224444 * x3 - 0.1189542 * x2 + 1.2872028 * x + 0.0086618;
                    lamb [8] = 0.0727007 * x3 + 0.2915599 * x2 + 1.002391 * x + 0.0188425;
                    lamb [9] = lamb [8];
                    lamb [10] = lamb [8];
                    sfinex1 (10 , frbl , lamb , frb , lambda);
                    if (beta != 0)
                        {
                            beta0 = beta / 57.29;
                            lambda = power (lambda , 0.8) / cosp (beta0) * (1 - 0.29 * power (sinp (beta0) , 0.28)) * (1 + 1.35 * power (sinp (beta0) , 0.44) * mdelta / sqrtp (frb));
                        }
                    x = lambda;
                    x2 = x * x;
                    x3 = x2 * x;
                    x4 = x3 * x;
                    x5 = x4 * x;
                    cba [1] = 0.006023 * x5 - 0.0789897 * x4 + 0.4007108 * x3 - 0.88125 * x2 + 1.568401 * x - 0.0011054;
                    cba [2] = 5.6471065e-4 * x5 - 0.0173562 * x4 + 0.174458 * x3 - 0.6480927 * x2 + 1.4938827 * x + 1.9443499e-4;
                    cba [3] = 0.0039329 * x5 - 0.0664014 * x4 + 0.4218362 * x3 - 1.1780371 * x2 + 1.8156875 * x - 2.2135733e-14;
                    cba [4] = 0.0019057 * x5 - 0.0363587 * x4 + 0.2645258 * x3 - 0.8691566 * x2 + 1.6425118 * x - 4.2390418e-5;
                    cba [5] = 0.0018769 * x5 - 0.035802 * x4 + 0.2632762 * x3 - 0.8901269 * x2 + 1.654128 * x + 7.9306198e-4;
                    cba [6] = 0.0028126 * x5 - 0.0473596 * x4 + 0.3112191 * x3 - 0.9859457 * x2 + 1.7202846 * x - 3.8697736e-4;
                    cba [7] = 0.0026331 * x5 - 0.0452836 * x4 + 0.3061261 * x3 - 0.9963483 * x2 + 1.7282248 * x + 3.1026496e-4;
                    cba [8] = 0.0018866 * x5 - 0.0352181 * x4 + 0.2599424 * x3 - 0.9205687 * x2 + 1.6912495 * x + 1.0641188e-4;
                    cba [9] = 5.3932921e-4 * x5 - 0.0156321 * x4 + 0.1605772 * x3 - 0.7270102 * x2 + 1.5740995 * x + 0.0011091;
                    cba [10] = 8.6874449e-4 * x5 - 0.0200962 * x4 + 0.1786243 * x3 - 0.7617407 * x2 + 1.5986308 * x + 4.6474589e-4;
                    sfinex1 (10 , frbc , cba , frb , cb_a);
                    cb = 2 * g * displacement / ro / vms / vms / beamwaterline / beamwaterline;
                    alfa = cb / cb_a;
                    if (beta != 0)
                        {
                            beta0 = beta / 57.29;
                            alfab = alfa + 0.15 * power (sinp (beta0) , 0.8) / power (frb , 0.3) * (1 - 0.17 * sqrtp (lambda * cosp (beta0))) / sqrtp (lambda * cosp (beta0));
                            alfa = alfab;
                        }
                    mdeltaopt = 28.6 * cb - 0.57;
                    omega = lambda * beamwaterline * beamwaterline;
                    alcm = lambda * beamwaterline;
                    re = vms * alcm / nu;
                    cf0 = 0.455 / power (logp (re) , 2.58);
                    dcf0 = fgetcaa() / 1000.f;
                    rf = (cf0 + dcf0) * ro * vms * vms / 2.f * omega;
                    rd = g * displacement * tanp (alfa);
                    frv_ = vms / sqrtp (g * power (displacement / ro , 0.33333));
                    sfinex1 (7 , frv , k_ap , frv_ , kap);
                    if (frv_ > frv [7])
                        ii = ii + 1;
                    rt = rf + rd;
                    k_red = 0.77 + 0.025 * (frb - 5) * (frb - 5) - 0.0035 * (frb - 5) * (frb - 5) * (frb - 5);
                    rap = kap * rt * i4;
                    if (i5 == 1)
                        rt = rt * k_red;
                    raa = 1.226 * vms * vms * 0.7 / 2.f * fgetsa() * i3;
                    rt = rt + rap + raa;
                    pe = rt * vms;
                    resvs [i] = vms / 0.51444;
                    resrt [i] = rt / 1000;
                    y = g * displacement / 1000.f;
                    ak = y / rt;
                    akm = 0.36 * (cb + 0.02) + 4.63;
                    eps = 1 / ak;
                    a1 = 0.7 * 3.1415926 * lambda / (1 + 1.4 * lambda);
                    a2 = lambda * (lambda - 0.4) / (lambda + 0.4) / frb / frb;
                    cb_a_ = a1 + lambda * a2;
                    mdelta_ = 0.75f * a1 + 0.08 * power (lambda , 0.865) / sqrtp (frb) + power (lambda , 2) * (lambda - 0.8) / (1.2 + 3 * lambda) / frb / frb;
                    mdelta_ = mdelta_ / (a1 + a2);
                    if (settings. projectunits != 1)
                        {
                            pe = pe * 0.001;
                            if (pe >= 100000)
                                n1 = 0;
                            if ((pe >= 10000) && (pe < 100000))
                                n1 = 1;
                            if ((pe >= 1000) && (pe < 10000))
                                n1 = 2;
                            if ((pe >= 100) && (pe < 1000))
                                n1 = 3;
                            if (pe < 100)
                                n1 = 4;
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (v , 2 , 5) + QStringLiteral(" | ") + pad (frb , 2 , 5) + QStringLiteral(" | ") + pad (alfa * 57.3 , 2 , 5) + QStringLiteral(" | ") + pad (rf * 0.001 , 3 , 7) + QStringLiteral(" | ") + pad (rd * 0.001 , 3 , 7) + QStringLiteral(" | ") + pad (rt * 0.001 , 3 , 7) + QStringLiteral(" | ") + pad (pe , n1 , 7) + QStringLiteral(" |"));
                            pointIndex(0, speed , rf * 0.01);
                            pointIndex(1, speed , rd * 0.01);
                            pointIndex(2, speed , rt * 0.01);
                            pointIndex(3, speed , pe);
                            pointIndex(4, speed , alfa * 57.3);
                        }
                    else
                        {
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (v , 2 , 5) + QStringLiteral(" | ") + pad (frb , 2 , 5) + QStringLiteral(" | ") + pad (alfa * 57.3 , 2 , 5) + QStringLiteral(" | ") + pad (rf / 4.448222 , 1 , 7) + QStringLiteral(" | ") + pad (rd / 4.448222 , 1 , 7) + QStringLiteral(" | ") + pad (rt / 4.448222 , 1 , 7) + QStringLiteral(" | ") + pad (pe * 0.00136 , 1 , 7) + QStringLiteral(" |"));
                            pointIndex(0, speed , rf / 4.448222 * 0.1);
                            pointIndex(1, speed , rd / 4.448222 * 0.1);
                            pointIndex(2, speed , rt / 4.448222 * 0.1);
                            pointIndex(3, speed , pe * 0.00136);
                            pointIndex(4, speed , alfa * 57.3);
                        }
                }
            ; // presentation only
        }
    }
    if (field(QStringLiteral("combobox")) . itemindex == 1)
        {
            if (settings. projectunits != 1)
                {
                    effectivelengthwaterline = fgetewl() / foot;
                    beamwaterline = fgetbwl() / foot;
                    xg = fgetlcb() / foot;
                    displacement = fgetdispl() * 1000;
                    ro = ro * 63.4297;
                }
            else
                {
                    beamwaterline = fgetbwl();
                    bwl_imp = fgetbwl() * foot;
                    effectivelengthwaterline = fgetewl();
                    displacement = fgetdispl() * 1000;
                    xg = fgetlcb();
                }
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_for_planing_ships")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_d_savitsky")));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_l___vs__l__frb__l__tau__l___r_f___l___r_p___l___r_t___l___pe____l")));
            if (settings. projectunits == 1)
                resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_l___kn__l_______l__degr_l___lbf___l___lbf___l___lbf___l___ehp___l")));
            else
                resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_l___kn__l_______l__degr_l___kn____l___kn____l___kn____l___kw____l")));
            resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            for (i=1; i<=11; ++i)
            {
                check();
                {
                    v = vo + dv * (i - 1);
                    speed = v;
                    frb = v * 0.51444 / sqrtp (g * beamwaterline * foot);
                    v_ = displacement / ro;
                    vms = v * 0.51444;
                    g_imp = 32.174;
                    ro_imp = 1.937;
                    slr = v / sqrtp (effectivelengthwaterline);
                    vfoot = v * 1.689;
                    fr_v = vfoot / sqrtp (g_imp * power (displacement / ro_imp / g_imp * 2.240 , 0.333333));
                    cv = vfoot / sqrtp (g_imp * beamwaterline);
                    if ((cv <= 0.6) || (cv >= 25))
                        resultsmemo.append(space (10) + QStringLiteral(" Cv ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,6 ... 25,0"));
                    clbeta = displacement * 2.24 / 0.5f / ro_imp / vfoot / vfoot / beamwaterline / beamwaterline;
                    if (fgetle() > 0)
                        {
                            for (ki=1; ki<=5; ++ki)
                            {
                                check();
                                {
                                    ax [ki] = clbeta + 0.02 * ki - 0.06;
                                    if (ax [ki] <= 0)
                                        ax [ki] = 0.0001;
                                    bx [ki] = ax [ki] - 0.0065 * fgetle() * power (ax [ki] , 0.6);
                                }
                            }
                            j = 5;
                            sfinex1 (j , bx , ax , clbeta , clo);
                            if (clo < 0)
                                clo = ax [3];
                        }
                    else
                        clo = clbeta;
                    tau = 1;
                    label_1:;
                    clon = clo / power (tau , 1.1);
                    lambda = 1.8;
                    for (ki=1; ki<=5; ++ki)
                    {
                        check();
                        {
                            ax [ki] = lambda - 3.4 + 1.7 * ki;
                            bx [ki] = 0.012 * sqrtp (ax [ki]) + 0.0055 * power (ax [ki] , 2.5f) / cv / cv;
                        }
                    }
                    j = 5;
                    sfinex1 (j , bx , ax , clon , lambda);
                    if (lambda < 0.4)
                        lambda = 0.4;
                    vm_v = sqrtp (1 - ((0.0120 * power (tau , 1.1)) / (sqrtp (lambda) * cosp (tau / 57.29)) - (0.000457 * fgetle() * power (tau , 0.66)) / (power (lambda , 0.34) * cosp (tau / 57.29))));
                    vms = v * 0.51444 * vm_v;
                    re = vms * lambda * bwl_imp / nu;
                    vms = vfoot * vm_v;
                    nu_imp = nu / foot / foot;
                    re = vms * lambda * beamwaterline / nu_imp;
                    cf0 = 0.075 / power ((logp (re) - 2) , 2);
                    dcf0 = fgetcaa() / 1000.f;
                    dtau = ro_imp * power (vfoot * vm_v , 2) * lambda * beamwaterline * beamwaterline * (cf0 + dcf0) / 2 / cosp (fgetle() / 57.29);
                    dtau_f = dtau / cosp (tau / 57.29);
                    x = displacement * 2.240 * tanp (tau / 57.29);
                    rt = x + dtau_f;
                    x2 = 0.75f - (1 / (5.21 * cv * cv / lambda / lambda + 2.39));
                    x3 = x2 * lambda * beamwaterline;
                    x4 = xg - x3;
                    f = ke;
                    epsilon = fgetangle();
                    x5 = (x4 / cosp (tau / 57.29)) * (1 - sinp (tau / 57.29) * sinp ((tau + epsilon) / 57.29)) - f * sinp (tau / 57.29);
                    x2 = displacement * 2.240 * x5;
                    x3 = dtau * (1.383 - f);
                    x4 = x2 + x3;
                    if (x4 < 0)
                        {
                            tau = tau + 0.001;
                            if (tau < 15)
                                goto label_1;
                        }
                    pe = rt * v * 0.51444;
                    resvs [i] = v;
                    if ((lambda > 5) || (lambda < 0.5f))
                        {
                            resultsmemo.append(space (10) + QStringLiteral(" Lambda ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 2,0 ... 5,0"));
                        }
                    if ((tau > 14.99) || (tau < 1.1))
                        {
                            resultsmemo.append(space (10) + QStringLiteral(" Tau ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 2,0 ... 15,0"));
                        }
                    x2 = 0.012 * sqrtp (lambda) + 0.0055 * power (lambda , 2.5f) / cv / cv;
                    x3 = x2 * power (tau , 1.1);
                    x = sqrtp (x3 / 2);
                    ax [1] = 0;
                    ax [2] = 10;
                    ax [3] = 20;
                    bx [1] = ((- 372.6982759 * x + 323.514167) * x - 36.3284019) * x + 1.1203417;
                    bx [2] = ((- 386.2648965 * x + 337.1403335) * x - 46.9653753) * x + 3.9393772;
                    bx [3] = ((- 168.068218 * x + 187.7969211) * x - 14.0830549) * x + 2.4001697;
                    j = 3;
                    sfinex1 (j , ax , bx , fgetle() , x4);
                    if (x4 < tau)
                        {
                            resultsmemo.append(space (10) + QStringLiteral("   +-------+-------+-------+---------+---------+---------+---------+"));
                            resultsmemo.append(space (10) + tr(QStringLiteral("rs_attention____porpoising_regime____")));
                            resultsmemo.append(space (10) + QStringLiteral("Tau = ") + number (tau , 0 , 6 , 4) + QStringLiteral(" > Tau_Min = ") + number (x4 , 0 , 6 , 4));
                        }
                    if (settings. projectunits != 1)
                        {
                            pe = pe * 4.448222 * 0.001;
                            resrt [i] = rt * 4.448222e-3;
                            if (pe >= 100000)
                                n1 = 0;
                            if ((pe >= 10000) && (pe < 100000))
                                n1 = 1;
                            if ((pe >= 1000) && (pe < 10000))
                                n1 = 2;
                            if ((pe >= 100) && (pe < 1000))
                                n1 = 3;
                            if (pe < 100)
                                n1 = 4;
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (v , 2 , 5) + QStringLiteral(" | ") + pad (frb , 2 , 5) + QStringLiteral(" | ") + pad (tau , 2 , 5) + QStringLiteral(" | ") + pad (dtau_f * 4.448222e-3 , 3 , 7) + QStringLiteral(" | ") + pad ((rt - dtau_f) * 4.448222e-3 , 3 , 7) + QStringLiteral(" | ") + pad (rt * 4.448222e-3 , 3 , 7) + QStringLiteral(" | ") + pad (pe , n1 , 7) + QStringLiteral(" |"));
                            pointIndex(0, speed , dtau_f * 4.448222e-2);
                            pointIndex(1, speed , (rt - dtau_f) * 4.448222e-2);
                            pointIndex(2, speed , rt * 4.448222e-2);
                            pointIndex(3, speed , pe);
                            pointIndex(4, speed , tau);
                        }
                    else
                        {
                            resrt [i] = rt;
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (v , 2 , 5) + QStringLiteral(" | ") + pad (frb , 2 , 5) + QStringLiteral(" | ") + pad (tau , 2 , 5) + QStringLiteral(" | ") + pad (dtau_f , 1 , 7) + QStringLiteral(" | ") + pad (rt - dtau_f , 1 , 7) + QStringLiteral(" | ") + pad (rt , 1 , 7) + QStringLiteral(" | ") + pad (pe * 4.448222 / 736 , 1 , 7) + QStringLiteral(" |"));
                            pointIndex(0, speed , dtau_f * 0.1);
                            pointIndex(1, speed , (rt - dtau_f) * 0.1);
                            pointIndex(2, speed , rt * 0.1);
                            pointIndex(3, speed , pe * 0.0013587 * 4.448222);
                            pointIndex(4, speed , tau);
                        }
                }
            }
        }
    if (field(QStringLiteral("combobox")) . itemindex == 2)
        {
            frv_ = vms / sqrtp (g * power (displacement / ro , 0.33333));
            if ((frv_ < 1) || (frv_ > 2))
                {
                    resultsmemo.append(space (10) + QStringLiteral("FrV ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 1,0 ... 2,0"));
                    resultsmemo.append(QStringLiteral(" "));
                }
            v_ = displacement / ro;
            x = power (v_ , 0.33333) / effectivelengthwaterline;
            z = v_ / beamwaterline / beamwaterline / beamwaterline;
            u = power (2 * ie_ , 0.5f);
            if (settings. projectunits != 1)
                fsetangle(beamwaterline * draft * cm);
            else
                fsetangle(beamwaterline * draft * cm / foot / foot);
            if ((fgetk() > fgetangle()) || (fgetk() < 0))
                fsetk(fgetangle());
            if ((fgetangle() > 0) && (fgetk() > 0))
                w = fgetk() / fgetangle();
            else
                {
                    resultsmemo.append(space (10) + QStringLiteral(" Errore: At=") + number (fgetangle() , 0 , 6 , 3) + QStringLiteral(" <=0"));
                    return;
                }
            if ((w > 1) || (w < 0))
                {
                    resultsmemo.append(space (10) + QStringLiteral(" At/Axx is out of valid domain 0,0 ... 1,0"));
                    return;
                }
            wetarea = 2.262 * sqrtp (effectivelengthwaterline / power (v_ , 0.33333)) * (1 + 0.046 * beamwaterline / draft + 0.00287 * (beamwaterline / draft) * (beamwaterline / draft)) * power (v_ , 0.66666);
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_by_mercier_and_savitsky_s_method")));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_l___vs__l__frb__l__frv__l___r_f___l___r_r___l___r_t___l___pe____l")));
            if (settings. projectunits == 1)
                resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_l___kn__l_______l_______l___lbf___l___lbf___l___lbf___l___ehp___l")));
            else
                resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_l___kn__l_______l_______l___kn____l___kn____l___kn____l___kw____l")));
            resultsmemo.append(space (10) + QStringLiteral("   ") + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            for (i=0; i<=10; ++i)
            {
                check();
                {
                    rtt [i + 1] = 444.97375 * (aa [i][0] + aa [i][1] * x + aa [i][2] * u + aa [i][3] * w + aa [i][4] * x * z + aa [i][5] * x * u + aa [i][6] * x * w + aa [i][7] * z * u + aa [i][8] * z * w + aa [i][9] * (w * w) + aa [i][10] * x * (w * w) + aa [i][11] * z * (x * x) + aa [i][12] * u * (w * w) + aa [i][13] * w * (u * u));
                    fnv = 1.0f + i * 0.1;
                    fnv_ [i + 1] = fnv;
                }
            }
            for (i=1; i<=16; ++i)
            {
                check();
                {
                    fnv = 1.0f + i * 0.0625f;
                    vms = fnv * sqrtp (power (v_ , 0.33333) * g);
                    rn = vms * effectivelengthwaterline / nu;
                    rn_ = vms * effectivelengthwaterline / power (v_ / (444.97 / ro / g * 1000) , 0.33333) / 1.189 * 1000000;
                    cf0 = 0.075 / power ((logp (rn) - 2) , 2);
                    cf0_ = 0.075 / power ((logp (rn_) - 2) , 2);
                    ki = 11;
                    sfinex1 (ki , fnv_ , rtt , fnv , rtt_);
                    rt = (rtt_ / 444.97375 + 0.5f * (cf0 - cf0_ + fgetcaa() / 1000) * wetarea / v_ * vms * vms / g) * v_ * ro * g / 1000;
                    frb = vms / sqrtp (g * beamwaterline);
                    dtau_f = cf0 * ro * vms * vms / 2 * wetarea / 1000;
                    pe = rt * vms;
                    speed = vms / 0.514444;
                    resvs [i] = speed;
                    resrt [i] = rt;
                    if (settings. projectunits == 1)
                        {
                            pe = pe / 0.736;
                            resrt [i] = rt / 4.448222 * 1000;
                            if (pe >= 100000)
                                n1 = 0;
                            if ((pe >= 10000) && (pe < 100000))
                                n1 = 1;
                            if ((pe >= 1000) && (pe < 10000))
                                n1 = 2;
                            if ((pe >= 100) && (pe < 1000))
                                n1 = 3;
                            if (pe < 100)
                                n1 = 4;
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (vms / 0.514444 , 2 , 5) + QStringLiteral(" | ") + pad (frb , 2 , 5) + QStringLiteral(" | ") + pad (fnv , 2 , 5) + QStringLiteral(" | ") + pad (dtau_f / 4.448222 * 1000 , 1 , 7) + QStringLiteral(" | ") + pad ((rt - dtau_f) / 4.448222 * 1000 , 1 , 7) + QStringLiteral(" | ") + pad (rt / 4.448222 * 1000 , 1 , 7) + QStringLiteral(" | ") + pad (pe , n1 , 7) + QStringLiteral(" |"));
                            pointIndex(0, speed , dtau_f / 4.448222 * 100);
                            pointIndex(1, speed , (rt - dtau_f) / 4.448222 * 100);
                            pointIndex(2, speed , rt / 4.448222 * 100);
                            pointIndex(3, speed , pe);
                        }
                    else
                        {
                            resrt [i] = rt;
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (vms / 0.514444 , 2 , 5) + QStringLiteral(" | ") + pad (frb , 2 , 5) + QStringLiteral(" | ") + pad (fnv , 2 , 5) + QStringLiteral(" | ") + pad (dtau_f , 3 , 7) + QStringLiteral(" | ") + pad (rt - dtau_f , 3 , 7) + QStringLiteral(" | ") + pad (rt , 3 , 7) + QStringLiteral(" | ") + pad (pe , 2 , 7) + QStringLiteral(" |"));
                            pointIndex(0, speed , dtau_f * 10);
                            pointIndex(1, speed , (rt - dtau_f) * 10);
                            pointIndex(2, speed , rt * 10);
                            pointIndex(3, speed , pe);
                        }
                }
            }
        }
    if (field(QStringLiteral("combobox")) . itemindex == 3)
        {
            dat [1] = displacement * 9.81 / 1000;
            if ((dat [1] > 400) || (dat [1] < 5))
                resultsmemo.append(space (14) + QStringLiteral("Displacement   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   5 ... 400 kN"));
            if ((beta > 15) || (beta < 5))
                resultsmemo.append(space (14) + QStringLiteral("Deadrise angle ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   5 ... 15 degr"));
            dat [2] = fgetewl();
            dat [3] = beamwaterline;
            dat [4] = fgetlcb();
            dat [5] = fgetsa();
            dat [6] = beta;
            dat [7] = ro;
            dat [8] = nu;
            dat [9] = 1;
            dat [10] = 2;
            check();
            check();
            fexecdirectory = solverDirectory;
            file_exportdatacp (dat);
            if (! exists (fexecdirectory + QStringLiteral("/") + QStringLiteral("CLEMPOUP.EXE")))
                {
                    warning(tr(QStringLiteral("rs_missing_file_in_exec__folder_")) + QStringLiteral(" CLEMPOUP.EXE"));
                    return;
                }
            filetofind = findFile (QStringLiteral("clempoup.dat") , work);
            if (filetofind != QStringLiteral("clempoup.dat"))
                {
                    warning(tr(QStringLiteral("rs_have_not_input_file_for_calculation_is_here___")));
                    return;
                }
            execfullname = fexecdirectory + QStringLiteral("/") + QStringLiteral("CLEMPOUP.EXE");
            runSolver(execfullname, fcclempoup);
            resultfilename = QStringLiteral("clempoup.RES");
            i = 1;
            label_newsearch:;
            filetofind = findFile (resultfilename , work);
            if (filetofind == QStringLiteral(""))
                {
                    check();
                    i = i + 1;
                    if (i < 25)
                        goto label_newsearch;
                    else
                        {
                            warning(tr(QStringLiteral("rs_do_not_calculate_this_task__because_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_1__do_not_found_into__exec_or_was_damaged_file")) + QStringLiteral(" Clempoup.exe ") + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_2__input_datas_are_outside_valid_domain_or_catalog_is_not_valid_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_3__this_is_very_slow_computer__fcpu___800_mhz_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_4__cpu_is_loaded_more_80__another_processes_")));
                            if (exists (QStringLiteral("clempoup.dat")))
                                check();
                            return;
                        }
                }
            if (exists (resultfilename))
                {
                    ffile.name = safePath(resultfilename);
                    readFile(ffile);
                    ; // presentation only
                    for (i=1; i<=10; ++i)
                    {
                        check();
                        {
                            readValues(ffile, resvs [i], resfd [i], resbe [i], resrf [i], resrd [i], resrt [i]);
                            if (resrt [i] <= 0)
                                {
                                    resultsmemo.append(space (14) + tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                                    warning(tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                                    ; // presentation only
                                    closeFile(ffile);
                                    check();
                                    return;
                                }
                        }
                    }
                    closeFile(ffile);
                }
            else
                {
                    if (exists (resultfilename))
                        check();
                    warning(QStringLiteral("Один или несколько параметров вне диапазона применения метода!!!"));
                    return;
                }
            if (exists (resultfilename))
                check();
            check();
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_by_clement_poup_s_method")));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l___vs__l__frv__l__tau__l___r_f___l___r_p___l___r_t___l___pe____l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l___kn__l_______l__degr_l___kn____l___kn____l___kn____l___kw____l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            for (i=1; i<=10; ++i)
            {
                check();
                {
                    if (resvs [i] < speed)
                        {
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (resvs [i] , 2 , 5) + QStringLiteral(" | ") + pad (resfd [i] , 2 , 5) + QStringLiteral(" | ") + pad (resbe [i] , 2 , 5) + QStringLiteral(" | ") + pad (resrf [i] , 3 , 7) + QStringLiteral(" | ") + pad (resrd [i] , 3 , 7) + QStringLiteral(" | ") + pad (resrt [i] , 3 , 7) + QStringLiteral(" | ") + pad (resvs [i] * resrt [i] * 0.514444 , 2 , 7) + QStringLiteral(" |"));
                            pointIndex(0, resvs [i] , resrf [i] * 10);
                            pointIndex(1, resvs [i] , resrd [i] * 10);
                            pointIndex(2, resvs [i] , resrt [i] * 10);
                            pointIndex(3, resvs [i] , resvs [i] * resrt [i] * 0.514444);
                            pointIndex(4, resvs [i] , resbe [i]);
                        }
                    else
                        {
                            sfinex1 (10 , resvs , resrf , speed , rf);
                            sfinex1 (10 , resvs , resrd , speed , rd);
                            sfinex1 (10 , resvs , resrt , speed , rt);
                            rt_max = rt;
                            sfinex1 (10 , resvs , resbe , speed , be);
                            sfinex1 (10 , resvs , resfd , speed , fd);
                            pe = rt * speed * 0.51444;
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (speed , 2 , 5) + QStringLiteral(" | ") + pad (fd , 2 , 5) + QStringLiteral(" | ") + pad (be , 2 , 5) + QStringLiteral(" | ") + pad (rf , 3 , 7) + QStringLiteral(" | ") + pad (rd , 3 , 7) + QStringLiteral(" | ") + pad (rt , 3 , 7) + QStringLiteral(" | ") + pad (pe , 2 , 7) + QStringLiteral(" |"));
                            pointIndex(0, speed , rf * 10);
                            pointIndex(1, speed , rd * 10);
                            pointIndex(2, speed , rt * 10);
                            pointIndex(3, speed , pe);
                            pointIndex(4, speed , be);
                            rt = rt * 1000;
                            goto label_2;
                        }
                }
            }
        }
    if (field(QStringLiteral("combobox")) . itemindex == 4)
        {
            dat [1] = displacement * 9.81 / 1000;
            if ((dat [1] > 400) || (dat [1] < 5))
                {
                    resultsmemo.append(space (14) + QStringLiteral("Displacement   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   5 ... 400 kN"));
                    resultsmemo.append(space (14) + tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                    warning(tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                    return;
                }
            dat [2] = beamwaterline;
            dat [3] = fgetlcb();
            dat [4] = fgetsa();
            dat [5] = 10;
            dat [6] = 0;
            dat [7] = ro;
            dat [8] = nu;
            dat [9] = 1;
            dat [10] = 2;
            check();
            check();
            fexecdirectory = solverDirectory;
            file_exportdatacb (dat);
            filetofind = findFile (QStringLiteral("clemblou.dat") , work);
            if (filetofind != QStringLiteral("clemblou.dat"))
                {
                    warning(tr(QStringLiteral("rs_have_not_input_file_for_calculation_is_here___")));
                    return;
                }
            execfullname = fexecdirectory + QStringLiteral("/") + QStringLiteral("CLEMBLOU.EXE");
            runSolver(execfullname, fcclemblou);
            resultfilename = QStringLiteral("clemblou.res");
            i = 1;
            label_newsearch1:;
            filetofind = findFile (resultfilename , work);
            if (filetofind == QStringLiteral(""))
                {
                    check();
                    i = i + 1;
                    if (i < 25)
                        goto label_newsearch1;
                    else
                        {
                            warning(tr(QStringLiteral("rs_do_not_calculate_this_task__because_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_1__do_not_found_into__exec_or_was_damaged_file")) + QStringLiteral(" Clemblou.exe ") + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_2__input_datas_are_outside_valid_domain_or_catalog_is_not_valid_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_3__this_is_very_slow_computer__fcpu___800_mhz_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_4__cpu_is_loaded_more_80__another_processes_")));
                            if (exists (QStringLiteral("clemblou.dat")))
                                check();
                            return;
                        }
                }
            ffile.name = safePath(resultfilename);
            readFile(ffile);
            ; // presentation only
            for (i=1; i<=7; ++i)
            {
                check();
                {
                    if (atEnd (ffile))
                        break;
                    readValues(ffile, resvs [i], resfd [i], resbe [i], resrf [i], resrd [i], resrt [i]);
                    if (resrt [i] <= 0)
                        {
                            resultsmemo.append(space (14) + tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                            warning(tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                            ; // presentation only
                            closeFile(ffile);
                            if (exists (resultfilename))
                                check();
                            return;
                        }
                }
            }
            closeFile(ffile);
            if (exists (resultfilename))
                check();
            check();
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_by_clement_blount_s_method")));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l___vs__l__frv__lepsilonl___r_f___l___r_p___l___r_t___l___pe____l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l___kn__l_______l_______l___kn____l___kn____l___kn____l___kw____l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            for (i=1; i<=7; ++i)
            {
                check();
                {
                    if (resvs [i] < speed)
                        {
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (resvs [i] , 2 , 5) + QStringLiteral(" | ") + pad (resfd [i] , 2 , 5) + QStringLiteral(" | ") + pad (resbe [i] , 3 , 5) + QStringLiteral(" | ") + pad (resrf [i] , 3 , 7) + QStringLiteral(" | ") + pad (resrd [i] , 3 , 7) + QStringLiteral(" | ") + pad (resrt [i] , 3 , 7) + QStringLiteral(" | ") + pad (resvs [i] * resrt [i] * 0.514444 , 2 , 7) + QStringLiteral(" |"));
                            pointIndex(0, resvs [i] , resrf [i] * 10);
                            pointIndex(1, resvs [i] , resrd [i] * 10);
                            pointIndex(2, resvs [i] , resrt [i] * 10);
                            pointIndex(3, resvs [i] , resvs [i] * resrt [i] * 0.514444);
                            pointIndex(4, resvs [i] , resbe [i]);
                        }
                    else
                        {
                            if (speed < resvs [1])
                                {
                                    resultsmemo.append(space (13) + QStringLiteral("Increase the designed speed..."));
                                    return;
                                }
                            sfinex1 (7 , resvs , resrf , speed , rf);
                            sfinex1 (7 , resvs , resrd , speed , rd);
                            if (rd < 0)
                                return;
                            sfinex1 (7 , resvs , resrt , speed , rt);
                            rt_max = rt;
                            sfinex1 (7 , resvs , resbe , speed , be);
                            sfinex1 (7 , resvs , resfd , speed , fd);
                            pe = rt * speed * 0.51444;
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (speed , 2 , 5) + QStringLiteral(" | ") + pad (fd , 2 , 5) + QStringLiteral(" | ") + pad (be , 3 , 5) + QStringLiteral(" | ") + pad (rf , 3 , 7) + QStringLiteral(" | ") + pad (rd , 3 , 7) + QStringLiteral(" | ") + pad (rt , 3 , 7) + QStringLiteral(" | ") + pad (pe , 2 , 7) + QStringLiteral(" |"));
                            pointIndex(0, speed , rf * 10);
                            pointIndex(1, speed , rd * 10);
                            pointIndex(2, speed , rt * 10);
                            pointIndex(3, speed , pe);
                            pointIndex(4, speed , be);
                            rt = rt * 1000;
                            goto label_2;
                        }
                }
            }
        }
    if (field(QStringLiteral("combobox")) . itemindex == 5)
        {
            dat [1] = effectivelengthwaterline;
            dat [2] = beamwaterline;
            dat [3] = draft;
            dat [4] = displacement / 1000;
            dat [5] = xg;
            dat [6] = 0.7;
            dat [7] = beta;
            dat [8] = fgetws() / power (displacement / ro , 0.6666);
            dat [9] = ro;
            dat [10] = nu;
            check();
            check();
            fexecdirectory = solverDirectory;
            file_exportdatabu (dat);
            resultsmemo.append(space (14) + QStringLiteral("L/B   =") + number (dat [1] / dat [2] , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Btr/B =") + number (dat [6] , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Xg/L  =") + number (dat [5] / dat [1] , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("D/B^3 =") + number (dat [4] / power (dat [2] , 3) , 0 , 6 , 3));
            if ((dat [1] / dat [2] > 7) || (dat [1] / dat [2] < 3.5f))
                resultsmemo.append(space (14) + QStringLiteral("L/B        ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("  3,5 ... 7,0"));
            if ((beta > 21) || (beta < 12))
                resultsmemo.append(space (14) + tr(QStringLiteral("rs_deadrise_amidships")) + QStringLiteral(" ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   12 ... 21 ") + tr(QStringLiteral("rs_degr")));
            if ((fgetbetar() > 12) || (fgetbetar() < 5))
                resultsmemo.append(space (14) + tr(QStringLiteral("rs_spray_strips_deadrise")) + QStringLiteral(" ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   5 ... 12 ") + tr(QStringLiteral("rs_degr")));
            if ((fgetbetat() > 6) || (fgetbetat() < 0))
                resultsmemo.append(space (14) + tr(QStringLiteral("rs_transom_deadrise")) + QStringLiteral(" ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   0 ... 6 ") + tr(QStringLiteral("rs_degr")));
            if ((dat [6] > 1) || (dat [6] < 0.675))
                resultsmemo.append(space (14) + QStringLiteral("Btr/B      ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("  0,675 ... 1,0"));
            if ((dat [5] / dat [1] > 0.475) || (dat [5] / dat [1] < 0.325))
                resultsmemo.append(space (14) + QStringLiteral("Xg/L       ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("  0,325 ... 0,475"));
            if ((dat [4] / power (dat [2] , 3) > 0.9) || (dat [4] / power (dat [2] , 3) < 0.427))
                resultsmemo.append(space (14) + QStringLiteral("D/B^3      ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("  0,427 ... 0,90"));
            if (dat [4] / power (dat [2] , 3) > 1)
                return;
            filetofind = findFile (QStringLiteral("bunkdata.dat") , work);
            if (filetofind != QStringLiteral("bunkdata.dat"))
                {
                    warning(tr(QStringLiteral("rs_have_not_input_file_for_calculation_is_here___")));
                    return;
                }
            execfullname = fexecdirectory + QStringLiteral("/") + QStringLiteral("BUNKOV.EXE");
            runSolver(execfullname, fcbunkov);
            resultfilename = QStringLiteral("bunkov00.res");
            i = 1;
            label_newsearch2:;
            filetofind = findFile (resultfilename , work);
            if (filetofind == QStringLiteral(""))
                {
                    check();
                    i = i + 1;
                    if (i < 25)
                        goto label_newsearch2;
                    else
                        {
                            warning(tr(QStringLiteral("rs_do_not_calculate_this_task__because_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_1__do_not_found_into__exec_or_was_damaged_file")) + QStringLiteral(" Bunkov.exe ") + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_2__input_datas_are_outside_valid_domain_or_catalog_is_not_valid_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_3__this_is_very_slow_computer__fcpu___800_mhz_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_4__cpu_is_loaded_more_80__another_processes_")));
                            if (exists (QStringLiteral("bunkdata.dat")))
                                check();
                            return;
                        }
                }
            ffile.name = safePath(resultfilename);
            readFile(ffile);
            ; // presentation only
            for (i=1; i<=16; ++i)
            {
                check();
                {
                    readValues(ffile, resvs_ [i], resfd_ [i], resbe_ [i], resrf_ [i], resrd_ [i], resrt_ [i]);
                    if ((resrt_ [i] <= 0) || (resrd_ [i] <= 0))
                        {
                            resultsmemo.append(space (14) + tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                            warning(tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                            ; // presentation only
                            closeFile(ffile);
                            if (exists (resultfilename))
                                check();
                            return;
                        }
                }
            }
            closeFile(ffile);
            if (exists (resultfilename))
                check();
            check();
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_by_m_bunkov_method")));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l___vs__l__frv__l__tau__l___r_f___l___r_p___l___r_t___l___pe____l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l___kn__l_______l__degr_l___kn____l___kn____l___kn____l___kw____l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            dvs = (speed - resvs_ [1]) / 15;
            vsr = resvs_ [1] - dvs;
            for (i=1; i<=16; ++i)
            {
                check();
                {
                    vsr = vsr + dvs;
                    sfinex1 (16 , resvs_ , resfd_ , vsr , fd);
                    sfinex1 (16 , resvs_ , resbe_ , vsr , be);
                    sfinex1 (16 , resvs_ , resrf_ , vsr , rf);
                    sfinex1 (16 , resvs_ , resrd_ , vsr , rd);
                    sfinex1 (16 , resvs_ , resrt_ , vsr , rt);
                    resvs [i] = vsr;
                    resfd [i] = fd;
                    resbe [i] = be;
                    resrf [i] = rf;
                    resrd [i] = rd;
                    resrt [i] = rt;
                    pe = rt * vsr * 0.51444;
                    resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (vsr , 2 , 5) + QStringLiteral(" | ") + pad (resfd [i] , 2 , 5) + QStringLiteral(" | ") + pad (resbe [i] , 2 , 5) + QStringLiteral(" | ") + pad (resrf [i] , 3 , 7) + QStringLiteral(" | ") + pad (resrd [i] , 3 , 7) + QStringLiteral(" | ") + pad (resrt [i] , 3 , 7) + QStringLiteral(" | ") + pad (resvs [i] * resrt [i] * 0.514444 , 2 , 7) + QStringLiteral(" |"));
                    pointIndex(0, resvs [i] , resrf [i] * 10);
                    pointIndex(1, resvs [i] , resrd [i] * 10);
                    pointIndex(2, resvs [i] , resrt [i] * 10);
                    pointIndex(3, resvs [i] , resvs [i] * resrt [i] * 0.514444);
                    pointIndex(4, resvs [i] , be);
                }
            }
            rt = rt * 1000;
            goto label_2;
        }
    if (field(QStringLiteral("combobox")) . itemindex == 6)
        {
            dat [1] = fgetewl();
            dat [2] = beamwaterline;
            dat [3] = draft;
            dat [4] = displacement / 1000;
            dat [5] = xg;
            dat [6] = fgetws();
            dat [7] = ro;
            dat [8] = nu;
            dat [9] = i6;
            dat [10] = 0;
            check();
            check();
            fexecdirectory = solverDirectory;
            file_exportdatacompton (dat);
            v_l = dat [4] / power (0.01 * dat [1] / 0.3048 , 3);
            resultsmemo.append(space (14) + QStringLiteral("L/B         = ") + number (dat [1] / dat [2] , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Xg/L        = ") + number (dat [5] / dat [1] , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("V/(0.01*L)^3= ") + number (v_l , 0 , 6 , 1));
            if ((dat [1] / dat [2] > 5.2) || (dat [1] / dat [2] < 4))
                resultsmemo.append(space (14) + QStringLiteral("L/B          ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 4,0 ... 5,2"));
            if ((dat [5] / dat [1] > 0.48) || (dat [5] / dat [1] < 0.37))
                resultsmemo.append(space (14) + QStringLiteral("Xg/L         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,37 ... 0,48"));
            if ((v_l > 150) || (v_l < 105))
                resultsmemo.append(space (14) + QStringLiteral("V/(0.01*L)^3 ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 105 ... 150"));
            v_l = maxspeed * 0.51444 / sqrtp (dat [1]);
            resultsmemo.append(space (14) + QStringLiteral("Vs/Lwl^0,5  = ") + number (v_l , 0 , 6 , 3));
            if ((v_l > 2) || (v_l < 0.35))
                resultsmemo.append(space (14) + QStringLiteral("Vs/Lwl^0,5   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,35 ... 2,0"));
            filetofind = findFile (QStringLiteral("cmptdata.dat") , work);
            if (filetofind != QStringLiteral("cmptdata.dat"))
                {
                    warning(tr(QStringLiteral("rs_have_not_input_file_for_calculation_is_here___")));
                    return;
                }
            execfullname = fexecdirectory + QStringLiteral("/") + QStringLiteral("COMPTON.EXE");
            runSolver(execfullname, fccompton);
            resultfilename = QStringLiteral("Compton0.res");
            i = 1;
            label_newsearch3:;
            filetofind = findFile (resultfilename , work);
            if (filetofind == QStringLiteral(""))
                {
                    check();
                    i = i + 1;
                    if (i < 25)
                        goto label_newsearch3;
                    else
                        {
                            warning(tr(QStringLiteral("rs_do_not_calculate_this_task__because_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_1__do_not_found_into__exec_or_was_damaged_file")) + QStringLiteral(" Compton.exe ") + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_2__input_datas_are_outside_valid_domain_or_catalog_is_not_valid_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_3__this_is_very_slow_computer__fcpu___800_mhz_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_4__cpu_is_loaded_more_80__another_processes_")));
                            if (exists (QStringLiteral("cmptdata.dat")))
                                check();
                            return;
                        }
                }
            ffile.name = safePath(resultfilename);
            readFile(ffile);
            ; // presentation only
            for (i=1; i<=11; ++i)
            {
                check();
                {
                    readValues(ffile, resvs_ [i], resfd_ [i], resbe_ [i], resrf_ [i], resrd_ [i], resrt_ [i]);
                    if ((resrt_ [i] <= 0) || (resrd_ [i] <= 0))
                        {
                            resultsmemo.append(space (14) + tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                            warning(tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                            ; // presentation only
                            closeFile(ffile);
                            if (exists (resultfilename))
                                check();
                            return;
                        }
                }
            }
            closeFile(ffile);
            if (exists (resultfilename))
                check();
            check();
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_by_compton_s_method")));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l___vs__l__frl__l__frv__l___r_f___l___r_r___l___r_t___l___pe____l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l___kn__l_______l_______l___kn____l____kn___l____kn___l___kw____l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            dvs = (speed - resvs_ [1]) / 15;
            vsr = resvs_ [1] - dvs;
            for (i=1; i<=16; ++i)
            {
                check();
                {
                    vsr = vsr + dvs;
                    sfinex1 (11 , resvs_ , resfd_ , vsr , fd);
                    sfinex1 (11 , resvs_ , resbe_ , vsr , be);
                    sfinex1 (11 , resvs_ , resrf_ , vsr , rf);
                    sfinex1 (11 , resvs_ , resrd_ , vsr , rd);
                    sfinex1 (11 , resvs_ , resrt_ , vsr , rt);
                    resvs [i] = vsr;
                    resfd [i] = fd;
                    resbe [i] = be;
                    resrf [i] = rf;
                    resrd [i] = rd;
                    resrt [i] = rt;
                    pe = rt * vsr * 0.51444;
                    if ((fd < 0.7))
                        {
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (vsr , 2 , 5) + QStringLiteral(" | ") + pad (resfd [i] , 2 , 5) + QStringLiteral(" | ") + pad (resbe [i] , 2 , 5) + QStringLiteral(" | ") + pad (resrf [i] , 3 , 7) + QStringLiteral(" | ") + pad (resrd [i] , 3 , 7) + QStringLiteral(" | ") + pad (resrt [i] , 3 , 7) + QStringLiteral(" | ") + pad (resvs [i] * resrt [i] * 0.514444 , 2 , 7) + QStringLiteral(" |"));
                            pointIndex(0, resvs [i] , resrf [i] * 10);
                            pointIndex(1, resvs [i] , resrd [i] * 10);
                            pointIndex(2, resvs [i] , resrt [i] * 10);
                            pointIndex(3, resvs [i] , resvs [i] * resrt [i] * 0.514444);
                        }
                }
            }
            rt = rt * 1000;
            goto label_2;
        }
    if (field(QStringLiteral("combobox")) . itemindex == 7)
        {
            dat [1] = fgetewl();
            dat [2] = beamwaterline;
            dat [3] = draft;
            dat [4] = displacement / 1000;
            dat [5] = xg;
            dat [6] = fgetws();
            dat [7] = ro;
            dat [8] = nu;
            dat [9] = i6;
            dat [10] = 0;
            check();
            check();
            fexecdirectory = solverDirectory;
            file_exportdatawolfson (dat);
            if ((dat [1] > 72) || (dat [1] < 9))
                resultsmemo.append(space (14) + QStringLiteral("Lwl   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 10 ... 70 m"));
            finitdirectory = work;
            filetofind = findFile (QStringLiteral("wolfdata.dat") , work);
            if (filetofind != QStringLiteral("wolfdata.dat"))
                {
                    warning(tr(QStringLiteral("rs_have_not_input_file_for_calculation_is_here___")));
                    return;
                }
            execfullname = fexecdirectory + QStringLiteral("/") + QStringLiteral("WOLFSON.EXE");
            runSolver(execfullname, fcwolfson);
            resultfilename = QStringLiteral("Wolfson0.res");
            i = 1;
            label_newsearch4:;
            filetofind = findFile (resultfilename , work);
            if (filetofind == QStringLiteral(""))
                {
                    check();
                    i = i + 1;
                    if (i < 25)
                        goto label_newsearch4;
                    else
                        {
                            warning(tr(QStringLiteral("rs_do_not_calculate_this_task__because_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_1__do_not_found_into__exec_or_was_damaged_file")) + QStringLiteral(" Wolfson.exe ") + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_2__input_datas_are_outside_valid_domain_or_catalog_is_not_valid_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_3__this_is_very_slow_computer__fcpu___800_mhz_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_4__cpu_is_loaded_more_80__another_processes_")));
                            if (exists (QStringLiteral("wolfdata.dat")))
                                check();
                            return;
                        }
                }
            ffile.name = safePath(resultfilename);
            readFile(ffile);
            ; // presentation only
            for (i=1; i<=10; ++i)
            {
                check();
                {
                    if (atEnd (ffile))
                        break;
                    readValues(ffile, resvs_ [i], resfd_ [i], resbe_ [i], resrf_ [i], resrd_ [i], resrt_ [i]);
                    if ((resrt_ [i] <= 0) || (resrd_ [i] <= 0))
                        {
                            resultsmemo.append(space (14) + tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                            warning(tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                            ; // presentation only
                            closeFile(ffile);
                            if (exists (resultfilename))
                                check();
                            return;
                        }
                }
            }
            closeFile(ffile);
            if (exists (resultfilename))
                check();
            check();
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_by_robinson_s_method")));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l___vs__l__frl__l__frv__l___r_f___l___r_r___l___r_t___l___pe____l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l___kn__l_______l_______l___kn____l____kn___l____kn___l___kw____l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            dvs = (speed - resvs_ [1]) / 15;
            vsr = resvs_ [1] - dvs;
            for (i=1; i<=16; ++i)
            {
                check();
                {
                    vsr = vsr + dvs;
                    sfinex1 (10 , resvs_ , resrf_ , vsr , rf);
                    sfinex1 (10 , resvs_ , resrd_ , vsr , rd);
                    sfinex1 (10 , resvs_ , resrt_ , vsr , rt);
                    sfinex1 (10 , resvs_ , resbe_ , vsr , be);
                    sfinex1 (10 , resvs_ , resfd_ , vsr , fd);
                    resvs [i] = vsr;
                    resfd [i] = fd;
                    resbe [i] = be;
                    resrf [i] = rf;
                    resrd [i] = rd;
                    resrt [i] = rt;
                    pe = rt * vsr * 0.51444;
                    if ((be < 3.3))
                        {
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (vsr , 2 , 5) + QStringLiteral(" | ") + pad (resfd [i] , 2 , 5) + QStringLiteral(" | ") + pad (resbe [i] , 2 , 5) + QStringLiteral(" | ") + pad (resrf [i] , 3 , 7) + QStringLiteral(" | ") + pad (resrd [i] , 3 , 7) + QStringLiteral(" | ") + pad (resrt [i] , 3 , 7) + QStringLiteral(" | ") + pad (resvs [i] * resrt [i] * 0.514444 , 2 , 7) + QStringLiteral(" |"));
                            pointIndex(0, resvs [i] , resrf [i] * 10);
                            pointIndex(1, resvs [i] , resrd [i] * 10);
                            pointIndex(2, resvs [i] , resrt [i] * 10);
                            pointIndex(3, resvs [i] , resvs [i] * resrt [i] * 0.514444);
                        }
                }
            }
            rt = rt * 1000;
            goto label_2;
        }
    if (field(QStringLiteral("combobox")) . itemindex == 8)
        {
            dat [1] = fgetewl();
            dat [2] = beamwaterline;
            dat [3] = beta;
            dat [4] = displacement / 1000;
            dat [5] = xg;
            dat [6] = fgetcaa() / 1000;
            dat [7] = ro;
            dat [8] = nu;
            dat [9] = i6;
            dat [10] = 0;
            check();
            check();
            fexecdirectory = solverDirectory;
            file_exportdataradojcic (dat);
            v_l = dat [1] * dat [2] / power (dat [4] , 0.66667);
            resultsmemo.append(space (14) + QStringLiteral("L/B         = ") + number (dat [1] / dat [2] , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Xg/L        = ") + number (dat [5] / dat [1] , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Ap/V^0.6667 = ") + number (v_l , 0 , 6 , 3));
            if ((dat [1] / dat [2] > 7) || (dat [1] / dat [2] < 2.2))
                resultsmemo.append(space (14) + QStringLiteral("L/B    ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 2,36 ... 6,73"));
            if ((dat [5] / dat [1] > 0.448) || (dat [5] / dat [1] < 0.3))
                resultsmemo.append(space (14) + QStringLiteral("Xg/L   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("  0,3 ... 0,448"));
            if ((beta > 37.4) || (beta < 13))
                {
                    resultsmemo.append(space (14) + QStringLiteral("Beta_x ") + pascalFormat(QStringLiteral("%7.2f "), {QVariant(beta)}) + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("  13° ... 37,4°"));
                    return;
                }
            if ((v_l > 9.5f) || (v_l < 4.25f))
                resultsmemo.append(space (14) + QStringLiteral("Ap/V^0.6667 ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 4,25 ... 9,5"));
            filetofind = findFile (QStringLiteral("radodata.dat") , work);
            if (filetofind != QStringLiteral("radodata.dat"))
                {
                    warning(tr(QStringLiteral("rs_have_not_input_file_for_calculation_is_here___")));
                    return;
                }
            execfullname = fexecdirectory + QStringLiteral("/") + QStringLiteral("RADOJCIC.EXE");
            runSolver(execfullname, fcradojcic);
            resultfilename = QStringLiteral("Radojcic.res");
            i = 1;
            label_newsearch5:;
            filetofind = findFile (resultfilename , work);
            if (filetofind == QStringLiteral(""))
                {
                    check();
                    i = i + 1;
                    if (i < 25)
                        goto label_newsearch5;
                    else
                        {
                            warning(tr(QStringLiteral("rs_do_not_calculate_this_task__because_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_1__do_not_found_into__exec_or_was_damaged_file")) + QStringLiteral(" Radojcic.exe ") + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_2__input_datas_are_outside_valid_domain_or_catalog_is_not_valid_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_3__this_is_very_slow_computer__fcpu___800_mhz_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_4__cpu_is_loaded_more_80__another_processes_")));
                            if (exists (QStringLiteral("radodata.dat")))
                                check();
                            return;
                        }
                }
            ffile.name = safePath(resultfilename);
            readFile(ffile);
            ; // presentation only
            for (i=1; i<=8; ++i)
            {
                check();
                {
                    readValues(ffile, resvs_ [i], resfd_ [i], resbe_ [i], resrf_ [i], resrd_ [i], resrt_ [i], resbe2 [i]);
                    if ((resrt_ [i] <= 0) || (resrd_ [i] <= 0))
                        {
                            resultsmemo.append(space (14) + tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                            warning(tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                            ; // presentation only
                            closeFile(ffile);
                            if (exists (resultfilename))
                                check();
                            return;
                        }
                }
            }
            closeFile(ffile);
            if (exists (resultfilename))
                check();
            check();
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_by_radojcic_s_method")));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l___vs__l__frl__l__frv__l___r_f___l___r_r___l___r_t___l___pe____l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l___kn__l_______l_______l___kn____l____kn___l____kn___l___kw____l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_______t_______t_______t_________t_________t_________t_________t")));
            dvs = (speed - resvs_ [1]) / 15;
            vsr = resvs_ [1] - dvs;
            for (i=1; i<=16; ++i)
            {
                check();
                {
                    vsr = vsr + dvs;
                    sfinex1 (8 , resvs_ , resrf_ , vsr , rf);
                    sfinex1 (8 , resvs_ , resrd_ , vsr , rd);
                    sfinex1 (8 , resvs_ , resrt_ , vsr , rt);
                    sfinex1 (8 , resvs_ , resbe_ , vsr , be);
                    sfinex1 (8 , resvs_ , resfd_ , vsr , fd);
                    resvs [i] = vsr;
                    resfd [i] = fd;
                    resbe [i] = be;
                    resrf [i] = rf;
                    resrd [i] = rd;
                    resrt [i] = rt;
                    sfinex1 (8 , resvs_ , resbe2 , vsr , be2);
                    pe = rt * vsr * 0.51444;
                    if (be < 4)
                        {
                            resultsmemo.append(space (10) + QStringLiteral("   | ") + pad (vsr , 2 , 5) + QStringLiteral(" | ") + pad (resfd [i] , 2 , 5) + QStringLiteral(" | ") + pad (resbe [i] , 2 , 5) + QStringLiteral(" | ") + pad (resrf [i] , 3 , 7) + QStringLiteral(" | ") + pad (resrd [i] , 3 , 7) + QStringLiteral(" | ") + pad (resrt [i] , 3 , 7) + QStringLiteral(" | ") + pad (resvs [i] * resrt [i] * 0.514444 , 2 , 7) + QStringLiteral(" |"));
                            pointIndex(0, resvs [i] , resrf [i] * 10);
                            pointIndex(1, resvs [i] , resrd [i] * 10);
                            pointIndex(2, resvs [i] , resrt [i] * 10);
                            pointIndex(3, resvs [i] , resvs [i] * resrt [i] * 0.514444);
                            pointIndex(4, resvs [i] , be2);
                        }
                }
            }
            rt = rt * 1000;
            goto label_2;
        }
    label_2:;
    resultsmemo.append(space (10) + QStringLiteral("   +-------+-------+-------+---------+---------+---------+---------+"));
    resultsmemo.append(QStringLiteral(" "));
    nser = 61 + field(QStringLiteral("combobox")) . itemindex;
    np = 1;
    wt = 0.05;
    t0 = 0.04;
    nr = 1;
    dp = 0.5f * draft;
    if (field(QStringLiteral("combobox1")) . itemindex == 2)
        {
            np = 0.99;
            wt = 0.03;
            t0 = 0.0f;
            nr = 1;
        }
    if (field(QStringLiteral("combobox1")) . itemindex == 3)
        {
            np = 0.99;
            wt = 0.0f;
            t0 = 0.0f;
            nr = 1;
        }
    tmpdir = work + QStringLiteral("/");
    ffile.name = safePath(tmpdir + QStringLiteral("RESISTp.dat"));
    newFile(ffile);
    writeLine(ffile, QStringLiteral("#     Nser      Np       Wt        t       Eta_R     Dp"));
    writeText(ffile, formatValue(nser, 10, 0));
    writeText(ffile, formatValue(np, 10, 0));
    writeText(ffile, formatValue(wt, 10, 4));
    writeText(ffile, formatValue(t0, 10, 4));
    writeText(ffile, formatValue(nr, 10, 4));
    writeLine(ffile, formatValue(dp, 10, 4));
    writeLine(ffile, QStringLiteral("#      Vs       Rt        Rte       Pe       Pee"));
    imax = 0;
    for (ii=2; ii<=16; ++ii)
    {
        check();
        {
            if (! std::isnan (resvs [ii]) && (resvs [ii] > 0) && (resvs [ii] <= speed + 0.1))
                {
                    imax = ii;
                    rt_max = resrt [ii];
                    writeText(ffile, formatValue(resvs [ii], 10, 2));
                    writeText(ffile, formatValue(resrt [ii], 10, 2));
                    writeText(ffile, formatValue(resrt [ii], 10, 2));
                    writeText(ffile, formatValue(resrt [ii] * 0.51444 * resvs [ii], 10, 2));
                    writeLine(ffile, formatValue(resrt [ii] * 0.51444 * resvs [ii], 10, 2));
                }
        }
    }
    closeFile(ffile);
    ffile.name = safePath(tmpdir + QStringLiteral("RESIST.dat"));
    newFile(ffile);
    if (imax < 4)
        bad (QStringLiteral("航速范围内不足四个有效计算点，请检查最大航速及方法适用范围。"));
    for (ii=imax - 3; ii<=imax; ++ii)
    {
        check();
        {
            writeText(ffile, formatValue(resvs [ii], 10, 2));
            writeText(ffile, formatValue(resrt [ii], 10, 2));
            writeText(ffile, formatValue(resrt [ii], 10, 2));
            writeText(ffile, formatValue(resrt [ii] * 0.51444 * resvs [ii], 10, 2));
            writeLine(ffile, formatValue(resrt [ii] * 0.51444 * resvs [ii], 10, 2));
        }
    }
    if (resvs [imax] != speed)
        resrt [imax] = rt_max;
    writeText(ffile, formatValue(speed, 10, 2));
    writeText(ffile, formatValue(resrt [imax], 10, 2));
    writeText(ffile, formatValue(resrt [imax], 10, 2));
    writeText(ffile, formatValue(resrt [imax] * 0.51444 * speed, 10, 2));
    writeLine(ffile, formatValue(resrt [imax] * 0.51444 * speed, 10, 2));
    closeFile(ffile);
    if (field(QStringLiteral("combobox")) . itemindex == 1)
        {
            lk = lambda * fgetbwl() + fgetbwl() / 6.28 * tanp (fgetle() / 57.3) / tanp (tau / 57.3);
            lc = lambda * fgetbwl() - fgetbwl() / 6.28 * tanp (fgetle() / 57.3) / tanp (tau / 57.3);
            ld = lk * sinp (tau / 57.3);
            resultsmemo.append(space (13) + QStringLiteral("Lambda       = ") + number (lambda , 0 , 6 , 3));
            if (settings. projectunits == 1)
                unt = tr(QStringLiteral("rs_ft"));
            else
                unt = tr(QStringLiteral("rs_m"));
            resultsmemo.append(pascalFormat(QStringLiteral("             Lk           = %-6.3f %s"), {QVariant(lk), QVariant(unt)}));
            resultsmemo.append(pascalFormat(QStringLiteral("             Lc           = %-6.3f %s"), {QVariant(lc), QVariant(unt)}));
            resultsmemo.append(pascalFormat(QStringLiteral("             Ld           = %-6.3f %s"), {QVariant(ld), QVariant(unt)}));
        }
    roi = ro;
    if (roi > 2000)
        roi = ro / 63.4297;
    bwli = fgetbwl();
    if (settings. projectunits == 1)
        {
            bwli = fgetbwl() * foot;
            frl = maxspeed * 0.51444 / sqrtp (g * fgetewl() * foot * foot);
        }
    else
        frl = maxspeed * 0.51444 / sqrtp (g * fgetewl());
    frvr = maxspeed * 0.51444 / sqrtp (g * power (displacement / roi , 0.33333));
    frl = maxspeed * 0.51444 / sqrtp (g * fgetewl());
    if (field(QStringLiteral("combobox")) . itemindex == 8)
        {
            if ((frvr > 4) || (frvr < 1))
                resultsmemo.append(space (14) + QStringLiteral("FrV  ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 1,0 ... 4,0"));
            if (frvr > 4)
                return;
        }
    if (field(QStringLiteral("combobox")) . itemindex == 7)
        {
            if ((frvr > 3) || (frvr < 0.5f))
                resultsmemo.append(space (14) + QStringLiteral("FrV  ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,5 ... 3,0"));
        }
    if (field(QStringLiteral("combobox")) . itemindex == 6)
        {
            if ((frl > 0.6) || (frl < 0.1))
                resultsmemo.append(space (14) + QStringLiteral("FrL  ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,1 ... 0,6"));
        }
    cg = displacement / roi / power (bwli , 3);
    if ((frvr > 2.25f) && (frvr < 4.75f))
        {
            sfinex1 (10 , frvl , lopt , frvr , l_opt);
            sfinex1 (8 , frvcg , cgopt , frvr , cg_opt);
            sfinex1 (7 , frvb , betao , frvr , be_opt);
            sfinex1 (18 , frvxg , xgopt , frvr , xg_opt);
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_calculated_frv_")) + QStringLiteral(" ") + number (frvr , 0 , 6 , 3));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t______________________________________________t")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_l____parameter____l___calculated_l___optimal___l")));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t_________________t______________t_____________t")));
            resultsmemo.append(space (13) + QStringLiteral("|      L/B        |     ") + number (effectivelengthwaterline / beamwaterline , 0 , 6 , 3) + QStringLiteral("    |    ") + number (l_opt , 0 , 6 , 3) + QStringLiteral("    |"));
            resultsmemo.append(space (13) + QStringLiteral("| Cg=D/(Rho*g*B^3)|     ") + number (cg , 0 , 6 , 3) + QStringLiteral("    |    ") + number (cg_opt , 0 , 6 , 3) + QStringLiteral("    |"));
            resultsmemo.append(space (13) + QStringLiteral("|     Beta        |    ") + number (fgetle() , 0 , 6 , 3) + QStringLiteral("°   |   ") + number (be_opt , 0 , 6 , 3) + QStringLiteral("°   |"));
            resultsmemo.append(space (13) + QStringLiteral("|   Xg=LCG/L      |     ") + number (xg / effectivelengthwaterline , 0 , 6 , 3) + QStringLiteral("    |    ") + number (xg_opt , 0 , 6 , 3) + QStringLiteral("    |"));
            resultsmemo.append(space (13) + tr(QStringLiteral("rs_t______________________________________________t")));
        }
    else
        resultsmemo.append(space (13) + QStringLiteral(" ") + tr(QStringLiteral("rs_estimated_frv_")) + QStringLiteral(" ") + number (frvr , 0 , 6 , 3));
    if ((cg > 0.5f) && (i5 == 1))
        resultsmemo.append(space (13) + QStringLiteral(" ") + tr(QStringLiteral("rs_using_of_spray_strips_with_cg__0_5_inappropriate___")));
    if (field(QStringLiteral("combobox1")) . itemindex == 0)
        {
            if ((i4 == 1) && (field(QStringLiteral("combobox")) . itemindex == 0))
                {
                    if ((ii > 0) && (frvr > 4))
                        resultsmemo.append(space (10) + tr(QStringLiteral("rs_frv_is_outside_valid_domain_1_____4___used_extrapolation_for_r_ap")));
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_note__appendage_resistance_is_calculated_for_stationary_engine")));
                }
            resultsmemo.append(QStringLiteral(" "));
            if (field(QStringLiteral("combobox")) . itemindex == 1)
                ro = ro / 63.4297;
            resultsmemo.append(QStringLiteral(" "));
            resultsmemo.append(space (10) + QStringLiteral(" C.W.F.Hamilton & Co. type of waterjet calculation, according to [1,2]:"));
            resultsmemo.append(QStringLiteral(" "));
            resultsmemo.append(pascalFormat(QStringLiteral("           Vs   = %-6.2f %s = %-6.3f %s = %-6.2f %s"), {QVariant(speed), QVariant(tr(QStringLiteral("rs_kn"))), QVariant(speed * 0.51444), QVariant(tr(QStringLiteral("rs_m_s"))), QVariant(speed * 1.852), QVariant(tr(QStringLiteral("rs_km_hour")))}));
            resultsmemo.append(QStringLiteral(" "));
            if (speed > 45)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_attention_____vs_is_out_valid_domain_0_____40_knots_for_used_diagram_")));
            resultsmemo.append(QStringLiteral(" "));
            if ((settings. projectunits == 1) && (field(QStringLiteral("combobox")) . itemindex == 1))
                ro = ro * 63.4297;
            for (nwj=1; nwj<=4; ++nwj)
            {
                check();
                {
                    if (field(QStringLiteral("combobox")) . itemindex == 1)
                        {
                            ct = rt / ro / 0.0177 / square (speed * 0.51444) * 4.448222 / nwj;
                        }
                    else
                        {
                            ct = rt / ro / 0.0177 / square (speed * 0.51444) / nwj;
                            if (field(QStringLiteral("combobox")) . itemindex == 2)
                                ct = ct * 1000;
                        }
                    if (ct <= 0)
                        continue;
                    resultsmemo.append(pascalFormat(QStringLiteral("           Nwj          = %-6,0f"), {QVariant(nwj)}));
                    resultsmemo.append(pascalFormat(QStringLiteral("           Ct           = %-6,3f"), {QVariant(ct)}));
                    ct = logp (ct);
                    cpw = ((((0.0014886 * ct - 0.0074414) * ct - 0.0271693) * ct + 0.2261992) * ct + 1.0189592) * ct + 0.2336918;
                    cpw = power (10 , cpw) * 1.06;
                    resultsmemo.append(pascalFormat(QStringLiteral("           Cp           = %-6,3f"), {QVariant(fgetcp())}));
                    if (cpw == 0)
                        {
                            resultsmemo.append(space (10) + QStringLiteral(" Problem: Cp = 0"));
                            continue;
                        }
                    ne = cpw * ro * 0.0177 * power (speed * 0.51444 , 3);
                    n9 = power (ne / 736 / (332.94798 / power (2992.278 / 60 , 3)) , 0.3333);
                    n10 = power (ne / 736 / (369.17148 / power (2988.417 / 60 , 3)) , 0.3333);
                    n12 = power (ne / 736 / (400.771 / power (2934.3629 / 60 , 3)) , 0.3333);
                    n13 = power (ne / 736 / (400.000f / power (2826.2548 / 60 , 3)) , 0.3333);
                    n14 = power (ne / 736 / (400.771 / power (2779.9228 / 60 , 3)) , 0.3333);
                    n15 = power (ne / 736 / (400.771 / power (2725.8687 / 60 , 3)) , 0.3333);
                    etaw = rt * speed * 0.51444 / ne / nwj;
                    if (field(QStringLiteral("combobox")) . itemindex == 1)
                        etaw = etaw * 4.448222;
                    if (field(QStringLiteral("combobox")) . itemindex == 2)
                        etaw = etaw * 1000;
                    resultsmemo.append(space (10) + QStringLiteral(" EtaW         = ") + number (etaw , 0 , 6 , 3));
                    resultsmemo.append(space (10) + QStringLiteral(" Power (P)    = ") + number (ne / 1000 , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_kw")) + QStringLiteral(" = ") + number (ne / 1000 / 0.736 , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_bhp")));
                    if (ne / 1000 > 450)
                        resultsmemo.append(space (10) + tr(QStringLiteral("rs_attention_____power_is_out_valid_domain_30_____400_bhp_per_unit_for_used_diagram_")));
                    resultsmemo.append(space (11) + tr(QStringLiteral("rs_impeller_type_")) + QStringLiteral("    9    11    12    13    14    15"));
                    resultsmemo.append(space (10) + QStringLiteral(" RPS (n)      = ") + number (n9 , 0 , 6 , 1) + QStringLiteral("  ") + number (n10 , 0 , 6 , 1) + QStringLiteral("  ") + number (n12 , 0 , 6 , 1) + QStringLiteral("  ") + number (n13 , 0 , 6 , 1) + QStringLiteral("  ") + number (n14 , 0 , 6 , 1) + QStringLiteral("  ") + number (n15 , 0 , 6 , 1));
                    resultsmemo.append(space (10) + QStringLiteral(" RPM (n)      = ") + number (n9 * 60 , 0 , 6 , 0) + QStringLiteral("  ") + number (n10 * 60 , 0 , 6 , 0) + QStringLiteral("  ") + number (n12 * 60 , 0 , 6 , 0) + QStringLiteral("  ") + number (n13 * 60 , 0 , 6 , 0) + QStringLiteral("  ") + number (n14 * 60 , 0 , 6 , 0) + QStringLiteral("  ") + number (n15 * 60 , 0 , 6 , 0));
                    if (nwj > 1)
                        resultsmemo.append(space (10) + QStringLiteral(" Power*Nwj    = ") + number (ne / 1000 * nwj , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_kw")) + QStringLiteral(" = ") + number (ne / 1000 / 0.736 * nwj , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_bhp")));
                    resultsmemo.append(QStringLiteral(" "));
                }
            }
            resultsmemo.append(QStringLiteral(" "));
            resultsmemo.append(space (10) + QStringLiteral(" REFERENCES:"));
            resultsmemo.append(QStringLiteral(" "));
            resultsmemo.append(space (10) + QStringLiteral("1. Donald M. MacPherson.- A Universal Parametric Model for Waterjet Performance,"));
            resultsmemo.append(space (10) + QStringLiteral("   HydroComp, Inc., Presented at FAST-99, Aug 1999."));
            resultsmemo.append(space (10) + QStringLiteral("2. HydroComp, Inc., \"Waterjet Data Files for NavCad\", HCI Report No. 119,"));
            resultsmemo.append(space (10) + QStringLiteral("   December 1997."));
        }
    else
        warning(tr(QStringLiteral("rs_data_for_propulsion_saved_")));
    resultsmemo.append(QStringLiteral(" "));
    resultsmemo.append(space (10) + QStringLiteral("Copyright (c) 2008-2012, Timoshenko V.F."));
    if (field(QStringLiteral("combobox")) . itemindex == 3)
        resultsmemo.append(space (10) + QStringLiteral("Copyright (c) 2010, Le Quoc Van (HydroNShIp_ClemPoup v1.0)"));
    if (field(QStringLiteral("combobox")) . itemindex == 4)
        resultsmemo.append(space (10) + QStringLiteral("Copyright (c) 2010, Le Quoc Van (HydroNShIp_ClemBlou v1.0)"));
    check();
}

bool Resistance_planing::execute() {
bool result{};
int rightaxis{};
    if (field(QStringLiteral("combobox")) . itemindex == 0)
        ; // presentation only
    if (field(QStringLiteral("combobox")) . itemindex == 1)
        ; // presentation only
    if (field(QStringLiteral("combobox")) . itemindex == 2)
        ; // presentation only
    yTitle = tr(QStringLiteral("rs_resistanceo10__kn___power__kw_"));
    xTitle = tr(QStringLiteral("rs_speed")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kn"));
    ;
    rightTitle = tr(QStringLiteral("rs_trim_angle_tau__degr_"));
    field(QStringLiteral("checkbox2")) . checked = false;
    field(QStringLiteral("label14")) . caption = tr(QStringLiteral("rs_t"));
    field(QStringLiteral("label16")) . caption = tr(QStringLiteral("rs_degr"));
    field(QStringLiteral("label21_")) . caption = tr(QStringLiteral("rs_knots"));
    field(QStringLiteral("label23_")) . caption = tr(QStringLiteral("rs_degr"));
    field(QStringLiteral("label26_")) . caption = tr(QStringLiteral("rs_degr"));
    field(QStringLiteral("label27_")) . caption = tr(QStringLiteral("rs_degr"));
    selectcalcmethod();
    if (settings. projectunits != 1)
        {
            field(QStringLiteral("label9")) . caption = tr(QStringLiteral("rs_m"));
            field(QStringLiteral("label10")) . caption = tr(QStringLiteral("rs_m"));
            field(QStringLiteral("label11")) . caption = tr(QStringLiteral("rs_m"));
            field(QStringLiteral("label7")) . caption = tr(QStringLiteral("rs_m_2"));
            field(QStringLiteral("label17_")) . caption = tr(QStringLiteral("rs_m"));
            field(QStringLiteral("label24_")) . caption = tr(QStringLiteral("rs_m"));
            field(QStringLiteral("label25_")) . caption = tr(QStringLiteral("rs_m"));
            field(QStringLiteral("label19_")) . caption = tr(QStringLiteral("rs_m_2"));
        }
    else
        {
            field(QStringLiteral("label9")) . caption = tr(QStringLiteral("rs_ft"));
            field(QStringLiteral("label10")) . caption = tr(QStringLiteral("rs_ft"));
            field(QStringLiteral("label11")) . caption = tr(QStringLiteral("rs_ft"));
            field(QStringLiteral("label7")) . caption = tr(QStringLiteral("rs_ft_2"));
            field(QStringLiteral("label17_")) . caption = tr(QStringLiteral("rs_ft"));
            field(QStringLiteral("label24_")) . caption = tr(QStringLiteral("rs_ft"));
            field(QStringLiteral("label25_")) . caption = tr(QStringLiteral("rs_ft"));
            field(QStringLiteral("label19_")) . caption = tr(QStringLiteral("rs_ft_2"));
        }
    i3 = 0;
    i4 = 0;
    i5 = 0;
    i6 = 0;
    result = false;
    return result;
}
}
