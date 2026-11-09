"""Model interchange: real codecs, independent formats/geometry, transactions and Qt actions.

Fixture expectations come from explicit coordinates and file grammar, never from
the production writer. All inputs/outputs live in the requested new report folder.
"""
import argparse,copy,json,math,struct,subprocess,uuid
from collections import Counter
from pathlib import Path
import qt_migration as q
import qt_editing as e

FILES=('qt_exchange.py','qt/exchange_tests.cpp','qt_migration.py','run_tests.py')
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def dump(p,data):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
def area(tri):
    a,b,c=tri;u=[b[i]-a[i] for i in range(3)];v=[c[i]-a[i] for i in range(3)]
    return math.sqrt(sum(x*x for x in (u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])))/2
def normal(tri):
    a,b,c=tri;u=[b[i]-a[i] for i in range(3)];v=[c[i]-a[i] for i in range(3)];n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]);s=math.sqrt(sum(x*x for x in n));return tuple(x/s for x in n) if s else (0,0,0)
def oriented(tri):
    t=tuple(tuple(p) for p in tri);return min(t,t[1:]+t[:1],t[2:]+t[:2])
def expected_triangles(model,full=True,hidden=False,scale=1,legacy=False,binary=False):
    layers={l['id']:l for l in model['layers']};out=[]
    def cv(p):
        p=tuple(x*scale for x in p);p=(p[1],p[2],p[0]) if legacy else p
        return tuple(f32(x) for x in p) if binary else p
    for t in model['triangles']:
        l=layers[t['layer']]
        if not hidden and not l['visible']:continue
        out.append(tuple(cv(p) for p in t['p']))
        if full and l['symmetric'] and any(p[1]!=0 for p in t['p']):out.append(tuple(cv((p[0],-p[1],p[2])) for p in reversed(t['p'])))
    return out
def obj_reader(path):
    vertices=[];faces=[];groups=[];group=''
    for line in path.read_text('utf8').splitlines():
        if not line or line.startswith('#'):continue
        row=line.split();kind=row.pop(0)
        if kind=='v':assert len(row)==3;vertices.append(tuple(map(float,row)))
        elif kind=='g':group=' '.join(row)
        elif kind=='f':
            ids=list(map(int,row));assert len(ids)>=3 and all(0<i<=len(vertices) for i in ids)
            faces.append(tuple(vertices[i-1] for i in ids));groups.append(group)
        else:raise ValueError('Unexpected OBJ record '+kind)
    return faces,groups
def stl_reader(path):
    data=path.read_bytes();tri=[];normals=[]
    if len(data)>=84 and len(data)==84+50*struct.unpack_from('<I',data,80)[0]:
        for offset in range(84,len(data),50):
            r=struct.unpack_from('<12fH',data,offset);normals.append(r[:3]);tri.append((r[3:6],r[6:9],r[9:12]))
    else:
        lines=data.decode().splitlines();assert lines[0].startswith('solid') and lines[-1].startswith('endsolid')
        for i in range(1,len(lines)-1,7):
            row=[s.split() for s in lines[i:i+7]];assert row[0][:2]==['facet','normal'] and row[1]==['outer','loop'] and row[5]==['endloop'] and row[6]==['endfacet']
            normals.append(tuple(map(float,row[0][2:])));tri.append(tuple(tuple(map(float,p[1:])) for p in row[2:5]))
    return tri,normals
def fef_reader(path):
    it=iter(path.read_text('utf8').splitlines());title,designer,creator,comment=[next(it) for _ in range(4)];settings=next(it).split();layers=[]
    for _ in range(int(next(it))):layers.append((next(it),next(it).split()))
    pts=[list(map(float,next(it).split())) for _ in range(int(next(it)))];edges=[list(map(int,next(it).split())) for _ in range(int(next(it)))];faces=[list(map(int,next(it).split())) for _ in range(int(next(it)))];assert not list(it)
    return {'text':(title,designer,creator,comment),'settings':settings,'layers':layers,'points':pts,'edges':edges,'faces':faces}
def fef_fixture():
    # Distinct flags, feet, tiny coordinate and material data. The old loader
    # forced symmetry true; the expected value below is explicitly false.
    return ('Exchange\nDesigner\nAuthor\ncomment\n4.1234564781188965 2.5 0.75 0.0019899999182671309 1.0123456716537476 1 -1 0\n'
            '1\nAsymmetric\n12 1193046 -1 0 0 -1 0 -1 7.8500003814697266 0.00009999999747378752\n'
            '4\n0.0000009999999974752427 0 0 3 0\n4 0 0 3 0\n4 2 0 3 0\n0 2 0 3 0\n'
            '4\n0 1 1 0\n1 2 1 0\n2 3 1 0\n3 0 1 0\n1\n4 0 1 2 3 0 0\n')
def fixtures(folder):
    folder.mkdir();requests=[]
    def add(name,fmt,data,ok=True,options=None):
        path=folder/(name+'.'+fmt);path.write_bytes(data if isinstance(data,bytes) else data.encode('utf8'))
        requests.append({'name':name,'method':'read','path':str(path),'format':fmt,'expected_ok':ok,'options':options or {}})
    square='v 0 0 0\nv 2 0 0\nv 2 1 0\nv 0 1 0\nf 1 2 3 4\n'
    add('square','obj',square);add('control','obj',square,options={'control_net':True})
    add('中文路径','obj',square)
    add('negative','obj','v\t0 0 0\nv 2 0 0\nv 2 1 0\nv 0 1 0\nvt 0 0\nvn 0 0 1\ng deck\nf -4/1/1 -3/1/1 -2/1/1 -1/1/1 # last\n')
    add('concave','obj','v 0 0 0\nv 2 0 0\nv 2 1 0\nv 1 1 0\nv 1 2 0\nv 0 2 0\nf 1 2 3 4 5 6\n')
    add('units-mm','obj',square,options={'units':'mm'});add('units-ft','obj',square,options={'units':'ft'});add('legacy-axes','obj',square,options={'legacy_axes':True})
    add('homogeneous','obj','v 0 0 0 2\nv 4 0 0 2\nv 4 2 0 2\nf 1 2 3\n')
    add('nearby','obj','v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 0.000001\nv 1 0 0.000001\nv 0 1 0.000001\nf 1 2 3\nf 4 5 6\n')
    for name,data in {'zero-index':square.replace('f 1','f 0'),'out-of-range':square.replace('f 1','f 5'),'negative-overflow':square.replace('f 1','f -5'),
                      'bad-number':square.replace('v 2','v nan',1),'duplicate-vertex':square.replace('f 1 2 3 4','f 1 2 2'),
                      'duplicate-face':square+'f 4 3 2 1\n','self-crossing':square.replace('f 1 2 3 4','f 1 3 2 4'),
                      'truncated':'v 0 0','empty':'# no faces\n','unsupported':square+'curv 0 1 1 2\n','utf8':b'v \xff\n',
                      'nonplanar':square.replace('v 2 1 0','v 2 1 1'),
                      'nonmanifold':'v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 -1 0\nv 0 0 1\nf 1 2 3\nf 2 1 4\nf 1 2 5\n'}.items():add(name,'obj',data,False)
    ascii_stl='solid\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 2 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid\n'
    add('ascii','stl',ascii_stl);add('ascii-truncated','stl',ascii_stl.replace('endfacet',''),False)
    binary=b'solid binary header'.ljust(80,b'\0')+struct.pack('<I12fH',1,0,0,1,0,0,0,2,0,0,0,1,0,0)
    add('binary-solid','stl',binary);add('binary-truncated','stl',binary[:-1],False);add('binary-huge-count','stl',binary[:80]+struct.pack('<I',2**32-1)+binary[84:],False)
    nans=bytearray(binary);struct.pack_into('<f',nans,96,float('nan'));add('binary-nan','stl',bytes(nans),False)
    add('binary-zero-area','stl',binary[:84]+struct.pack('<12fH',*([0]*12),0),False)
    fef=fef_fixture();add('exchange','fef',fef)
    add('fef-missing-edge','fef',fef.replace('3 0 1 0\n1\n','3 1 1 0\n1\n'),False)
    add('fef-bad-units','fef',fef.replace(' 1 -1 0\n',' 9 -1 0\n',1),False)
    add('fef-face-index','fef',fef.replace('4 0 1 2 3 0 0','4 0 1 2 99 0 0'),False)
    add('fef-tail','fef',fef+'unknown\n',False);add('fef-truncated','fef',fef[:80],False)
    return requests
def probe(executable,requests,folder):
    folder.mkdir(parents=True,exist_ok=True);manifest=folder/'request.json';output=folder/'results.json';dump(manifest,requests)
    p=subprocess.run([str(executable),str(manifest),str(output)],cwd=folder,env=q.private_environment(folder),capture_output=True,creationflags=q.common.FLAGS,timeout=120)
    (folder/'process.log').write_bytes(p.stdout+p.stderr)
    if p.returncode:raise RuntimeError('Codec probe failed '+str(p.returncode))
    return {item['name']:item for item in json.loads(output.read_text('utf8'))}
def codec_checks(r,runtime,out):
    requests=fixtures(out/'fixtures');values=probe(runtime/'qt_exchange_tests.exe',requests,out/'codecs')
    for c in requests:r.case('parse.'+c['name'],values[c['name']]['ok']==c['expected_ok'],result=values[c['name']].get('error'))
    def data(name):return values[name]['data']
    for name,expected in (('square',2),('negative',2),('concave',3),('homogeneous',1),('ascii',1),('binary-solid',1)):
        d=data(name);p=[x['p'] for x in d['points']];tri=[tuple(p[i] for i in f['points']) for f in d['faces']]
        r.case(name+'.area',abs(sum(area(t) for t in tri)-expected)<1e-12)
        r.case(name+'.winding',all(normal(t)==(0,0,1) for t in tri))
    d=data('concave');p=[x['p'] for x in d['points']]
    r.case('concave.missing-quadrant-preserved',all(not(sum(p[i][0] for i in f['points'])/3>1 and sum(p[i][1] for i in f['points'])/3>1) for f in d['faces']))
    r.case('obj.control-polygon-kept',len(data('control')['faces'])==1 and len(data('control')['faces'][0]['points'])==4 and all(not e[2] for e in data('control')['edges']))
    r.case('mesh.flat-edges-and-corners',all(e[2] for e in data('square')['edges']) and all(p['vertex_type']==3 for p in data('square')['points']))
    r.case('units.mm-to-metres',data('units-mm')['points'][1]['p'][0]==f32(.002))
    r.case('units.ft-kept',data('units-ft')['project']['units']=='imperial' and data('units-ft')['points'][1]['p'][0]==2)
    r.case('units.ft-density',data('units-ft')['project']['water_density']==f32(f32(1.025)*1000*.3048**3/.45359237))
    r.case('axes.explicit-legacy',data('legacy-axes')['points'][1]['p']==[0,2,0])
    r.case('nearby.no-tolerance-weld',len(data('nearby')['points'])==6 and data('nearby')['points'][3]['p'][2]==f32(.000001))
    r.case('fef.asymmetry-preserved',data('exchange')['layers'][0]['symmetric'] is False)
    r.case('fef.precision-preserved',data('exchange')['points'][0]['p'][0]==f32(.000001) and data('exchange')['project']['length']==f32(4.1234564781188965))
    r.case('fef.metadata-preserved',data('exchange')['creator']=='Author' and data('exchange')['particulars_set'] and data('exchange')['project']['units']=='imperial')
    return requests,values
def geometry_subset(model):
    points={p['id']:tuple(p['p']) for p in model['control_points']};layers={l['id']:{k:v for k,v in l.items() if k!='id'} for l in model['layers']}
    return {'points':Counter((tuple(p['p']),p['vertex_type']) for p in model['control_points']),
            'edges':Counter((tuple(sorted((points[e['a']],points[e['b']]))),e['crease']) for e in model['control_edges']),
            'faces':Counter((tuple(points[i] for i in f['points']),json.dumps(layers[f['layer']],sort_keys=True)) for f in model['control_faces'])}
def compare_geometry(r,key,a,b,path):
    actual,expected=geometry_subset(a),geometry_subset(b);differences={}
    for field in actual:
        if actual[field]!=expected[field]:differences[field]={'extra':list((actual[field]-expected[field]).items()),'missing':list((expected[field]-actual[field]).items())}
    if differences:dump(path,differences)
    r.case(key,not differences,difference_file=str(path) if differences else None)
def service_checks(r,exe,values,out):
    with q.Service(exe,out/'process') as s:
        first=s.command('load',path=str(q.DEFAULT_MODELS[0]),document_id=str(uuid.uuid4()),output=out/'original');q.require_ok(first);original=dict(s.state)
        d=values['exchange']['data'];path=out/'exchange.json';dump(path,d)
        stale=s.command('import_model',path=str(path),document_id=str(uuid.uuid4()),expected_revision=0,output=out/'stale');r.case('service.stale-import-rejected',not stale['ok'] and s.state==original)
        for name,mutate in (('bad-index',lambda x:x['faces'][0]['points'].__setitem__(0,9999)),('bad-units',lambda x:x['project'].__setitem__('units','unknown')),
                            ('missing-edge',lambda x:x['edges'].pop()),('bad-text',lambda x:x['project'].__setitem__('name','中文不可丢失'))):
            corrupt=copy.deepcopy(d);mutate(corrupt);p=out/(name+'.json');dump(p,corrupt)
            res=s.command('import_model',path=str(p),document_id=str(uuid.uuid4()),output=out/name);r.case('service.'+name+'.preserves-document',not res['ok'] and s.state==original,error=res.get('error'))
        after=s.command('snapshot',output=out/'after-failure');q.require_ok(after)
        for kind in ('scene','dxf'):r.compare('service.failure-'+kind,after[kind],first[kind])
        for name in ('exchange','square','nearby','concave','binary-solid','units-mm','units-ft','中文路径'):
            p=out/(name+'-valid.json');dump(p,values[name]['data']);res=s.command('import_model',path=str(p),document_id=str(uuid.uuid4()),output=out/name/'import');q.require_ok(res)
            r.case(name+'.import-unsaved',s.state['modified'] and not s.state['can_undo']);model=json.loads(Path(res['model']).read_text('utf8'))
            r.case(name+'.point-count',len(model['control_points'])==len(values[name]['data']['points']))
            r.case(name+'.point-coordinates',Counter(tuple(p['p']) for p in model['control_points'])==Counter(tuple(p['p']) for p in values[name]['data']['points']))
            if name=='exchange':r.case('fef.service-flags',model['layers'][0]['symmetric'] is False and model['settings']['created_by']=='Author' and model['units']=='imperial')
            if name=='concave':r.case('concave.subdivision-area',abs(sum(area(t['p']) for t in model['triangles'])-3)<1e-6)
            point=model['control_points'][0];moved=list(point['p']);moved[0]+=.125
            edit=s.command('move_point',point_id=point['id'],position=moved,output=out/name/'move');q.require_ok(edit)
            undo=s.command('undo',output=out/name/'undo');q.require_ok(undo);undo_model=json.loads(Path(undo['model']).read_text('utf8'))
            r.case(name+'.editable-undo',geometry_subset(model)==geometry_subset(undo_model))
            saved=s.command('save',output=out/name/'save');q.require_ok(saved)
            reopened=s.command('load',path=saved['saved_file'],document_id=str(uuid.uuid4()),output=out/name/'reopen');q.require_ok(reopened)
            r.case(name+'.fbm-roundtrip',geometry_subset(model)==geometry_subset(json.loads(Path(reopened['model']).read_text('utf8'))))
    return first
def export_checks(r,runtime,source_model,out):
    out.mkdir();model=json.loads(Path(source_model).read_text('utf8'));requests=[]
    for name,fmt,options in (('surface','obj',{}),('control','obj',{'control_net':True}),('half','obj',{'full_hull':False}),
                             ('axes','obj',{'legacy_axes':True}),('millimetres','obj',{'units':'mm'}),('all-layers','obj',{'include_hidden':True}),
                             ('binary','stl',{}),('ascii','stl',{'binary_stl':False}),('exchange','fef',{}),('camera','obj',{})):
        requests.append({'name':name,'method':'write','model':str(source_model),'path':str(out/(name+'.'+fmt)),'format':fmt,'options':options,'camera_half':name=='camera'})
    result=probe(runtime/'qt_exchange_tests.exe',requests,out/'probe')
    for c in requests:r.case('export.'+c['name'],result[c['name']]['ok'],error=result[c['name']].get('error'))
    for name,fmt,opts in [(c['name'],c['format'],c['options']) for c in requests if c['name'] not in ('control','exchange')]:
        if not result[name]['ok']:continue
        binary=fmt=='stl' and opts.get('binary_stl',True);expected=expected_triangles(model,opts.get('full_hull',True),opts.get('include_hidden',False),1000 if name=='millimetres' else 1,name=='axes',binary)
        if fmt=='obj':actual,_=obj_reader(out/(name+'.obj'))
        else:
            actual,normals=stl_reader(out/(name+'.stl'));r.case(name+'.normal-winding',all(max(abs(a-b) for a,b in zip(n,normal(tri)))<2e-6 for n,tri in zip(normals,actual)))
        r.case(name+'.all-triangles-exact',Counter(oriented(t) for t in actual)==Counter(oriented(t) for t in expected),expected=len(expected),actual=len(actual))
    r.case('camera.export-independent',(out/'camera.obj').read_bytes()==(out/'surface.obj').read_bytes())
    fef=fef_reader(out/'exchange.fef');r.case('fef.export-points-exact',[p[:3] for p in fef['points']]==[p['p'] for p in model['control_points']])
    r.case('fef.export-all-layers',len(fef['layers'])==len(model['layers']) and all(int(raw[1][4])==(-1 if layer['symmetric'] else 0) for raw,layer in zip(fef['layers'],model['layers'])))
    ctrl,groups=obj_reader(out/'control.obj');pmap={p['id']:p['p'] for p in model['control_points']};ls={l['id']:l for l in model['layers']};wanted=[]
    for f in model['control_faces']:
        if not ls[f['layer']]['visible']:continue
        poly=tuple(tuple(pmap[i]) for i in f['points']);wanted.append(poly)
        if ls[f['layer']]['symmetric'] and any(p[1]!=0 for p in poly):wanted.append(tuple((x,-y,z) for x,y,z in reversed(poly)))
    r.case('obj.control-topology',ctrl==wanted)
    keep=out/'preserved.obj';keep.write_bytes(b'original');failed=probe(runtime/'qt_exchange_tests.exe',[{'name':'failed','method':'write','model':str(source_model),'path':str(keep),'format':'bad'}],out/'failure')
    r.case('export.failure-preserves-file',not failed['failed']['ok'] and keep.read_bytes()==b'original')
    return out/'exchange.fef'
def production(r,runtime,model,fixtures,out):
    out.mkdir(parents=True);qt=out/'qt';qt.mkdir();commands=[]
    for fmt in ('obj','stl','fef'):commands.append({'name':'export-'+fmt,'method':'export','format':fmt,'path':str(qt/('result.'+fmt))})
    commands += [{'name':'bad-import','method':'import','format':'obj','path':str(fixtures/'out-of-range.obj'),'expect_failure':True},
                 {'name':'import-fef','method':'import','format':'fef','path':str(qt/'result.fef')},
                 {'name':'save-fef','method':'save','path':str(qt/'imported.fbm')},
                 {'name':'reopen-fef','method':'reopen','path':str(qt/'imported.fbm')},
                 {'name':'import-obj','method':'import','format':'obj','path':str(fixtures/'square.obj')},
                 {'name':'save-obj','method':'save','path':str(qt/'mesh.fbm')},
                 {'name':'import-stl','method':'import','format':'stl','path':str(fixtures/'binary-solid.stl')}]
    req=out/'request.json';dump(req,{'model':str(model),'commands':commands});child=q.run_child(r,'production',runtime/'FreeShipQt.exe',['--verify-exchange',req],qt,timeout=240,platform='windows')
    if not child:return
    art=child['artifacts'];original=json.loads(Path(art['original_model']).read_text('utf8'))
    for phase in ('export-obj','export-stl','export-fef','bad-import'):
        for kind in ('scene','dxf'):r.compare(phase+'.unchanged-'+kind,art[phase+'_'+kind],art['original_'+kind])
    for phase in ('import-fef','reopen-fef'):
        current=json.loads(Path(art[phase+'_model']).read_text('utf8'));compare_geometry(r,out.name+'.'+phase+'.control-net-and-layers',current,original,out/(phase+'-differences.json'))
        r.case(out.name+'.'+phase+'.derived-surface-exact',Counter(oriented(t['p']) for t in current['triangles'])==Counter(oriented(t['p']) for t in original['triangles']))
    for fmt in ('obj','stl'):
        actual=obj_reader(qt/'result.obj')[0] if fmt=='obj' else stl_reader(qt/'result.stl')[0]
        expected=expected_triangles(original,binary=fmt=='stl');r.case('production.'+fmt+'.exact-export',Counter(oriented(t) for t in actual)==Counter(oriented(t) for t in expected))
    child['artifacts']['fixture_model']=str(model);r.data['models'].append({'name':'exchange','sha256':q.common.digest_file(model),'production':child})

def empty_start(r,runtime,fixtures,out):
    out.mkdir();req=out/'request.json';dump(req,{'commands':[{'name':'direct-import','method':'import','format':'obj','path':str(fixtures/'中文路径.obj')},{'name':'saved','method':'save','path':str(out/'new.fbm')},{'name':'reopened','method':'reopen','path':str(out/'new.fbm')}]})
    q.run_child(r,'empty-start',runtime/'FreeShipQt.exe',['--verify-exchange',req],out/'qt',timeout=120,platform='windows')

def mixed_layers(r,runtime,model_path,out):
    out.mkdir();m=json.loads(model_path.read_text('utf8'));base=copy.deepcopy(m['layers'][0]);base['id']=1;base['symmetric']=False;base['visible']=True;base['name']='Asymmetric'
    symmetric=dict(base,id=2,symmetric=True,name='Symmetric');hidden=dict(base,id=3,visible=False,name='Hidden');m['layers']=[base,symmetric,hidden]
    for f in m['control_faces']:f['layer']=1
    m['triangles']=[{'layer':l,'p':points} for l,points in (
        (1,[[0,2,0],[1,2,0],[0,3,0]]),(2,[[2,1,0],[3,1,0],[2,2,0]]),
        (2,[[4,-1,0],[5,-1,0],[4,-2,0]]),(2,[[6,0,0],[7,0,0],[6,0,1]]),
        (2,[[8,f32(1e-7),0],[9,f32(1e-7),0],[8,f32(1e-7),1]]),(3,[[10,4,0],[11,4,0],[10,5,0]]))]
    path=out/'model.json';dump(path,m);export_checks(r,runtime,path,out/'exports')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT)
    p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data['suite']='qt-exchange';before=q.common.fingerprint(a.source)[0]
    r.data['limitations'][0]='C++ codecs and document creation with a Pascal analysis adapter; the service and Qt import tests use the native document owner.'
    hashes={n:q.common.digest_file(q.ROOT/n) for n in FILES};r.data['test_sources_sha256']=hashes;r.data['source_sha256']=before;r.data['qt_sources_sha256']=q.current_sources(a.qt_source)
    protected={str(f):q.common.digest_file(f) for f in [*q.DEFAULT_MODELS,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe'] if f.exists()}
    try:
        if a.verify_deployment:
            runtime=a.verify_deployment.resolve();manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
            if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=q.current_sources(a.qt_source):raise RuntimeError('Installed source manifest is stale')
            for n,h in manifest['binaries_sha256'].items():
                if q.common.digest_file(runtime/n)!=h:raise RuntimeError('Installed binary mismatch '+n)
            r.data['runtime_path']=str(runtime);r.data['runtime_sha256']={n:q.common.digest_file(runtime/n) for n in manifest['binaries_sha256']}
        else:
            bridge=q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),r);runtime=q.prepare_runtime(a,out,bridge,r)
        requests,values=codec_checks(r,runtime,out)
        first=service_checks(r,runtime/'FreeShipDocumentService.exe',values,out/'service')
        fef=export_checks(r,runtime,Path(first['model']),out/'exports')
        reread=probe(runtime/'qt_exchange_tests.exe',[{'name':'fef','method':'read','format':'fef','path':str(fef)}],out/'fef-reimport');r.case('exported-fef-readable',reread['fef']['ok'])
        production(r,runtime,q.DEFAULT_MODELS[0],out/'fixtures',out/'production')
        production(r,runtime,out/'service/exchange/save/saved.fbm',out/'fixtures',out/'production-feet')
        production(r,runtime,q.DEFAULT_MODELS[2],out/'fixtures',out/'production-real')
        empty_start(r,runtime,out/'fixtures',out/'empty-start')
        mixed_layers(r,runtime,Path(first['model']),out/'mixed-layers')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==before);r.case('qt-source-unchanged',q.current_sources(a.qt_source)==r.data['qt_sources_sha256']);r.case('test-sources-unchanged',all(q.common.digest_file(q.ROOT/n)==h for n,h in hashes.items()))
        for f,h in protected.items():r.case('protected.'+f,q.common.digest_file(Path(f))==h)
        r.case('visual-fidelity',status='review',reason='Hidden Windows dialog evidence, not complete original-window visual acceptance')
        r.case('desktop',status='not_run',reason='No desktop automation, native chooser, mouse/focus or external CAD product acceptance')
        e.write_report(r,out)
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'report':str(out/'report.json')}));return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]
if __name__=='__main__':raise SystemExit(main())
