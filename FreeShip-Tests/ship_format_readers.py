"""Independent legacy-layout readers; not Archimedes or a PAM solver.

Geometry checks use polygon integrals rather than the production writer code.
"""
import math
from pathlib import Path
import xml.etree.ElementTree as ET

def number(text):
    value=float(text)
    if not math.isfinite(value):raise ValueError('Non-finite number')
    return value

def count(text):
    value=int(text)
    if not 0<value<=4000000:raise ValueError('Invalid record count')
    return value

def polygon(points):
    if len(points)<3:raise ValueError('Insufficient polygon points')
    area=cy=cz=0
    for (y,z),(v,w) in zip(points,points[1:]+points[:1]):
        cross=y*w-v*z;area+=cross;cy+=(y+v)*cross;cz+=(z+w)*cross
    if area<=0:raise ValueError('Non-positive polygon area')
    return {'area':area/2,'centroid':[cy/(3*area),cz/(3*area)]}

def station(x,points):return dict(x=number(x),points=points,**polygon(points))

def read_hll(path):
    data=Path(path).read_bytes()
    if not data.startswith(b'<?xml') or b'<!DOCTYPE' in data or b'<!ENTITY' in data:raise ValueError('Unexpected XML preamble')
    root=ET.fromstring(data)
    if root.tag!='HULL':raise ValueError('Not a hull document')
    app=root.findall('APPENDAGE')
    if count(root.findtext('APPENDAGE_COUNT'))!=len(app) or len(app)!=1:raise ValueError('Appendage count mismatch')
    app=app[0];sections=app.findall('SECTION')
    if count(app.findtext('SECTION_COUNT'))!=len(sections):raise ValueError('Section count mismatch')
    stations=[]
    for s in sections:
        v=[number(x) for x in s.findtext('OFFSETS').split()]
        if len(v)!=count(s.findtext('OFFSET_COUNT'))*2:raise ValueError('Offset count mismatch')
        stations.append(station(s.findtext('YCOORD'),[v[i:i+2] for i in range(0,len(v),2)]))
    return dict(title=root.findtext('HULL_NAME'),density=number(root.findtext('DENSITY')),stations=stations,
                references={k:number(root.findtext(k)) for k in ('AP','FP','FWD_DRAUGHT_MARK','MID_DRAUGHT_MARK','AFT_DRAUGHT_MARK')})

class Lines:
    def __init__(self,path):self.lines=Path(path).read_text('ascii').splitlines();self.index=0
    def get(self):
        if self.index>=len(self.lines):raise ValueError('Unexpected EOF')
        s=self.lines[self.index];self.index+=1;return s
    def need(self,text):
        got=self.get()
        if got!=text:raise ValueError(f'Expected {text}, got {got}')
    def end(self):
        if self.index!=len(self.lines):raise ValueError('Trailing records')

def read_app(path):
    r=Lines(path)
    while r.lines[r.index].startswith('*'):r.get()
    r.need('APPENDAGE_NAME');title=r.get();r.need('AP');ap=number(r.get());r.need('FP');fp=number(r.get())
    r.need('DENSITY');density=number(r.get());r.need('NUMBER_OF_FRAMES');frames=count(r.get());r.need('***START_OF_FRAME_INFORMATION***')
    stations=[]
    for i in range(frames):
        r.need('FRAME_INDEX: '+str(i));r.need('FRAME_NAME');r.get();r.need('FRAME_Y');x=number(r.get());r.need('NUMBER_OF_COORDINATES');n=count(r.get());r.need('***START_OF_ORDINATES***')
        points=[[number(r.get()),number(r.get())] for _ in range(n)];r.need('***END_OF_ORDINATES***');stations.append(station(x,points))
    r.need('***END_OF_FILE***');r.end()
    return dict(title=title,density=density,stations=stations,references={'AP':ap,'FP':fp})

def read_pam(path):
    r=Lines(path);title=r.get();header=[number(x) for x in r.get().split()]
    if len(header)!=7:raise ValueError('PAM header size')
    loading=[number(x) for x in r.get().split()]
    if len(loading)!=10:raise ValueError('PAM loading header size')
    counts=r.get().split()
    if len(counts)!=3:raise ValueError('PAM station header size')
    stations=[]
    for _ in range(count(counts[0])):
        x,n=r.get().split();points=[]
        for _ in range(count(n)):
            z,y=map(number,r.get().split());points.append([y,z])
        stations.append(station(x,points))
    r.need('\\*');r.end()
    return dict(title=title,density=header[0],parameters=header[1:],loading=loading,section_parameters=list(map(number,counts[1:])),stations=stations)

READERS={'arch_hll':read_hll,'arch_app':read_app,'pam':read_pam}
def read(path,fmt):
    result=READERS[fmt](path);stations=result['stations']
    if len(stations)<2 or any(a['x']>=b['x'] for a,b in zip(stations,stations[1:])):raise ValueError('Stations not ordered')
    if result['density']<=0:raise ValueError('Invalid density')
    return result
