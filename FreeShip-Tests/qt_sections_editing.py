"""Four section kinds and safe layer management through production actions/service.

Private inputs, independent plane expectations, legacy output comparisons,
undo/save/reopen and invalid-command rollback. No desktop input automation.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import math
import shutil
import subprocess
import uuid
import qt_migration as q
import qt_editing as editing
import native_linesplan_app as native

PHASES=('original','added','edited','deleted','undo','redo','layer','complete','reopened')

def section_display(report, prefix, response, folder, model):
    path=q.artifact(response,'sections',folder)
    data=path.read_bytes();root=json.loads(data)
    meta=json.loads(q.artifact(response,'native_report',folder).read_text('utf8'))
    report.case(prefix+'.sidecar-hash',hashlib.sha256(data).hexdigest()==meta['sections_sha256'])
    report.case(prefix+'.sidecar-format',root['format']=='FreeShipSectionDisplay' and root['version']==1)
    expected={(s['kind'],s['index']):s for s in model['sections']}
    report.case(prefix+'.sidecar-keys',set(expected)=={(s['kind'],s['index']) for s in root['sections']})
    count=0;valid=True
    for section in root['sections']:
        plane=section['plane'];original=expected[(section['kind'],section['index'])]['plane']
        valid &= all(editing.f32(a)==editing.f32(b) for a,b in zip(plane,original))
        for curve in section['paths']:
            valid &= len(curve)>=2
            for p in curve:
                count+=1;valid &= len(p)==3 and all(math.isfinite(x) for x in p)
                # A spline through a plane stays in that plane. Allow only
                # Single evaluation rounding, independently of either exporter.
                scale=max(1,abs(plane[3]),*(abs(x) for x in p))
                valid &= abs(sum(a*b for a,b in zip(plane,p))+plane[3])<=scale*2e-6
    report.case(prefix+'.independent-plane-equations',valid,sampled_points=count)
    return root

def recipe(report,executable,model,folder):
    stages={}
    with q.Service(executable,folder/'native-service',native_linesplan=True) as service, q.Service(executable,folder/'legacy-service',native_linesplan=False) as legacy:
        def capture(name,method='snapshot',**fields):
            results=[]
            for tag,current in [('native',service),('legacy',legacy)]:
                out=folder/name/tag;out.mkdir(parents=True,exist_ok=True)
                response=current.command(method,output=out,**fields)
                result=q.validate_snapshot(report,name+'.'+tag,response,out);results.append(result)
                if tag=='native':section_display(report,name,response,out,result['model'])
            a,b=results
            report.case(name+'.model-exact',q.geometry(a['model'])==q.geometry(b['model']) and a['model']['sections']==b['model']['sections'])
            report.compare(name+'.dxf-exact',a['paths']['dxf'],b['paths']['dxf'])
            native.compare_scene(report,name,b['paths']['scene'],a['paths']['scene'])
            stages[name]=a;return a['model']
        original=capture('original','load',path=str(model),document_id=str(uuid.uuid4()))
        def reject(name,method='sections',**fields):
            before=dict(service.state);out=folder/name;out.mkdir()
            response=service.command(method,output=out,**fields)
            report.case('reject.'+name,not response['ok'] and service.state==before,error=response.get('error'))
        for kind in range(1,5):
            # Known offset, independent of hull dimensions; outside-hull
            # definitions are deliberately retained, too.
            pos=1.234567+kind*0.01
            added=capture(f'add-{kind}','sections',operation='add',kind=kind,positions=[pos,pos+0.5,pos+1,pos])
            item=next(s for s in added['sections'] if s['kind']==kind and abs(s['distance']-editing.f32(pos))<1e-6)
            plane=item['plane'];n=editing.f32(1/math.sqrt(2))
            want=[1,0,0,-editing.f32(pos)] if kind==1 else [0,1,0,-editing.f32(pos)] if kind==2 else [0,0,1,-editing.f32(pos)] if kind==3 else [0,n,n,editing.f32(-editing.f32(pos)*n)]
            report.case(f'kind-{kind}.independent-plane',all(editing.f32(a)==b for a,b in zip(plane,want)))
            state=dict(service.state);capture(f'no-op-{kind}','sections',operation='add',kind=kind,positions=[pos])
            report.case(f'kind-{kind}.duplicate-no-history',service.state==state)
            changed=capture(f'update-{kind}','sections',operation='update',kind=kind,index=item['index'],distance=pos+0.125,show_curvature=True)
            updated=next(s for s in changed['sections'] if s['kind']==kind and abs(s['distance']-editing.f32(pos+0.125))<1e-6)
            report.case(f'kind-{kind}.distance-and-curvature',updated['show_curvature'] and abs(updated['distance']-editing.f32(pos+0.125))<1e-6)
            reject(f'duplicate-distance-{kind}',operation='update',kind=kind,index=updated['index'],distance=pos+0.5)
            reject(f'huge-index-{kind}',operation='update',kind=kind,index=2**32+updated['index'],distance=5)
            capture(f'delete-{kind}','sections',operation='delete',kind=kind,indices=[updated['index']])
            restored=capture(f'undo-{kind}','undo');report.case(f'kind-{kind}.undo-exact',restored['sections']==changed['sections'])
            capture(f'redo-{kind}','redo')
        for name,fields in [('kind-overflow',dict(kind=2**32+1,operation='clear')),
                            ('index-negative',dict(kind=1,operation='update',index=-1,distance=1)),
                            ('index-fraction',dict(kind=1,operation='delete',indices=[0.5])),
                            ('index-duplicate',dict(kind=1,operation='delete',indices=[0,0])),
                            ('nonfinite',dict(kind=1,operation='add',positions=['EXPONENT_OVERFLOW'],raw_nonfinite=True)),
                            ('partial-update',dict(kind=1,operation='update',index=0,distance=54321,show_curvature='wrong')),
                            ('too-many',dict(kind=1,operation='add',positions=list(range(513)))),
                            ('stale',dict(kind=1,operation='clear',expected_revision=1))]:reject(name,**fields)
        before=capture('before-clear');cleared=capture('clear','sections',kind=4,operation='clear')
        report.case('clear-only-selected-kind',all(s['kind']!=4 for s in cleared['sections']) and [s for s in before['sections'] if s['kind']!=4]==cleared['sections'])
        restored=capture('undo-clear','undo');report.case('clear-undo-exact',restored['sections']==before['sections'])
        layer=capture('new-layer','manage_layers',operation='create',changes={'name':'Section protocol','color':123456})
        created=next(l for l in layer['layers'] if l['name']=='Section protocol');lid=created['id']
        report.case('layer-new-identity',lid>max(l['id'] for l in original['layers']) and created['color']==123456)
        occupied=original['control_faces'][0]['layer'];reject('occupied-layer','manage_layers',operation='delete',layer_id=occupied)
        reject('invalid-new-layer','manage_layers',operation='create',changes={'name':'Bad','thickness':-1})
        after=capture('after-invalid');report.case('invalid-layer-no-ghost',after['layers']==layer['layers'])
        capture('delete-layer','manage_layers',operation='delete',layer_id=lid)
        restored=capture('restore-layer','undo');report.case('layer-undo-id-and-properties',restored['layers']==layer['layers'])
        capture('redo-layer','redo')
        branch=capture('branch-layer','manage_layers',operation='create',changes={'name':'Branch layer'})
        report.case('layer-deleted-id-not-reused',branch['layers'][-1]['id']>lid)
        clean=capture('clean-empty','manage_layers',operation='delete_empty')
        report.case('cleanup-preserves-faces-and-last-layer',len(clean['layers'])>=1 and clean['control_faces']==original['control_faces'])
        reject('deleted-layer-id','update_layer',layer_id=lid,changes={'name':'Must reject'})
        # Exercise a large batched edit without building 512 legacy redraws.
        batch=capture('batch','sections',operation='add',kind=1,positions=[10000+i*0.25 for i in range(128)])
        report.case('batch-add-count',len(batch['sections'])==len(clean['sections'])+128)
        capture('undo-batch','undo')
        for tag,current in [('native',service),('legacy',legacy)]:
            out=folder/('save-'+tag);out.mkdir();saved=q.require_ok(current.command('save',output=out))
            out=folder/('reopen-'+tag);out.mkdir();response=current.command('load',output=out,path=saved['saved_file'],document_id=str(uuid.uuid4()))
            reopened=q.validate_snapshot(report,'reopen-'+tag,response,out)['model']
            report.case('reopen-'+tag+'.sections-exact',reopened['sections']==clean['sections'])
            report.case('reopen-'+tag+'.attributed-model',native.reopened_geometry(reopened)==native.reopened_geometry(clean))
        return stages

def cache_recovery(report,runtime,source,folder):
    cache=folder/'cache';env=q.private_environment(folder/'env')
    def run(name):
        out=folder/name;out.mkdir(parents=True)
        result=subprocess.run([str(runtime/'FreeShipNativeLinesplan.exe'),str(source),str(out),str(cache)],env=env,capture_output=True,timeout=120,creationflags=q.common.FLAGS)
        if result.returncode:raise RuntimeError('Native cache probe failed: '+result.stderr.decode('utf8',errors='replace'))
        return out,json.loads((out/'native-report.json').read_text('utf8'))
    cold,first=run('cold');warm,second=run('warm')
    report.case('cache.cold-then-hit',not first['cache_hit'] and second['cache_hit'])
    (cache/first['cache_key']/'sections.json').write_text('{}',encoding='utf8')
    recovered,third=run('recovered')
    report.case('cache.corrupt-section-data-regenerated',not third['cache_hit'])
    for filename in ('scene.fslp','reference.dxf','sections.json'):
        report.case('cache.recovered-'+filename,(cold/filename).read_bytes()==(recovered/filename).read_bytes()==(warm/filename).read_bytes())

def last_layer_guard(report,runtime,model,folder):
    # The independent box has no locked or constrained vertices. Remove its
    # faces through the existing production command to reach a truly empty
    # last layer, rather than merely exercising the occupied-layer guard again.
    with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',folder/'service') as service:
        def call(name,method,**fields):
            out=folder/name;out.mkdir(parents=True);r=q.require_ok(service.command(method,output=out,**fields))
            return json.loads(q.artifact(r,'model',out).read_text('utf8'))
        original=call('load','load',path=str(model),document_id=str(uuid.uuid4()))
        call('remove-faces','topology',operation='delete',kind='face',ids=[f['id'] for f in original['control_faces']])
        empty=call('empty','manage_layers',operation='delete_empty')
        report.case('layer.last-empty-layer-retained',len(empty['layers'])==1 and not empty['control_faces'])
        out=folder/'reject-last';out.mkdir();before=dict(service.state)
        r=service.command('manage_layers',output=out,operation='delete',layer_id=empty['layers'][0]['id'])
        report.case('layer.last-empty-layer-delete-rejected',not r['ok'] and before==service.state,error=r.get('error'))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True);parser.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE)
    parser.add_argument('--qt-source',type=Path,default=q.QT);parser.add_argument('--reuse-build',type=Path);parser.add_argument('--runtime',type=Path)
    parser.add_argument('--model',type=Path,action='append');parser.add_argument('--verify-deployment',type=Path);parser.add_argument('--workflow-reference',type=Path)
    args=parser.parse_args();args.source=args.source.resolve();args.qt_source=args.qt_source.resolve();out=args.output.resolve()
    models=[p.resolve(strict=True) for p in (args.model or q.DEFAULT_MODELS)]
    if args.verify_deployment:return q.verify_deployment(args,models,out,'--verify-sections',PHASES,('scene','dxf','sections'))
    for protected in (args.source,args.qt_source,q.PILOT):
        if out==protected or protected in out.parents:parser.error('Use an isolated report directory')
    out.mkdir(parents=True,exist_ok=False);report=q.Report();report.data['suite']='qt-sections-editing'
    report.data['test_sources_sha256']={name:q.common.digest_file(q.ROOT/name) for name in ('qt_sections_editing.py','qt_migration.py','native_linesplan_app.py','run_tests.py','qt/editing_tests.cpp')}
    before=q.common.fingerprint(args.source)[0];qt_before=q.current_sources(args.qt_source)
    protected={str(p):q.common.digest_file(p) for p in [*models,args.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe'] if p.exists()}
    try:
        bridge=q.prepare_bridge(args,out,q.bridge_builder(args.qt_source),report);runtime=q.prepare_runtime(args,out,bridge,report)
        for i,model in enumerate(models,1):
            folder=out/f'model-{i}';folder.mkdir();private=folder/'input.fbm';shutil.copy2(model,private)
            entry={'model':str(model),'sha256':q.common.digest_file(model)};report.data['models'].append(entry)
            try:
                stages=recipe(report,runtime/'bridge/FreeShipGeometryBridge.exe',private,folder/'protocol')
                entry['production']=q.run_child(report,f'{i}.production',runtime/'FreeShipQt.exe',['--verify-sections',private],folder/'production',timeout=300)
                if entry['production']['status'] in ('error','fail'):
                    continue
                artifacts=entry['production']['artifacts']
                outref=folder/'saved-reference';outref.mkdir()
                with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',folder/'saved-reference-service',native_linesplan=False) as service:
                    response=service.command('load',output=outref,path=str(folder/'production/sections-saved.fbm'),document_id=str(uuid.uuid4()))
                    reference=q.validate_snapshot(report,f'{i}.saved-reference',response,outref)
                report.compare(f'{i}.actual-dialog-save-dxf-exact',artifacts['reopened_dxf'],reference['paths']['dxf'])
                native.compare_scene(report,f'{i}.actual-dialog-save-scene',reference['paths']['scene'],Path(artifacts['reopened_scene']))
                if i==1:
                    entry['widgets']=q.run_child(report,'widgets',runtime/'qt_editing_tests.exe',[stages['original']['paths']['model']],folder/'widgets')
                    cache_recovery(report,runtime,stages['original']['paths']['model'].parent/'native-input.json',folder/'cache-probe')
                    if model.name=='box.fbm':last_layer_guard(report,runtime,private,folder/'last-layer-probe')
            except Exception as error:report.case(f'{i}.execution',status='error',reason=str(error))
    except Exception as error:report.case('suite.execution',status='error',reason=str(error))
    finally:
        report.case('legacy-source-unchanged',q.common.fingerprint(args.source)[0]==before)
        report.case('qt-source-unchanged',q.current_sources(args.qt_source)==qt_before)
        for path,digest in protected.items():report.case('protected.'+path,q.common.digest_file(Path(path))==digest)
        report.case('curvature-comb',status='not_run',reason='Stored flag retained; curvature comb rendering and global scale control remain unported.')
        report.case('ui-parity',status='review',reason='Original StudioLight icons and section categories retained; full original dialog and overlay fidelity remains pending.')
        report.case('desktop-acceptance',status='not_run',reason='Hidden production workflows only; no computer-use automation.')
        report.finish();(out/'report.json').write_text(json.dumps(report.data,ensure_ascii=False,indent=2,default=str),encoding='utf8')
    print(json.dumps({'status':report.data['status'],'counts':report.data['counts'],'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'partial':2,'fail':1,'error':3}[report.data['status']]
if __name__=='__main__':raise SystemExit(main())
