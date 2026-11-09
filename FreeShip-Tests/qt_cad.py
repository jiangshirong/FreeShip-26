"""CAD output: immutable service, independent IGES/DXF readers, actual Qt routes."""
import argparse,copy,json,math,subprocess,uuid
from collections import Counter
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_exchange as x
import cad_readers as readers
import qt_native_cad as native_cad

FILES=('qt_cad.py','qt_native_cad.py','qt_document.py','cad_readers.py','cad_occt_oracle.py','qt/exchange_tests.cpp','qt_migration.py','run_tests.py')
EXT={'iges':'igs','dxfmesh':'dxf','dxf3d':'dxf','offsets':'txt','coordinates':'txt'}

def same_state(r,key,before,after):
    r.case(key,all(before[k]==after[k] for k in q.STATE_KEYS))

def expected_curves(m,sections):
    out=[]
    for sec in sections['sections']:
        for i,s in enumerate(sec.get('splines',[]),1):out.append({'name':f'Section_{sec["kind"]}_{sec["index"]}_{i}',**s})
    for c in m.get('control_curves',[]):out.append({'name':f'ControlCurve_{c["id"]}',**c['spline']})
    return out

def verify_iges(r,key,path,data,m,options):
    actual=readers.iges(path);layers={l['id']:l for l in m['layers']};expected=[]
    scale=(.3048 if m['units']=='imperial' else 1)/{'m':1,'mm':.001,'cm':.01,'ft':.3048,'in':.0254}[options.get('units','m')]
    for p in data['patches']:
        l=layers[p['layer']]
        if not options.get('include_hidden') and not l['visible']:continue
        expected.append((p,False))
        if options.get('full_hull',True) and l['symmetric'] and any(v[1]!=0 for v in p['points']):expected.append((p,True))
    r.case(key+'.complete-patch-count',len(actual['patches'])==len(expected),actual=len(actual['patches']),expected=len(expected))
    ok=True
    for a,(p,mirror) in zip(actual['patches'],expected):
        pts=p['points']
        if mirror:pts=[(v[0],-v[1],v[2]) for row in range(p['rows']) for v in reversed(pts[row*p['cols']:(row+1)*p['cols']])]
        ok &= a['points']==[tuple(c*scale for c in v) for v in pts]
        ok &= a['u']==([p['u'][0]+p['u'][-1]-v for v in reversed(p['u'])] if mirror else p['u']) and a['v']==p['v']
        ok &= all(a[k]==p[k] for k in ('cols','rows','degree_u','degree_v'))
    r.case(key+'.exact-points-knots-and-mirror-orientation',ok)
    r.case(key+'.units',int(actual['global'][13])=={'m':6,'mm':2,'cm':10,'ft':4,'in':1}[options.get('units','m')])
    return actual

def verify_other(r,key,path,fmt,m,sections,opts):
    scale=(.3048 if m['units']=='imperial' else 1)/{'m':1,'mm':.001,'cm':.01,'ft':.3048,'in':.0254}[opts.get('units','m')]
    if fmt=='offsets':
        wanted=expected_curves(m,sections);actual=readers.offsets(path)
        r.case(key+'.names-and-segments',[c['name'] for c in actual]==[c['name'] for c in wanted])
        r.case(key+'.all-points-and-knuckles',len(actual)==len(wanted) and all(a['points']==[tuple(v*scale for v in p) for p in w['points']] and a['knuckles']==w['knuckles'] for a,w in zip(actual,wanted)))
    elif fmt=='coordinates':
        actual=[tuple(map(float,line.split())) for line in path.read_text('utf8').splitlines()]
        if opts.get('control_net'):wanted={tuple(v*scale for v in p['p']) for p in m['control_points']}
        else:wanted={p for tri in x.expected_triangles(m,opts.get('full_hull',True),opts.get('include_hidden',False),scale) for p in tri}
        r.case(key+'.complete-unique-coordinates',len(actual)==len(set(actual)) and set(actual)==wanted)
    else:
        import ezdxf
        doc=ezdxf.readfile(path);audit=doc.audit();r.case(key+'.independent-dxf-reader',not audit.has_errors,errors=[str(i) for i in audit.errors],fixes=[str(i) for i in audit.fixes])
        r.case(key+'.units',doc.header['$INSUNITS']=={'m':6,'mm':4,'cm':5,'ft':2,'in':1}[opts.get('units','m')])
        if fmt=='dxfmesh':
            actual=[]
            for f in doc.modelspace():
                assert f.dxftype()=='3DFACE';assert f.dxf.vtx2==f.dxf.vtx3
                actual.append(tuple(tuple(f.dxf.get('vtx'+str(i))) for i in range(3)))
            wanted=x.expected_triangles(m,opts.get('full_hull',True),opts.get('include_hidden',False),scale)
            r.case(key+'.all-oriented-triangles',Counter(x.oriented(t) for t in actual)==Counter(x.oriented(t) for t in wanted))
        else:
            wanted=expected_curves(m,sections);entities=list(doc.modelspace());endpoints=[];expected=[]
            for c in wanted:
                for a,b in zip(c['points'],c['points'][1:]):
                    if a!=b:expected.append((c['name'],tuple(v*scale for v in a),tuple(v*scale for v in b)))
            for spline in entities:
                assert spline.dxftype()=='SPLINE' and spline.dxf.degree==3
                assert list(spline.knots)==[0]*4+[1]*4
                points=list(spline.control_points);assert len(points)==4
                endpoints.append((spline.dxf.layer,tuple(points[0]),tuple(points[-1])))
            r.case(key+'.all-segments-and-endpoints',endpoints==expected)
    return path

def run_oracle(r,key,path,out,interpreter):
    if not interpreter.exists():r.case(key+'.opencascade',status='not_run',reason='Optional OCP environment missing');return
    dest=out/(key+'-occt.json')
    with (out/(key+'-occt.log')).open('wb') as log:
        done=subprocess.run([str(interpreter),str(q.ROOT/'cad_occt_oracle.py'),str(path),str(dest)],stdout=log,stderr=subprocess.STDOUT,creationflags=q.common.FLAGS,timeout=120)
    if done.returncode:r.case(key+'.opencascade',False,log=str(out/(key+'-occt.log')));return
    data=json.loads(dest.read_text('utf8'));expected=len(readers.iges(path)['patches'])
    r.case(key+'.opencascade',data.get('read_ok') and data.get('transferred',0)>0 and not data.get('null') and data.get('faces')==expected,result=data)
    r.case(key+'.opencascade-face-validity',data.get('valid',False),result=data)

def synthetic_cases(r,runtime,base,out,oracle):
    """Independent planar patches, mixed symmetry and an analytic natural cubic."""
    out.mkdir();m=json.loads(Path(base['model']).read_text('utf8'))
    layer=copy.deepcopy(m['layers'][0]);m['layers']=[dict(layer,id=1,symmetric=False,visible=True),dict(layer,id=2,symmetric=True,visible=True),dict(layer,id=3,symmetric=True,visible=False)]
    for face in m['control_faces']:face['layer']=1
    m['triangles']=[{'layer':lid,'p':pts} for lid,pts in ((1,[[0,1,0],[1,1,0],[0,2,0]]),(2,[[2,-1,0],[3,-1,0],[2,-2,0]]),(2,[[4,0,0],[5,0,0],[4,0,1]]),(2,[[6,1e-7,0],[7,1e-7,0],[6,1e-7,1]]),(3,[[8,2,0],[9,2,0],[8,3,0]]))]
    patches=[]
    for i,(lid,y) in enumerate(((1,1),(2,-1),(2,0),(2,1e-7),(3,2))):
        patches.append({'layer':lid,'cols':2,'rows':2,'degree_u':1,'degree_v':1,'u':[0,0,1,1],'v':[0,0,1,1],'points':[[i*2,y,0],[i*2+1,y,0],[i*2,y,1],[i*2+1,y,1]]})
    data={'schema':1,'document_id':m['document_id'],'revision':m['revision'],'patches':patches}
    definitions=[];render=[]
    for i,flags in enumerate(([False,False,False],[True,True,True])):
        plane=[0,0,1,0];definitions.append({'kind':3,'index':i,'distance':0,'show_curvature':False,'color':255,'plane':plane})
        points=[[0,0,0],[1,1,0],[2,0,0]]
        render.append({'kind':3,'index':i,'plane':plane,'paths':[points],'splines':[{'points':points,'knuckles':flags}]})
    m['sections']=definitions;m['control_curves']=[];sections={'format':'FreeShipSectionDisplay','version':1,'sections':render}
    mp=out/'model.json';sp=out/'sections.json';pp=out/'patches.json';x.dump(mp,m);x.dump(sp,sections);x.dump(pp,data)
    requests=[]
    for fmt in EXT:
        for label,opts in [('base',{}),('hidden',{'include_hidden':True}),('original',{'full_hull':False}),('inches',{'units':'in'})]:
            name=fmt+'-'+label;requests.append({'name':name,'method':'write','format':fmt,'model':str(mp),'sections':str(sp),'patches':str(pp),'path':str(out/(name+'.'+EXT[fmt])),'options':opts})
    imperial=copy.deepcopy(m);imperial['units']='imperial';imperial['settings']['units']='imperial';imp=out/'imperial.json';x.dump(imp,imperial)
    for fmt in ('iges','dxfmesh'):
        requests.append({**next(c for c in requests if c['format']==fmt),'name':fmt+'-feet','model':str(imp),'path':str(out/(fmt+'-feet.'+EXT[fmt])),'options':{'units':'ft'}})
    result=x.probe(runtime/'qt_exchange_tests.exe',requests,out/'probe')
    for c in requests:
        ok=result[c['name']]['ok'];r.case('synthetic.'+c['name'],ok,error=result[c['name']].get('error'))
        if not ok:continue
        current=imperial if c['name'].endswith('-feet') else m
        if c['format']=='iges':verify_iges(r,c['name'],Path(c['path']),data,current,c['options'])
        else:verify_other(r,c['name'],Path(c['path']),c['format'],current,sections,c['options'])
    import ezdxf
    entities=list(ezdxf.readfile(out/'dxf3d-base.dxf').modelspace())
    errors=[]
    for segment,entity in enumerate(entities):
        for j in range(21):
            t=j/20;pts=list(entity.control_points);actual=tuple((1-t)**3*pts[0][k]+3*(1-t)**2*t*pts[1][k]+3*(1-t)*t*t*pts[2][k]+t**3*pts[3][k] for k in range(3))
            # y=1.5t-.5t^3 on the first span of the natural cubic through
            # (0,0),(1,1),(2,0); the other span is its reflection about x=1.
            second=segment%2==1;v=1-t if second else t
            y=v if segment>=2 else 1.5*v-.5*v**3
            expected=(segment%2+t,y,0);errors.append(max(abs(a-b) for a,b in zip(actual,expected)))
    r.case('dxf.cubic-independent-polynomial',len(entities)==4 and max(errors)<1e-7,maximum_error=max(errors),bound='1e-7 source units; Single h^2/6 coefficient')
    run_oracle(r,'linear-patches',out/'iges-base.igs',out,oracle)
    if (out/'linear-patches-occt.json').exists():
        occt=json.loads((out/'linear-patches-occt.json').read_text('utf8'))
        r.case('iges.independent-plane-area',math.isclose(occt.get('area_mm2',0),6e6,rel_tol=1e-10))
    # Once parsed, raw coordinate precision and small offsets must remain exact.
    r.case('iges.near-center-not-collapsed',any(any(p[1]==1e-7 for p in a['points']) for a in readers.iges(out/'iges-base.igs')['patches']))

def plane_fixture(r,runtime,out,oracle):
    out.mkdir();path=out/'plane.fef'
    path.write_text('Plane\nDesigner\nAuthor\nplanar rectangle\n4 2 0.5 1.025 1 0 -1 0\n1\nPlane\n1 255 -1 -1 0 -1 0 -1 7.85 0.01\n4\n0 0 0 3 0\n4 0 0 3 0\n4 2 0 3 0\n0 2 0 3 0\n4\n0 1 1 0\n1 2 1 0\n2 3 1 0\n3 0 1 0\n1\n4 0 1 2 3 0 0\n',encoding='utf8')
    parsed=x.probe(runtime/'qt_exchange_tests.exe',[{'name':'plane','method':'read','format':'fef','path':str(path)}],out/'parse')
    r.case('plane.read-fixture',parsed['plane']['ok']);payload=out/'plane.json';x.dump(payload,parsed['plane']['data'])
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'service') as s:
        loaded=s.command('import_model',path=str(payload),document_id=str(uuid.uuid4()),output=out/'loaded');q.require_ok(loaded)
        cad=s.command('cad_iges',output=out/'cad');q.require_ok(cad);same_state(r,'plane.unchanged',loaded,cad)
    iges=out/'plane.igs';request={'name':'plane','method':'write','format':'iges','path':str(iges),'model':loaded['model'],'patches':cad['cad']}
    written=x.probe(runtime/'qt_exchange_tests.exe',[request],out/'write');r.case('plane.write',written['plane']['ok'])
    run_oracle(r,'plane',iges,out,oracle)
    if (out/'plane-occt.json').exists():
        result=json.loads((out/'plane-occt.json').read_text('utf8'))
        r.case('plane.original-nurbs-mathematical-area',math.isclose(result.get('area_mm2',0),8e6,rel_tol=1e-6),result=result)
        r.case('plane.original-nurbs-bounds',max(abs(a-b) for a,b in zip(result['bounds'],[0,0,0,4000,2000,0]))<.001)

def exercise(r,runtime,model,out,oracle):
    out.mkdir(parents=True)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'service') as s:
        before=s.command('load',path=str(model),document_id=str(uuid.uuid4()),output=out/'load');q.require_ok(before)
        cad=s.command('cad_iges',output=out/'cad');q.require_ok(cad);same_state(r,'cad.read-only-state',before,cad)
        r.case('cad.native-engine',cad.get('analysis_engine')=='cpp-cad')
        repeated=s.command('cad_iges',output=out/'cad-repeat');q.require_ok(repeated)
        r.case('cad.repeat-deterministic',json.loads(Path(cad['cad']).read_text('utf8'))==json.loads(Path(repeated['cad']).read_text('utf8')))
        rejected=s.command('cad_iges',expected_revision=before['revision']+1,output=out/'stale');r.case('cad.stale-request',not rejected['ok'])
        cancel=out/'cancel';cancel.mkdir();(cancel/'cancel.flag').write_text('cancel')
        rejected=s.command('cad_iges',output=cancel);r.case('cad.cancel',not rejected['ok'])
        after=s.command('snapshot',output=out/'after');q.require_ok(after);same_state(r,'cad.failure-state',before,after)
        for field in ('model','scene','dxf','sections'):r.compare('cad.unchanged-'+field,before[field],after[field])
    m=json.loads(Path(before['model']).read_text('utf8'));sections=json.loads(Path(before['sections']).read_text('utf8'));data=json.loads(Path(cad['cad']).read_text('utf8'))
    has_curves=bool(expected_curves(m,sections))
    requests=[]
    for fmt in EXT:
        for variant,opts in [('default',{}),('original',{'full_hull':False}),('mm',{'units':'mm'}),('all',{'include_hidden':True}),('camera',{})]:
            name=fmt+'-'+variant;requests.append({'name':name,'method':'write','format':fmt,'model':before['model'],'sections':before['sections'],'patches':cad['cad'],'path':str(out/(name+'.'+EXT[fmt])),'options':opts,'camera_half':variant=='camera'})
    requests.append({**requests[-1],'name':'control-points','format':'coordinates','options':{'control_net':True},'path':str(out/'control-points.txt')})
    results=x.probe(runtime/'qt_exchange_tests.exe',requests,out/'probe')
    for c in requests:
        ok=results[c['name']]['ok'];expected_empty=c['format'] in ('dxf3d','offsets') and not has_curves
        r.case(c['name']+('.reject-empty' if expected_empty else '.write'),not ok if expected_empty else ok,error=results[c['name']].get('error'))
        if not ok:continue
        if c['format']=='iges':verify_iges(r,c['name'],Path(c['path']),data,m,c['options'])
        else:verify_other(r,c['name'],Path(c['path']),c['format'],m,sections,c['options'])
    for fmt in EXT:
        if results[fmt+'-default']['ok'] and results[fmt+'-camera']['ok']:
            a=out/(fmt+'-default.'+EXT[fmt]);b=out/(fmt+'-camera.'+EXT[fmt]);same=a.read_bytes()==b.read_bytes()
            if fmt=='iges':same=readers.iges(a)['entities']==readers.iges(b)['entities']
            r.case(fmt+'.camera-independent',same)
    run_oracle(r,'default',out/'iges-default.igs',out,oracle)
    # Invalid metadata must not overwrite an existing export.
    for key,change in [('revision',lambda d:d.update(revision=999999)),('knots',lambda d:d['patches'][0]['u'].reverse()),('count',lambda d:d['patches'][0].update(cols=100000))]:
        bad=copy.deepcopy(data);change(bad);raw=out/(key+'.json');x.dump(raw,bad);target=out/(key+'.igs');target.write_bytes(b'preserve')
        res=x.probe(runtime/'qt_exchange_tests.exe',[{**requests[0],'name':'invalid','patches':str(raw),'path':str(target)}],out/('invalid-'+key))
        r.case('cad.invalid-'+key+'-preserves-file',not res['invalid']['ok'] and target.read_bytes()==b'preserve')
    # Exercise the actual menu and dialog, then compare to the codec evidence.
    qt=out/'qt';qt.mkdir();commands=[{'name':fmt,'method':'export','format':fmt,'path':str(qt/('result-'+fmt+'.'+ext)),'expect_failure':fmt in ('dxf3d','offsets') and not has_curves} for fmt,ext in EXT.items()]
    protected=qt/'preserved.fbm';protected.write_bytes(b'preserve')
    commands.append({'name':'wrong-suffix','method':'export','format':'iges','path':str(protected),'expect_failure':True})
    cancelled=qt/'cancelled.igs'
    commands.append({'name':'late-cancel','method':'export','format':'iges','path':str(cancelled),'late_cancel':True,'expect_failure':True})
    req=out/'workflow.json';x.dump(req,{'model':str(model),'commands':commands})
    child=q.run_child(r,'production',runtime/'FreeShipQt.exe',['--verify-exchange',req],qt,timeout=300,platform='windows')
    if child:
        art=child['artifacts']
        r.case('ui.wrong-suffix-preserves-file',protected.read_bytes()==b'preserve')
        r.case('ui.late-cancel-no-output',not cancelled.exists() and '取消' in art.get('late-cancel_error',''))
        for c in commands:
            if c['name'] in ('wrong-suffix','late-cancel'):continue
            fmt=c['format'];path=Path(c['path'])
            if not path.exists():continue
            if fmt=='iges':verify_iges(r,'ui.'+fmt,path,data,m,{})
            else:verify_other(r,'ui.'+fmt,path,fmt,m,sections,{})
            for field in ('model','scene','dxf'):r.compare('ui.'+fmt+'.unchanged-'+field,art[fmt+'_'+field],art['original_'+field])
    return before,data

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT)
    p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--model',type=Path,action='append');p.add_argument('--occt-python',type=Path,default=Path('C:/FreeShip26Tests/tools/cad-oracle/Scripts/python.exe'))
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data['suite']='qt-cad';before=q.common.fingerprint(a.source)[0]
    hashes={n:q.common.digest_file(q.ROOT/n) for n in FILES};r.data['test_sources_sha256']=hashes;r.data['source_sha256']=before;r.data['qt_sources_sha256']=q.current_sources(a.qt_source)
    models=[f.resolve() for f in (a.model or q.DEFAULT_MODELS)];protected={str(f):q.common.digest_file(f) for f in [*models,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe'] if f.exists()}
    try:
        if a.verify_deployment:
            runtime=a.verify_deployment.resolve();manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
            if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=q.current_sources(a.qt_source):raise RuntimeError('Installed source manifest is stale')
            for n,h in manifest['binaries_sha256'].items():
                if q.common.digest_file(runtime/n)!=h:raise RuntimeError('Installed binary mismatch '+n)
            r.data['runtime_path']=str(runtime);r.data['runtime_manifest_sha256']=q.common.digest_file(runtime/'build-manifest.json')
        else:
            bridge=q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),r);runtime=q.prepare_runtime(a,out,bridge,r)
        for i,model in enumerate(models):
            try:
                base,_=exercise(r,runtime,model,out/('model-'+str(i+1)),a.occt_python)
                native_cad.paired(r,runtime,model,out/('native-model-'+str(i+1)))
                if i==0:
                    synthetic_cases(r,runtime,base,out/'synthetic',a.occt_python)
                    plane_fixture(r,runtime,out/'plane',a.occt_python)
                    native_cad.run(r,runtime,out/'native',a.occt_python)
            except Exception as ex:r.case('model-'+str(i+1)+'.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==before);r.case('qt-source-unchanged',q.current_sources(a.qt_source)==r.data['qt_sources_sha256']);r.case('test-sources-unchanged',all(q.common.digest_file(q.ROOT/n)==h for n,h in hashes.items()))
        for f,h in protected.items():r.case('protected.'+f,q.common.digest_file(Path(f))==h)
        r.case('visual-fidelity',status='review',reason='Hidden Windows dialogs, not complete original-window visual acceptance')
        r.case('desktop-and-cad-app',status='not_run',reason='No physical desktop, commercial CAD app or manufacturing verification')
        e.write_report(r,out)
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'report':str(out/'report.json')}));return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]
if __name__=='__main__':raise SystemExit(main())
