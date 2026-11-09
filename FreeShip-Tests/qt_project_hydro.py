"""Project hydrostatic options: independent FBM bytes, box formulae and actual UI.

Preserves user models/settings. Project flags follow FreeVisibility.SaveBinary;
coefficient expectations use a 20 x 6 rectangular hull at draft 2.
"""
import argparse,json,math,os,shutil,struct,subprocess,time
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_native_project as fixtures
import fbm_preview_bytes as fbm

FILES=('qt_project_hydro.py','fbm_preview_bytes.py','qt_native_project.py','qt_migration.py','qt_editing.py','qt_modeling.py','qt_native_transforms.py','native_linesplan_app.py','qt/project_edit_probe.cpp','qt/project_units_math.pas','run_tests.py')
FIELDS=fbm.HYDRO_FIELDS

def read(path):return json.loads(Path(path).read_text('utf8'))

def patch_bytes(raw,changes):
    info=fbm.envelope(raw);data=bytearray(raw)
    if 'coefficient_basis' in changes:struct.pack_into('<i',data,info['coefficient_offset'],changes['coefficient_basis'])
    for key in FIELDS:
        if key in changes:data[info['hydro_offsets'][key]]=changes[key]
    return bytes(data)

def formats(r,runtime,out):
    out.mkdir();exe=runtime/'native_project_edit_probe.exe'
    env=dict(os.environ);env['PATH']=str(runtime)+os.pathsep+env.get('PATH','')
    for version in range(45):
        source=out/f'v{version}.fbm';fixtures.fixture(source,version=version,rich=version>=20)
        initial=fbm.envelope(source.read_bytes())
        for operation in ('noop','set'):
            changes=dict(coefficient_basis=initial['coefficient_basis'],**initial['hydro_fields']) if operation=='noop' else dict(coefficient_basis=0,**dict.fromkeys(FIELDS,False))
            minimum_only=operation=='set' and version<17
            request=out/f'v{version}-{operation}.json';target=request.with_suffix('.fbm')
            request.write_text(json.dumps(dict(request=dict(method='update_project',changes=changes),bindings=dict(fbm_encoding='cp1251',default_underwater_color=0x800000),minimum_only=minimum_only)),encoding='utf8')
            process=subprocess.run([str(exe),str(source),str(request),str(target)],env=env,capture_output=True,timeout=20,creationflags=q.common.FLAGS)
            request.with_suffix('.log').write_bytes(process.stdout+process.stderr)
            if process.returncode:raise RuntimeError('Project probe crashed')
            result=json.loads(process.stdout);r.case(f'v{version}.{operation}.accepted',result['ok'],detail=result)
            if not result['ok']:continue
            if minimum_only:r.case(f'v{version}.promotion-required',result['minimum_version']==17)
            else:
                expected=source.read_bytes() if operation=='noop' else patch_bytes(source.read_bytes(),changes)
                r.case(f'v{version}.{operation}.exact-bytes',target.read_bytes()==expected)
                r.case(f'v{version}.{operation}.minimum-version',result['minimum_version']==version)

def backend(r,runtime,model,out):
    out.mkdir();private=out/'input.fbm';raw=bytearray(model.read_bytes());info=fbm.envelope(raw)
    raw[info['hydro_offsets'][FIELDS[0]]-1]=1 # Master visibility, private fixture only.
    private.write_bytes(patch_bytes(raw,dict.fromkeys(FIELDS,True)))
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        def call(name,method,**fields):return q.require_ok(s.command(method,output=out/name,**fields))
        def save(name):return Path(call(name,'save')['saved_file'])
        def row(name):
            response=call(name,'hydrostatics',mode='single',draft=2,heel=0,trim=0)
            return read(response['hydro'])['rows'][0]
        call('load','load',path=str(private),document_id=str(q.uuid.uuid4()))
        # The stock rectangular fixture has no station definitions. SAC needs
        # actual stations, so declare three known interior sections explicitly.
        call('stations','sections',kind=1,operation='add',positions=[5,10,15])
        baseline=call('dimensions','update_project',changes=dict(length=40,beam=12,draft=2,use_default_split_section_location=False,split_section_location=10,coefficient_basis=1))
        before=save('before').read_bytes();actual=row('actual-coefficients');m=read(baseline['model'])
        for key,expected in dict(volume=240,midship_area=12,cb=1,cm=1,cp=1).items():
            r.case('box.actual.'+key,math.isclose(actual[key],expected,abs_tol=3e-5,rel_tol=3e-5),observed=actual[key],expected=expected)
        old=dict(s.state)
        noop=call('noop','update_project',changes=dict(coefficient_basis=1,**dict.fromkeys(FIELDS,True)))
        r.case('noop.keeps-revision',s.state==old and save('noop-save').read_bytes()==before)
        all_off=dict(coefficient_basis=0,**dict.fromkeys(FIELDS,False))
        changed=call('off','update_project',changes=all_off);after=save('after').read_bytes();project=row('project-coefficients')
        r.case('mixed-edit.one-revision',s.state['revision']==old['revision']+1)
        r.case('mixed-edit.exact-persisted-bytes',after==patch_bytes(before,all_off))
        r.case('calculation-dialog.updated-default',call('defaults','hydro_defaults')['analysis']['coefficient_basis']==0)
        for key,expected in dict(cb=.25,cm=.5,cp=.5).items():
            r.case('box.project.'+key,math.isclose(project[key],expected,abs_tol=3e-5,rel_tol=3e-5),observed=project[key],expected=expected)
        # Current Plus uses waterline dimensions for Cwp in project mode too;
        # this migration exposes its switch without changing that convention.
        r.case('box.base-measurements-identical',all(project[k]==v for k,v in actual.items() if k not in ('cb','cm','cp','cvp')))
        hydro=read(changed['model'])['overlay']['hydro']
        r.case('features.all-off',hydro['available'] and not any(hydro[k] for k in ('markers','areas','path')))
        for kind in ('scene','dxf','surface','sections'):r.compare('options.geometry-'+kind,baseline[kind],changed[kind])
        call('undo','undo');r.case('mixed-edit.one-undo-exact',save('undo-save').read_bytes()==before)
        call('redo','redo');r.case('mixed-edit.redo-exact',save('redo-save').read_bytes()==after)
        for name,value in [('basis-high',2),('basis-negative',-1),('basis-fraction',.5),('basis-boolean',True),('basis-string','0')]:
            state=dict(s.state);result=s.command('update_project',changes={'coefficient_basis':value},output=out/name)
            r.case('invalid.'+name,not result['ok'] and s.state==state)
        for field in FIELDS:
            result=s.command('update_project',changes={field:1},output=out/('invalid-'+field))
            r.case('invalid.'+field,not result['ok'])
        result=s.command('update_project',changes={'coefficient_basis':1,'hydro_lcf':'false'},output=out/'invalid-mixed')
        r.case('invalid.mixed-patch-atomic',not result['ok'] and save('invalid-mixed-save').read_bytes()==after)
        keys=[{'displacement','weight'},{'lateral','wind'},set(),{'km'},{'lcf'}]
        for field,wanted in zip(FIELDS,keys):
            response=call(field,'update_project',changes=dict(dict.fromkeys(FIELDS,False),**{field:True}))
            h=read(response['model'])['overlay']['hydro'];seen={m['key'] for m in h['markers']}
            r.case(field+'.only-selected-markers',seen<=wanted and (bool(seen) if wanted else not seen))
            r.case(field+'.section-curve',bool(h['areas'])==bool(h['path'])==(field=='hydro_sectional_areas'))
        call('restore-off','update_project',changes=all_off)
        call('reopen','load',path=str(out/'after/saved.fbm'),document_id=str(q.uuid.uuid4()))
        r.case('reopen.persisted-options',all(read(call('snapshot','snapshot')['model'])['settings'][k]==v for k,v in all_off.items()))
        with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'legacy',native_linesplan=False) as legacy:
            response=q.require_ok(legacy.command('load',path=str(out/'after/saved.fbm'),document_id=str(q.uuid.uuid4()),output=out/'legacy-load'))
            legacy_hydro=read(response['model'])['overlay']['hydro']
            r.case('legacy.reads-hidden-features',legacy_hydro['available'] and not any(legacy_hydro[k] for k in ('markers','areas','path')))
            result=q.require_ok(legacy.command('hydrostatics',mode='single',draft=2,heel=0,trim=0,output=out/'legacy-hydro'))
            legacy_row=read(result['hydro'])['rows'][0]
            r.case('legacy.coefficients-exact',all(legacy_row[k]==project[k] for k in ('cb','cm','cp','cvp','cwp','volume')))
    # Promotion plus one undo must preserve even historical raw representations.
    for version in (0,15,16):
        old=out/f'old-{version}.fbm';fixtures.fixture(old,version=version,rich=False)
        with q.Service(runtime/'FreeShipDocumentService.exe',out/f'old-{version}-process',native_linesplan=True) as s:
            base=q.require_ok(s.command('load',path=str(old),document_id=str(q.uuid.uuid4()),output=out/f'old-{version}-load'))
            q.require_ok(s.command('update_project',changes=all_off,output=out/f'old-{version}-edit'))
            saved=q.require_ok(s.command('save',output=out/f'old-{version}-save'));values=fbm.envelope(Path(saved['saved_file']).read_bytes())
            r.case(f'old-{version}.promotion',values['version']>=17 and values['coefficient_basis']==0 and not any(values['hydro_fields'].values()))
            q.require_ok(s.command('undo',output=out/f'old-{version}-undo'))
            saved=q.require_ok(s.command('save',output=out/f'old-{version}-undo-save'))
            r.case(f'old-{version}.undo-exact',Path(saved['saved_file']).read_bytes()==old.read_bytes())

def production(r,runtime,model,out):
    out.mkdir();private=out/'input.fbm';shutil.copy2(model,private);before=private.read_bytes();settings=fbm.envelope(before)
    edits=dict(coefficient_basis=1-settings['coefficient_basis'],**{k:not v for k,v in settings['hydro_fields'].items()})
    commands=[dict(kind='startup',path=str(private)),dict(kind='project_action',changes=edits,cancel=True),
        dict(kind='project_action',changes=edits),dict(kind='undo'),dict(kind='redo'),dict(kind='save_action')]
    request=out/'request.json';request.write_text(json.dumps(dict(history_file=str(out/'history.json'),recovery_root=str(out/'recovery'),commands=commands)),encoding='utf8')
    child=q.run_child(r,'project-actions',runtime/'FreeShipQt.exe',['--verify-startup',request],out/'qt',platform='windows');r.data['production']=child
    rows=read(child['artifacts']['observations']);r.case('production.complete',len(rows)==len(commands) and all(not row['error'] for row in rows))
    models=[read(row['model']) for row in rows]
    r.case('production.cancel-exact',models[0]==models[1])
    for i in (2,4,5):r.case(f'production.{i}.submitted-values',all(models[i]['settings'][k]==v for k,v in edits.items()))
    for key in edits:r.case('production.undo.'+key,models[3]['settings'][key]==models[0]['settings'][key])
    # GUI save may refresh the thumbnail. Compare all other bytes independently.
    r.case('production.only-approved-byte-changes',fbm.replace(private.read_bytes(),enabled=False)==fbm.replace(patch_bytes(before,edits),enabled=False))
    r.case('production.backup-exact',private.with_suffix('.Bak').read_bytes()==before)
    for i in range(1,len(rows)):
        for kind in ('scene','dxf'):r.compare(f'production.{i}.'+kind,rows[0][kind],rows[i][kind])

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    runtime=(a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.monotonic();sources=q.current_sources(q.QT);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    originals={str(p):q.common.digest_file(p) for p in q.DEFAULT_MODELS}
    r.data.update(suite='qt-project-hydro',runtime=str(runtime),qt_sources_sha256=sources,source_sha256=legacy,test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES})
    try:
        manifest=read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if {n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime mismatch')
        r.case('runtime-manifest',True);formats(r,runtime,out/'formats');backend(r,runtime,q.DEFAULT_MODELS[0],out/'service');production(r,runtime,q.DEFAULT_MODELS[0],out/'production')
        fixtures.run(r,runtime,q.DEFAULT_MODELS[0],out/'existing-project-regression')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==legacy)
        r.case('models-protected',all(q.common.digest_file(Path(p))==h for p,h in originals.items()))
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.case('desktop-acceptance',status='not_run',reason='Hidden production actions only; no desktop automation.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))

if __name__=='__main__':raise SystemExit(main())
