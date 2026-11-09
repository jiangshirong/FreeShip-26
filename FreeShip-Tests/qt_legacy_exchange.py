"""Independent VRML/part/wave fixtures, append transactions and production Qt routes.

Part fixtures are packed from the legacy wire layout, not the product writer.
Wave fixtures keep a nonzero last column, negative and positive rows, and zero
rows to expose the old reader's truncation and stale-last-value defects.
"""
import argparse,copy,json,math,struct,time,uuid,os,subprocess
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_exchange as x

FILES=('qt_legacy_exchange.py','qt_exchange.py','qt_migration.py','qt/exchange_tests.cpp','run_tests.py')
POINTS=[[0,0,0],[4,0,0],[4,2,0],[0,2,0]]

def part_fixture(units=0,version=21,name='Part fixture'):
    b=bytearray()
    def integer(v):b.extend(struct.pack('<i',v))
    def flag(v):b.extend(struct.pack('<?',v))
    def real(v):b.extend(struct.pack('<f',v))
    def text(s):v=s.encode('cp1252');integer(len(v));b.extend(v)
    text('FREE!ship partfile');integer(version);integer(units);integer(1)
    text(name);integer(99);integer(0x1280ab)
    for v in (True,False,True):flag(v)
    if version>=9:
        flag(True);flag(False)
        if version>=11:
            real(7.8);real(12.7)
            if version>=15:
                flag(False)
                if version>=21:integer(123)
    integer(4)
    for i,p in enumerate(POINTS):
        for v in p:real(v)
        integer(3);flag(False)
        if version>=13:flag(i==2)
    integer(4)
    for a,c in ((0,1),(1,2),(2,3),(3,0)):integer(a);integer(c);flag(True)
    integer(1);integer(4)
    for i in range(4):integer(i)
    integer(0);integer(1);integer(3)
    for i in (0,1,2):integer(i)
    return bytes(b)

def wave_fixture(sector=False):
    rows=['SECTORIAL PATCH' if sector else 'RECTANGULAR PATCH','speed','2.5','extent','1,3,3','transverse','-10,10,3']
    rows+=['comment']*(13 if sector else 5)
    rows+=['coordinate,1,2,3','-10,0,0,0','0,1,2,9','10,-1,-2,-7']
    return '\n'.join(rows)+'\n'

def fixtures(folder):
    folder.mkdir();req=[]
    def add(name,fmt,data,ok=True,options=None):
        path=folder/(name+'.'+dict(part='part',waves='mlt',vrml='wrl')[fmt]);path.write_bytes(data if isinstance(data,bytes) else data.encode())
        req.append(dict(name=name,method='read',format=fmt,path=str(path),expected_ok=ok,options=options or {}))
    for version in (0,9,11,13,15,21,43):add('part-v'+str(version),'part',part_fixture(version=version))
    add('part-feet','part',part_fixture(1));add('part-empty','part',b'',False)
    b=part_fixture()
    for n in (1,4,20,31,len(b)-1):add('part-truncated-'+str(n),'part',b[:n],False)
    add('part-tail','part',b+b'junk',False)
    add('part-future','part',b[:21]+struct.pack('<i',100)+b[25:],False)
    add('part-bad-units','part',b[:25]+struct.pack('<i',2)+b[29:],False)
    add('part-nan','part',b.replace(struct.pack('<f',4),struct.pack('<f',float('nan')),1),False)
    for sector in (False,True):add('sector' if sector else 'rect','waves',wave_fixture(sector))
    wave=wave_fixture();add('wave-tail','waves',wave+'1,2,3,4\n',False);add('wave-truncated','waves','\n'.join(wave.splitlines()[:-1]),False)
    add('wave-nan','waves',wave.replace('0,1,2,9','0,1,2,nan'),False)
    add('wave-huge','waves',wave.replace('1,3,3','1,3,999999'),False)
    add('wave-duplicate-x','waves',wave.replace('coordinate,1,2,3','coordinate,1,1,3'),False)
    header='#VRML V2.0 utf8\n';shape='Shape { geometry IndexedFaceSet { coord Coordinate { point [0 0 0,4 0 0,4 2 0,0 2 0] } coordIndex [0,1,2,3,-1] } }'
    add('vrml','vrml',header+shape)
    add('vrml-transform','vrml',header+'Transform { children ['+shape+'] translation 10 20 30 scale 2 3 4 }')
    add('vrml-rotate','vrml',header+'Transform { center 1 0 0 rotation 0 0 1 1.5707963267948966 children ['+shape+'] }')
    add('vrml-reflection','vrml',header+'Transform { scale -1 1 1 children ['+shape+'] }')
    add('vrml-cw','vrml',header+shape.replace('coordIndex','ccw FALSE coordIndex'))
    add('vrml-use','vrml',header+'DEF Mesh '+shape+' Transform { translation 0 0 2 children [USE Mesh] }')
    add('vrml-v1','vrml','#VRML V1.0 ascii\nSeparator { Translation { translation 0 0 3 } Coordinate3 { point [0 0 0,4 0 0,4 2 0,0 2 0] } IndexedFaceSet { coordIndex [0,1,2,3,-1] } }')
    add('vrml-concave','vrml',header+shape.replace('4 2 0,0 2 0','2 1 0,4 2 0,0 2 0').replace('0,1,2,3,-1','0,1,2,3,4,-1'))
    add('vrml-mm','vrml',header+shape,options=dict(units='mm'))
    add('vrml-control','vrml',header+shape,options=dict(control_net=True))
    for name,data in {'bad-index':shape.replace('0,1,2,3,-1','0,1,99,-1'),'self-intersect':shape.replace('0,1,2,3,-1','0,2,1,3,-1'),'dynamic':'Script {} '+shape,'unknown-geometry':'Shape { geometry Box {} }','missing-use':'USE Missing','cycle':'DEF A Group { children [USE A] }','truncated':shape[:-2],'bad-scale':'Transform { scale 0 1 1 children ['+shape+'] }','bad-number':shape.replace('4 2 0','NaN 2 0')}.items():add('vrml-'+name,'vrml',header+data,False)
    return req

def codec(r,runtime,out):
    req=fixtures(out/'fixtures')
    historical=[]
    for path in sorted((q.common.DEFAULT_SOURCE/'Ships').rglob('*')):
        if path.suffix.lower() not in ('.part','.wrl'):continue
        name='historical-'+str(len(historical));historical.append(dict(name=name,path=str(path),sha256=q.common.digest_file(path)))
        req.append(dict(name=name,method='read',format='part' if path.suffix.lower()=='.part' else 'vrml',path=str(path),options=dict(control_net=True)))
    r.data['historical_files']=historical
    values=x.probe(runtime/'qt_exchange_tests.exe',req,out/'codec')
    for item in req:
        got=values[item['name']]
        if 'expected_ok' in item:r.case('parse.'+item['name'],got['ok']==item['expected_ok'],detail=got.get('error'))
        else:r.case('parse.'+item['name'],status='pass' if got['ok'] else 'review',path=item['path'],reason=got.get('error','Real bundled legacy sample parsed.'))
    r.case('historical-files-protected',all(q.common.digest_file(Path(v['path']))==v['sha256'] for v in historical))
    def d(name):return values[name]['data']
    def points(name):return [p['p'] for p in d(name)['points']]
    r.case('part.coordinates',points('part-v21')==POINTS)
    r.case('part.locked',[p['locked'] for p in d('part-v21')['points']]==[False,False,True,False])
    r.case('part.curves',d('part-v21')['control_curves']==[[0,1,2]])
    r.case('part.layer-attributes',d('part-v21')['layers'][0]['alpha_blend']==123 and not d('part-v21')['layers'][0]['use_in_hydrostatics'])
    r.case('vrml.coordinates',points('vrml')==POINTS)
    r.case('vrml.transform',[p for p in points('vrml-transform')]==[[10+2*x,20+3*y,30+4*z] for x,y,z in POINTS])
    r.case('vrml.rotation',all(abs(a-b)<1e-6 for p,v in zip(points('vrml-rotate'),[[1, -1, 0],[1,3,0],[-1,3,0],[-1,-1,0]]) for a,b in zip(p,v)))
    r.case('vrml.use',points('vrml-use')==POINTS+[[x,y,z+2] for x,y,z in POINTS])
    r.case('vrml.v1-state',points('vrml-v1')==[[x,y,z+3] for x,y,z in POINTS])
    r.case('vrml.mm',points('vrml-mm')==[[x.f32(v*.001) for v in p] for p in POINTS])
    for name,sign in [('vrml',1),('vrml-cw',-1),('vrml-reflection',1)]:
        data=d(name);pts=points(name);r.case(name+'.orientation',all(x.normal([pts[i] for i in face['points']])[2]*sign>0 for face in data['faces']))
    for name,sector in [('rect',False),('sector',True)]:
        expected=[]
        for y,heights in [(-10,[0,0,0]),(0,[1,2,9]),(10,[-1,-2,-7])]:
            for radius,z in zip((1,2,3),heights):
                expected.append([x.f32(radius*math.cos(math.radians(-y))) if sector else radius,x.f32(radius*math.sin(math.radians(-y))) if sector else y,z])
        r.case(name+'.all-grid-values',points(name)==expected)
        l=d(name)['layers'][0];r.case(name+'.excluded-from-hull',not any(l[k] for k in ('symmetric','use_in_hydrostatics','use_for_intersections','show_in_linesplan')))
    return values

def service(r,runtime,values,out):
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process') as s:
        base=q.require_ok(s.command('load',path=str(q.DEFAULT_MODELS[0]),document_id=str(uuid.uuid4()),output=out/'base'))
        original=json.loads(Path(base['model']).read_text('utf8'))
        for name in ('part-v21','part-feet','rect','sector'):
            data=copy.deepcopy(values[name]['data']);data['wave_patch']=name in ('rect','sector');path=out/(name+'.json');x.dump(path,data)
            before=copy.deepcopy(s.state);made=q.require_ok(s.command('append_model',path=str(path),output=out/(name+'-added')));m=json.loads(Path(made['model']).read_text('utf8'))
            r.case(name+'.same-document',s.state['document_id']==before['document_id'] and s.state['revision']>before['revision'] and s.state['can_undo'])
            for key in ('control_points','control_edges','control_faces','layers'):
                n=len(original[key]);r.case(name+'.original-'+key,m[key][:n]==original[key])
            r.case(name+'.project-preserved',m['settings']==original['settings'])
            r.case(name+'.new-points',len(m['control_points'])==len(original['control_points'])+len(data['points']))
            if name in ('rect','sector'):
                for key in ('scene','dxf'):r.compare(name+'.hull-unchanged-'+key,made[key],base[key])
            if name=='part-feet':r.case('part.unit-conversion',[p['p'] for p in m['control_points'][-4:]]==[[x.f32(v*.3048) for v in p] for p in POINTS])
            if name.startswith('part'):r.case(name+'.lock-roundtrip',m['control_points'][-2]['locked'])
            saved=e.saved_copy(s,out/(name+'-save'))
            undone=q.require_ok(s.command('undo',output=out/(name+'-undo')))
            for key in ('scene','dxf'):r.compare(name+'.undo-'+key,undone[key],base[key])
            redone=q.require_ok(s.command('redo',output=out/(name+'-redo')))
            for key in ('scene','dxf'):r.compare(name+'.redo-'+key,redone[key],made[key])
            reopened=q.require_ok(s.command('load',path=str(saved),document_id=str(uuid.uuid4()),output=out/(name+'-reopen')))
            x.compare_geometry(r,name+'.saved',m,json.loads(Path(reopened['model']).read_text('utf8')),out/(name+'-diff.json'))
            base=q.require_ok(s.command('load',path=str(q.DEFAULT_MODELS[0]),document_id=str(uuid.uuid4()),output=out/(name+'-reset')));original=json.loads(Path(base['model']).read_text('utf8'))
        invalid=copy.deepcopy(values['part-v21']['data']);invalid['faces'][0]['points'][0]=999999;path=out/'invalid.json';x.dump(path,invalid);state=copy.deepcopy(s.state)
        response=s.command('append_model',path=str(path),output=out/'invalid');r.case('append.failure-preserves-history',not response['ok'] and s.state==state)
        after=q.require_ok(s.command('snapshot',output=out/'after'))
        for key in ('scene','dxf','model'):r.compare('append.failure-preserves-'+key,after[key],base[key])
        r.case('append.native',after.get('adapter_pid')==0 and after.get('adapter_requests')==0)
        for units in ('metric','imperial'):
            origin_base=q.require_ok(s.command('update_project',changes=dict(units=units),output=out/('origin-'+units+'-base')))
            old=json.loads(Path(origin_base['model']).read_text('utf8'));scale=1 if units=='metric' else 1/.3048
            data=copy.deepcopy(values['rect']['data']);data.update(wave_patch=True,wave_origin_m=dict(x=3,y=-5));path=out/('origin-'+units+'.json');x.dump(path,data)
            made=q.require_ok(s.command('append_model',path=str(path),output=out/('origin-'+units+'-append')));m=json.loads(Path(made['model']).read_text('utf8'))
            actual=[p['p'][:2] for p in m['control_points'][len(old['control_points']):]]
            expected=[[x.f32(3*scale-x.f32(point['p'][0]*scale)),x.f32(-5*scale+x.f32(point['p'][1]*scale))] for point in data['points']]
            r.case('waves.'+units+'.explicit-origin',actual==expected,actual=actual,expected=expected)
            for key in ('scene','dxf'):r.compare('waves.'+units+'.origin-preserves-'+key,made[key],origin_base[key])
            undone=q.require_ok(s.command('undo',output=out/('origin-'+units+'-undo')))
            for key in ('scene','dxf'):r.compare('waves.'+units+'.origin-undo-'+key,undone[key],origin_base[key])
            state=copy.deepcopy(s.state);data['wave_origin_m']['x']='invalid';x.dump(path,data)
            rejected=s.command('append_model',path=str(path),output=out/('origin-'+units+'-invalid'));r.case('waves.'+units+'.invalid-origin-atomic',not rejected['ok'] and s.state==state)
        # Product writer checked by a separately authored reader, including
        # selected-face filtering. The source model has known independent data.
        path=out/'part-source.json';part=copy.deepcopy(values['part-v21']['data']);part['project'].update(length=4,beam=2);x.dump(path,part);made=q.require_ok(s.command('import_model',path=str(path),document_id=str(uuid.uuid4()),output=out/'part-source'))
        target=out/'written.part';result=x.probe(runtime/'qt_exchange_tests.exe',[dict(name='write',method='write',format='part',path=str(target),model=made['model'])],out/'write')
        r.case('part.write',result['write']['ok']);r.case('part.independent-wire-roundtrip',target.read_bytes()==part_fixture(name='Part fixture').replace(struct.pack('<i',99),struct.pack('<i',1),1))

def production(r,runtime,out,fixtures):
    out.mkdir();commands=[]
    for name,fmt,file in [('part','part','part-v21.part'),('waves','waves','rect.mlt')]:
        commands += [dict(name=name,method='import',format=fmt,path=str(fixtures/file)),dict(name=name+'-undo',method='undo'),dict(name=name+'-redo',method='redo')]
    commands += [dict(name='waves-origin',method='import',format='waves',path=str(fixtures/'rect.mlt'),options=dict(references=dict(origin_x=3,origin_y=-5))),dict(name='waves-origin-undo',method='undo')]
    commands += [dict(name='export-part',method='export',format='part',path=str(out/'export.part')),dict(name='vrml',method='import',format='vrml',path=str(fixtures/'vrml-transform.wrl'))]
    req=out/'request.json';x.dump(req,dict(model=str(q.DEFAULT_MODELS[0]),commands=commands))
    child=q.run_child(r,'production',runtime/'FreeShipQt.exe',['--verify-exchange',req],out/'qt',timeout=180,platform='windows');r.data['production']=child
    for name in ('part','waves'):r.case('production.'+name+'.no-error',not child['artifacts'].get(name+'_error'))
    before=json.loads(Path(child['artifacts']['waves-redo_model']).read_text('utf8'));after=json.loads(Path(child['artifacts']['waves-origin_model']).read_text('utf8'))
    points=[p['p'] for p in after['control_points'][len(before['control_points']):]]
    r.case('production.wave-origin',[(p[0],p[1]) for p in points]==[(3-x,-5+y) for y in (-10,0,10) for x in (1,2,3)])
    for key in ('scene','dxf'):r.compare('production.wave-origin-preserves-'+key,child['artifacts']['waves-origin_'+key],child['artifacts']['waves-redo_'+key])

def grid_checks(r,runtime,values,out):
    out.mkdir();box=copy.deepcopy(values['part-v21']['data'])
    box['project'].update(name='Analytic box',units='metric',length=8,beam=4,draft=2,water_density=1.025,precision=0);box['particulars_set']=True
    box['points']=[dict(p=p,vertex_type=3,locked=False) for p in [[0,0,0],[8,0,0],[8,2,0],[0,2,0],[0,0,4],[8,0,4],[8,2,4],[0,2,4]]]
    faces=[[0,3,2,1],[3,7,6,2],[0,4,7,3],[1,2,6,5]];box['faces']=[dict(layer=0,points=p) for p in faces]
    edges=sorted({tuple(sorted((a,b))) for face in faces for a,b in zip(face,face[1:]+face[:1])});box['edges']=[[*edge,True] for edge in edges];box.pop('control_curves')
    box['layers'][0].update(symmetric=True,use_in_hydrostatics=True,use_for_intersections=True,show_in_linesplan=True)
    curves=[dict(name='two-roots',method='waterline',points=[[0,0,0],[0,1,1],[0,2,0]],knuckles=[False]*3,z=.6875),dict(name='tangent',method='waterline',points=[[0,0,0],[0,1,1],[0,2,0]],knuckles=[False]*3,z=1),dict(name='above',method='waterline',points=[[0,0,0],[0,1,1],[0,2,0]],knuckles=[False]*3,z=1.01)]
    result=x.probe(runtime/'qt_exchange_tests.exe',curves,out/'cubic')
    for name,expected in [('two-roots',[.5,1.5]),('tangent',[1.]),('above',[])]:
        got=result[name].get('points',[]);r.case('cubic.'+name,result[name]['ok'] and len(got)==len(expected) and all(abs(a-b)<2e-7 for a,b in zip(got,expected)),actual=got)
    source=out/'box.json';x.dump(source,box)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process') as s:
        made=q.require_ok(s.command('import_model',path=str(source),document_id=str(uuid.uuid4()),output=out/'box'))
        for kind,positions in ((1,[0,2,4,8]),(3,[0,1,2,5])):made=q.require_ok(s.command('sections',kind=kind,operation='add',positions=positions,output=out/('sections-'+str(kind))))
        saved=e.saved_copy(s,out/'saved');base=json.loads(Path(made['model']).read_text('utf8'));native=Path(made['model']).parent/'native-input.json'
        def worker(name,fmt,refs=None,model=None):
            folder=out/name;folder.mkdir();target=folder/'output';target.write_bytes(b'preserve');mp=Path(made['model'])
            if model is not None:mp=folder/'model.json';x.dump(mp,model)
            request=dict(format=fmt,model=str(mp),input=str(native),output=str(target),document_id=base['document_id'],revision=base['revision'],units='m',close_deck=False,references=refs or {})
            req=folder/'request.json';x.dump(req,request)
            with (folder/'worker.log').open('wb') as log:result=subprocess.run([str(runtime/'FreeShipGeometryExport.exe'),str(req),str(folder/'report.json'),str(os.getpid())],stdout=log,stderr=subprocess.STDOUT,timeout=60,creationflags=q.common.FLAGS)
            report=json.loads((folder/'report.json').read_text('utf8'));return report,target
        for fmt in ('michlet','addmass'):
            got,path=worker(fmt,fmt,dict(stations=5,waterlines=5))
            if fmt=='michlet':
                # Michlet 9.33 manual 8.1: nonzero bow rows select another
                # grammar. The old test only checked numbers, not this rule.
                r.case('michlet.reject-blunt-box',got['status']=='error' and path.read_bytes()==b'preserve',result=got);continue
            r.case(fmt+'.box-export',got['status']=='pass',result=got)
            if got['status']!='pass':continue
            r.case(fmt+'.independent-volume',math.isclose(got['volume_m3'],64,abs_tol=1e-8))
            r.case(fmt+'.independent-dimensions',got['length_m']==8 and got['draft_m']==2 and got['x_min_m']==0 and got['x_max_m']==8)
            records=[line for line in path.read_text('utf8').splitlines() if line and not line.startswith('#')]
            if fmt=='michlet':
                # 48 scalar/vector header records precede the hull table in the
                # legacy SaveToFile grammar. Checked independently of labels.
                start=next(i for i,line in enumerate(records) if len(line.split(','))==5)
                table=[list(map(float,row.split(','))) for row in records[start:start+5]]
                r.case('michlet.header-count',start==48,actual=start)
                r.case('michlet.speed-units',records[15:18]==['1','5','50'],actual=records[15:18])
                r.case('michlet.hull-tail',records[start+5:start+10]==['64','8','2','0','0'])
            else:
                table=[list(map(float,row.split())) for row in records[6:11]]
                params=list(map(float,records[1].split()));r.case('addmass.parameters',math.isclose(params[0],1025,abs_tol=.001) and params[1:]==[1000,100,.2,1,10000,0,10,0,1])
                r.case('addmass.dimensions-and-end',records[3:6]==['8 2','5 1. 0.','5'] and records[-1]=='\\*')
            r.case(fmt+'.all-offsets-including-ends',table==[[2.]*5]*5,actual=table)
            for name,refs in [('even-stations',dict(stations=6)),('too-large',dict(waterlines=999999)),('bad-draft',dict(draft=-2)),('shallow',dict(depth=1))]:
                rejected,dest=worker(fmt+'-'+name,fmt,refs);r.case(fmt+'.reject-'+name,rejected['status']=='error' and dest.read_bytes()==b'preserve')
        got,path=worker('table','offset_table');r.case('offset-table.export',got['status']=='pass',result=got)
        if got['status']=='pass':
            rows=path.read_text().splitlines();r.case('offset-table.headers',rows[1]=='X/Z,0,1,2,5');r.case('offset-table.no-fake-zero',all(row.endswith(',') for row in rows[2:]));r.case('offset-table.interior-breadths',rows[3].split(',')[2:4]==['2','2'])
        got,path=worker('creases','dxfcrease',dict(full_hull=False));r.case('creases.export',got['status']=='pass',result=got)
        if got['status']=='pass':
            lines=path.read_text().splitlines();pairs=list(zip(map(int,lines[::2]),lines[1::2]));r.case('creases.has-lines',sum(c==0 and v=='LINE' for c,v in pairs)>0)
            length=0;vertices={}
            for code,value in pairs:
                if code==0:
                    if vertices:length+=math.dist([vertices.get(c,0) for c in (10,20,30)],[vertices.get(c,0) for c in (11,21,31)])
                    vertices={}
                elif code in (10,11,20,21,30,31):vertices[code]=float(value)
            # Unique 10 edges of the half-box (no top face, no centerplane).
            expected=sum(math.dist(box['points'][a]['p'],box['points'][b]['p']) for a,b in edges)
            r.case('creases.independent-total-length',math.isclose(length,expected,abs_tol=1e-8),actual=length,expected=expected)
        made=q.require_ok(s.command('update_project',changes=dict(units='imperial'),output=out/'feet'))
        base=json.loads(Path(made['model']).read_text('utf8'));native=Path(made['model']).parent/'native-input.json'
        for fmt in ('michlet','addmass'):
            got,path=worker(fmt+'-feet',fmt,dict(stations=5,waterlines=5))
            if fmt=='michlet':r.case('michlet.reject-imperial-blunt-box',got['status']=='error' and path.read_bytes()==b'preserve',result=got)
            else:r.case(fmt+'.imperial-physical-values',got['status']=='pass' and math.isclose(got.get('volume_m3',0),64,rel_tol=3e-7) and math.isclose(got.get('length_m',0),8,rel_tol=2e-7) and math.isclose(got.get('draft_m',0),2,rel_tol=2e-7),result=got)
        commands=[dict(name=f,method='export',format=f,expect_failure=f=='michlet',path=str(out/(f+'.'+{'michlet':'mlt','addmass':'txt','offset_table':'csv','dxfcrease':'dxf'}[f])),options=dict(references=dict(stations=5,waterlines=5))) for f in ('michlet','addmass','offset_table','dxfcrease')]
        req=out/'production.json';x.dump(req,dict(model=str(saved),commands=commands));r.data['grid_production']=q.run_child(r,'grid-production',runtime/'FreeShipQt.exe',['--verify-exchange',req],out/'qt',timeout=180,platform='windows')
    multihull_checks(r,runtime,box,out/'multihull')


def read_michlet(path):
    """Independent record reader: manual 3.1, 7.13, 7.16, 8.1 (not labels)."""
    records=[s.strip() for s in path.read_text('utf8').splitlines() if s.strip() and not s.startswith('#')]
    header=records[:48];hulls=int(header[5]);nx=int(header[25]);nz=int(header[26]);at=48;result=[]
    for _ in range(hulls):
        table=[list(map(float,s.split(','))) for s in records[at:at+nx]];at+=nx
        assert len(table)==nx and all(len(row)==nz for row in table)
        assert table[0]==[0.]*nz, 'Nonzero first row is a hull-series code in Michlet'
        tail=records[at:at+21];at+=21
        assert tail[5:7]==['3','1.0,0.0,0.0']
        assert tail[7:15]==['3','2','0.0,0.0','10.0,0.0']*2
        assert tail[15:]==['0','2','0.0,0.0','10.0,0.0','0','0']
        result.append(dict(table=table,volume=float(tail[0]),length=float(tail[1]),draft=float(tail[2]),x=float(tail[3]),y=float(tail[4])))
    assert at==len(records), 'Missing or trailing hull records'
    return header,result


def multihull_checks(r,runtime,template,out):
    out.mkdir()
    def hull(points,faces):
        data=copy.deepcopy(template);data['points']=[dict(p=p,vertex_type=3,locked=False) for p in points];data['faces']=[dict(layer=0,points=f) for f in faces]
        edges=sorted({tuple(sorted((a,b))) for f in faces for a,b in zip(f,f[1:]+f[:1])});data['edges']=[[*edge,True] for edge in edges];return data
    # Triangular waterplane, constant vertical section: L=8, B=4, T=2;
    # volume=(8*4/2)*2=32. Row breadths from bow are 0,.5,1,1.5,2.
    mono=hull([[0,0,0],[8,0,0],[0,2,0],[0,0,4],[8,0,4],[0,2,4]],[[0,2,1],[2,5,4,1],[0,3,5,2]])
    cat=hull([[0,3,0],[0,7,0],[8,5,0],[0,3,4],[0,7,4],[8,5,4]],[[0,1,2],[1,4,5,2],[0,2,5,3],[0,3,4,1]])
    options=dict(stations=5,waterlines=5,hull_mode='duplicate',separation=10,gravity=9.81,start_speed=2,end_speed=8,speeds=13,friction=2,form_factor=3,r0=15,r1=50,beta=30,nr=41,nbeta=31,wave_x0=12,wave_x1=40,wave_y0=-7,wave_y1=19,wave_nx=51,wave_ny=25)
    expected=[[v]*5 for v in (0,.5,1,1.5,2)];saved={}
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process') as s:
        def make(name,data):
            path=out/(name+'.json');x.dump(path,data);return q.require_ok(s.command('import_model',path=str(path),document_id=str(uuid.uuid4()),output=out/name))
        def worker(name,refs,fmt='michlet'):
            folder=out/name;folder.mkdir();target=folder/'output';target.write_bytes(b'preserve')
            request=dict(format=fmt,model=made['model'],input=str(Path(made['model']).parent/'native-input.json'),output=str(target),document_id=s.state['document_id'],revision=s.state['revision'],units='m',close_deck=False,references=refs)
            x.dump(folder/'request.json',request)
            with (folder/'worker.log').open('wb') as log:subprocess.run([str(runtime/'FreeShipGeometryExport.exe'),str(folder/'request.json'),str(folder/'report.json'),str(os.getpid())],stdout=log,stderr=subprocess.STDOUT,timeout=60,creationflags=q.common.FLAGS)
            return json.loads((folder/'report.json').read_text('utf8')),target
        def validate(name,refs,nhulls,imperial=False):
            got,path=worker(name,refs);r.case(name+'.export',got['status']=='pass',result=got)
            if got['status']!='pass':return
            header,bodies=read_michlet(path)
            r.case(name+'.complete-records',len(bodies)==nhulls and bodies[0]['x']==bodies[0]['y']==0 and (nhulls==1 or (bodies[1]['x']==0 and bodies[1]['y']==10)))
            for i,body in enumerate(bodies):
                r.case(name+f'.hull-{i}-dimensions',all(math.isclose(body[k],v,rel_tol=4e-7,abs_tol=1e-7) for k,v in [('volume',32),('length',8),('draft',2)]),actual=body)
                r.case(name+f'.hull-{i}-ordinates',all(math.isclose(a,b,rel_tol=2e-6,abs_tol=2e-6) for row,want in zip(body['table'],expected) for a,b in zip(row,want)))
            r.case(name+'.total-volume',math.isclose(got['total_volume_m3'],nhulls*32,rel_tol=4e-7))
            if refs.get('wave_nx')==51:
                r.case(name+'.environment',float(header[6])==9.81 and header[15:18]==['2','8','13'] and header[20:22]==['2','3'])
                r.case(name+'.wave-domain',list(map(float,header[29:40]))==[15,50,30,41,31,12,40,-7,19,51,25])
        made=make('wedge',mono);saved['mono']=e.saved_copy(s,out/'wedge-saved');original=copy.deepcopy(s.state)
        validate('single',dict(stations=5,waterlines=5),1)
        validate('duplicate',options,2)
        invalid=[('overlap',dict(separation=3)),('touching',dict(separation=4)),('missing-gap',dict(separation=0)),('unknown-mode',dict(hull_mode='trimaran')),('nontext-mode',dict(hull_mode=1)),('even-stations',dict(stations=6)),('large-grid',dict(waterlines=999999)),('bad-draft',dict(draft=-1)),('shallow',dict(depth=1)),('r-order',dict(r1=10)),('x-order',dict(wave_x1=10)),('zero-width',dict(wave_y1=-7)),('fractional-samples',dict(nr=4.5)),('oversized-patch',dict(wave_nx=10000,wave_ny=10000)),('bad-count',dict(nbeta=1)),('invalid-friction',dict(friction=1.5)),('invalid-speed',dict(end_speed=1))]
        for name,changes in invalid:
            got,target=worker('reject-'+name,options|changes);r.case('michlet.reject-'+name,got['status']=='error' and target.read_bytes()==b'preserve',result=got)
        got,target=worker('reject-addmass-dual',options,'addmass');r.case('addmass.reject-unsupported-layout',got['status']=='error' and target.read_bytes()==b'preserve')
        after=q.require_ok(s.command('snapshot',output=out/'unchanged'))
        r.case('multihull.export-preserves-history',s.state==original)
        for key in ('model','scene','dxf'):r.compare('multihull.export-preserves-'+key,made[key],after[key])
        made=q.require_ok(s.command('update_project',changes=dict(units='imperial'),output=out/'wedge-feet'));validate('duplicate-imperial',options,2,True)
        made=make('cat',cat);saved['cat']=e.saved_copy(s,out/'cat-saved');validate('modelled-cat',options|dict(hull_mode='catamaran'),2)
        got,target=worker('wrong-centre',options|dict(hull_mode='catamaran',separation=11));r.case('cat.reject-wrong-centre',got['status']=='error' and target.read_bytes()==b'preserve')
        made=q.require_ok(s.command('update_project',changes=dict(units='imperial'),output=out/'cat-feet'));validate('cat-imperial',options|dict(hull_mode='catamaran'),2,True)
        asymmetric=copy.deepcopy(cat);asymmetric['points'][0]['p'][1]=3.1;made=make('asymmetric',asymmetric)
        got,target=worker('reject-asymmetric',options|dict(hull_mode='catamaran'));r.case('cat.reject-asymmetric-demihull',got['status']=='error' and target.read_bytes()==b'preserve')
    commands=[dict(name='duplicate',method='export',format='michlet',path=str(out/'duplicate.mlt'),options=dict(references=options)),dict(name='invalid-domain',method='export',format='michlet',path=str(out/'invalid.mlt'),expect_failure=True,options=dict(references=options|dict(wave_y1=-7)))]
    req=out/'qt-request.json';x.dump(req,dict(model=str(saved['mono']),commands=commands));child=q.run_child(r,'multihull-production',runtime/'FreeShipQt.exe',['--verify-exchange',req],out/'qt',timeout=180,platform='windows');r.data['multihull_production']=child
    for name in ('duplicate','invalid-domain'):
        for key in ('scene','dxf'):r.compare('multihull.production-'+name+'-preserves-'+key,child['artifacts'][name+'_'+key],child['artifacts']['original_'+key])
    header,bodies=read_michlet(out/'duplicate.mlt');r.case('multihull.production-values',len(bodies)==2 and bodies[1]['y']==10 and list(map(float,header[29:40]))==[15,50,30,41,31,12,40,-7,19,51,25])
    req=out/'qt-cat-request.json';x.dump(req,dict(model=str(saved['cat']),commands=[dict(name='cat',method='export',format='michlet',path=str(out/'cat.mlt'),options=dict(references=options|dict(hull_mode='catamaran')))]));child=q.run_child(r,'cat-production',runtime/'FreeShipQt.exe',['--verify-exchange',req],out/'qt-cat',timeout=180,platform='windows');r.data['cat_production']=child
    for key in ('scene','dxf'):r.compare('cat.production-preserves-'+key,child['artifacts']['cat_'+key],child['artifacts']['original_'+key])
    # Same Single-mesh arithmetic bound as the independent service check above.
    _,bodies=read_michlet(out/'cat.mlt');r.case('cat.production-volume',len(bodies)==2 and all(math.isclose(body['volume'],32,rel_tol=4e-7) for body in bodies))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--verify-deployment',type=Path);p.add_argument('--runtime',type=Path);p.add_argument('--reuse-build',type=Path);a=p.parse_args()
    runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    if any(root==out or root in out.parents for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT)):p.error('Use private output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.monotonic();sources=q.current_sources(q.QT);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0];tests={n:q.common.digest_file(q.ROOT/n) for n in FILES}
    protected={str(path):q.common.digest_file(path) for path in q.DEFAULT_MODELS}
    r.data.update(suite='qt-legacy-exchange',runtime=str(runtime),qt_sources_sha256=sources,test_sources_sha256=tests)
    try:
        if a.reuse_build:
            bridge=q.get_bridge(a,out,q.bridge_builder(q.QT),r);runtime=q.prepare_runtime(a,out,bridge,r);r.data['runtime']=str(runtime)
        manifest=json.loads((runtime/'build-manifest.json').read_text());q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale source manifest')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime hash mismatch')
        r.case('runtime-manifest',True);values=codec(r,runtime,out);service(r,runtime,values,out/'service');production(r,runtime,out/'production',out/'fixtures');grid_checks(r,runtime,values,out/'grid')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('models-protected',all(q.common.digest_file(Path(path))==h for path,h in protected.items()))
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==legacy);r.case('tests-preserved',all(q.common.digest_file(q.ROOT/n)==h for n,h in tests.items()))
        r.case('external-interoperability',status='not_run',reason='Bundled real part/VRML files are checked; external receiving applications and real wave files were not supplied.')
        r.case('desktop',status='not_run',reason='Hidden production callbacks do not cover physical mouse, focus, native picker or printer.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]

if __name__=='__main__':raise SystemExit(main())
