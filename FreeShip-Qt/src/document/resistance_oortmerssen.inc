// GPL-2.0-or-later. Generated from FreeResistance_OortmerDlg.pas.
namespace {

struct Resistance_oortmerssen final: ResistanceBase {
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
float ae0{};
float p_d0{};
float nver{};
float wlarea{};
Array<Array<float,1,11>,1,3> restot{};
Array<Array<float,1,11>,1,3> vsres{};
QString pathfile{};
QString pathfileold{};
QString filetofind{};
QString filename{};
QString ftmpfilename{};
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
void calculateresistanceoortmer(float convertedspeed, float lcb, float cp, float &rf, float &rr, float &w, float &t0, float &nr);
void file_exportdata(Span<float> dat, Span<float> dan);
void file_exportdatake(Span<float> dat, Span<float> dan);
void comboboxclick(void* sender);
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
void choiceChanged(const QString &key) override { if (key==QStringLiteral("combobox")) comboboxclick(nullptr); }
void runCalculation() override { calculateinworkspace(); }
};
bool Resistance_oortmerssen::correctinputdata() {
bool result{};

    result = false;
    if ((fgetdraft() <= 0.02))
        fsetdraft(0.02001);
    if ((fgetstartspeed() <= 0.5f))
        fsetstartspeed(0.5001);
    if ((fgetstepspeed() <= 0))
        fsetstepspeed(1.0f);
    if ((fgetdrafttotal() < fgetdraft()))
        fsetdraft(fgetdrafttotal());
    if ((fgetlwl() <= 0))
        ; // presentation only
    else
        ; // presentation only
    if ((fgetbwl() <= 0))
        ; // presentation only
    else
        ; // presentation only
    if ((fgetdrafttotal() <= 0.02))
        ; // presentation only
    else
        ; // presentation only
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
    if ((fgetdrafttotal() <= 0.02) || (fgetlwl() <= 0) || (fgetbwl() <= 0) || (fgetdisplacement() <= 0) || (fgetcp() <= 0) || (fgetviscosity() <= 0) || (fgetendspeed() <= fgetstartspeed()) || (fgetstartspeed() < 0.5f))
        return result;
    b_t = fgetbwl() / fgetdrafttotal();
    l_d = fgetlwl() / power (fgetdisplacement() , 1.0 / 3);
    a_d = wlarea / power (fgetdisplacement() , 2.0 / 3);
    l_b = fgetlwl() / fgetbwl();
    cm = fgetcp() / fgetdisplacement() * (fgetlwl() * fgetbwl() * fgetdrafttotal());
    am = cm * fgetdrafttotal() * fgetbwl();
    if (cm >= 1.f)
        {
            am = fgetdisplacement() / (fgetlwl() * fgetcp());
            cm = am / (fgetbwl() * fgetdrafttotal());
        }
    cwp = wlarea / (fgetlwl() * fgetbwl());
    if ((cwp == 1) || (fgetcp() == 1))
        {
            warning(tr(QStringLiteral("rs_error__cp_1_or_cwp_1")));
            return result;
        }
    if ((fgetlwl() > 90) || (fgetlwl() <= 4))
        {
            warning(tr(QStringLiteral("rs_error__lwl_90m_or_lwl__4m")));
            return result;
        }
    result = true;
    return result;
}

float Resistance_oortmerssen::fgetstartspeed() {
float result{};

    result = field(QStringLiteral("startspeedbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetstartspeed(float val) {

    field(QStringLiteral("startspeedbox")).set(val);
}

float Resistance_oortmerssen::fgetendspeed() {
float result{};

    result = field(QStringLiteral("endspeedbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetendspeed(float val) {

    field(QStringLiteral("endspeedbox")).set(val);
}

float Resistance_oortmerssen::fgetdensity() {
float result{};

    result = field(QStringLiteral("densitybox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetdensity(float val) {

    field(QStringLiteral("densitybox")).set(val);
}

float Resistance_oortmerssen::fgetdisplacement() {
float result{};

    result = field(QStringLiteral("displacementbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetdisplacement(float val) {

    field(QStringLiteral("displacementbox")).set(val);
}

float Resistance_oortmerssen::fgetdraft() {
float result{};

    result = field(QStringLiteral("draftbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetdraft(float val) {

    field(QStringLiteral("draftbox")).set(val);
}

float Resistance_oortmerssen::fgetdrafttotal() {
float result{};

    result = field(QStringLiteral("drafttotalbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetdrafttotal(float val) {

    field(QStringLiteral("drafttotalbox")).set(val);
}

float Resistance_oortmerssen::fgetlwl() {
float result{};

    result = field(QStringLiteral("lwlbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetlwl(float val) {

    field(QStringLiteral("lwlbox")).set(val);
}

float Resistance_oortmerssen::fgetlcb() {
float result{};

    result = field(QStringLiteral("lcbbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetlcb(float val) {

    field(QStringLiteral("lcbbox")).set(val);
}

void Resistance_oortmerssen::fsetke(float val) {

    field(QStringLiteral("kebox")).set(val);
    if (field(QStringLiteral("kebox")) . value == 0)
        field(QStringLiteral("kebox")).set(1);
}

float Resistance_oortmerssen::fgetke() {
float result{};

    result = field(QStringLiteral("kebox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetk1(float val) {

    field(QStringLiteral("k1box")).set(val);
}

float Resistance_oortmerssen::fgetk1() {
float result{};

    result = field(QStringLiteral("k1box")) . value;
    return result;
}

void Resistance_oortmerssen::fsetk2(float val) {

    field(QStringLiteral("k2box")).set(val);
}

float Resistance_oortmerssen::fgetk2() {
float result{};

    result = field(QStringLiteral("k2box")) . value;
    return result;
}

void Resistance_oortmerssen::fsetk3(float val) {

    field(QStringLiteral("k3box")).set(val);
}

float Resistance_oortmerssen::fgetk3() {
float result{};

    result = field(QStringLiteral("k3box")) . value;
    return result;
}

void Resistance_oortmerssen::fsetk4(float val) {

    field(QStringLiteral("k4box")).set(val);
}

float Resistance_oortmerssen::fgetk4() {
float result{};

    result = field(QStringLiteral("k4box")) . value;
    return result;
}

void Resistance_oortmerssen::fsetk5(float val) {

    field(QStringLiteral("k5box")).set(val);
}

float Resistance_oortmerssen::fgetk5() {
float result{};

    result = field(QStringLiteral("k5box")) . value;
    return result;
}

void Resistance_oortmerssen::fsetk6(float val) {

    field(QStringLiteral("k6box")).set(val);
}

float Resistance_oortmerssen::fgetk6() {
float result{};

    result = field(QStringLiteral("k6box")) . value;
    return result;
}

void Resistance_oortmerssen::fsetk7(float val) {

    field(QStringLiteral("k7box")).set(val);
}

float Resistance_oortmerssen::fgetk7() {
float result{};

    result = field(QStringLiteral("k7box")) . value;
    return result;
}

void Resistance_oortmerssen::fseta1(float val) {

    field(QStringLiteral("a1box")).set(val);
}

float Resistance_oortmerssen::fgeta1() {
float result{};

    result = field(QStringLiteral("a1box")) . value;
    return result;
}

void Resistance_oortmerssen::fseta2(float val) {

    field(QStringLiteral("a2box")).set(val);
}

float Resistance_oortmerssen::fgeta2() {
float result{};

    result = field(QStringLiteral("a2box")) . value;
    return result;
}

void Resistance_oortmerssen::fseta3(float val) {

    field(QStringLiteral("a3box")).set(val);
}

float Resistance_oortmerssen::fgeta3() {
float result{};

    result = field(QStringLiteral("a3box")) . value;
    return result;
}

void Resistance_oortmerssen::fseta4(float val) {

    field(QStringLiteral("a4box")).set(val);
}

float Resistance_oortmerssen::fgeta4() {
float result{};

    result = field(QStringLiteral("a4box")) . value;
    return result;
}

void Resistance_oortmerssen::fseta5(float val) {

    field(QStringLiteral("a5box")).set(val);
}

float Resistance_oortmerssen::fgeta5() {
float result{};

    result = field(QStringLiteral("a5box")) . value;
    return result;
}

void Resistance_oortmerssen::fseta6(float val) {

    field(QStringLiteral("a6box")).set(val);
}

float Resistance_oortmerssen::fgeta6() {
float result{};

    result = field(QStringLiteral("a6box")) . value;
    return result;
}

void Resistance_oortmerssen::fseta7(float val) {

    field(QStringLiteral("a7box")).set(1.226);
}

float Resistance_oortmerssen::fgeta7() {
float result{};

    result = field(QStringLiteral("a7box")) . value;
    return result;
}

void Resistance_oortmerssen::fseta8(float val) {

    field(QStringLiteral("a8box")).set(val);
}

float Resistance_oortmerssen::fgeta8() {
float result{};

    result = field(QStringLiteral("a8box")) . value;
    return result;
}

void Resistance_oortmerssen::fseta9(float val) {

    field(QStringLiteral("a9box")).set(val);
}

float Resistance_oortmerssen::fgeta9() {
float result{};

    result = field(QStringLiteral("a9box")) . value;
    return result;
}

void Resistance_oortmerssen::fseta10(float val) {

    field(QStringLiteral("a10box")).set(val);
}

float Resistance_oortmerssen::fgeta10() {
float result{};

    result = field(QStringLiteral("a10box")) . value;
    return result;
}

void Resistance_oortmerssen::fseta11(int val) {

    field(QStringLiteral("a11box")) . itemindex = val - 1;
}

int Resistance_oortmerssen::fgeta11() {
int result{};

    result = field(QStringLiteral("a11box")) . itemindex + 1;
    return result;
}

void Resistance_oortmerssen::fsetba(float val) {

    field(QStringLiteral("babox")).set(val);
}

float Resistance_oortmerssen::fgetba() {
float result{};

    result = field(QStringLiteral("babox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetkbulb(float val) {

    field(QStringLiteral("kbulbbox")).set(val);
}

float Resistance_oortmerssen::fgetkbulb() {
float result{};

    result = field(QStringLiteral("kbulbbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetzbulb(float val) {

    field(QStringLiteral("zbulbbox")).set(val);
}

float Resistance_oortmerssen::fgetzbulb() {
float result{};

    result = field(QStringLiteral("zbulbbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetcstrn(float val) {

    field(QStringLiteral("cstrnbox")).set(val);
}

float Resistance_oortmerssen::fgetcstrn() {
float result{};

    result = field(QStringLiteral("cstrnbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetnp(float val) {

    field(QStringLiteral("npbox")).set(val);
}

float Resistance_oortmerssen::fgetnp() {
float result{};

    result = field(QStringLiteral("npbox")) . value;
    if (field(QStringLiteral("npbox")) . value == 0)
        field(QStringLiteral("npbox")).set(1);
    return result;
}

void Resistance_oortmerssen::fsetdp(float val) {

    field(QStringLiteral("dpbox")).set(val);
}

float Resistance_oortmerssen::fgetdp() {
float result{};

    result = field(QStringLiteral("dpbox")) . value;
    if ((field(QStringLiteral("dpbox")) . value == 0) && (fgetnp() == 1))
        field(QStringLiteral("dpbox")).set(fgetdrafttotal() * 0.75f);
    if ((field(QStringLiteral("dpbox")) . value == 0) && (fgetnp() == 2))
        field(QStringLiteral("dpbox")).set(fgetdrafttotal() * 0.7);
    return result;
}

void Resistance_oortmerssen::fsetks(float val) {

    field(QStringLiteral("ksbox")).set(val);
}

float Resistance_oortmerssen::fgetks() {
float result{};

    result = field(QStringLiteral("ksbox")) . value;
    return result;
}

float Resistance_oortmerssen::fgetkeelchordlength() {
float result{};

    result = field(QStringLiteral("keelchordlengthbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetkeelchordlength(float val) {

    field(QStringLiteral("keelchordlengthbox")).set(val);
}

float Resistance_oortmerssen::fgetkeelarea() {
float result{};

    result = field(QStringLiteral("keelareabox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetkeelarea(float val) {

    field(QStringLiteral("keelareabox")).set(val);
}

float Resistance_oortmerssen::fgetrudderchordlength() {
float result{};

    result = field(QStringLiteral("rudderchordlengthbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetrudderchordlength(float val) {

    field(QStringLiteral("rudderchordlengthbox")).set(val);
}

float Resistance_oortmerssen::fgetrudderarea() {
float result{};

    result = field(QStringLiteral("rudderareabox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetrudderarea(float val) {

    field(QStringLiteral("rudderareabox")).set(val);
}

float Resistance_oortmerssen::fgetbwl() {
float result{};

    result = field(QStringLiteral("bwlbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetbwl(float val) {

    field(QStringLiteral("bwlbox")).set(val);
}

float Resistance_oortmerssen::fgetcp() {
float result{};

    result = field(QStringLiteral("cpbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetcp(float val) {

    field(QStringLiteral("cpbox")).set(val);
}

float Resistance_oortmerssen::fgetviscosity() {
float result{};

    result = field(QStringLiteral("viscositybox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetviscosity(float val) {

    field(QStringLiteral("viscositybox")).set(val);
}

float Resistance_oortmerssen::fgetwettedsurface() {
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
                    cm = am / (fgetbwl() * fgetdrafttotal());
                    cwp = wlarea / (fgetlwl() * fgetbwl());
                    c23 = 0.453 + 0.443 * (fgetcp() * cm) - 0.286 * cm - 0.00347 * (fgetbwl() / fgetdrafttotal()) + 0.37 * cwp;
                    scbfact = 0.616 * c23 + 0.111 * cm * cm * cm + 0.245 * (c23 / cm) - 0.0228;
                    result = scbfact * fgetlwl() * (2 * fgetdrafttotal() + fgetbwl()) * sqrtp (cm);
                }
            else
                result = 0;
        }
    else
        result = field(QStringLiteral("wettedsurfacebox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetwettedsurface(float val) {

    field(QStringLiteral("wettedsurfacebox")).set(val);
}

float Resistance_oortmerssen::fgetlos() {
float result{};

    result = field(QStringLiteral("losbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetlos(float val) {

    field(QStringLiteral("losbox")).set(val);
}

float Resistance_oortmerssen::fgetstepspeed() {
float result{};

    result = field(QStringLiteral("stepspeedbox")) . value;
    return result;
}

void Resistance_oortmerssen::fsetstepspeed(float val) {

    field(QStringLiteral("stepspeedbox")).set(val);
}

bool Resistance_oortmerssen::fgetextractfromhull() {
bool result{};

    result = field(QStringLiteral("checkbox2")) . checked;
    return result;
}

void Resistance_oortmerssen::fsetextractfromhull(bool val) {

    if (field(QStringLiteral("checkbox2")) . checked != val)
        field(QStringLiteral("checkbox2")) . checked = val;
}

void Resistance_oortmerssen::checkbox2click(void* sender) {

    field(QStringLiteral("lwlbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("bwlbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("wettedsurfacebox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("losbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("displacementbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("lcbbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("cpbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("kbulbbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("cstrnbox")) . enabled = false;
    field(QStringLiteral("babox")) . enabled = false;
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

void Resistance_oortmerssen::drafttotalboxaftersetvalue(void* sender) {
HydroInput hydobject{};
    if ((field(QStringLiteral("checkbox2")) . checked) && (this != nullptr))
        {
            ; // local immutable hydrostatic request
            hydobject . draft = fgetdrafttotal();
            hydobject.data = hydro(hydobject.draft);
            fsetlwl(hydValue(hydobject, QStringLiteral("lengthwaterline")));
            fsetlos(hydValue(hydobject, QStringLiteral("submax.x")) - hydValue(hydobject, QStringLiteral("submin.x")));
            fsetbwl(hydValue(hydobject, QStringLiteral("beamwaterline")));
            fsetwettedsurface(hydValue(hydobject, QStringLiteral("wettedsurface")));
            wlarea = hydValue(hydobject, QStringLiteral("waterplanearea"));
            ie_ = hydValue(hydobject, QStringLiteral("waterplaneentranceangle"));
            fsetkbulb(ie_);
            fsetdisplacement(hydValue(hydobject, QStringLiteral("volume")));
            if (fgetlwl() != 0)
                fsetlcb(100 * (hydValue(hydobject, QStringLiteral("centerofbuoyancy.x")) - (hydValue(hydobject, QStringLiteral("wlmin.x")) + 0.5f * fgetlwl())) / fgetlwl());
            else
                fsetlcb(0);
            fsetcp(hydValue(hydobject, QStringLiteral("prismcoefficient")));
            check();
        }
}

void Resistance_oortmerssen::estimateboxclick(void* sender) {

    field(QStringLiteral("wettedsurfacebox")) . enabled = ! field(QStringLiteral("estimatebox")) . checked;
}

void Resistance_oortmerssen::estimate2boxclick(void* sender) {

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
    fseta10(cm * fgetbwl() * fgetdrafttotal());
    if (fgeta11() == 0)
        fseta11(10);
}

void Resistance_oortmerssen::calculateresistanceoortmer(float convertedspeed, float lcb, float cp, float &rf, float &rr, float &w, float &t0, float &nr) {
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
float nscrew{};
float nballast{};
float nbulb{};
float nrud{};
float nbrac{};
float nboss{};
float nthrus{};
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
    if (fgetnp() == 0)
        nscrew = 1;
    if (fgetnp() > 2)
        nscrew = 2;
    nballast = 0;
    nbulb = 0;
    nrud = 0;
    nbrac = 0;
    nboss = 0;
    nthrus = 0;
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
            dat [4] = fgetdrafttotal() + fgetdraft() / 2.f;
            dat [5] = fgetdrafttotal() - fgetdraft() / 2.f;
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
            vs = vsr * 0.514444;
            c_ = rtotal / (0.5f * waterdensity * square (vs) * wetarea);
            dat [29] = c_;
            dat [30] = vsr;
            file_exportdatake (dat , dan);
        }
    dat [1] = fgetlwl();
    dat [2] = settings. projectlength;
    dat [3] = fgetbwl();
    dat [4] = fgetdrafttotal();
    dat [5] = cp * cm;
    dat [6] = cm;
    dat [7] = lcb;
    dat [8] = fgetkbulb();
    dat [9] = fgetdisplacement();
    dat [10] = wetarea;
    if (dat [10] == 0)
        dat [10] = s_;
    dat [11] = waterdensity;
    dat [12] = fgetviscosity();
    dat [13] = fgetstartspeed();
    dat [14] = fgetendspeed();
    dat [15] = fgetstepspeed();
    dat [16] = nscrew;
    dat [17] = nballast;
    dat [18] = nbulb;
    dat [19] = nrud;
    dat [20] = nbrac;
    dat [21] = nboss;
    dat [22] = nthrus;
    dat [23] = 0;
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
    t = fgetdrafttotal();
    tf = fgetdrafttotal() + fgetdraft() / 2.f;
    ta = fgetdrafttotal() - fgetdraft() / 2.f;
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
    if (lr < 0.2)
        {
            resultsmemo.append(space (10) + QStringLiteral("Lr = ") + number (lr , 0 , 6 , 4) + QStringLiteral(" < 0,2"));
            return;
        }
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
    ca = 0.006 * power ((l + 100) , (- 0.16)) - 0.00205 + 0.003 * sqrtp (l / 7.5f) * square (square (cb)) * c [2] * (0.04 - c [4]);
    if (fgetks() >= 150.f)
        ca = ca + (0.105 * power (fgetks() / 1000000.f , 0.333333) - 0.005579) / power (l , 0.33333);
    cp1 = 1.45 * cp - 0.315 - 0.0225 * lcb;
    if (cp1 > 1)
        return;
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

void Resistance_oortmerssen::file_exportdata(Span<float> dat, Span<float> dan) {
int i{};
TextFile ffile{};
    if (exists (pathfileold + QStringLiteral("TMP6.tsk")))
        check();
    ftmpfilename = QStringLiteral("TMP6.tsk");
    ffile.name = safePath(QStringLiteral("TMP6.tsk"));
    newFile(ffile);
    for (i=0; i<=29; ++i)
    {
        check();
        writeLine(ffile, dat [i]);
    }
    closeFile(ffile);
}

void Resistance_oortmerssen::file_exportdatake(Span<float> dat, Span<float> dan) {
int i{};
TextFile ffile{};
    ftmpfilename = QStringLiteral("TMPke.txt");
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

void Resistance_oortmerssen::comboboxclick(void* sender) {

    if (field(QStringLiteral("combobox")) . itemindex == 0)
        {
            ; // presentation only
        }
    if (field(QStringLiteral("combobox")) . itemindex == 1)
        {
            ; // presentation only
        }
    if (field(QStringLiteral("combobox")) . itemindex == 2)
        {
            ; // presentation only
        }
    if ((field(QStringLiteral("combobox")) . itemindex > 2) && (field(QStringLiteral("combobox")) . itemindex < 5))
        warning(tr(QStringLiteral("rs_this_calculation_method_is_here___series_by_oct_5_0181_75_and_other_methods____")));
    if (field(QStringLiteral("combobox")) . itemindex == 5)
        {
            ; // presentation only
        }
}

void Resistance_oortmerssen::calculateinworkspace() {
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
float nballast{};
int index{};
int nser{};
float speed{};
float stepspeed1{};
float lr{};
float l{};
float ke_{};
float cc{};
QString line{};
QString tmp{};
QString str{};
int units{};
bool stop{};
int i{};
int ii{};
int ispeed{};
TextFile ffile{};
Array<float,1,5> vs{};
Array<Array<float,1,11>,1,11> res{};
Array<QString,1,11> ss{};
QString pathfile{};
QString filetofind{};
QString pathfileold{};
QString fopendirectory{};
QString fexecdirectory{};
QString filename{};
QString outputstr{};
QString errorstr{};
QString execfullname{};
int exitstatus{};
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
    field(QStringLiteral("printbutton")) . enabled = false;
    resultsmemo2.clear();
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_oortmer_note_p1")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p2")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_oortmer_note_p3")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p4")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_oortmer_note_p5")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_oortmer_note_p6")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_oortmer_note_p7")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(QStringLiteral("Copyright (c) 2009-2012, Timoshenko V.F."));
    ; // presentation only
    if (correctinputdata())
        {
            units = settings. projectunits;
            resultsmemo.clear();
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            if (field(QStringLiteral("combobox")) . itemindex == 0)
                header (space (10) + tr(QStringLiteral("rs_program_of_resistance_and_power_prediction_for_tugs_and_trawlers__nuos__ukraine_2009_")) + QStringLiteral(".") , resultsmemo);
            if (field(QStringLiteral("combobox")) . itemindex == 1)
                header (space (10) + tr(QStringLiteral("rs_resistance_and_power_of_tugs_and_trawlers_by_ridgeley_nevitt_1967_method")) + QStringLiteral(".") , resultsmemo);
            if (field(QStringLiteral("combobox")) . itemindex == 2)
                header (space (10) + tr(QStringLiteral("rs_resistance_and_power_for_small_displacement_ships_by_ubc_1993_method")) + QStringLiteral(".") , resultsmemo);
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
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_length_over_surface_los")) , 40) + QStringLiteral(" : ") + number (fgetlos() , 0 , 6 , 2) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_beam_on_waterline")) , 40) + QStringLiteral(" : ") + number (fgetbwl() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_draft_on_midship")) , 40) + QStringLiteral(" : ") + number (fgetdrafttotal() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_draft_on_f_p_")) , 40) + QStringLiteral(" : ") + number (fgetdrafttotal() + fgetdraft() / 2.f , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (14) + pad (tr(QStringLiteral("rs_draft_on_f_a_")) , 40) + QStringLiteral(" : ") + number (fgetdrafttotal() - fgetdraft() / 2.f , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
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
                    s = fgetlwl() * (2 * fgetdrafttotal() + fgetbwl()) * sqrtp (cm) * (0.453 + 0.442 * fgetcp() * cm - 0.2862 * cm - 0.003467 * fgetbwl() / fgetdrafttotal() + 0.3696 * cwp) + 2.38 * fgetkbulb() / fgetcp() * cm;
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
            psi = lpp / power (fgetdisplacement() , 0.3333);
            cb = fgetcp() * cm;
            ta = fgetdrafttotal() - fgetdraft() / 2.f;
            nballast = 0;
            lr = lpp * (1 - fgetcp() + 0.06 * fgetcp() * fgetlcb() / (4 * fgetcp() - 1));
            if (fgetkbulb() == 0)
                fsetkbulb(1.f + 89.f * expp (- power (fgetlwl() / fgetbwl() , 0.80856) * power ((1 - cwp) , 0.30484) * power ((1 - fgetcp() - 0.0225 * fgetlcb()) , 0.6367) * power ((lr / fgetbwl()) , 0.34574) * power ((100 * fgetdisplacement() / fgetlwl() / fgetlwl() / fgetlwl()) , 0.16302)));
            resultsmemo.append(space (14) + QStringLiteral("Cp            = ") + number (fgetcp() , 0 , 6 , 4));
            resultsmemo.append(space (14) + QStringLiteral("Cb            = ") + number (fgetcp() * cm , 0 , 6 , 4));
            resultsmemo.append(space (14) + QStringLiteral("Cwp           = ") + number (cwp , 0 , 6 , 4));
            resultsmemo.append(space (14) + QStringLiteral("Cm            = ") + number (cm , 0 , 6 , 4));
            resultsmemo.append(space (14) + QStringLiteral("Ld=(Lwl+Lpp)/2= ") + number ((fgetlwl() + lpp) / 2 , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (14) + QStringLiteral("Am            = ") + number (am , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(space (14) + QStringLiteral("L/B           = ") + number (fgetlwl() / fgetbwl() , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("ie            = ") + number (fgetkbulb() , 0 , 6 , 3) + QStringLiteral(" degr."));
            resultsmemo.append(space (14) + QStringLiteral("B/T           = ") + number (fgetbwl() / fgetdrafttotal() , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Lwl/T         = ") + number (fgetlwl() / fgetdrafttotal() , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("L/V^0.333     = ") + number (psi , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Vms/L^0,5     = ") + number (vs [5] * 0.51444 / sqrtp ((fgetlwl() + lpp) / 2) , 0 , 6 , 3));
            resultsmemo.append(space (14) + QStringLiteral("Np            = ") + number (fgetnp() , 0 , 6 , 0));
            resultsmemo.append(QStringLiteral(""));
            if (field(QStringLiteral("combobox")) . itemindex == 0)
                {
                    fr_max = vs [5] * 0.514444 / sqrtp (9.81 * (fgetlwl() + lpp) / 2);
                    psi = vs [5] * 0.514444 / sqrtp ((fgetlwl() + lpp) / 2);
                    if ((fgetlwl() > 80) || (fgetlwl() < 8))
                        resultsmemo.append(space (14) + QStringLiteral("Lwl        ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   8 ... 80 m"));
                    if ((fgetdisplacement() > 3000) || (fgetdisplacement() < 5))
                        resultsmemo.append(space (14) + QStringLiteral("V          ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   5 ... 3000 m3"));
                    if ((fgetlwl() / fgetbwl() > 6.2) || (fgetlwl() / fgetbwl() < 3.0f))
                        resultsmemo.append(space (14) + QStringLiteral("L/B        ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 3,0 ... 6,2"));
                    if ((fgetbwl() / fgetdrafttotal() > 4.0f) || (fgetbwl() / fgetdrafttotal() < 1.9))
                        resultsmemo.append(space (14) + QStringLiteral("B/T        ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 1,9 ... 4,0"));
                    if ((cb / cm > 0.73) || (cb / cm < 0.5f))
                        resultsmemo.append(space (14) + QStringLiteral("Cp         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,5 ... 0,73"));
                    if ((cm > 0.97) || (cm < 0.7))
                        resultsmemo.append(space (14) + QStringLiteral("Cm         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,7 ... 0,97"));
                    if ((fgetlcb() > 2.8) || (fgetlcb() < - 7))
                        resultsmemo.append(space (14) + QStringLiteral("LCB        ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("  -7 ... 2,8 % L"));
                    if ((fgetkbulb() > 46) || (fgetkbulb() < 10))
                        resultsmemo.append(space (14) + QStringLiteral("ie         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("  10 ... 46 degr"));
                    if ((fr_max > 0.5f) || (fr_max < 0))
                        resultsmemo.append(space (14) + QStringLiteral("Fr         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   0 ... 0,5"));
                    if ((psi > 1.79) || (psi < 0))
                        resultsmemo.append(space (14) + QStringLiteral("Vms/sqrt(L)") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("   0 ... 1,79"));
                }
            if (field(QStringLiteral("combobox")) . itemindex == 1)
                {
                    psi = vs [5] * 0.514444 / sqrtp (fgetlwl());
                    if ((fgetlwl() / fgetbwl() > 5.2) || (fgetlwl() / fgetbwl() < 4.0f))
                        resultsmemo.append(space (14) + QStringLiteral("L/B          ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 4,0 ... 5,2"));
                    if ((cb / cm > 0.70) || (cb / cm < 0.55))
                        resultsmemo.append(space (14) + QStringLiteral("Cp           ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,55 ... 0,73"));
                    if ((psi > 1.5f) || (psi < 0.7))
                        resultsmemo.append(space (14) + QStringLiteral("Vms/sqrt(L)  ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,7  ... 1,50"));
                    psi = fgetdisplacement() / power (0.01 * fgetlwl() / 0.3048 , 3);
                    if ((psi > 500) || (psi < 200))
                        resultsmemo.append(space (14) + QStringLiteral("V/(0,01*L)^3 ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 200  ... 500"));
                }
            if (field(QStringLiteral("combobox")) . itemindex == 2)
                {
                    fr_max = vs [5] * 0.514444 / sqrtp (9.81 * (fgetlwl() + lpp) / 2);
                    if ((fgetlwl() / fgetbwl() > 4.0f) || (fgetlwl() / fgetbwl() < 2.6))
                        resultsmemo.append(space (14) + QStringLiteral("L/B        ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("  2,6 ... 3,98"));
                    if ((fgetbwl() / fgetdrafttotal() > 4.23) || (fgetbwl() / fgetdrafttotal() < 2.0f))
                        resultsmemo.append(space (14) + QStringLiteral("B/T        ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("  2,0 ... 4,23"));
                    if ((cb / cm > 0.61) || (cb / cm < 0.45))
                        resultsmemo.append(space (14) + QStringLiteral("Cp         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,45 ... 0,61"));
                    if ((cm > 0.878) || (cm < 0.747))
                        resultsmemo.append(space (14) + QStringLiteral("Cm         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,747 ... 0,878"));
                    if ((psi > 5.31) || (psi < 3))
                        resultsmemo.append(space (14) + QStringLiteral("L/V^0.333  ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("    3 ... 5,31"));
                    if ((fr_max > 0.5f) || (fr_max < 0.2))
                        resultsmemo.append(space (14) + QStringLiteral("Fr         ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral("    0 ... 0,5"));
                }
            speed = vs [4];
            convertedspeed = speed * 1852 / 3600;
            calculateresistanceoortmer (convertedspeed , fgetlcb() , fgetcp() , rf , rr , w , t0 , nr);
            if ((fgetlwl() > 0) && (fgetbwl() > 0) && (fgetdp() > 0) && (fgetnp() > 0) && (ta > 0))
                {
                    fexecdirectory = solverDirectory;
                    check();
                    check();
                    filetofind = findFile (QStringLiteral("TMP6.tsk") , work);
                    if (filetofind != QStringLiteral("TMP6.tsk"))
                        {
                            warning(tr(QStringLiteral("rs_have_not_input_file_for_calculation_is_here___")));
                            return;
                        }
                    if (field(QStringLiteral("combobox")) . itemindex == 0)
                        {
                            ; // presentation only
                            execfullname = fexecdirectory + QStringLiteral("/") + QStringLiteral("OORTMERS.EXE");
                            runSolver(execfullname, fcoortmer);
                            nser = 1;
                        }
                    if (field(QStringLiteral("combobox")) . itemindex == 1)
                        {
                            ; // presentation only
                            execfullname = fexecdirectory + QStringLiteral("/") + QStringLiteral("RNTSP.EXE");
                            runSolver(execfullname, fcoortmer);
                            nser = 2;
                        }
                    if (field(QStringLiteral("combobox")) . itemindex == 2)
                        {
                            ; // presentation only
                            execfullname = fexecdirectory + QStringLiteral("/") + QStringLiteral("UBCRT.EXE");
                            runSolver(execfullname, fcoortmer);
                            nser = 3;
                        }
                    if (field(QStringLiteral("combobox")) . itemindex == 5)
                        {
                            ; // presentation only
                            yTitle = tr(QStringLiteral("rs_resistance"));
                            rightTitle = tr(QStringLiteral("rs_kn"));
                            setSeriesTitle(QStringLiteral("series1"), QStringLiteral("van Oortmerssen"));
                            setSeriesTitle(QStringLiteral("series2"), QStringLiteral("Ridgeley-Nevitt"));
                            setSeriesTitle(QStringLiteral("series3"), QStringLiteral("UBC"));
                            setSeriesTitle(QStringLiteral("series4"), QStringLiteral(" "));
                            for (ii=1; ii<=10; ++ii)
                            {
                                check();
                                {
                                    point(QStringLiteral("series1"), vsres [1][ii] ,  restot [1][ii] );
                                    point(QStringLiteral("series2"), vsres [2][ii] ,  restot [2][ii] );
                                    point(QStringLiteral("series3"), vsres [3][ii] ,  restot [3][ii] );
                                    point(QStringLiteral("series4"), vsres [3][ii] ,  0 );
                                }
                            }
                            if (exists (QStringLiteral("TMP6.TSK")))
                                check();
                            return;
                        }
                    filename = QStringLiteral("oortmers.res");
                    i = 1;
                    label_newsearch:;
                    filetofind = findFile (QStringLiteral("TMP6.tsk") , work);
                    if (filetofind == QStringLiteral("TMP6.tsk"))
                        {
                            check();
                            i = i + 1;
                            if (i < 25)
                                goto label_newsearch;
                            else
                                {
                                    if (exists (QStringLiteral("TMP6.tsk")))
                                        check();
                                    warning(tr(QStringLiteral("rs_do_not_calculate_this_task__because_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_1__do_not_found_into__exec_or_was_damaged_file")) + QStringLiteral(" EXE file ") + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_2__input_datas_are_outside_valid_domain_or_catalog_is_not_valid_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_3__this_is_very_slow_computer__fcpu___800_mhz_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_4__cpu_is_loaded_more_80__another_processes_")));
                                    return;
                                }
                        }
                    if (exists (QStringLiteral("oortmers.res")))
                        {
                            ftmpfilename = QStringLiteral("oortmers.res");
                            ffile.name = safePath(QStringLiteral("oortmers.res"));
                            readFile(ffile);
                            ii = 0;
                            while ((! atEnd (ffile)) && (ii < 10)) {
                                check();
                                {
                                    ii = ii + 1;
                                    for (i=1; i<=8; ++i)
                                    {
                                        check();
                                        readValues(ffile, res [i][ii]);
                                    }
                                    restot [nser][ii] = res [7][ii] * 10;
                                    vsres [nser][ii] = res [1][ii];
                                    if (res [6][ii] < 0)
                                        {
                                            resultsmemo.append(space (14) + tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                                            warning(tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                                            ; // presentation only
                                            closeFile(ffile);
                                            if (exists (QStringLiteral("oortmers.res")))
                                                check();
                                            return;
                                        }
                                }
                            }
                            closeFile(ffile);
                            check();
                        }
                    rtotal = res [6][7];
                    if (fgetke() == 0)
                        {
                            fexecdirectory = solverDirectory;
                            check();
                            check();
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
                                            warning(tr(QStringLiteral("rs_do_not_calculate_this_task__because_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_1__do_not_found_into__exec_or_was_damaged_file")) + QStringLiteral(" SeaMargn.EXE ") + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + userstring (1140) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_3__this_is_very_slow_computer__fcpu___800_mhz_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_4__cpu_is_loaded_more_80__another_processes_")));
                                            fsetke(1);
                                            return;
                                        }
                                }
                            ftmpfilename = QStringLiteral("OUT.TXT");
                            ffile.name = safePath(QStringLiteral("OUT.TXT"));
                            readFile(ffile);
                            readLine(ffile, ke_);
                            closeFile(ffile);
                            if (exists (QStringLiteral("OUT.TXT")))
                                check();
                            fsetke(ke_);
                        }
                    if (fgetke() < 1)
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
                                            point(QStringLiteral("series2"), res [1][ii] ,  res [9][ii] );
                                            point(QStringLiteral("series4"), res [1][ii] ,  res [10][ii] );
                                        }
                                    point(QStringLiteral("series1"), res [1][ii] ,  res [7][ii] );
                                    point(QStringLiteral("series3"), res [1][ii] ,  res [8][ii] );
                                }
                        }
                    }
                }
            ftmpfilename = QStringLiteral("RESISTp.dat");
            ffile.name = safePath(trailing (work) + QStringLiteral("RESISTp.dat"));
            newFile(ffile);
            writeLine(ffile, QStringLiteral("#     Nser      Np       Wt        t       Eta_R     Dp"));
            writeText(ffile, QStringLiteral("       53 "));
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
            ftmpfilename = QStringLiteral("RESIST.dat");
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
            if (field(QStringLiteral("combobox")) . itemindex == 0)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_by_method_van_oortmerssen_1971")));
            if (field(QStringLiteral("combobox")) . itemindex == 1)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_by_ridgeley_nevitt_1967_method")));
            if (field(QStringLiteral("combobox")) . itemindex == 2)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_by_ubc_1993_method")));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_t________t________t________t________t________t________t________t________t________t")));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___vs___l___vms__l___fr___l___r_f__l___r_r__l___r_t__l___pe___l__r_t_e_l__pe_e__l")));
            setSeriesTitle(QStringLiteral("series1"), QStringLiteral("Rt"));
            setSeriesTitle(QStringLiteral("series2"), QStringLiteral("Rt_e"));
            setSeriesTitle(QStringLiteral("series3"), QStringLiteral("Pe"));
            setSeriesTitle(QStringLiteral("series4"), QStringLiteral("Pe_e"));
            yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral("*10, ") + tr(QStringLiteral("rs_kn"));
            rightTitle = tr(QStringLiteral("rs_power")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kw"));
            if (flag == 1)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn___l___m_s__l________l___kn___l___kn___l___kn___l___kw___l___kn___l___kw___l")));
            else
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn___l___m_s__l________l____n___l____n___l____n___l___w____l____n___l___w____l")));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_t________t________t________t________t________t________t________t________t________t")));
            index = 0;
            for (ii=1; ii<=10; ++ii)
            {
                check();
                {
                    for (i=1; i<=10; ++i)
                    {
                        check();
                        {
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
                    ta = fgetdrafttotal() - fgetdraft() / 2.f;
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
            resultsmemo.append(space (10) + QStringLiteral("Copyright (c) 2009-2012, Timoshenko V.F."));
            ; // presentation only
            field(QStringLiteral("printbutton")) . enabled = true;
        }
    check();
}

bool Resistance_oortmerssen::execute() {
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
    field(QStringLiteral("zbulbbox")) . enabled = false;
    field(QStringLiteral("babox")) . enabled = false;
    field(QStringLiteral("draftbox")) . enabled = false;
    field(QStringLiteral("cstrnbox")) . enabled = false;
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
    comboboxclick (nullptr);
    result = false;
    return result;
}
}
