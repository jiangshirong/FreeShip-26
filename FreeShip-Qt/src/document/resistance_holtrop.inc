// GPL-2.0-or-later. Generated from FreeResistance_HoltrDlg.pas.
namespace {

struct Resistance_holtrop final: ResistanceBase {
using ResistanceBase::ResistanceBase;
float b_t{};
float l_d{};
float a_d{};
float l_b{};
float vsr{};
float am{};
float cm{};
float cwp{};
float ie_{};
float ie{};
float ae0{};
float p_d0{};
float nver{};
float z0{};
float tc{};
int iflag{};
int iflag1{};
int jspeed{};
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
float fgetwlarea();
void fsetwlarea(float val);
float fgetstepspeed();
void fsetstepspeed(float val);
bool fgetextractfromhull();
void fsetextractfromhull(bool val);
float optimumcph(float speed);
float optimumlcbh(float speed);
void checkbox2click(void* sender);
void drafttotalboxaftersetvalue(void* sender);
void estimateboxclick(void* sender);
void estimate2boxclick(void* sender);
void estimate3boxclick(void* sender);
void estimate4boxclick(void* sender);
void calculateresistanceholtr(float convertedspeed, float lcb, float cp, float &rf, float &rr, float &w, float &t0, float &nr);
void resist(float vms, Span<float> dat, float &rf, float &rr, float &w, float &t0, float &nr);
void file_exportdata(Span<float> dat, Span<float> dan);
void file_exportdata84(Span<float> dat, Span<float> dan);
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
if (member==QStringLiteral("wlarea")) { fsetwlarea(value); return; }
ResistanceBase::loadProperty(member,value);
}
void booleanChanged(const QString &key) override { if (key==QStringLiteral("estimatebox")) estimateboxclick(nullptr);if (key==QStringLiteral("estimate2box")) estimate2boxclick(nullptr);if (key==QStringLiteral("estimate3box")) estimate3boxclick(nullptr);if (key==QStringLiteral("estimate4box")) estimate4boxclick(nullptr); }
void extract() override { field("checkbox2").checked=true;checkbox2click(nullptr);field("checkbox2").checked=false;checkbox2click(nullptr); }
void initialise() override { execute(); }
void choiceChanged(const QString &key) override {  }
void runCalculation() override { calculateinworkspace(); }
};
bool Resistance_holtrop::correctinputdata() {
bool result{};
if(tc==0)tc=fgetdrafttotal();

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
    if ((fgetwlarea() <= 0))
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
        fsetstepspeed(1.0f);
    if ((fgetdraft() <= 0.02) || (fgetlwl() <= 0) || (fgetbwl() <= 0) || (fgetwlarea() <= 0) || (fgetdisplacement() <= 0) || (fgetdrafttotal() < fgetdraft()) || (fgetcp() <= 0) || (fgetviscosity() <= 0) || (fgetendspeed() <= fgetstartspeed()))
        return result;
    if (tc == 0)
        tc = fgetdrafttotal();
    b_t = fgetbwl() / tc;
    l_d = fgetlwl() / power (fgetdisplacement() , 1.0 / 3);
    a_d = fgetwlarea() / power (fgetdisplacement() , 2.0 / 3);
    l_b = fgetlwl() / fgetbwl();
    cm = fgetcp() / fgetdisplacement() * (fgetlwl() * fgetbwl() * tc);
    am = cm * tc * fgetbwl();
    if (cm >= 1.f)
        {
            am = fgetdisplacement() / (fgetlwl() * fgetcp());
            cm = am / (fgetbwl() * tc);
        }
    cwp = fgetwlarea() / (fgetlwl() * fgetbwl());
    if ((cwp >= 1) || (fgetcp() >= 1))
        {
            warning(tr(QStringLiteral("rs_error__cp_1_or_cwp_1")));
            return result;
        }
    if ((fgetzbulb() / 0.8 > tc + fgetdraft() / 2))
        {
            warning(tr(QStringLiteral("rslocbulblimit")));
            return result;
        }
    result = true;
    return result;
}

float Resistance_holtrop::fgetstartspeed() {
float result{};

    result = field(QStringLiteral("startspeedbox")) . value;
    return result;
}

void Resistance_holtrop::fsetstartspeed(float val) {

    field(QStringLiteral("startspeedbox")).set(val);
}

float Resistance_holtrop::fgetendspeed() {
float result{};

    result = field(QStringLiteral("endspeedbox")) . value;
    return result;
}

void Resistance_holtrop::fsetendspeed(float val) {

    field(QStringLiteral("endspeedbox")).set(val);
}

float Resistance_holtrop::fgetdensity() {
float result{};

    result = field(QStringLiteral("densitybox")) . value;
    return result;
}

void Resistance_holtrop::fsetdensity(float val) {

    field(QStringLiteral("densitybox")).set(val);
}

float Resistance_holtrop::fgetdisplacement() {
float result{};

    result = field(QStringLiteral("displacementbox")) . value;
    return result;
}

void Resistance_holtrop::fsetdisplacement(float val) {

    field(QStringLiteral("displacementbox")).set(val);
}

float Resistance_holtrop::fgetdraft() {
float result{};

    result = field(QStringLiteral("draftbox")) . value;
    return result;
}

void Resistance_holtrop::fsetdraft(float val) {

    field(QStringLiteral("draftbox")).set(val);
}

float Resistance_holtrop::fgetdrafttotal() {
float result{};

    result = field(QStringLiteral("drafttotalbox")) . value;
    return result;
}

void Resistance_holtrop::fsetdrafttotal(float val) {

    field(QStringLiteral("drafttotalbox")).set(val);
}

float Resistance_holtrop::fgetlwl() {
float result{};

    result = field(QStringLiteral("lwlbox")) . value;
    return result;
}

void Resistance_holtrop::fsetlwl(float val) {

    field(QStringLiteral("lwlbox")).set(val);
}

float Resistance_holtrop::fgetlcb() {
float result{};

    result = field(QStringLiteral("lcbbox")) . value;
    return result;
}

void Resistance_holtrop::fsetlcb(float val) {

    field(QStringLiteral("lcbbox")).set(val);
}

void Resistance_holtrop::fsetke(float val) {

    field(QStringLiteral("kebox")).set(val);
    if (field(QStringLiteral("kebox")) . value == 0)
        field(QStringLiteral("kebox")).set(1);
}

float Resistance_holtrop::fgetke() {
float result{};

    result = field(QStringLiteral("kebox")) . value;
    return result;
}

void Resistance_holtrop::fsetk1(float val) {

    field(QStringLiteral("k1box")).set(val);
}

float Resistance_holtrop::fgetk1() {
float result{};

    result = field(QStringLiteral("k1box")) . value;
    return result;
}

void Resistance_holtrop::fsetk2(float val) {

    field(QStringLiteral("k2box")).set(val);
}

float Resistance_holtrop::fgetk2() {
float result{};

    result = field(QStringLiteral("k2box")) . value;
    return result;
}

void Resistance_holtrop::fsetk3(float val) {

    field(QStringLiteral("k3box")).set(val);
}

float Resistance_holtrop::fgetk3() {
float result{};

    result = field(QStringLiteral("k3box")) . value;
    return result;
}

void Resistance_holtrop::fsetk4(float val) {

    field(QStringLiteral("k4box")).set(val);
}

float Resistance_holtrop::fgetk4() {
float result{};

    result = field(QStringLiteral("k4box")) . value;
    return result;
}

void Resistance_holtrop::fsetk5(float val) {

    field(QStringLiteral("k5box")).set(val);
}

float Resistance_holtrop::fgetk5() {
float result{};

    result = field(QStringLiteral("k5box")) . value;
    return result;
}

void Resistance_holtrop::fsetk6(float val) {

    field(QStringLiteral("k6box")).set(val);
}

float Resistance_holtrop::fgetk6() {
float result{};

    result = field(QStringLiteral("k6box")) . value;
    return result;
}

void Resistance_holtrop::fsetk7(float val) {

    field(QStringLiteral("k7box")).set(val);
}

float Resistance_holtrop::fgetk7() {
float result{};

    result = field(QStringLiteral("k7box")) . value;
    return result;
}

void Resistance_holtrop::fseta1(float val) {

    field(QStringLiteral("a1box")).set(val);
}

float Resistance_holtrop::fgeta1() {
float result{};

    result = field(QStringLiteral("a1box")) . value;
    return result;
}

void Resistance_holtrop::fseta2(float val) {

    field(QStringLiteral("a2box")).set(val);
}

float Resistance_holtrop::fgeta2() {
float result{};

    result = field(QStringLiteral("a2box")) . value;
    return result;
}

void Resistance_holtrop::fseta3(float val) {

    field(QStringLiteral("a3box")).set(val);
}

float Resistance_holtrop::fgeta3() {
float result{};

    result = field(QStringLiteral("a3box")) . value;
    return result;
}

void Resistance_holtrop::fseta4(float val) {

    field(QStringLiteral("a4box")).set(val);
}

float Resistance_holtrop::fgeta4() {
float result{};

    result = field(QStringLiteral("a4box")) . value;
    return result;
}

void Resistance_holtrop::fseta5(float val) {

    field(QStringLiteral("a5box")).set(val);
}

float Resistance_holtrop::fgeta5() {
float result{};

    result = field(QStringLiteral("a5box")) . value;
    return result;
}

void Resistance_holtrop::fseta6(float val) {

    field(QStringLiteral("a6box")).set(val);
}

float Resistance_holtrop::fgeta6() {
float result{};

    result = field(QStringLiteral("a6box")) . value;
    return result;
}

void Resistance_holtrop::fseta7(float val) {

    field(QStringLiteral("a7box")).set(1.226);
}

float Resistance_holtrop::fgeta7() {
float result{};

    result = field(QStringLiteral("a7box")) . value;
    return result;
}

void Resistance_holtrop::fseta8(float val) {

    field(QStringLiteral("a8box")).set(val);
}

float Resistance_holtrop::fgeta8() {
float result{};

    result = field(QStringLiteral("a8box")) . value;
    return result;
}

void Resistance_holtrop::fseta9(float val) {

    field(QStringLiteral("a9box")).set(val);
}

float Resistance_holtrop::fgeta9() {
float result{};

    result = field(QStringLiteral("a9box")) . value;
    return result;
}

void Resistance_holtrop::fseta10(float val) {

    field(QStringLiteral("a10box")).set(val);
}

float Resistance_holtrop::fgeta10() {
float result{};

    result = field(QStringLiteral("a10box")) . value;
    return result;
}

void Resistance_holtrop::fseta11(int val) {

    field(QStringLiteral("a11box")) . itemindex = val - 1;
}

int Resistance_holtrop::fgeta11() {
int result{};

    result = field(QStringLiteral("a11box")) . itemindex + 1;
    return result;
}

void Resistance_holtrop::fsetba(float val) {

    field(QStringLiteral("babox")).set(val);
}

float Resistance_holtrop::fgetba() {
float result{};

    result = field(QStringLiteral("babox")) . value;
    return result;
}

void Resistance_holtrop::fsetkbulb(float val) {

    field(QStringLiteral("kbulbbox")).set(val);
}

float Resistance_holtrop::fgetkbulb() {
float result{};

    result = field(QStringLiteral("kbulbbox")) . value;
    return result;
}

void Resistance_holtrop::fsetzbulb(float val) {

    field(QStringLiteral("zbulbbox")).set(val);
}

float Resistance_holtrop::fgetzbulb() {
float result{};

    result = field(QStringLiteral("zbulbbox")) . value;
    return result;
}

void Resistance_holtrop::fsetcstrn(float val) {

    field(QStringLiteral("cstrnbox")).set(val);
}

float Resistance_holtrop::fgetcstrn() {
float result{};

    result = field(QStringLiteral("cstrnbox")) . value;
    return result;
}

void Resistance_holtrop::fsetnp(float val) {

    field(QStringLiteral("npbox")).set(val);
}

float Resistance_holtrop::fgetnp() {
float result{};

    result = field(QStringLiteral("npbox")) . value;
    return result;
}

void Resistance_holtrop::fsetdp(float val) {

    field(QStringLiteral("dpbox")).set(val);
}

float Resistance_holtrop::fgetdp() {
float result{};

    result = field(QStringLiteral("dpbox")) . value;
    if ((field(QStringLiteral("dpbox")) . value != 0) && (fgetnp() == 0))
        field(QStringLiteral("dpbox")).set(0.0f);
    if ((field(QStringLiteral("dpbox")) . value == 0) && (fgetnp() == 1))
        field(QStringLiteral("dpbox")).set((fgetdrafttotal() - fgetdraft() / 2) * 0.75f);
    if ((field(QStringLiteral("dpbox")) . value == 0) && (fgetnp() == 2))
        field(QStringLiteral("dpbox")).set((fgetdrafttotal() - fgetdraft() / 2) * 0.70);
    return result;
}

void Resistance_holtrop::fsetks(float val) {

    field(QStringLiteral("ksbox")).set(val);
}

float Resistance_holtrop::fgetks() {
float result{};

    result = field(QStringLiteral("ksbox")) . value;
    return result;
}

float Resistance_holtrop::fgetkeelchordlength() {
float result{};

    result = field(QStringLiteral("keelchordlengthbox")) . value;
    return result;
}

void Resistance_holtrop::fsetkeelchordlength(float val) {

    field(QStringLiteral("keelchordlengthbox")).set(val);
}

float Resistance_holtrop::fgetkeelarea() {
float result{};

    result = field(QStringLiteral("keelareabox")) . value;
    return result;
}

void Resistance_holtrop::fsetkeelarea(float val) {

    field(QStringLiteral("keelareabox")).set(val);
}

float Resistance_holtrop::fgetrudderchordlength() {
float result{};

    result = field(QStringLiteral("rudderchordlengthbox")) . value;
    return result;
}

void Resistance_holtrop::fsetrudderchordlength(float val) {

    field(QStringLiteral("rudderchordlengthbox")).set(val);
}

float Resistance_holtrop::fgetrudderarea() {
float result{};

    result = field(QStringLiteral("rudderareabox")) . value;
    return result;
}

void Resistance_holtrop::fsetrudderarea(float val) {

    field(QStringLiteral("rudderareabox")).set(val);
}

float Resistance_holtrop::fgetbwl() {
float result{};

    result = field(QStringLiteral("bwlbox")) . value;
    return result;
}

void Resistance_holtrop::fsetbwl(float val) {

    field(QStringLiteral("bwlbox")).set(val);
}

float Resistance_holtrop::fgetcp() {
float result{};

    result = field(QStringLiteral("cpbox")) . value;
    return result;
}

void Resistance_holtrop::fsetcp(float val) {

    field(QStringLiteral("cpbox")).set(val);
}

float Resistance_holtrop::fgetviscosity() {
float result{};

    result = field(QStringLiteral("viscositybox")) . value;
    return result;
}

void Resistance_holtrop::fsetviscosity(float val) {

    field(QStringLiteral("viscositybox")).set(val);
}

float Resistance_holtrop::fgetwettedsurface() {
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
                    cwp = fgetwlarea() / (fgetlwl() * fgetbwl());
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

void Resistance_holtrop::fsetwettedsurface(float val) {

    field(QStringLiteral("wettedsurfacebox")).set(val);
}

float Resistance_holtrop::fgetwlarea() {
float result{};

    result = field(QStringLiteral("wlareabox")) . value;
    return result;
}

void Resistance_holtrop::fsetwlarea(float val) {

    field(QStringLiteral("wlareabox")).set(val);
}

float Resistance_holtrop::fgetstepspeed() {
float result{};

    result = field(QStringLiteral("stepspeedbox")) . value;
    return result;
}

void Resistance_holtrop::fsetstepspeed(float val) {

    field(QStringLiteral("stepspeedbox")).set(val);
}

bool Resistance_holtrop::fgetextractfromhull() {
bool result{};

    result = field(QStringLiteral("checkbox2")) . checked;
    return result;
}

void Resistance_holtrop::fsetextractfromhull(bool val) {

    if (field(QStringLiteral("checkbox2")) . checked != val)
        field(QStringLiteral("checkbox2")) . checked = val;
}

float Resistance_holtrop::optimumcph(float speed) {
float result{};
float tmpcp{};
float rmin{};
float rf{};
float rr{};
float rt{};
float w{};
float t0{};
float nr{};
float optimum{};
    result = 0;
    return result;
    rmin = std::numeric_limits<float>::infinity();
    tmpcp = 0.30;
    optimum = fgetcp();
    while (tmpcp <= 0.9) {
        check();
        {
            calculateresistanceholtr (speed , fgetlcb() , tmpcp , rf , rr , w , t0 , nr);
            rt = rf + rr;
            if (rt < rmin)
                {
                    rmin = rt;
                    optimum = tmpcp;
                }
            tmpcp = tmpcp + 0.01;
        }
    }
    result = optimum;
    return result;
}

float Resistance_holtrop::optimumlcbh(float speed) {
float result{};
float tmplcb{};
float rmin{};
float rf{};
float rr{};
float rt{};
float w{};
float t0{};
float nr{};
float optimum{};
    result = 0;
    return result;
    rmin = std::numeric_limits<float>::infinity();
    tmplcb = - 6;
    optimum = tmplcb;
    while (tmplcb <= 0) {
        check();
        {
            calculateresistanceholtr (speed , tmplcb , fgetcp() , rf , rr , w , t0 , nr);
            rt = rf + rr;
            if (rt < rmin)
                {
                    rmin = rt;
                    optimum = tmplcb;
                }
            tmplcb = tmplcb + 0.01;
        }
    }
    result = optimum;
    return result;
}

void Resistance_holtrop::checkbox2click(void* sender) {
int editfontcolor{};
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    field(QStringLiteral("estimatebox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    editfontcolor = 0;
    if (field(QStringLiteral("checkbox2")) . checked)
        editfontcolor = 2;
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
    ; // presentation only
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

void Resistance_holtrop::drafttotalboxaftersetvalue(void* sender) {
HydroInput hydobject{};
    tc = fgetdrafttotal();
    if ((field(QStringLiteral("checkbox2")) . checked) && (this != nullptr))
        {
            ; // local immutable hydrostatic request
            hydobject . draft = fgetdrafttotal();
            hydobject.data = hydro(hydobject.draft);
            fsetlwl(hydValue(hydobject, QStringLiteral("lengthwaterline")));
            fsetbwl(hydValue(hydobject, QStringLiteral("beamwaterline")));
            z0 = hydValue(hydobject, QStringLiteral("modelmin.z"));
            tc = fgetdrafttotal() + z0;
            fsetwettedsurface(hydValue(hydobject, QStringLiteral("wettedsurface")));
            fsetwlarea(hydValue(hydobject, QStringLiteral("waterplanearea")));
            am = hydValue(hydobject, QStringLiteral("midshiparea"));
            fsetdisplacement(hydValue(hydobject, QStringLiteral("volume")));
            if (fgetlwl() != 0)
                fsetlcb(100 * (hydValue(hydobject, QStringLiteral("centerofbuoyancy.x")) - (hydValue(hydobject, QStringLiteral("wlmin.x")) + 0.5f * fgetlwl())) / fgetlwl());
            else
                fsetlcb(0);
            fsetcp(hydValue(hydobject, QStringLiteral("prismcoefficient")));
            ie_ = hydValue(hydobject, QStringLiteral("waterplaneentranceangle"));
            if (hydValue(hydobject, QStringLiteral("bulbsectionarea")) > 0.1)
                {
                    fsetkbulb(hydValue(hydobject, QStringLiteral("bulbsectionarea")));
                    fsetzbulb(hydValue(hydobject, QStringLiteral("bulbsectioncog.z")));
                }
            check();
        }
}

void Resistance_holtrop::estimateboxclick(void* sender) {

    field(QStringLiteral("wettedsurfacebox")) . enabled = ! field(QStringLiteral("estimatebox")) . checked;
}

void Resistance_holtrop::estimate2boxclick(void* sender) {

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
        fseta11(2);
}

void Resistance_holtrop::estimate3boxclick(void* sender) {

}

void Resistance_holtrop::estimate4boxclick(void* sender) {

}

void Resistance_holtrop::calculateresistanceholtr(float convertedspeed, float lcb, float cp, float &rf, float &rr, float &w, float &t0, float &nr) {
QString processcallerdirectory{};
int processcallercursor{};
float rn_keel{};
float cf_keel{};
float rf_keel{};
double vs{};
double c{};
double ke_{};
float rn_rudder{};
float cf_rudder{};
float rf_rudder{};
float fraction{};
float froudenumber{};
float t{};
Array<float,1,30> dat{};
Array<float,1,15> dan{};
Array<double,0,9> factors{};
int a{};
int i{};
int lower{};
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
QString str{};
QString filetofind{};
QString fopendirectory{};
QString ftempdirectory{};
QString fexecdirectory{};
QString pathfileold{};
QString filename{};
QString outputstr{};
QString errorstr{};
QString execfullname{};
int exitstatus{};
TextFile ffile{};
    rf = 0.0f;
    rr = 0.0f;
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
    dat [1] = fgetlwl();
    dat [2] = fgetlwl();
    dat [3] = fgetbwl();
    dat [4] = tc + fgetdraft() / 2.f;
    dat [5] = tc - fgetdraft() / 2.f;
    dat [6] = fgetdisplacement();
    dat [7] = wetarea;
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
    dat [29] = 0;
    dat [30] = 0;
    resist (convertedspeed , dat , rf , rr , w , t0 , nr);
    if (fgetke() == 0)
        {
            if ((a_9 <= 0) || (a_8 <= 0) || (a_6 <= 0) || (a_7 <= 0) || (a_11 <= 0))
                return;
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
            c = (rf + rr) / (0.5f * waterdensity * square (convertedspeed) * wetarea);
            vs = vsr;
            dat [29] = c;
            dat [30] = vs;
            file_exportdata (dat , dan);
        }
    rf_keel = 0;
    cf_keel = 0;
    rn_keel = keel_chord + keel_area + rudder_chord + rudder_area + k_1 + k_2 + k_3 + k_4 + k_5 + k_6 + k_7;
    if (rn_keel > 0)
        {
            rn_rudder = convertedspeed * fgetlwl() / waterviscosity;
            cf_keel = keel_chord * 1.75f + keel_area * 1.4 + rudder_chord * 1.4 + rudder_area * 3.f + k_1 * 4 + k_2 * 3.5f + k_3 * 2.8 + k_4 * 2.f + k_5 * 2.7 + k_6 * 2 + k_7 * 5;
            rf_keel = 0.075 / square (logp (rn_rudder) - 2) * cf_keel / rn_keel * 0.5f * waterdensity * square (convertedspeed) * rn_keel;
        }
    rr = rr + rf_keel;
    if (fgetke() == 0)
        {
            fexecdirectory = solverDirectory;
            check();
            check();
            filetofind = findFile (QStringLiteral("TMPke.txt") , work);
            if (filetofind != QStringLiteral("TMPke.txt"))
                {
                    check();
                    warning(pascalFormat(tr(QStringLiteral("rslockefilemissing")), {QVariant(work)}));
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
                    if (i < 5)
                        goto label_newsearch1;
                    else
                        {
                            if (exists (QStringLiteral("TMPke.txt")))
                                check();
                            check();
                            warning(tr(QStringLiteral("rs_do_not_calculate_this_task__because_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_1__do_not_found_into__exec_or_was_damaged_file")) + QStringLiteral(" SeaMargn.EXE ") + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_2__input_datas_are_outside_valid_domain_or_catalog_is_not_valid_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_3__this_is_very_slow_computer__fcpu___800_mhz_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_4__cpu_is_loaded_more_80__another_processes_")));
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
            check();
            fsetke(ke_);
        }
    if ((jspeed == 1) && (iflag > 0))
        {
            dat [1] = fgetlwl();
            dat [2] = fgetbwl();
            dat [3] = 0;
            dat [4] = tc + fgetdraft() / 2.f;
            dat [5] = tc - fgetdraft() / 2.f;
            dat [6] = a_10;
            dat [7] = cp * cm;
            dat [8] = cm;
            dat [9] = cwp;
            dat [10] = fgetke() - 1;
            dat [11] = 0;
            dat [12] = 0.004;
            dat [13] = fgetkbulb();
            dat [14] = fgetzbulb();
            dat [15] = fgetba();
            dat [16] = 0;
            dat [17] = fgetnp() + 1;
            dat [18] = lcb;
            dat [19] = wetarea;
            dat [20] = 0;
            dat [21] = fgetdp();
            if ((fgetnp() > 2) || (fgetnp() < 0))
                fsetnp(1);
            if (fgetnp() == 0)
                {
                    dat [17] = 2;
                    dat [21] = 0;
                    dat [22] = 0;
                    dat [23] = 0;
                }
            if (fgetnp() == 1)
                {
                    dat [17] = 1;
                    dat [22] = ae0;
                    dat [23] = 0;
                }
            if (fgetnp() == 2)
                {
                    dat [17] = 3;
                    dat [22] = 0;
                    dat [23] = p_d0;
                }
            dat [24] = 1;
            dat [25] = convertedspeed / 0.514444;
            dat [26] = 1;
            dat [27] = waterdensity * 1000;
            dat [28] = waterviscosity;
            dat [29] = 0;
            dat [30] = 0;
            file_exportdata84 (dat , dan);
            fexecdirectory = solverDirectory;
            filetofind = findFile (QStringLiteral("IN.") , work);
            if (filetofind != QStringLiteral("IN."))
                {
                    check();
                    warning(tr(QStringLiteral("rs_have_not_input_file_for_calculation_is_here___")));
                    return;
                }
            execfullname = fexecdirectory + QStringLiteral("/") + QStringLiteral("PowerPrd.EXE");
            runSolver(execfullname, fcpowerprediction);
            filename = QStringLiteral("OUT.");
            i = 1;
            label_newsearch:;
            filetofind = findFile (QStringLiteral("IN.") , work);
            if (filetofind == QStringLiteral("IN."))
                {
                    check();
                    if (exists (QStringLiteral("OUT.")))
                        {
                            check();
                            check();
                            return;
                        }
                    i = i + 1;
                    if (i < 25)
                        goto label_newsearch;
                    else
                        {
                            if (exists (QStringLiteral("IN.")))
                                check();
                            check();
                            warning(tr(QStringLiteral("rs_do_not_calculate_this_task__because_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_1__do_not_found_into__exec_or_was_damaged_file")) + QStringLiteral(" PowerPrd.EXE ") + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_2__input_datas_are_outside_valid_domain_or_catalog_is_not_valid_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_3__this_is_very_slow_computer__fcpu___800_mhz_")) + QString(QChar(13)) + QString(QChar(10)) + QString(QChar(13)) + QString(QChar(10)) + tr(QStringLiteral("rs_4__cpu_is_loaded_more_80__another_processes_")));
                            return;
                        }
                }
        }
    check();
}

void Resistance_holtrop::resist(float vms, Span<float> dat, float &rf, float &rr, float &w, float &t0, float &nr) {
float t{};
float ta{};
float tf{};
float d{};
float l{};
float b{};
float v{};
float cb{};
float cp{};
float abt{};
float cbt{};
float ca{};
float ks{};
float appd{};
float sapp{};
float thb{};
float s{};
float cm{};
float cwp{};
float lr{};
float hb{};
float at{};
float lamb{};
float cstrn{};
float k1{};
float k2{};
float ke{};
float l_b{};
float b_t{};
float b_l{};
float l3v{};
float b_ta{};
float ta_d{};
float tf_l{};
float t_l{};
float t_b{};
float ks_{};
Array<float,1,4> m{};
Array<float,1,20> c{};
int i{};
float np{};
float ro{};
float nu{};
float lpp{};
float nh{};
float temp{};
float fn{};
float fnt{};
float fni{};
float cf{};
float cv{};
float cp1{};
float ra{};
float rb{};
float rtr{};
float pb{};
float re{};
float rw{};
float rwa{};
float rwb{};
    lpp = dat [0];
    l = dat [1];
    b = dat [2];
    tf = dat [3];
    ta = dat [4];
    t = (tf + ta) / 2.f;
    v = dat [5];
    s = dat [6];
    fsetlcb(dat [7]);
    cb = dat [8];
    cp = dat [9];
    cwp = dat [10];
    cm = dat [11];
    at = dat [12];
    abt = dat [13];
    hb = dat [14];
    d = dat [15];
    ro = dat [16];
    nu = dat [17] / 1000000;
    cstrn = dat [18];
    np = dat [19];
    ke = dat [20];
    ks = dat [21];
    cbt = abt / (b * t * cm);
    if (cbt <= 0)
        cbt = 0.001;
    if (d == 0)
        {
            d = 0.65 * ta;
            if (np == 1)
                d = 0.7 * ta;
        }
    l_b = l / b;
    b_l = b / l;
    b_ta = b / ta;
    ta_d = ta / d;
    tf_l = tf / l;
    t_l = t / l;
    t_b = t / b;
    abt = b * t * cm * cbt;
    if (s <= 0)
        s = l * (2 * t + b) * sqrtp (cm) * (0.453 + 0.442 * cb - 0.2862 * cm - 0.003467 * b / t + 0.3696 * cwp) + 2.38 * abt / cb;
    lr = l * (1 - cp + 0.06 * cp * fgetlcb() / (4 * cp - 1));
    c [17] = 6919.3 * power (cm , - 1.3346) * power ((v / l / l / l) , 2.00977) * power ((l_b - 2) , 1.40692);
    m [3] = - 7.2035 * power (b_l , 0.326869) * power (t_b , 0.605375);
    if ((ie_ == 0) || (iflag1 == 0))
        ie = 1.f + 89.f * expp (- power (l_b , 0.80856) * power ((1 - cwp) , 0.30484) * power ((1 - cp - 0.0225 * fgetlcb()) , 0.6367) * power ((lr / b) , 0.34574) * power ((100 * v / l / l / l) , 0.16302));
    else
        ie = ie_;
    temp = (b * t * (0.31 * sqrtp (abt) + tf - hb));
    if (temp == 0)
        temp = 0.001;
    c [3] = 0.56 * power (abt , 1.5f) / temp;
    c [2] = expp (- 1.89 * sqrtp (c [3]));
    c [5] = 1 - 0.8 * at / (b * t * cm);
    if (l_b > 12.f)
        lamb = 1.446 * cp - 0.36;
    else
        lamb = 1.446 * cp - 0.03 * l_b;
    c [7] = b_l;
    if (b_l < 0.11)
        c [7] = 0.229577 * power (b_l , 0.33333);
    if (b_l > 0.25f)
        c [7] = 0.5f - 0.0625f * l_b;
    c [1] = 2223105 * power (c [7] , 3.78613) * power (t_b , 1.07961) * power ((90 - ie) , (- 1.37565));
    l3v = l * l * l / v;
    lr = l * (1 - cp + 0.06 * cp * fgetlcb() / (4 * cp - 1));
    c [14] = 1.f + 0.011 * cstrn;
    k1 = 0.93 + 0.487118 * c [14] * power (b_l , 1.06806) * power (t_l , 0.46106) * power ((l / lr) , 0.121563) * power (l3v , 0.36486) * power ((1 - cp) , - 0.604247);
    c [15] = 0.f;
    if (l3v < 512)
        c [15] = - 1.69385;
    if ((l3v > 512) && (l3v < 1726.91))
        c [15] = - 1.69385 + (l / power (v , 0.3333) - 8) / 2.36;
    if (cp < 0.8)
        c [16] = 8.07981 * cp - 13.8673 * cp * cp + 6.984388 * cp * cp * cp;
    else
        c [16] = 1.73014 - 0.7067 * cp;
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
    c [20] = 1 + 0.0015 * cstrn;
    if (tf_l <= 0.04)
        c [4] = tf_l;
    else
        c [4] = 0.04;
    ca = 0.006 * power ((l + 100) , (- 0.16)) - 0.00205 + 0.003 * sqrtp (l / 7.5f) * square (square (cb)) * c [2] * (0.04 - c [4]);
    if (fgeta1() > 0)
        {
            ks_ = ks * ((((0.00010874793 * fgeta1() - 0.0019781) * fgeta1() + 0.0108798) * fgeta1() + 0.0078064) * fgeta1() + 1.0047939);
            if (ks_ >= 150.f)
                ca = ca + (0.105 * power (ks_ / 1000000.f , 0.333333) - 0.005579) / power (l , 0.33333);
        }
    else
        if (ks >= 150.f)
            ca = ca + (0.105 * power (ks / 1000000.f , 0.333333) - 0.005579) / power (l , 0.33333);
    m [1] = 0.0140407 * l / t - 1.75254 * power (v , 0.33333) / l - 4.79323 * b_l - c [16];
    fn = vms / sqrtp (9.81 * l);
    m [4] = c [15] * 0.4 * expp ((- 0.034) * power (fn , - 3.29));
    re = vms * l / nu;
    cf = 0.075 / square (logp (re) - 2);
    rf = cf * s * k1 * ro * vms * vms / 2;
    rwa = 0;
    rwb = 0;
    if (m [1] < 0)
        {
            rwa = c [1] * c [2] * c [5] * 10.05525 * v * expp (m [1] * power (fn , - 0.9) + m [4] * cosp (lamb / fn / fn));
            rwb = c [17] * c [2] * c [5] * 10.05525 * v * expp (m [3] * power (fn , - 0.9) + m [4] * cosp (lamb / fn / fn));
        }
    if (fn <= 0.4)
        rw = rwa;
    if (fn >= 0.55)
        rw = rwb;
    if ((fn > 0.4) && (fn < 0.55))
        {
            rwa = c [1] * c [2] * c [5] * 10.05525 * v * expp (m [1] * power (0.4 , - 0.9) + m [4] * cosp (lamb / 0.4 / 0.4));
            rwb = c [17] * c [2] * c [5] * 10.05525 * v * expp (m [3] * power (0.55 , - 0.9) + m [4] * cosp (lamb / 0.55 / 0.55));
            rw = rwa + (10.0f * fn - 4.0f) * (rwb - rwa) / 1.5f;
        }
    thb = tf - 1.5f * hb;
    if (thb == 0)
        pb = 1000;
    else
        pb = 0.56 * sqrtp (abt) / (tf - 1.5f * hb);
    temp = sqrtp (9.81 * (tf - hb - 0.25f * sqrtp (abt)) + 0.15 * vms * vms);
    if (temp == 0)
        temp = 0.001;
    fni = vms / temp;
    rb = 0.00011 * expp (- 3 / pb / pb) * fni * fni * fni * power (abt , 1.5f) * 10055.25f / (1 + fni * fni);
    rtr = 0.0f;
    if (at > 0.0f)
        fnt = vms / sqrtp (19.62 * at / (b + b * cwp));
    else
        fnt = 100;
    if (fnt < 5)
        c [6] = 0.2 * (1 - 0.2 * fnt);
    else
        c [6] = 0.f;
    rtr = c [6] * at * ro * vms * vms / 2;
    ra = ca * s * ro * vms * vms / 2;
    cp1 = 1.45 * cp - 0.315 - 0.0225 * fgetlcb();
    cv = k1 * cf + ca;
    if (cp1 < 1)
        {
            nver = 1988;
            if (np == 0)
                {
                    w = 0.3 * cb + 10 * cv * cb - 0.1;
                    t0 = 0.1;
                    nr = 0.98;
                }
            if (np == 1)
                {
                    w = c [20] * (c [9] * cv * l / ta * (0.050776 + 0.93405 * c [11] * cv / (1 - cp1)) + 0.27915 * sqrtp (b / (l * (1 - cp1))) + c [19]);
                    t0 = 0.25014 * power (b_l , 0.28956) * power ((sqrtp (b * t) / d) , 0.2624) / power ((1 - cp + 0.0225 * fgetlcb()) , 0.01762) + 0.0015 * cstrn;
                    nr = 0.9922 - 0.05908 * ae0 + 0.07424 * (cp - 0.0225 * fgetlcb());
                    nh = (1 - t0) / (1 - w) * nr;
                    if (nh > 1.4)
                        {
                            w = c [9] * cv * l / ta * (0.0661875 + 1.21756 * c [11] * cv / (1 - cp1)) + 0.24558 * sqrtp (b / (l * (1 - cp1))) - 0.09726 / (0.95 - cp) + 0.11434 / (0.95 - cb) + cstrn * (0.75f * cv + 0.002);
                            if (l_b >= 5.2)
                                c [10] = b_l;
                            else
                                c [10] = 0.25f - 0.003328402 / (b_l - 0.134615385);
                            t0 = 0.001979 * l / b / (1 - cp1) + 1.0585 * c [10] - 0.00524 - 0.1418 * d * d / b / t + 0.0015 * cstrn;
                            nr = 0.9922 - 0.05908 * ae0 + 0.07424 * (cp - 0.0225 * fgetlcb());
                            nver = 1984;
                        }
                }
            if (np == 2)
                {
                    w = 0.3095 * cb + 10 * cv * cb - 0.23 * d / sqrtp (b * t);
                    t0 = 0.325 * cb - 0.1885 * d / sqrtp (b * t);
                    nr = 0.9737 + 0.111 * (cp - 0.0225 * fgetlcb()) - 0.06325 * p_d0;
                }
        }
    rr = rb + rtr + ra + rw;
}

void Resistance_holtrop::file_exportdata(Span<float> dat, Span<float> dan) {
int i{};
TextFile ffile{};
    ftmpfilename = QStringLiteral("TMPke.txt");
    ffile.name = safePath(ftmpfilename);
    newFile(ffile);
    for (i=0; i<=29; ++i)
    {
        check();
        {
            writeLine(ffile, dat [i]);
        }
    }
    for (i=0; i<=14; ++i)
    {
        check();
        {
            writeLine(ffile, dan [i]);
        }
    }
    closeFile(ffile);
}

void Resistance_holtrop::file_exportdata84(Span<float> dat, Span<float> dan) {
int i{};
TextFile ffile{};
    ftmpfilename = QStringLiteral("IN.");
    ffile.name = safePath(QStringLiteral("IN."));
    newFile(ffile);
    for (i=0; i<=4; ++i)
    {
        check();
        writeText(ffile, formatValue(dat [i], 9, 4));
    }
    writeText(ffile, formatValue(dat [i], 9, 3));
    for (i=6; i<=8; ++i)
    {
        check();
        writeText(ffile, formatValue(dat [i], 10, 6));
    }
    writeLine(ffile, formatValue(dat [9], 10, 6));
    for (i=10; i<=15; ++i)
    {
        check();
        writeText(ffile, formatValue(dat [i], 9, 4));
    }
    writeLine(ffile, formatValue(dat [16], 2, 0));
    for (i=17; i<=22; ++i)
    {
        check();
        writeText(ffile, formatValue(dat [i], 11, 4));
    }
    writeLine(ffile, formatValue(dat [23], 2, 0));
    for (i=24; i<=26; ++i)
    {
        check();
        writeText(ffile, formatValue(dat [i], 10, 3));
    }
    writeLine(ffile, formatValue(dat [27], 14, 11));
    closeFile(ffile);
}

void Resistance_holtrop::calculateinworkspace() {
float convertedspeed{};
float froudenumber{};
float rf{};
float rr{};
float rt{};
float dvs{};
float lr{};
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
float flag{};
int index{};
std::vector<float> cpopt{};
std::vector<float> lcbopt{};
float speed{};
float stepspeed1{};
QString line{};
QString tmp{};
QString pathfileold{};
int units{};
bool stop{};
int ispeed{};
int iii{};
int i{};
int ii{};
int j{};
int jj{};
int j3{};
TextFile ffile{};
Array<float,1,10> vs{};
Array<QString,1,125> str_{};
Array<QString,1,40> str0{};
    if (field(QStringLiteral("estimate3box")) . checked)
        iflag = 1;
    else
        iflag = 0;
    if (field(QStringLiteral("estimate4box")) . checked)
        iflag1 = 1;
    else
        iflag1 = 0;
    vs [1] = fgetstartspeed();
    vs [8] = fgetendspeed();
    vs [10] = fgetstepspeed();
    dvs = (vs [8] - vs [1]) / 7;
    vs [2] = vs [1] + dvs;
    vs [3] = vs [2] + dvs;
    vs [4] = vs [3] + dvs;
    vs [5] = vs [4] + dvs;
    vs [6] = vs [5] + dvs;
    vs [7] = vs [6] + dvs;
    vs [9] = vs [8] + (vs [10] - vs [8]) / 2;
    vsr = fgetendspeed();
    clearSeries(QStringLiteral("series1"));
    clearSeries(QStringLiteral("series2"));
    clearSeries(QStringLiteral("series3"));
    clearSeries(QStringLiteral("series4"));
    ; // presentation only
    ; // presentation only
    ; // presentation only
    field(QStringLiteral("tbprint")) . enabled = false;
    resultsmemo2.clear();
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p1")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p2")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p3")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p4")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p5")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p6")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p7")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p8")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_holtrop_note_p9")));
    resultsmemo2.append(QStringLiteral(" "));
    resultsmemo2.append(QStringLiteral("Copyright (c) 2007, Timoshenko V.F."));
    ; // presentation only
    if (correctinputdata())
        {
            units = settings. projectunits;
            resultsmemo.clear();
            for (i=1; i<=4; ++i)
            {
                check();
                resultsmemo.append(QStringLiteral(""));
            }
            header (space (10) + tr(QStringLiteral("rs_resistance_prediction_for_sea_vessels__according_to_associate_prof__timoshenko")) + QStringLiteral(".") , resultsmemo);
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral("          ----------------------------------------------------------------------------------"));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_input_variables")));
            resultsmemo.append(QStringLiteral("          ----------------------------------------------------------------------------------"));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_general")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_start_speed")) , 40) + QStringLiteral(" : ") + number (vs [1] , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_kn")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_end_speed")) , 40) + QStringLiteral(" : ") + number (vs [10] , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_kn")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_water_density")) , 40) + QStringLiteral(" : ") + number (fgetdensity() , 0 , 8 , 3) + QString(QChar(32)) + densitystr (units));
            if (units == 1)
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_water_viscosity")) , 40) + QStringLiteral(" : ") + number (fgetviscosity() , 0 , 8 , 4) + QStringLiteral("*10^(-6) ") + tr(QStringLiteral("rs_ft_2_s")));
            else
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_water_viscosity")) , 40) + QStringLiteral(" : ") + number (fgetviscosity() , 0 , 8 , 4) + QStringLiteral("*10^(-6) ") + tr(QStringLiteral("rs_m_2_s")));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_hull")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_length_on_waterline")) , 40) + QStringLiteral(" : ") + number (fgetlwl() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_beam_on_waterline")) , 40) + QStringLiteral(" : ") + number (fgetbwl() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_draft_on_midship")) , 40) + QStringLiteral(" : ") + number (tc , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_draft_on_f_p_")) , 40) + QStringLiteral(" : ") + number (tc + fgetdraft() / 2.f , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_draft_on_f_a_")) , 40) + QStringLiteral(" : ") + number (tc - fgetdraft() / 2.f , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            stop = false;
            if ((absp (fgetdraft()) / fgetlwl() > 0.05) || (absp (fgetdraft()) > fgetdrafttotal() / 2))
                {
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_warning____very_big_trim__tf_ta_")));
                    stop = true;
                }
            if (field(QStringLiteral("estimatebox")) . checked)
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_wetted_surface_area")) , 40) + QStringLiteral(" : ") + number (fgetwettedsurface() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(")"));
            else
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_wetted_surface_area")) , 40) + QStringLiteral(" : ") + number (fgetwettedsurface() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_waterplane_area")) , 40) + QStringLiteral(" : ") + number (fgetwlarea() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_displacement")) , 40) + QStringLiteral(" : ") + number (fgetdisplacement() , 0 , 6 , 3) + QString(QChar(32)) + volstr (units));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_longitudinal_center_of_buoyancy")) , 40) + QStringLiteral(" : ") + number (fgetlcb() , 0 , 6 , 3) + QStringLiteral(" %"));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_prismatic_coefficient")) , 40) + QStringLiteral(" : ") + number (fgetcp() , 0 , 6 , 4));
            resultsmemo.append(QStringLiteral(""));
            iii = 5;
            if ((fgetk1() + fgetk2() + fgetk3() + fgetk4() + fgetk5() + fgetk6() + fgetk7() + fgetkeelchordlength() + fgetkeelarea() + fgetrudderchordlength() + fgetrudderarea() > 0))
                {
                    for (i=1; i<=3; ++i)
                    {
                        check();
                        resultsmemo.append(QStringLiteral(""));
                    }
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_appendages_areas_")));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_shaft_brackets_area")) , 40) + QStringLiteral(" : ") + number (fgetk1() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_strut_bossing_area")) , 40) + QStringLiteral(" : ") + number (fgetk2() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_hull_bossing_area")) , 40) + QStringLiteral(" : ") + number (fgetk4() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_stabiliser_fins_area")) , 40) + QStringLiteral(" : ") + number (fgetk3() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_dome_area")) , 40) + QStringLiteral(" : ") + number (fgetk5() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_exposed_shafts_area_")) , 40));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_angle_vs_buttoks_about_10__")) , 40) + QStringLiteral(" : ") + number (fgetk6() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_angle_vs_buttoks_about_20__")) , 40) + QStringLiteral(" : ") + number (fgetk7() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(QStringLiteral(""));
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_areas_of_keels")));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_skeg")) , 40) + QStringLiteral(" : ") + number (fgetkeelchordlength() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_bilge_keels")) , 40) + QStringLiteral(" : ") + number (fgetkeelarea() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(QStringLiteral(""));
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_areas_of_rudders")));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_rudder_behind_skeg_stern")) , 40) + QStringLiteral(" : ") + number (fgetrudderchordlength() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_2_screw_rudder_slender_thick")) , 40) + QStringLiteral(" : ") + number (fgetrudderarea() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    for (i=1; i<=4; ++i)
                    {
                        check();
                        resultsmemo.append(QStringLiteral(""));
                    }
                    iii = 3;
                }
            if ((fgeta1() + fgeta2() + fgeta3() + fgeta4() + fgeta5() + fgeta6() + fgeta8() + fgeta9() + fgeta11() + fgetks() > 0) && (fgeta7() > 1) && (fgeta10() > 0))
                {
                    for (i=1; i<=4; ++i)
                    {
                        check();
                        resultsmemo.append(QStringLiteral(""));
                    }
                    resultsmemo.append(space (10) + pad (field(QStringLiteral("groupbox3")) . caption , 30));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label6_1")) . caption , 45) + QStringLiteral(" : ") + number (fgeta1() , 0 , 6 , 1));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label6_2")) . caption , 45) + QStringLiteral(" : ") + number (fgeta2() , 0 , 6 , 3));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label6_3")) . caption , 45) + QStringLiteral(" : ") + number (fgeta3() , 0 , 6 , 2));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label6_4")) . caption , 45) + QStringLiteral(" : ") + number (fgeta4() , 0 , 6 , 2));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label6_5")) . caption , 45) + QStringLiteral(" : ") + number (fgeta5() , 0 , 6 , 2));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label6_6")) . caption , 45) + QStringLiteral(" : ") + number (fgeta6() , 0 , 6 , 3));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label6_7")) . caption , 45) + QStringLiteral(" : ") + number (fgeta7() , 0 , 6 , 3));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label6_8")) . caption , 45) + QStringLiteral(" : ") + number (fgeta8() , 0 , 6 , 3));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label6_9")) . caption , 45) + QStringLiteral(" : ") + number (fgeta9() , 0 , 6 , 2));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label6_10")) . caption , 45) + QStringLiteral(" : ") + number (fgeta10() , 0 , 6 , 2));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label6_11")) . caption , 45) + QStringLiteral(" : ") + number (fgeta11() , 0 , 6 , 0));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label17")) . caption , 45) + QStringLiteral(" : ") + number (fgetks() , 0 , 6 , 0));
                    for (i=1; i<=iii; ++i)
                    {
                        check();
                        resultsmemo.append(QStringLiteral(""));
                    }
                    iii = 6;
                }
            for (i=1; i<=iii; ++i)
            {
                check();
                resultsmemo.append(QStringLiteral(""));
            }
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_calculated_variables")));
            resultsmemo.append(space (10) + QStringLiteral("----------------------------------------------------------------------------------"));
            resultsmemo.append(QStringLiteral(""));
            if (fgetwettedsurface() <= 0)
                {
                    s = fgetlwl() * (2 * tc + fgetbwl()) * sqrtp (cm) * (0.453 + 0.442 * fgetcp() * cm - 0.2862 * cm - 0.003467 * fgetbwl() / tc + 0.3696 * cwp) + 2.38 * fgetkbulb() / fgetcp() * cm;
                    resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_method_holtrop_1988")) + QStringLiteral(")"));
                }
            resultsmemo.append(space (10) + QStringLiteral("Cp            = ") + number (fgetcp() , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("Cb            = ") + number (fgetcp() * cm , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("Cwp           = ") + number (cwp , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("Cm            = ") + number (cm , 0 , 6 , 4));
            if (am > 0.0f)
                resultsmemo.append(space (10) + QStringLiteral("Cbt           = ") + number (fgetkbulb() / am , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("Am            = ") + number (am , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(space (10) + QStringLiteral("Lwl/Bwl       = ") + number (fgetlwl() / fgetbwl() , 0 , 6 , 3));
            if (z0 != 0.0f)
                resultsmemo.append(space (10) + QStringLiteral("Tc            = ") + number (tc , 0 , 6 , 3) + QStringLiteral(" m"));
            resultsmemo.append(space (10) + QStringLiteral("Bwl/T         = ") + number (fgetbwl() / tc , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Lwl/T         = ") + number (fgetlwl() / tc , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Cstrn         = ") + number (fgetcstrn() , 0 , 6 , 0));
            resultsmemo.append(space (10) + QStringLiteral("Np            = ") + number (fgetnp() , 0 , 6 , 0));
            if (iflag1 == 1)
                resultsmemo.append(space (10) + QStringLiteral("ie            = ") + number (ie_ , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_degr")));
            resultsmemo.append(QStringLiteral(""));
            if ((fgetcp() > 0.85) || (fgetcp() < 0.55))
                resultsmemo.append(space (10) + QStringLiteral("Cp      ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,55 ... 0,85"));
            if ((fgetlwl() / fgetbwl() > 14.9) || (fgetlwl() / fgetbwl() < 3.9))
                resultsmemo.append(space (10) + QStringLiteral("Lwl/Bwl ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 3,90 ... 14,9"));
            if ((fgetbwl() / tc > 4) || (fgetbwl() / tc < 2.1))
                resultsmemo.append(space (10) + QStringLiteral("Bwl/T   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 2,10 ... 4,0"));
            if ((fgetlcb() > 6) || (fgetlcb() < - 6))
                resultsmemo.append(space (10) + QStringLiteral("LCB     ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" -6 % ... 6 %"));
            if ((fgetlcb() > 6.1) || (fgetcp() > 0.9))
                return;
            lr = fgetlwl() * (1 - fgetcp() + 0.06 * fgetcp() * fgetlcb() / (4 * fgetcp() - 1));
            if (lr < 0.2)
                {
                    resultsmemo.append(space (10) + QStringLiteral("Lr = ") + number (lr , 0 , 6 , 4) + QStringLiteral(" < 0,2"));
                    return;
                }
            if (fgetlwl() / fgetbwl() <= 2)
                {
                    warning(tr(QStringLiteral("rs_calculation_is_not_possible__because_l_b_2____")));
                    return;
                }
            for (i=1; i<=9; ++i)
            {
                check();
                resultsmemo.append(QStringLiteral(""));
            }
            speed = vs [10];
            convertedspeed = speed * 1852 / 3600;
            flag = 0.0f;
            calculateresistanceholtr (convertedspeed , fgetlcb() , fgetcp() , rf , rr , w , t0 , nr);
            flag = 0.001;
            if ((rf + rr) > 10)
                flag = 1.f;
            for (i=1; i<=10; ++i)
            {
                check();
                resultsmemo.append(QStringLiteral(""));
            }
            resultsmemo.append(space (8) + tr(QStringLiteral("rs_final_calculations_of_resistance_by_a_method_holtrop_1988")));
            resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+--------+--------+--------+---------+--------+---------+"));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_l_speed_l_speed_l__fr___l___r_f__l___r_r__l___r_t__l__power__l__r_t_e_l_power_e_l")));
            if (flag == 1)
                {
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn__l__m_s__l_______l___kn___l___kn___l____kn__l____kw___l____kn__l____kw___l")));
                    yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kn"));
                    rightTitle = tr(QStringLiteral("rs_power")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kw"));
                }
            else
                {
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn__l__m_s__l_______l____n___l____n___l____n___l____w____l____n___l____w____l")));
                    yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral(", ") + tr(QStringLiteral("rs_n"));
                    rightTitle = tr(QStringLiteral("rs_power")) + QStringLiteral(", ") + tr(QStringLiteral("rs_w"));
                }
            resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+--------+--------+--------+---------+--------+---------+"));
            index = 0;
            cpopt.resize(truncp ((fgetendspeed() - fgetstartspeed()) / fgetstepspeed()) + 10);
            lcbopt.resize(truncp ((fgetendspeed() - fgetstartspeed()) / fgetstepspeed()) + 10);
            stepspeed1 = (vs [10] - vs [1]) / 20;
            speed = vs [1];
            check();
            check();
            ftmpfilename = QStringLiteral("RESISTp.dat");
            ffile.name = safePath(trailing (work) + QStringLiteral("RESISTp.dat"));
            newFile(ffile);
            writeLine(ffile, QStringLiteral("#     Nser      Np       Wt        t       Eta_R       Dp"));
            writeText(ffile, QStringLiteral("       51 "));
            writeText(ffile, formatValue(fgetnp(), 10, 0));
            writeText(ffile, formatValue(w, 10, 4));
            writeText(ffile, formatValue(t0, 10, 4));
            writeText(ffile, formatValue(nr, 10, 4));
            writeLine(ffile, formatValue(fgetdp(), 10, 4));
            writeLine(ffile, QStringLiteral("#      Vs       Rt        Rte       Pe       Pee"));
            for (ispeed=1; ispeed<=20; ++ispeed)
            {
                check();
                {
                    speed = speed + stepspeed1;
                    convertedspeed = speed * 1852 / 3600;
                    if (settings. projectunits == 1)
                        froudenumber = convertedspeed / sqrtp (9.81 * foot * fgetlwl());
                    else
                        froudenumber = convertedspeed / sqrtp (9.81 * fgetlwl());
                    if ((froudenumber > 1.0f) || (froudenumber < 0.05))
                        resultsmemo.append(space (10) + QStringLiteral("Fr ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,05 ... 1,0"));
                    calculateresistanceholtr (convertedspeed , fgetlcb() , fgetcp() , rf , rr , w , t0 , nr);
                    point(QStringLiteral("series1"), speed ,  (rf + rr) / flag );
                    point(QStringLiteral("series2"), speed ,  (rf + rr) / flag * fgetke() );
                    point(QStringLiteral("series3"), speed ,  (rf + rr) / flag * convertedspeed );
                    point(QStringLiteral("series4"), speed ,  (rf + rr) / flag * convertedspeed * fgetke() );
                    writeText(ffile, formatValue(speed, 10, 2));
                    writeText(ffile, formatValue(rf + rr, 10, 2));
                    writeText(ffile, formatValue((rf + rr) * fgetke(), 10, 2));
                    writeText(ffile, formatValue((rf + rr) * convertedspeed, 10, 2));
                    writeLine(ffile, formatValue((rf + rr) * fgetke() * convertedspeed, 10, 2));
                }
            }
            closeFile(ffile);
            ftmpfilename = QStringLiteral("RESIST.dat");
            ffile.name = safePath(trailing (work) + QStringLiteral("RESIST.dat"));
            newFile(ffile);
            for (ispeed=1; ispeed<=10; ++ispeed)
            {
                check();
                {
                    jspeed = ispeed;
                    speed = vs [ispeed];
                    convertedspeed = speed * 1852 / 3600;
                    if (settings. projectunits == 1)
                        froudenumber = convertedspeed / sqrtp (9.81 * foot * fgetlwl());
                    else
                        froudenumber = convertedspeed / sqrtp (9.81 * fgetlwl());
                    calculateresistanceholtr (convertedspeed , fgetlcb() , fgetcp() , rf , rr , w , t0 , nr);
                    line = formatValue(speed, 6, 2);
                    line = QStringLiteral("|") + line + QStringLiteral(" |");
                    tmp = formatValue(convertedspeed, 6, 2);
                    line = line + tmp + QStringLiteral(" |");
                    tmp = formatValue(froudenumber, 6, 3);
                    line = line + tmp + QStringLiteral(" |");
                    tmp = formatValue(rf / flag, 7, 1);
                    line = line + tmp + QStringLiteral(" |");
                    if (((froudenumber >= 0.05) && (froudenumber <= 1)))
                        {
                            ; // presentation only
                            tmp = formatValue(rr / flag, 7, 1);
                            line = line + tmp + QStringLiteral(" |");
                            tmp = formatValue((rf + rr) / flag, 7, 1);
                            line = line + tmp + QStringLiteral(" |");
                            tmp = formatValue((rf + rr) / flag * convertedspeed, 8, 1);
                            line = line + tmp + QStringLiteral(" |");
                            tmp = formatValue((rf + rr) / flag * fgetke(), 7, 1);
                            line = line + tmp + QStringLiteral(" |");
                            tmp = formatValue((rf + rr) / flag * convertedspeed * fgetke(), 8, 1);
                            line = line + tmp + QStringLiteral(" |");
                            if ((ispeed == 1) || (ispeed == 4) || (ispeed == 6) || (ispeed == 8) || (ispeed == 10))
                                {
                                    writeText(ffile, formatValue(speed, 10, 2));
                                    writeText(ffile, formatValue(rf + rr, 10, 2));
                                    writeText(ffile, formatValue((rf + rr) * fgetke(), 10, 2));
                                    writeText(ffile, formatValue((rf + rr) * convertedspeed, 10, 2));
                                    writeLine(ffile, formatValue((rf + rr) * fgetke() * convertedspeed, 10, 2));
                                }
                        }
                    resultsmemo.append(space (10) + line);
                    if (ispeed < 8)
                        tb = 0.01;
                    if (ispeed == 8)
                        {
                            tb = (rf + rr) * fgetke() / (1 - t0);
                            vms = convertedspeed;
                        }
                }
            }
            closeFile(ffile);
            resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+--------+--------+--------+---------+--------+---------+"));
            resultsmemo.append(QStringLiteral(""));
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
                        resultsmemo.append(QStringLiteral("Ae/Ao   =  ") + number (ae0 , 0 , 6 , 4));
                    if ((ae0 > 1.1) && (fgetnp() <= 2))
                        {
                            resultsmemo.append(tr(QStringLiteral("rs_attention____increase_number_of_propellers_or_decrease_estimate_speed_")));
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
                        resultsmemo.append(space (10) + QStringLiteral("Tb       =  ") + number (tb , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_kn")));
                    else
                        resultsmemo.append(space (10) + QStringLiteral("Tb       =  ") + number (tb / flag , 0 , 6 , 0) + QStringLiteral(" ") + tr(QStringLiteral("rs_n")));
                    resultsmemo.append(space (10) + QStringLiteral("Kdt      =  ") + number (kdt , 0 , 6 , 3));
                    resultsmemo.append(space (10) + QStringLiteral("Dp       =  ") + number (dp_ , 0 , 6 , 3) + QStringLiteral(" ") + lengthstr (units));
                    resultsmemo.append(space (10) + QStringLiteral("Z        =  ") + number (z , 0 , 6 , 0));
                    resultsmemo.append(space (10) + QStringLiteral("Ae/Ao    =  ") + number (ae0 , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs___calculated")));
                    p_d0 = p_d;
                    ae0 = ae_ao;
                    resultsmemo.append(space (10) + QStringLiteral("Ae/Ao    =  ") + number (ae0 , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs___for_selection_of_propeller_diagram")));
                    resultsmemo.append(space (10) + QStringLiteral("P/Dp     =  ") + number (p_d0 , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs___by_curve_of_propeller_optimal_frequency")));
                }
            if (fgetnp() == 1)
                nr = 0.9922 - 0.05908 * ae0 + 0.07424 * (fgetcp() - 0.0225 * fgetlcb());
            else
                nr = 0.9737 + 0.111 * (fgetcp() - 0.0225 * fgetlcb()) - 0.06325 * p_d0;
            resultsmemo.append(space (10) + QStringLiteral("Ke       =  ") + number (fgetke() , 0 , 6 , 3));
            if (iflag1 == 0)
                resultsmemo.append(space (10) + QStringLiteral("ie       =  ") + number (ie , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_degr")));
            resultsmemo.append(space (10) + QStringLiteral("Wt       =  ") + number (w , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("t        =  ") + number (t0 , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("EtaR     =  ") + number (nr , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("EtaH     =  ") + number ((1 - t0) / (1 - w) , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("EtaH*EtaR=  ") + number ((1 - t0) / (1 - w) * nr , 0 , 6 , 4));
            cpopt [1] = optimumcph (convertedspeed);
            lcbopt [1] = optimumlcbh (convertedspeed);
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_note__coefficients_wt__t_and_etar_were_calculated_by_formulaes_of_method_holtrop_")) + number (nver , 0 , 6 , 0));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + QStringLiteral("Copyright (c) 2007-2010, Timoshenko V.F."));
            if (iflag == 1)
                {
                    resultsmemo.append(QStringLiteral(""));
                    resultsmemo.append(QStringLiteral("----------------------------------------------------------------------------------"));
                    if (exists (QStringLiteral("OUT.")))
                        {
                            ftmpfilename = QStringLiteral("OUT.");
                            ffile.name = safePath(QStringLiteral("OUT."));
                            readFile(ffile);
                            ii = 0;
                            while (! atEnd (ffile)) {
                                check();
                                {
                                    ii = ii + 1;
                                    readLine(ffile, str_ [ii]);
                                }
                            }
                            closeFile(ffile);
                            str0 [1] = QStringLiteral(" Input Verification:");
                            str0 [2] = QStringLiteral("      Length of Waterline LWL (m)            =");
                            str0 [3] = QStringLiteral("      Maximum Beam on LWL (m)                =");
                            str0 [4] = QStringLiteral("      Depth at the Bow (m)                   =");
                            str0 [5] = QStringLiteral("      Mean Draft (m)                         =");
                            str0 [6] = QStringLiteral("      Draft Forward (m)                      =");
                            str0 [7] = QStringLiteral("      Draft Aft (m)                          =");
                            str0 [8] = QStringLiteral("      Block Coefficient on LWL CB            =");
                            str0 [9] = QStringLiteral("      Prismatic Coefficient on LWL CP        =");
                            str0 [10] = QStringLiteral("      Midship Coefficient to LWL CM=CX       =");
                            str0 [11] = QStringLiteral("      Waterplane Coefficient on LWL CWP      =");
                            str0 [12] = QStringLiteral("      Center of Buoyancy LCB (% LWL; + Fwd)  =");
                            str0 [13] = QStringLiteral("      Center of Buoyancy LCB (m from FP)     =");
                            str0 [14] = QStringLiteral("      Molded Volume (m^3)                    =");
                            str0 [15] = QStringLiteral("      Deck House/Cargo Frontal Area (m^2)    =");
                            str0 [16] = QStringLiteral("      Water Type                             =");
                            str0 [17] = QStringLiteral("      Water Density (kg/m^3)                 =");
                            str0 [18] = QStringLiteral("      Kinematic Viscosity (m^2/s)            =");
                            str0 [19] = QStringLiteral("      Appen. Drag (% Bare Hull Resistance)   =");
                            str0 [20] = QStringLiteral("      Bulb Section Area at Station 0 (m^2)   =");
                            str0 [21] = QStringLiteral("      Vertical Center of Bulb Area (m)       =");
                            str0 [22] = QStringLiteral("      Transom Immersed Area (m^2)            =");
                            str0 [23] = QStringLiteral("      Stern Type                             =");
                            str0 [24] = QStringLiteral("      Design Margin on RT,PE,REQ.THR (%)     =");
                            str0 [25] = QStringLiteral("      Propulsion Type                        =");
                            str0 [26] = QStringLiteral("      Propeller Diameter (m)                 =");
                            str0 [27] = QStringLiteral("      Propeller Expanded Area Ratio Ae/Ao    =");
                            str0 [28] = QStringLiteral("      Wetted Surface (m^2)                   =");
                            str0 [29] = QStringLiteral("      Half Angle of Entrance (deg)           =");
                            str0 [30] = QStringLiteral("  Speed, Resistance Coefficients and Frictional Resistance RF(N):");
                            str0 [31] = QStringLiteral(" V(kts)    V(m/s)      FN     SLRATIO     CF        CR        CA       RF");
                            str0 [32] = QStringLiteral("  Remaining Resistance Components (N):");
                            str0 [33] = QStringLiteral("          Form    Appendage   Wave       Bulb    Transom Correlation Air Drag");
                            str0 [34] = QStringLiteral(" V(kts)   RF*K1      RAPP      RW         RB       RTR       RA       RAIR");
                            str0 [35] = QStringLiteral("  Resistance, Effective Power, Propulsion Factors and Required Thrust");
                            str0 [36] = QStringLiteral(" V(kts)   RT(N)     PE(kW)      w        t     REQ.THR(N)    etaH      etaRR");
                            str0 [37] = QStringLiteral(" Design Margin Has Been Included in RT, PE, and REQ.THR = RT/(1-t).");
                            str0 [38] = QStringLiteral("      Propeller Pitch Diameter Ratio P/Dp    =");
                            for (ii=1; ii<=5; ++ii)
                            {
                                check();
                                {
                                    resultsmemo.append(space (10) + str_ [ii]);
                                }
                            }
                            jj = 13;
                            j3 = 0;
                            if (fgetnp() == 0)
                                j3 = 4;
                            for (ii=14; ii<=57 - j3; ++ii)
                            {
                                check();
                                {
                                    if (str_ [ii] == QStringLiteral(" "))
                                        {
                                            resultsmemo.append(space (10) + QStringLiteral(" "));
                                            jj = jj + 1;
                                        }
                                    else
                                        {
                                            if ((fgetnp() == 0) && (ii == 49))
                                                jj = jj - 2;
                                            tmp = str0 [ii - jj];
                                            if ((fgetnp() == 2) && (ii == 50))
                                                tmp = str0 [38];
                                            do {
                                                check();
                                                j = position (tmp , str_ [ii]);
                                                if (j != 0)
                                                    str_ [ii].remove((j)-1, 46);
                                            } while (!(j == 0));
                                            if (fgetnp() == 0)
                                                resultsmemo.append(space (10) + userstring (1499 + ii - jj) + str_ [ii]);
                                            if (fgetnp() == 1)
                                                resultsmemo.append(space (10) + userstring (1499 + ii - jj) + str_ [ii]);
                                            if ((fgetnp() == 2) && (ii == 50))
                                                resultsmemo.append(space (10) + tr(QStringLiteral("rs_propeller_pitch_diameter_ratio_p_dp_____")) + str_ [ii]);
                                            else
                                                if (fgetnp() == 2)
                                                    resultsmemo.append(space (10) + userstring (1499 + ii - jj) + str_ [ii]);
                                        }
                                }
                            }
                            jj = 52;
                            for (ii=82; ii<=85; ++ii)
                            {
                                check();
                                {
                                    if (str_ [ii] == QStringLiteral(" "))
                                        {
                                            resultsmemo.append(space (10) + QStringLiteral(" "));
                                            jj = jj + 1;
                                        }
                                    else
                                        {
                                            tmp = str0 [ii - jj];
                                            do {
                                                check();
                                                j = position (tmp , str_ [ii]);
                                                if (j != 0)
                                                    str_ [ii].remove((j)-1, 77);
                                            } while (!(j == 0));
                                            resultsmemo.append(space (10) + userstring (1499 + ii - jj) + str_ [ii]);
                                        }
                                }
                            }
                            j3 = 0;
                            if (fgetnp() == 0)
                                j3 = 2;
                            for (ii=86; ii<=93 + j3; ++ii)
                            {
                                check();
                                resultsmemo.append(space (10) + str_ [ii]);
                            }
                            jj = 62 + j3;
                            for (ii=94 + j3; ii<=98 + j3; ++ii)
                            {
                                check();
                                {
                                    if (str_ [ii] == QStringLiteral(" "))
                                        {
                                            resultsmemo.append(space (10) + QStringLiteral(" "));
                                            jj = jj + 1;
                                        }
                                    else
                                        {
                                            tmp = str0 [ii - jj];
                                            do {
                                                check();
                                                j = position (tmp , str_ [ii]);
                                                if (j != 0)
                                                    str_ [ii].remove((j)-1, 77);
                                            } while (!(j == 0));
                                            resultsmemo.append(space (10) + userstring (1499 + ii - jj) + str_ [ii]);
                                        }
                                }
                            }
                            for (ii=99 + j3; ii<=107 + j3; ++ii)
                            {
                                check();
                                resultsmemo.append(space (10) + str_ [ii]);
                            }
                            jj = 73 + j3;
                            for (ii=108 + j3; ii<=111 + j3; ++ii)
                            {
                                check();
                                {
                                    if (str_ [ii] == QStringLiteral(" "))
                                        {
                                            resultsmemo.append(space (10) + QStringLiteral(" "));
                                            jj = jj + 1;
                                        }
                                    else
                                        {
                                            tmp = str0 [ii - jj];
                                            do {
                                                check();
                                                j = position (tmp , str_ [ii]);
                                                if (j != 0)
                                                    str_ [ii].remove((j)-1, 77);
                                            } while (!(j == 0));
                                            resultsmemo.append(space (10) + userstring (1499 + ii - jj) + str_ [ii]);
                                        }
                                }
                            }
                            for (ii=112 + j3; ii<=122 + j3; ++ii)
                            {
                                check();
                                resultsmemo.append(space (10) + str_ [ii]);
                            }
                            if (fgetke() > 1)
                                resultsmemo.append(space (10) + tr(QStringLiteral("rs_design_margin_has_been_included_in_rt__pe__and_req_thr___rt__1_t__")));
                            resultsmemo.append(QStringLiteral(""));
                            resultsmemo.append(space (10) + QStringLiteral("Copyright (c) 1996, M. G. Parsons (PPP v1.8)"));
                            resultsmemo.append(space (10) + QStringLiteral("Copyright (c) 2008-2010, Timoshenko V.F."));
                            check();
                        }
                }
            check();
            ; // presentation only
            field(QStringLiteral("tbprint")) . enabled = true;
        }
}

bool Resistance_holtrop::execute() {
bool result{};
int units{};
float temper{};
int rightaxis{};
    units = settings. projectunits;
    if (units == 1)
        {
            warning(tr(QStringLiteral("rs_this_is_version_of_program_work_with_metric_system_units_only")));
            result = false;
            return result;
        }
    ; // presentation only
    xTitle = tr(QStringLiteral("rs_speed")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kn"));
    ;
    rightTitle = tr(QStringLiteral("rs_power")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kw"));
    units = settings. projectunits;
    temper = settings. projectwatertemper;
    field(QStringLiteral("densitybox")) . enabled = false;
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
