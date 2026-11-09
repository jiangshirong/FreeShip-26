"""Documented deterministic protocol samples, not measured ship trials.

Conventional hull: L=120 m, B=18 m, T=6 m, volume=8500 m3.
Yacht/kayak samples exercise internal formulas. Propeller parameters exercise
historical executable protocols; accuracy certification is a separate task.
"""
CONVENTIONAL=dict(StartSpeedBox=5,EndSpeedBox=18,StepSpeedBox=20,
    LwlBox=120,BwlBox=18,DraftTotalBox=6,DraftBox=0,WettedSurfacebox=2800,
    WlAreabox=1800,DisplacementBox=8500,LCBBox=0,CpBox=.65,
    NpBox=1,DpBox=4,KeBox=1,A11Box=0,KBulbBox=0,ZBulbBox=0,KsBox=150,Losbox=125,LoSbox=125)
YACHT=dict(StartSpeedBox=2,EndSpeedBox=7,StepSpeedBox=.5,LwlBox=10,
    BwlBox=3,DraftBox=.6,DraftTotalBox=1.6,WettedSurfacebox=22,
    WlAreabox=18,DisplacementBox=8,LCBBox=-3,CpBox=.55)
KAYAK=dict(Edit2=5,Edit3=.6,Edit4=.2,Edit5=3.2,Edit6=.55,Edit7=.2,Edit8=8,Edit9=.5,Edit10=0)
PLANING=dict(Edit2=20,Edit3=4,Edit4=.9,Edit5=70,Edit6=.7,Edit7=40,
    Edit8=15,Edit9=8,Edit10=3,Edit11=.4,Edit12=35,ComboBox=0)
PROPELLERS={
    'propeller1':dict(Edit2=15,Edit3=100,Edit4=1,Edit5=4,Edit6=.1,Edit17=4,Edit18=5),
    'propeller2':dict(Edit2=1000,Edit3=250,Edit4=1,Edit16=15,
        **{f'Edit17_{i}':10+2*i for i in range(1,6)},**{f'Edit18_{i}':40+20*i for i in range(1,6)}),
    'propeller3':dict(Edit2=3,Edit3=.6,Edit4=1,Edit5=.6,Edit8=1,Edit9=200,Edit17=600),
    'propeller4':dict(Edit2=15,Edit3=100,Edit8=1.2,Edit10=4,Edit11=2,Edit12=4),
    'propeller5':dict(ComboBox=1,ComboBox1=1,Edit2=200,Edit3=.6,Edit4=1,Edit5=4,Edit6=3,Edit7=4),
}
def sample(method,catalog):
    source=YACHT if method=='delft' else KAYAK if method=='kaper' else PLANING if method=='planing' else PROPELLERS[method] if method.startswith('propeller') else CONVENTIONAL
    keys={f['key'] for f in catalog[method]['fields']}
    if method=='oortmerssen':source=dict(CONVENTIONAL,LwlBox=40,BwlBox=8,DraftTotalBox=3,DisplacementBox=600,WettedSurfacebox=450,LoSbox=41,WlAreabox=260,DpBox=2,EndSpeedBox=10,StepSpeedBox=12)
    if method in ('ost','rbhs','mh'):source=dict(CONVENTIONAL,NserBox=1)
    if method in ('rbhs','mh'):source=dict(CONVENTIONAL,ComboBox=1,LwlBox=30,BwlBox=5,DraftTotalBox=1.5,DisplacementBox=100,WettedSurfacebox=190,WlAreabox=100,StartSpeedBox=12,EndSpeedBox=25,StepSpeedBox=28,DpBox=1)
    lower={key.lower():key for key in keys}
    return {lower[key.lower()]:value for key,value in source.items() if key.lower() in lower}
