"""Isolated topology editing and StudioLight integration through production routes."""
from pathlib import Path
from collections import Counter
import argparse
import copy
import json
import shutil
import uuid

import qt_migration as q
import native_linesplan_app as native

PHASES=('original','split','undo','redo','reopened','created-face','deleted-face','complete','complete-reopened')


def recipe(report, executable, model, folder):
    stages={}
    with q.Service(executable,folder/'native-service',native_linesplan=True) as current, \
         q.Service(executable,folder/'legacy-service',native_linesplan=False) as legacy:
        def capture(name,method='snapshot',**parameters):
            outputs=[]
            for tag,service in [('native',current),('legacy',legacy)]:
                out=folder/name/tag;out.mkdir(parents=True,exist_ok=True)
                response=service.command(method,output=out,**parameters)
                outputs.append(q.validate_snapshot(report,name+'.'+tag,response,out))
            a,b=outputs
            report.case(name+'.attributed-model-equal',q.geometry(a['model'])==q.geometry(b['model']))
            report.compare(name+'.complete-dxf',a['paths']['dxf'],b['paths']['dxf'])
            native.compare_scene(report,name,b['paths']['scene'],a['paths']['scene'])
            stages[name]=a
            return a['model']
        original=capture('original','load',path=str(model),document_id=str(uuid.uuid4()))
        points={p['id']:p for p in original['control_points']}
        incidence=Counter(tuple(sorted((face['points'][i],face['points'][(i+1)%len(face['points'])])))
                          for face in original['control_faces'] for i in range(len(face['points'])))
        interior=next((e for e in original['control_edges'] if incidence[tuple(sorted((e['a'],e['b'])))]==2
                       and all(points[p].get('batch_movable',not points[p]['locked']) for p in (e['a'],e['b']))),None)
        if interior:
            changed=capture('crease-change','topology',operation='set_crease',ids=[interior['id']],crease=not interior['crease'])
            report.case('crease.only-selected-edge-property',changed['control_points']==original['control_points']
                        and changed['control_faces']==original['control_faces']
                        and all(e==({**interior,'crease':not interior['crease']} if e['id']==interior['id'] else next(x for x in original['control_edges'] if x['id']==e['id']))
                                for e in changed['control_edges']))
            restored=capture('crease-undo','undo');report.case('crease.undo-exact',q.geometry(restored)==q.geometry(original))
        else:report.case('crease.interior-edit',status='not_run',reason='Fixture has no editable two-face edge')
        edge=next(e for e in original['control_edges'] if all(points[p].get('batch_movable',not points[p]['locked']) for p in (e['a'],e['b'])))
        split=capture('split','topology',operation='split_edge',edge_id=edge['id'])
        new_points=[p for p in split['control_points'] if p['id'] not in points]
        midpoint=[float(native.editing.f32((a+b)/2)) for a,b in zip(points[edge['a']]['p'],points[edge['b']]['p'])]
        report.case('split.independent-midpoint',len(new_points)==1 and new_points[0]['p']==midpoint)
        report.case('split.original-point-identities',all(p in split['control_points'] for p in original['control_points']))
        report.case('split.topology-counts',len(split['control_edges'])==len(original['control_edges'])+1 and len(split['control_faces'])==len(original['control_faces']))
        undone=capture('undo','undo');report.case('undo.complete-original-model',q.geometry(undone)==q.geometry(original))
        redone=capture('redo','redo');report.case('redo.complete-split-model',q.geometry(redone)==q.geometry(split))
        # Save/reopen through each service before making more changes. The fresh
        # document may renumber IDs, but exact attributed topology must survive.
        for tag,service in [('native',current),('legacy',legacy)]:
            out=folder/'save'/tag;out.mkdir(parents=True,exist_ok=True)
            saved=q.require_ok(service.command('save',output=out));q.require_ok(service.command('mark_saved'))
            target=folder/'reopened'/tag;target.mkdir(parents=True,exist_ok=True)
            result=service.command('load',output=target,path=saved['saved_file'],document_id=str(uuid.uuid4()))
            reopened=q.validate_snapshot(report,'reopen.'+tag,result,target)['model']
            report.case('reopen.'+tag+'.attributed-topology',native.reopened_geometry(reopened)==native.reopened_geometry(redone))
        # Start a common document again so the following cross-engine sequence
        # uses the same document-scoped IDs independently of allocator order.
        capture('reload','load',path=str(model),document_id=str(uuid.uuid4()))
        capture('split-again','topology',operation='split_edge',edge_id=edge['id'])
        capture('branch-undo','undo')
        branch=capture('branch','topology',operation='add_point',position=[2,3,4])
        branch_point=next(p for p in branch['control_points'] if p['id'] not in points)
        report.case('branch.never-reuses-undone-id',branch_point['id']>new_points[0]['id'])
        report.case('branch.drops-redo',not current.state['can_redo'])
        deleted=capture('delete-loose','topology',operation='delete',kind='point',ids=[branch_point['id']])
        report.case('delete.restores-original-geometry',q.geometry(deleted)==q.geometry(original))
        before=dict(current.state)
        failure=current.command('move_point',output=folder/'deleted-id-reject',point_id=branch_point['id'],position=[9,9,9])
        report.case('delete.old-id-rejected',not failure['ok'] and current.state==before)
        capture('after-reject')
        # An independent triangle is constructed in an empty area of the hull.
        x=max(p['p'][0] for p in original['control_points'])+4
        z=max(p['p'][2] for p in original['control_points'])+2
        ids=[];known=set(points)
        for index,position in enumerate(([x,1,z],[x+2,1,z],[x,3,z])):
            result=capture('new-point-'+str(index),'topology',operation='add_point',position=position)
            created=next(p for p in result['control_points'] if p['id'] not in known)
            known.add(created['id']);ids.append(created['id'])
        triangle=capture('triangle','topology',operation='add_face',ids=ids,layer_id=original['layers'][0]['id'])
        faces={f['id'] for f in original['control_faces']}
        face=next(f for f in triangle['control_faces'] if f['id'] not in faces)
        report.case('face.ordered-vertices-and-layer',face['points']==ids and face['layer']==original['layers'][0]['id'])
        known_edges={e['id'] for e in original['control_edges']}
        new_edges=[e for e in triangle['control_edges'] if e['id'] not in known_edges]
        report.case('face.independent-three-boundary-edges',len(new_edges)==3 and {tuple(sorted((e['a'],e['b']))) for e in new_edges}=={tuple(sorted((ids[i],ids[(i+1)%3]))) for i in range(3)})
        for name,parameters in [
            ('duplicate-face',dict(operation='add_face',ids=ids,layer_id=face['layer'])),
            ('repeated-vertex',dict(operation='add_face',ids=[ids[0],ids[1],ids[0]],layer_id=face['layer'])),
            ('boundary-uncrease',dict(operation='set_crease',ids=[new_edges[0]['id']],crease=False)),
            ('unknown-edge',dict(operation='split_edge',edge_id=999999)),
            ('empty-selection',dict(operation='delete',kind='point',ids=[]))]:
            before=dict(current.state)
            result=current.command('topology',output=folder/name,**parameters)
            report.case('reject.'+name,not result['ok'] and current.state==before,error=result.get('error'))
        capture('rejection-rollback')
        locked=capture('locked','topology',operation='set_locked',ids=[ids[0]],locked=True)
        before=dict(current.state)
        result=current.command('topology',output=folder/'locked-delete',operation='delete',kind='face',ids=[face['id']])
        report.case('reject.locked-neighbor-face',not result['ok'] and current.state==before)
        capture('unlocked','topology',operation='set_locked',ids=[ids[0]],locked=False)
        state=dict(current.state)
        capture('same-layer','topology',operation='assign_layer',ids=[face['id']],layer_id=face['layer'])
        report.case('same-layer.no-history-or-revision',current.state==state)
        capture('deleted-face','topology',operation='delete',kind='face',ids=[face['id']])
        restored=capture('undo-face','undo');report.case('undo-face.exact',q.geometry(restored)==q.geometry(triangle))
        report.case('old-ids.still-correct',all(p in restored['control_points'] for p in original['control_points']))
        # Missing worker fault is injected only in a private copied runtime.
        engine=executable.parent.parent/'FreeShipNativeLinesplan.exe';parked=engine.with_suffix('.parked')
        state=dict(current.state);engine.rename(parked)
        try: result=current.command('topology',output=folder/'missing-worker',operation='split_edge',edge_id=edge['id'])
        finally:parked.rename(engine)
        report.case('worker-failure.restores-topology-history',not result['ok'] and current.state==state)
        after=capture('after-worker-failure');report.case('worker-failure.complete-model-preserved',q.geometry(after)==q.geometry(restored))
        # Cascading deletion on the independent triangle has a known incident
        # face and cannot touch any of the original hull objects.
        for kind,selected in [('edge',[e['id'] for e in new_edges]),('point',ids)]:
            gone=capture('cascade-'+kind,'topology',operation='delete',kind=kind,ids=selected)
            report.case('cascade-'+kind+'.removes-incident-face',all(f['id']!=face['id'] for f in gone['control_faces']))
            report.case('cascade-'+kind+'.preserves-unrelated-hull',all(p in gone['control_points'] for p in original['control_points'])
                        and all(e in gone['control_edges'] for e in original['control_edges'])
                        and all(f in gone['control_faces'] for f in original['control_faces']))
            restored=capture('cascade-'+kind+'-undo','undo')
            report.case('cascade-'+kind+'.undo-full-model',q.geometry(restored)==q.geometry(triangle))
    return stages


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE)
    p.add_argument('--qt-source',type=Path,default=q.QT);p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path)
    p.add_argument('--model',type=Path,action='append');p.add_argument('--verify-deployment',type=Path);p.add_argument('--workflow-reference',type=Path)
    args=p.parse_args();args.source=args.source.resolve();args.qt_source=args.qt_source.resolve();out=args.output.resolve()
    models=[m.resolve(strict=True) for m in (args.model or q.DEFAULT_MODELS)]
    if args.verify_deployment:return q.verify_deployment(args,models,out,'--verify-topology',PHASES)
    for protected in (args.source,args.qt_source,q.PILOT):
        if out==protected or protected in out.parents:p.error('Use an isolated report directory')
    out.mkdir(parents=True,exist_ok=False);report=q.Report();report.data['suite']='qt-topology'
    report.data['test_sources_sha256']={name:q.common.digest_file(q.ROOT/name) for name in ('qt_topology.py','qt_migration.py','native_linesplan_app.py','run_tests.py','qt/editing_tests.cpp')}
    report.data['limitations']=['StudioLight assets and main shell are migrated; auxiliary lines, startup animations, remaining dialogs and full visual parity remain pending.',
        'Hidden Qt workflows do not establish desktop input or visual acceptance.',
        'Basic topology operations are covered; collapse/extrude/mirror and full topology-core migration are separate work.']
    before=q.common.fingerprint(args.source)[0];qt_before=q.current_sources(args.qt_source)
    protected={str(path):q.common.digest_file(path) for path in [*models,args.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe'] if path.exists()}
    try:
        bridge=q.prepare_bridge(args,out,q.bridge_builder(args.qt_source),report);runtime=q.prepare_runtime(args,out,bridge,report)
        icons=args.source/'Themes/StudioLight/icons';manifest=json.loads((icons/'manifest.json').read_text('utf8'))
        report.case('studio.original-icon-manifest',q.common.digest_file(icons/'manifest.json')==q.common.digest_file(runtime/'bridge/Themes/StudioLight/icons/manifest.json'))
        for path in icons.rglob('*.png'):
            report.case('studio.asset.'+path.relative_to(icons).as_posix(),q.common.digest_file(path)==q.common.digest_file(runtime/'bridge/Themes/StudioLight/icons'/path.relative_to(icons)))
        for index,model in enumerate(models,1):
            folder=out/f'model-{index}';folder.mkdir();private=folder/'input.fbm';shutil.copy2(model,private)
            entry={'model':str(model),'sha256':q.common.digest_file(model)};report.data['models'].append(entry)
            try:
                stages=recipe(report,runtime/'bridge/FreeShipGeometryBridge.exe',private,folder/'protocol')
                entry['production']=q.run_child(report,f'{index}.production',runtime/'FreeShipQt.exe',['--verify-topology',private],folder/'production')
                for phase in ('original','split','undo','redo'):
                    for kind in ('scene','dxf'):report.compare(f'{index}.production.{phase}.{kind}',entry['production']['artifacts'][phase+'_'+kind],stages[phase]['paths'][kind])
                # Independently reload the file saved by the actual dialogs in
                # the old reference route and compare its complete outputs.
                target=folder/'saved-reference';target.mkdir()
                with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',folder/'saved-reference-service',native_linesplan=False) as service:
                    response=service.command('load',output=target,path=str(folder/'production/complete-topology-saved.fbm'),document_id=str(uuid.uuid4()))
                    reference=q.validate_snapshot(report,f'{index}.saved-reference',response,target)
                artifacts=entry['production']['artifacts']
                report.compare(f'{index}.complete-saved-dxf-exact',artifacts['complete-reopened_dxf'],reference['paths']['dxf'])
                native.compare_scene(report,f'{index}.complete-saved-scene',reference['paths']['scene'],Path(artifacts['complete-reopened_scene']))
                if index==1:entry['editing_widgets']=q.run_child(report,'editing-widgets',runtime/'qt_editing_tests.exe',[stages['original']['paths']['model']],folder/'editing-widgets')
            except Exception as error:report.case(str(index)+'.execution',status='error',reason=str(error))
    except Exception as error:report.case('suite.execution',status='error',reason=str(error))
    finally:
        report.case('legacy-source-unchanged',q.common.fingerprint(args.source)[0]==before)
        report.case('qt-source-unchanged',q.current_sources(args.qt_source)==qt_before)
        for path,digest in protected.items():report.case('protected.'+path,q.common.digest_file(Path(path))==digest)
        report.case('ui-visual-parity',status='review',reason='Main shell, original icons and colors have evidence; full UI parity includes remaining overlays/animations/dialogs.')
        report.case('desktop-acceptance',status='not_run',reason='No desktop automation was used.')
        report.finish();(out/'report.json').write_text(json.dumps(report.data,ensure_ascii=False,indent=2,default=str),encoding='utf8')
    print(json.dumps({'status':report.data['status'],'counts':report.data['counts'],'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'partial':2,'fail':1,'error':3}[report.data['status']]

if __name__=='__main__':raise SystemExit(main())
