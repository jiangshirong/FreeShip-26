"""Background image migration: production menus, independent calibration, FBM and original-core inspection.

Retains reusable scripts and compact evidence; validates current build provenance.
Qt menus/dialogs run on the offscreen platform, never on the user's desktop.
"""
from pathlib import Path
import argparse, base64, json, shutil, uuid
import qt_migration as q
import qt_editing as editing
import qt_editing_fixture as fixture
import native_linesplan_app as native

PHASES=('original','imported','moved','undo','redo','calibrated','appearance','removed','restored','reopened')

def backgrounds(model):
    return sorted(model.get('backgrounds',[]),key=lambda b:b['view'])

def prepare_oracle(a,out,report):
    if not a.reuse_probe:return fixture.build(a.source,a.qt_source,out/'oracle-build')
    folder=a.reuse_probe.resolve(strict=True);r=json.loads((folder/'report.json').read_text('utf8'))
    if r['status']!='pass' or r['source_sha256']!=q.common.fingerprint(a.source)[0]:raise ValueError('Stale oracle source')
    if r['adapter_sha256']!=q.bridge_builder(a.qt_source).adapter_hashes():raise ValueError('Stale oracle adapter')
    for name,digest in r['test_sha256'].items():
        if q.common.digest_file(q.ROOT/name)!=digest:raise ValueError('Stale oracle test')
    exe=folder/'QtEditingFixture.exe'
    if q.common.digest_file(exe)!=r['build']['executable_sha256']:raise ValueError('Stale oracle executable')
    report.data['reused_oracle']=str(folder);return exe

def inspect_saved(report,prefix,probe,saved,model,folder):
    observed=fixture.run(probe,saved,folder)
    expected=backgrounds(model);got=sorted(observed['backgrounds'],key=lambda b:b['view'])
    report.case(prefix+'.old-core-image-count',len(got)==len(expected))
    for before,after in zip(expected,got):
        for field in ('view','origin','scale','visible','alpha','transparent','transparent_color','tolerance','quality'):
            report.case(prefix+'.old-core-'+str(before['view'])+'.'+field,before[field]==after[field])
        report.case(prefix+'.old-core-jpeg-'+str(before['view']),base64.b64decode(before['jpeg'])==Path(after['image_file']).read_bytes())

def service_recipe(report,runtime,model,image,probe,folder):
    folder.mkdir();exe=runtime/'bridge/FreeShipGeometryBridge.exe'
    with q.Service(exe,folder/'service',native_linesplan=True) as service:
        def take(name,method='snapshot',**fields):
            dest=folder/name;dest.mkdir();result=q.validate_snapshot(report,'service.'+name,service.command(method,output=dest,**fields),dest)
            if name!='original':
                report.case('service.'+name+'.hull-unchanged',native.reopened_geometry(result['model'])==native.reopened_geometry(original['model']))
                for key in ('scene','dxf'):report.case('service.'+name+'.'+key+'-bytes-unchanged',result['paths'][key].read_bytes()==original['paths'][key].read_bytes())
            return result
        original=take('original','load',path=str(model),document_id=str(uuid.uuid4()))
        for view in range(3):
            current=take('import-'+str(view),'background',operation='import',view=view,path=str(image),scale=.125*(view+1))
            b=next(b for b in backgrounds(current['model']) if b['view']==view)
            report.case('service.import-'+str(view)+'.exact-jpeg',base64.b64decode(b['jpeg'])==image.read_bytes())
            report.case('service.import-'+str(view)+'.independent-origin',b['origin']==[0,96] and b['scale']==.125*(view+1))
        patch={'origin':[7,-11],'scale':.125,'visible':False,'alpha':96,'transparent':True,'transparent_color':0xffffff,'tolerance':7}
        current=take('properties','background',operation='update',view=1,changes=patch)
        report.case('service.appearance-values',all(backgrounds(current['model'])[1][k]==v for k,v in patch.items()))
        take('undo','undo');before=dict(service.state)
        take('no-op','background',operation='update',view=1,changes={})
        report.case('service.no-op-keeps-redo',service.state==before)
        redone=take('redo','redo');report.case('service.redo-exact-backgrounds',backgrounds(redone['model'])==backgrounds(current['model']))
        bad=folder/'broken.jpg';bad.write_bytes(image.read_bytes()[:50])
        invalid=[('zero-scale',{'operation':'update','view':1,'changes':{'scale':0}}),
            ('negative-scale',{'operation':'update','view':1,'changes':{'scale':-1}}),
            ('fractional-origin',{'operation':'update','view':1,'changes':{'origin':[1.5,2]}}),
            ('oversize-origin',{'operation':'update','view':1,'changes':{'origin':[1e10,2]}}),
            ('alpha',{'operation':'update','view':1,'changes':{'alpha':256}}),
            ('color',{'operation':'update','view':1,'changes':{'transparent_color':-1}}),
            ('unknown-property',{'operation':'update','view':1,'changes':{'quality':30}}),
            ('perspective',{'operation':'import','view':3,'path':str(image),'scale':1}),
            ('failed-replacement',{'operation':'import','view':1,'path':str(bad),'scale':1}),
            ('stale-revision',{'operation':'remove','view':1,'expected_revision':1})]
        for name,fields in invalid:
            before=dict(service.state);dest=folder/('reject-'+name)
            response=service.command('background',output=dest,**fields)
            report.case('service.'+name+'.rejected',response.get('ok') is False,error=response.get('error'))
            report.case('service.'+name+'.history-preserved',service.state==before)
        after=take('after-rejections');report.case('service.failed-edits-keep-exact-image',backgrounds(after['model'])==backgrounds(current['model']))
        blocked=folder/'blocked';blocked.mkdir();(blocked/'model.json').mkdir();before=dict(service.state)
        failed=service.command('background',output=blocked,operation='update',view=1,changes={'alpha':17})
        report.case('service.publish-failure-rejected',not failed['ok'])
        report.case('service.publish-failure-keeps-history',service.state==before)
        after=take('after-publish-failure');report.case('service.publish-failure-restores-image',backgrounds(after['model'])==backgrounds(current['model']))
        saved=editing.saved_copy(service,folder/'saved')
        inspect_saved(report,'service.saved',probe,saved,after['model'],folder/'old-inspection')
        reopened=take('reopened','load',path=str(saved),document_id=str(uuid.uuid4()))
        report.case('service.reopen-keeps-all-backgrounds',backgrounds(reopened['model'])==backgrounds(current['model']))
        # Units alter geometry too, so this observation has its own expectations.
        dest=folder/'imperial';dest.mkdir();imp=q.validate_snapshot(report,'service.imperial',service.command('update_project',output=dest,changes={'units':'imperial'}),dest)
        for a,b in zip(backgrounds(reopened['model']),backgrounds(imp['model'])):
            report.case('service.units-scale-'+str(a['view']),b['scale']==editing.f32(a['scale']/editing.FOOT))
            report.case('service.units-keeps-pixels-and-origin-'+str(a['view']),a['jpeg']==b['jpeg'] and a['origin']==b['origin'])
        dest=folder/'units-undo';dest.mkdir();undone=q.validate_snapshot(report,'service.units-undo',service.command('undo',output=dest),dest)
        report.case('service.units-undo-exact',backgrounds(undone['model'])==backgrounds(current['model']))
        seeded=fixture.run(probe,model,folder/'legacy-seed','seed')
        legacy=Path(seeded['artifacts']['legacy.fbm'])
        legacy_state=json.loads(Path(seeded['artifacts']['legacy-state.json']).read_text('utf8'))
        dest=folder/'legacy-load';dest.mkdir()
        legacy_loaded=q.validate_snapshot(report,'legacy.load',service.command('load',output=dest,path=str(legacy),document_id=str(uuid.uuid4())),dest)
        report.case('legacy.starts-without-background',not backgrounds(legacy_loaded['model']))
        dest=folder/'legacy-import';dest.mkdir()
        legacy_imported=q.validate_snapshot(report,'legacy.import',service.command('background',output=dest,operation='import',view=1,path=str(image),scale=.25),dest)
        report.case('legacy.import-keeps-hull',native.reopened_geometry(legacy_loaded['model'])==native.reopened_geometry(legacy_imported['model']))
        for key in ('scene','dxf'):
            report.case('legacy.import-keeps-'+key,legacy_loaded['paths'][key].read_bytes()==legacy_imported['paths'][key].read_bytes())
        upgraded=editing.saved_copy(service,folder/'legacy-save')
        inspect_saved(report,'legacy.saved',probe,upgraded,legacy_imported['model'],folder/'legacy-inspection')
        upgraded_state=json.loads((folder/'legacy-inspection/state.json').read_text('utf8'))
        report.case('legacy.file-version-promoted-to-retain-image',upgraded_state['file_version']>legacy_state['file_version'])
    return saved

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT)
    p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path);p.add_argument('--reuse-probe',type=Path)
    p.add_argument('--model',type=Path,action='append');p.add_argument('--verify-deployment',type=Path);p.add_argument('--workflow-reference',type=Path)
    a=p.parse_args();a.source=a.source.resolve();a.qt_source=a.qt_source.resolve();out=a.output.resolve();models=[m.resolve(strict=True) for m in (a.model or q.DEFAULT_MODELS)]
    if a.verify_deployment:
        if not a.workflow_reference:p.error('--workflow-reference is required for deployment verification')
        return q.verify_deployment(a,models,out,'--verify-backgrounds',PHASES,('scene','dxf','sections'))
    for root in (a.source,a.qt_source,q.PILOT):
        if out==root or root in out.parents:p.error('Use an isolated report directory')
    out.mkdir(parents=True,exist_ok=False);report=q.Report();report.data['suite']='qt-backgrounds'
    sources=('qt_backgrounds.py','qt/background_tests.cpp','qt_migration.py','qt_editing_fixture.py','QtEditingFixture.pas','run_tests.py')
    report.data['test_sources_sha256']={name:q.common.digest_file(q.ROOT/name) for name in sources}
    before=q.common.fingerprint(a.source)[0];qt_before=q.current_sources(a.qt_source)
    protected={str(x):q.common.digest_file(x) for x in [*models,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe'] if x.exists()}
    try:
        bridge=q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),report);runtime=q.prepare_runtime(a,out,bridge,report)
        probe=prepare_oracle(a,out,report)
        for i,model in enumerate(models,1):
            folder=out/f'model-{i}';folder.mkdir();private=folder/'input.fbm';shutil.copy2(model,private)
            entry={'model':str(model),'sha256':q.common.digest_file(model)};report.data['models'].append(entry)
            try:
                child=q.run_child(report,f'{i}.production',runtime/'FreeShipQt.exe',['--verify-backgrounds',private],folder/'production',timeout=180,platform='offscreen');entry['production']=child
                if any(c.get('name')=='background_workflow_unavailable' for c in child.get('cases',[])):
                    report.case(f'{i}.dependent-artifacts',status='not_run',reason='Action discovery did not permit workflow execution');continue
                if child.get('status')=='error':
                    report.case(f'{i}.dependent-artifacts',status='not_run',reason='Production workflow did not complete; see its error evidence');continue
                artifacts=child['artifacts'];original=json.loads(Path(artifacts['original_model']).read_text('utf8'))
                for phase in PHASES:
                    current=json.loads(Path(artifacts[phase+'_model']).read_text('utf8'))
                    report.case(f'{i}.{phase}.hull-unchanged',native.reopened_geometry(current)==native.reopened_geometry(original))
                    for kind in ('scene','dxf','sections'):
                        report.case(f'{i}.{phase}.{kind}-unchanged',Path(artifacts[phase+'_'+kind]).read_bytes()==Path(artifacts['original_'+kind]).read_bytes())
                final=json.loads(Path(artifacts['reopened_model']).read_text('utf8'))
                inspect_saved(report,f'{i}.saved',probe,Path(artifacts['saved_file']),final,folder/'old-inspection')
                entry['widgets']=q.run_child(report,f'{i}.widgets',runtime/'qt_background_tests.exe',[artifacts['original_model']],folder/'widgets',platform='offscreen')
                if i==1:
                    service_recipe(report,runtime,private,folder/'production/background.jpg',probe,folder/'service-cases')
                    entry['gpu_composition']=q.run_child(report,'gpu-composition',runtime/'qt_background_tests.exe',['--gpu-only'],folder/'gpu-composition')
                    entry['editing_regression']=q.run_child(report,'editing-regression',runtime/'qt_editing_tests.exe',[artifacts['original_model']],folder/'editing-regression',platform='offscreen')
                entry['lines_regression']=q.run_child(report,f'{i}.lines-regression',runtime/'qt_lines_tests.exe',[artifacts['original_scene']],folder/'lines-regression',platform='offscreen')
            except Exception as error:report.case(f'{i}.execution',status='error',reason=f'{type(error).__name__}: {error}')
    except Exception as error:report.case('suite.execution',status='error',reason=f'{type(error).__name__}: {error}')
    finally:
        report.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==before);report.case('qt-source-unchanged',q.current_sources(a.qt_source)==qt_before)
        report.case('test-source-unchanged',all(q.common.digest_file(q.ROOT/name)==digest for name,digest in report.data['test_sources_sha256'].items()))
        for path,digest in protected.items():report.case('protected.'+path,q.common.digest_file(Path(path))==digest)
        report.case('desktop-acceptance',status='not_run',reason='Qt offscreen menus/dialogs and events only, not actual desktop acceptance.')
        report.finish();(out/'report.json').write_text(json.dumps(report.data,ensure_ascii=False,indent=2,default=str),encoding='utf8')
    print(json.dumps({'status':report.data['status'],'counts':report.data['counts'],'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'partial':2,'fail':1,'error':3}[report.data['status']]
if __name__=='__main__':raise SystemExit(main())
