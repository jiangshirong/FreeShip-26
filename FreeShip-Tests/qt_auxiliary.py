"""Control curves and flowlines: production actions, original-core splines, atomic history and FBM/DXF."""
from pathlib import Path
import argparse, json, shutil, uuid
import qt_migration as q
import qt_editing as editing
import qt_editing_fixture as fixture
import qt_backgrounds as backgrounds
import native_linesplan_app as native

PHASES=('original','control','moved','undo_move','redo_move','flow','deleted_curve','restored_curve','deleted_flow','restored_flow','reopened')


def auxiliary(model, stable=False):
    points=editing.points(model)
    return {
        'control_curves':[{**({'id':c['id']} if stable else {}),'control_positions':[points[i] for i in c['controls']],'spline':c['spline']} for c in model.get('control_curves',[])],
        'flowlines':[{**({'id':c['id']} if stable else {}),'projection_point':c['source'],'view':c['view'],'spline':c['spline']} for c in model.get('flowlines',[])]}


def inspect_saved(r,prefix,probe,saved,model,out):
    actual=auxiliary(model);expected=fixture.run(probe,saved,out)
    for key in actual:
        r.case(prefix+'.old-core-'+key,actual[key]==expected[key],counts=[len(actual[key]),len(expected[key])])
    for key in ('show_control_curves','show_flowlines'):
        r.case(prefix+'.old-core-'+key,expected['display'][key]==model['display'][key])


def service_cases(r,runtime,model,probe,out):
    out.mkdir();exe=runtime/'bridge/FreeShipGeometryBridge.exe'
    with q.Service(exe,out/'service',native_linesplan=True) as service:
        def take(name,method='snapshot',**fields):return editing.take(r,'service',service,out,name,method,**fields)
        original=take('original','load',path=str(model));current=original
        for key,kind in (('control_curves','curve'),('flowlines','flow')):
            if current['model'][key]:current=take('clear-'+kind,'auxiliary',operation='delete',kind=kind,ids=[c['id'] for c in current['model'][key]])
        base=current;edge_map={frozenset((e['a'],e['b'])):e['id'] for e in base['model']['control_edges']}
        face=base['model']['control_faces'][0]['points'];cycle=[edge_map[frozenset((a,b))] for a,b in zip(face,face[1:]+face[:1])]
        closed=take('closed','auxiliary',operation='create_control',ids=cycle)
        curves=closed['model']['control_curves'];r.case('service.closed-curve-keeps-repeated-endpoint',len(curves)==1 and curves[0]['controls'][0]==curves[0]['controls'][-1] and len(curves[0]['controls'])==len(cycle)+1)
        r.case('service.curve-definition-keeps-hull',q.geometry(closed['model'])==q.geometry(base['model']))
        before=dict(service.state);same=take('repeat','auxiliary',operation='create_control',ids=cycle)
        r.case('service.already-assigned-edges-are-noop',service.state==before and auxiliary(same['model'],True)==auxiliary(closed['model'],True))
        undo=take('undo-closed','undo');r.case('service.undo-closed',auxiliary(undo['model'],True)==auxiliary(base['model'],True))
        redo=take('redo-closed','redo');r.case('service.redo-closed-stable-id',auxiliary(redo['model'],True)==auxiliary(closed['model'],True))
        # A failed publication must undo even newly allocated curve definitions.
        free=next(e['id'] for e in base['model']['control_edges'] if e['id'] not in cycle)
        blocked=out/'blocked';blocked.mkdir();(blocked/'model.json').mkdir();before=dict(service.state)
        rejected=service.command('auxiliary',output=blocked,operation='create_control',ids=[free]);r.case('service.failed-publication-rejected',not rejected['ok']);r.case('service.failed-publication-keeps-history',service.state==before)
        after=take('after-publication-failure');r.case('service.failed-publication-restores-curves',auxiliary(after['model'],True)==auxiliary(redo['model'],True))
        take('remove-closed','auxiliary',operation='delete',kind='curve',ids=[curves[0]['id']])
        adjacency={}
        for e in base['model']['control_edges']:
            for point in (e['a'],e['b']):adjacency.setdefault(point,[]).append(e['id'])
        branch=next(es[:3] for es in adjacency.values() if len(es)>=3)
        branched=take('branched','auxiliary',operation='create_control',ids=branch)
        r.case('service.branch-splits-at-junction',len(branched['model']['control_curves'])==3 and all(len(c['controls'])==2 for c in branched['model']['control_curves']))
        r.case('service.branch-keeps-hull',q.geometry(branched['model'])==q.geometry(base['model']))
        take('undo-branch','undo')
        before=dict(service.state)
        invalid=[('duplicate',dict(operation='create_control',ids=[free,free])),('missing',dict(operation='delete',kind='curve',ids=[999999])),('kind',dict(operation='delete',kind='face',ids=[1])),('view',dict(operation='create_flow',view=3,source=[0,0])),('no-hit',dict(operation='create_flow',view=1,source=[1e8,1e8])),('stale',dict(operation='create_control',ids=[free],expected_revision=1))]
        for name,fields in invalid:
            response=service.command('auxiliary',output=out/('reject-'+name),**fields)
            r.case('service.reject-'+name,not response['ok'],error=response.get('error'));r.case('service.preserve-after-'+name,service.state==before)
        restored=take('after-rejections');r.case('service.rejected-edits-keep-model',q.geometry(restored['model'])==q.geometry(base['model']) and auxiliary(restored['model'])==auxiliary(base['model']))
        first=base['model']['control_edges'][0];second=next(e for e in base['model']['control_edges'] if not {e['a'],e['b']}&{first['a'],first['b']})
        separate=take('disconnected','auxiliary',operation='create_control',ids=[first['id'],second['id']]);r.case('service.disconnected-edges-create-two-curves',len(separate['model']['control_curves'])==2)
        r.case('service.deleted-curve-id-not-reused',all(c['id']>curves[0]['id'] for c in separate['model']['control_curves']))
        hidden=take('hidden','display',changes={'show_control_curves':False,'show_flowlines':False});saved=editing.saved_copy(service,out/'saved');inspect_saved(r,'service',probe,saved,hidden['model'],out/'old-core')
        # Geometry edits must keep stable curve IDs and regenerate their splines.
        point=next(p for p in separate['model']['control_points'] if p['id']==first['a']);position=list(point['p']);position[2]+=.025
        moved=take('point-moved','move_point',point_id=point['id'],position=position)
        r.case('service.point-edit-keeps-curve-ids',[c['id'] for c in moved['model']['control_curves']]==[c['id'] for c in hidden['model']['control_curves']])
        r.case('service.point-edit-regenerates-curve',auxiliary(moved['model'])!=auxiliary(hidden['model']))
        saved=editing.saved_copy(service,out/'moved-save');inspect_saved(r,'service.moved',probe,saved,moved['model'],out/'moved-old-core')
        deleted=take('edge-deleted','topology',operation='delete',kind='edge',ids=[first['id']]);r.case('service.edge-delete-prunes-dependent-curve',len(deleted['model']['control_curves'])==1)
        restored=take('undo-edge-delete','undo');r.case('service.undo-edge-delete-restores-curves',auxiliary(restored['model'],True)==auxiliary(moved['model'],True))
    # Existing low-version files must be promoted before history/save can hold curves.
    seed=fixture.run(probe,model,out/'seed','seed')
    with q.Service(exe,out/'legacy-service',native_linesplan=True) as service:
        legacy=editing.take(r,'legacy',service,out,'legacy-load','load',path=seed['artifacts']['legacy.fbm'])
        changed=editing.take(r,'legacy',service,out,'legacy-curve','auxiliary',operation='create_control',ids=[legacy['model']['control_edges'][0]['id']])
        saved=editing.saved_copy(service,out/'legacy-save');inspect_saved(r,'legacy',probe,saved,changed['model'],out/'legacy-old-core')


def flow_cases(r,runtime,saved,model,probe,out):
    out.mkdir();exe=runtime/'bridge/FreeShipGeometryBridge.exe'
    flow=model['flowlines'][-1];point=flow['spline']['points'][0]
    requests=[{'name':'profile','view':1,'source':[point[0],point[2]]},
              {'name':'plan','view':2,'source':[point[0],point[1]]},
              {'name':'body','view':0,'source':[point[1],point[2]]},
              {'name':'outside','view':1,'source':[1e8,1e8]}]
    original=fixture.run(probe,saved,out/'flow-oracle','flow',requests)['flowlines']
    with q.Service(exe,out/'service',native_linesplan=True) as service:
        def take(name,method='snapshot',**fields):return editing.take(r,'flow',service,out,name,method,**fields)
        current=take('loaded','load',path=str(saved))
        for request,expected in zip(requests,original):
            before=dict(service.state);name=request['name'];folder=out/('add-'+name)
            response=service.command('auxiliary',output=folder,operation='create_flow',view=request['view'],source=request['source'])
            if len(expected['points'])<2:
                r.case('flow.'+name+'.rejects-empty-or-stagnant-trace',not response['ok']);r.case('flow.'+name+'.keeps-state',service.state==before)
            else:
                current=q.validate_snapshot(r,'flow.'+name,response,folder)
                r.case('flow.'+name+'.exact-old-core-trace',current['model']['flowlines'][-1]['spline']=={k:expected[k] for k in ('points','knuckles')},points=len(expected['points']))
        reference=current['model'];ids=[f['id'] for f in reference['flowlines']]
        p=next(p for p in reference['control_points'] if p['batch_movable'] and not p['locked']);position=list(p['p']);position[2]+=.03125
        moved=take('point-moved','move_point',point_id=p['id'],position=position)
        r.case('flow.move-keeps-identities',[f['id'] for f in moved['model']['flowlines']]==ids)
        target=editing.saved_copy(service,out/'moved-save');inspect_saved(r,'flow.moved',probe,target,moved['model'],out/'moved-old-core')
        undone=take('undo-move','undo');r.case('flow.undo-restores-exact-splines',auxiliary(undone['model'],True)==auxiliary(reference,True))
        units='imperial' if reference['units']=='metric' else 'metric';factor=1/.3048 if units=='imperial' else .3048
        converted=take('converted','update_project',changes={'units':units})
        r.case('flow.units-scale-sources-once',[f['source'] for f in converted['model']['flowlines']]==[[editing.f32(x*factor) for x in f['source']] for f in reference['flowlines']])
        target=editing.saved_copy(service,out/'converted-save');inspect_saved(r,'flow.converted',probe,target,converted['model'],out/'converted-old-core')
        precision=0 if converted['model']['settings']['precision'] else 1
        refined=take('precision','update_project',changes={'precision':precision});target=editing.saved_copy(service,out/'precision-save');inspect_saved(r,'flow.precision',probe,target,refined['model'],out/'precision-old-core')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT)
    p.add_argument('--reuse-build',type=Path);p.add_argument('--reuse-probe',type=Path);p.add_argument('--runtime',type=Path)
    p.add_argument('--model',type=Path,action='append');p.add_argument('--verify-deployment',type=Path);p.add_argument('--workflow-reference',type=Path)
    a=p.parse_args();a.source=a.source.resolve();a.qt_source=a.qt_source.resolve();out=a.output.resolve();models=[x.resolve(strict=True) for x in (a.model or q.DEFAULT_MODELS)]
    if a.verify_deployment:
        if not a.workflow_reference:p.error('--workflow-reference is required for deployment verification')
        return q.verify_deployment(a,models,out,'--verify-auxiliary',PHASES,('scene','dxf','sections'))
    for root in (a.source,a.qt_source,q.PILOT):
        if root==out or root in out.parents:p.error('Use a private report directory')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data['suite']='qt-auxiliary'
    legacy=q.common.fingerprint(a.source)[0];sources=q.current_sources(a.qt_source)
    protected={str(x):q.common.digest_file(x) for x in [*models,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe']}
    tests=('qt_auxiliary.py','qt/auxiliary_tests.cpp','QtEditingFixture.pas','qt_editing_fixture.py','qt_migration.py','qt_backgrounds.py','run_tests.py')
    r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in tests}
    try:
        bridge=q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),r);runtime=q.prepare_runtime(a,out,bridge,r);probe=backgrounds.prepare_oracle(a,out,r)
        for i,model in enumerate(models,1):
            folder=out/f'model-{i}';folder.mkdir();private=folder/'input.fbm';shutil.copy2(model,private)
            child=q.run_child(r,f'{i}.production',runtime/'FreeShipQt.exe',['--verify-auxiliary',private],folder/'production',platform='offscreen')
            r.data['models'].append({'model':str(model),'sha256':q.common.digest_file(model),'production':child})
            if any(c.get('name')=='auxiliary_workflow_unavailable' for c in child.get('cases',[])) or child['status']=='error':
                r.case(f'{i}.dependent-artifacts',status='not_run',reason='Production workflow unavailable; see discovery/error');continue
            artifacts=child['artifacts'];states={phase:json.loads(Path(artifacts[phase+'_model']).read_text('utf8')) for phase in PHASES}
            for later,earlier in (('control','original'),('undo_move','control'),('redo_move','moved'),('flow','moved'),('deleted_curve','flow'),('restored_curve','flow'),('deleted_flow','flow'),('restored_flow','flow'),('reopened','flow')):
                r.case(f'{i}.{later}.unchanged-hull',native.reopened_geometry(states[later])==native.reopened_geometry(states[earlier]))
                for key in ('scene','dxf','sections'):r.compare(f'{i}.{later}.same-'+key,Path(artifacts[later+'_'+key]),Path(artifacts[earlier+'_'+key]))
            for later,earlier in (('undo_move','control'),('redo_move','moved'),('restored_curve','flow'),('restored_flow','flow')):
                r.case(f'{i}.{later}.stable-auxiliary-ids',auxiliary(states[later],True)==auxiliary(states[earlier],True))
            r.case(f'{i}.reopened-auxiliary-exact',auxiliary(states['reopened'])==auxiliary(states['flow']))
            r.case(f'{i}.delete-curve-detaches-definition',len(states['deleted_curve']['control_curves'])==len(states['flow']['control_curves'])-1)
            r.case(f'{i}.delete-flow-detaches-definition',len(states['deleted_flow']['flowlines'])==len(states['flow']['flowlines'])-1)
            saved=Path(artifacts['saved_file']);inspect_saved(r,f'{i}.saved',probe,saved,states['reopened'],folder/'saved-inspection')
            with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',folder/'legacy-export-service',native_linesplan=False) as service:
                baseline=editing.take(r,f'{i}.old',service,folder,'old-export','load',path=str(saved))
                r.case(f'{i}.dxf-exact-original-core',native.numeric_entities(baseline['paths']['dxf'])==native.numeric_entities(Path(artifacts['reopened_dxf'])))
                native.compare_scene(r,f'{i}.scene-original-core',baseline['paths']['scene'],Path(artifacts['reopened_scene']))
            if i==1:
                q.run_child(r,'widgets',runtime/'qt_auxiliary_tests.exe',[artifacts['control_model']],folder/'widgets',platform='offscreen')
                q.run_child(r,'editing-regression',runtime/'qt_editing_tests.exe',[artifacts['original_model']],folder/'editing-regression',platform='offscreen')
                service_cases(r,runtime,private,probe,folder/'service-cases')
                flow_cases(r,runtime,saved,states['reopened'],probe,folder/'flow-cases')
            q.run_child(r,f'{i}.lines-regression',runtime/'qt_lines_tests.exe',[artifacts['reopened_scene']],folder/'lines-regression',platform='offscreen')
    except Exception as e:r.case('suite.execution',status='error',reason=f'{type(e).__name__}: {e}')
    finally:
        r.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==legacy);r.case('qt-source-unchanged',q.current_sources(a.qt_source)==sources)
        r.case('test-source-unchanged',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        for path,h in protected.items():r.case('protected.'+path,q.common.digest_file(Path(path))==h)
        r.case('desktop-acceptance',status='not_run',reason='Offscreen widgets/events only');r.finish();(out/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2,default=str),encoding='utf8')
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'report':str(out/'report.json')},ensure_ascii=False));return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]
if __name__=='__main__':raise SystemExit(main())
