"""Developed plate service, independent geometry/oracle and hidden production UI.

No desktop automation. Retains all fixtures, diagnostics and export checks here.
"""
import argparse,json,math,shutil,subprocess
from pathlib import Path
from collections import Counter
import qt_migration as q
import qt_editing as e
import qt_plate_fixture as fixture
import qt_native_plates as native_plates

FILES=('qt_plates.py','qt_native_plates.py','qt_plate_fixture.py','QtPlateFixture.pas','qt_migration.py','qt_native_project.py','qt_document.py','run_tests.py')
def dist(a,b):return math.sqrt(sum((x-y)**2 for x,y in zip(a,b)))
def triangle(a,b,c):
    u=[x-y for x,y in zip(b,a)];v=[x-y for x,y in zip(c,a)]
    if len(u)==2:return abs(u[0]*v[1]-u[1]*v[0])/2
    return math.sqrt(sum(x*x for x in (u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])))/2
def metrics(r,key,p,reference=None):
    pts,orig=p['points'],p['original'];multiplier=2 if p['joined_mirror'] else 1
    r.case(key+'.finite',all(math.isfinite(x) for pt in pts for x in pt))
    errors=[dist(pts[a],pts[b])-dist(orig[a],orig[b]) for a,b,_,_ in p['edges']]
    # The old core stores coordinates as Single. Its rotation introduces a few
    # Single ULPs, so dimensional checks use a declared scale-relative tolerance.
    length=max(1,max(dist(orig[a],orig[b]) for a,b,_,_ in p['edges']));eps=2e-6*length
    r.case(key+'.edge-errors-independent',max(abs(err-edge[2]) for err,edge in zip(errors,p['edges']))<eps)
    a3=[];a2=[]
    for f in p['faces']:
        a3.append(sum(triangle(orig[f[0]],orig[f[i-1]],orig[f[i]]) for i in range(2,len(f))))
        a2.append(sum(triangle(pts[f[0]],pts[f[i-1]],pts[f[i]]) for i in range(2,len(f))))
    area=sum(a3);tol=2e-5*max(1,area)
    r.case(key+'.reference-area-independent',abs(area-multiplier*p['reference_area'])<tol)
    r.case(key+'.signed-area-independent',abs(sum(b-a for a,b in zip(a3,a2))-multiplier*p['signed_area_error'])<tol)
    r.case(key+'.absolute-area-independent',abs(sum(abs(b-a) for a,b in zip(a3,a2))-multiplier*p['absolute_area_error'])<tol)
    r.case(key+'.no-error-cancellation',p['absolute_area_error']+tol>=abs(p['signed_area_error']))
    if reference:
        r.case(key+'.original-metrics',all(abs(p[k]-reference[k])<2e-5*max(1,abs(reference[k])) for k in ('reference_area','absolute_area_error','normalized_error','min_edge_error','max_edge_error')))
        n=reference['point_count'];actual=sorted(dist(pts[i],pts[j]) for i in range(n) for j in range(i+1,n));expected=sorted(reference['distances'])
        r.case(key+'.original-shape',len(actual)==len(expected) and max((abs(a-b) for a,b in zip(actual,expected)),default=0)<2e-5*length)

def service(r,exe,models,out):
    oracle=json.loads((models/'oracle.json').read_text('utf8'))
    with q.Service(exe,out/'process') as s:
        for file in sorted(models.glob('*.fbm')):
            name=file.stem;folder=out/name
            load=s.command('load',path=str(file),document_id=str(q.uuid.uuid4()),output=folder/'load');q.require_ok(load);state=dict(s.state)
            response=s.command('develop_plates',output=folder/'plates')
            if name=='none':r.case('no-developable-layer',not response['ok'] and '可展开' in response.get('error',''));continue
            q.require_ok(response);data=json.loads(Path(response['plates']).read_text('utf8'))
            r.case(name+'.native-engine',response.get('analysis_engine')=='cpp-plates' and data.get('analysis_engine')=='cpp-plates')
            loaded=json.loads(Path(load['model']).read_text('utf8'))
            r.case(name+'.project-colors',data['underwater_color']==loaded['settings']['underwater_color'] and data['shade_submerged']==loaded['settings']['shade_underwater'])
            r.case(name+'.layer-colors',all(p['color']==loaded['layers'][0]['color'] for p in data['plates']))
            r.case(name+'.readonly-state',s.state==state)
            refs={x['name']:x for x in oracle[name]}
            if name=='bent-centre':
                r.case(name+'.legacy-incorrectly-joined',len(refs)==1)
                r.case(name+'.bent-seam-keeps-separate-halves',len(data['plates'])==2 and not any(p['joined_mirror'] for p in data['plates']))
                p=data['plates'][0];centre=[p['points'][i] for i,xyz in enumerate(p['original']) if abs(xyz[1])<=1e-4]
                a=min(centre);b=max(centre);length=dist(a,b)
                deviation=max(abs((b[0]-a[0])*(v[1]-a[1])-(b[1]-a[1])*(v[0]-a[0]))/length for v in centre)
                r.case(name+'.independent-noncollinear-seam',deviation>1e-3,maximum_distance=deviation)
            else:r.case(name+'.plate-count',len(data['plates'])==len(refs))
            for i,p in enumerate(data['plates']):metrics(r,name+f'.{i}',p,refs.get(p['name']))
            if name in ('plane','feet','mirror-bottom'):
                expected=9/(0.3048**2 if name=='feet' else 1)
                r.case(name+'.analytic-area',abs(data['plates'][0]['reference_area']-expected)<1e-5*expected)
            if name=='mirror-bottom':r.case(name+'.both-halves',data['plates'][0]['joined_mirror'] and len(data['plates'][0]['points'])==2*refs['mirror-bottom']['point_count'])
            if name=='hole':r.case(name+'.analytic-hole-area',abs(data['plates'][0]['reference_area']-8)<1e-5)
            if name=='saddle':r.case(name+'.nondevelopable-error-visible',data['plates'][0]['absolute_area_error']>1e-4)
            if name=='disconnected':r.case(name+'.separate-components',len(data['plates'])==2)
            cancelled=folder/'cancel';cancelled.mkdir();(cancelled/'cancel.flag').write_text('cancel')
            result=s.command('develop_plates',output=cancelled);r.case(name+'.cancel',not result['ok'] and '取消' in result.get('error','') and not (cancelled/'plates.json').exists())
            stale=s.command('develop_plates',expected_revision=0,output=folder/'stale');r.case(name+'.stale-rejected',not stale['ok'])
            after=s.command('snapshot',output=folder/'after');q.require_ok(after)
            r.case(name+'.readonly-history',s.state==state)
            for kind in ('scene','dxf'):r.compare(name+'.readonly-'+kind,after[kind],load[kind])
            # Intersections use the existing document transaction and participate
            # in the same development. No test-only calculation path.
            if name=='plane':
                q.require_ok(s.command('sections',operation='add',kind=1,positions=[1.0,2.0],output=folder/'sections'))
                res=s.command('develop_plates',output=folder/'intersections');q.require_ok(res)
                curves=json.loads(Path(res['plates']).read_text('utf8'))['plates'][0]['curves']
                r.case('plane.section-curves',len(curves)==2 and all(c['kind']==1 and len(c['points'])>=2 for c in curves))
            if name=='tilted':
                for kind,position in ((1,1.1),(2,2.7),(3,.8),(4,2.8)):
                    q.require_ok(s.command('sections',operation='add',kind=kind,positions=[position],output=folder/('section-'+str(kind))))
                res=s.command('develop_plates',output=folder/'intersections');q.require_ok(res)
                curves=json.loads(Path(res['plates']).read_text('utf8'))['plates'][0]['curves']
                r.case('tilted.all-section-kinds',set(c['kind'] for c in curves)=={1,2,3,4})

def pairs(path):
    lines=path.read_text('utf8').splitlines();return [(int(lines[i]),lines[i+1]) for i in range(0,len(lines),2)]
def dxf_lines(path):
    ps=pairs(path);polylines=[];current=None;vertex=None
    for code,value in ps:
        if code==0:
            if vertex is not None:current['points'].append((float(vertex.get(10,0)),float(vertex.get(20,0))));vertex=None
            if value=='POLYLINE':current={'points':[],'closed':False};polylines.append(current)
            elif value=='VERTEX':vertex={}
            elif value=='SEQEND':current=None
        elif vertex is not None:vertex[code]=value
        elif current is not None:
            if code==70:current['closed']=bool(int(value)&1)
            elif code==8:current['layer']=value
    return polylines
def placed(p,layout):
    pts=p['points'];cx=(min(x for x,y in pts)+max(x for x,y in pts))/2;cy=(min(y for x,y in pts)+max(y for x,y in pts))/2
    angle=math.radians(layout['rotation']);c=math.cos(angle);s=math.sin(angle)
    out=[]
    for x,y in pts:
        x-=cx;y-=cy
        if layout['mirrored']:y=-y
        out.append((x*c-y*s+cx+layout['x'],x*s+y*c+cy+layout['y']))
    return out
def edgekey(a,b):return tuple(sorted((tuple(round(v,9) for v in a),tuple(round(v,9) for v in b))))
def exports(r,key,folder,data):
    expected=Counter()
    for p,layout in zip(data['plates'],data['layout']):
        if not layout['visible']:continue
        pts=placed(p,layout)
        for a,b,err,boundary in p['edges']:
            if boundary:expected[edgekey(pts[a],pts[b])]+=1
    actual=Counter();polys=dxf_lines(folder/'plates.dxf')
    for poly in polys:
        if not poly['closed']:continue
        pts=poly['points']
        for a,b in zip(pts,pts[1:]+pts[:1]):actual[edgekey(a,b)]+=1
    r.case(key+'.dxf-all-boundary-edges',actual==expected,expected_edges=sum(expected.values()),actual_edges=sum(actual.values()))
    r.case(key+'.dxf-valid-closed-outlines',all(len(p['points'])>=3 for p in polys if p['closed']))
    try:
        import ezdxf
        drawing=ezdxf.readfile(folder/'plates.dxf');audit=drawing.audit()
        r.case(key+'.independent-dxf-reader',not audit.errors and not audit.fixes,errors=[str(x) for x in audit.errors],fixes=[str(x) for x in audit.fixes])
    except ImportError:r.case(key+'.independent-dxf-reader',status='not_run',reason='ezdxf is not installed')
    ps=pairs(folder/'plates.dxf');idx=ps.index((9,'$INSUNITS'));r.case(key+'.dxf-units',ps[idx+1]==(70,'6' if data['units']==0 else '2'))
    r.case(key+'.json-raw-precision',json.loads((folder/'plates.json').read_text('utf8'))==data)
    text=(folder/'plates.txt').read_text('utf8');rows=[]
    for line in text.splitlines():
        try:
            a=line.split()
            if len(a)==2:rows.append(tuple(float(v) for v in a))
        except ValueError:pass
    count=sum(len(poly['points']) for poly in polys if poly['closed'])
    r.case(key+'.text-all-boundary-vertices',len(rows)==count)
    r.case(key+'.text-unit',('单位: m' if data['units']==0 else '单位: ft') in text)
    try:
        import pymupdf as fitz
        with fitz.open(folder/'plates.pdf') as pdf:
            r.case(key+'.pdf-pages',len(pdf)>0);contents=''.join(p.get_text() for p in pdf)
            r.case(key+'.pdf-labels','FreeShip_26' in contents and '板材展开' in contents)
            pdf[0].get_pixmap(matrix=fitz.Matrix(1,1)).save(folder/'pdf-page-1.png')
        with fitz.open(folder/'scaled.pdf') as pdf:
            rects=[d['rect'] for d in pdf[0].get_drawings() if d['fill'] is not None and d['fill'] != (1.0,1.0,1.0)]
            bounds=None
            for rect in rects:bounds=rect if bounds is None else bounds|rect
            # A filled plate occupies its exact placement bounds at 1:100.
            pts=[pt for p,layout in zip(data['plates'],data['layout']) if layout['visible'] for pt in placed(p,layout)]
            expected_width=(max(x for x,y in pts)-min(x for x,y in pts))*(1000 if data['units']==0 else 304.8)/100/25.4*72
            r.case(key+'.pdf-physical-scale',bounds is not None and abs(bounds.width-expected_width)<.15,actual_points=None if bounds is None else bounds.width,expected_points=expected_width)
        with fitz.open(folder/'tiled.pdf') as pdf:r.case(key+'.pdf-tiles',len(pdf)>=2)
    except ImportError:r.case(key+'.pdf-reader',status='not_run',reason='PyMuPDF missing')

def production(r,runtime,models,out):
    for name in ('plane','mirror-bottom','mirror-side','cylinder','saddle','hole','feet','none'):
        folder=out/name;folder.mkdir(parents=True);request=folder/'request.json'
        request.write_text(json.dumps({'model':str(models/(name+'.fbm')),'cancel':name=='plane','late_cancel':name=='plane','expect_failure':name=='none'},ensure_ascii=False),encoding='utf8')
        child=q.run_child(r,name+'.production',runtime/'FreeShipQt.exe',['--verify-plates',request],folder/'qt',timeout=150,platform='windows')
        if not child:continue
        for kind in ('scene','dxf'):r.compare(name+'.production.readonly-'+kind,child['artifacts']['after_'+kind],child['artifacts']['original_'+kind])
        if name!='none':
            r.case(name+'.production.native-engine',child['artifacts']['plate_data'].get('analysis_engine')=='cpp-plates')
            exports(r,name,folder/'qt',child['artifacts']['plate_data'])
        child['artifacts']['fixture_model']=str(models/(name+'.fbm'))
        r.data['models'].append({'name':name,'sha256':q.common.digest_file(models/(name+'.fbm')),'production':child})
    r.case('visual-fidelity',status='review',reason='Hidden Windows widgets and exported pages are evidence, not full desktop visual acceptance')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT)
    p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path);p.add_argument('--oracle',type=Path);p.add_argument('--service-executable',type=Path)
    p.add_argument('--verify-deployment',type=Path);p.add_argument('--workflow-reference',type=Path)
    a=p.parse_args();out=a.output.resolve()
    if a.verify_deployment:
        if not a.workflow_reference:p.error('--workflow-reference required')
        reference=json.loads((a.workflow_reference/'report.json').read_text('utf8'));model=Path(reference['models'][0]['production']['artifacts']['fixture_model'])
        def prepare(model,folder):
            path=folder/'request.json';path.write_text(json.dumps({'model':str(model),'cancel':True,'late_cancel':True}),encoding='utf8');return path
        return q.verify_deployment(a,[model],out,'--verify-plates',('original','after'),('scene','dxf'),workflow_prepare=prepare)
    if any(out==root.resolve() or root.resolve() in out.parents for root in (a.source,a.qt_source,q.PILOT)):p.error('Output must be outside source roots')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data['suite']='qt-plates';before=q.common.fingerprint(a.source)[0]
    r.data['limitations']=['Plate calculation uses C++; the legacy kernel is an independent compatibility oracle.', 'Hidden Qt callbacks and PDF/DXF checks do not cover desktop input, physical printing or machining.']
    hashes={n:q.common.digest_file(q.ROOT/n) for n in FILES};r.data['test_sources_sha256']=hashes;r.data['source_sha256']=before;r.data['qt_sources_sha256']=q.current_sources(a.qt_source)
    protected={str(path):q.common.digest_file(path) for path in [*q.DEFAULT_MODELS,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe'] if path.exists()}
    try:
        if a.service_executable:exe=a.service_executable.resolve();runtime=None;r.case('production-ui',status='not_run',reason='Service diagnostic')
        else:
            bridge=q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),r);runtime=q.prepare_runtime(a,out,bridge,r);exe=runtime/'FreeShipDocumentService.exe'
        if a.oracle:oracle=a.oracle.resolve()
        else:
            built=fixture.build(a.source,out/'oracle-build')
            if built['status']!='pass':raise RuntimeError(str(built.get('error')))
            oracle=out/'oracle-build/QtPlateFixture.exe'
        built=json.loads((oracle.parent/'report.json').read_text('utf8'))
        if built['source_sha256']!=before or any(q.common.digest_file(q.ROOT/n)!=h for n,h in built['test_sources_sha256'].items()):raise RuntimeError('Stale original observer')
        models=out/'fixtures';models.mkdir();proc=subprocess.run([str(oracle),str(models)],cwd=models,env=q.private_environment(models),capture_output=True,creationflags=q.common.FLAGS,timeout=90)
        (out/'fixture.log').write_bytes(proc.stdout+b'\n'+proc.stderr)
        if proc.returncode:raise RuntimeError('Fixture failed; see fixture.log')
        service(r,exe,models,out/'service')
        if runtime:
            native_plates.run(r,runtime,models,out/'native')
            native_plates.precision_cases(r,runtime,models,out/'precision')
            native_plates.failures(r,runtime,out/'precision/saddle-3/fixture.fbm',out/'failures')
            native_plates.old_versions(r,runtime,out/'old-versions')
            production(r,runtime,models,out/'production')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==before)
        r.case('qt-source-unchanged',q.current_sources(a.qt_source)==r.data['qt_sources_sha256'])
        r.case('test-source-unchanged',all(q.common.digest_file(q.ROOT/n)==h for n,h in hashes.items()))
        for path,digest in protected.items():r.case('protected.'+path,q.common.digest_file(Path(path))==digest)
        r.case('native-desktop',status='not_run',reason='Hidden widgets only; real mouse, printer, focus and multi-monitor acceptance remain separate');e.write_report(r,out)
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'report':str(out/'report.json')}));return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]
if __name__=='__main__':raise SystemExit(main())
