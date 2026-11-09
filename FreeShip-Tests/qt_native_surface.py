"""Native four-view mesh, normals and control splines; independent Plus oracle.

Runs in private processes, no desktop automation. Keeps exact geometric values,
fan winding and layer IDs; only normal-list address traversal order is ignored.
"""
import argparse,copy,hashlib,json,math,subprocess,uuid
from pathlib import Path
import qt_migration as q
import qt_modeling as modeling
import qt_native_project as project


def run(r,runtime,models,out):
    out.mkdir(parents=True);timings=[];sample=None
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as native, q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        step=0
        def both(label,method='snapshot',**fields):
            nonlocal step,sample
            step+=1;values=[]
            for tag,service in [('native',native),('oracle',oracle)]:
                reply=q.require_ok(service.command(method,output=out/f'{step:03}-{label}'/tag,**fields))
                values.append((reply,json.loads(Path(reply['model']).read_text('utf8'))))
            (a,am),(b,bm)=values;prefix=f'surface.{step:03}-{label}'
            ga,gb=modeling.geometry(am),modeling.geometry(bm)
            r.case(prefix+'.model',ga==gb,different_fields=[k for k in ga if ga[k]!=gb.get(k)])
            r.case(prefix+'.history',all(a[k]==b[k] for k in q.STATE_KEYS))
            for key in ('scene','dxf','sections'):r.compare(prefix+'.'+key,a[key],b[key])
            report=json.loads(Path(a['native_report']).read_text('utf8'));surface=Path(a['surface']).read_bytes()
            r.case(prefix+'.native-only',am['diagnostics']['surface_engine']=='cpp-native' and am['diagnostics']['legacy_triangle_count']==0 and bm['diagnostics']['surface_engine']=='pascal-oracle')
            r.case(prefix+'.bound-artifact',report['surface_display'] and report['surface_sha256']==hashlib.sha256(surface).hexdigest())
            timings.append(dict(label=label,cache_hit=report['cache_hit'],surface_ms=report['surface_display_ms'],worker_ms=report['total_ms']))
            sample=Path(a['model']).parent/'native-input.json'
            return am,a
        for i,path in enumerate(models):
            base,_=both(f'model-{i}-load','load',path=str(path),document_id=str(uuid.uuid4()))
            base,_=both('normals','display',changes={'show_normals':True})
            for precision in range(4):
                both(f'precision-{precision}','update_project',changes={'precision':precision})
            # High precision is covered above; edit/history on large hulls use
            # medium precision to avoid repeating expensive unchanged exports.
            if len(base['control_points'])>100:
                both('edit-precision','update_project',changes={'precision':1})
            p=next(p for p in base['control_points'] if p.get('batch_movable',not p['locked']))
            position=list(p['p']);position[2]+=.125
            both('point-edit','move_point',point_id=p['id'],position=position)
            both('undo','undo');both('redo','redo')
        fixture=out/'known-box.fbm';project.fixture(fixture,rich=False)
        box,_=both('known-box','load',path=str(fixture),document_id=str(uuid.uuid4()))
        box,box_reply=both('known-box-normals','display',changes={'show_normals':True})
        q.run_child(r,'surface.integrity-and-widgets',runtime/'qt_auxiliary_tests.exe',
                    [box_reply['model']],out/'surface-widget-tests',platform='offscreen')
        # Independent normal direction: this closed-at-ends half box has five
        # planar faces. Each interior child point has exactly its plane normal.
        expected=[(2,0,-1),(2,3,1),(1,2,1),(0,0,-1),(0,10,1)]
        for face,(axis,offset,sign) in zip(box['control_faces'],expected):
            ns=[n for n in box['surface_normals'] if n['face']==face['id']]
            interior=[n for n in ns if all(0<n['p'][a]<[10,2,3][a] for a in range(3) if a!=axis)]
            r.case(f'surface.box.face-{face["id"]}-normal',bool(interior) and all(n['p'][axis]==offset and n['n']==[sign if a==axis else 0 for a in range(3)] for n in interior))
        r.case('surface.box.normal-lengths',all(abs(sum(x*x for x in n['n'])-1)<3e-7 for n in box['surface_normals']))
        tilted,_=both('legacy-normal-sum','move_point',point_id=box['control_points'][0]['id'],position=[0,0,.25])
        bottom=[n for n in tilted['surface_normals'] if n['face']==box['control_faces'][0]['id'] and 0<n['p'][0]<10 and 0<n['p'][1]<2]
        r.case('surface.legacy-negative-y-clamp-is-explicit',bool(bottom) and all(n['n'][1]==0 and n['n'][2]<0 for n in bottom) and any(n['n'][0]!=0 for n in bottom))
        both('undo-normal-sum','undo')
        layer=box['layers'][0]['id']
        hidden,_=both('hide-layer','update_layer',layer_id=layer,changes={'visible':False})
        r.case('surface.hidden-retains-triangles',hidden['triangles']==box['triangles'])
        visible_faces={f['id'] for f in hidden['control_faces'] if f['layer']!=layer}
        r.case('surface.hidden-normal-filter',all(n['face'] in visible_faces for n in hidden['surface_normals']))
        both('show-layer','undo')
        repeated,_=both('cached');r.case('surface.repeated-identical',modeling.geometry(repeated)==modeling.geometry(box))
        state=dict(native.state);blocked=out/'blocked';(blocked/'surface.json').mkdir(parents=True)
        reply=native.command('move_point',point_id=box['control_points'][0]['id'],position=[0,0,.25],output=blocked)
        r.case('surface.publication-rollback',not reply['ok'] and native.state==state)
        restored,_=both('after-failed-publication');r.case('surface.restored',modeling.geometry(restored)==modeling.geometry(box))
        source=json.loads(sample.read_text('utf8'))
    # Each worker invocation is a new process. Cache reuse crosses process exit.
    env=q.private_environment(out/'worker-env');cache=out/'cache'
    def worker(label,data):
        target=out/('worker-'+label);target.mkdir();inp=target/'input.json';inp.write_text(json.dumps(data),'utf8')
        p=subprocess.run([str(runtime/'FreeShipNativeLinesplan.exe'),str(inp),str(target),str(cache)],env=env,capture_output=True,timeout=120,creationflags=q.common.FLAGS)
        (target/'stderr.txt').write_bytes(p.stderr)
        return p.returncode,json.loads((target/'native-report.json').read_text('utf8')),target
    code,first,a=worker('cold',source);r.case('surface.cache.cold',code==0 and not first['cache_hit'])
    code,second,b=worker('warm',source);r.case('surface.cache.cross-process',code==0 and second['cache_hit'] and (a/'surface.json').read_bytes()==(b/'surface.json').read_bytes())
    owned=cache/first['cache_key']/'surface.json';owned.write_bytes(b'corrupt')
    code,third,c=worker('corrupt',source);r.case('surface.cache.corruption-rebuilt',code==0 and not third['cache_hit'] and (a/'surface.json').read_bytes()==(c/'surface.json').read_bytes())
    for key in ('faces','layers','curves'):
        bad=copy.deepcopy(source);bad['surface_display'][key].append(1)
        code,report,_=worker('bad-count-'+key,bad);r.case('surface.reject-count-'+key,code!=0 and report['status']=='error')
    bad=copy.deepcopy(source);bad['surface_display']['faces'][1]=bad['surface_display']['faces'][0]
    code,report,_=worker('duplicate-id',bad);r.case('surface.reject-duplicate-id',code!=0 and report['status']=='error')
    rebound=copy.deepcopy(source);rebound['surface_display']['layers']=[x+100 for x in source['surface_display']['layers']]
    code,report,d=worker('rebound',rebound);data=json.loads((d/'surface.json').read_text('utf8'))
    r.case('surface.cache.stable-ids-keyed',code==0 and not report['cache_hit'] and {t['layer'] for t in data['triangles']}=={x+100 for x in {t['layer'] for t in box['triangles']}})
    r.data['native_surface_timings']=timings


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,action='append');a=p.parse_args();r=q.Report()
    try:run(r,a.runtime.resolve(),[x.resolve() for x in (a.model or q.DEFAULT_MODELS)],a.output.resolve())
    except Exception as ex:r.case('surface.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    r.finish();a.output.mkdir(parents=True,exist_ok=True);(a.output/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2),'utf8');print(r.data['counts'])
    raise SystemExit(1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0)
