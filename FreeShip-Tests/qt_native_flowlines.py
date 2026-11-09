"""Native flow tracing: independent geometry and corrected Plus compatibility.

The reference adapter explicitly initializes Lines3DIntersect.Param=mub; the
protected original never assigns it. No comparison uses uninitialized values as
an expected answer. Exact curves/export comparisons are separate from the small
absolute float-roundoff bound of the analytically straight panel fixture.
"""
import argparse,copy,json,subprocess,uuid
from pathlib import Path
import qt_migration as q
import qt_modeling as modeling
import qt_native_project as project
import native_linesplan_app as nla

def run(r,runtime,source,out):
    out.mkdir(parents=True);env=q.private_environment(out/'probe-env')
    p=subprocess.run([str(runtime/'native_flowline_probe.exe')],env=env,capture_output=True,timeout=30,creationflags=q.common.FLAGS)
    (out/'flat-panel.json').write_bytes(p.stdout);data=json.loads(p.stdout)
    for i,(x,y,crossing) in enumerate([(8,1,5),(7,.5,2.5)]):
        points=data['flat'][i]['spline']['points'];expected=[[x,y,0],[crossing,y,0],[0,y,0]]
        r.case(f'flow.flat-{i}.straight-panel',len(points)==3 and all(abs(a-b)<3e-6 for p,e in zip(points,expected) for a,b in zip(p,e)),actual=points,expected=expected)
    r.case('flow.flat.missed-ray',not data['flat'][2]['spline']['points'])
    points=data['interpolated'][0]['spline']['points'];expected=[[8,1,0],[6.5,1.3,0],[3.8974358974358974,2,0]]
    r.case('flow.edge-fraction-controls-next-tangent',len(points)==3 and all(abs(a-b)<3e-5 for p,e in zip(points,expected) for a,b in zip(p,e)),actual=points,expected=expected)
    for key in ('dry_layer','degenerate'):r.case('flow.'+key,all(not f['spline']['points'] for f in data[key]))
    original=(q.common.DEFAULT_SOURCE/'Units/FreeGometry_Functions.inc').read_text('utf-8-sig')
    body=original.split('function Lines3DIntersect(')[1].split('end;{Lines3DIntersect}')[0]
    import re
    r.case('flow.original-missing-out-parameter-is-observed',not re.search(r'\bParam\s*:=',body,re.I))
    timings=[];fixture=out/'curved-box.fbm';project.fixture(fixture,rich=False)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        counter=0;last=None
        def both(label,method='snapshot',**fields):
            nonlocal counter,last
            counter+=1;results=[]
            for tag,s in [('native',native),('oracle',oracle)]:
                reply=q.require_ok(s.command(method,output=out/f'{counter:02}-{label}'/tag,**fields));model=json.loads(Path(reply['model']).read_text('utf8'));results.append((reply,model))
            (a,am),(b,bm)=results;prefix='flow.'+label
            r.case(prefix+'.exact-flow',am['flowlines']==bm['flowlines'])
            # Separate documents reopened after save may have different point
            # IDs (the Plus writer sorts point pointers); compare exact physical
            # topology then, retaining normal exact-ID checks before reopen.
            geometry=nla.reopened_geometry if label=='after-miss' else modeling.geometry
            r.case(prefix+'.exact-other-geometry',geometry(am)==geometry(bm))
            r.case(prefix+'.native',am['diagnostics']['flow_engine']=='cpp-native' and bm['diagnostics']['flow_engine']=='pascal-initialized-edge-parameter')
            for key in ('scene','dxf','sections'):r.compare(prefix+'.'+key,a[key],b[key])
            report=json.loads(Path(a['native_report']).read_text('utf8'));timings.append(dict(label=label,count=len(am['flowlines']),surface_ms=report['surface_display_ms'],cache_hit=report['cache_hit']))
            last=a;return am
        base=both('load','load',path=str(fixture),document_id=str(uuid.uuid4()))
        for precision in range(4):
            base=both(f'precision-{precision}','update_project',changes={'precision':precision})
            for x in (2.25,5.25,8):
                # A coplanar/boundary ray or stationary tangent can legitimately
                # fail. Compare acceptance before inspecting a published path.
                replies=[]
                for tag,s in [('native',native),('oracle',oracle)]:
                    state=dict(s.state);reply=s.command('auxiliary',operation='create_flow',view=1,source=[x,.4],output=out/f'p{precision}-seed-{x}-{tag}');replies.append(reply)
                    if not reply['ok']:r.case(f'flow.p{precision}-{x}-{tag}.atomic-miss',state==s.state)
                r.case(f'flow.p{precision}-{x}.same-acceptance',replies[0]['ok']==replies[1]['ok'])
                both(f'p{precision}-seed-{x}')
        before=both('cached');r.case('flow.multiple-paths',len(before['flowlines'])>=2);changed=both('move-corner','move_point',point_id=base['control_points'][0]['id'],position=[0,0,-.25])
        r.case('flow.point-edit-retraces',changed['flowlines']!=before['flowlines'])
        reverted=both('undo','undo');r.case('flow.undo-restores',reverted['flowlines']==before['flowlines'])
        redone=both('redo','redo');r.case('flow.redo-restores',redone['flowlines']==changed['flowlines'])
        snapshot=copy.deepcopy(redone)
        for tag,s in [('native',native),('oracle',oracle)]:
            save=q.require_ok(s.command('save',output=out/('save-'+tag)))
            reply=q.require_ok(s.command('load',path=save['saved_file'],document_id=str(uuid.uuid4()),output=out/('reopen-'+tag)))
            loaded=json.loads(Path(reply['model']).read_text('utf8'))
            r.case('flow.save-reopen-'+tag,loaded['flowlines']==snapshot['flowlines'])
        # A failed native trace must not create a partially added flowline.
        state=dict(native.state);reply=native.command('auxiliary',operation='create_flow',view=1,source=[1e6,1e6],output=out/'missing-hull')
        r.case('flow.miss-is-atomic',not reply['ok'] and native.state==state)
        current=both('after-miss');r.case('flow.miss-keeps-curves',current['flowlines']==snapshot['flowlines'])
        source_data=json.loads((Path(last['model']).parent/'native-input.json').read_text('utf8'))
    cache=out/'cache'
    def worker(label,data):
        target=out/label;target.mkdir();path=target/'input.json';path.write_text(json.dumps(data),'utf8')
        p=subprocess.run([str(runtime/'FreeShipNativeLinesplan.exe'),str(path),str(target),str(cache)],env=env,capture_output=True,timeout=120,creationflags=q.common.FLAGS)
        return p.returncode,json.loads((target/'native-report.json').read_text('utf8')),target
    code,report,cold=worker('worker-cold',source_data);r.case('flow.cache.cold',code==0 and not report['cache_hit'])
    code,report,warm=worker('worker-warm',source_data);r.case('flow.cache.restart',code==0 and report['cache_hit'] and (cold/'surface.json').read_bytes()==(warm/'surface.json').read_bytes())
    changed=copy.deepcopy(source_data);changed['surface_display']['flows'][0]['source'][0]+=.125
    code,report,edited=worker('worker-new-seed',changed);r.case('flow.cache.seed-invalidates',code==0 and not report['cache_hit'] and (cold/'surface.json').read_bytes()!=(edited/'surface.json').read_bytes())
    for key in ('scene.fslp','reference.dxf','sections.json'):r.compare('flow.seed-keeps-'+key,cold/key,edited/key)
    for label,mutate in [('view',lambda d:d['surface_display']['flows'][0].update(view=3)),('duplicate',lambda d:d['surface_display']['flows'].append(d['surface_display']['flows'][0])),('source',lambda d:d['surface_display']['flows'][0].update(source=[True,0]))]:
        changed=copy.deepcopy(source_data);mutate(changed);code,report,_=worker('worker-invalid-'+label,changed)
        r.case('flow.reject-'+label,code!=0 and report['status']=='error')
    r.data['native_flow_timings']=timings

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=q.DEFAULT_MODELS[0]);a=p.parse_args();r=q.Report()
    try:run(r,a.runtime.resolve(),a.model.resolve(),a.output.resolve())
    except Exception as e:r.case('flow.execution',status='error',reason=f'{type(e).__name__}: {e}')
    import qt_editing as e;e.write_report(r,a.output);print(r.data['counts'])

    raise SystemExit(1 if r.data["counts"]["fail"] or r.data["counts"]["error"] else 0)
