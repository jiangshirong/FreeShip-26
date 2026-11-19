"""Install reviewed English choices and record exact shared-key decisions.

Offline, tied to the frozen 174-key review. Not an automatic translation rule.
Different product meanings use explicit contexts; the unverified formula stays held.
"""
from pathlib import Path
import json
import re
import review_english_contexts as tool

CHOICES={
0:'Edge Extrusion',1:'Close',2:'Notice',3:'Browse…',4:'Number of transverse stations (odd)',
5:'Units',6:'Layer',7:'Faces',8:'Calculate',9:'Curves',
10:'Parameters changed; results need updating.',11:'Untitled Model',12:'Wetted surface area',13:'Trim',
14:'Unable to save the report:',15:'Shaded',16:'Image save failed; the original file was not replaced:',
17:'Plate unfolding cancelled.',18:'Selection',19:'Dart point',20:'Corner point',21:'All files present',
22:'Stations',23:'Buttock lines',24:'Waterlines',25:'Diagonals',
26:'The preferences storage location is unavailable.',27:'Shortcuts',28:'Perspective View',29:'Point',
30:'Flowlines',31:'Markers',32:'Delft Yacht Resistance',33:'Holtrop Ship Resistance',
34:'Engine & Propeller Matching',35:'Vessel Propulsion Characteristics (Passport)',36:'Profile View',37:'Body Plan View',
38:'Duplicate faces detected.',39:'The number of propellers must be an integer from 1 to 3.',40:'Trapezoidal',
41:'Calculation cancelled.',42:'The legacy solver requires an ASCII temporary directory.',
43:'Unable to create the solver process group.',44:'Unable to track the solver process.',45:'The solver log is too large.',
46:'The stability table must not exceed 500 cases.',47:'This method supports metric units only. Convert units in Project Settings first.',
48:'Solver method',49:'Propulsion interaction (user-specified)',50:'Specified thrust deduction fraction t',
51:' failed to start:',52:' failed to run, exit code:',53:'Projected wind area [m²]',
54:'Expanded blade area ratio Ae/Ao',55:'Propeller rotational speed [1/min]',
56:'This method calculates calm-water resistance only. Enter a manual sea margin, or select an external solver for sea-state corrections.',
57:'Unable to write the calculation file:',58:'Water kinematic viscosity',59:'The calculation result cache is in use.',
60:'The solver output exceeds the size limit.',61:'Propeller diameter (m)',62:'KA-4-70 series',
63:'1. File not found in \\Exec, or the file is damaged',64:'2. Input data are outside the valid range, or the data directory is invalid',
65:'3. This computer is very slow, Fcpu < 800 MHz',66:'4. Other processes are using more than 80% of the CPU',
67:'Ae/Ao is outside the limits for this chart!',68:'Ae/Ao** > 1.3: increase Dp',69:'Warning!_',
70:'Warning: the calculated propeller diameter exceeds the limit Dp_lim=',
71:'Warning: the power limit of the legacy freeware Free!ship+ has been reached!',
72:'Warning: more than 11 propeller diagrams are available only in the legacy Pro version!',
73:'Warning: the main engine power is very high!',74:'Warning: the main engine power is very low!',
75:'Warning: the requested speed is outside the calculated speed range!',76:'Passport diagram calculation',
77:'Calculate a propeller for the selected engine',78:'Propeller calculation stopped because Np is zero in the resistance calculation!',
79:'Propeller overall strength calculation at r/D=0.3:',80:'Click the button to select an engine',
81:'Thrust non-uniformity influence coefficient',82:'Torque non-uniformity influence coefficient',
83:'E-series controllable-pitch propeller in a nozzle',84:'No resistance calculation results were found in this directory!',
85:'| Value    |              Meaning                         |',
86:'| Value    |  Unit |                 Meaning                       |',
87:'| Value    |  Unit |              Meaning                         |',
88:'Maximum propeller diameter [m]',89:'The legacy freeware Free!ship+ power limit has been reached; try increasing the propeller diameter!',
90:'The legacy freeware Free!ship+ speed limit has been reached!',91:'min',
92:'Note: Propeller materials: 1-Brass Mn+Fe; 2-Bronze Al+Fe H9-4-4; 3-Carbon steel;',
93:'The blade count is outside the range for this chart!',94:'Propeller strength is sufficient in non-uniform flow',
95:'Propeller strength is sufficient in uniform flow',96:'Passport diagram calculation results',
97:'Optimal propeller calculation results for engine selection',98:'Propeller characteristics for the selected engine',
99:'The task cannot be solved with these input data!',100:'This propeller chart is available only in the legacy Pro version.',
101:'Propeller strength is insufficient in non-uniform flow!',102:'Propeller strength is insufficient in uniform flow!',
103:'This version of the program uses metric units only.',104:'Thrust deduction fraction t',105:'Water density [kg/m³]',
106:'Increase Ae/Ao to avoid cavitation and/or propeller damage.',107:'Increase the blade count until',
108:'Calculate propeller characteristics for main engine selection',109:'Main engine power [W] - Pe;  Thrust [N] - Te',
110:'Using OCT 5.0181-75 (former USSR)',111:'Using the Braun charts',112:'Based on model tests',
113:'Using the Neyman charts',114:'Using the Volodin charts',115:'Using the Nordstrom charts',116:'Using the de Groot charts',
117:'Using the SSPA charts',118:'Using the NPL charts',119:'Using the Series 64 charts',120:'Using the Doubrovsky method',
121:'long ton',122:'Outside the valid range',123:'Using the Papmehl formula with KSRI (Russia) additions',
124:'Some error may occur because extrapolation is used.',125:'Warning: this series is not applicable to this project!',
126:'Note 3: Bulbous-bow properties are valid when FP passes through the intersection',127:'of the bow with the DWL',
128:'Final resistance and power prediction using',129:'HydroNShIp v1.08 (NUoS, Ukraine): displacement-vessel resistance prediction',
130:'in',131:'Planing and pre-planing vessel resistance prediction',
132:'Pre-planing resistance and power calculation using the Mercier-Savitsky method',
133:'Final resistance and power calculation using the Mercier-Savitsky method',
134:'Static and Dynamic Stability Curves (SSD & DSD)',135:'Heel angle, [°]',
136:'Propeller pitch-to-diameter ratio P/Dp                      =',
137:'Required propeller thrust (kN)                   =',138:'Shaft depth below waterline (m)                    =',
139:'Burrill back cavitation constraint    =',140:'Initial expanded blade area ratio Ae/Ao     =',
141:'Initial propeller diameter Dp (m)                 =',142:'Minimum diameter constraint Dpmin (m) =',
143:'Propeller diameter Dp (m)                       =',144:'Propeller pitch P (m)                        =',
145:'Propeller open-water efficiency Eta0                     =',146:'Optimization search evaluation count    =',
147:'Some input data are incorrect!',148:'The Holtrop wetted-area calculation is invalid because Cwp=0!',
149:'Z coordinate of the bulbous-bow area centroid',150:'Bulbous-bow area at FP',151:'Delft sailing yacht series',
152:'| Speed | Speed | S/L | Friction resistance | Residual resistance | Total resistance | Spilman |',
153:'Warning: Teta0 < 60°',154:'The SSD calculation may be inaccurate: increase the maximum heel angle!',
155:'Deadrise amidships',156:'Center of gravity X coordinate',157:'Roughness coefficient ×10³',
158:'Resistance ×10 [kN]; Power [kW]',159:'Calculation time',160:'s',
161:'Planing-craft resistance and power using the Lyubomirov I.P. formula',
162:'Planing-craft resistance and power using the Sedov L.I. method',163:'Final planing-craft resistance and power calculation',
164:'Warning: increase the number of propellers or reduce the requested speed.',165:'- calculated',
166:'- for propeller chart selection',167:'- from the optimal propeller RPM curve',
168:'This wetted area is invalid because Cwp=0!',169:'No parent-ship data for this series are currently available in the database!',
170:'Additional Input Data',171:'AutoCAD DXF File',172:'Text File',
}

SCOPES={
    ('menu','计算'):'Calculations',('visibility','关闭'):'Off',('collection','图层'):'Layers',
    ('action','选择'):'Select',('column','快捷方式'):'Shortcut',
    ('warnings','提示'):'Warnings',
    ('performance.wind.Edit4.label','横向受风投影面积 [m2]'):'Lateral projected wind area [m²]',
    ('performance.wind.Edit6.label','横向受风投影面积 [m2]'):'Frontal projected wind area [m²]',
}

FEEDBACK={
    '完成 %1 个工况，耗时 %2 s。':'Cases completed: %1. Time: %2 s.',
    '其中 %1 个未得到有效结果。':'Without valid results: %1.',
    '其中 %1 个工况的部分量未提供（见状态列）。':'Cases with missing values: %1 (see Status).',
    '完成 %1 组数据，耗时 %2 s。':'Data groups completed: %1. Time: %2 s.',
    '完成 %1 条曲线，耗时 %2 s。':'Curves completed: %1. Time: %2 s.',
    '有 %1 条提示，见报告。':'Notices: %1 (see report).',
    '已导出 %1 个断面、%2 个点。':'Exported %1 sections and %2 points.',
    '已水平封顶 %1 个断面。':'Sections capped horizontally: %1.',
    '有 %1 个断面未与船体相交。':'Sections not intersecting the hull: %1.',
}


def main():
    revision=tool.QT.parent/'翻译工作档案'/'TranslationContext_Qt_20261113_verified'
    conflicts=tool.load(revision/'english.conflicts.json')
    if len(conflicts)!=174 or conflicts[8]['source_text']!='计算' or conflicts[173]['variants']:
        raise ValueError('This decision set applies only to the frozen 174-key review')
    path=tool.QT/'src/app/languages/en.json';before=tool.load(path)
    updated=json.loads(json.dumps(before));decisions=[]
    for i,english in CHOICES.items():
        row=conflicts[i];source=row['source_text']
        english=re.match(r'^\s*',source)[0]+english.strip()+re.search(r'\s*$',source)[0]
        issues=tool.protected_issues(source,english)
        if issues:raise ValueError(str(i)+': '+str(issues))
        updated['messages'][source]=english
        decisions.append(dict(index=i,source_text=source,canonical_en=english,
            previous_variants=row['variants'],sites=row['sites'],decision='repository_source_and_terminology_review',
            boundary='An English wording decision; historical reference-only copies retain their original reachability classification.'))
    for (context,source),english in SCOPES.items():
        if tool.protected_issues(source,english):raise ValueError('Scoped token/unit mismatch')
        updated['messages'][context+'\x04'+source]=english
    for source,english in FEEDBACK.items():
        if tool.parameter_tokens(source)!=tool.parameter_tokens(english):raise ValueError('Feedback placeholder mismatch')
        updated['messages'][source]=english
    generated=[]
    # These captions are assembled in resistance.cpp, so literal-only scans
    # cannot see the complete lookup keys. Units/indices remain verbatim.
    for prefix,units in [('阻力 (',('N','kN')),('有效功率 (',('W','kW')),('10 × 阻力 (',('N','kN'))]:
        for unit in units:
            source=prefix+unit+')';english=updated['messages'][prefix]+unit+')'
            updated['messages'][source]=english
            generated.append(dict(source_text=source,canonical_en=english,role='axis_title',
                context_en='Axis caption for resistance or effective power. Preserve the displayed SI unit and the explicit factor 10 where present.',
                evidence_file='src/document/resistance.cpp'))
    for prefix in ('弗劳德数 Fr ','剩余阻力系数 Cr '):
        for index in range(10):
            source=prefix+str(index);english=updated['messages'][prefix]+str(index)
            updated['messages'][source]=english
            generated.append(dict(source_text=source,canonical_en=english,role='field',
                context_en='Indexed resistance-data input label. Fr is the Froude number and Cr the residual-resistance coefficient. The trailing digit identifies a data point, not a quantity to translate.',
                evidence_file='src/document/resistance.cpp'))
    if tool.load(path)!=before:raise ValueError('English changed concurrently')
    tool.write(path,updated)
    tool.write(tool.QT/'docs/migration/ENGLISH-GAPS-DECISIONS-20261113.json',dict(
        previous_revision=str(revision),before_sha256=tool.digest(json.dumps(before,ensure_ascii=False,sort_keys=True).encode()),
        after_file_sha256=tool.digest(path.read_bytes()),entries=len(updated['messages']),decisions=decisions,
        scoped_messages=[dict(context=c,source_text=s,canonical_en=t) for (c,s),t in SCOPES.items()],
        feedback_messages=FEEDBACK,
        generated_captions=generated,
        held_source=conflicts[173]['source_text'],translation_api_calls=0))
    print('Installed reviewed English decisions:',len(decisions),'scoped meanings:',len(SCOPES),'entries:',len(updated['messages']))


if __name__=='__main__':main()
