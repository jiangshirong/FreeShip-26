"""Small section fixtures; expected cuts are analytic intersections of flat polygons."""
from __future__ import annotations
import copy
import struct
from native_subdivision_cases import geometry,layer,cases as subdivision_cases

SAMPLES=[0,.125,.25,.5,.75,.875,1]
def f32(v):return struct.unpack('<f',struct.pack('<f',v))[0]
def adjacent(v,step):return struct.unpack('<f',struct.pack('<I',struct.unpack('<I',struct.pack('<f',v))[0]+step))[0]
def plane(name,coefficients,**kw):return dict(id=name,coefficients=coefficients,route='surface',hydrostatics_layers_only=False,**kw)
def square(y0=-1,y1=1,z=0):return [[-1,y0,z],[1,y0,z],[1,y1,z],[-1,y1,z]]
def sheets(intervals):
    points=[];faces=[]
    for a,b in intervals:
        n=len(points);points.extend(square(a,b));faces.append([n+i for i in range(4)])
    return geometry(points,faces,corners=range(len(points)))
def surface(name,g,queries,level=1):return dict(id=name,kind='surface',geometry=g,level=level,planes=queries,sample_parameters=SAMPLES)
def chain(points,flags=None,fragments=2):return dict(points=points,knuckles=flags or [False]*len(points),fragments=fragments)
def join(name,chains,error=.01,force=False,expected=None):
    c=dict(id=name,kind='join',chains=chains,join_error=error,force_to_one=force,sample_parameters=SAMPLES)
    if expected is not None:c['expected_join']=expected
    return c

def cases():
    standard=[plane('station',[1,0,0,-.25]),plane('buttock',[0,1,0,-.25]),plane('diagonal',[1,1,0,0]),
        plane('coplanar',[0,0,1,0]),plane('outside',[0,0,1,-2]),plane('edge',[1,0,0,-1]),plane('vertex',[1,1,0,-2]),plane('scaled-station',[2,0,0,-.5])]
    result=[surface('square',sheets([(-1,1)]),standard)]
    cube=next(c['geometry'] for c in subdivision_cases()['cases'] if c['id']=='closed-cube-sharp-mode-0')
    result.append(surface('cube',cube,[plane('station',[1,0,0,-.25]),plane('buttock',[0,1,0,-.25]),plane('waterline',[0,0,1,-.25]),
        plane('diagonal',[1,1,0,0]),plane('coplanar-face',[1,0,0,-1]),plane('tangent-edge',[1,1,0,-2]),plane('tangent-vertex',[1,1,1,-3])]))
    layerg=sheets([(-1,1),(3,5)]);layerg['layers']=[layer('Hidden intersections',surface_visible=False,use_in_hydrostatics=False),layer('Hydro only',use_for_intersections=False)]
    layerg['faces'][1]['layer']=1
    result.append(surface('layers',layerg,[plane('standard',[1,0,0,-.25]),dict(plane('hydro',[1,0,0,-.25]),hydrostatics_layers_only=True),
        dict(plane('explicit-reverse',[1,0,0,-.25]),route='faces',control_faces=[1,0]),dict(plane('explicit-empty',[1,0,0,-.25]),route='faces',control_faces=[])]))
    result.append(surface('coincident-disconnected',sheets([(-1,1),(-1,1)]),[plane('station',[1,0,0,-.25])]))
    for name,intervals in [('tiny-alone',[(0,1/32)]),('tiny-and-large',[(0,1/32),(2,4)]),('small-and-large',[(0,1/16),(2,4)]),
        ('gap-below',[(0,1),(1+1/16,2)]),('gap-above',[(0,1),(1+1/8,2)])]:
        result.append(surface(name,sheets(intervals),[plane('station',[1,0,0,-.25])]))
    for step in (-1,0,1):
        width=adjacent(.001**.5,step)
        result.append(surface(f'tiny-threshold-{step:+d}',sheets([(0,width),(2,4)]),[plane('station',[1,0,0,-.25])]))
    result.append(surface('level-zero',sheets([(-1,1)]),[plane('station',[1,0,0,0])],0))
    result.append(surface('branch',geometry([[0,0,0],[2,0,-1],[2,0,1],[-2,0,-1],[-2,0,1],[0,2,-1],[0,2,1]],[[0,1,2],[0,3,4],[0,5,6]],corners=range(7)),[plane('waterline',[0,0,1,0])]))
    result.append(surface('empty',geometry([],[]),[plane('inside-default-box',[1,0,0,-.5])]))
    result.append(surface('isolated',geometry([[99,99,99]],[]),[plane('outside-zero-box',[1,0,0,-.5])]))
    result.append(surface('free-curve',geometry([[0,0,0],[1,1,0],[2,0,0]],[],curves=[dict(points=[0,1,2],visible=True)]),[plane('middle',[1,0,0,-1])]))
    # The old algorithm's direct constants and Single JoinError have different
    # arithmetic origins. Keep exact neighbour values in the JSON evidence.
    for epsilon in (1e-5,1e-4):
        for step in (-1,0,1):
            distance=adjacent(epsilon,step)
            shifted=sheets([(-1,1)])
            for point in shifted['points']:point['position'][0]-=1
            result.append(surface(f'epsilon-{epsilon:g}-{step:+d}',shifted,[
                plane('near-edge',[1,0,0,distance]),plane('outside-near-edge',[1,0,0,-distance]),
                dict(plane('outside-explicit',[1,0,0,-distance]),route='faces',control_faces=[0])]))
            result.append(join(f'closed-threshold-{epsilon:g}-{step:+d}',[chain([[0,0,0],[1,0,0],[distance,0,0]],fragments=3),chain([[0,0,0],[-1,0,0]])]))
    fixed=chain([[0,0,0],[1,0,0]],[False,False],2)
    for name,source in [('prepend-start',chain([[0,0,0],[-1,1,0]],[True,False],20)),('prepend-end',chain([[-1,1,0],[0,0,0]],[False,True],20)),
        ('append-start',chain([[1,0,0],[2,1,0]],[True,False],20)),('append-end',chain([[2,1,0],[1,0,0]],[False,True],20))]:
        expected=chain(([[-1,1,0],[0,0,0],[1,0,0]] if name.startswith('prepend') else [[0,0,0],[1,0,0],[2,1,0]]),[False,True,False],2)
        result.append(join(name,[copy.deepcopy(fixed),source],expected=[expected]))
    result.append(join('corrected-prepend-reference',[chain([[-1,1,0],[0,0,0],[1,0,0]],[False,True,False],2)]))
    for name,gap,error in [('join-gap-below',1/16,.01),('join-gap-above',1/8,.01),('join-equal',1/8,1/64),
        ('join-threshold-minus',adjacent(.125,-1),1/64),('join-threshold-plus',adjacent(.125,1),1/64)]:
        # Fixed endpoint at zero keeps adjacent-Single gap exact on transport.
        result.append(join(name,[chain([[-1,0,0],[0,0,0]]),chain([[gap,0,0],[2,1,0]])],error))
    result.append(join('force-gap',[copy.deepcopy(fixed),chain([[3,1,0],[4,1,0]])],force=True))
    result.append(join('closed-skip',[chain([[0,0,0],[1,0,0],[0,0,0]],fragments=2),chain([[0,0,0],[-1,0,0]])],force=True))
    result.append(join('equal-source-tie',[copy.deepcopy(fixed),chain([[1,0,0],[2,1,0]]),chain([[1,0,0],[2,-1,0]])]))
    result.append(join('early-candidate',[copy.deepcopy(fixed),chain([[1.002,0,0],[2,0,0]]),chain([[1.001,0,0],[3,1,0]])]))
    return dict(format='FreeShipNativeSectionsCases',version=1,cases=result)


def expected_segments(case,query):
    """Undirected raw geometric multiset only; topology/order are checked separately."""
    name=case['id'];qid=query['id'];g=case.get('geometry')
    if name in ('empty','isolated','free-curve','level-zero'):return []
    if name=='square':
        if qid in ('station','scaled-station'):return [([.25,-1,0],[.25,0,0]),([.25,0,0],[.25,1,0])]
        if qid=='buttock':return [([-1,.25,0],[0,.25,0]),([0,.25,0],[1,.25,0])]
        if qid=='diagonal':return [([-1,1,0],[0,0,0]),([0,0,0],[1,-1,0])]
        if qid in ('outside','vertex'):return []
        if qid=='edge':return [([1,-1,0],[1,0,0]),([1,0,0],[1,1,0])]
        if qid=='coplanar':
            ring=[[-1,-1,0],[0,-1,0],[1,-1,0],[1,0,0],[1,1,0],[0,1,0],[-1,1,0],[-1,0,0]]
            return list(zip(ring,ring[1:]+ring[:1]))
    if name=='cube' and qid in ('station','buttock','waterline','coplanar-face'):
        axis={'station':0,'buttock':1,'waterline':2,'coplanar-face':0}[qid];fixed=1 if qid=='coplanar-face' else .25
        other=[i for i in range(3) if i!=axis];ring=[]
        for a,b in [(-1,-1),(0,-1),(1,-1),(1,0),(1,1),(0,1),(-1,1),(-1,0)]:
            p=[0,0,0];p[axis]=fixed;p[other[0]]=a;p[other[1]]=b;ring.append(p)
        return list(zip(ring,ring[1:]+ring[:1]))
    if name=='cube' and qid=='tangent-vertex':return []
    if name in ('coincident-disconnected','tiny-alone','tiny-and-large','small-and-large','gap-below','gap-above','layers'):
        roots=list(range(len(g['faces'])))
        if name=='layers':roots={'standard':[0],'hydro':[1],'explicit-reverse':[1,0],'explicit-empty':[]}[qid]
        segments=[]
        for root in roots:
            ys=[g['points'][i]['position'][1] for i in g['faces'][root]['points']];a,b=min(ys),max(ys);mid=(a+b)/2
            segments.extend([([.25,a,0],[.25,mid,0]),([.25,mid,0],[.25,b,0])])
        return segments
    return None
