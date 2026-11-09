"""Half/both hull display and curvature: real actions, independent old-core samples, editing and export invariance."""
from pathlib import Path
import argparse, json, shutil, struct, uuid
import qt_migration as q
import qt_editing as editing
import qt_editing_fixture as fixture
import qt_backgrounds as backgrounds
import native_linesplan_app as native

PHASES=('original','toggled','undo','redo','curvature_toggled','scaled','section','reopened')

def service_cases(r,runtime,model,probe,out):
    out.mkdir()
    with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'service',native_linesplan=True) as service:
        def take(name,method='snapshot',**fields):
            folder=out/name;folder.mkdir();value=q.validate_snapshot(r,'service.'+name,service.command(method,output=folder,**fields),folder);value['paths']['sections']=folder/'sections.json'
            if name!='original':
                r.case('service.'+name+'.exact-hull',q.geometry(value['model'])==q.geometry(original['model']))
                for kind in ('scene','dxf','sections'):r.case('service.'+name+'.exact-'+kind,value['paths'][kind].read_bytes()==original['paths'][kind].read_bytes())
            return value
        original=take('original','load',path=str(model),document_id=str(uuid.uuid4()))
        current=take('half','display',changes={'both_sides':False,'show_curvature':True,'curvature_scale':2})
        r.case('service.requested-properties',current['model']['display']=={**original['model']['display'],'both_sides':False,'show_curvature':True,'curvature_scale':2})
        full=take('full','display',changes={'both_sides':True});take('undo','undo');before=dict(service.state)
        take('no-op','display',changes={'both_sides':False});r.case('service.no-op-preserves-redo',service.state==before)
        redone=take('redo','redo');r.case('service.redo-properties',redone['model']['display']==full['model']['display'])
        before=dict(service.state)
        for name,fields in [('negative',{'changes':{'curvature_scale':-1}}),('overflow',{'changes':{'curvature_scale':1e12}}),('wrong-type',{'changes':{'both_sides':1}}),('unknown',{'changes':{'invented':True}}),('stale',{'changes':{'both_sides':False},'expected_revision':1})]:
            folder=out/name;folder.mkdir();reply=service.command('display',output=folder,**fields)
            r.case('service.reject-'+name,reply.get('ok') is False);r.case('service.preserve-after-'+name,service.state==before)
        blocked=out/'blocked';blocked.mkdir();(blocked/'model.json').mkdir()
        reply=service.command('display',output=blocked,changes={'both_sides':False})
        r.case('service.failed-publication-rejected',not reply['ok']);r.case('service.failed-publication-keeps-history',service.state==before)
        restored=take('after-rejection');r.case('service.failed-publication-restores-options',restored['model']['display']==full['model']['display'])
        saved=editing.saved_copy(service,out/'saved');observed=fixture.run(probe,saved,out/'old-core')
        for key in ('both_sides','show_curvature','curvature_scale'):r.case('service.original-core-saved-'+key,observed['display'][key]==full['model']['display'][key])

def curvature_requests(section_file):
    requests=[]
    def add(name,points,knuckles=None,precision=0,scale=1):requests.append({'name':name,'points':points,'knuckles':knuckles or [False]*len(points),'precision':precision,'scale':scale})
    add('straight',[[0,0,0],[0,1,0],[0,2,0]])
    add('curved',[[0,0,0],[0,1,1],[0,2,0]])
    add('space',[[0,0,0],[1,2,3],[2,-1,4],[3,2,1]])
    add('knuckles',[[0,0,0],[0,1,1],[0,2,0],[0,3,2]],[True,True,True,True])
    add('repeated',[[0,0,0],[0,0,0],[0,1,1],[0,2,1]])
    add('zero',[[1,1,1],[1,1,1],[1,1,1]])
    for precision in (1,2,3):add('precision-'+str(precision),[[0,0,0],[0,1,1],[0,2,0]],precision=precision,scale=1.1)
    data=json.loads(section_file.read_text('utf8'))
    for section in data['sections']:
        if section.get('splines'):
            s=section['splines'][0];add('model-'+str(section['kind'])+'-'+str(section['index']),s['points'],s['knuckles'])
            if len(requests)>=17:break
    return requests

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT)
    p.add_argument('--reuse-build',type=Path);p.add_argument('--reuse-probe',type=Path);p.add_argument('--runtime',type=Path)
    p.add_argument('--model',type=Path,action='append');p.add_argument('--verify-deployment',type=Path);p.add_argument('--workflow-reference',type=Path)
    a=p.parse_args();a.source=a.source.resolve();a.qt_source=a.qt_source.resolve();out=a.output.resolve();models=[x.resolve() for x in (a.model or q.DEFAULT_MODELS)]
    if a.verify_deployment:
        if not a.workflow_reference:p.error('--workflow-reference is required for deployment verification')
        return q.verify_deployment(a,models,out,'--verify-display',PHASES,('scene','dxf','sections'))
    for root in (a.source,a.qt_source,q.PILOT):
        if root==out or root in out.parents:p.error('Use a private report directory')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data['suite']='qt-display'
    legacy=q.common.fingerprint(a.source)[0];sources=q.current_sources(a.qt_source)
    protected={str(x):q.common.digest_file(x) for x in [*models,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe']}
    tests=('qt_display.py','qt/display_tests.cpp','QtEditingFixture.pas','qt_editing_fixture.py','qt_migration.py','qt_backgrounds.py','run_tests.py')
    r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in tests}
    try:
        bridge=q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),r);runtime=q.prepare_runtime(a,out,bridge,r);probe=backgrounds.prepare_oracle(a,out,r)
        for i,model in enumerate(models,1):
            folder=out/f'model-{i}';folder.mkdir();private=folder/'input.fbm';shutil.copy2(model,private)
            child=q.run_child(r,f'{i}.production',runtime/'FreeShipQt.exe',['--verify-display',private],folder/'production',platform='offscreen')
            r.data['models'].append({'model':str(model),'sha256':q.common.digest_file(model),'production':child})
            if any(c.get('name')=='display_workflow_unavailable' for c in child.get('cases',[])) or child['status']=='error':
                r.case(f'{i}.dependent-artifacts',status='not_run',reason='Production workflow unavailable; see discovery/error');continue
            artifacts=child['artifacts'];original=json.loads(Path(artifacts['original_model']).read_text('utf8'))
            for phase in PHASES:
                model_state=json.loads(Path(artifacts[phase+'_model']).read_text('utf8'))
                r.case(f'{i}.{phase}.exact-hull',native.reopened_geometry(model_state)==native.reopened_geometry(original))
                for kind in ('scene','dxf','sections'):r.case(f'{i}.{phase}.exact-'+kind,Path(artifacts[phase+'_'+kind]).read_bytes()==Path(artifacts['original_'+kind]).read_bytes())
            observed=fixture.run(probe,Path(artifacts['saved_file']),folder/'saved-inspection')
            for key in ('both_sides','show_curvature','curvature_scale'):r.case(f'{i}.original-core-'+key,observed['display'][key]==model_state['display'][key])
            if i==1:
                request=curvature_requests(Path(artifacts['original_sections']));request_file=folder/'curvature-request.json';request_file.write_text(json.dumps(request),encoding='utf8')
                q.run_child(r,'widgets-and-curvature',runtime/'qt_display_tests.exe',[request_file],folder/'widgets',platform='offscreen')
                actual=json.loads((folder/'widgets/curvature.json').read_text('utf8'));expected=fixture.run(probe,private,folder/'curvature-oracle','curvature',request)['curves']
                total=0;max_difference=0
                for candidate,reference in zip(actual,expected):
                    name=candidate['name'];same=len(candidate['samples'])==len(reference['samples']);r.case('curvature.'+name+'.sample-count',same)
                    if not same:continue
                    for key in ('base','tip'):
                        pairs=[(x,y) for a,b in zip(candidate['samples'],reference['samples']) for x,y in zip(a[key],b[key])];total+=len(pairs)
                        difference=max(abs(x-y) for x,y in pairs);max_difference=max(max_difference,difference)
                        r.case('curvature.'+name+'.exact-single-'+key,all(struct.pack('<f',x)==struct.pack('<f',y) for x,y in pairs),max_difference=difference)
                    r.case('curvature.'+name+'.rib-placement',[s['rib'] for s in candidate['samples']]==[s['rib'] for s in reference['samples']])
                    if name in ('straight','zero'):r.case('curvature.'+name+'.no-normal-spike',all(s['base']==s['tip'] for s in candidate['samples']))
                r.data['curvature_values_compared']=total;r.data['curvature_max_difference']=max_difference
                service_cases(r,runtime,private,probe,folder/'service-cases')
                q.run_child(r,'editing-regression',runtime/'qt_editing_tests.exe',[artifacts['original_model']],folder/'editing-regression',platform='offscreen')
            q.run_child(r,f'{i}.lines-regression',runtime/'qt_lines_tests.exe',[artifacts['original_scene']],folder/'lines-regression',platform='offscreen')
    except Exception as e:r.case('suite.execution',status='error',reason=f'{type(e).__name__}: {e}')
    finally:
        r.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==legacy);r.case('qt-source-unchanged',q.current_sources(a.qt_source)==sources)
        r.case('test-source-unchanged',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        for path,h in protected.items():r.case('protected.'+path,q.common.digest_file(Path(path))==h)
        r.case('desktop-acceptance',status='not_run',reason='Offscreen widgets/events only');r.finish();(out/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2,default=str),encoding='utf8')
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'report':str(out/'report.json')},ensure_ascii=False));return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]
if __name__=='__main__':raise SystemExit(main())
