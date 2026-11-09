"""Qt reference lines, labels and entrance animation; compact reusable suite.

Retains production workflow artifacts, deterministic animation frames and a
machine-readable report. No full copy of the legacy source; no desktop input.
"""
from pathlib import Path
import argparse, json, shutil, uuid
import qt_migration as q
import native_linesplan_app as native

PHASES=('original','moved','undo','redo','reopened')
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE)
    p.add_argument('--qt-source',type=Path,default=q.QT);p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path)
    p.add_argument('--model',type=Path,action='append');p.add_argument('--verify-deployment',type=Path);p.add_argument('--workflow-reference',type=Path)
    a=p.parse_args();a.source=a.source.resolve();a.qt_source=a.qt_source.resolve();out=a.output.resolve()
    models=[x.resolve(strict=True) for x in (a.model or q.DEFAULT_MODELS)]
    if a.verify_deployment:return q.verify_deployment(a,models,out,'--verify-overlays',PHASES,('scene','dxf','sections'))
    for root in (a.source,a.qt_source,q.PILOT):
        if out==root or root in out.parents:p.error('Use an isolated report directory')
    out.mkdir(parents=True,exist_ok=False);report=q.Report();report.data['suite']='qt-overlays'
    report.data['test_sources_sha256']={name:q.common.digest_file(q.ROOT/name) for name in ('qt_overlays.py','qt/overlay_tests.cpp','qt_migration.py','run_tests.py')}
    before=q.common.fingerprint(a.source)[0];qt_before=q.current_sources(a.qt_source)
    protected={str(x):q.common.digest_file(x) for x in [*models,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe'] if x.exists()}
    try:
        bridge=q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),report);runtime=q.prepare_runtime(a,out,bridge,report)
        for i,model in enumerate(models,1):
            folder=out/f'model-{i}';folder.mkdir();private=folder/'input.fbm';shutil.copy2(model,private)
            entry={'model':str(model),'sha256':q.common.digest_file(model)};report.data['models'].append(entry)
            try:
                child=q.run_child(report,f'{i}.production',runtime/'FreeShipQt.exe',['--verify-overlays',private],folder/'production',timeout=240,platform='offscreen');entry['production']=child
                artifacts=child.get('artifacts',{})
                with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',folder/'reference-service',native_linesplan=False) as service:
                    original=None
                    for phase in PHASES:
                        dest=folder/'reference'/phase;dest.mkdir(parents=True)
                        current=json.loads(Path(artifacts[phase+'_model']).read_text('utf8'))
                        fields={};method='snapshot'
                        if phase=='original':method='load';fields={'path':str(private),'document_id':str(uuid.uuid4())};original=current
                        elif phase=='moved':
                            old={x['id']:x for x in original['control_points']};changed=[x for x in current['control_points'] if x['p']!=old[x['id']]['p']]
                            if len(changed)!=1:raise ValueError('Expected one edited control point in production workflow')
                            method='move_point';fields={'point_id':changed[0]['id'],'position':changed[0]['p']}
                        elif phase in ('undo','redo'):method=phase
                        elif phase=='reopened':
                            saved=folder/'production/工作流另存.fbm'
                            # Discover the FBM actually produced by the existing
                            # workflow rather than assuming a dialog filename.
                            choices=list((folder/'production').glob('*.fbm'))
                            if len(choices)!=1:raise ValueError('Saved production FBM is missing or ambiguous')
                            method='load';fields={'path':str(choices[0]),'document_id':str(uuid.uuid4())}
                        response=service.command(method,output=dest,**fields)
                        expected=q.validate_snapshot(report,f'{i}.{phase}.reference',response,dest)
                        report.compare(f'{i}.{phase}.dxf-exact',artifacts[phase+'_dxf'],expected['paths']['dxf'])
                        native.compare_scene(report,f'{i}.{phase}.scene',expected['paths']['scene'],Path(artifacts[phase+'_scene']))
                        report.case(f'{i}.{phase}.overlay-matches-original-core',current.get('overlay')==expected['model'].get('overlay'))
                        if phase=='reopened':
                            report.case(f'{i}.reopen-attributed-geometry',native.reopened_geometry(current)==native.reopened_geometry(expected['model']))
                        else:report.case(f'{i}.{phase}.geometry-exact',q.geometry(current)==q.geometry(expected['model']))
                original_path=Path(artifacts['original_model'])
                edited=json.loads(Path(artifacts['moved_model']).read_text('utf8'))
                redone=json.loads(Path(artifacts['redo_model']).read_text('utf8'))
                report.case(f'{i}.redo-restores-hydro-markers-and-SAC',edited['overlay']==redone['overlay'])
                report.compare(f'{i}.redo-restores-scene-bytes',artifacts['moved_scene'],artifacts['redo_scene'])
                if model.name=='box.fbm':
                    initial=json.loads(original_path.read_text('utf8'));markers={x['key']:x for x in initial['overlay']['hydro']['markers']}
                    # Independent rectangular hull: 20m * 6m * 4m * 1.025 t/m3.
                    report.case('box-independent-displacement',markers['displacement']['text']=='Displ=492.00')
                    report.case('box-independent-buoyancy-center',markers['displacement']['p']==[10,0,2])
                    moved={x['key']:x for x in edited['overlay']['hydro']['markers']}
                    report.case('point-edit-invalidates-hydro-cache',moved['displacement']!=markers['displacement'])
                entry['widgets']=q.run_child(report,f'{i}.widgets',runtime/'qt_overlay_tests.exe',[original_path],folder/'widgets',timeout=180,platform='offscreen')
                if i==1:
                    entry['editing_regression']=q.run_child(report,'editing-regression',runtime/'qt_editing_tests.exe',[original_path],folder/'editing-regression',platform='offscreen')
                entry['lines_regression']=q.run_child(report,f'{i}.lines-regression',runtime/'qt_lines_tests.exe',[artifacts['original_scene']],folder/'lines-regression',platform='offscreen')
                report.case(f'{i}.input-unchanged',q.common.digest_file(private)==entry['sha256'])
            except Exception as error:report.case(f'{i}.execution',status='error',reason=f'{type(error).__name__}: {error}')
    except Exception as error:report.case('suite.execution',status='error',reason=f'{type(error).__name__}: {error}')
    finally:
        report.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==before)
        report.case('qt-source-unchanged',q.current_sources(a.qt_source)==qt_before)
        for path,digest in protected.items():report.case('protected.'+path,q.common.digest_file(Path(path))==digest)
        report.case('desktop-acceptance',status='not_run',reason='Hidden production actions and offscreen synthetic events only.')
        report.case('visual-parity',status='review',reason='Compare saved four-view/animation images with the StudioLight baseline; no claim of complete UI migration.')
        report.finish();(out/'report.json').write_text(json.dumps(report.data,ensure_ascii=False,indent=2,default=str),encoding='utf8')
    print(json.dumps({'status':report.data['status'],'counts':report.data['counts'],'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'partial':2,'fail':1,'error':3}[report.data['status']]
if __name__=='__main__':raise SystemExit(main())
