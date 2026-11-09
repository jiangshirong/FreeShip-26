// GPL-2.0-or-later. Generated from FreeResistance_DelftDlg.pas.
namespace {
const Array<Array<double,0,9>,0,13> matrix1 = {{-6.735654, 38.368310, -0.008193, 0.055234, -1.997242, -38.860810,
    0.956591, -0.002171, 0.272895, -0.017516},
    {-0.382870, 38.172900,
    0.007243, 0.026644, -5.295332, -39.550320, 1.219563, 0.000052, 0.824568, -0.047842},
    {-1.503526, 24.408030,
    0.012200, 0.067221, -2.448582, -31.913700, 2.216098, 0.000074, 0.244345, -0.015887},
    {11.292180, -14.519470,
    0.047182, 0.085176, -2.673016, -11.418190, 5.654065, 0.007021, -0.094934, 0.006325},
    {22.178670, -49.167840,
    0.085998, 0.150725, -2.878684, 7.167049, 8.600272, 0.012981, -0.327085, 0.018271},
    {25.908670, -74.756680,
    0.153521, 0.188568, -0.889467, 24.121370, 10.485160, 0.025348, -0.854940, 0.048449},
    {40.975590, -114.285500,
    0.207226, 0.250827, -3.072662, 53.015700, 13.021770, 0.035943, -0.715457, 0.039874},
    {45.837590, -184.764600,
    0.357031, 0.338343, 3.871658, 132.256800, 10.860540, 0.066809, -1.719215, 0.095977},
    {89.203820, -393.0127,
    0.617466, 0.460472, 11.54327, 331.1197, 8.598136, 0.104073, -2.815203, 0.155960},
    {
    212.678800, -801.790800, 1.087307, 0.538938, 10.802730, 667.644500, 12.398150,
    0.166473, -3.026131, 0.165055},
    {
    336.235400, -1085.134000, 1.644191, 0.532702, -1.224173, 831.144500, 26.183210,
    0.238795, -2.450470, 0.139154},
    {
    566.547600, -1609.632000, 2.016090, 0.265722, -29.244120, 1154.091, 51.46575,
    0.288046, -0.178354, 0.018446},
    {
    743.410700, -1708.263000, 2.435809, 0.013553, -81.161890, 937.401400,
    115.600600, 0.365071, 1.838967, -0.062023},
    {1200.62, -2751.715,
    3.208577, 0.254920, -132.0424, 1489.269, 196.3406, 0.528225, 1.379102, 0.013577}};
const Array<Array<double,0,5>,0,11> matrix2 = {{180.100400, -31.502570, -7.451141, 2.195042, 2.689623, 0.006480},
    {243.999400, -44.525510,
    -11.154560, 2.179046, 3.857403, 0.009676},
    {282.987300, -51.519530,
    -12.973100, 2.274505, 4.343662, 0.011066},
    {313.410900, -56.582570,
    -14.419780, 2.326117, 4.690432, 0.012147},
    {337.003800, -59.190290,
    -16.069750, 2.419156, 4.766793, 0.014147},
    {356.457200, -62.853950,
    -16.851120, 2.437056, 5.078768, 0.014980},
    {324.735700, -51.312520,
    -15.345950, 2.334146, 3.855368, 0.013695},
    {301.126800, -39.796310,
    -15.022990, 2.059657, 2.545676, 0.013588},
    {292.057100, -31.853030,
    -15.585480, 1.847926, 1.569917, 0.014014},
    {284.464100, -25.145580,
    -16.154230, 1.703981, 0.817921, 0.014575},
    {256.636700, -19.319220,
    -13.084500, 2.152824, 0.348305, 0.011343},
    {304.180300, -30.115120,
    -15.854290, 2.863173, 1.524379, 0.014031}};
const Array<Array<double,0,9>,0,13> matrix3 = {{-6.735654, 38.368310, -0.008193, 0.055234, -1.997242, -38.860810,
    0.956591, -0.002171, 0.272895, -0.017516},
    {-0.382870, 38.172900,
    0.007243, 0.026644, -5.295332, -39.550320, 1.219563, 0.000052, 0.824568, -0.047842},
    {-1.503526, 24.408030,
    0.012200, 0.067221, -2.448582, -31.913700, 2.216098, 0.000074, 0.244345, -0.015887},
    {11.292180, -14.519470,
    0.047182, 0.085176, -2.673016, -11.418190, 5.654065, 0.007021, -0.094934, 0.006325},
    {22.178670, -49.167840,
    0.085998, 0.150725, -2.878684, 7.167049, 8.600272, 0.012981, -0.327085, 0.018271},
    {25.908670, -74.756680,
    0.153521, 0.188568, -0.889467, 24.121370, 10.485160, 0.025348, -0.854940, 0.048449},
    {40.975590, -114.285500,
    0.207226, 0.250827, -3.072662, 53.015700, 13.021770, 0.035943, -0.715457, 0.039874},
    {45.837590, -184.764600,
    0.357031, 0.338343, 3.871658, 132.256800, 10.860540, 0.066809, -1.719215, 0.095977},
    {89.203820, -393.0127,
    0.617466, 0.460472, 11.54327, 331.1197, 8.598136, 0.104073, -2.815203, 0.155960},
    {
    212.678800, -801.790800, 1.087307, 0.538938, 10.802730, 667.644500, 12.398150,
    0.166473, -3.026131, 0.165055},
    {
    336.235400, -1085.134000, 1.644191, 0.532702, -1.224173, 831.144500, 26.183210,
    0.238795, -2.450470, 0.139154},
    {
    566.547600, -1609.632000, 2.016090, 0.265722, -29.244120, 1154.091, 51.46575,
    0.288046, -0.178354, 0.018446},
    {
    743.410700, -1708.263000, 2.435809, 0.013553, -81.161890, 937.401400,
    115.600600, 0.365071, 1.838967, -0.062023},
    {1200.62, -2751.715,
    3.208577, 0.254920, -132.0424, 1489.269, 196.3406, 0.528225, 1.379102, 0.013577}};
struct Resistance_delft final: ResistanceBase {
using ResistanceBase::ResistanceBase;
float b_t{};
float l_d{};
float a_d{};
float l_b{};
float am{};
float cm{};
float cwp{};
float cb{};
float xwlmin{};
float lcb_{};
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
void calculateresistance(float convertedspeed, float lcb, float cp, float &rf, float &rr, float &rt);
float optimumcp(float speed);
float optimumlcb(float speed);
float optimumcph(float speed);
float optimumlcbh(float speed);
void checkbox2click(void* sender);
void drafttotalboxaftersetvalue(void* sender);
void estimateboxclick(void* sender);
void calculateresistanceholtr(float convertedspeed, float lcb, float cp, float &rf, float &rr, float &w, float &t0, float &nr);
void resist(float vms, Span<float> dat, float &rf, float &rr, float &w, float &t0, float &nr);
void calculate();
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
void booleanChanged(const QString &key) override { if (key==QStringLiteral("estimatebox")) estimateboxclick(nullptr); }
void extract() override { field("checkbox2").checked=true;checkbox2click(nullptr);field("checkbox2").checked=false;checkbox2click(nullptr); }
void initialise() override { execute(); }
void choiceChanged(const QString &key) override {  }
void runCalculation() override { calculate(); }
};
bool Resistance_delft::correctinputdata() {
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
    if ((fgetdraft() <= 0.02) || (fgetlwl() <= 0) || (fgetbwl() <= 0) || (fgetdrafttotal() < fgetdraft()) || (fgetwettedsurface() <= 0) || (fgetwlarea() <= 0) || (fgetdisplacement() <= 0) || (fgetcp() <= 0) || (fgetviscosity() <= 0) || (fgetendspeed() <= fgetstartspeed()) || (fgetstepspeed() <= 0))
        return result;
    b_t = fgetbwl() / fgetdraft();
    l_d = fgetlwl() / power (fgetdisplacement() , 1.0 / 3);
    a_d = fgetwlarea() / power (fgetdisplacement() , 2.0 / 3);
    l_b = fgetlwl() / fgetbwl();
    if (fgetdrafttotal() > fgetdraft())
        {
            cb = fgetdisplacement() / (fgetlwl() * fgetbwl() * fgetdraft());
            cm = cb / fgetcp();
            am = cm * fgetdraft() * fgetbwl();
        }
    else
        {
            cm = fgetcp() / fgetdisplacement() * (fgetlwl() * fgetbwl() * fgetdraft());
            am = cm * fgetdraft() * fgetbwl();
        }
    if (cm >= 1.f)
        {
            am = fgetdisplacement() / (fgetlwl() * fgetcp());
            cm = am / (fgetbwl() * fgetdraft());
        }
    cwp = fgetwlarea() / (fgetlwl() * fgetbwl());
    if ((cwp > 0.85) || (fgetcp() > 0.85))
        {
            warning(tr(QStringLiteral("rsloccplimit")));
            return result;
        }
    result = true;
    return result;
}

float Resistance_delft::fgetstartspeed() {
float result{};

    result = field(QStringLiteral("startspeedbox")) . value;
    return result;
}

void Resistance_delft::fsetstartspeed(float val) {

    field(QStringLiteral("startspeedbox")).set(val);
}

float Resistance_delft::fgetendspeed() {
float result{};

    result = field(QStringLiteral("endspeedbox")) . value;
    return result;
}

void Resistance_delft::fsetendspeed(float val) {

    field(QStringLiteral("endspeedbox")).set(val);
}

float Resistance_delft::fgetdensity() {
float result{};

    result = field(QStringLiteral("densitybox")) . value;
    return result;
}

void Resistance_delft::fsetdensity(float val) {

    field(QStringLiteral("densitybox")).set(val);
}

float Resistance_delft::fgetdisplacement() {
float result{};

    result = field(QStringLiteral("displacementbox")) . value;
    return result;
}

void Resistance_delft::fsetdisplacement(float val) {

    field(QStringLiteral("displacementbox")).set(val);
}

float Resistance_delft::fgetdraft() {
float result{};

    result = field(QStringLiteral("draftbox")) . value;
    return result;
}

void Resistance_delft::fsetdraft(float val) {

    field(QStringLiteral("draftbox")).set(val);
}

float Resistance_delft::fgetdrafttotal() {
float result{};

    result = field(QStringLiteral("drafttotalbox")) . value;
    return result;
}

void Resistance_delft::fsetdrafttotal(float val) {

    field(QStringLiteral("drafttotalbox")).set(val);
}

float Resistance_delft::fgetlwl() {
float result{};

    result = field(QStringLiteral("lwlbox")) . value;
    return result;
}

void Resistance_delft::fsetlwl(float val) {

    field(QStringLiteral("lwlbox")).set(val);
}

float Resistance_delft::fgetlcb() {
float result{};

    result = field(QStringLiteral("lcbbox")) . value;
    return result;
}

void Resistance_delft::fsetlcb(float val) {

    field(QStringLiteral("lcbbox")).set(val);
}

float Resistance_delft::fgetkeelchordlength() {
float result{};

    result = field(QStringLiteral("keelchordlengthbox")) . value;
    return result;
}

void Resistance_delft::fsetkeelchordlength(float val) {

    field(QStringLiteral("keelchordlengthbox")).set(val);
}

float Resistance_delft::fgetkeelarea() {
float result{};

    result = field(QStringLiteral("keelareabox")) . value;
    return result;
}

void Resistance_delft::fsetkeelarea(float val) {

    field(QStringLiteral("keelareabox")).set(val);
}

float Resistance_delft::fgetrudderchordlength() {
float result{};

    result = field(QStringLiteral("rudderchordlengthbox")) . value;
    return result;
}

void Resistance_delft::fsetrudderchordlength(float val) {

    field(QStringLiteral("rudderchordlengthbox")).set(val);
}

float Resistance_delft::fgetrudderarea() {
float result{};

    result = field(QStringLiteral("rudderareabox")) . value;
    return result;
}

void Resistance_delft::fsetrudderarea(float val) {

    field(QStringLiteral("rudderareabox")).set(val);
}

float Resistance_delft::fgetbwl() {
float result{};

    result = field(QStringLiteral("bwlbox")) . value;
    return result;
}

void Resistance_delft::fsetbwl(float val) {

    field(QStringLiteral("bwlbox")).set(val);
}

float Resistance_delft::fgetcp() {
float result{};

    result = field(QStringLiteral("cpbox")) . value;
    return result;
}

void Resistance_delft::fsetcp(float val) {

    field(QStringLiteral("cpbox")).set(val);
}

float Resistance_delft::fgetviscosity() {
float result{};

    result = field(QStringLiteral("viscositybox")) . value;
    return result;
}

void Resistance_delft::fsetviscosity(float val) {

    field(QStringLiteral("viscositybox")).set(val);
}

float Resistance_delft::fgetwettedsurface() {
float result{};
float c23{};
float scbfact{};
float am{};
float cm{};
float cwp{};
    if (field(QStringLiteral("estimatebox")) . checked)
        {
            if ((fgetdraft() > 0) && (fgetlwl() > 0) && (fgetbwl() > 0) && (fgetcp() > 0) && (fgetdisplacement() > 0))
                {
                    am = fgetdisplacement() / (fgetlwl() * fgetcp());
                    cm = am / (fgetbwl() * fgetdraft());
                    cwp = fgetwlarea() / (fgetlwl() * fgetbwl());
                    c23 = 0.453 + 0.443 * (fgetcp() * cm) - 0.286 * cm - 0.00347 * (fgetbwl() / fgetdraft()) + 0.37 * cwp;
                    scbfact = 0.616 * c23 + 0.111 * cm * cm * cm + 0.245 * (c23 / cm) - 0.0228;
                    result = scbfact * fgetlwl() * (2 * fgetdraft() + fgetbwl()) * sqrtp (cm);
                }
            else
                result = 0;
        }
    else
        result = field(QStringLiteral("wettedsurfacebox")) . value;
    return result;
}

void Resistance_delft::fsetwettedsurface(float val) {

    field(QStringLiteral("wettedsurfacebox")).set(val);
}

float Resistance_delft::fgetwlarea() {
float result{};

    result = field(QStringLiteral("wlareabox")) . value;
    return result;
}

void Resistance_delft::fsetwlarea(float val) {

    field(QStringLiteral("wlareabox")).set(val);
}

float Resistance_delft::fgetstepspeed() {
float result{};

    result = field(QStringLiteral("stepspeedbox")) . value;
    return result;
}

void Resistance_delft::fsetstepspeed(float val) {

    field(QStringLiteral("stepspeedbox")).set(val);
}

bool Resistance_delft::fgetextractfromhull() {
bool result{};

    result = field(QStringLiteral("checkbox2")) . checked;
    return result;
}

void Resistance_delft::fsetextractfromhull(bool val) {

    if (field(QStringLiteral("checkbox2")) . checked != val)
        field(QStringLiteral("checkbox2")) . checked = val;
}

void Resistance_delft::calculateresistance(float convertedspeed, float lcb, float cp, float &rf, float &rr, float &rt) {
float rn_hull{};
float cf_hull{};
float rf_hull{};
float rn_keel{};
float cf_keel{};
float rf_keel{};
float rn_rudder{};
float cf_rudder{};
float rf_rudder{};
float fraction{};
float froudenumber{};
Array<double,0,9> factors{};
int a{};
int lower{};
float keel_chord{};
float keel_area{};
float rudder_chord{};
float rudder_area{};
float lengthwaterline{};
float waterdensity{};
float waterviscosity{};
float displ{};
float wetarea{};
    rf = 0.0f;
    rr = 0.0f;
    rt = 0.0f;
    if (convertedspeed <= 0)
        return;
    if (settings. projectunits == 1)
        {
            lengthwaterline = fgetlwl() * foot;
            waterdensity = fgetdensity() / weightconversionfactor;
            keel_chord = fgetkeelchordlength() * foot;
            keel_area = fgetkeelarea() * foot * foot;
            rudder_chord = fgetrudderchordlength() * foot;
            rudder_area = fgetrudderarea() * foot * foot;
            waterviscosity = fgetviscosity() * 0.3048 * 0.3048 * 1e-6;
            displ = fgetdisplacement() * foot * foot * foot;
            wetarea = fgetwettedsurface() * foot * foot;
        }
    else
        {
            lengthwaterline = fgetlwl();
            waterdensity = fgetdensity();
            keel_chord = fgetkeelchordlength();
            keel_area = fgetkeelarea();
            rudder_chord = fgetrudderchordlength();
            rudder_area = fgetrudderarea();
            waterviscosity = fgetviscosity() * 1e-6;
            displ = fgetdisplacement();
            wetarea = fgetwettedsurface();
        }
    froudenumber = convertedspeed / sqrtp (9.81 * lengthwaterline);
    rn_hull = (convertedspeed * 0.7 * lengthwaterline) / waterviscosity;
    cf_hull = 0.075 / square (logp (rn_hull) - 2);
    rf_hull = cf_hull * 0.5f * 1000 * waterdensity * square (convertedspeed) * wetarea;
    if ((keel_chord > 0) && (keel_area > 0))
        {
            rn_keel = (convertedspeed * keel_chord) / waterviscosity;
            cf_keel = 0.075 / square (logp (rn_keel) - 2);
            rf_keel = cf_keel * 0.5f * 1000 * waterdensity * square (convertedspeed) * keel_area;
        }
    else
        rf_keel = 0.0f;
    if ((rudder_chord > 0) && (rudder_area > 0))
        {
            rn_rudder = (convertedspeed * rudder_chord) / waterviscosity;
            cf_rudder = 0.075 / square (logp (rn_rudder) - 2);
            rf_rudder = cf_rudder * 0.5f * 1000 * waterdensity * square (convertedspeed) * rudder_area;
        }
    else
        rf_rudder = 0.0f;
    rf = rf_hull + rf_keel + rf_rudder;
    rr = 0;
    if (froudenumber <= 0.45)
        {
            if (froudenumber < 0.125f)
                {
                    fraction = froudenumber / 0.125f;
                    for (a=0; a<=9; ++a)
                    {
                        check();
                        factors [a] = fraction * (matrix1 [0][a]);
                    }
                }
            else
                {
                    lower = truncp ((froudenumber - 0.125f) / 0.025);
                    fraction = (froudenumber - (0.125f + lower * 0.025)) / 0.025;
                    for (a=0; a<=9; ++a)
                    {
                        check();
                        factors [a] = matrix1 [lower][a] + fraction * (matrix1 [lower + 1][a] - matrix1 [lower][a]);
                    }
                }
            rr = 9.81 * displ * waterdensity * (factors [0] + factors [1] * cp + factors [2] * lcb + factors [3] * b_t + factors [4] * l_d + factors [5] * cp * cp + factors [6] * cp * l_d + factors [7] * lcb * lcb + factors [8] * l_d * l_d + factors [9] * l_d * l_d * l_d);
        }
    else
        if ((froudenumber > 0.475) && (froudenumber <= 0.75f))
            {
                lower = truncp ((froudenumber - 0.475) / 0.025);
                fraction = (froudenumber - (0.475 + lower * 0.025)) / 0.025;
                for (a=0; a<=5; ++a)
                {
                    check();
                    factors [a] = matrix2 [lower][a] + fraction * (matrix2 [lower + 1][a] - matrix2 [lower][a]);
                }
                rr = displ * waterdensity * (factors [0] + factors [1] * l_b + factors [2] * a_d + factors [3] * lcb + factors [4] * l_b * l_b + factors [5] * l_b * a_d * a_d * a_d);
            }
    rt = rf + rr;
}

float Resistance_delft::optimumcp(float speed) {
float result{};
float tmpcp{};
float rmin{};
float rf{};
float rr{};
float rt{};
float optimum{};
    result = 0;
    rmin = std::numeric_limits<float>::infinity();
    tmpcp = 0.3;
    optimum = fgetcp();
    while (tmpcp <= 0.9) {
        check();
        {
            calculateresistance (speed , fgetlcb() , tmpcp , rf , rr , rt);
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

float Resistance_delft::optimumlcb(float speed) {
float result{};
float tmplcb{};
float rmin{};
float rf{};
float rr{};
float rt{};
float optimum{};
    result = 0;
    rmin = std::numeric_limits<float>::infinity();
    tmplcb = - 6;
    optimum = tmplcb;
    while (tmplcb <= 0) {
        check();
        {
            calculateresistance (speed , tmplcb , fgetcp() , rf , rr , rt);
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

float Resistance_delft::optimumcph(float speed) {
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
    rmin = std::numeric_limits<float>::infinity();
    tmpcp = 0.85;
    optimum = fgetcp();
    while (tmpcp >= 0.3) {
        check();
        {
            calculateresistanceholtr (speed , fgetlcb() , tmpcp , rf , rr , w , t0 , nr);
            rt = rf + rr;
            if ((rt < rmin) && (rt > 0))
                {
                    rmin = rt;
                    optimum = tmpcp;
                }
            tmpcp = tmpcp - 0.01;
        }
    }
    result = optimum;
    resultsmemo.append(space (10) + QStringLiteral("R_T_min = ") + number (rmin , 0 , 6 , 4) + QStringLiteral(" ") + tr(QStringLiteral("rs_kn")) + QStringLiteral(" ") + tr(QStringLiteral("rs_for")) + QStringLiteral(" Cp_opt  = ") + number (optimum , 0 , 6 , 4));
    return result;
}

float Resistance_delft::optimumlcbh(float speed) {
float result{};
float tmplcb{};
float rmin{};
float optimum{};
float lcb_n{};
float rf{};
float rr{};
float rt{};
float w{};
float t0{};
float nr{};
    result = 0;
    rmin = std::numeric_limits<float>::infinity();
    tmplcb = - 6;
    optimum = tmplcb;
    while (tmplcb <= 0) {
        check();
        {
            calculateresistanceholtr (speed , tmplcb , fgetcp() , rf , rr , w , t0 , nr);
            rt = rf + rr;
            if ((rt < rmin) && (rt > 0))
                {
                    rmin = rt;
                    optimum = tmplcb;
                }
            tmplcb = tmplcb + 0.01;
        }
    }
    result = optimum;
    lcb_n = optimum * fgetlwl() / 100 + (xwlmin + 0.5f * fgetlwl());
    resultsmemo.append(space (10) + QStringLiteral("R_T_min = ") + number (rmin , 0 , 6 , 4) + QStringLiteral(" ") + tr(QStringLiteral("rs_kn")) + QStringLiteral(" ") + tr(QStringLiteral("rs_for")) + QStringLiteral(" Lcb_opt = ") + number (optimum , 0 , 6 , 3) + QStringLiteral(" % Or Lcb_opt =") + number (lcb_n , 0 , 6 , 3) + QStringLiteral(" m"));
    return result;
}

void Resistance_delft::checkbox2click(void* sender) {

    field(QStringLiteral("lwlbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("bwlbox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("wettedsurfacebox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
    field(QStringLiteral("wlareabox")) . enabled = ! field(QStringLiteral("checkbox2")) . checked;
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

void Resistance_delft::drafttotalboxaftersetvalue(void* sender) {
HydroInput hydobject{};
    if ((field(QStringLiteral("checkbox2")) . checked) && (this != nullptr))
        {
            ; // local immutable hydrostatic request
            hydobject . draft = fgetdrafttotal();
            hydobject.data = hydro(hydobject.draft);
            fsetlwl(hydValue(hydobject, QStringLiteral("lengthwaterline")));
            fsetbwl(hydValue(hydobject, QStringLiteral("beamwaterline")));
            fsetwettedsurface(hydValue(hydobject, QStringLiteral("wettedsurface")));
            fsetwlarea(hydValue(hydobject, QStringLiteral("waterplanearea")));
            fsetdisplacement(hydValue(hydobject, QStringLiteral("volume")));
            lcb_ = hydValue(hydobject, QStringLiteral("centerofbuoyancy.x"));
            xwlmin = hydValue(hydobject, QStringLiteral("wlmin.x"));
            if (fgetlwl() != 0)
                fsetlcb(100 * (hydValue(hydobject, QStringLiteral("centerofbuoyancy.x")) - (hydValue(hydobject, QStringLiteral("wlmin.x")) + 0.5f * fgetlwl())) / fgetlwl());
            else
                fsetlcb(0);
            fsetcp(hydValue(hydobject, QStringLiteral("prismcoefficient")));
            cb = hydValue(hydobject, QStringLiteral("blockcoefficient"));
            cm = hydValue(hydobject, QStringLiteral("midshipcoeff"));
            check();
        }
}

void Resistance_delft::estimateboxclick(void* sender) {

    field(QStringLiteral("wettedsurfacebox")) . enabled = ! field(QStringLiteral("estimatebox")) . checked;
}

void Resistance_delft::calculateresistanceholtr(float convertedspeed, float lcb, float cp, float &rf, float &rr, float &w, float &t0, float &nr) {
float rn_keel{};
float cf_keel{};
float rf_keel{};
float rn_rudder{};
float cf_rudder{};
float rf_rudder{};
Array<float,1,22> dat{};
float keel_chord{};
float keel_area{};
float rudder_chord{};
float rudder_area{};
float lengthwaterline{};
float bwaterline{};
float drafttot{};
float waterdensity{};
float waterviscosity{};
float displ{};
float wetarea{};
    rf = 0.0f;
    rr = 0.0f;
    if (convertedspeed <= 0)
        return;
    if (settings. projectunits == 1)
        {
            lengthwaterline = fgetlwl() * foot;
            bwaterline = fgetbwl() * foot;
            if (fgetdrafttotal() - fgetdraft() == 0)
                drafttot = fgetdrafttotal() * foot;
            else
                drafttot = (fgetdrafttotal() - fgetdraft()) * foot;
            waterdensity = fgetdensity() / weightconversionfactor;
            keel_chord = fgetkeelchordlength() * foot;
            keel_area = fgetkeelarea() * foot * foot;
            rudder_chord = fgetrudderchordlength() * foot;
            rudder_area = fgetrudderarea() * foot * foot;
            waterviscosity = fgetviscosity() * 0.3048 * 0.3048 * 1e-6;
            displ = fgetdisplacement() * foot * foot * foot;
            wetarea = fgetwettedsurface() * foot * foot;
        }
    else
        {
            lengthwaterline = fgetlwl();
            bwaterline = fgetbwl();
            if (fgetdrafttotal() - fgetdraft() == 0)
                drafttot = fgetdrafttotal();
            else
                drafttot = fgetdrafttotal() - fgetdraft();
            waterdensity = fgetdensity();
            keel_chord = fgetkeelchordlength();
            keel_area = fgetkeelarea();
            rudder_chord = fgetrudderchordlength();
            rudder_area = fgetrudderarea();
            waterviscosity = fgetviscosity() * 1e-6;
            displ = fgetdisplacement();
            wetarea = fgetwettedsurface();
        }
    dat [1] = lengthwaterline;
    dat [2] = lengthwaterline;
    dat [3] = bwaterline;
    dat [4] = drafttot;
    dat [5] = drafttot;
    dat [6] = drafttot;
    dat [7] = displ;
    dat [8] = wetarea;
    dat [9] = lcb;
    dat [10] = cp * cm;
    dat [11] = cp;
    dat [12] = cwp;
    dat [13] = cm;
    dat [14] = 0.0f;
    dat [15] = 0.001;
    dat [16] = 0.0f;
    dat [17] = waterdensity;
    dat [18] = waterviscosity;
    dat [19] = 0;
    dat [20] = 1;
    dat [21] = 1.15;
    dat [22] = 120;
    resist (convertedspeed , dat , rf , rr , w , t0 , nr);
    if ((keel_chord > 0) && (keel_area > 0))
        {
            rn_keel = (convertedspeed * keel_chord) / waterviscosity;
            cf_keel = 0.075 / square (logp (rn_keel) - 2);
            rf_keel = cf_keel * 0.5f * waterdensity * square (convertedspeed) * keel_area;
        }
    else
        rf_keel = 0.0f;
    if ((rudder_chord > 0) && (rudder_area > 0))
        {
            rn_rudder = (convertedspeed * rudder_chord) / waterviscosity;
            cf_rudder = 0.075 / square (logp (rn_rudder) - 2);
            rf_rudder = cf_rudder * 0.5f * waterdensity * square (convertedspeed) * rudder_area;
        }
    else
        rf_rudder = 0.0f;
    rf = rf + rf_keel + rf_rudder;
}

void Resistance_delft::resist(float vms, Span<float> dat, float &rf, float &rr, float &w, float &t0, float &nr) {
const Array<double,1,11> appc = {1.75, 1.4, 2.8, 3.0, 1.75, 3.0, 2.0, 3.0, 2.8, 2.7, 1.4};
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
float ie{};
float hb{};
float at{};
float lamb{};
float cstrn{};
float k1{};
float k2{};
float ke{};
float ae0{};
float l_b{};
float b_t{};
float b_l{};
float l3v{};
float b_ta{};
float ta_d{};
float tf_l{};
float t_l{};
float t_b{};
Array<float,1,4> m{};
Array<float,1,20> c{};
Array<float,1,11> app{};
int i{};
float np{};
float ro{};
float nu{};
float lpp{};
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
float rapp{};
    lpp = dat [0];
    l = dat [1];
    b = dat [2];
    tf = dat [3];
    ta = dat [4];
    t = (tf + ta) / 2.f;
    v = dat [6];
    s = dat [7];
    fsetlcb(dat [8]);
    cb = dat [9];
    cp = dat [10];
    cwp = dat [11];
    cm = dat [12];
    abt = dat [13];
    cbt = dat [14];
    hb = dat [15];
    ro = dat [16];
    nu = dat [17];
    cstrn = dat [18];
    np = dat [19];
    ke = dat [20];
    ks = dat [21];
    at = 0;
    d = 0.7 * ta;
    if (np == 1)
        d = 0.8 * ta;
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
    if (lr < 0.2)
        {
            rr = std::numeric_limits<float>::infinity();
            resultsmemo.append(QStringLiteral("Lr = ") + number (lr , 0 , 6 , 4) + QStringLiteral(" < 0,2"));
            return;
        }
    c [17] = 6919.3 * power (cm , - 1.3346) * power ((v / l / l / l) , 2.00977) * power ((l_b - 2) , 1.40692);
    m [3] = - 7.2035 * power (b_l , 0.326869) * power (t_b , 0.605375);
    ie = 1.f + 89.f * expp (- power (l_b , 0.80856) * power ((1 - cwp) , 0.30484) * power ((1 - cp - 0.0225 * fgetlcb()) , 0.6367) * power ((lr / b) , 0.34574) * power ((100 * v / l / l / l) , 0.16302));
    c [3] = 0.56 * power (abt , 1.5f) / (b * t * (0.31 * sqrtp (abt) + tf - hb));
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
    if (np == 1)
        {
            ae0 = 0.55;
            t0 = 0.25014 * power (b_l , 0.28956) * power ((sqrtp (b * t) / d) , 0.2624) / power ((1 - cp + 0.0225 * fgetlcb()) , 0.01762) + 0.0015 * cstrn;
            nr = 0.9922 - 0.05908 * ae0 + 0.07424 * (cp - 0.0225 * fgetlcb());
        }
    else
        {
            t0 = 0.325 * cb - 0.1885 * d / sqrtp (b * t);
            nr = 0.9737 + 0.111 * (cp - 0.0225 * fgetlcb()) - 0.06325;
        }
    if (tf_l <= 0.04)
        c [4] = tf_l;
    else
        c [4] = 0.04;
    ca = 0.006 * power ((l + 100) , (- 0.16)) - 0.00205 + 0.003 * sqrtp (l / 7.5f) * square (square (cb)) * c [2] * (0.04 - c [4]);
    if (ks >= 150.f)
        ca = ca + (0.105 * power (ks / 1000000.f , 0.333333) - 0.005579) / power (l , 0.33333);
    m [1] = 0.0140407 * l / t - 1.75254 * power (v , 0.33333) / l - 4.79323 * b_l - c [16];
    fn = vms / sqrtp (9.81 * l);
    m [4] = c [15] * 0.4 * expp ((- 0.034) * power (fn , - 3.29));
    re = vms * l / nu;
    cf = 0.075 / square (logp (re) - 2);
    rf = cf * s * k1 * ro * vms * vms / 2;
    if (fn > 0.65)
        return;
    rwa = c [1] * c [2] * c [5] * 10.05525 * v * expp (m [1] * power (fn , - 0.9) + m [4] * cosp (lamb / fn / fn));
    rwb = c [17] * c [2] * c [5] * 10.05525 * v * expp (m [3] * power (fn , - 0.9) + m [4] * cosp (lamb / fn / fn));
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
    fni = vms / sqrtp (9.81 * (tf - hb - 0.25f * sqrtp (abt)) + 0.15 * vms * vms);
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
            if (np == 1)
                w = c [9] * c [20] * cv * l / ta * (0.050776 + 0.93405 * c [11] * cv / (1 - cp1)) + 0.27915 * c [20] * sqrtp (b / (l * (1 - cp1))) + c [19] * c [20];
            else
                w = 0.3095 * cb + 10 * cv * cb - 0.23 * d / sqrtp (b * t);
        }
    rapp = 0;
    rr = rb + rtr + ra + rw + rapp;
}

void Resistance_delft::calculate() {
float convertedspeed{};
float froudenumber{};
float rf{};
float rr{};
float rt{};
float flag{};
float w{};
float t0{};
float nr{};
int ii{};
std::vector<float> cpopt{};
std::vector<float> lcbopt{};
float speed{};
float lcb_n{};
QString line{};
QString tmp{};
int units{};
bool stop{};
int index{};
int rightaxis{};
    clearSeries(QStringLiteral("series1"));
    clearSeries(QStringLiteral("series2"));
    clearSeries(QStringLiteral("series3"));
    clearSeries(QStringLiteral("series4"));
    field(QStringLiteral("printbutton")) . enabled = false;
    ; // presentation only
    if (correctinputdata())
        {
            lcb_n = fgetlcb();
            units = settings. projectunits;
            resultsmemo.clear();
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            header (space (10) + tr(QStringLiteral("rs_yachtres__resistance_prediction_for_sailing_yachts__according_to_prof__gerritsma")) + QStringLiteral(".") , resultsmemo);
            resultsmemo.append(QStringLiteral("          ------------------------------------------------------------------------"));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_input_variables")));
            resultsmemo.append(QStringLiteral("          ------------------------------------------------------------------------"));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_general")));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_start_speed")) , 30) + QStringLiteral(" : ") + number (fgetstartspeed() , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_knots")));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_end_speed")) , 30) + QStringLiteral(" : ") + number (fgetendspeed() , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_knots")));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_speed_step")) , 30) + QStringLiteral(" : ") + number (fgetstepspeed() , 0 , 6 , 2) + QStringLiteral(" ") + tr(QStringLiteral("rs_knots")));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_water_density")) , 30) + QStringLiteral(" : ") + number (fgetdensity() , 0 , 8 , 4) + QStringLiteral(" ") + densitystr (units));
            if (units == 1)
                resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_water_viscosity")) , 30) + QStringLiteral(" : ") + number (fgetviscosity() , 0 , 8 , 4) + QStringLiteral("*10^(-6) ") + tr(QStringLiteral("rs_ft_2_s")));
            else
                resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_water_viscosity")) , 30) + QStringLiteral(" : ") + number (fgetviscosity() , 0 , 8 , 4) + QStringLiteral("*10^(-6) ") + tr(QStringLiteral("rs_m_2_s")));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_hull")));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_length_on_waterline")) , 30) + QStringLiteral(" : ") + number (fgetlwl() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_beam_on_waterline")) , 30) + QStringLiteral(" : ") + number (fgetbwl() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_draft_hull")) , 30) + QStringLiteral(" : ") + number (fgetdraft() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_total_draft")) , 30) + QStringLiteral(" : ") + number (fgetdrafttotal() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            if (field(QStringLiteral("estimatebox")) . checked)
                resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_wetted_surface_area")) , 30) + QStringLiteral(" : ") + number (fgetwettedsurface() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units) + QStringLiteral(" (") + tr(QStringLiteral("rs_estimated")) + QStringLiteral(")"));
            else
                resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_wetted_surface_area")) , 30) + QStringLiteral(" : ") + number (fgetwettedsurface() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_waterplane_area")) , 30) + QStringLiteral(" : ") + number (fgetwlarea() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_displacement")) , 30) + QStringLiteral(" : ") + number (fgetdisplacement() , 0 , 6 , 3) + QString(QChar(32)) + volstr (units));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_longitudinal_center_of_buoyancy")) , 30) + QStringLiteral(" : ") + number (fgetlcb() , 0 , 6 , 3) + QStringLiteral(" %"));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_prismatic_coefficient")) , 30) + QStringLiteral(" : ") + number (fgetcp() , 0 , 6 , 4));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_keel")));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_average_chordlength")) , 30) + QStringLiteral(" : ") + number (fgetkeelchordlength() , 0 , 6 , 3) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_wetted_area")) , 30) + QStringLiteral(" : ") + number (fgetkeelarea() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_rudder")));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_average_chordlength")) , 30) + QStringLiteral(" : ") + number (fgetrudderchordlength() , 0 , 6 , 2) + QString(QChar(32)) + lengthstr (units));
            resultsmemo.append(space (10) + pad (tr(QStringLiteral("rs_wetted_area")) , 30) + QStringLiteral(" : ") + number (fgetrudderarea() , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + tr(QStringLiteral("rs_calculated_variables")));
            resultsmemo.append(QStringLiteral("          ---------------------------------------------------------------------------------"));
            resultsmemo.append(QStringLiteral(""));
            resultsmemo.append(space (10) + QStringLiteral("Cb         = ") + number (cb , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("Cwp        = ") + number (cwp , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("Cm         = ") + number (cm , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("Cp         = ") + number (fgetcp() , 0 , 6 , 4));
            resultsmemo.append(space (10) + QStringLiteral("LCB        = ") + number (fgetlcb() , 0 , 6 , 4) + QStringLiteral(" %"));
            resultsmemo.append(space (10) + QStringLiteral("Am         = ") + number (am , 0 , 6 , 2) + QString(QChar(32)) + areastr (units));
            resultsmemo.append(space (10) + QStringLiteral("Lwl/Bwl    = ") + number (fgetlwl() / fgetbwl() , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Bwl/Tc     = ") + number (fgetbwl() / fgetdraft() , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Lwl/V^3    = ") + number (fgetlwl() / power (fgetdisplacement() , 1.0 / 3) , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Tc/T       = ") + number (fgetdraft() / fgetdrafttotal() , 0 , 6 , 3));
            resultsmemo.append(space (10) + QStringLiteral("Aw/V^0.667 = ") + number (fgetwlarea() / power (fgetdisplacement() , 2.0 / 3) , 0 , 7 , 3));
            resultsmemo.append(QStringLiteral(""));
            for (ii=1; ii<=12; ++ii)
            {
                check();
                resultsmemo.append(QStringLiteral(""));
            }
            if (fgetstepspeed() == 0.0f)
                fsetstepspeed(0.1);
            stop = false;
            if ((l_b < 2.76) || (l_b > 5.00f))
                {
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_lwl_bwl_is_outside_valid_domain")) + QStringLiteral(" [2.76 ... 5.00]"));
                    stop = true;
                }
            if ((b_t < 2.46) || (b_t > 19.32))
                {
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_bwl_tc_is_outside_valid_domain")) + QStringLiteral(" [2.46 ... 19.32]"));
                    stop = true;
                }
            if ((l_d < 4.34) || (l_d > 8.50f))
                {
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_lwl_v_3_is_outside_valid_domain")) + QStringLiteral(" [4.34 ... 8.50]"));
                    stop = true;
                }
            if ((fgetlcb() < - 6) || (fgetlcb() > 0))
                {
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_lcb_is_outside_valid_domain")) + QStringLiteral(" [-6.00 ... 0.00]"));
                    stop = true;
                }
            if ((fgetcp() < 0.52) || (fgetcp() > 0.60))
                {
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_cp_is_outside_valid_domain")) + QStringLiteral(" [0.52 ... 0.60]"));
                    stop = true;
                }
            if (stop)
                resultsmemo.append(space (10) + tr(QStringLiteral("rs_attention____calculation_for_sea_vessels_by_holtrop_1988_method_")));
            if (! stop)
                {
                    speed = fgetendspeed();
                    convertedspeed = speed * 1852 / 3600;
                    calculateresistance (convertedspeed , fgetlcb() , fgetcp() , rf , rr , rt);
                    flag = 0.001;
                    if (rt < 10000)
                        flag = 1.f;
                    resultsmemo.append(QStringLiteral(""));
                    resultsmemo.append(QStringLiteral(""));
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_by")) + QStringLiteral(" Delft"));
                    resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+---------+---------+-----------+---------+---------+---------+"));
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_l_speed_l_speed_l__fr___l___r_f___l___r_r___l____r_t____l__power__l_cp_opt__l_lcb_opt_l")));
                    yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kn"));
                    ;
                    rightTitle = tr(QStringLiteral("rs_power")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kw"));
                    if (flag == 1)
                        {
                            resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn__l__m_s__l_______l____n____l____n____l_____n_____l____w____l_________l_________l")));
                            yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral(", ") + tr(QStringLiteral("rs_n"));
                            rightTitle = tr(QStringLiteral("rs_power")) + QStringLiteral(", ") + tr(QStringLiteral("rs_w"));
                        }
                    else
                        resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn__l__m_s__l_______l___kn____l___kn____l____kn_____l____kw___l_________l_________l")));
                    resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+---------+---------+-----------+---------+---------+---------+"));
                    speed = fgetstartspeed();
                    index = 0;
                    cpopt.resize(truncp ((fgetendspeed() - fgetstartspeed()) / fgetstepspeed()) + 10);
                    lcbopt.resize(truncp ((fgetendspeed() - fgetstartspeed()) / fgetstepspeed()) + 10);
                    while (speed <= fgetendspeed()) {
                        check();
                        {
                            convertedspeed = speed * 1852 / 3600;
                            if (settings. projectunits == 1)
                                froudenumber = convertedspeed / sqrtp (9.81 * foot * fgetlwl());
                            else
                                froudenumber = convertedspeed / sqrtp (9.81 * fgetlwl());
                            calculateresistance (convertedspeed , fgetlcb() , fgetcp() , rf , rr , rt);
                            line = formatValue(speed, 6, 2);
                            line = QStringLiteral("|") + line + QStringLiteral(" |");
                            tmp = formatValue(convertedspeed, 6, 2);
                            line = line + tmp + QStringLiteral(" |");
                            tmp = formatValue(froudenumber, 6, 3);
                            line = line + tmp + QStringLiteral(" |");
                            tmp = formatValue(rf * flag, 8, 1);
                            line = line + tmp + QStringLiteral(" |");
                            if (((froudenumber >= 0) && (froudenumber <= 0.45)) || ((froudenumber > 0.475) && (froudenumber <= 0.75f)))
                                {
                                    ; // presentation only
                                    point(QStringLiteral("series1"), speed ,  rf * flag );
                                    point(QStringLiteral("series2"), speed ,  rr * flag );
                                    point(QStringLiteral("series3"), speed ,  rt * flag );
                                    point(QStringLiteral("series4"), speed ,  rt * flag * convertedspeed );
                                    index += 1;
                                    cpopt [index] = optimumcp (convertedspeed);
                                    lcbopt [index] = optimumlcb (convertedspeed);
                                    tmp = formatValue(rr * flag, 8, 1);
                                    line = line + tmp + QStringLiteral(" |");
                                    tmp = formatValue(rt * flag, 10, 1);
                                    line = line + tmp + QStringLiteral(" |");
                                    tmp = formatValue(rt * convertedspeed * flag, 8, 1);
                                    line = line + tmp + QStringLiteral(" |");
                                    tmp = formatValue(cpopt [index], 8, 3);
                                    line = line + tmp + QStringLiteral(" |");
                                    tmp = formatValue(lcbopt [index], 8, 3);
                                    line = line + tmp + QStringLiteral(" |");
                                }
                            else
                                line = line + QStringLiteral(" ------- | --------- | ------- | ------- | ------- |");
                            resultsmemo.append(space (10) + line);
                            speed = speed + fgetstepspeed();
                        }
                    }
                    resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+---------+---------+-----------+---------+---------+---------+"));
                }
            else
                {
                    if (units == 1)
                        {
                            resultsmemo.append(space (10) + tr(QStringLiteral("rs_this_is_version_of_program_work_with_metric_system_units_only")));
                            return;
                        }
                    resultsmemo.append(QStringLiteral(""));
                    if (fgetdraft() / fgetdrafttotal() < 0.99)
                        {
                            resultsmemo.append(space (10) + QStringLiteral("Tc/T    ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" [0.99 - 1.00]"));
                            resultsmemo.append(QStringLiteral(""));
                            resultsmemo.append(space (10) + tr(QStringLiteral("rs_calculation_of_resistance_for_keeled_yacht_by_holtrop_s_method_is_impossible___")));
                            ; // presentation only
                            return;
                        }
                    if ((fgetcp() > 0.85) || (fgetcp() < 0.55))
                        resultsmemo.append(space (10) + QStringLiteral("Cp      ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" [0.55 ... 0.85]"));
                    if ((fgetlwl() / fgetbwl() > 14.9) || (fgetlwl() / fgetbwl() < 3.9))
                        resultsmemo.append(space (10) + QStringLiteral("Lwl/Bwl ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" [3.90 ... 14.9]"));
                    if ((fgetbwl() / fgetdraft() > 4) || (fgetbwl() / fgetdraft() < 2.1))
                        resultsmemo.append(space (10) + QStringLiteral("Bwl/Tc  ") + tr(QStringLiteral("rs_is_outside_valid_domain")) + QStringLiteral(" [2.10 ... 4.0]"));
                    if (fgetlwl() / fgetbwl() <= 2)
                        {
                            warning(tr(QStringLiteral("rs_calculation_is_not_possible__because_l_b_2____")));
                            return;
                        }
                    speed = fgetendspeed();
                    convertedspeed = speed * 1852 / 3600;
                    calculateresistanceholtr (convertedspeed , fgetlcb() , fgetcp() , rf , rr , w , t0 , nr);
                    flag = 0.001;
                    if ((rf + rr) > 10)
                        flag = 1.f;
                    resultsmemo.append(QStringLiteral(""));
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_final_calculations_of_resistance_by_a_method_holtrop_1988")));
                    resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+--------+--------+--------+---------+-------+-------+"));
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_l_speed_l_speed_l__fr___l___r_f__l___r_r__l___r_t__l__power__l___wt__l___t___l")));
                    yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral(", ") + tr(QStringLiteral("rs_n"));
                    ;
                    rightTitle = tr(QStringLiteral("rs_power")) + QStringLiteral(", ") + tr(QStringLiteral("rs_w"));
                    if (flag == 1)
                        {
                            resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn__l__m_s__l_______l___kn___l___kn___l____kn__l____kw___l_______l_______l")));
                            yTitle = tr(QStringLiteral("rs_resistance")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kn"));
                            rightTitle = tr(QStringLiteral("rs_power")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kw"));
                        }
                    else
                        resultsmemo.append(space (10) + tr(QStringLiteral("rs_l___kn__l__m_s__l_______l____n___l____n___l____n___l____w____l_______l_______l")));
                    resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+--------+--------+--------+---------+-------+-------+"));
                    speed = fgetstartspeed();
                    cpopt.resize(truncp ((fgetendspeed() - fgetstartspeed()) / fgetstepspeed()) + 10);
                    lcbopt.resize(truncp ((fgetendspeed() - fgetstartspeed()) / fgetstepspeed()) + 10);
                    while (speed <= fgetendspeed()) {
                        check();
                        {
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
                            tmp = formatValue(rf / flag, 7, 2);
                            line = line + tmp + QStringLiteral(" |");
                            if (((froudenumber >= 0) && (froudenumber <= 0.55)))
                                {
                                    ; // presentation only
                                    point(QStringLiteral("series1"), speed ,  rf / flag );
                                    point(QStringLiteral("series2"), speed ,  rr / flag );
                                    point(QStringLiteral("series3"), speed ,  (rf + rr) / flag );
                                    point(QStringLiteral("series4"), speed ,  (rf + rr) / flag * convertedspeed );
                                    tmp = formatValue(rr / flag, 7, 2);
                                    line = line + tmp + QStringLiteral(" |");
                                    tmp = formatValue((rf + rr) / flag, 7, 2);
                                    line = line + tmp + QStringLiteral(" |");
                                    tmp = formatValue((rf + rr) / flag * convertedspeed, 8, 2);
                                    line = line + tmp + QStringLiteral(" |");
                                    tmp = formatValue(w, 6, 3);
                                    line = line + tmp + QStringLiteral(" |");
                                    tmp = formatValue(t0, 6, 3);
                                    line = line + tmp + QStringLiteral(" |");
                                }
                            resultsmemo.append(space (10) + line);
                            speed = speed + fgetstepspeed();
                        }
                    }
                    resultsmemo.append(space (10) + QStringLiteral("+-------+-------+-------+--------+--------+--------+---------+-------+-------+"));
                    resultsmemo.append(space (10) + QStringLiteral("EtaR =  ") + number (nr , 0 , 6 , 4));
                    speed = speed - fgetstepspeed();
                    convertedspeed = speed * 1852 / 3600;
                    resultsmemo.append(space (10) + tr(QStringLiteral("rs_results_of_optimisation_")));
                    cpopt [1] = optimumcph (convertedspeed);
                    lcbopt [1] = optimumlcbh (convertedspeed);
                    resultsmemo.append(QStringLiteral(""));
                    resultsmemo.append(space (10) + QStringLiteral("Copyright (c) 2007, Timoshenko V.F."));
                }
            ; // presentation only
            field(QStringLiteral("printbutton")) . enabled = true;
            fsetlcb(lcb_n);
        }
}

bool Resistance_delft::execute() {
bool result{};
int units{};
float temper{};
    ; // presentation only
    xTitle = tr(QStringLiteral("rs_speed")) + QStringLiteral(", ") + tr(QStringLiteral("rs_kn"));
    units = settings. projectunits;
    field(QStringLiteral("densitybox")) . enabled = false;
    temper = settings. projectwatertemper;
    field(QStringLiteral("viscositybox")) . enabled = false;
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
    field(QStringLiteral("_label17")) . caption = lengthstr (units);
    field(QStringLiteral("_label18")) . caption = areastr (units);
    field(QStringLiteral("_label19")) . caption = lengthstr (units);
    field(QStringLiteral("_label30")) . caption = areastr (units);
    fsetviscosity(findwaterviscosity (temper , units));
    if (units == 0)
        field(QStringLiteral("_label36")) . caption = QStringLiteral("*10^(-6) ") + tr(QStringLiteral("rs_m_2_s"));
    else
        {
            field(QStringLiteral("_label36")) . caption = QStringLiteral("*10^(-6) ") + tr(QStringLiteral("rs_ft_2_s"));
            fsetviscosity(fgetviscosity() * 10.76391);
        }
    result = false;
    return result;
}
}
