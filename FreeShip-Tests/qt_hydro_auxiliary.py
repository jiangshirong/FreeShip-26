"""Native auxiliary hydrostatic reports, independent box integrals and actual UI.

Legacy SAC/Bonjean/stem text is an upright zero-keel comparison only. Corrected
whole-face weights and elevated-keel semantics have independent expectations.
"""
import argparse,json,math,os,re,shutil,subprocess,time
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_native_project as fixture
import qt_native_hydrostatics as hydro_base
import qt_hydrostatics as hydro_ui
import fbm_preview_bytes as fbm

FIELD='generate_auxiliary_reports'
FILES=('qt_hydro_auxiliary.py','fbm_preview_bytes.py','qt_native_project.py','qt_native_hydrostatics.py','qt_hydrostatics.py','qt_hydro_cases.py','qt_migration.py','qt_editing.py','run_tests.py')
def read(path):return json.loads(Path(path).read_text('utf8'))
def patched(raw,value):
    data=bytearray(raw);data[fbm.envelope(data)['auxiliary_reports_offset']]=value;return bytes(data)
def near(a,b):return math.isclose(a,b,rel_tol=3e-5,abs_tol=1e-5)

def formats(r,runtime,out):
    out.mkdir();env=dict(os.environ);env['PATH']=str(runtime)+os.pathsep+env.get('PATH','')
    for version in range(45):
        source=out/f'v{version}.fbm';fixture.fixture(source,version=version,rich=version>=20);raw=source.read_bytes();value=fbm.envelope(raw)[FIELD]
        for name,next_value in [('noop',value),('toggle',not value)]:
            req=out/f'v{version}-{name}.json';target=req.with_suffix('.fbm');promote=version<39 and next_value!=value
            req.write_text(json.dumps(dict(bindings=dict(fbm_encoding='cp1251',default_underwater_color=0x800000),request=dict(method='update_project',changes={FIELD:next_value}),minimum_only=promote)),encoding='utf8')
            child=subprocess.run([str(runtime/'native_project_edit_probe.exe'),str(source),str(req),str(target)],capture_output=True,env=env,timeout=15,creationflags=q.common.FLAGS)
            req.with_suffix('.log').write_bytes(child.stdout+child.stderr)
            if child.returncode:raise RuntimeError('Project probe crashed')
            result=json.loads(child.stdout);r.case(f'v{version}.{name}.accepted',result['ok'],observed=result)
            if result['ok']:
                r.case(f'v{version}.{name}.minimum',result['minimum_version']==(39 if promote else version))
                if not promote:r.case(f'v{version}.{name}.exact-bytes',target.read_bytes()==(raw if name=='noop' else patched(raw,next_value)))

def prepare(runtime,out):
    out.mkdir();seed=out/'seed.fbm';fixture.fixture(seed,rich=False)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        def call(name,method,**fields):return q.require_ok(s.command(method,output=out/name,**fields))
        original=call('load','load',path=str(seed),document_id=str(q.uuid.uuid4()));model=read(original['model'])
        for layer in model['layers']:call('material-'+str(layer['id']),'update_layer',layer_id=layer['id'],changes=dict(thickness=10,material_density=7.85))
        for kind,positions in ((1,[2.5,5,7.5]),(3,[1,2,2.5])):
            call('clear-'+str(kind),'sections',kind=kind,operation='clear');call('add-'+str(kind),'sections',kind=kind,operation='add',positions=positions)
        call('settings','update_project',changes={FIELD:True,'precision':0,'draft':2})
        saved=call('save','save');model=out/'box.fbm';shutil.copy2(saved['saved_file'],model);return model

def backend(r,runtime,model,out):
    out.mkdir()
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        def call(name,method,**fields):return q.require_ok(s.command(method,output=out/name,**fields))
        def saved(name):return Path(call(name,'save')['saved_file']).read_bytes()
        def calc(name,**fields):return read(call(name,'hydrostatics',mode='single',draft=fields.pop('draft',2),heel=fields.pop('heel',0),trim=fields.pop('trim',0),**fields)['hydro'])
        initial=call('load','load',path=str(model),document_id=str(q.uuid.uuid4()));raw=model.read_bytes();state=dict(s.state)
        result=calc('upright');aux=result['auxiliary'];r.case('auxiliary.complete',aux['status']=='ok',observed=aux.get('message'))
        r.case('auxiliary.four-files',{f['name'] for f in aux['files']}=={'SACs.txt','Bonjean.txt','Weights.txt','Sterns.txt'})
        for row,z in zip(aux['waterlines'],(1,2,2.5)):
            r.case(f'z{z}.areas',all(near(a,4*z) for a in row['bonjean_areas']+row['sac_areas']))
            r.case(f'z{z}.stems',near(row['aft_x'],0) and near(row['fore_x'],10))
        r.case('station-cuts-reused',aux['station_cuts']==len(aux['stations'])==3)
        r.case('weights.surface-area',near(sum(l['surface_area'] for l in aux['layers']),164))
        total=sum(l['weight'] for l in aux['layers']);r.case('weights.independent-integral',near(total,12.874),observed=total)
        r.case('weights.matches-main',near(total,result['rows'][0]['weight']))
        for layer in aux['layers']:
            r.case('weights.complete-child-sums-'+str(layer['index']),near(sum(f['weight'] for f in layer['faces']),layer['weight']) and near(sum(f['area'] for f in layer['faces']),layer['surface_area']))
        tilted=calc('tilted',heel=5,trim=.1);r.case('auxiliary.horizontal-reference',tilted['auxiliary']==aux)
        r.case('calculation.read-only',s.state==state and saved('unchanged')==raw)
        after=call('after','snapshot')
        for kind in ('scene','dxf','surface','sections'):r.compare('calculation.unchanged-'+kind,initial[kind],after[kind])
        off=call('off','update_project',changes={FIELD:False});r.case('disabled.exact-bytes',saved('off-save')==patched(raw,False))
        r.case('disabled.no-extra-work','auxiliary' not in calc('off-calc'))
        call('undo','undo');r.case('toggle.undo-exact',saved('undo-save')==raw)
        call('redo','redo');r.case('toggle.redo-exact',saved('redo-save')==patched(raw,False))
        for i,value in enumerate((1,'true',None)):
            before=dict(s.state);reply=s.command('update_project',output=out/f'invalid-{i}',changes={FIELD:value,'name':'must not apply'});r.case(f'invalid-{i}.atomic',not reply['ok'] and s.state==before)
        reply=s.command('update_project',output=out/'stale',expected_revision=1,changes={FIELD:True});r.case('stale.rejected',not reply['ok'])
        call('reload','load',path=str(model),document_id=str(q.uuid.uuid4()))
        # Physically translating the ship and waterlines must not double keel Z.
        points=read(call('points','snapshot')['model'])['control_points']
        call('raise','move_points',moves=[dict(point_id=p['id'],position=[p['p'][0],p['p'][1],p['p'][2]+5]) for p in points])
        call('clear-raised','sections',kind=3,operation='clear');call('add-raised','sections',kind=3,operation='add',positions=[6,7,7.5])
        raised=calc('raised')['auxiliary'];r.case('raised.valid',raised['status']=='ok',observed=raised.get('message'))
        if raised['status']=='ok':
            # Translation changes Single rounding during clipping. Use the same
            # independent box integral/tolerance as above, not bitwise equality
            # between two different coordinate frames.
            for a,b in zip(raised['waterlines'],aux['waterlines']):r.case('raised.independent-area-and-stems',all(near(v,4*b['draft']) for v in a['sac_areas']+a['bonjean_areas']) and a['aft_x']==0 and a['fore_x']==10 and near(a['draft'],b['draft']))
        call('reload-units','load',path=str(model),document_id=str(q.uuid.uuid4()));call('imperial','update_project',changes={'units':'imperial'})
        imperial=calc('imperial-calc',draft=2/.3048)['auxiliary'];r.case('imperial.complete',imperial['status']=='ok')
        if imperial['status']=='ok':
            r.case('imperial.weight',near(sum(l['weight'] for l in imperial['layers']),12.874*1000/(2240*.45359237)))
            r.case('imperial.area',near(imperial['waterlines'][0]['sac_areas'][0],4/.3048**2))
        call('reload-limits','load',path=str(model),document_id=str(q.uuid.uuid4()));call('no-stations','sections',kind=1,operation='clear')
        no_stations=calc('no-stations-calc')['auxiliary'];r.case('no-stations.partial',no_stations['status']=='partial' and {f['name'] for f in no_stations['files']}=={'Weights.txt','Sterns.txt'})
        call('no-waterlines','sections',kind=3,operation='clear');missing=calc('no-waterlines-calc');r.case('missing.no-partial-files',missing['auxiliary']['status']=='error' and not missing['auxiliary']['files'] and missing['rows'][0]['status']=='ok')
        call('many-waterlines','sections',kind=3,operation='add',positions=[.01*(i+1) for i in range(100)])
        r.case('bounded-waterlines',calc('bounded')['auxiliary']['status']=='error')
        call('reload-bad','load',path=str(model),document_id=str(q.uuid.uuid4()));bad=calc('invalid-hull',draft=0)
        r.case('incomplete.no-files',bad['rows'][0]['status']=='error' and bad['auxiliary']['status']=='error' and not bad['auxiliary']['files'])
        call('dry-waterlines','sections',kind=3,operation='clear');call('dry-add','sections',kind=3,operation='add',positions=[-1,0,1])
        dry=calc('dry')['auxiliary'];r.case('dry.no-fabricated-stems',dry['status']=='ok' and all(row['dry'] and row['aft_x'] is None and row['fore_x'] is None and not any(row['sac_areas']) for row in dry['waterlines'][:2]))
        layers=read(call('asymmetric-snapshot','snapshot')['model'])['layers'];call('asymmetric','update_layer',layer_id=layers[0]['id'],changes={'symmetric':False})
        asym=calc('asymmetric-calc')['auxiliary'];r.case('asymmetric.no-invented-results',asym['status']=='error' and not asym['files'])
        call('old-load','load',path=str(out.parent/'formats/v38.fbm'),document_id=str(q.uuid.uuid4()));old=saved('old-original')
        call('old-enable','update_project',changes={FIELD:True});upgraded=saved('old-enabled')
        r.case('old.format-promotion',fbm.envelope(upgraded)['version']==39 and fbm.envelope(upgraded)[FIELD]);call('old-undo','undo');r.case('old.undo-exact',saved('old-restored')==old)
    # Historical formatted tables are read-only observations, not the oracle
    # for the corrected weight records or a translated hull's waterline datum.
    with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'legacy') as legacy:
        q.require_ok(legacy.command('load',path=str(model),document_id=str(q.uuid.uuid4()),output=out/'legacy-load'))
        old=read(q.require_ok(legacy.command('hydrostatics',mode='single',draft=2,heel=0,trim=0,legacy_auxiliary_probe=True,output=out/'legacy-calc'))['hydro'])['legacy_auxiliary']['files']
        current={f['name']:f['text'] for f in aux['files']}
        for name in ('SACs.txt','Bonjean.txt','Sterns.txt'):
            r.case('legacy.'+name,current[name].split()==old[name].split(),actual=current[name],expected=old[name])

def production(r,runtime,model,out):
    out.mkdir();private=out/'input.fbm';shutil.copy2(model,private);raw=private.read_bytes()
    commands=[dict(kind='startup',path=str(private)),dict(kind='project_action',changes={FIELD:False},cancel=True),dict(kind='project_action',changes={FIELD:False}),dict(kind='undo'),dict(kind='redo'),dict(kind='save_action')]
    req=out/'settings.json';req.write_text(json.dumps(dict(history_file=str(out/'history.json'),recovery_root=str(out/'recovery'),commands=commands)),encoding='utf8')
    child=q.run_child(r,'project-actions',runtime/'FreeShipQt.exe',['--verify-startup',req],out/'project',platform='windows');r.data['project_production']=child
    observations=read(child['artifacts']['observations']);models=[read(row['model']) for row in observations]
    r.case('project.cancel-exact',models[0]==models[1]);r.case('project.undo-redo',models[3]['settings'][FIELD] and not models[2]['settings'][FIELD] and not models[4]['settings'][FIELD])
    r.case('project.saved-flag',not fbm.envelope(private.read_bytes())[FIELD]);r.case('project.backup',private.with_suffix('.Bak').read_bytes()==raw)
    for index in range(1,len(observations)):
        for kind in ('scene','dxf'):r.compare(f'project.{index}.unchanged-'+kind,observations[0][kind],observations[index][kind])
    req=hydro_ui.production_request(model,out/'hydro-request','single',export_auxiliary=True,expected_auxiliary_count=4,repeat_cancel=True)
    child=q.run_child(r,'hydro-actions',runtime/'FreeShipQt.exe',['--verify-hydro',req],out/'hydro',platform='windows',timeout=240);r.data['production']=child
    for kind in ('scene','dxf'):r.compare('hydro.readonly-'+kind,child['artifacts']['original_'+kind],child['artifacts']['after_'+kind])
    report=read(child['artifacts']['report_json']);r.case('hydro.ui-complete',report['auxiliary']['status']=='ok')
    for p in child['artifacts']['auxiliary_files']:r.case('hydro.retained-'+Path(p).name,Path(p).stat().st_size>0)
    r.case('input-unchanged',model.read_bytes()==raw)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--focused',action='store_true');a=p.parse_args()
    runtime=(a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.monotonic();sources=q.current_sources(q.QT);originals={str(p):q.common.digest_file(p) for p in q.DEFAULT_MODELS}
    r.data.update(suite='qt-hydro-auxiliary',runtime=str(runtime),qt_sources_sha256=sources,test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES})
    try:
        manifest=read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if {n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime mismatch')
        r.case('runtime-manifest',True);formats(r,runtime,out/'formats');model=prepare(runtime,out/'fixture');backend(r,runtime,model,out/'service');production(r,runtime,model,out/'production')
        if a.focused:r.case('existing-hydro-regression',status='not_run',reason='Focused diagnostic; final unified run includes existing native hydrostatic cases.')
        else:hydro_base.run(r,runtime,q.DEFAULT_MODELS,out/'existing-hydro-regression')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('models-protected',all(q.common.digest_file(Path(p))==h for p,h in originals.items()))
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.case('desktop-acceptance',status='not_run',reason='Hidden production events only; no desktop, native file dialog or multi-monitor acceptance.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
