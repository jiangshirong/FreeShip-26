"""Native affine edits: exact original-service oracle and independent FBM/math checks.

run(report, runtime, source, out) is included by qt-document. The synthetic FBM
writer/observer below is test-only, independently encoded from SaveBinary's 5.0
layout. It seeds every resistance record, persistent hydro ranges and markers;
neither fixture construction nor expected values call the new C++ codec.
"""
import argparse
import json
import math
import struct
import uuid
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_modeling as modeling
import native_linesplan_app as nla

f32=e.f32
RECORDS=[('delft',17,2),('kaper',9,1),('holtr',43,2),('ost',52,2),
         ('propeller_task1',15,1),('propeller_task2',25,1),('propeller_task3',16,1),
         ('planing',13,1),('reverse_propeller',16,1),('hollenbach',43,2),
         ('propeller_task4',15,1),('propeller_task5',5,1),('oortmerssen',43,2),
         ('fung',43,2),('maneuvering',19,1),('hydrodynamic_task1',11,1),('rbhs',52,2),('mh',52,2)]
SCALED={'delft','kaper','holtr','ost','planing','hollenbach','oortmerssen','fung','rbhs','mh'}


def fixture(path,degenerate=False):
    data=bytearray()
    def pack(fmt,*v):data.extend(struct.pack('<'+fmt,*v))
    def text(v):v=v.encode('utf8');pack('i',len(v));data.extend(v)
    def floats(v):pack('f'*len(v),*v)
    def record(index):
        _,count,flags=RECORDS[index];floats([0 if degenerate else .375+i*.125+index*.03125 for i in range(count)])
        data.extend(bytes(flags)+bytes([0xa5])*(4-flags))
    text('FREE!ship');pack('Bi',44,1)
    pack('i',1);data.extend(bytes([1,0,1,1,1,1,1,0,0,0]));pack('fBf',1,1,.1);data.extend(bytes(7))
    text('Native affine oracle');text('Independent test fixture')
    floats([1e-7]*3 if degenerate else [10,4,1]);pack('BffBiiBfB',0,1.025,1,0,0x5080c0,0,int(degenerate),2.125,0)
    text('Preserve hydro, names, groups, calculator padding');text('FreeShip regression')
    pack('iBB',0,0,0);floats([.25,2,.125,.375]);pack('i',2);floats([4,6]);floats([2,8,.5]);pack('Bi',0,2);floats([0,20]);pack('i',2);floats([.1,.2]);pack('BfBBi',0,1.25,0,0,123)
    pack('i',1);text('Hull');pack('iiBBBBBffBi',7,0x80a0c0,1,1,0,1,1,7.85,.004,1,255);pack('i',0)
    points=[(0,0,0),(10,0,0),(10,2,0),(0,2,0),(0,0,3),(10,0,3),(10,2,3),(0,2,3)]
    faces=[[0,3,2,1],[4,5,6,7],[3,7,6,2],[0,4,7,3],[1,2,6,5]]
    if degenerate:points=[(0,0,0)];faces=[]
    edges=sorted({tuple(sorted((a,b))) for face in faces for a,b in zip(face,face[1:]+face[:1])})
    pack('i',len(points))
    for p in points:floats(p);pack('iBB',3,0,0)
    pack('i',len(edges))
    for a,b in edges:pack('iiBB',a,b,1,0)
    if degenerate:pack('i',0)
    else:pack('iiiiiB',1,3,0,1,2,0)
    pack('i',len(faces))
    for face in faces:pack('i',len(face));pack('i'*len(face),*face);pack('iB',0,0)
    for kind,plane in enumerate([(1,0,0,-5),(0,1,0,-1),(0,0,1,-1.5),(0,.70710678,.70710678,-1)],1):
        if degenerate:pack('i',0)
        else:pack('iiB',1,kind,0);floats(plane);pack('Bi',0,0)
    markers=[] if degenerate else [[(1.25,2.5,3.75),(2,3,4)],[(-.25,.75,-1.125),(0,1,2)]]
    pack('i',len(markers))
    for marker in markers:
        pack('BBBfi',1,0,1,.125,len(marker))
        for p in marker:floats(p);pack('B',1)
    record(0);record(1);pack('ii',0,0)
    for i in range(2,len(RECORDS)):record(i)
    for i,name in enumerate(('Bow reference','Keel boundary','Bottom panel','Reference curve')):
        if degenerate and i:pack('i',0)
        else:pack('ii',1,0);text(name)
    if degenerate:pack('iii',0,0,0)
    else:pack('ii',0,1);text('Reference group');pack('i',2);text('0');text('7');pack('i',0)
    path.write_bytes(data)
    return observe(path)


def observe(path):
    """Independent fixed-version parser for the generated 5.0 fixture and saves."""
    data=path.read_bytes();at=0
    def take(fmt):
        nonlocal at
        size=struct.calcsize('<'+fmt);result=struct.unpack_from('<'+fmt,data,at);at+=size
        return result[0] if len(result)==1 else result
    def skip(n):
        nonlocal at
        if n<0 or at+n>len(data):raise ValueError('Truncated test fixture')
        at+=n
    def text():
        n=take('i');start=at;skip(n);return data[start:at]
    def array():return [take('f') for _ in range(take('i'))]
    if text()!=b'FREE!ship' or take('B')!=44:raise ValueError('This observer covers the generated 5.0 fixture only')
    skip(4+4+10+4+1+4+6+1)
    start=at;text();text();dimension_at=at;dimensions=list(take('fff'));particulars=take('B')
    skip(8+1+4+4);default_split=take('B');split=take('f');skip(1);text();text();skip(4)
    if take('B'):skip(8);skip(take('i'))
    skip(1);hydro_start=at;hydro=list(take('ffff'));displacements=array();ranges=list(take('fff'));use_steps=take('B');heels=array();trims=array();free=take('B');vcg=take('f');skip(1+1+4)
    settings=data[start:at];hydro_bytes=data[hydro_start:at]
    for _ in range(take('i')):text();skip(4+4+3+2+8+1+4)
    skip(4)
    points=[]
    for _ in range(take('i')):points.append(list(take('fff')));skip(6)
    skip(take('i')*10)
    for _ in range(take('i')):skip(take('i')*4);skip(1)
    for _ in range(take('i')):skip(take('i')*4);skip(5)
    planes=[]
    for _ in range(4):
        for _ in range(take('i')):
            kind=take('i');skip(1);planes.append(list(take('ffff')));skip(1)
            for _ in range(take('i')):skip(take('i')*(9 if kind in (1,2,3) else 13))
    markers=[];attributes=[]
    for _ in range(take('i')):
        attr=list(take('BBBf'));marker=[];knuckles=[]
        for _ in range(take('i')):marker.append(list(take('fff')));knuckles.append(take('B'))
        markers.append(marker);attributes.append([attr,knuckles])
    records={};raw={}
    for index,(name,count,flags) in enumerate(RECORDS):
        if index==2:
            if take('i') or take('i'):raise ValueError('Fixture unexpectedly acquired background/flow records')
        start=at;records[name]=[take('f') for _ in range(count)];skip(4);raw[name]=data[start:at]
    return dict(dimensions=dimensions,particulars=particulars,default_split=default_split,split=split,
                settings=settings,hydro_bytes=hydro_bytes,markers=markers,marker_attributes=attributes,
                planes=planes,calculations=records,calculation_bytes=raw,points=points,dimension_offset=dimension_at,
                hydro=dict(drafts=hydro,displacements=displacements,ranges=ranges,use_steps=use_steps,heels=heels,trims=trims,free=free,vcg=vcg))


def expected_calculation(name,values,vector):
    # Dimensional expectations: B -> Y, length -> X, draft -> Z,
    # displacement -> XYZ, longitudinal immersed areas -> XZ, waterplane -> XY.
    x,y,z=map(f32,vector);result=list(values)
    if name not in SCALED:return result
    if name in ('kaper','planing'):table={0:[z],1:[x],2:[y],4:[x,y,z],6:[x,z]}
    else:
        table={3:[y],5:[x,y,z],6:[z],7:[z],8:[x],9:[x,z],11:[x],12:[x],13:[x,z],15:[x,z]}
        table[16]=[x] if name in ('hollenbach','oortmerssen','fung') else [x,y]
    for index,factors in table.items():
        for factor in factors:result[index]=f32(result[index]*factor)
    return result


def analytic(points,selected,op,vector,pivot=(0,0,0)):
    values={i:list(p) for i,p in points.items()};v=list(map(f32,vector));pivot=list(map(f32,pivot))
    for i in selected:
        p=points[i]
        if op=='translate':values[i]=[f32(a+b) for a,b in zip(p,v)]
        elif op=='scale':values[i]=[f32(a*b) for a,b in zip(p,v)]
        else:
            # Independently compose inverse Z, then inverse Y, then inverse X
            # rotations. Compare with float-domain tolerance, separately from
            # the strict bit-for-bit original-service oracle.
            x,y,z=[a-b for a,b in zip(p,pivot)];rx,ry,rz=map(math.radians,v)
            x,y=x*math.cos(rz)+y*math.sin(rz),-x*math.sin(rz)+y*math.cos(rz)
            x,z=x*math.cos(ry)-z*math.sin(ry),x*math.sin(ry)+z*math.cos(ry)
            y,z=y*math.cos(rx)+z*math.sin(rx),-y*math.sin(rx)+z*math.cos(rx)
            values[i]=[a+b for a,b in zip((x,y,z),pivot)]
    return values


def probe_quadrants(r,runtime,out):
    """Small exact oracle check before the complete workflow/parameter suite."""
    out.mkdir(parents=True);seed=out/'quadrants.fbm';fixture(seed)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native, q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        for index,angles in enumerate([[0,0,90],[0,0,180],[0,0,270],[360000,-360000,270],[44.99999,135.125,-270.03125]]):
            ident=str(uuid.uuid4());models=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                base=q.require_ok(service.command('load',output=out/f'{index}-{tag}-load',path=str(seed),document_id=ident))
                model=json.loads(Path(base['model']).read_text('utf8'))
                response=q.require_ok(service.command('advanced',output=out/f'{index}-{tag}-rotate',operation='rotate',kind='point',ids=list(e.points(model)),vector=angles,adjust_markers=True))
                models.append(json.loads(Path(response['model']).read_text('utf8')))
            r.case('transforms.quadrant-probe-'+str(index),modeling.geometry(models[0])==modeling.geometry(models[1]),angles=angles)


def run(r,runtime,source,out):
    out.mkdir(parents=True);seed=out/'rich-5.0.fbm';seed_fields=fixture(seed);timings=[]
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native, q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0;current=None
        def both(label,method='snapshot',**fields):
            nonlocal step,current
            step+=1;results=[]
            for tag,s in [('native',native),('oracle',oracle)]:
                response=q.require_ok(s.command(method,output=out/f'{step:03}-{label}'/tag,**fields))
                results.append((response,json.loads(Path(response['model']).read_text('utf8'))))
            (a,am),(b,bm)=results;ag,bg=modeling.geometry(am),modeling.geometry(bm);prefix=f'transforms.{step:03}-{label}'
            r.case(prefix+'.complete-model',ag==bg,different_fields=[k for k in ag if ag[k]!=bg.get(k)])
            r.case(prefix+'.state',all(a[k]==b[k] for k in q.STATE_KEYS))
            for key in ('scene','dxf','sections'):r.compare(prefix+'.'+key,a[key],b[key])
            if method=='advanced':
                r.case(prefix+'.native-dispatch',a.get('edit_engine')=='cpp-transforms' and a.get('fbm_write_engine')=='cpp-transform-writer')
                timings.append({'operation':fields['operation'],'edit_ms':a.get('transform_edit_ms'),'total_ms':a.get('document_total_ms')})
            current=am;return am
        def reload(path=seed):return both('load','load',path=str(path),document_id=str(uuid.uuid4()))
        def edit(op,kind,ids,vector,**extra):return both(op+'-'+kind,'advanced',operation=op,kind=kind,ids=ids,vector=vector,**extra)
        def history(label,before,after):
            undone=both(label+'-undo','undo');r.case('transforms.'+label+'.undo',modeling.geometry(before)==modeling.geometry(undone))
            redone=both(label+'-redo','redo');r.case('transforms.'+label+'.redo',modeling.geometry(after)==modeling.geometry(redone))
        def reject(label,**fields):
            for tag,s in [('native',native),('oracle',oracle)]:
                old=dict(s.state);response=s.command('advanced',output=out/('reject-'+label+'-'+tag),**fields)
                r.case('transforms.reject-'+label+'.'+tag,not response['ok'] and s.state==old,error=response.get('error'))
        def save_aux(label):
            states=[]
            for tag,s in [('native',native),('oracle',oracle)]:
                response=q.require_ok(s.command('save',output=out/('save-'+label+'-'+tag)));states.append(observe(Path(response['saved_file'])))
            a,b=states
            for key in ('settings','hydro_bytes','markers','marker_attributes','planes','calculations'):
                r.case('transforms.'+label+'.saved-original-'+key,a[key]==b[key])
            # The old loader normalizes otherwise-unused record padding. Compare
            # actual semantic flags with it; require native preservation of the
            # supplied padding separately against the pre-edit original bytes.
            r.case('transforms.'+label+'.saved-original-calculator-flags',all(a['calculation_bytes'][n][-4:][:flags]==b['calculation_bytes'][n][-4:][:flags] for n,_,flags in RECORDS))
            return a
        def math_check(label,before,after,selected,op,v,pivot=(0,0,0)):
            expected=analytic(e.points(before),selected,op,v,pivot);actual=e.points(after)
            good=actual==expected if op!='rotate' else all(math.isclose(a,b,rel_tol=2e-6,abs_tol=3e-5) for i,p in expected.items() for a,b in zip(actual[i],p))
            r.case('transforms.'+label+'.independent-math',good)
            r.case('transforms.'+label+'.unselected-fixed',all(actual[i]==p for i,p in e.points(before).items() if i not in selected))

        base=reload(source)
        movable=[p['id'] for p in base['control_points'] if p['batch_movable']]
        if movable:
            after=edit('translate','point',movable[:2],[.1234567,.2345678,.3456789]);math_check('supplied',base,after,set(movable[:2]),'translate',[.1234567,.2345678,.3456789]);history('supplied',base,after)
        base=reload();start=save_aux('seed')
        r.case('transforms.fixture.nonzero-all-ten-records',len(SCALED)==10 and all(any(start['calculations'][n]) for n in SCALED))
        r.case('transforms.fixture.known-persistent-settings',start['hydro']==seed_fields['hydro'] and start['markers']==seed_fields['markers'])
        for kind,key in [('point','control_points'),('edge','control_edges'),('face','control_faces'),('curve','control_curves')]:
            for op,v in [('translate',[.1234567,.2345678,.3456789]),('scale',[1.1234567,.8765432,1.2345678]),('rotate',[17.125,-23.5,41.75])]:
                base=reload();entity=base[key][0];selection={entity['id']} if kind=='point' else ({entity['a'],entity['b']} if kind=='edge' else set(entity['controls' if kind=='curve' else 'points']))
                pivot=[.375,-.125,1.25];after=edit(op,kind,[entity['id']],v,pivot=pivot)
                math_check(kind+'-'+op,base,after,selection,op,v,pivot)
                actual=save_aux(kind+'-'+op)
                for field in ('settings','markers','calculation_bytes'):
                    r.case('transforms.partial-'+kind+'-'+op+'.fixed-'+field,actual[field]==start[field])

        for label,op,v,adjust,pivot in [
            ('translation','translate',[.1234567,.2345678,.3456789],True,[0,0,0]),
            ('scale','scale',[1.1234567,.8765432,1.2345678],True,[0,0,0]),
            ('scale-fixed-markers','scale',[.75,1.25,1.5],False,[0,0,0]),
            ('quarter-turn','rotate',[0,0,90],True,[0,0,0]),
            ('multi-axis-pivot','rotate',[17.125,-23.5,41.75],True,[.375,-.125,1.25]),
            ('extreme-angle','rotate',[360000,-360000,270],False,[0,0,0])]:
            base=reload();ids=list(e.points(base));after=edit(op,'point',ids,v,adjust_markers=adjust,pivot=pivot)
            math_check(label,base,after,set(ids),op,v,pivot);actual=save_aux(label)
            r.case('transforms.'+label+'.persistent-hydro-unchanged',actual['hydro_bytes']==start['hydro_bytes'])
            r.case('transforms.'+label+'.marker-attributes-unchanged',actual['marker_attributes']==start['marker_attributes'])
            for name,old in start['calculations'].items():
                expected=expected_calculation(name,old,v) if op=='scale' else old
                r.case('transforms.'+label+'.parameter-'+name,actual['calculations'][name]==expected)
                r.case('transforms.'+label+'.flags-padding-'+name,actual['calculation_bytes'][name][-4:]==start['calculation_bytes'][name][-4:])
            if op=='scale':
                r.case('transforms.'+label+'.project-dimensions',actual['dimensions']==[f32(a*f32(b)) for a,b in zip(start['dimensions'],v)] and actual['particulars']==1)
                r.case('transforms.'+label+'.explicit-split',actual['split']==f32(start['split']*f32(v[0])))
            if op=='translate':
                planes=[]
                for plane in start['planes']:
                    dot=f32(f32(f32(plane[0]*f32(v[0]))+f32(plane[1]*f32(v[1])))+f32(plane[2]*f32(v[2])))
                    planes.append(plane[:3]+[f32(plane[3]-dot)])
                r.case('transforms.translation.section-plane-formula',actual['planes']==planes)
                r.case('transforms.translation.explicit-split',actual['split']==f32(start['split']+f32(v[0])))
            else:r.case('transforms.'+label+'.section-planes-fixed',actual['planes']==start['planes'])
            if not adjust:r.case('transforms.'+label+'.unchecked-markers-fixed',actual['markers']==start['markers'])
            elif op!='rotate':
                expected=[[[f32(a+f32(b)) if op=='translate' else f32(a*f32(b)) for a,b in zip(p,v)] for p in marker] for marker in start['markers']]
                r.case('transforms.'+label+'.marker-formula',actual['markers']==expected)
            if label=='quarter-turn':r.case('transforms.rotation.signed-negative-y',any(p[1]<0 for p in e.points(after).values()))
            # Saving defines a clean revision on both sides; undo/redo still
            # compares each complete state and ancillary payload.
            history(label,base,after)
            reopened=q.require_ok(native.command('save',output=out/('reopen-save-'+label)))
            reloaded=both(label+'-reopen','load',path=reopened['saved_file'],document_id=str(uuid.uuid4()))
            r.case('transforms.'+label+'.save-reopen',nla.reopened_geometry(reloaded)==nla.reopened_geometry(after))

        tiny=out/'origin-only.fbm';fixture(tiny,degenerate=True);base=reload(tiny);old=dict(native.state)
        after=edit('scale','point',list(e.points(base)),[1.25,1.25,1.25])
        r.case('transforms.origin-only.nonunit-scale-is-edit',native.state['revision']==old['revision']+1 and native.state['can_undo'] and native.state['modified'])
        r.case('transforms.origin-only.values-stay-equal',modeling.geometry(base)==modeling.geometry(after))
        history('origin-only',base,after)

        base=reload();ids=list(e.points(base));common=dict(operation='translate',kind='point',ids=[ids[0]],vector=[1,2,3])
        for label,extra in [('duplicate',{'ids':[ids[0],ids[0]]}),('empty',{'ids':[]}),('missing',{'ids':[1999999]}),
                ('fractional',{'ids':[1.25]}),('vector-type',{'vector':[1,True,3]}),('overflow',{'vector':[1e10,0,0]}),
                ('zero-scale',{'operation':'scale','vector':[1,0,1]}),('negative-scale',{'operation':'scale','vector':[-1,1,1]}),
                ('underflow-scale',{'operation':'scale','vector':[1e-100,1,1]}),('angle-limit',{'operation':'rotate','vector':[360001,0,0]}),
                ('marker-subset',{'adjust_markers':True}),('marker-type',{'adjust_markers':1}),('pivot-length',{'pivot':[0,0]}),
                ('pivot-limit',{'pivot':[0,1e10,0]}),('pivot-type',{'pivot':[0,'1',0]}),('selection-kind',{'kind':'layer'})]:
            reject(label,**{**common,**extra})
        both('atomic-after-invalid')
        for op,v in [('translate',[0,0,0]),('scale',[1,1,1]),('rotate',[0,0,0])]:
            old=dict(native.state);edit(op,'point',ids,v,adjust_markers=True);r.case('transforms.noop-'+op,native.state==old)
        locked=both('lock','topology',operation='set_locked',ids=[ids[0]],locked=True)
        reject('locked',**common);both('unlock','undo')
        anchored=both('anchor','modeling',operation='anchor',ids=ids[:2],hard=False)
        for i in ids[:2]:reject('anchor-'+str(i),**{**common,'ids':[i]})
        both('unanchor','undo')
        both('linear-constraint','modeling',operation='linear_constraint',ids=ids[:3])
        for i in ids[:3]:reject('linear-'+str(i),**{**common,'ids':[i]})
        base=reload();before=modeling.geometry(base);old=dict(native.state);blocked=out/'blocked';blocked.mkdir();(blocked/'model.json').mkdir()
        response=native.command('advanced',output=blocked,**common)
        r.case('transforms.failed-publication.atomic',not response['ok'] and native.state==old)
        after=both('after-publication-failure');r.case('transforms.failed-publication.model',modeling.geometry(after)==before)
        for op in ('translate','scale','rotate'):
            response=oracle.command('advanced',output=out/('forbidden-'+op),external_document=True,**{**common,'operation':op})
            r.case('transforms.forbidden-legacy-'+op,not response['ok'] and 'C++' in response.get('error',''))
    r.data['native_transform_timings']=timings


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);p.add_argument('--quadrants-only',action='store_true');a=p.parse_args();r=q.Report()
    try:
        if a.quadrants_only:probe_quadrants(r,a.runtime.resolve(),a.output.resolve())
        else:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as ex:r.case('transforms.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.finish();a.output.mkdir(parents=True,exist_ok=True);(a.output/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2),'utf8');print(r.data['counts'])
    raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
