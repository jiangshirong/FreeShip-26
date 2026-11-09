// GPL-2.0-or-later. Generated from FreeResistance_OSTDlg.pas.
namespace {

struct Resistance_ost final: ResistanceBase {
using ResistanceBase::ResistanceBase;
float b_t{};
float l_d{};
float a_d{};
float l_b{};
float am{};
float cm{};
float cwp{};
float cb{};
float z0{};
float tc{};
QString pathfile{};
QString pathfileold{};
QString filetofind{};
QString filename{};
QString ftempdirectory{};
bool l{};
float capp_{};
int nwa{};
QString label10old{};
QString label11old{};
QString label15old{};
QString label16old{};
bool correctinputdata();
float fgetwettedsurface();
void checkbox2click(void* sender);
void drafttotalboxaftersetvalue(void* sender);
void estimateboxclick(void* sender);
void estimate2boxclick(void* sender);
void calculateresistanceost(Span<float> vs, float lcb, float cp, float &rf, float &rr, float &w, float &t0, float &nr);
void file_exportdata(Span<float> dat, Span<float> dan);
float fgetstartspeed();
void fsetstartspeed(float val);
float fgetendspeed();
void fsetendspeed(float val);
float fgetstepspeed();
void fsetstepspeed(float val);
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
void fsetnser(float val);
float fgetnser();
void fsetnf(float val);
float fgetnf();
void fsetna(float val);
float fgetna();
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
void fsetwettedsurface(float val);
float fgetwlarea();
void fsetwlarea(float val);
bool fgetextractfromhull();
void fsetextractfromhull(bool val);
float fgetdat17_1();
void fsetdat17_1(float val);
float fgetdat17_2();
void fsetdat17_2(float val);
float fgetdat17_3();
void fsetdat17_3(float val);
float fgetdat17_4();
void fsetdat17_4(float val);
float fgetdat17_5();
void fsetdat17_5(float val);
float fgetdat18_1();
void fsetdat18_1(float val);
float fgetdat18_2();
void fsetdat18_2(float val);
float fgetdat18_3();
void fsetdat18_3(float val);
float fgetdat18_4();
void fsetdat18_4(float val);
float fgetdat18_5();
void fsetdat18_5(float val);
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
if (member==QStringLiteral("nser")) { fsetnser(value); return; }
if (member==QStringLiteral("nf")) { fsetnf(value); return; }
if (member==QStringLiteral("na")) { fsetna(value); return; }
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
if (member==QStringLiteral("dat17_1")) { fsetdat17_1(value); return; }
if (member==QStringLiteral("dat17_2")) { fsetdat17_2(value); return; }
if (member==QStringLiteral("dat17_3")) { fsetdat17_3(value); return; }
if (member==QStringLiteral("dat17_4")) { fsetdat17_4(value); return; }
if (member==QStringLiteral("dat17_5")) { fsetdat17_5(value); return; }
if (member==QStringLiteral("dat18_1")) { fsetdat18_1(value); return; }
if (member==QStringLiteral("dat18_2")) { fsetdat18_2(value); return; }
if (member==QStringLiteral("dat18_3")) { fsetdat18_3(value); return; }
if (member==QStringLiteral("dat18_4")) { fsetdat18_4(value); return; }
if (member==QStringLiteral("dat18_5")) { fsetdat18_5(value); return; }
ResistanceBase::loadProperty(member,value);
}
void booleanChanged(const QString &key) override { if (key==QStringLiteral("estimatebox")) estimateboxclick(nullptr);if (key==QStringLiteral("estimate2box")) estimate2boxclick(nullptr); }
void extract() override { field("checkbox2").checked=true;checkbox2click(nullptr);field("checkbox2").checked=false;checkbox2click(nullptr); }
void initialise() override { execute(); }
void choiceChanged(const QString &key) override { if (key==QStringLiteral("combobox")) comboboxclick(nullptr); }
void runCalculation() override { calculateinworkspace(); }
};
bool Resistance_ost::correctinputdata() {
bool result{};

    result = false;
    if ((fgetdraft() <= 0.02))
        fsetdraft(0.021);
    if ((fgetdrafttotal() <= 0.02))
        fsetdrafttotal(0.021);
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
        fsetendspeed(6.0f);
    if ((fgetendspeed() <= fgetstartspeed()))
        ; // presentation only
    else
        ; // presentation only
    if ((fgetstepspeed() <= fgetendspeed()))
        ; // presentation only
    else
        ; // presentation only
    if ((fgetstepspeed() <= 0))
        fsetstepspeed(10.0f);
    if ((fgetdrafttotal() <= 0.02) || (fgetlwl() <= 0) || (fgetbwl() <= 0) || (fgetdisplacement() <= 0) || (fgetcp() <= 0) || (fgetviscosity() <= 0) || (fgetendspeed() <= fgetstartspeed()) || (fgetstepspeed() <= fgetendspeed()) || (fgetstartspeed() < 0.1))
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
    result = true;
    return result;
}

float Resistance_ost::fgetwettedsurface() {
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
                    if (cwp != 0)
                        {
                            c23 = 0.453 + 0.443 * (fgetcp() * cm) - 0.286 * cm - 0.00347 * (fgetbwl() / tc) + 0.37 * cwp;
                            scbfact = 0.616 * c23 + 0.111 * cm * cm * cm + 0.245 * (c23 / cm) - 0.0228;
                            result = scbfact * fgetlwl() * (2 * tc + fgetbwl()) * sqrtp (cm);
                        }
                    else
                        result = fgetlwl() * (1.36 * tc + 1.13 * fgetcp() * cm * fgetbwl());
                }
            else
                result = 0;
        }
    else
        result = field(QStringLiteral("wettedsurfacebox")) . value;
    return result;
}

void Resistance_ost::checkbox2click(void* sender) {

    field(QStringLiteral("lwlbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("bwlbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("wettedsurfacebox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("displacementbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("lcbbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("cpbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
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

void Resistance_ost::drafttotalboxaftersetvalue(void* sender) {
HydroInput hydobject{};
    tc = fgetdrafttotal();
    if ((field(QStringLiteral("checkbox2")) . checked) && (this != nullptr))
        {
            ; // local immutable hydrostatic request
            hydobject . draft = fgetdrafttotal();
            hydobject.data = hydro(hydobject.draft);
            z0 = hydValue(hydobject, QStringLiteral("modelmin.z"));
            tc = fgetdrafttotal() + z0;
            fsetlwl(hydValue(hydobject, QStringLiteral("lengthwaterline")));
            fsetbwl(hydValue(hydobject, QStringLiteral("beamwaterline")));
            fsetwettedsurface(hydValue(hydobject, QStringLiteral("wettedsurface")));
            fsetwlarea(hydValue(hydobject, QStringLiteral("waterplanearea")));
            fsetdisplacement(hydValue(hydobject, QStringLiteral("volume")));
            if (fgetlwl() != 0)
                fsetlcb(100 * (hydValue(hydobject, QStringLiteral("centerofbuoyancy.x")) - settings. projectlength * 0.5f) / fgetlwl());
            else
                fsetlcb(0);
            fsetcp(hydValue(hydobject, QStringLiteral("prismcoefficient")));
            cb = hydValue(hydobject, QStringLiteral("blockcoefficient"));
            cm = hydValue(hydobject, QStringLiteral("midshipcoeff"));
            cwp = hydValue(hydobject, QStringLiteral("waterplanecoeff"));
            check();
        }
}

void Resistance_ost::estimateboxclick(void* sender) {

    field(QStringLiteral("wettedsurfacebox")) . enabled = ! field(QStringLiteral("estimatebox")) . checked;
}

void Resistance_ost::estimate2boxclick(void* sender) {

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

void Resistance_ost::calculateresistanceost(Span<float> vs, float lcb, float cp, float &rf, float &rr, float &w, float &t0, float &nr) {
QString processcallerdirectory{};
int processcallercursor{};
float ca{};
float capp{};
float caero{};
float cb{};
float nrud{};
float ncalc{};
double c{};
double ke_{};
int units{};
float c_l{};
float a_l{};
float froudenumber{};
float t{};
float convertedspeed{};
Array<float,1,30> dat{};
Array<float,1,25> dan{};
Array<double,0,9> factors{};
int a{};
int lower{};
int sw_shownormal{};
float rudder_chord{};
float rudder_area{};
float lengthwaterline{};
float waterdensity{};
float waterviscosity{};
float displ{};
float wetarea{};
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
float e_1{};
float e_2{};
float e_3{};
float e_4{};
float e_5{};
float e_6{};
float e_7{};
float e_8{};
float e_9{};
float e_10{};
float frlmin{};
float frlmax{};
float frvmin{};
float frvmax{};
int destint{};
int i{};
int imax{};
bool l{};
QString execfullname{};
    cb = 0.0f;
    units = settings. projectunits;
    if (fgetnser() == 0)
        {
            field(QStringLiteral("edit17_1")) . enabled = true;
            field(QStringLiteral("edit17_2")) . enabled = true;
            field(QStringLiteral("edit17_3")) . enabled = true;
            field(QStringLiteral("edit17_4")) . enabled = true;
            field(QStringLiteral("edit17_5")) . enabled = true;
            field(QStringLiteral("edit18_1")) . enabled = true;
            field(QStringLiteral("edit18_2")) . enabled = true;
            field(QStringLiteral("edit18_3")) . enabled = true;
            field(QStringLiteral("edit18_4")) . enabled = true;
            field(QStringLiteral("edit18_5")) . enabled = true;
            field(QStringLiteral("nfbox")) . enabled = false;
            field(QStringLiteral("nabox")) . enabled = false;
        }
    else
        {
            field(QStringLiteral("edit17_1")) . enabled = false;
            field(QStringLiteral("edit17_2")) . enabled = false;
            field(QStringLiteral("edit17_3")) . enabled = false;
            field(QStringLiteral("edit17_4")) . enabled = false;
            field(QStringLiteral("edit17_5")) . enabled = false;
            field(QStringLiteral("edit18_1")) . enabled = false;
            field(QStringLiteral("edit18_2")) . enabled = false;
            field(QStringLiteral("edit18_3")) . enabled = false;
            field(QStringLiteral("edit18_4")) . enabled = false;
            field(QStringLiteral("edit18_5")) . enabled = false;
            field(QStringLiteral("nfbox")) . enabled = true;
            field(QStringLiteral("nabox")) . enabled = true;
        }
    field(QStringLiteral("label10")) . caption = QStringLiteral("");
    field(QStringLiteral("label11")) . caption = QStringLiteral("");
    field(QStringLiteral("label15")) . caption = label15old;
    field(QStringLiteral("label16")) . caption = label16old;
    if (fgetnser() == 1)
        {
            field(QStringLiteral("label10")) . caption = label10old + QStringLiteral("   (Nf = 2,4)");
            field(QStringLiteral("label11")) . caption = label11old + QStringLiteral("   (Na = 2)");
        }
    if (fgetnser() == 2)
        {
            field(QStringLiteral("label10")) . caption = label10old + QStringLiteral("   (Nf = 2)");
            field(QStringLiteral("label11")) . caption = label11old + QStringLiteral("   (Na = 2)");
        }
    if (fgetnser() == 3)
        {
            field(QStringLiteral("label10")) . caption = label10old + QStringLiteral("   (Nf = 1,2,4)");
            field(QStringLiteral("label11")) . caption = label11old + QStringLiteral("   (Na = 2)");
        }
    if (fgetnser() == 4)
        {
            field(QStringLiteral("label10")) . caption = label10old + QStringLiteral("   (Nf = 2...5)");
            field(QStringLiteral("label11")) . caption = label11old + QStringLiteral("   (Na = 1...3)");
        }
    if (fgetnser() == 5)
        {
            field(QStringLiteral("label10")) . caption = label10old + QStringLiteral("   (Nf = 1,2)");
            field(QStringLiteral("label11")) . caption = label11old + QStringLiteral("   (Na = 1,2)");
            field(QStringLiteral("nabox")) . enabled = false;
        }
    if (fgetnser() == 6)
        {
            field(QStringLiteral("label10")) . caption = tr(QStringLiteral("rs_type_of_ship______nship___1___4_"));
            field(QStringLiteral("nabox")) . enabled = false;
        }
    if ((fgetnser() == 7) || (fgetnser() == 8) || (fgetnser() == 9) || (fgetnser() == 10) || (fgetnser() == 11) || (fgetnser() == 12) || (fgetnser() == 13))
        {
            field(QStringLiteral("nfbox")) . enabled = false;
            field(QStringLiteral("nabox")) . enabled = false;
        }
    if (fgetnser() == 14)
        {
            field(QStringLiteral("label15")) . caption = tr(QStringLiteral("rs_number_of_hulls")) + QStringLiteral(" (Nh = 1...2)");
            field(QStringLiteral("label16")) . caption = tr(QStringLiteral("rs_distance_between_the_hulls__m"));
            field(QStringLiteral("nfbox")) . enabled = false;
            field(QStringLiteral("nabox")) . enabled = false;
        }
    if (fgetnser() == 15)
        {
            field(QStringLiteral("label15")) . caption = tr(QStringLiteral("rs_number_of_hulls")) + QStringLiteral(" (Nh = 1...3)");
            field(QStringLiteral("label16")) . caption = tr(QStringLiteral("rs_distance_between_centerplans_s__m"));
            field(QStringLiteral("nfbox")) . enabled = false;
            field(QStringLiteral("nabox")) . enabled = false;
            if (fgetnp() > 2)
                {
                    field(QStringLiteral("label11")) . caption = tr(QStringLiteral("rs_advance_of_central_hull_a__mm"));
                    field(QStringLiteral("nabox")) . enabled = true;
                }
            else
                {
                    field(QStringLiteral("label11")) . caption = QStringLiteral("");
                    field(QStringLiteral("nabox")) . enabled = false;
                }
        }
    if (fgetnser() == 16)
        {
            field(QStringLiteral("label10")) . caption = tr(QStringLiteral("rs_type_of_river_ship___nsh___1___3_"));
            field(QStringLiteral("label11")) . caption = tr(QStringLiteral("rs_prototype_of_river_ship__npr___1___")) + QStringLiteral("7)");
            field(QStringLiteral("nfbox")) . enabled = true;
            field(QStringLiteral("nabox")) . enabled = true;
            if (fgetnf() == 1)
                field(QStringLiteral("label16")) . caption = tr(QStringLiteral("rs_length_of_cylindrical_body__m"));
            if (fgetnf() == 3)
                field(QStringLiteral("label11")) . caption = tr(QStringLiteral("rs_prototype_of_river_ship__npr___1___")) + QStringLiteral("5)");
        }
    if (fgetnser() == 17)
        {
            field(QStringLiteral("label10")) . caption = tr(QStringLiteral("rs_type_of_fishing_ship__nsh___1___3_"));
            field(QStringLiteral("nfbox")) . enabled = true;
            field(QStringLiteral("nabox")) . enabled = false;
        }
    if (fgetnser() == 18)
        {
            field(QStringLiteral("label10")) . caption = tr(QStringLiteral("rs_method__nm_1___geers__nm_2___doubrovin_"));
            field(QStringLiteral("label11")) . caption = tr(QStringLiteral("rs_number_of_prototype_in_bd__npr___0___20_"));
            field(QStringLiteral("nfbox")) . enabled = true;
            field(QStringLiteral("nabox")) . enabled = true;
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_this_is_series_have_not_datas_of_prototype_in_bd_now___")));
        }
    if (fgetnser() == 19)
        {
            field(QStringLiteral("label15")) . caption = tr(QStringLiteral("rs_number_of_hulls")) + QStringLiteral(" (Nh = 2)");
            field(QStringLiteral("label16")) . caption = tr(QStringLiteral("rs_distance_between_centerplans_s__m"));
            field(QStringLiteral("nfbox")) . enabled = false;
            field(QStringLiteral("nabox")) . enabled = false;
        }
    if (fgetnser() > 19)
        {
            field(QStringLiteral("nfbox")) . enabled = false;
            field(QStringLiteral("nabox")) . enabled = false;
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_this_is_series_not_available_now___")));
            return;
        }
    ncalc = 1;
    nrud = 1;
    ca = 0.45;
    if ((fgetlwl() > 50) && (fgetlwl() <= 150))
        ca = 0.35;
    if ((fgetlwl() > 150) && (fgetlwl() <= 210))
        ca = 0.2;
    if ((fgetlwl() > 210) && (fgetlwl() <= 250))
        ca = 0.1;
    if ((fgetlwl() > 250) && (fgetlwl() <= 300))
        ca = 0.0f;
    if ((fgetlwl() > 300) && (fgetlwl() <= 350))
        ca = - 0.1;
    if ((fgetlwl() > 350) && (fgetlwl() <= 450))
        ca = - 0.2;
    capp = 0.05;
    if ((fgetnp() > 2) && (fgetnser() != 15))
        fsetnp(2);
    if (fgetnp() == 1)
        {
            if ((fgetlwl() > 50) && (fgetlwl() <= 150))
                capp = 0.15;
            if ((fgetlwl() > 150) && (fgetlwl() <= 200))
                capp = 0.1;
            if ((fgetlwl() > 200) && (fgetlwl() <= 400))
                capp = 0.05;
        }
    if ((fgetnp() == 2) && (nrud == 1))
        {
            if ((cb > 0.55) && (cb <= 0.6))
                capp = 0.45;
            if ((cb > 0.6) && (cb <= 0.7))
                capp = 0.40;
        }
    if ((fgetnp() == 2) && (nrud == 2))
        {
            if ((cb > 0.55) && (cb <= 0.6))
                capp = 0.6;
            if ((cb > 0.6) && (cb <= 0.7))
                capp = 0.55;
        }
    if (fgetnser() == 14)
        {
            capp = 0;
            ca = 0.05;
            if (fgetnp() == 2)
                ca = 0.1;
        }
    if (fgetnser() == 15)
        {
            capp = 0;
            ca = 0.05;
            if (fgetnp() == 2)
                ca = 0.1;
            if (fgetnp() == 3)
                ca = 0.15;
        }
    if (fgetnser() == 19)
        {
            capp = 0;
            ca = 0.1;
        }
    if (capp_ > 0)
        capp = 0;
    caero = 0.00f;
    if ((fgetnser() == 5) || ((fgetnser() >= 8) && (fgetnser() < 15)))
        caero = 0.05;
    if ((fgetnser() == 14) && (fgetnp() == 2))
        caero = 0.1;
    convertedspeed = vs [1];
    rf = 0.0f;
    rr = 0.0f;
    if (convertedspeed <= 0)
        return;
    if (settings. projectunits == 1)
        {
            lengthwaterline = fgetlwl() * foot;
            waterdensity = fgetdensity() / weightconversionfactor;
            waterviscosity = fgetviscosity() * 0.3048 * 0.3048 * 1e-6;
            displ = fgetdisplacement() * foot * foot * foot;
            wetarea = fgetwettedsurface() * foot * foot;
        }
    else
        {
            lengthwaterline = fgetlwl();
            waterdensity = fgetdensity();
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
    e_1 = fgetdat17_1();
    e_2 = fgetdat17_2();
    e_3 = fgetdat17_3();
    e_4 = fgetdat17_4();
    e_5 = fgetdat17_5();
    e_6 = fgetdat18_1();
    e_7 = fgetdat18_2();
    e_8 = fgetdat18_3();
    e_9 = fgetdat18_4();
    e_10 = fgetdat18_5();
    dat [1] = 2007;
    dat [2] = 14.07;
    dat [3] = fgetlwl();
    dat [4] = settings. projectlength;
    if (dat [4] == 1)
        dat [4] = dat [3];
    dat [5] = fgetbwl();
    dat [6] = tc;
    dat [7] = lcb / 100 * dat [4];
    dat [8] = wetarea;
    dat [9] = fgetdisplacement();
    dat [10] = dat [9] / dat [3] / dat [5] / dat [6];
    if ((dat [10] > 0.95))
        {
            warning(tr(QStringLiteral("rs_some_input_datas_are_not_right___")));
            ; // presentation only
            return;
        }
    dat [11] = cp;
    dat [12] = waterdensity * 1000.f;
    dat [13] = fgetviscosity();
    dat [14] = ca;
    dat [15] = capp;
    dat [16] = caero;
    dat [17] = fgetke();
    dat [18] = vs [0];
    dat [19] = vs [3];
    dat [20] = vs [5];
    dat [21] = vs [7];
    dat [22] = vs [9];
    dat [23] = 1;
    dat [24] = fgetnser();
    dat [25] = fgetnf();
    dat [26] = fgetna();
    if (fgetnser() > 4)
        dat [26] = 1;
    if (fgetnser() > 6)
        dat [25] = 1;
    dat [27] = fgetnp();
    if ((fgetnp() > 2) && (fgetnser() != 15))
        dat [27] = 2;
    dat [28] = 0;
    dat [29] = 0;
    if (fgetnser() == 14)
        {
            dat [28] = fgetnp();
            dat [29] = fgetdp();
        }
    if (fgetnser() == 15)
        dat [29] = fgetdp();
    if (fgetnser() == 16)
        {
            dat [25] = fgetnf();
            dat [26] = fgetna();
            dat [29] = fgetdp();
        }
    if ((fgetnser() == 17) || (fgetnser() == 18))
        dat [25] = fgetnf();
    if (fgetnser() == 19)
        {
            dat [28] = 2;
            if (fgetdp() == 0)
                dat [29] = fgetbwl() * 0.15;
            else
                dat [29] = fgetdp();
        }
    a_l = fgetna() / 1000 / fgetlwl();
    if (fgetnser() != 15)
        dat [30] = 999999;
    else
        dat [30] = a_l;
    frlmin = vs [0] * 0.51444 / sqrtp (9.81 * fgetlwl());
    frlmax = frlmin * vs [9] / vs [0];
    frvmin = vs [0] * 0.51444 / sqrtp (9.81 * power (dat [9] , 0.3333));
    frvmax = frvmin * vs [9] / vs [0];
    if (((frlmin < 0.1) || (frlmax > 0.23)) && (fgetnser() == 4))
        {
            resultsmemo.append(space (10) + QStringLiteral("Fr_min        = ") + number (frlmin , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Fr_max        = ") + number (frlmax , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Fr  ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,1 ... 0,23"));
            return;
        }
    if (((frlmin < 0.1) || (frlmax > 0.5f)) && (fgetnser() == 17) && (fgetnf() == 1))
        {
            resultsmemo.append(space (10) + QStringLiteral("Fr_min        = ") + number (frlmin , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Fr_max        = ") + number (frlmax , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Fr  ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,1 ... 0,5"));
            return;
        }
    if (((frlmin < 0.21) || (frlmax > 0.37)) && (fgetnser() == 17) && (fgetnf() == 2))
        {
            resultsmemo.append(space (10) + QStringLiteral("Fr_min        = ") + number (frlmin , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Fr_max        = ") + number (frlmax , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Fr  ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,21 ... 0,37"));
            return;
        }
    resultsmemo.append(space (10) + tr(QStringLiteral("rs_lcb")) + QStringLiteral("        = ") + number (dat [7] , 0 , 6 , 3) + QStringLiteral(" ") + lengthstr (units));
    resultsmemo.append(space (10) + tr(QStringLiteral("rs_lcb_lpp")) + QStringLiteral("        = ") + number (dat [7] / settings. projectlength , 0 , 6 , 4));
    resultsmemo.append(space (10) + QStringLiteral("Ca*10^3       = ") + number (ca , 0 , 6 , 3));
    if (capp > 0)
        resultsmemo.append(space (10) + QStringLiteral("Capp*10^3     = ") + number (capp , 0 , 6 , 3));
    resultsmemo.append(space (10) + QStringLiteral("Caero*10^3    = ") + number (caero , 0 , 6 , 3));
    resultsmemo.append(space (10) + QStringLiteral("Nser          = ") + number (dat [24] , 0 , 6 , 0));
    if ((fgetnser() < 14) && (fgetnser() > 15) && (fgetnser() != 6))
        {
            resultsmemo.append(space (10) + QStringLiteral("Nf            = ") + number (dat [25] , 0 , 6 , 0));
            resultsmemo.append(space (10) + QStringLiteral("Na            = ") + number (dat [26] , 0 , 6 , 0));
        }
    if (fgetnser() == 6)
        resultsmemo.append(space (10) + QStringLiteral("Nship         = ") + number (dat [25] , 0 , 6 , 0));
    if (fgetnser() == 14)
        {
            resultsmemo.append(space (10) + QStringLiteral("Nh            = ") + number (fgetnp() , 0 , 6 , 0));
            c_l = fgetdp() / fgetlwl();
            if (fgetnp() > 1)
                resultsmemo.append(space (10) + QStringLiteral("c/Lwl         = ") + number (c_l , 0 , 6 , 3));
            if ((c_l > 0.15) && (fgetnp() > 1))
                resultsmemo.append(space (10) + QStringLiteral("c/Lwl ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,01 ... 0,15"));
        }
    if (fgetnser() == 15)
        {
            resultsmemo.append(space (10) + QStringLiteral("Nh            = ") + number (fgetnp() , 0 , 6 , 0));
            c_l = fgetdp() / fgetlwl();
            if (fgetnp() > 1)
                resultsmemo.append(space (10) + QStringLiteral("S/Lwl         = ") + number (c_l , 0 , 6 , 3));
            if ((fgetdp() < fgetbwl()) && (fgetnp() > 1))
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_error_____s___bwl____")));
            if (fgetnp() == 3)
                resultsmemo.append(space (10) + QStringLiteral("A/Lwl         = ") + number (a_l , 0 , 6 , 3));
            if ((c_l > 0.45) && (fgetnp() > 1))
                resultsmemo.append(space (10) + QStringLiteral("S/Lwl ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,15 ... 0,45"));
            if ((a_l > 0.5f) && (fgetnp() == 3))
                resultsmemo.append(space (10) + QStringLiteral("A/Lwl ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,0  ... 0,5"));
        }
    if (fgetnser() == 16)
        {
            resultsmemo.append(space (10) + QStringLiteral("Nsh           = ") + number (fgetnf() , 0 , 6 , 0));
            resultsmemo.append(space (10) + QStringLiteral("Npr           = ") + number (fgetna() , 0 , 6 , 0));
            if (fgetnf() == 1)
                {
                    c_l = fgetdp() / fgetlwl();
                    resultsmemo.append(space (10) + QStringLiteral("Lc/Lwl        = ") + number (c_l , 0 , 6 , 3));
                    resultsmemo.append(space (10) + QStringLiteral("Np            = ") + number (fgetnp() , 0 , 6 , 0));
                    if ((c_l > 0.85) || (c_l < 0.5f))
                        resultsmemo.append(space (10) + QStringLiteral("Lc/Lwl ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,5 ... 0,85"));
                }
        }
    if (fgetnser() == 17)
        resultsmemo.append(space (10) + QStringLiteral("Nsh           = ") + number (fgetnf() , 0 , 6 , 0));
    if ((fgetnser() != 14) && (fgetnser() != 15) && (fgetnser() != 16))
        resultsmemo.append(space (10) + QStringLiteral("Np            = ") + number (fgetnp() , 0 , 6 , 0));
    if (((frlmin < 0.05) || (frlmax > 0.37)) && (fgetnser() < 4))
        {
            resultsmemo.append(space (10) + QStringLiteral("Fr_min        = ") + number (frlmin , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Fr_max        = ") + number (frlmax , 0 , 6 , 3));
            if (fgetnser() > 0)
                resultsmemo.append(space (10) + QStringLiteral("Fr  ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,05 ... 0,37"));
        }
    if ((fgetnser() == 16) && (fgetnf() != 3) && ((frlmin < 0.05) || (frlmax > 0.35)))
        resultsmemo.append(space (10) + QStringLiteral("FrL   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,05 ... 0,35"));
    if ((fgetnser() == 16) && (fgetnf() == 3) && ((frvmin < 0.3) || (frvmax > 0.8)))
        {
            resultsmemo.append(space (10) + QStringLiteral("FrV_min       = ") + number (frvmin , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("FrV_max       = ") + number (frvmax , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("FrV   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,3 ... 0,8"));
        }
    if ((fgetnser() == 15) && (frlmax > 0.6))
        resultsmemo.append(space (10) + QStringLiteral("Fr  ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,18 ... 0,6"));
    if (((frvmin < 0.5f) || (frvmax > 2.8)) && ((fgetnser() == 5) || ((fgetnser() >= 8) && (fgetnser() <= 14))))
        {
            resultsmemo.append(space (10) + QStringLiteral("FrV_min       = ") + number (frvmin , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("FrV_max       = ") + number (frvmax , 0 , 6 , 3));
            if (fgetnser() == 5)
                resultsmemo.append(space (10) + QStringLiteral("FrV ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,8 ... 2,6"));
            if (fgetnser() == 8)
                resultsmemo.append(space (10) + QStringLiteral("FrV ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 1,0 ... 2,5"));
            if (fgetnser() == 9)
                resultsmemo.append(space (10) + QStringLiteral("FrV ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,9 ... 2,0"));
            if (fgetnser() == 10)
                resultsmemo.append(space (10) + QStringLiteral("FrV ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,8 ... 2,7"));
            if (fgetnser() == 11)
                resultsmemo.append(space (10) + QStringLiteral("FrV ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 1,0 ... 2,0"));
            if (fgetnser() == 12)
                resultsmemo.append(space (10) + QStringLiteral("FrV ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,5 ... 2,8"));
            if (fgetnser() == 13)
                resultsmemo.append(space (10) + QStringLiteral("FrV ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,8 ... 3,0"));
            if (fgetnser() == 14)
                resultsmemo.append(space (10) + QStringLiteral("FrV ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,8 ... 2,2"));
            return;
        }
    if ((frvmin < 0.7) && (fgetnser() == 11))
        {
            resultsmemo.append(QStringLiteral("(FrVmin<0.7) and (Nser=11). Exit"));
            return;
        }
    if ((frvmin < 0.5f) && ((fgetnser() == 5) || ((fgetnser() >= 8) && (fgetnser() <= 14))))
        {
            resultsmemo.append(QStringLiteral("(FrVmin<0.5) and ((Nser=5) or ((Nser>=8) and (Nser<=14))). Exit"));
            return;
        }
    if (fgetke() == 0)
        {
            if ((a_9 <= 0) || (a_8 <= 0) || (a_6 <= 0) || (a_7 <= 0) || (a_11 <= 0))
                return;
            dan [1] = a_1;
            dan [2] = fgetks();
            dan [3] = a_2;
            dan [4] = a_3;
            dan [5] = a_4;
            dan [6] = a_5;
            dan [7] = a_11;
            dan [8] = a_6;
            dan [9] = a_7;
            dan [10] = a_8;
            dan [11] = a_9;
            dan [12] = a_10;
            dan [13] = 99;
            dan [14] = 999;
            dan [15] = 9999;
        }
    dan [16] = e_1;
    dan [17] = e_2;
    dan [18] = e_3;
    dan [19] = e_4;
    dan [20] = e_5;
    dan [21] = e_6;
    dan [22] = e_7;
    dan [23] = e_8;
    dan [24] = e_9;
    dan [25] = e_10;
    if (fgetnser() == 0)
        for (i=1; i<=5; ++i)
        {
            check();
            {
                if ((dan [15 + i] <= 0))
                    {
                        resultsmemo.append(QStringLiteral("Error: Fr[") + QString::number (i) + QStringLiteral("] is ") + number (dan [15 + i] , 0 , 6 , 3) + QStringLiteral(" <=0"));
                        return;
                    }
                if ((dan [20 + i] <= 0))
                    {
                        resultsmemo.append(QStringLiteral("Error: Cr[") + QString::number (i) + QStringLiteral("] is ") + number (dan [20 + i] , 0 , 6 , 3) + QStringLiteral(" <=0"));
                        return;
                    }
                if ((dan [15 + i] >= dan [16 + i]) && (i < 5))
                    {
                        resultsmemo.append(QStringLiteral("Error: Fr[") + QString::number (i) + QStringLiteral("]:") + number (dan [15 + i] , 0 , 6 , 3) + QStringLiteral(" >=") + QStringLiteral("Fr[") + QString::number (i + 1) + QStringLiteral("]:") + number (dan [16 + i] , 0 , 6 , 3));
                        return;
                    }
            }
        }
    filetofind = solverDirectory;
    file_exportdata (dat , dan);
    execfullname = solverDirectory + QStringLiteral("/") + QStringLiteral("hship.EXE");
    runSolver(execfullname, fchship);
    check();
}

void Resistance_ost::file_exportdata(Span<float> dat, Span<float> dan) {
int i{};
TextFile ffile{};
QString fn{};
    fn = work + QStringLiteral("/") + QStringLiteral("ostdat.tmp");
    if (exists (fn))
        check();
    ffile.name = safePath(fn);
    newFile(ffile);
    for (i=0; i<=29; ++i)
    {
        check();
        writeLine(ffile, dat [i]);
    }
    for (i=0; i<=24; ++i)
    {
        check();
        writeLine(ffile, dan [i]);
    }
    closeFile(ffile);
}

float Resistance_ost::fgetstartspeed() {
float result{};

    result = field(QStringLiteral("startspeedbox")) . value;
    return result;
}

void Resistance_ost::fsetstartspeed(float val) {

    field(QStringLiteral("startspeedbox")).set(val);
    if (field(QStringLiteral("startspeedbox")) . value == 0)
        field(QStringLiteral("startspeedbox")).set(1.f);
}

float Resistance_ost::fgetendspeed() {
float result{};

    result = field(QStringLiteral("endspeedbox")) . value;
    return result;
}

void Resistance_ost::fsetendspeed(float val) {

    field(QStringLiteral("endspeedbox")).set(val);
    if (field(QStringLiteral("endspeedbox")) . value == 0)
        field(QStringLiteral("endspeedbox")).set(5.f);
}

float Resistance_ost::fgetstepspeed() {
float result{};

    result = field(QStringLiteral("stepspeedbox")) . value;
    return result;
}

void Resistance_ost::fsetstepspeed(float val) {

    field(QStringLiteral("stepspeedbox")).set(val);
    if (field(QStringLiteral("stepspeedbox")) . value == 0)
        field(QStringLiteral("stepspeedbox")).set(6.f);
}

float Resistance_ost::fgetdensity() {
float result{};

    result = field(QStringLiteral("densitybox")) . value;
    return result;
}

void Resistance_ost::fsetdensity(float val) {

    field(QStringLiteral("densitybox")).set(val);
}

float Resistance_ost::fgetdisplacement() {
float result{};

    result = field(QStringLiteral("displacementbox")) . value;
    return result;
}

void Resistance_ost::fsetdisplacement(float val) {

    field(QStringLiteral("displacementbox")).set(val);
}

float Resistance_ost::fgetdraft() {
float result{};

    result = field(QStringLiteral("draftbox")) . value;
    return result;
}

void Resistance_ost::fsetdraft(float val) {

    field(QStringLiteral("draftbox")).set(val);
}

float Resistance_ost::fgetdrafttotal() {
float result{};

    result = field(QStringLiteral("drafttotalbox")) . value;
    return result;
}

void Resistance_ost::fsetdrafttotal(float val) {

    field(QStringLiteral("drafttotalbox")).set(val);
}

float Resistance_ost::fgetlwl() {
float result{};

    result = field(QStringLiteral("lwlbox")) . value;
    return result;
}

void Resistance_ost::fsetlwl(float val) {

    field(QStringLiteral("lwlbox")).set(val);
}

float Resistance_ost::fgetlcb() {
float result{};

    result = field(QStringLiteral("lcbbox")) . value;
    return result;
}

void Resistance_ost::fsetlcb(float val) {

    field(QStringLiteral("lcbbox")).set(val);
}

void Resistance_ost::fsetke(float val) {

    field(QStringLiteral("kebox")).set(val);
    if (field(QStringLiteral("kebox")) . value < 1)
        field(QStringLiteral("kebox")).set(1);
}

float Resistance_ost::fgetke() {
float result{};

    result = field(QStringLiteral("kebox")) . value;
    return result;
}

void Resistance_ost::fsetk1(float val) {

    field(QStringLiteral("k1box")).set(val);
}

float Resistance_ost::fgetk1() {
float result{};

    result = field(QStringLiteral("k1box")) . value;
    return result;
}

void Resistance_ost::fsetk2(float val) {

    field(QStringLiteral("k2box")).set(val);
}

float Resistance_ost::fgetk2() {
float result{};

    result = field(QStringLiteral("k2box")) . value;
    return result;
}

void Resistance_ost::fsetk3(float val) {

    field(QStringLiteral("k3box")).set(val);
}

float Resistance_ost::fgetk3() {
float result{};

    result = field(QStringLiteral("k3box")) . value;
    return result;
}

void Resistance_ost::fsetk4(float val) {

    field(QStringLiteral("k4box")).set(val);
}

float Resistance_ost::fgetk4() {
float result{};

    result = field(QStringLiteral("k4box")) . value;
    return result;
}

void Resistance_ost::fsetk5(float val) {

    field(QStringLiteral("k5box")).set(val);
}

float Resistance_ost::fgetk5() {
float result{};

    result = field(QStringLiteral("k5box")) . value;
    return result;
}

void Resistance_ost::fsetk6(float val) {

    field(QStringLiteral("k6box")).set(val);
}

float Resistance_ost::fgetk6() {
float result{};

    result = field(QStringLiteral("k6box")) . value;
    return result;
}

void Resistance_ost::fsetk7(float val) {

    field(QStringLiteral("k7box")).set(val);
}

float Resistance_ost::fgetk7() {
float result{};

    result = field(QStringLiteral("k7box")) . value;
    return result;
}

void Resistance_ost::fseta1(float val) {

    field(QStringLiteral("a1box")).set(val);
}

float Resistance_ost::fgeta1() {
float result{};

    result = field(QStringLiteral("a1box")) . value;
    return result;
}

void Resistance_ost::fseta2(float val) {

    field(QStringLiteral("a2box")).set(val);
}

float Resistance_ost::fgeta2() {
float result{};

    result = field(QStringLiteral("a2box")) . value;
    return result;
}

void Resistance_ost::fseta3(float val) {

    field(QStringLiteral("a3box")).set(val);
}

float Resistance_ost::fgeta3() {
float result{};

    result = field(QStringLiteral("a3box")) . value;
    return result;
}

void Resistance_ost::fseta4(float val) {

    field(QStringLiteral("a4box")).set(val);
}

float Resistance_ost::fgeta4() {
float result{};

    result = field(QStringLiteral("a4box")) . value;
    return result;
}

void Resistance_ost::fseta5(float val) {

    field(QStringLiteral("a5box")).set(val);
}

float Resistance_ost::fgeta5() {
float result{};

    result = field(QStringLiteral("a5box")) . value;
    return result;
}

void Resistance_ost::fseta6(float val) {

    field(QStringLiteral("a6box")).set(val);
}

float Resistance_ost::fgeta6() {
float result{};

    result = field(QStringLiteral("a6box")) . value;
    return result;
}

void Resistance_ost::fseta7(float val) {

    field(QStringLiteral("a7box")).set(val);
    if (field(QStringLiteral("a7box")) . value == 0)
        field(QStringLiteral("a7box")).set(1.226);
}

float Resistance_ost::fgeta7() {
float result{};

    result = field(QStringLiteral("a7box")) . value;
    return result;
}

void Resistance_ost::fseta8(float val) {

    field(QStringLiteral("a8box")).set(val);
}

float Resistance_ost::fgeta8() {
float result{};

    result = field(QStringLiteral("a8box")) . value;
    return result;
}

void Resistance_ost::fseta9(float val) {

    field(QStringLiteral("a9box")).set(val);
}

float Resistance_ost::fgeta9() {
float result{};

    result = field(QStringLiteral("a9box")) . value;
    return result;
}

void Resistance_ost::fseta10(float val) {

    field(QStringLiteral("a10box")).set(val);
}

float Resistance_ost::fgeta10() {
float result{};

    result = field(QStringLiteral("a10box")) . value;
    return result;
}

void Resistance_ost::fseta11(int val) {

    field(QStringLiteral("a11box")) . itemindex = val - 1;
}

int Resistance_ost::fgeta11() {
int result{};

    result = field(QStringLiteral("a11box")) . itemindex + 1;
    return result;
}

void Resistance_ost::fsetnser(float val) {

    field(QStringLiteral("nserbox")).set(val);
}

float Resistance_ost::fgetnser() {
float result{};

    result = field(QStringLiteral("nserbox")) . value;
    return result;
}

void Resistance_ost::fsetnf(float val) {

    field(QStringLiteral("nfbox")).set(val);
}

float Resistance_ost::fgetnf() {
float result{};

    result = field(QStringLiteral("nfbox")) . value;
    return result;
}

void Resistance_ost::fsetna(float val) {

    field(QStringLiteral("nabox")).set(val);
}

float Resistance_ost::fgetna() {
float result{};

    result = field(QStringLiteral("nabox")) . value;
    return result;
}

void Resistance_ost::fsetnp(float val) {

    field(QStringLiteral("npbox")).set(val);
}

float Resistance_ost::fgetnp() {
float result{};

    result = field(QStringLiteral("npbox")) . value;
    return result;
}

void Resistance_ost::fsetdp(float val) {

    field(QStringLiteral("dpbox")).set(val);
}

float Resistance_ost::fgetdp() {
float result{};

    result = field(QStringLiteral("dpbox")) . value;
    return result;
}

void Resistance_ost::fsetks(float val) {

    field(QStringLiteral("ksbox")).set(val);
}

float Resistance_ost::fgetks() {
float result{};

    result = field(QStringLiteral("ksbox")) . value;
    return result;
}

float Resistance_ost::fgetkeelchordlength() {
float result{};

    result = field(QStringLiteral("keelchordlengthbox")) . value;
    return result;
}

void Resistance_ost::fsetkeelchordlength(float val) {

    field(QStringLiteral("keelchordlengthbox")).set(val);
}

float Resistance_ost::fgetkeelarea() {
float result{};

    result = field(QStringLiteral("keelareabox")) . value;
    return result;
}

void Resistance_ost::fsetkeelarea(float val) {

    field(QStringLiteral("keelareabox")).set(val);
}

float Resistance_ost::fgetrudderchordlength() {
float result{};

    result = field(QStringLiteral("rudderchordlengthbox")) . value;
    return result;
}

void Resistance_ost::fsetrudderchordlength(float val) {

    field(QStringLiteral("rudderchordlengthbox")).set(val);
}

float Resistance_ost::fgetrudderarea() {
float result{};

    result = field(QStringLiteral("rudderareabox")) . value;
    return result;
}

void Resistance_ost::fsetrudderarea(float val) {

    field(QStringLiteral("rudderareabox")).set(val);
}

float Resistance_ost::fgetbwl() {
float result{};

    result = field(QStringLiteral("bwlbox")) . value;
    return result;
}

void Resistance_ost::fsetbwl(float val) {

    field(QStringLiteral("bwlbox")).set(val);
}

float Resistance_ost::fgetcp() {
float result{};

    result = field(QStringLiteral("cpbox")) . value;
    return result;
}

void Resistance_ost::fsetcp(float val) {

    field(QStringLiteral("cpbox")).set(val);
}

float Resistance_ost::fgetviscosity() {
float result{};

    result = field(QStringLiteral("viscositybox")) . value;
    return result;
}

void Resistance_ost::fsetviscosity(float val) {

    field(QStringLiteral("viscositybox")).set(val);
}

void Resistance_ost::fsetwettedsurface(float val) {

    field(QStringLiteral("wettedsurfacebox")).set(val);
}

float Resistance_ost::fgetwlarea() {
float result{};

    result = field(QStringLiteral("wlareabox")) . value;
    return result;
}

void Resistance_ost::fsetwlarea(float val) {

    field(QStringLiteral("wlareabox")).set(val);
}

bool Resistance_ost::fgetextractfromhull() {
bool result{};

    result = field(QStringLiteral("checkbox2")) . checked;
    return result;
}

void Resistance_ost::fsetextractfromhull(bool val) {

    if (field(QStringLiteral("checkbox2")) . checked != val)
        field(QStringLiteral("checkbox2")) . checked = val;
}

float Resistance_ost::fgetdat17_1() {
float result{};

    result = field(QStringLiteral("edit17_1")) . value;
    return result;
}

void Resistance_ost::fsetdat17_1(float val) {

    field(QStringLiteral("edit17_1")).set(val);
}

float Resistance_ost::fgetdat17_2() {
float result{};

    result = field(QStringLiteral("edit17_2")) . value;
    return result;
}

void Resistance_ost::fsetdat17_2(float val) {

    field(QStringLiteral("edit17_2")).set(val);
}

float Resistance_ost::fgetdat17_3() {
float result{};

    result = field(QStringLiteral("edit17_3")) . value;
    return result;
}

void Resistance_ost::fsetdat17_3(float val) {

    field(QStringLiteral("edit17_3")).set(val);
}

float Resistance_ost::fgetdat17_4() {
float result{};

    result = field(QStringLiteral("edit17_4")) . value;
    return result;
}

void Resistance_ost::fsetdat17_4(float val) {

    field(QStringLiteral("edit17_4")).set(val);
}

float Resistance_ost::fgetdat17_5() {
float result{};

    result = field(QStringLiteral("edit17_5")) . value;
    return result;
}

void Resistance_ost::fsetdat17_5(float val) {

    field(QStringLiteral("edit17_5")).set(val);
}

float Resistance_ost::fgetdat18_1() {
float result{};

    result = field(QStringLiteral("edit18_1")) . value;
    return result;
}

void Resistance_ost::fsetdat18_1(float val) {

    field(QStringLiteral("edit18_1")).set(val);
}

float Resistance_ost::fgetdat18_2() {
float result{};

    result = field(QStringLiteral("edit18_2")) . value;
    return result;
}

void Resistance_ost::fsetdat18_2(float val) {

    field(QStringLiteral("edit18_2")).set(val);
}

float Resistance_ost::fgetdat18_3() {
float result{};

    result = field(QStringLiteral("edit18_3")) . value;
    return result;
}

void Resistance_ost::fsetdat18_3(float val) {

    field(QStringLiteral("edit18_3")).set(val);
}

float Resistance_ost::fgetdat18_4() {
float result{};

    result = field(QStringLiteral("edit18_4")) . value;
    return result;
}

void Resistance_ost::fsetdat18_4(float val) {

    field(QStringLiteral("edit18_4")).set(val);
}

float Resistance_ost::fgetdat18_5() {
float result{};

    result = field(QStringLiteral("edit18_5")) . value;
    return result;
}

void Resistance_ost::fsetdat18_5(float val) {

    field(QStringLiteral("edit18_5")).set(val);
}

void Resistance_ost::comboboxclick(void* sender) {

    if (field(QStringLiteral("combobox")) . itemindex == 0)
        field(QStringLiteral("label9")) . caption = tr(QStringLiteral("rs_number_of_a_ship_series")) + QStringLiteral(" (Nser = 0 ... 30)");
    if (field(QStringLiteral("combobox")) . itemindex == 1)
        field(QStringLiteral("label9")) . caption = tr(QStringLiteral("rs_number_of_a_ship_series")) + QStringLiteral(" (Nser = 1 ... 4)");
    if (field(QStringLiteral("combobox")) . itemindex == 2)
        field(QStringLiteral("label9")) . caption = tr(QStringLiteral("rs_number_of_a_ship_series")) + QStringLiteral(" (Nser = 7;16)");
    if (field(QStringLiteral("combobox")) . itemindex == 3)
        field(QStringLiteral("label9")) . caption = tr(QStringLiteral("rs_number_of_a_ship_series")) + QStringLiteral(" (Nser = 5;8...13)");
    if (field(QStringLiteral("combobox")) . itemindex == 4)
        field(QStringLiteral("label9")) . caption = tr(QStringLiteral("rs_number_of_a_ship_series")) + QStringLiteral(" (Nser = 14;15;19)");
    if (field(QStringLiteral("combobox")) . itemindex == 5)
        field(QStringLiteral("label9")) . caption = tr(QStringLiteral("rs_number_of_a_ship_series")) + QStringLiteral(" (Nser = 6;17;18)");
}

void Resistance_ost::calculateinworkspace() {
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
float ae0{};
float x{};
float kde{};
float p_d{};
float z{};
float k{};
float tb{};
float ta{};
float dp_{};
float kdt{};
float vms{};
float kbulb{};
int index{};
int npapm{};
std::vector<float> cpopt{};
std::vector<float> lcbopt{};
float speed{};
float stepspeed1{};
QString line{};
QString tmp{};
int units{};
bool stop{};
int i{};
int j{};
int ij{};
int ji{};
int iii{};
int ispeed{};
int nstr{};
int nn{};
TextFile ffile{};
TextFile ffile2{};
Array<float,1,10> vs{};
Array<float,1,10> r_r{};
Array<Array<float,1,30>,1,10> res{};
Array<Array<float,1,5>,1,10> res_{};
Array<Array<float,1,10>,1,3> parres{};
float nser{};
float cb{};
float psi{};
float dvs{};
float rf_keel{};
float rn_keel{};
float cf_keel{};
float rn_rudder{};
float vmin{};
float w_tmp{};
    vmin = 0.1 * sqrtp (9.81 * fgetlwl()) / 0.51444;
    if (fgetstartspeed() < vmin)
        {
            fsetstartspeed(vmin + 0.01);
            fsetendspeed(fgetstartspeed() + 5.f);
            fsetstepspeed(fgetendspeed() + 1.f);
        }
    dvs = (fgetendspeed() - fgetstartspeed()) / 6;
    for (i=1; i<=7; ++i)
    {
        check();
        vs [i] = fgetstartspeed() + dvs * (i - 1);
    }
    vs [8] = fgetendspeed();
    vs [9] = (fgetendspeed() + fgetstepspeed()) / 2;
    vs [10] = fgetstepspeed();
    nser = fgetnser();
    if ((nser < 5) && (fgetna() == 0))
        fsetna(2);
    if ((nser < 5) && (fgetnf() == 0))
        fsetnf(2);
    if (nser > 0)
        fsetdat17_1(0);
    clearSeries(QStringLiteral("series1"));
    clearSeries(QStringLiteral("series2"));
    clearSeries(QStringLiteral("series3"));
    clearSeries(QStringLiteral("series4"));
    ; // presentation only
    ; // presentation only
    field(QStringLiteral("printbutton")) . enabled = false;
    resultsmemo2.clear();
    resultsmemo2.append(tr(QStringLiteral("rs_resistance_mh_note")));
    resultsmemo2.append(QStringLiteral(""));
    resultsmemo2.append(QStringLiteral("Copyright (c) 2007-2012, Timoshenko V.F."));
    ; // presentation only
    if (correctinputdata())
        {
            units = settings. projectunits;
            resultsmemo.clear();
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            header (space (10) + tr(QStringLiteral("rs_program_of_resistance_prediction_for_displacement_ships_hydronship_v1_08__nuos__ukraine_")) + QStringLiteral(".") , resultsmemo);
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral("          ----------------------------------------------------------------------------------"));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_input_variables")));
            resultsmemo.append(QStringLiteral("          ----------------------------------------------------------------------------------"));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_general")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_start_speed")) , 35) + QStringLiteral(" : ") + number (vs [1] , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_kn")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_end_speed")) , 35) + QStringLiteral(" : ") + number (vs [10] , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_kn")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_water_density")) , 35) + QStringLiteral(" : ") + number (fgetdensity() , 0 , 8 , 3) + QString(QChar(32)) + densitystr (units));
            if (units == 1)
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_water_viscosity")) , 35) + QStringLiteral(" : ") + number (fgetviscosity() , 0 , 8 , 4) + QStringLiteral("*10^(-6) ") + tr(QStringLiteral("rs_ft_2_s")));
            else
                resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_water_viscosity")) , 35) + QStringLiteral(" : ") + number (fgetviscosity() , 0 , 8 , 4) + QStringLiteral("*10^(-6) ") + tr(QStringLiteral("rs_m_2_s")));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_hull")));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_length_on_waterline")) , 35) + QStringLiteral(" : ") + number (fgetlwl() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_beam_on_waterline")) , 35) + QStringLiteral(" : ") + number (fgetbwl() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_draft_on_midship")) , 35) + QStringLiteral(" : ") + number (tc , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            stop = false;
            if ((absp (fgetdraft()) / fgetlwl() > 0.05) || (absp (fgetdraft()) > fgetdrafttotal()))
                {
                    resultsmemo.append(space (16) + tr(QStringLiteral("rs_warning____very_big_trim__tf_ta_")));
                    stop = true;
                }
            if (fgetwettedsurface() > 0)
                if (field(QStringLiteral("estimatebox")) . checked)
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_wetted_surface_area")) , 35) + QStringLiteral(" : ") + number (fgetwettedsurface() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(")"));
                else
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_wetted_surface_area")) , 35) + QStringLiteral(" : ") + number (fgetwettedsurface() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_waterplane_area")) , 35) + QStringLiteral(" : ") + number (fgetwlarea() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_displacement")) , 35) + QStringLiteral(" : ") + number (fgetdisplacement() , 0 , 6 , 3) + QString(QChar(32)) + volstr (units));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_longitudinal_center_of_buoyancy")) , 35) + QStringLiteral(" : ") + number (fgetlcb() , 0 , 6 , 3) + QStringLiteral(" %"));
            resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_prismatic_coefficient")) , 35) + QStringLiteral(" : ") + number (fgetcp() , 0 , 6 , 4));
            resultsmemo.append(QStringLiteral(""));
            if ((fgetk1() + fgetk2() + fgetk3() + fgetk4() + fgetk5() + fgetk6() + fgetk7() + fgetkeelchordlength() + fgetkeelarea() + fgetrudderchordlength() + fgetrudderarea() > 0))
                {
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_appendages_areas_")));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_shaft_brackets_area")) , 35) + QStringLiteral(" : ") + number (fgetk1() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_strut_bossing_area")) , 35) + QStringLiteral(" : ") + number (fgetk2() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_hull_bossing_area")) , 35) + QStringLiteral(" : ") + number (fgetk4() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_stabiliser_fins_area")) , 35) + QStringLiteral(" : ") + number (fgetk3() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_dome_area")) , 35) + QStringLiteral(" : ") + number (fgetk5() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_exposed_shafts_area_")) , 35));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_angle_vs_buttoks_about_10__")) , 35) + QStringLiteral(" : ") + number (fgetk6() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_angle_vs_buttoks_about_20__")) , 35) + QStringLiteral(" : ") + number (fgetk7() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(QStringLiteral(""));
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_areas_of_keels")));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_skeg")) , 35) + QStringLiteral(" : ") + number (fgetkeelchordlength() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_bilge_keels")) , 35) + QStringLiteral(" : ") + number (fgetkeelarea() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(QStringLiteral(""));
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_areas_of_rudders")));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_rudder_behind_skeg_stern")) , 35) + QStringLiteral(" : ") + number (fgetrudderchordlength() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(space (16) + pad (tr(QStringLiteral("rs_2_screw_rudder_slender_thick")) , 35) + QStringLiteral(" : ") + number (fgetrudderarea() , 0 , 6 , 3) + QString(QChar(32)) + areastr (units));
                    resultsmemo.append(QStringLiteral(""));
                }
            if ((fgeta1() + fgeta2() + fgeta3() + fgeta4() + fgeta5() + fgeta6() + fgeta7() + fgeta8() + fgeta9() + fgeta10() + fgetks() > 0) && field(QStringLiteral("estimate2box")) . checked)
                {
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
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label17")) . caption , 45) + QStringLiteral(" : ") + number (fgetks() , 0 , 6 , 0));
                    resultsmemo.append(space (16) + pad (field(QStringLiteral("label6_11")) . caption , 45) + QStringLiteral(" : ") + number (fgeta11() , 0 , 6 , 0));
                    resultsmemo.append(QStringLiteral(""));
                }
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_calculated_variables")));
            resultsmemo.append(QStringLiteral("          ----------------------------------------------------------------------------------"));
            resultsmemo.append(QStringLiteral(""));
            cb = fgetcp() * cm;
            if (fgetwettedsurface() <= 0)
                {
                    kbulb = 0;
                    if (fgetwettedsurface() == 0)
                        {
                            nwa = 0;
                            s = fgetlwl() * (2 * tc + fgetbwl()) * sqrtp (cm) * (0.453 + 0.442 * cb - 0.2862 * cm - 0.003467 * fgetbwl() / tc + 0.3696 * cwp);
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_method_holtrop_1988")) + QStringLiteral(")"));
                            if (cwp == 0)
                                resultsmemo.append(space (10) + tr(QStringLiteral("rs_because_cwp_0__this_wetted_area_is_not_right___")));
                        }
                    if (fgetwettedsurface() == - 1)
                        {
                            nwa = - 1;
                            s = fgetlwl() * (1.36 * tc + 1.13 * cb * fgetbwl());
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_muragin_s_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 2)
                        {
                            nwa = - 2;
                            s = fgetlwl() * tc * (2 + 1.37 * (cb - 0.274) * fgetbwl() / tc);
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_semeka_s_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 3)
                        {
                            nwa = - 3;
                            s = fgetlwl() * (cb * fgetbwl() + 1.7 * tc);
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_moomford_s_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 4)
                        {
                            nwa = - 4;
                            s = fgetlwl() * (fgetbwl() + 2 * tc) * (0.76 * cb + 0.28);
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_benhe_s_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 5)
                        {
                            nwa = - 5;
                            s = fgetlwl() * (0.85 * fgetbwl() + 0.25f * tc);
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_for_ship_with_transom")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 6)
                        {
                            nwa = - 6;
                            s = power (fgetdisplacement() , 0.666666) * (5.1 + 0.074 * fgetlwl() / tc - 0.4 * cb);
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_karpov_s_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 7)
                        {
                            nwa = - 7;
                            s = settings. projectlength * (0.5f * fgetbwl() + tc) * (0.55 + 1.52 * cb);
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_yeroshin_s_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 8)
                        {
                            nwa = - 8;
                            s = power (fgetdisplacement() , 0.3333) * (3.4 * power (fgetdisplacement() , 0.3333) + 0.5f * fgetlwl());
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_lap_1954_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 9)
                        {
                            nwa = - 9;
                            s = (fgetdisplacement() / fgetbwl() * 1.7 / (cb - 0.2 * (cb - 0.65)) + fgetbwl() / tc) * 2.f;
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_danckwardt_1969_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 10)
                        {
                            nwa = - 10;
                            s = (fgetdisplacement() / fgetbwl() * 1.7 / cb + fgetbwl() / tc * (0.92 + 0.092 * cb)) * 2.f;
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_danckwardt_1969_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 11)
                        {
                            nwa = - 11;
                            s = settings. projectlength * (1.8 * tc + fgetbwl() * cb);
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_schneekluth_1988_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 12)
                        {
                            nwa = - 12;
                            s = 3.223 * power (fgetdisplacement() , 0.6666) + 0.5402 * fgetlwl() * power (fgetdisplacement() , 0.3333);
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_van_oortmerssen_1971_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 13)
                        {
                            nwa = - 13;
                            s = fgetdisplacement() * (1.f / tc + 1.75f / fgetbwl() / cb);
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_grigoriev_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 14)
                        {
                            nwa = - 14;
                            s = fgetlwl() * (2 * tc + fgetbwl()) * (0.654 * cb + 0.338);
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_benge_afanasiev_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 15)
                        {
                            nwa = - 15;
                            s = (0.085 * fgetbwl() / tc - 2 * cb + 3.29) * sqrtp (fgetdisplacement() * fgetlwl());
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_vashedchenko_1985_formulae")) + QStringLiteral(")"));
                        }
                    if (fgetwettedsurface() == - 16)
                        {
                            nwa = - 16;
                            s = fgetlwl() * (2 * tc + fgetbwl()) * (4.84 * cb - 4.15 * cb * cb - 0.77);
                            resultsmemo.append(space (10) + QStringLiteral("S             = ") + number (s , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_for_megayachts_formulae")) + QStringLiteral(")"));
                        }
                    fsetwettedsurface(s);
                }
            resultsmemo.append(space (10) + QStringLiteral("Cp            = ") + number (fgetcp() , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("Cb            = ") + number (cb , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("Cwp           = ") + number (cwp , 0 , 6 , 4));
            if ((cwp == 0) && (nwa == 0))
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_wetted_area_by_holtrop_formulae_is_not_right__because_cwp_0____")));
            resultsmemo.append(space (10) + QStringLiteral("Cm            = ") + number (cm , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("Am            = ") + number (am , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            if (z0 != 0.0f)
                resultsmemo.append(space (10) + QStringLiteral("Tc            = ") + number (tc , 0 , 6 , 3) + QStringLiteral(" ") + lengthstr (units));
            resultsmemo.append(space (10) + QStringLiteral("Lwl/Bwl       = ") + number (fgetlwl() / fgetbwl() , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Bwl/T         = ") + number (fgetbwl() / tc , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Lwl/T         = ") + number (fgetlwl() / tc , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Psi           = ") + number (fgetlwl() / power (fgetdisplacement() , 0.3333) , 0 , 6 , 3));
            rn_keel = fgetkeelchordlength() + fgetkeelarea() + fgetrudderchordlength() + fgetrudderarea() + fgetk1() + fgetk2() + fgetk3() + fgetk4() + fgetk5() + fgetk6() + fgetk7();
            capp_ = 0;
            if ((nser <= 15) && (nser >= 14))
                if ((cb > 0.9) || (cb < 0.2))
                    {
                        resultsmemo.append(space (10) + QStringLiteral("Cb   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" 0,2...0,9"));
                        ; // presentation only
                        return;
                    }
            if (rn_keel > 0)
                {
                    cf_keel = fgetkeelchordlength() * 1.75f + fgetkeelarea() * 1.4 + fgetrudderchordlength() * 1.4 + fgetrudderarea() * 3.f + fgetk1() * 4 + fgetk2() * 3.5f + fgetk3() * 2.8 + fgetk4() * 2.f + fgetk5() * 2.7 + fgetk6() * 2 + fgetk7() * 5;
                    for (i=1; i<=10; ++i)
                    {
                        check();
                        {
                            speed = vs [i];
                            convertedspeed = speed * 1852 / 3600;
                            rf_keel = 0;
                            rn_rudder = convertedspeed * fgetlwl() / fgetviscosity() * 1000000.f;
                            rf_keel = 0.075 / square (logp (rn_rudder) - 2) * cf_keel / rn_keel * 0.5f * fgetdensity() * square (convertedspeed) * rn_keel;
                            r_r [i] = rf_keel;
                            capp_ = cf_keel / rn_keel;
                        }
                    }
                }
            ; // presentation only
            flag = 0.001;
            speed = vs [10];
            dvs = (vs [8] - vs [1]) / 7;
            ftempdirectory = work;
            check();
            for (j=1; j<=10; ++j)
            {
                check();
                vs [j] = vs [1] + dvs * (j - 1);
            }
            vs [9] = (fgetendspeed() + fgetstepspeed()) / 2;
            vs [10] = fgetstepspeed();
            calculateresistanceost (vs , fgetlcb() , fgetcp() , rf , rr , w , t0 , nr);
            check();
            filename = QStringLiteral("ostres.tmp");
            nn = 2;
            if (exists (ftempdirectory + QStringLiteral("/") + QStringLiteral("ostres.tmp")))
                {
                    ffile.name = safePath(ftempdirectory + QStringLiteral("/") + QStringLiteral("ostres.tmp"));
                    readFile(ffile);
                    readValues(ffile, nser);
                    if ((nser == 1) || (nser == 0))
                        {
                            nstr = 22;
                            nn = 2;
                        }
                    if (nser == 2)
                        {
                            nstr = 23;
                            nn = 1;
                        }
                    if (nser == 3)
                        {
                            nstr = 24;
                            nn = 0;
                        }
                    if (nser == 4)
                        {
                            nstr = 24;
                            nn = 0;
                        }
                    if (nser >= 5)
                        {
                            nstr = 24;
                            nn = 0;
                        }
                    for (j=1; j<=8; ++j)
                    {
                        check();
                        for (i=1; i<=3; ++i)
                        {
                            check();
                            readValues(ffile, parres [i][j]);
                        }
                    }
                    for (j=1; j<=nstr; ++j)
                    {
                        check();
                        for (i=1; i<=10; ++i)
                        {
                            check();
                            readValues(ffile, res [i][j]);
                        }
                    }
                    flag = 0.001;
                    if (res [10][17 - nn] > 10)
                        flag = 1.f;
                    for (i=1; i<=10; ++i)
                    {
                        check();
                        {
                            point(QStringLiteral("series1"), res [i][1] ,  res [i][17 - nn] / flag );
                            point(QStringLiteral("series3"), res [i][1] ,  res [i][19 - nn] / flag );
                            point(QStringLiteral("series2"), res [i][1] ,  res [i][21 - nn] / flag );
                            point(QStringLiteral("series4"), res [i][1] ,  res [i][23 - nn] / flag );
                        }
                    }
                    closeFile(ffile);
                }
            else
                {
                    resultsmemo.append(QStringLiteral("Result file ") + ftempdirectory + QStringLiteral("/") + QStringLiteral("ostres.tmp") + QStringLiteral(" was not created"));
                    return;
                }
            if (exists (ftempdirectory + QStringLiteral("/") + QStringLiteral("ostres.tmp")))
                check();
            if (exists (ftempdirectory + QStringLiteral("/") + QStringLiteral("Resist.dat")))
                check();
            if (exists (ftempdirectory + QStringLiteral("/") + QStringLiteral("Resistp.dat")))
                check();
            if ((fgetke() == 0) && (res [10][17 - nn] > 0))
                fsetke(res [10][21 - nn] / res [10][17 - nn]);
            i = 1;
            if (nser > 0)
                {
                    if ((parres [3][1] < parres [1][1]) || (parres [3][1] > parres [2][1]))
                        {
                            if (nser != 6)
                                resultsmemo.append(space (10) + QStringLiteral("Cb    ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][1] , 0 , 6 , 4) + QStringLiteral("...") + number (parres [2][1] , 0 , 6 , 4));
                            else
                                resultsmemo.append(space (10) + QStringLiteral("Cp    ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][1] , 0 , 6 , 4) + QStringLiteral("...") + number (parres [2][1] , 0 , 6 , 4));
                            i = 0;
                        }
                    if ((parres [3][2] < parres [1][2]) || (parres [3][2] > parres [2][2]))
                        if (nser < 5)
                            {
                                resultsmemo.append(space (10) + tr(QStringLiteral("rs_lcb_lpp")) + QStringLiteral(" ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][2] , 0 , 6 , 4) + QStringLiteral("...") + number (parres [2][2] , 0 , 6 , 4));
                                i = 0;
                            }
                    if ((parres [3][3] < parres [1][3]) || (parres [3][3] > parres [2][3]))
                        {
                            if ((nser != 6) && (nser != 17) && (nser != 14) && (nser != 15))
                                resultsmemo.append(space (10) + QStringLiteral("L/B   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][3] , 0 , 6 , 3) + QStringLiteral("...") + number (parres [2][3] , 0 , 6 , 3));
                            if (nser == 17)
                                resultsmemo.append(space (10) + QStringLiteral("Lpp/B ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][3] , 0 , 6 , 3) + QStringLiteral("...") + number (parres [2][3] , 0 , 6 , 3));
                            if ((nser == 14) || (nser == 15))
                                resultsmemo.append(space (10) + QStringLiteral("Lwl/Bwl ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][3] , 0 , 6 , 3) + QStringLiteral("...") + number (parres [2][3] , 0 , 6 , 3));
                            if (nser != 6)
                                i = 0;
                        }
                    if ((parres [3][4] < parres [1][4]) || (parres [3][4] > parres [2][4]))
                        {
                            resultsmemo.append(space (10) + QStringLiteral("B/T   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][4] , 0 , 6 , 3) + QStringLiteral("...") + number (parres [2][4] , 0 , 6 , 3));
                            i = 0;
                        }
                    if ((parres [3][5] < parres [1][5]) || (parres [3][5] > parres [2][5]))
                        {
                            if (nser != 17)
                                resultsmemo.append(space (10) + QStringLiteral("Psi   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][5] , 0 , 6 , 3) + QStringLiteral("...") + number (parres [2][5] , 0 , 6 , 3));
                            else
                                resultsmemo.append(space (10) + QStringLiteral("Cm    ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][5] , 0 , 6 , 3) + QStringLiteral("...") + number (parres [2][5] , 0 , 6 , 3));
                            i = 0;
                        }
                    if ((parres [3][6] < parres [1][6]) || (parres [3][6] > parres [2][6]))
                        {
                            if ((nser == 14) || (nser == 15))
                                resultsmemo.append(space (10) + QStringLiteral("Nh    ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][6] , 0 , 6 , 0) + QStringLiteral("...") + number (parres [2][6] , 0 , 6 , 0));
                            else
                                resultsmemo.append(space (10) + QStringLiteral("Np    ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][6] , 0 , 6 , 0) + QStringLiteral("...") + number (parres [2][6] , 0 , 6 , 0));
                            i = 0;
                        }
                    if (nser != 3)
                        if ((parres [3][7] < parres [1][7]) || (parres [3][7] > parres [2][7]))
                            {
                                if (nser == 16)
                                    resultsmemo.append(space (10) + QStringLiteral("Nsh   ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][7] , 0 , 6 , 0) + QStringLiteral("...") + number (parres [2][7] , 0 , 6 , 0));
                                if (nser == 6)
                                    resultsmemo.append(space (10) + QStringLiteral("Nship ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][7] , 0 , 6 , 0) + QStringLiteral("...") + number (parres [2][7] , 0 , 6 , 0));
                                if ((nser != 6) && (nser != 16))
                                    resultsmemo.append(space (10) + QStringLiteral("Nf    ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][7] , 0 , 6 , 0) + QStringLiteral("...") + number (parres [2][7] , 0 , 6 , 0));
                                i = 0;
                            }
                }
            if (nser == 3)
                if ((parres [3][7] == 3) || (parres [3][7] == 5))
                    {
                        resultsmemo.append(space (10) + QStringLiteral("Nf    ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][7] , 0 , 6 , 0) + QStringLiteral("...") + number (parres [2][7] , 0 , 6 , 0) + QStringLiteral(",4"));
                        i = 0;
                    }
            if ((parres [3][8] < parres [1][8]) || (parres [3][8] > parres [2][8]))
                {
                    if ((nser != 15) && (nser != 16) && (nser != 0))
                        resultsmemo.append(space (10) + QStringLiteral("Na    ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][8] , 0 , 6 , 0) + QStringLiteral("...") + number (parres [2][8] , 0 , 6 , 0));
                    if (nser == 16)
                        resultsmemo.append(space (10) + QStringLiteral("Npr    ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" ") + number (parres [1][8] , 0 , 6 , 0) + QStringLiteral("...") + number (parres [2][8] , 0 , 6 , 0));
                    if (nser != 0)
                        i = 0;
                }
            if (i == 0)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_because_extrapolation_is_used__can_be_some_errores_")));
            for (iii=1; iii<=9; ++iii)
            {
                check();
                resultsmemo.append(QStringLiteral(""));
            }
            resultsmemo.append(QStringLiteral(""));
            if (nser == 0)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_according_to_modeling_experiment")));
            if (nser == 5)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_diagram_of_braun")));
            if (nser == 6)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_diagram_of_neyman")));
            if (nser == 7)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_method_of_niewt__russia_")));
            if (nser == 8)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_diagram_of_volodin")));
            if (nser == 9)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_diagram_of_nordstrom")));
            if (nser == 10)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_diagram_of_de_groot")));
            if (nser == 11)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_diagram_of_sspa")));
            if (nser == 12)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_diagram_of_npl")));
            if (nser == 13)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_diagram_of_64_series")));
            if (nser == 14)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_method_of_doubrovsky")));
            if (nser == 15)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_a_method_of_doubrovsky")));
            if (nser == 16)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_methods_niewt__giewt_and_liwt__russia_")));
            if (nser == 17)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_method_of_yeroshin")));
            if ((nser == 18) && (fgetnf() == 1))
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_method_of_geers")));
            if ((nser == 18) && (fgetnf() == 2))
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_method_of_doubrovin")));
            if (nser == 19)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_resistance_and_power_are_calculated")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_hard_chine_catamarans_regression_method")));
            if ((nser > 0) && (nser < 5))
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_and_power_prediction_by")) + QStringLiteral(" ") + tr(QStringLiteral("rs_by_oct_5_0181_75__xussr_")));
            resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+--------+--------+--------+---------+--------+---------+"));
            if ((nser == 5) || ((nser >= 8) && (nser < 15)))
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_l_speed_l_speed_l_fr_v__l___r_f__l___r_r__l___r_t__l__power__l__r_t_e_l_power_e_l")));
            else
                if (nser == 19)
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_l_speed_l_speed_l__fr___l___r_v__l___r_r__l___r_t__l__power__l__r_t_e_l_power_e_l")));
                else
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_l_speed_l_speed_l__fr___l___r_f__l___r_r__l___r_t__l__power__l__r_t_e_l_power_e_l")));
            if (flag == 1)
                {
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn__l__m_s__l_______l___kn___l___kn___l____kn__l____kw___l____kn__l____kw___l")));
                    yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral("*10, ") + tr(QStringLiteral("rs_kn")) + QStringLiteral(";   ") + tr(QStringLiteral("rs_power")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kw"));
                }
            else
                {
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn__l__m_s__l_______l____n___l____n___l____n___l____w____l____n___l____w____l")));
                    yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral("*10, ") + tr(QStringLiteral("rs_n")) + QStringLiteral(";   ") + tr(QStringLiteral("rs_power")) + QStringLiteral(", ") + tr(QStringLiteral("rs_w"));
                }
            resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+--------+--------+--------+---------+--------+---------+"));
            index = 0;
            ffile2.name = safePath(ftempdirectory + QStringLiteral("/") + QStringLiteral("Resistp.dat"));
            newFile(ffile2);
            for (ispeed=1; ispeed<=10; ++ispeed)
            {
                check();
                {
                    speed = res [ispeed][1];
                    convertedspeed = speed * 1852 / 3600;
                    if (settings. projectunits == 1)
                        froudenumber = convertedspeed / sqrtp (9.81 * foot * fgetlwl());
                    else
                        froudenumber = convertedspeed / sqrtp (9.81 * fgetlwl());
                    if ((nser == 5) || ((nser >= 8) && (nser <= 14)))
                        froudenumber = res [ispeed][4 - nn];
                    rf = res [ispeed][12 - nn] * fgetdensity() * fgetwettedsurface() / 2 * res [ispeed][2] * res [ispeed][2] / 1000;
                    rr = (res [ispeed][16 - nn] - res [ispeed][12 - nn]) * fgetdensity() * fgetwettedsurface() / 2 * res [ispeed][2] * res [ispeed][2] / 1000;
                    if (rr < 0)
                        resultsmemo.append(QStringLiteral(" ATTENTION!!! Input data Error =>  R_r < 0 !"));
                    rr = rr + r_r [ispeed];
                    line = formatValue(speed, 6, 2);
                    line = QStringLiteral("|") + line + QStringLiteral(" |");
                    tmp = formatValue(convertedspeed, 6, 2);
                    line = line + tmp + QStringLiteral(" |");
                    tmp = formatValue(froudenumber, 6, 3);
                    line = line + tmp + QStringLiteral(" |");
                    tmp = formatValue(rf / flag, 7, 1);
                    line = line + tmp + QStringLiteral(" |");
                    if (((froudenumber >= 0) && (froudenumber <= 0.6) || (nser == 5) || (nser == 8) || (nser == 9) || (nser == 10) || (nser == 11) || (nser == 12) || (nser == 13) || (nser == 14) || (nser == 15) || (nser == 16) || (nser == 17) || (nser == 19)))
                        {
                            if ((nser < 5) && (nser > 0))
                                ; // presentation only
                            if (nser == 0)
                                ; // presentation only
                            if (nser == 5)
                                ; // presentation only
                            if (nser == 6)
                                ; // presentation only
                            if (nser == 7)
                                ; // presentation only
                            if (nser == 8)
                                ; // presentation only
                            if (nser == 9)
                                ; // presentation only
                            if (nser == 10)
                                ; // presentation only
                            if (nser == 11)
                                ; // presentation only
                            if (nser == 12)
                                ; // presentation only
                            if (nser == 13)
                                ; // presentation only
                            if (nser == 14)
                                ; // presentation only
                            if (nser == 15)
                                ; // presentation only
                            if (nser == 16)
                                ; // presentation only
                            if (nser == 17)
                                ; // presentation only
                            if ((nser == 18) && (fgetnf() == 1))
                                ; // presentation only
                            if ((nser == 18) && (fgetnf() == 2))
                                ; // presentation only
                            if (nser == 19)
                                ; // presentation only
                            index += 1;
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
                            res_ [ispeed][1] = speed;
                            res_ [ispeed][2] = rf + rr;
                            res_ [ispeed][3] = (rf + rr) * fgetke();
                            res_ [ispeed][4] = (rf + rr) * convertedspeed;
                            res_ [ispeed][5] = (rf + rr) * convertedspeed * fgetke();
                            writeLine(ffile2, formatValue(speed, 10, 2), formatValue(rf + rr, 11, 2), formatValue((rf + rr) * fgetke(), 11, 2), formatValue((rf + rr) * convertedspeed, 12, 2), formatValue((rf + rr) * fgetke() * convertedspeed, 12, 2));
                        }
                    resultsmemo.append(space (10) + line);
                    if (ispeed == 8)
                        {
                            tb = (res [8][17 - nn]) * fgetke();
                            if ((tb <= 0))
                                {
                                    resultsmemo.append(QStringLiteral(""));
                                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                                    warning(tr(QStringLiteral("rs_attention____this_is_series_not_correct_for_this_project_")));
                                    resultsmemo.append(QStringLiteral(""));
                                }
                            vms = convertedspeed;
                        }
                }
            }
            closeFile(ffile2);
            resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+--------+--------+--------+---------+--------+---------+"));
            resultsmemo.append(QStringLiteral(""));
            if (fgetnp() > 0)
                {
                    p_d = 0.9;
                    z = 4;
                    k = 0.1;
                    t0 = 0.001;
                    ta = fgetdrafttotal() - fgetdraft() / 2.f;
                    dp_ = fgetdp();
                    if ((fgetdp() == 0) && (fgetnp() == 1))
                        dp_ = 0.7 * ta;
                    if ((fgetdp() == 0) && (fgetnp() == 2))
                        dp_ = 0.65 * ta;
                    if ((fgetdp() > ta))
                        dp_ = 0.75f * ta;
                    if ((nser == 14) || (nser == 15))
                        dp_ = 0.65 * ta;
                    if (fgetnp() == 2)
                        k = 0.2;
                    w = 0.11 + 0.16 * power (cb , fgetnp()) / fgetnp() * sqrtp (power (fgetdisplacement() , 0.33333) / dp_);
                    froudenumber = res [8][4];
                    if (froudenumber > 0.2)
                        w = w - 0.3 * cb * (froudenumber - 0.2);
                    if (fgetnp() == 1)
                        t0 = 0.6 * w * (1.f + 0.67 * w);
                    if (fgetnp() == 2)
                        t0 = 0.8 * w * (1.f + 0.25f * w);
                    npapm = 0;
                    if (tb <= 0)
                        tb = 0.01;
                    kde = vms * dp_ * sqrtp (fgetdensity() / tb * fgetnp());
                    if ((field(QStringLiteral("combobox")) . itemindex == 1) && (cb >= 0.6) && (fgetnp() == 1))
                        {
                            w_tmp = (0.25f + 2.2 * square (cb - 0.5f)) * (0.94 + 1.8 * square (0.8 - dp_ / fgetdrafttotal()));
                            if (w_tmp < 0.45)
                                w = w_tmp;
                            t0 = 0.2 + 0.1 * (cb - 0.5f) + 0.055 * (kde - 1.8);
                            npapm = 1;
                        }
                    if ((field(QStringLiteral("combobox")) . itemindex == 1) && (cb < 0.65) && (fgetnp() == 1) && (dp_ / fgetdrafttotal() <= 0.7))
                        {
                            w = (0.2 + 8 * square (cb - 0.55)) * (0.97 + 7 * square (0.7 - dp_ / fgetdrafttotal()));
                            t0 = 0.18 + 0.35 * (cb - 0.5f) + 0.055 * (kde - 2);
                            npapm = 2;
                        }
                    if ((field(QStringLiteral("combobox")) . itemindex == 1) && (cb < 0.65) && (fgetnp() == 2) && (dp_ / fgetdrafttotal() <= 0.65) && (dp_ / fgetdrafttotal() >= 0.6))
                        {
                            w = 0.09 + 0.14 * (cb - 0.5f);
                            t0 = 0.15 + 0.353 * (cb - 0.5f) + 0.055 * (kde - 2);
                            npapm = 3;
                        }
                    if (field(QStringLiteral("combobox")) . itemindex == 2)
                    if (field(QStringLiteral("combobox")) . itemindex == 3)
                        {
                            if (cwp == 0)
                                cwp = 0.75f;
                            if (fgetnp() == 1)
                                t0 = w * (1.57 + 1.5f * cb - 2.3 * cb / cwp);
                            if (fgetnp() == 2)
                                t0 = w * (1.67 + 1.5f * cb - 2.3 * cb / cwp);
                        }
                    if (field(QStringLiteral("combobox")) . itemindex == 4)
                    if ((field(QStringLiteral("combobox")) . itemindex == 5) && (fgetnp() == 1))
                        {
                            w = (0.17 + square (fgetcp() / cm - 0.6)) * (0.94 + 6 * square (0.7 - dp_ / tc));
                            t0 = 0.19 + 1.6 * square (fgetcp() / cm - 0.6) + 0.1 * (kde - 2.0f);
                            npapm = 10;
                        }
                    tb = tb / fgetnp() / (1 - t0);
                    if (tb <= 0)
                        tb = 0.01;
                    kdt = vms * (1 - w) * dp_ * sqrtp (fgetdensity() / tb);
                    nr = 1;
                    if (kdt >= 2)
                        z = 3;
                    ae_ao = k + (1.3 + 0.3 * z) * tb * 1000.f / (dp_ * dp_ * (99047.f + fgetdensity() * 1000.f * 9.81 * (dp_ / 2.f + ta * 0.2)));
                    ae0 = ae_ao;
                    x = kdt;
                    if ((ae0 > 1.1) && (fgetnp() == 1))
                        resultsmemo.append(space (10) + tr(QStringLiteral("rs_attention____increase_number_of_propellers_or_decrease_estimate_speed_")));
                    if (z == 2)
                        p_d = ((- 0.0030823 * x + 0.0142337) * x + 0.1859161) * x + 0.4598835;
                    if (z == 3)
                        {
                            if (ae0 <= 0.35)
                                {
                                    p_d = ((0.0081576 * x - 0.0083761 * x) + 0.3275126) * x + 0.3668222;
                                    ae_ao = 0.35;
                                }
                            if ((ae0 <= 0.5f) && (ae0 > 0.35))
                                {
                                    p_d = ((- 0.0240385 * x + 0.13499) * x + 0.1599401) * x + 0.498986;
                                    ae_ao = 0.5f;
                                }
                            if ((ae0 <= 0.65) && (ae0 > 0.5f))
                                {
                                    p_d = ((((0.0236147 * x - 0.2234597) * x + 0.8044646) * x - 1.3956004) * x + 1.3848023) * x + 0.3433638;
                                    ae_ao = 0.65;
                                }
                        }
                    if (z == 4)
                        {
                            if (ae0 <= 0.4)
                                {
                                    p_d = ((0.039967 * x - 0.2388861) * x + 0.7556191) * x + 0.2328998;
                                    ae_ao = 0.4;
                                }
                            if ((ae0 <= 0.55) && (ae0 > 0.4))
                                {
                                    p_d = ((0.0353248 * x - 0.2223617) * x + 0.6754278) * x + 0.4570168;
                                    ae_ao = 0.55;
                                }
                            if ((ae0 <= 0.7) && (ae0 > 0.55))
                                {
                                    p_d = ((((0.0074646 * x - 0.0783429) * x + 0.2946838) * x - 0.5441205) * x + 0.8133517) * x + 0.3989677;
                                    ae_ao = 0.7;
                                }
                        }
                    if ((fgetnp() == 0) || (nser == 14) || (nser == 15))
                        {
                            w = 0;
                            t0 = 0;
                            nr = 1;
                        }
                    ffile2.name = safePath(ftempdirectory + QStringLiteral("/") + QStringLiteral("Resistp.dat"));
                    newFile(ffile2);
                    writeLine(ffile2, QStringLiteral("#     Nser      Np       Wt        t       Eta_R     Dp"));
                    writeLine(ffile2, formatValue(nser, 10, 0), formatValue(fgetnp(), 10, 0), formatValue(w, 10, 4), formatValue(t0, 10, 4), formatValue(nr, 10, 4), formatValue(fgetdp(), 10, 4));
                    writeLine(ffile2, QStringLiteral("#      Vs       Rt        Rte       Pe       Pee"));
                    for (i=1; i<=10; ++i)
                    {
                        check();
                        writeLine(ffile2, formatValue(res_ [i][1], 10, 3), formatValue(res_ [i][2], 11, 2), formatValue(res_ [i][3], 11, 2), formatValue(res_ [i][4], 12, 1), formatValue(res_ [i][5], 12, 1));
                    }
                    closeFile(ffile2);
                    if (flag == 1)
                        resultsmemo.append(space (10) + QStringLiteral("Tb        =  ") + number (tb , 0 , 6 , 3) + QStringLiteral(" ") + tr(QStringLiteral("rs_kn")));
                    else
                        resultsmemo.append(space (10) + QStringLiteral("Tb        =  ") + number (tb / flag , 0 , 6 , 0) + QStringLiteral(" ") + tr(QStringLiteral("rs_n")));
                    if ((ae0 < 1.3) && ((nser != 5) || ((nser > 7) && (nser < 14))))
                        {
                            resultsmemo.append(space (10) + QStringLiteral("Kde       =  ") + number (kde , 0 , 6 , 3));
                            resultsmemo.append(space (10) + QStringLiteral("Kdt       =  ") + number (kdt , 0 , 6 , 3));
                            resultsmemo.append(space (10) + QStringLiteral("Dp        =  ") + number (dp_ , 0 , 6 , 3) + QStringLiteral(" ") + lengthstr (units));
                            resultsmemo.append(space (10) + QStringLiteral("Z         =  ") + number (z , 0 , 6 , 0));
                            resultsmemo.append(space (10) + QStringLiteral("Ae/Ao     =  ") + number (ae0 , 0 , 6 , 3) + QStringLiteral("  ") + tr(QStringLiteral("rs___calculated")));
                            resultsmemo.append(space (10) + QStringLiteral("Ae/Ao     =  ") + number (ae_ao , 0 , 6 , 3) + QStringLiteral("  ") + tr(QStringLiteral("rs___for_selection_of_propeller_diagram")));
                            resultsmemo.append(space (10) + QStringLiteral("P/Dp      =  ") + number (p_d , 0 , 6 , 3) + QStringLiteral("  ") + tr(QStringLiteral("rs___by_curve_of_propeller_optimal_frequency")));
                        }
                }
            resultsmemo.append(space (10) + QStringLiteral("Ke        =  ") + number (fgetke() , 0 , 6 , 3));
            if ((fgetnp() > 0) && (nser != 14) && (nser != 15))
                {
                    if (npapm == 0)
                        resultsmemo.append(space (10) + QStringLiteral("Wt        =  ") + number (w , 0 , 6 , 4) + QStringLiteral(" - ") + tr(QStringLiteral("rs_by_papmehl_s_formulae_with_addings_of_ksri__russia_")));
                    else
                        resultsmemo.append(space (10) + QStringLiteral("Wt        =  ") + number (w , 0 , 6 , 4));
                    resultsmemo.append(space (10) + QStringLiteral("t         =  ") + number (t0 , 0 , 6 , 4));
                    resultsmemo.append(space (10) + QStringLiteral("EtaR      =  ") + number (nr , 0 , 6 , 4));
                    resultsmemo.append(space (10) + QStringLiteral("EtaH      =  ") + number ((1 - t0) / (1 - w) , 0 , 6 , 4));
                    resultsmemo.append(space (10) + QStringLiteral("EtaH*EtaR =  ") + number ((1 - t0) / (1 - w) * nr , 0 , 6 , 4));
                }
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + QStringLiteral("Copyright (c) 2007-2012, Timoshenko V.F."));
            ; // presentation only
            field(QStringLiteral("printbutton")) . enabled = true;
        }
}

bool Resistance_ost::execute() {
bool result{};
int units{};
float temper{};
int rightaxis{};
    nwa = 1;
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
    label10old = field(QStringLiteral("label10")) . caption;
    label11old = field(QStringLiteral("label11")) . caption;
    label15old = field(QStringLiteral("label15")) . caption;
    label16old = field(QStringLiteral("label16")) . caption;
    field(QStringLiteral("edit17_1")) . enabled = ! true;
    field(QStringLiteral("edit17_2")) . enabled = ! true;
    field(QStringLiteral("edit17_3")) . enabled = ! true;
    field(QStringLiteral("edit17_4")) . enabled = ! true;
    field(QStringLiteral("edit17_5")) . enabled = ! true;
    field(QStringLiteral("edit18_1")) . enabled = ! true;
    field(QStringLiteral("edit18_2")) . enabled = ! true;
    field(QStringLiteral("edit18_3")) . enabled = ! true;
    field(QStringLiteral("edit18_4")) . enabled = ! true;
    field(QStringLiteral("edit18_5")) . enabled = ! true;
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
    field(QStringLiteral("wlareabox")) . enabled = field(QStringLiteral("estimate2box")) . checked;
    field(QStringLiteral("draftbox")) . enabled = field(QStringLiteral("estimate2box")) . checked;
    if (units == 0)
        field(QStringLiteral("_label36")) . caption = QStringLiteral(" ") + tr(QStringLiteral("rs_m_2_s"));
    else
        field(QStringLiteral("_label36")) . caption = QStringLiteral(" ") + tr(QStringLiteral("rs_ft_2_s"));
    temper = 3.8;
    fsetviscosity(findwaterviscosity (temper , units));
    result = false;
    return result;
}
}
