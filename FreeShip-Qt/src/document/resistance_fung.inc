// GPL-2.0-or-later. Generated from FreeResistance_FungLeibDlg.pas.
namespace {

struct Resistance_fung final: ResistanceBase {
using ResistanceBase::ResistanceBase;
float b_t{};
float l_d{};
float a_d{};
float l_b{};
float vsr{};
float rtotal{};
float am{};
float cm{};
float cwp{};
float s_{};
float ie_{};
float ie{};
float z0{};
float tc{};
float ae0{};
float p_d0{};
float nver{};
QString pathfile{};
QString pathfileold{};
QString filetofind{};
QString filename{};
float wlarea{};
bool correctinputdata();
float fgetstartspeed();
void fsetstartspeed(float val);
float fgetendspeed();
void fsetendspeed(float val);
float fgetdensity();
void fsetdensity(float val);
float fgetdisplacement();
void fsetdisplacement(float val);
float fgetdraft();
void fsetdraft(float val);
float fgetdrafttotal();
void fsetdrafttotal(float val);
float fgetlwl();
void fsetlwl(float val);
float fgetlcb();
void fsetlcb(float val);
void fsetke(float val);
float fgetke();
void fsetk1(float val);
float fgetk1();
void fsetk2(float val);
float fgetk2();
void fsetk3(float val);
float fgetk3();
void fsetk4(float val);
float fgetk4();
void fsetk5(float val);
float fgetk5();
void fsetk6(float val);
float fgetk6();
void fsetk7(float val);
float fgetk7();
void fseta1(float val);
float fgeta1();
void fseta2(float val);
float fgeta2();
void fseta3(float val);
float fgeta3();
void fseta4(float val);
float fgeta4();
void fseta5(float val);
float fgeta5();
void fseta6(float val);
float fgeta6();
void fseta7(float val);
float fgeta7();
void fseta8(float val);
float fgeta8();
void fseta9(float val);
float fgeta9();
void fseta10(float val);
float fgeta10();
void fseta11(int val);
int fgeta11();
void fsetba(float val);
float fgetba();
void fsetkbulb(float val);
float fgetkbulb();
void fsetzbulb(float val);
float fgetzbulb();
void fsetcstrn(float val);
float fgetcstrn();
void fsetnp(float val);
float fgetnp();
void fsetdp(float val);
float fgetdp();
void fsetks(float val);
float fgetks();
float fgetkeelchordlength();
void fsetkeelchordlength(float val);
float fgetkeelarea();
void fsetkeelarea(float val);
float fgetrudderchordlength();
void fsetrudderchordlength(float val);
float fgetrudderarea();
void fsetrudderarea(float val);
float fgetbwl();
void fsetbwl(float val);
float fgetcp();
void fsetcp(float val);
float fgetviscosity();
void fsetviscosity(float val);
float fgetwettedsurface();
void fsetwettedsurface(float val);
float fgetlos();
void fsetlos(float val);
float fgetstepspeed();
void fsetstepspeed(float val);
bool fgetextractfromhull();
void fsetextractfromhull(bool val);
void checkbox2click(void* sender);
void drafttotalboxaftersetvalue(void* sender);
void estimateboxclick(void* sender);
void estimate2boxclick(void* sender);
void calculateresistancefungleib(float convertedspeed, float lcb, float cp, float &rf, float &rr, float &w, float &t0, float &nr);
void file_exportdata(Span<float> dat, Span<float> dan);
void file_exportdatake(Span<float> dat, Span<float> dan);
void calculateinworkspace();
bool execute();
void loadProperty(const QString &member,float value) override {
if (member=="projectwaterdensity") { fsetdensity(value); return; }
if (member==QStringLiteral("bwl")) { fsetbwl(value); return; }
if (member==QStringLiteral("cp")) { fsetcp(value); return; }
if (member==QStringLiteral("density")) { fsetdensity(value); return; }
if (member==QStringLiteral("displacement")) { fsetdisplacement(value); return; }
if (member==QStringLiteral("draft")) { fsetdraft(value); return; }
if (member==QStringLiteral("drafttotal")) { fsetdrafttotal(value); return; }
if (member==QStringLiteral("endspeed")) { fsetendspeed(value); return; }
if (member==QStringLiteral("keelchordlength")) { fsetkeelchordlength(value); return; }
if (member==QStringLiteral("keelarea")) { fsetkeelarea(value); return; }
if (member==QStringLiteral("lcb")) { fsetlcb(value); return; }
if (member==QStringLiteral("ke")) { fsetke(value); return; }
if (member==QStringLiteral("k1")) { fsetk1(value); return; }
if (member==QStringLiteral("k2")) { fsetk2(value); return; }
if (member==QStringLiteral("k3")) { fsetk3(value); return; }
if (member==QStringLiteral("k4")) { fsetk4(value); return; }
if (member==QStringLiteral("k5")) { fsetk5(value); return; }
if (member==QStringLiteral("k6")) { fsetk6(value); return; }
if (member==QStringLiteral("k7")) { fsetk7(value); return; }
if (member==QStringLiteral("a1")) { fseta1(value); return; }
if (member==QStringLiteral("a2")) { fseta2(value); return; }
if (member==QStringLiteral("a3")) { fseta3(value); return; }
if (member==QStringLiteral("a4")) { fseta4(value); return; }
if (member==QStringLiteral("a5")) { fseta5(value); return; }
if (member==QStringLiteral("a6")) { fseta6(value); return; }
if (member==QStringLiteral("a7")) { fseta7(value); return; }
if (member==QStringLiteral("a8")) { fseta8(value); return; }
if (member==QStringLiteral("a9")) { fseta9(value); return; }
if (member==QStringLiteral("a10")) { fseta10(value); return; }
if (member==QStringLiteral("a11")) { fseta11(value); return; }
if (member==QStringLiteral("ba")) { fsetba(value); return; }
if (member==QStringLiteral("kbulb")) { fsetkbulb(value); return; }
if (member==QStringLiteral("zbulb")) { fsetzbulb(value); return; }
if (member==QStringLiteral("cstrn")) { fsetcstrn(value); return; }
if (member==QStringLiteral("np")) { fsetnp(value); return; }
if (member==QStringLiteral("dp")) { fsetdp(value); return; }
if (member==QStringLiteral("ks")) { fsetks(value); return; }
if (member==QStringLiteral("lwl")) { fsetlwl(value); return; }
if (member==QStringLiteral("rudderchordlength")) { fsetrudderchordlength(value); return; }
if (member==QStringLiteral("rudderarea")) { fsetrudderarea(value); return; }
if (member==QStringLiteral("startspeed")) { fsetstartspeed(value); return; }
if (member==QStringLiteral("stepspeed")) { fsetstepspeed(value); return; }
if (member==QStringLiteral("viscosity")) { fsetviscosity(value); return; }
if (member==QStringLiteral("wettedsurface")) { fsetwettedsurface(value); return; }
if (member==QStringLiteral("los")) { fsetlos(value); return; }
ResistanceBase::loadProperty(member,value);
}
void booleanChanged(const QString &key) override { if (key==QStringLiteral("estimatebox")) estimateboxclick(nullptr);if (key==QStringLiteral("estimate2box")) estimate2boxclick(nullptr); }
void extract() override { field("checkbox2").checked=true;checkbox2click(nullptr);field("checkbox2").checked=false;checkbox2click(nullptr); }
void initialise() override { execute(); }
void choiceChanged(const QString &key) override {  }
void runCalculation() override { calculateinworkspace(); }
};
bool Resistance_fung::correctinputdata() {
bool result{};

    result = false;
    if ((fgetdraft() <= 0.02))
        fsetdraft(0.021);
    if ((fgetlwl() <= 0))
        ; // presentation only
    else
        ; // presentation only
    if ((fgetbwl() <= 0))
        ; // presentation only
    else
        ; // presentation only
    if ((fgetdrafttotal() < fgetdraft()))
        fsetdraft(fgetdrafttotal());
    if ((fgetwettedsurface() <= 0))
        ; // presentation only
    else
        ; // presentation only
    if ((fgetdisplacement() <= 0))
        ; // presentation only
    else
        ; // presentation only
    if ((fgetcp() <= 0))
        ; // presentation only
    else
        ; // presentation only
    if ((fgetviscosity() <= 0))
        ; // presentation only
    else
        ; // presentation only
    if ((fgetendspeed() <= 0.0f))
        fsetendspeed(10.0f);
    if ((fgetendspeed() <= fgetstartspeed()))
        ; // presentation only
    else
        ; // presentation only
    if ((fgetstepspeed() <= 0))
        fsetstepspeed(10.0f);
    if ((fgetdrafttotal() <= 0.02) || (fgetlwl() <= 0) || (fgetbwl() <= 0) || (fgetlos() <= 0) || (fgetdisplacement() <= 0) || (fgetcp() <= 0) || (fgetviscosity() <= 0) || (fgetendspeed() <= fgetstartspeed()))
        return result;
    if (tc == 0)
        tc = fgetdrafttotal();
    b_t = fgetbwl() / tc;
    l_d = fgetlwl() / power (fgetdisplacement() , 1.0 / 3);
    a_d = wlarea / power (fgetdisplacement() , 2.0 / 3);
    l_b = fgetlwl() / fgetbwl();
    cm = fgetcp() * (fgetlwl() * fgetbwl() * tc) / fgetdisplacement();
    am = cm * tc * fgetbwl();
    if (cm >= 1.f)
        {
            am = fgetdisplacement() / (fgetlwl() * fgetcp());
            cm = am / (fgetbwl() * tc);
        }
    cwp = wlarea / (fgetlwl() * fgetbwl());
    if ((cwp == 1) || (fgetcp() == 1))
        {
            warning(tr(QStringLiteral("rs_error__cp_1_or_cwp_1")));
            return result;
        }
    result = true;
    return result;
}

float Resistance_fung::fgetstartspeed() {
float result{};

    result = field(QStringLiteral("startspeedbox")) . value;
    return result;
}

void Resistance_fung::fsetstartspeed(float val) {

    field(QStringLiteral("startspeedbox")).set(val);
}

float Resistance_fung::fgetendspeed() {
float result{};

    result = field(QStringLiteral("endspeedbox")) . value;
    return result;
}

void Resistance_fung::fsetendspeed(float val) {

    field(QStringLiteral("endspeedbox")).set(val);
}

float Resistance_fung::fgetdensity() {
float result{};

    result = field(QStringLiteral("densitybox")) . value;
    return result;
}

void Resistance_fung::fsetdensity(float val) {

    field(QStringLiteral("densitybox")).set(val);
}

float Resistance_fung::fgetdisplacement() {
float result{};

    result = field(QStringLiteral("displacementbox")) . value;
    return result;
}

void Resistance_fung::fsetdisplacement(float val) {

    field(QStringLiteral("displacementbox")).set(val);
}

float Resistance_fung::fgetdraft() {
float result{};

    result = field(QStringLiteral("draftbox")) . value;
    return result;
}

void Resistance_fung::fsetdraft(float val) {

    field(QStringLiteral("draftbox")).set(val);
}

float Resistance_fung::fgetdrafttotal() {
float result{};

    result = field(QStringLiteral("drafttotalbox")) . value;
    return result;
}

void Resistance_fung::fsetdrafttotal(float val) {

    field(QStringLiteral("drafttotalbox")).set(val);
}

float Resistance_fung::fgetlwl() {
float result{};

    result = field(QStringLiteral("lwlbox")) . value;
    return result;
}

void Resistance_fung::fsetlwl(float val) {

    field(QStringLiteral("lwlbox")).set(val);
}

float Resistance_fung::fgetlcb() {
float result{};

    result = field(QStringLiteral("lcbbox")) . value;
    return result;
}

void Resistance_fung::fsetlcb(float val) {

    field(QStringLiteral("lcbbox")).set(val);
}

void Resistance_fung::fsetke(float val) {

    field(QStringLiteral("kebox")).set(val);
    if (field(QStringLiteral("kebox")) . value == 0)
        field(QStringLiteral("kebox")).set(1);
}

float Resistance_fung::fgetke() {
float result{};

    result = field(QStringLiteral("kebox")) . value;
    return result;
}

void Resistance_fung::fsetk1(float val) {

    field(QStringLiteral("k1box")).set(val);
}

float Resistance_fung::fgetk1() {
float result{};

    result = field(QStringLiteral("k1box")) . value;
    return result;
}

void Resistance_fung::fsetk2(float val) {

    field(QStringLiteral("k2box")).set(val);
}

float Resistance_fung::fgetk2() {
float result{};

    result = field(QStringLiteral("k2box")) . value;
    return result;
}

void Resistance_fung::fsetk3(float val) {

    field(QStringLiteral("k3box")).set(val);
}

float Resistance_fung::fgetk3() {
float result{};

    result = field(QStringLiteral("k3box")) . value;
    return result;
}

void Resistance_fung::fsetk4(float val) {

    field(QStringLiteral("k4box")).set(val);
}

float Resistance_fung::fgetk4() {
float result{};

    result = field(QStringLiteral("k4box")) . value;
    return result;
}

void Resistance_fung::fsetk5(float val) {

    field(QStringLiteral("k5box")).set(val);
}

float Resistance_fung::fgetk5() {
float result{};

    result = field(QStringLiteral("k5box")) . value;
    return result;
}

void Resistance_fung::fsetk6(float val) {

    field(QStringLiteral("k6box")).set(val);
}

float Resistance_fung::fgetk6() {
float result{};

    result = field(QStringLiteral("k6box")) . value;
    return result;
}

void Resistance_fung::fsetk7(float val) {

    field(QStringLiteral("k7box")).set(val);
}

float Resistance_fung::fgetk7() {
float result{};

    result = field(QStringLiteral("k7box")) . value;
    return result;
}

void Resistance_fung::fseta1(float val) {

    field(QStringLiteral("a1box")).set(val);
}

float Resistance_fung::fgeta1() {
float result{};

    result = field(QStringLiteral("a1box")) . value;
    return result;
}

void Resistance_fung::fseta2(float val) {

    field(QStringLiteral("a2box")).set(val);
}

float Resistance_fung::fgeta2() {
float result{};

    result = field(QStringLiteral("a2box")) . value;
    return result;
}

void Resistance_fung::fseta3(float val) {

    field(QStringLiteral("a3box")).set(val);
}

float Resistance_fung::fgeta3() {
float result{};

    result = field(QStringLiteral("a3box")) . value;
    return result;
}

void Resistance_fung::fseta4(float val) {

    field(QStringLiteral("a4box")).set(val);
}

float Resistance_fung::fgeta4() {
float result{};

    result = field(QStringLiteral("a4box")) . value;
    return result;
}

void Resistance_fung::fseta5(float val) {

    field(QStringLiteral("a5box")).set(val);
}

float Resistance_fung::fgeta5() {
float result{};

    result = field(QStringLiteral("a5box")) . value;
    return result;
}

void Resistance_fung::fseta6(float val) {

    field(QStringLiteral("a6box")).set(val);
}

float Resistance_fung::fgeta6() {
float result{};

    result = field(QStringLiteral("a6box")) . value;
    return result;
}

void Resistance_fung::fseta7(float val) {

    field(QStringLiteral("a7box")).set(1.226);
}

float Resistance_fung::fgeta7() {
float result{};

    result = field(QStringLiteral("a7box")) . value;
    return result;
}

void Resistance_fung::fseta8(float val) {

    field(QStringLiteral("a8box")).set(val);
}

float Resistance_fung::fgeta8() {
float result{};

    result = field(QStringLiteral("a8box")) . value;
    return result;
}

void Resistance_fung::fseta9(float val) {

    field(QStringLiteral("a9box")).set(val);
}

float Resistance_fung::fgeta9() {
float result{};

    result = field(QStringLiteral("a9box")) . value;
    return result;
}

void Resistance_fung::fseta10(float val) {

    field(QStringLiteral("a10box")).set(val);
}

float Resistance_fung::fgeta10() {
float result{};

    result = field(QStringLiteral("a10box")) . value;
    return result;
}

void Resistance_fung::fseta11(int val) {

    field(QStringLiteral("a11box")) . itemindex = val - 1;
}

int Resistance_fung::fgeta11() {
int result{};

    result = field(QStringLiteral("a11box")) . itemindex + 1;
    return result;
}

void Resistance_fung::fsetba(float val) {

    field(QStringLiteral("babox")).set(val);
}

float Resistance_fung::fgetba() {
float result{};

    result = field(QStringLiteral("babox")) . value;
    return result;
}

void Resistance_fung::fsetkbulb(float val) {

    field(QStringLiteral("kbulbbox")).set(val);
}

float Resistance_fung::fgetkbulb() {
float result{};

    result = field(QStringLiteral("kbulbbox")) . value;
    return result;
}

void Resistance_fung::fsetzbulb(float val) {

    field(QStringLiteral("zbulbbox")).set(val);
}

float Resistance_fung::fgetzbulb() {
float result{};

    result = field(QStringLiteral("zbulbbox")) . value;
    return result;
}

void Resistance_fung::fsetcstrn(float val) {

    field(QStringLiteral("cstrnbox")).set(val);
}

float Resistance_fung::fgetcstrn() {
float result{};

    result = field(QStringLiteral("cstrnbox")) . value;
    if ((field(QStringLiteral("cstrnbox")) . value == 0) && (fgetnp() != 0))
        field(QStringLiteral("cstrnbox")).set(2);
    return result;
}

void Resistance_fung::fsetnp(float val) {

    field(QStringLiteral("npbox")).set(val);
}

float Resistance_fung::fgetnp() {
float result{};

    result = field(QStringLiteral("npbox")) . value;
    if (field(QStringLiteral("npbox")) . value > 2)
        field(QStringLiteral("npbox")).set(2);
    return result;
}

void Resistance_fung::fsetdp(float val) {

    field(QStringLiteral("dpbox")).set(val);
}

float Resistance_fung::fgetdp() {
float result{};

    result = field(QStringLiteral("dpbox")) . value;
    if ((field(QStringLiteral("dpbox")) . value == 0) && (fgetnp() == 1))
        field(QStringLiteral("dpbox")).set(fgetdrafttotal() * 0.75f);
    if ((field(QStringLiteral("dpbox")) . value == 0) && (fgetnp() == 2))
        field(QStringLiteral("dpbox")).set(fgetdrafttotal() * 0.7);
    return result;
}

void Resistance_fung::fsetks(float val) {

    field(QStringLiteral("ksbox")).set(val);
}

float Resistance_fung::fgetks() {
float result{};

    result = field(QStringLiteral("ksbox")) . value;
    return result;
}

float Resistance_fung::fgetkeelchordlength() {
float result{};

    result = field(QStringLiteral("keelchordlengthbox")) . value;
    return result;
}

void Resistance_fung::fsetkeelchordlength(float val) {

    field(QStringLiteral("keelchordlengthbox")).set(val);
}

float Resistance_fung::fgetkeelarea() {
float result{};

    result = field(QStringLiteral("keelareabox")) . value;
    return result;
}

void Resistance_fung::fsetkeelarea(float val) {

    field(QStringLiteral("keelareabox")).set(val);
}

float Resistance_fung::fgetrudderchordlength() {
float result{};

    result = field(QStringLiteral("rudderchordlengthbox")) . value;
    return result;
}

void Resistance_fung::fsetrudderchordlength(float val) {

    field(QStringLiteral("rudderchordlengthbox")).set(val);
}

float Resistance_fung::fgetrudderarea() {
float result{};

    result = field(QStringLiteral("rudderareabox")) . value;
    return result;
}

void Resistance_fung::fsetrudderarea(float val) {

    field(QStringLiteral("rudderareabox")).set(val);
}

float Resistance_fung::fgetbwl() {
float result{};

    result = field(QStringLiteral("bwlbox")) . value;
    return result;
}

void Resistance_fung::fsetbwl(float val) {

    field(QStringLiteral("bwlbox")).set(val);
}

float Resistance_fung::fgetcp() {
float result{};

    result = field(QStringLiteral("cpbox")) . value;
    return result;
}

void Resistance_fung::fsetcp(float val) {

    field(QStringLiteral("cpbox")).set(val);
}

float Resistance_fung::fgetviscosity() {
float result{};

    result = field(QStringLiteral("viscositybox")) . value;
    return result;
}

void Resistance_fung::fsetviscosity(float val) {

    field(QStringLiteral("viscositybox")).set(val);
}

float Resistance_fung::fgetwettedsurface() {
float result{};
float c23{};
float scbfact{};
float am{};
float cm{};
float cwp{};
    if (field(QStringLiteral("estimatebox")) . checked)
        {
            if ((fgetlwl() > 0) && (fgetbwl() > 0) && (fgetcp() > 0) && (fgetdisplacement() > 0))
                {
                    am = fgetdisplacement() / (fgetlwl() * fgetcp());
                    cm = am / (fgetbwl() * tc);
                    cwp = wlarea / (fgetlwl() * fgetbwl());
                    c23 = 0.453 + 0.443 * (fgetcp() * cm) - 0.286 * cm - 0.00347 * (fgetbwl() / tc) + 0.37 * cwp;
                    scbfact = 0.616 * c23 + 0.111 * cm * cm * cm + 0.245 * (c23 / cm) - 0.0228;
                    result = scbfact * fgetlwl() * (2 * tc + fgetbwl()) * sqrtp (cm);
                }
            else
                result = 0;
        }
    else
        result = field(QStringLiteral("wettedsurfacebox")) . value;
    return result;
}

void Resistance_fung::fsetwettedsurface(float val) {

    field(QStringLiteral("wettedsurfacebox")).set(val);
}

float Resistance_fung::fgetlos() {
float result{};

    result = field(QStringLiteral("losbox")) . value;
    return result;
}

void Resistance_fung::fsetlos(float val) {

    field(QStringLiteral("losbox")).set(val);
}

float Resistance_fung::fgetstepspeed() {
float result{};

    result = field(QStringLiteral("stepspeedbox")) . value;
    return result;
}

void Resistance_fung::fsetstepspeed(float val) {

    field(QStringLiteral("stepspeedbox")).set(val);
}

bool Resistance_fung::fgetextractfromhull() {
bool result{};

    result = field(QStringLiteral("checkbox2")) . checked;
    return result;
}

void Resistance_fung::fsetextractfromhull(bool val) {

    if (field(QStringLiteral("checkbox2")) . checked != val)
        field(QStringLiteral("checkbox2")) . checked = val;
}

void Resistance_fung::checkbox2click(void* sender) {

    field(QStringLiteral("lwlbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("bwlbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("wettedsurfacebox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("losbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("displacementbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("lcbbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("cpbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("kbulbbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("draftbox")) . enabled = false;
    field(QStringLiteral("estimatebox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    if (field(QStringLiteral("checkbox2")) . checked)
        {
            field(QStringLiteral("estimatebox")) . checked = false;
            if (fgetdrafttotal() == 0.0f)
                fsetdrafttotal(settings. projectdraft);
            if (this != nullptr)
                fsetdensity(settings. projectwaterdensity);
            drafttotalboxaftersetvalue (nullptr);
        }
}

void Resistance_fung::drafttotalboxaftersetvalue(void* sender) {
HydroInput hydobject{};
    if ((field(QStringLiteral("checkbox2")) . checked) && (this != nullptr))
        {
            ; // local immutable hydrostatic request
            hydobject . draft = fgetdrafttotal();
            hydobject.data = hydro(hydobject.draft);
            fsetlwl(hydValue(hydobject, QStringLiteral("lengthwaterline")));
            fsetlos(hydValue(hydobject, QStringLiteral("submax.x")) - hydValue(hydobject, QStringLiteral("submin.x")));
            fsetbwl(hydValue(hydobject, QStringLiteral("beamwaterline")));
            z0 = hydValue(hydobject, QStringLiteral("modelmin.z"));
            tc = fgetdrafttotal() + z0;
            fsetwettedsurface(hydValue(hydobject, QStringLiteral("wettedsurface")));
            wlarea = hydValue(hydobject, QStringLiteral("waterplanearea"));
            ie_ = hydValue(hydobject, QStringLiteral("waterplaneentranceangle"));
            fsetkbulb(ie_);
            fsetdisplacement(hydValue(hydobject, QStringLiteral("volume")));
            if (fgetlwl() > 0)
                fsetlcb(100 * (hydValue(hydobject, QStringLiteral("centerofbuoyancy.x")) - (hydValue(hydobject, QStringLiteral("wlmin.x")) + 0.5f * fgetlwl())) / fgetlwl());
            else
                fsetlcb(0);
            fsetcp(hydValue(hydobject, QStringLiteral("prismcoefficient")));
            check();
        }
}

void Resistance_fung::estimateboxclick(void* sender) {

    field(QStringLiteral("wettedsurfacebox")) . enabled = ! field(QStringLiteral("estimatebox")) . checked;
}

void Resistance_fung::estimate2boxclick(void* sender) {

    if (fgeta1() == 0)
        fseta1(6.f);
    if (fgeta2() == 0)
        fseta2(1.f);
    if (fgeta4() == 0)
        fseta4(1.f);
    if (fgeta6() == 0)
        fseta6(0.5f);
    if (fgeta7() == 0)
        fseta7(1.226);
    if (fgeta8() == 0)
        fseta8(1.f);
    if (fgeta9() == 0)
        fseta9(100.f);
    fseta10(cm * fgetbwl() * tc);
    if (fgeta11() == 0)
        fseta11(1);
}

void Resistance_fung::calculateresistancefungleib(float convertedspeed, float lcb, float cp, float &rf, float &rr, float &w, float &t0, float &nr) {
float rn_keel{};
float cf_keel{};
float rf_keel{};
float vs{};
float ke_{};
float c_{};
float rn_rudder{};
float cf_rudder{};
float rf_rudder{};
float fraction{};
float froudenumber{};
float t{};
Array<float,1,30> dat{};
Array<float,1,15> dan{};
Array<double,0,9> factors{};
Array<float,1,4> m{};
Array<float,1,20> c{};
int a{};
int lower{};
float nscrew{};
float l{};
float b{};
float d{};
float ta{};
float tf{};
float cb{};
float s{};
float cp1{};
float cv{};
float nh{};
float cf{};
float re{};
float ca{};
float k1_{};
float lr{};
float v{};
float l_b{};
float b_t{};
float b_l{};
float l3v{};
float b_ta{};
float ta_d{};
float tf_l{};
float t_l{};
float t_b{};
float keel_chord{};
float keel_area{};
float k_1{};
float k_2{};
float k_3{};
float k_4{};
float k_5{};
float k_6{};
float k_7{};
float a_1{};
float a_2{};
float a_3{};
float a_4{};
float a_5{};
float a_6{};
float a_7{};
float a_8{};
float a_9{};
float a_10{};
float a_11{};
float rudder_chord{};
float rudder_area{};
float lengthwaterline{};
float waterdensity{};
float waterviscosity{};
float displ{};
float wetarea{};
    rf = 0.0f;
    rr = 0.0f;
    nscrew = fgetnp();
    if (fgetnp() < 0)
        nscrew = 0;
    if (fgetnp() > 2)
        nscrew = 2;
    if (convertedspeed <= 0)
        return;
    if (settings. projectunits == 1)
        {
            lengthwaterline = fgetlwl() * foot;
            waterdensity = fgetdensity() / weightconversionfactor;
            keel_chord = fgetkeelchordlength() * foot;
            keel_area = fgetkeelarea() * foot * foot;
            k_1 = fgetk1() * foot * foot;
            k_2 = fgetk2() * foot * foot;
            k_3 = fgetk3() * foot * foot;
            k_4 = fgetk4() * foot * foot;
            k_5 = fgetk5() * foot * foot;
            k_6 = fgetk6() * foot * foot;
            k_7 = fgetk7() * foot * foot;
            rudder_chord = fgetrudderchordlength() * foot;
            rudder_area = fgetrudderarea() * foot * foot;
            waterviscosity = fgetviscosity() * 0.3048 * 0.3048 * 1e-6;
            displ = fgetdisplacement() * foot * foot * foot;
            wetarea = fgetwettedsurface() * foot * foot;
            a_1 = fgeta1();
            a_2 = fgeta2() * foot;
            a_3 = fgeta3();
            a_4 = fgeta4() * foot;
            a_5 = fgeta5();
            a_6 = fgeta6() * foot;
            a_7 = fgeta7() / (foot * foot * foot);
            a_8 = fgeta8() * foot;
            a_9 = fgeta9() * foot;
            a_10 = fgeta10() * foot * foot;
            a_11 = fgeta11();
            s_ = s_ * foot * foot;
        }
    else
        {
            lengthwaterline = fgetlwl();
            waterdensity = fgetdensity();
            keel_chord = fgetkeelchordlength();
            keel_area = fgetkeelarea();
            k_1 = fgetk1();
            k_2 = fgetk2();
            k_3 = fgetk3();
            k_4 = fgetk4();
            k_5 = fgetk5();
            k_6 = fgetk6();
            k_7 = fgetk7();
            rudder_chord = fgetrudderchordlength();
            rudder_area = fgetrudderarea();
            waterviscosity = fgetviscosity() * 1e-6;
            displ = fgetdisplacement();
            wetarea = fgetwettedsurface();
            a_1 = fgeta1();
            a_2 = fgeta2();
            a_3 = fgeta3();
            a_4 = fgeta4();
            a_5 = fgeta5();
            a_6 = fgeta6();
            a_7 = fgeta7();
            a_8 = fgeta8();
            a_9 = fgeta9();
            a_10 = fgeta10();
            a_11 = fgeta11();
        }
    if (fgetke() == 0)
        {
            if ((a_9 <= 0) || (a_8 <= 0) || (a_6 <= 0) || (a_7 <= 0) || (a_11 <= 0))
                {
                    warning(tr(QStringLiteral("rslockedatarequired")));
                    return;
                }
            dat [1] = fgetlwl();
            dat [2] = fgetlwl();
            dat [3] = fgetbwl();
            dat [4] = tc + fgetdraft() / 2.f;
            dat [5] = tc - fgetdraft() / 2.f;
            dat [6] = fgetdisplacement();
            dat [7] = wetarea;
            if (dat [7] == 0)
                dat [7] = s_;
            dat [8] = lcb;
            dat [9] = cp * cm;
            dat [10] = cp;
            dat [11] = cwp;
            dat [12] = cm;
            dat [13] = fgetba();
            dat [14] = fgetkbulb();
            dat [15] = fgetzbulb();
            dat [16] = fgetdp();
            dat [17] = waterdensity;
            dat [18] = fgetviscosity();
            dat [19] = fgetcstrn();
            dat [20] = fgetnp();
            dat [21] = fgetke();
            dat [22] = fgetks();
            dat [23] = 0;
            dat [24] = 0;
            dat [25] = 0;
            dat [26] = 0;
            dat [27] = 0;
            dat [28] = 0;
            vs = convertedspeed;
            c_ = rtotal / (0.5f * waterdensity * square (vs) * wetarea);
            dat [29] = c_;
            dat [30] = convertedspeed;
            dan [1] = a_1;
            dan [2] = a_2;
            dan [3] = a_3;
            dan [4] = a_4;
            dan [5] = a_5;
            dan [6] = a_6;
            dan [7] = a_7;
            dan [8] = a_8;
            dan [9] = a_9;
            dan [10] = a_10;
            dan [11] = a_11;
            dan [12] = a_11;
            dan [13] = a_11;
            dan [14] = a_11;
            dan [15] = a_11;
            file_exportdatake (dat , dan);
        }
    dat [1] = fgetlwl();
    dat [2] = fgetbwl();
    dat [3] = tc;
    dat [4] = fgetdisplacement();
    dat [5] = cm;
    dat [6] = fgetkbulb();
    dat [7] = wetarea;
    if (dat [7] <= 0)
        dat [7] = s_;
    if (fgetba() < 0)
        fsetba(- fgetba());
    dat [8] = fgetba() / (fgetbwl() * tc * cm);
    if (dat [8] > 0.74)
        dat [8] = 0.74;
    dat [9] = fgetzbulb() / fgetbwl();
    if (dat [9] > 1)
        dat [9] = 1;
    dat [10] = waterdensity;
    dat [11] = fgetviscosity();
    dat [12] = fgetstartspeed();
    dat [13] = fgetendspeed();
    dat [14] = fgetstepspeed();
    dat [15] = nscrew;
    if (dat [15] > 2)
        dat [15] = 2;
    if (dat [15] < 1)
        dat [15] = 0;
    fsetnp(dat [15]);
    dat [16] = fgetcstrn();
    if (dat [16] > 2)
        dat [16] = 2;
    if (dat [16] < 1)
        dat [16] = 0;
    fsetcstrn(dat [16]);
    dat [17] = 3;
    dat [18] = 4;
    dat [19] = 5;
    dat [20] = 6;
    dat [21] = 7;
    dat [22] = 8;
    dat [23] = 9;
    dat [24] = 0;
    dat [25] = 0;
    dat [26] = 0;
    dat [27] = 0;
    dat [28] = 0;
    dat [29] = 0;
    dat [30] = 0;
    file_exportdata (dat , dan);
    l = settings. projectlength;
    b = fgetbwl();
    t = tc;
    tf = tc + fgetdraft() / 2.f;
    ta = tc - fgetdraft() / 2.f;
    v = fgetdisplacement();
    d = fgetdp();
    if (d == 0)
        d = 0.7 * t;
    cb = cp * cm;
    s = wetarea;
    re = convertedspeed * l / fgetviscosity() * 1e6f;
    cf = 0.075 / square (logp (re) - 2);
    l_b = l / b;
    b_l = b / l;
    b_ta = b / ta;
    ta_d = ta / d;
    tf_l = tf / l;
    t_l = t / l;
    t_b = t / b;
    lr = l * (1 - cp + 0.06 * cp * lcb / (4 * cp - 1));
    c [14] = 1.f + 0.011 * fgetcstrn();
    k1_ = 0.93 + 0.487118 * c [14] * power (b_l , 1.06806) * power (t_l , 0.46106) * power ((l / lr) , 0.121563) * power (l * l * l / v , 0.36486) * power ((1 - cp) , - 0.604247);
    if (b_ta > 5)
        c [8] = s * (7 * b_ta - 25) / (l * d * (b_ta - 3));
    else
        c [8] = b * s / (l * d * ta);
    if (c [8] > 28)
        c [9] = 32 - 16 / (c [8] - 24);
    else
        c [9] = c [8];
    if (ta_d > 2)
        c [11] = 0.0833333 * power (ta_d , 3) + 1.33333;
    else
        c [11] = ta_d;
    if (cp > 0.7)
        c [19] = 0.18567 / (1.3571 - cm) - 0.71276 + 0.38648 * cp;
    else
        c [19] = 0.12997 / (0.95 - cb) - 0.11056 / (0.95 - cp);
    c [20] = 1 + 0.0015 * fgetcstrn();
    ca = 0.00001;
    ca = 0.006 * power ((l + 100) , (- 0.16)) - 0.00205 + 0.003 * sqrtp (l / 7.5f) * square (square (cb)) * c [2] * (0.04 - c [4]);
    if (fgetks() >= 150.f)
        ca = ca + (0.105 * power (fgetks() / 1000000.f , 0.333333) - 0.005579) / power (l , 0.33333);
    cp1 = 1.45 * cp - 0.315 - 0.0225 * lcb;
    cv = k1_ * cf + ca;
    w = 0.1;
    nver = 1988;
    resultsmemo.append(space (14) + QStringLiteral("Cv            = ") + number (cv , 0 , 6 , 5));
    resultsmemo.append(space (14) + QStringLiteral("Cp1           = ") + number (cp1 , 0 , 6 , 5));
    resultsmemo.append(space (14) + QStringLiteral("Cf            = ") + number (cf , 0 , 6 , 5));
    resultsmemo.append(space (14) + QStringLiteral("Ca            = ") + number (ca , 0 , 6 , 5));
    resultsmemo.append(space (14) + QStringLiteral("(1+k)         = ") + number (k1_ , 0 , 6 , 5));
    if (fgetnp() == 1)
        {
            w = c [20] * (c [9] * cv * l / ta * (0.050776 + 0.93405 * c [11] * cv / (1 - cp1)) + 0.27915 * sqrtp (b / (l * (1 - cp1))) + c [19]);
            t0 = 0.25014 * power (b_l , 0.28956) * power ((sqrtp (b * t) / d) , 0.2624) / power ((1 - cp + 0.0225 * lcb) , 0.01762) + 0.0015 * fgetcstrn();
            nr = 0.9922 - 0.05908 * ae0 + 0.07424 * (cp - 0.0225 * lcb);
            nh = (1 - t0) / (1 - w) * nr;
            if (nh > 1.33)
                {
                    w = c [9] * cv * l / ta * (0.0661875 + 1.21756 * c [11] * cv / (1 - cp1)) + 0.24558 * sqrtp (b / (l * (1 - cp1))) - 0.09726 / (0.95 - cp) + 0.11434 / (0.95 - cb) + fgetcstrn() * (0.75f * cv + 0.002);
                    if (l_b >= 5.2)
                        c [10] = b_l;
                    else
                        c [10] = 0.25f - 0.003328402 / (b_l - 0.134615385);
                    t0 = 0.001979 * l / b / (1 - cp1) + 1.0585 * c [10] - 0.00524 - 0.1418 * d * d / b / t + 0.0015 * fgetcstrn();
                    nr = 0.9922 - 0.05908 * ae0 + 0.07424 * (cp - 0.0225 * lcb);
                    nh = (1 - t0) / (1 - w) * nr;
                    nver = 1984;
                }
        }
    if (fgetnp() == 2)
        {
            w = 0.3095 * cb + 10 * cv * cb - 0.23 * d / sqrtp (b * t);
            t0 = 0.325 * cb - 0.1885 * d / sqrtp (b * t);
            nr = 0.9737 + 0.111 * (cp - 0.0225 * lcb) - 0.06325 * p_d0;
        }
}

void Resistance_fung::file_exportdata(Span<float> dat, Span<float> dan) {
int i{};
TextFile ffile{};
    if (exists (pathfileold + QStringLiteral("fungdata.dat")))
        check();
    if (exists (pathfileold + QStringLiteral("fungdata.res")))
        check();
    ffile.name = safePath(QStringLiteral("fungdata.dat"));
    newFile(ffile);
    for (i=0; i<=29; ++i)
    {
        check();
        writeLine(ffile, dat [i]);
    }
    closeFile(ffile);
}

void Resistance_fung::file_exportdatake(Span<float> dat, Span<float> dan) {
int i{};
TextFile ffile{};
    ffile.name = safePath(QStringLiteral("TMPke.txt"));
    newFile(ffile);
    for (i=0; i<=29; ++i)
    {
        check();
        writeLine(ffile, dat [i]);
    }
    for (i=0; i<=14; ++i)
    {
        check();
        writeLine(ffile, dan [i]);
    }
    closeFile(ffile);
}

void Resistance_fung::calculateinworkspace() {
QString processcallerdirectory{};
int processcallercursor{};
float convertedspeed{};
float froudenumber{};
float rf{};
float rr{};
float rt{};
float flag{};
float w{};
float t0{};
float nr{};
float s{};
float ae_ao{};
float p_d{};
float z{};
float k{};
float tb{};
float ta{};
float dp_{};
float kdt{};
float vms{};
float x{};
float tmax{};
float psi{};
float lpp{};
float cb{};
float fr_max{};
int index{};
std::vector<float> cpopt{};
std::vector<float> lcbopt{};
float speed{};
float stepspeed1{};
float lr{};
float l{};
float dlr{};
float bt{};
float tarea{};
float tw{};
QString line{};
QString tmp{};
QString str{};
int units{};
bool stop{};
int i{};
int ii{};
int ispeed{};
TextFile ffile{};
float vsr{};
float c{};
float ke_{};
Array<float,1,5> vs{};
Array<Array<float,1,11>,1,11> res{};
Array<float,1,11> rr_min{};
Array<QString,1,11> ss{};
QString pathfile{};
QString filetofind{};
QString pathfileold{};
QString fopendirectory{};
QString fexecdirectory{};
QString execfullname{};
const QString paramfilename = QStringLiteral("fungdata.dat");
const QString resultfilename = QStringLiteral("fungleib.res");
    vs [1] = fgetstartspeed();
    vs [4] = fgetendspeed();
    vs [5] = fgetstepspeed();
    vs [2] = vs [1] + (vs [4] - vs [1]) / 3;
    vs [3] = vs [1] + (vs [4] - vs [1]) * 2 / 3;
    vsr = fgetendspeed();
    clearSeries(QStringLiteral("series1"));
    clearSeries(QStringLiteral("series2"));
    clearSeries(QStringLiteral("series3"));
    clearSeries(QStringLiteral("series4"));
    ; // presentation only
    ; // presentation only
    field(QStringLiteral("printbutton")) . enabled = false;
    resultsmemo2.clear();
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p1")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p2")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p3")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p4")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_fungleib_note_p5")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(QStringLiteral("Copyright (c) 2010, Timoshenko V.F."));
    ; // presentation only
    if (correctinputdata())
        {
            units = settings. projectunits;
            resultsmemo.clear();
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            header (space (10) + tr(QStringLiteral("rs_program_of_resistance_and_power_prediction_for_naval_ships__nuos_ukraine_2010_")) + QStringLiteral(".") , resultsmemo);
            resultsmemo.append(QStringLiteral("          ----------------------------------------------------------------------------------"));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_input_variables")));
            resultsmemo.append(QStringLiteral("          ----------------------------------------------------------------------------------"));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_general")));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_start_speed")) , 40) + QStringLiteral(" : ") + number (vs [1] , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_kn")));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_end_speed")) , 40) + QStringLiteral(" : ") + number (vs [5] , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_kn")));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_water_density")) , 40) + QStringLiteral(" : ") + number (fgetdensity() , 0 , 8 , 3) + QString(QChar(32)) + densitystr (units));
            if (units == 1)
                resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_water_viscosity")) , 40) + QStringLiteral(" : ") + number (fgetviscosity() , 0 , 8 , 4) + QStringLiteral("*10^(-6) ") + tr(QStringLiteral("rs_ft_2_s")));
            else
                resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_water_viscosity")) , 40) + QStringLiteral(" : ") + number (fgetviscosity() , 0 , 8 , 4) + QStringLiteral("*10^(-6) ") + tr(QStringLiteral("rs_m_2_s")));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_hull")));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_length_on_waterline")) , 40) + QStringLiteral(" : ") + number (fgetlwl() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_length_over_surface_los")) , 40) + QStringLiteral(" : ") + number (fgetlos() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_beam_on_waterline")) , 40) + QStringLiteral(" : ") + number (fgetbwl() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_draft_on_midship")) , 40) + QStringLiteral(" : ") + number (tc , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_draft_on_f_p_")) , 40) + QStringLiteral(" : ") + number (tc + fgetdraft() / 2.f , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_draft_on_f_a_")) , 40) + QStringLiteral(" : ") + number (tc - fgetdraft() / 2.f , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            stop = false;
            if ((absp (fgetdraft()) / fgetlwl() > 0.05) || (absp (fgetdraft()) > fgetdrafttotal()))
                {
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_warning____very_big_trim__tf_ta_")));
                    stop = true;
                }
            if (field(QStringLiteral("estimatebox")) . checked)
                resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_wetted_surface_area")) , 40) + QStringLiteral(" : ") + number (fgetwettedsurface() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(")"));
            else
                resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_wetted_surface_area")) , 40) + QStringLiteral(" : ") + number (fgetwettedsurface() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_waterplane_area")) , 40) + QStringLiteral(" : ") + number (wlarea , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_displacement")) , 40) + QStringLiteral(" : ") + number (fgetdisplacement() , 0 , 6 , 3) + QString(QChar(32)) + volstr (units));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_longitudinal_center_of_buoyancy")) , 40) + QStringLiteral(" : ") + number (fgetlcb() , 0 , 6 , 3) + QStringLiteral(" %"));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_prismatic_coefficient")) , 40) + QStringLiteral(" : ") + number (fgetcp() , 0 , 6 , 4));
            resultsmemo.append(QStringLiteral(""));
            if ((fgeta1() + fgeta2() + fgeta3() + fgeta4() + fgeta5() + fgeta6() + fgeta7() + fgeta8() + fgeta9() + fgeta10() + fgeta11() + fgetks() > 0) && field(QStringLiteral("estimate2box")) . checked)
                {
                    resultsmemo.append(space (10) + pad (field(QStringLiteral("groupbox3")) . caption , 30));
                    resultsmemo.append(space (14) + pad (field(QStringLiteral("label6_1")) . caption , 45) + QStringLiteral(" : ") + number (fgeta1() , 0 , 6 , 1));
                    resultsmemo.append(space (14) + pad (field(QStringLiteral("label6_2")) . caption , 45) + QStringLiteral(" : ") + number (fgeta2() , 0 , 6 , 3));
                    resultsmemo.append(space (14) + pad (field(QStringLiteral("label6_3")) . caption , 45) + QStringLiteral(" : ") + number (fgeta3() , 0 , 6 , 2));
                    resultsmemo.append(space (14) + pad (field(QStringLiteral("label6_4")) . caption , 45) + QStringLiteral(" : ") + number (fgeta4() , 0 , 6 , 2));
                    resultsmemo.append(space (14) + pad (field(QStringLiteral("label6_5")) . caption , 45) + QStringLiteral(" : ") + number (fgeta5() , 0 , 6 , 2));
                    resultsmemo.append(space (14) + pad (field(QStringLiteral("label6_6")) . caption , 45) + QStringLiteral(" : ") + number (fgeta6() , 0 , 6 , 3));
                    resultsmemo.append(space (14) + pad (field(QStringLiteral("label6_7")) . caption , 45) + QStringLiteral(" : ") + number (fgeta7() , 0 , 6 , 3));
                    resultsmemo.append(space (14) + pad (field(QStringLiteral("label6_8")) . caption , 45) + QStringLiteral(" : ") + number (fgeta8() , 0 , 6 , 3));
                    resultsmemo.append(space (14) + pad (field(QStringLiteral("label6_9")) . caption , 45) + QStringLiteral(" : ") + number (fgeta9() , 0 , 6 , 2));
                    resultsmemo.append(space (14) + pad (field(QStringLiteral("label6_10")) . caption , 45) + QStringLiteral(" : ") + number (fgeta10() , 0 , 6 , 2));
                    resultsmemo.append(space (14) + pad (field(QStringLiteral("label6_11")) . caption , 45) + QStringLiteral(" : ") + number (fgeta11() , 0 , 6 , 0));
                    resultsmemo.append(space (14) + pad (field(QStringLiteral("label17")) . caption , 45) + QStringLiteral(" : ") + number (fgetks() , 0 , 6 , 0));
                    resultsmemo.append(QStringLiteral(""));
                }
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_calculated_variables")));
            resultsmemo.append(QStringLiteral("          ----------------------------------------------------------------------------------"));
            resultsmemo.append(QStringLiteral(""));
            if (fgetwettedsurface() <= 0)
                {
                    s = fgetlwl() * (2 * tc + fgetbwl()) * sqrtp (cm) * (0.453 + 0.442 * fgetcp() * cm - 0.2862 * cm - 0.003467 * fgetbwl() / tc + 0.3696 * cwp) + 2.38 * fgetkbulb() / fgetcp() * cm;
                    resultsmemo.append(space (14) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_method_holtrop_1988")) + QStringLiteral(")"));
                    s_ = s;
                }
            lpp = settings. projectlength;
            tmax = settings. projectdraft;
            if ((lpp == 1) && (tmax == 1))
                {
                    warning(tr(QStringLiteral("rs_do_not_set_main_dimensions_lpp__b_and_t____")));
                    return;
                }
            cb = fgetcp() * cm;
            ta = tc - fgetdraft() / 2.f;
            bt = fgetzbulb();
            dlr = fgetdisplacement() / power (fgetlwl() / 100 / 0.3054545 , 3) * 1.01633;
            lr = lpp * (1 - fgetcp() + 0.06 * fgetcp() * fgetlcb() / (4 * fgetcp() - 1));
            tarea = fgetba() / (fgetbwl() * tc * cm);
            tw = bt / fgetbwl();
            if (cwp == 0)
                cwp = fgetcp() * cm / (0.471 + 0.551 * fgetcp() * cm);
            if (fgetkbulb() < 2.6)
                fsetkbulb(1.f + 89.f * expp (- power (fgetlwl() / fgetbwl() , 0.80856) * power ((1 - cwp) , 0.30484) * power ((1 - fgetcp() - 0.0225 * fgetlcb()) , 0.6367) * power ((lr / fgetbwl()) , 0.34574) * power ((100 * fgetdisplacement() / fgetlwl() / fgetlwl() / fgetlwl()) , 0.16302)));
            resultsmemo.append(space (14) + QStringLiteral("Cp            = ") + number (fgetcp() , 0 , 6 , 4));
            resultsmemo.append(space (14) + QStringLiteral("Cb            = ") + number (fgetcp() * cm , 0 , 6 , 4));
            resultsmemo.append(space (14) + QStringLiteral("Cwp           = ") + number (cwp , 0 , 6 , 4));
            resultsmemo.append(space (14) + QStringLiteral("Cm            = ") + number (cm , 0 , 6 , 4));
            resultsmemo.append(space (14) + QStringLiteral("Am            = ") + number (am , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(space (14) + QStringLiteral("L/B           = ") + number (lpp / fgetbwl() , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("ie            = ") + number (fgetkbulb() , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_degr")));
            resultsmemo.append(space (14) + QStringLiteral("Tc            = ") + number (tc , 0 , 6 , 3) + QStringLiteral(" m"));
            resultsmemo.append(space (14) + QStringLiteral("B/T           = ") + number (fgetbwl() / tc , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Lwl/T         = ") + number (fgetlwl() / tc , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("DLR           = ") + number (dlr , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Ta            = ") + number (tarea , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Tw            = ") + number (tw , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Np            = ") + number (fgetnp() , 0 , 6 , 0));
            resultsmemo.append(QStringLiteral(""));
            fr_max = vs [5] * 0.514444 / sqrtp (9.81 * (fgetlwl() + lpp) / 2);
            if ((fgetlwl() / fgetbwl() > 18) || (fgetlwl() / fgetbwl() < 2.52))
                resultsmemo.append(space (14) + QStringLiteral("L/B        ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("  2,52 ... 18"));
            if ((fgetbwl() / tc > 10.2) || (fgetbwl() / tc < 1.7))
                resultsmemo.append(space (14) + QStringLiteral("B/T        ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   1,7 ... 10,2"));
            if ((cb / cm > 0.774) || (cb / cm < 0.526))
                resultsmemo.append(space (14) + QStringLiteral("Cp         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,526 ... 0,774"));
            if ((cm > 0.994) || (cm < 0.556))
                resultsmemo.append(space (14) + QStringLiteral("Cm         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,556 ... 0,994"));
            if ((cwp > 0.841) || (cwp < 0.662))
                resultsmemo.append(space (14) + QStringLiteral("Cwp        ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,662 ... 0,841"));
            if ((fgetkbulb() > 32) || (fgetkbulb() < 2.6))
                resultsmemo.append(space (14) + QStringLiteral("ie         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   2,6 ... 31,73") + QStringLiteral(" ") + tr(QStringLiteral("rs_degr")));
            if ((tarea > 0.74) || (tarea < 0))
                resultsmemo.append(space (14) + QStringLiteral("Ta         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   0,0 ... 0,74"));
            if ((tw > 1.0f) || (tw < 0))
                resultsmemo.append(space (14) + QStringLiteral("Tw         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   0,0 ... 1,00"));
            if ((fr_max > 0.9) || (fr_max < 0.15))
                resultsmemo.append(space (14) + QStringLiteral("Fr         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("  0,15 ... 0,9"));
            if ((dlr > 360) || (dlr < 16))
                resultsmemo.append(space (14) + QStringLiteral("10^4*V/L^3 ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 16,24 ... 359,18"));
            speed = vs [4];
            convertedspeed = speed * 0.514444;
            check();
            check();
            calculateresistancefungleib (convertedspeed , fgetlcb() , fgetcp() , rf , rr , w , t0 , nr);
            if ((fgetlwl() > 0) && (fgetbwl() > 0) && (fgetdp() >= 0) && (fgetnp() >= 0) && (ta > 0))
                {
                    fexecdirectory = solverDirectory;
                    filetofind = findFile (paramfilename , work);
                    if (filetofind != paramfilename)
                        {
                            warning(tr(QStringLiteral("rs_have_not_input_file_for_calculation_is_here___")));
                            return;
                        }
                    execfullname = fexecdirectory + QStringLiteral("/") + QStringLiteral("fungleib.EXE");
                    runSolver(execfullname, fcfung);
                    i = 1;
                    label_newsearch:;
                    filetofind = findFile (resultfilename , work);
                    if (filetofind == paramfilename)
                        {
                            check();
                            i = i + 1;
                            if (i < 25)
                                goto label_newsearch;
                            else
                                {
                                    warning(tr(QStringLiteral("rs_do_not_calculate_this_task__because_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_1__do_not_found_into__exec_or_was_damaged_file")) + QStringLiteral(" ") + fexecdirectory + QStringLiteral("/") + QStringLiteral("fungleib.EXE") + QStringLiteral(" ") + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_2__input_datas_are_outside_valid_domain_or_catalog_is_not_valid_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_3__this_is_very_slow_computer__fcpu___800_mhz_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_4__cpu_is_loaded_more_80__another_processes_")));
                                    if (exists (QStringLiteral("fungdata.dat")))
                                        check();
                                    return;
                                }
                        }
                    ffile.name = safePath(resultfilename);
                    readFile(ffile);
                    ii = 0;
                    ; // presentation only
                    while ((! atEnd (ffile)) && (ii < 10)) {
                        check();
                        {
                            ii = ii + 1;
                            for (i=1; i<=8; ++i)
                            {
                                check();
                                readValues(ffile, res [i][ii]);
                            }
                            if (std::isnan (res [6][ii]))
                                continue;
                            if ((res [6][ii] < 0))
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
                    rtotal = res [7][9];
                    if (fgetke() == 0)
                        {
                            fexecdirectory = solverDirectory;
                            filetofind = findFile (QStringLiteral("TMPke.txt") , work);
                            if (filetofind != QStringLiteral("TMPke.txt"))
                                {
                                    warning(tr(QStringLiteral("rs_have_not_input_file_for_calculation_is_here___")));
                                    return;
                                }
                            execfullname = fexecdirectory + QStringLiteral("/") + QStringLiteral("SeaMargn.EXE");
                            runSolver(execfullname, fcseamargin);
                            filename = QStringLiteral("OUT.TXT");
                            i = 1;
                            label_newsearch1:;
                            filetofind = findFile (QStringLiteral("TMPke.txt") , work);
                            if (filetofind == QStringLiteral("TMPke.txt"))
                                {
                                    check();
                                    i = i + 1;
                                    if (i < 25)
                                        goto label_newsearch1;
                                    else
                                        {
                                            if (exists (QStringLiteral("TMPke.txt")))
                                                check();
                                            warning(tr(QStringLiteral("rs_do_not_calculate_this_task__because_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_1__do_not_found_into__exec_or_was_damaged_file")) + QStringLiteral(" SeaMargn.EXE ") + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_2__input_datas_are_outside_valid_domain_or_catalog_is_not_valid_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_3__this_is_very_slow_computer__fcpu___800_mhz_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_4__cpu_is_loaded_more_80__another_processes_")));
                                            fsetke(1);
                                            return;
                                        }
                                }
                            ffile.name = safePath(QStringLiteral("OUT.TXT"));
                            readFile(ffile);
                            readLine(ffile, ke_);
                            closeFile(ffile);
                            if (exists (QStringLiteral("OUT.TXT")))
                                check();
                            fsetke(ke_);
                        }
                    if (fgetke() < 0.2)
                        fsetke(1);
                    for (ii=1; ii<=10; ++ii)
                    {
                        check();
                        {
                            res [9][ii] = res [7][ii] * fgetke();
                            res [10][ii] = res [8][ii] * fgetke();
                            if (res [1][ii] > 1)
                                {
                                    if (fgetke() > 1)
                                        {
                                            point(QStringLiteral("series2"), res [1][ii] ,  res [9][ii] * 10 );
                                            point(QStringLiteral("series4"), res [1][ii] ,  res [10][ii] );
                                        }
                                    point(QStringLiteral("series1"), res [1][ii] ,  res [7][ii] * 10 );
                                    point(QStringLiteral("series3"), res [1][ii] ,  res [8][ii] );
                                }
                        }
                    }
                }
            ffile.name = safePath(trailing (work) + QStringLiteral("RESISTp.dat"));
            newFile(ffile);
            writeLine(ffile, QStringLiteral("#     Nser      Np       Wt        t       Eta_R     Dp"));
            writeText(ffile, QStringLiteral("       54 "));
            writeText(ffile, formatValue(fgetnp(), 10, 0));
            writeText(ffile, formatValue(w, 10, 4));
            writeText(ffile, formatValue(t0, 10, 4));
            writeText(ffile, formatValue(nr, 10, 4));
            writeLine(ffile, formatValue(fgetdp(), 10, 4));
            writeLine(ffile, QStringLiteral("#      Vs       Rt        Rte       Pe       Pee"));
            for (ii=2; ii<=10; ++ii)
            {
                check();
                {
                    writeText(ffile, formatValue(res [1][ii], 10, 2));
                    writeText(ffile, formatValue(res [7][ii], 10, 2));
                    writeText(ffile, formatValue(res [9][ii], 10, 2));
                    writeText(ffile, formatValue(res [8][ii], 10, 2));
                    writeLine(ffile, formatValue(res [10][ii], 10, 2));
                }
            }
            closeFile(ffile);
            ffile.name = safePath(trailing (work) + QStringLiteral("RESIST.dat"));
            newFile(ffile);
            for (i=1; i<=5; ++i)
            {
                check();
                {
                    if (i == 1)
                        ii = 2;
                    if (i == 2)
                        ii = 4;
                    if (i == 3)
                        ii = 6;
                    if (i == 4)
                        ii = 9;
                    if (i == 5)
                        ii = 10;
                    writeText(ffile, formatValue(res [1][ii], 10, 2));
                    writeText(ffile, formatValue(res [7][ii], 10, 2));
                    writeText(ffile, formatValue(res [9][ii], 10, 2));
                    writeText(ffile, formatValue(res [8][ii], 10, 2));
                    writeLine(ffile, formatValue(res [10][ii], 10, 2));
                }
            }
            closeFile(ffile);
            flag = 0.001;
            if (res [9][10] > 5)
                flag = 1.f;
            for (ii=1; ii<=10; ++ii)
            {
                check();
                for (i=1; i<=10; ++i)
                {
                    check();
                    if (i > 4)
                        res [i][ii] = res [i][ii] / flag;
                }
            }
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_by_method_fung_leibman_1995")));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_t________t________t________t________t________t________t________t________t________t")));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___vs___l___vms__l___fr___l___r_f__l___r_r__l___r_t__l___pe___l__r_t_e_l__pe_e__l")));
            yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral("*10, ") + tr(QStringLiteral("rs_kn")) + QStringLiteral(";   ") + tr(QStringLiteral("rs_power")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kw"));
            if (flag == 1)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn___l___m_s__l________l___kn___l___kn___l___kn___l___kw___l___kn___l___kw___l")));
            else
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn___l___m_s__l________l____n___l____n___l____n___l___w____l____n___l___w____l")));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_t________t________t________t________t________t________t________t________t________t")));
            index = 0;
            cpopt.resize(truncp ((fgetendspeed() - fgetstartspeed()) / fgetstepspeed()) + 10);
            lcbopt.resize(truncp ((fgetendspeed() - fgetstartspeed()) / fgetstepspeed()) + 10);
            for (ii=1; ii<=10; ++ii)
            {
                check();
                {
                    for (i=1; i<=10; ++i)
                    {
                        check();
                        {
                            if (std::isnan (res [i][ii]))
                                {
                                    ss [i] = QStringLiteral("   NaN  | ");
                                    continue;
                                }
                            if (res [i][ii] >= 10000)
                                ss [i] = number (res [i][ii] , 0 , 6 , 0) + QStringLiteral("  | ");
                            if ((res [i][ii] >= 1000) && (res [i][ii] < 10000))
                                ss [i] = number (res [i][ii] , 0 , 6 , 0) + QStringLiteral(".  | ");
                            if ((res [i][ii] >= 100) && (res [i][ii] < 1000))
                                ss [i] = number (res [i][ii] , 0 , 6 , 1) + QStringLiteral("  | ");
                            if ((res [i][ii] >= 10) && (res [i][ii] < 100))
                                ss [i] = number (res [i][ii] , 0 , 6 , 2) + QStringLiteral("  | ");
                            if (res [i][ii] < 10)
                                ss [i] = number (res [i][ii] , 0 , 6 , 3) + QStringLiteral("  | ");
                        }
                    }
                    if (ii > 1)
                        resultsmemo.append(space (10) + QStringLiteral("| ") + ss [1] + ss [2] + ss [3] + ss [5] + ss [6] + ss [7] + ss [8] + ss [9] + ss [10]);
                }
            }
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_t________t________t________t________t________t________t________t________t________t")));
            resultsmemo.append(QStringLiteral(""));
            tb = res [7][9] * fgetke() / (1 - t0) * flag;
            vms = res [2][9];
            if (fgetnp() > 0)
                {
                    p_d = 0.9;
                    z = 4;
                    k = 0.1;
                    ta = tc - fgetdraft() / 2.f;
                    dp_ = fgetdp();
                    tb = tb / fgetnp();
                    if (tb == 0)
                        tb = 0.01;
                    if ((fgetdp() == 0) && (fgetnp() == 1))
                        dp_ = 0.7 * ta;
                    if ((fgetdp() == 0) && (fgetnp() == 2))
                        dp_ = 0.65 * ta;
                    if (fgetnp() == 2)
                        k = 0.2;
                    kdt = vms * (1 - w) * dp_ * sqrtp (fgetdensity() / tb);
                    if (kdt >= 2)
                        z = 3;
                    x = kdt;
                    ae_ao = k + (1.3 + 0.3 * z) * tb * 1000.f / (dp_ * dp_ * (99047.f + fgetdensity() * 1000.f * 9.81 * (dp_ / 2.f + ta * 0.2)));
                    ae0 = ae_ao;
                    if (ae0 > 1.1)
                        resultsmemo.append(space (14) + QStringLiteral("Ae/Ao   =  ") + number (ae0 , 0 , 6 , 4));
                    if ((ae0 > 1.1) && (fgetnp() <= 2))
                        {
                            resultsmemo.append(space (10) + tr(QStringLiteral("rs_attention____increase_number_of_propellers_or_decrease_estimate_speed_")));
                            ; // presentation only
                            return;
                        }
                    if (z == 2)
                        p_d = - 0.0030823 * x * x * x + 0.0142337 * x * x + 0.1859161 * x + 0.4598835;
                    if (z == 3)
                        {
                            if (ae0 <= 0.35)
                                {
                                    p_d = 0.0081576 * x * x * x - 0.0083761 * x * x + 0.3275126 * x + 0.3668222;
                                    ae_ao = 0.35;
                                }
                            if ((ae0 <= 0.5f) && (ae0 > 0.35))
                                {
                                    p_d = - 0.0240385 * x * x * x + 0.13499 * x * x + 0.1599401 * x + 0.498986;
                                    ae_ao = 0.5f;
                                }
                            if ((ae0 <= 0.65) && (ae0 > 0.5f))
                                {
                                    p_d = 0.0236147 * x * x * x * x * x - 0.2234597 * x * x * x * x + 0.8044646 * x * x * x - 1.3956004 * x * x + 1.3848023 * x + 0.3433638;
                                    ae_ao = 0.65;
                                }
                        }
                    if (z == 4)
                        {
                            if (ae0 <= 0.4)
                                {
                                    p_d = 0.039967 * x * x * x - 0.2388861 * x * x + 0.7556191 * x + 0.2328998;
                                    ae_ao = 0.4;
                                }
                            if ((ae0 <= 0.55) && (ae0 > 0.4))
                                {
                                    p_d = 0.0353248 * x * x * x - 0.2223617 * x * x + 0.6754278 * x + 0.4570168;
                                    ae_ao = 0.55;
                                }
                            if ((ae0 <= 0.7) && (ae0 > 0.55))
                                {
                                    p_d = 0.0074646 * x * x * x * x * x - 0.0783429 * x * x * x * x + 0.2946838 * x * x * x - 0.5441205 * x * x + 0.8133517 * x + 0.3989677;
                                    ae_ao = 0.7;
                                }
                        }
                    if (flag == 1)
                        resultsmemo.append(space (14) + QStringLiteral("Tb        =  ") + number (tb , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_kn")));
                    else
                        resultsmemo.append(space (14) + QStringLiteral("Tb        =  ") + number (tb / flag , 0 , 6 , 0) + QStringLiteral(" ") + tr(QStringLiteral("rs_n")));
                    resultsmemo.append(space (14) + QStringLiteral("Kdt       =  ") + number (kdt , 0 , 6 , 3));
                    resultsmemo.append(space (14) + QStringLiteral("Dp        =  ") + number (dp_ , 0 , 6 , 3) + QStringLiteral(" ") + lengthstr (units));
                    resultsmemo.append(space (14) + QStringLiteral("Z         =  ") + number (z , 0 , 6 , 0));
                    resultsmemo.append(space (14) + QStringLiteral("Ae/Ao     =  ") + number (ae0 , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs___calculated")));
                    p_d0 = p_d;
                    ae0 = ae_ao;
                    resultsmemo.append(space (14) + QStringLiteral("Ae/Ao     =  ") + number (ae0 , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs___for_selection_of_propeller_diagram")));
                    resultsmemo.append(space (14) + QStringLiteral("P/Dp      =  ") + number (p_d0 , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs___by_curve_of_propeller_optimal_frequency")));
                }
            if (fgetnp() == 1)
                nr = 0.9922 - 0.05908 * ae0 + 0.07424 * (fgetcp() - 0.0225 * fgetlcb());
            else
                nr = 0.9737 + 0.111 * (fgetcp() - 0.0225 * fgetlcb()) - 0.06325 * p_d0;
            resultsmemo.append(space (14) + QStringLiteral("Ke        =  ") + number (fgetke() , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Wt        =  ") + number (w , 0 , 6 , 4));
            resultsmemo.append(space (14) + QStringLiteral("t         =  ") + number (t0 , 0 , 6 , 4));
            resultsmemo.append(space (14) + QStringLiteral("EtaR      =  ") + number (nr , 0 , 6 , 4));
            resultsmemo.append(space (14) + QStringLiteral("EtaH      =  ") + number ((1 - t0) / (1 - w) , 0 , 6 , 4));
            resultsmemo.append(space (14) + QStringLiteral("EtaH*EtaR =  ") + number ((1 - t0) / (1 - w) * nr , 0 , 6 , 4));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (14) + tr(QStringLiteral("rs_note__coefficients_wt__t_and_etar_were_calculated_by_formulaes_of_method_holtrop_")) + number (nver , 0 , 6 , 0));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + QStringLiteral("Copyright (c) 2010, Timoshenko V.F."));
            ; // presentation only
            field(QStringLiteral("printbutton")) . enabled = true;
        }
    check();
}

bool Resistance_fung::execute() {
bool result{};
int units{};
float temper{};
int rightaxis{};
    units = settings. projectunits;
    if (units == 1)
        {
            warning(tr(QStringLiteral("rs_this_is_version_of_program_work_with_metric_system_units_only")));
            return result;
        }
    ; // presentation only
    xTitle = tr(QStringLiteral("rs_speed")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kn"));
    ;
    rightTitle = tr(QStringLiteral("rs_power")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kw"));
    units = settings. projectunits;
    temper = settings. projectwatertemper;
    field(QStringLiteral("densitybox")) . enabled = false;
    field(QStringLiteral("k1box")) . enabled = false;
    field(QStringLiteral("k2box")) . enabled = false;
    field(QStringLiteral("k3box")) . enabled = false;
    field(QStringLiteral("k4box")) . enabled = false;
    field(QStringLiteral("k5box")) . enabled = false;
    field(QStringLiteral("k6box")) . enabled = false;
    field(QStringLiteral("k7box")) . enabled = false;
    field(QStringLiteral("keelchordlengthbox")) . enabled = false;
    field(QStringLiteral("keelareabox")) . enabled = false;
    field(QStringLiteral("zbulbbox")) . enabled = true;
    field(QStringLiteral("babox")) . enabled = true;
    field(QStringLiteral("draftbox")) . enabled = false;
    field(QStringLiteral("cstrnbox")) . enabled = true;
    field(QStringLiteral("rudderchordlengthbox")) . enabled = false;
    field(QStringLiteral("rudderareabox")) . enabled = false;
    field(QStringLiteral("checkbox2")) . checked = false;
    field(QStringLiteral("_label31")) . caption = tr(QStringLiteral("rs_knots"));
    field(QStringLiteral("_label32")) . caption = tr(QStringLiteral("rs_knots"));
    field(QStringLiteral("_label33")) . caption = tr(QStringLiteral("rs_knots"));
    field(QStringLiteral("label34")) . caption = densitystr (units);
    field(QStringLiteral("_label8")) . caption = lengthstr (units);
    field(QStringLiteral("_label9")) . caption = lengthstr (units);
    field(QStringLiteral("_label10")) . caption = lengthstr (units);
    field(QStringLiteral("_label11")) . caption = lengthstr (units);
    field(QStringLiteral("_label12")) . caption = areastr (units);
    field(QStringLiteral("_label14")) . caption = areastr (units);
    field(QStringLiteral("_label15")) . caption = volstr (units);
    field(QStringLiteral("_label16")) . caption = QStringLiteral("%");
    if (units == 0)
        field(QStringLiteral("_label36")) . caption = QStringLiteral(" ") + tr(QStringLiteral("rs_m_2_s"));
    else
        field(QStringLiteral("_label36")) . caption = QStringLiteral(" ") + tr(QStringLiteral("rs_ft_2_s"));
    fsetviscosity(findwaterviscosity (temper , units));
    result = false;
    return result;
}
}
