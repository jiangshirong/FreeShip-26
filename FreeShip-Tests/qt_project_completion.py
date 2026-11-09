"""Remaining project settings: explicit baseline alignment, authorship, viscosity.

Independent FBM observer, rigid-translation expectations, physical invariants,
solver input and real hidden Qt callbacks. No user models or desktop automation.
"""
import argparse,json,math,os,shutil,struct,subprocess,time
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_native_project as f
import qt_performance_cases as samples
import qt_performance_checks as pc
import qt_native_propulsion as propulsion

FILES=('qt_project_completion.py','qt_native_project.py','qt_native_propulsion.py','qt_performance_cases.py','qt_performance_checks.py','qt_migration.py','qt_editing.py','run_tests.py')
def read(path):return json.loads(Path(path).read_text('utf8'))
def near(a,b):return math.isclose(a,b,rel_tol=3e-6,abs_tol=3e-5)
def values(schema):return {x['key']:x['value'] for x in schema['fields']}

def elevated(path,version=44,relations=False):
    observed=f.fixture(path,version=version,rich=version>=20,relations=relations)
    data=bytearray(path.read_bytes());start=observed['spans']['control_points'][0]+4;stride=17+(version>=13)
    for i,p in enumerate(observed['points']):struct.pack_into('<f',data,start+i*stride+8,p[2]+5)
    path.write_bytes(data);return f.observe(path)

def formats(r,runtime,out):
    out.mkdir();env=dict(os.environ);env['PATH']=str(runtime)+os.pathsep+env.get('PATH','')
    def probe(source,name,changes,minimum_only=False):
        request=out/(name+'.json');target=out/(name+'.fbm')
        request.write_text(json.dumps(dict(bindings=dict(fbm_encoding='cp1251'),request=dict(method='update_project',changes=changes),minimum_only=minimum_only)),encoding='utf8')
        p=subprocess.run([str(runtime/'native_project_edit_probe.exe'),str(source),str(request),str(target)],env=env,capture_output=True,timeout=20,creationflags=q.common.FLAGS)
        request.with_suffix('.log').write_bytes(p.stdout+p.stderr)
        if p.returncode:raise RuntimeError('Project probe crashed')
        return json.loads(p.stdout),target
    for version in range(45):
        source=out/f'v{version}-source.fbm';before=elevated(source,version);raw=source.read_bytes()
        old=before['settings'].get('created_by',b'').decode('ascii')
        for name,changes in [('noop',dict(created_by=old,align_to_baseline=False)),('author',dict(created_by='New author')),('align',dict(align_to_baseline=True))]:
            promote=version<2 and name!='noop';result,target=probe(source,f'v{version}-{name}',changes,promote)
            r.case(f'v{version}.{name}.accepted',result['ok'],detail=result)
            if not result['ok']:continue
            r.case(f'v{version}.{name}.format',result['minimum_version']==(2 if promote else version))
            if promote:continue
            if name=='noop':r.case(f'v{version}.noop.exact',target.read_bytes()==raw and not result['changed'])
            if name=='author':
                at=before['offsets']['created_by'];n=len(before['settings']['created_by'])
                expected=raw[:at]+struct.pack('<i',10)+b'New author'+raw[at+4+n:]
                r.case(f'v{version}.author.exact',target.read_bytes()==expected)
            if name=='align':
                after=f.observe(target)
                r.case(f'v{version}.all-points',after['points']==[[x,y,z-5] for x,y,z in before['points']])
                r.case(f'v{version}.metadata',after['settings']==before['settings'] and after['hydro']==before['hydro'])
                r.case(f'v{version}.markers',[[p for p in m['points']] for m in after['markers']]==[[[x,y,z-5] for x,y,z in m['points']] for m in before['markers']])
                # Independent signed-plane translation: d' = d + 5*nz.
                for kind,items in before['sections'].items():
                    for i,(a,b) in enumerate(zip(after['sections'][kind],items)):
                        r.case(f'v{version}.plane.{kind}.{i}',a['plane']==b['plane'][:3]+[e.f32(b['plane'][3]+e.f32(5*b['plane'][2]))])
                r.case(f'v{version}.ancillary',after['backgrounds']==before['backgrounds'] and after['calculations']==before['calculations'])
                if before['flows']:r.case(f'v{version}.flow-seed',after['flows'][0]['seed']==[1.25,-2.5] and not after['flows'][0]['built'])
                again,dest=probe(target,f'v{version}-again',dict(align_to_baseline=True))
                r.case(f'v{version}.idempotent',again['ok'] and not again['changed'] and dest.read_bytes()==target.read_bytes())
    source=out/'locked.fbm';elevated(source,relations=True)
    for name,changes in [('locked',dict(align_to_baseline=True,created_by='reject all')),('type',dict(align_to_baseline=1)),('encoding',dict(created_by='无法编码'))]:
        result,target=probe(source,name+'-rejected',changes)
        r.case(name+'.rejected',not result['ok'] and not target.exists(),detail=result)

def backend(r,runtime,out):
    out.mkdir();model=out/'source.fbm';elevated(model);raw=model.read_bytes()
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        def call(name,method,**fields):return q.require_ok(s.command(method,output=out/name,**fields))
        def saved(name):return Path(call(name,'save')['saved_file'])
        before=call('load','load',path=str(model),document_id=str(q.uuid.uuid4()));initial=dict(s.state)
        # Old enable-auto-move bit is on in the fixture; calculations stay read-only.
        hydro0=read(call('hydro-before','hydrostatics',mode='single',draft=1,heel=0,trim=0)['hydro'])['rows'][0]
        r.case('hydro.no-implicit-move',s.state==initial and saved('save-before').read_bytes()==raw)
        changed=call('align','update_project',changes=dict(align_to_baseline=True,created_by='Verified author'))
        aligned=saved('aligned');observed=f.observe(aligned)
        r.case('mixed.one-revision',s.state['revision']==initial['revision']+1)
        r.case('mixed.coordinates',min(p[2] for p in observed['points'])==0)
        r.case('mixed.author',observed['settings']['created_by']==b'Verified author' and read(changed['model'])['settings']['created_by']=='Verified author')
        hydro1=read(call('hydro-after','hydrostatics',mode='single',draft=1,heel=0,trim=0)['hydro'])['rows'][0]
        for key in ('volume','displacement','wetted_area','waterplane_area','lwl','bwl','cb','cm','cp'):
            r.case('rigid.hydro.'+key,near(hydro0[key],hydro1[key]),before=hydro0[key],after=hydro1[key])
        # Hydrostatic Z is relative to the keel, not the model origin. A box
        # at draft 1 has its buoyancy centre at 0.5 before and after translation.
        r.case('rigid.buoyancy-z',near(hydro1['buoyancy'][2],.5) and near(hydro0['buoyancy'][2],.5))
        state=dict(s.state);call('noop','update_project',changes=dict(align_to_baseline=True));r.case('noop.no-revision',s.state==state)
        restored=call('undo','undo');r.case('undo.exact',saved('undo-save').read_bytes()==raw)
        for kind in ('scene','dxf'):r.compare('undo.'+kind,before[kind],restored[kind])
        redone=call('redo','redo');r.case('redo.exact',saved('redo-save').read_bytes()==aligned.read_bytes())
        for kind in ('scene','dxf'):r.compare('redo.'+kind,changed[kind],redone[kind])
        with q.Service(runtime/'FreeShipDocumentService.exe',out/'reopen-process',native_linesplan=True) as other:
            reopened=q.require_ok(other.command('load',output=out/'reopen',path=str(aligned),document_id=str(q.uuid.uuid4())))
            for kind in ('scene','dxf'):r.compare('reopen.'+kind,changed[kind],reopened[kind])
        call('reset','undo');unit=call('units-align','update_project',changes=dict(units='imperial',align_to_baseline=True))
        obs=f.observe(saved('units-save'));r.case('units.aligned',min(p[2] for p in obs['points'])==0 and obs['settings']['units']==1)
        call('unit-undo','undo');r.case('units.undo-whole',saved('unit-undo-save').read_bytes()==raw)
        lockmodel=out/'locked.fbm';elevated(lockmodel,relations=True);call('load-locked','load',path=str(lockmodel),document_id=str(q.uuid.uuid4()))
        state=dict(s.state);rejected=s.command('update_project',output=out/'reject',changes=dict(align_to_baseline=True,name='Must not persist'))
        r.case('locked.atomic-rejection',not rejected['ok'] and s.state==state and saved('locked-save').read_bytes()==lockmodel.read_bytes())
    return model

def water(r,runtime,out):
    out.mkdir()
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        def call(name,method,**fields):return q.require_ok(s.command(method,output=out/name,**fields))
        call('load','load',path=str(q.DEFAULT_MODELS[0]),document_id=str(q.uuid.uuid4()));state=dict(s.state)
        schema=pc.read(call('schema','performance_defaults',calculator='propeller4'))
        r.case('water.default-parity',values(schema)['WaterViscosity']==e.f32(1.189))
        # Execute original solver, observe its actual task file (not a mock).
        for viscosity in (1.189,.817):
            name='nu-'+str(viscosity);args=dict(samples.PROPELLERS['propeller4'],WaterViscosity=viscosity)
            result=pc.read(call(name,'performance',calculator='propeller4',values=args,solver_directory=str(propulsion.SOLVERS)))
            line=(out/name/'solver-files/INO').read_text('ascii').splitlines()[-1]
            r.case(name+'.solver-input',near(float(line.split()[-1])*1e6,e.f32(viscosity)))
            r.case(name+'.reported-input',values(result['parameters'])['WaterViscosity']==viscosity)
        r.case('water.readonly',s.state==state)
        for value in (0,-1,101,'bad'):
            reply=s.command('performance_defaults',output=out/('bad-'+str(value)),calculator='propeller4',values={'WaterViscosity':value})
            r.case('water.reject-'+str(value),not reply['ok'] and s.state==state)
        call('prop-save','performance_settings',calculator='propeller4',values={'WaterViscosity':.817})
        r.case('propeller.no-false-fbm-edit',s.state==state)
        defaults=values(pc.read(call('maneuver-defaults','performance_defaults',calculator='maneuver')))
        call('maneuver-save','performance_settings',calculator='maneuver',values=dict(defaults,Edit11=.817))
        extracted=pc.read(call('extract','performance_defaults',calculator='maneuver',extract=True,values={'Edit11':.95}))
        r.case('extract.keeps-entered-viscosity',values(extracted)['Edit11']==.95)
        extracted=pc.read(call('extract-stored','performance_defaults',calculator='maneuver',extract=True))
        r.case('extract.keeps-stored-viscosity',values(extracted)['Edit11']==e.f32(.817))
        saved=Path(call('save','save')['saved_file'])
        with q.Service(runtime/'FreeShipDocumentService.exe',out/'reopen-process',native_linesplan=True) as other:
            q.require_ok(other.command('load',output=out/'reopen',path=str(saved),document_id=str(q.uuid.uuid4())))
            data=pc.read(other.command('performance_defaults',output=out/'reopened-parameters',calculator='maneuver'))
            r.case('water.maneuver-fbm-roundtrip',values(data)['Edit11']==e.f32(.817))

def production(r,runtime,model,out):
    out.mkdir();private=out/'input.fbm';shutil.copy2(model,private);raw=private.read_bytes()
    edits=dict(align_to_baseline=True,created_by='UI author')
    commands=[dict(kind='startup',path=str(private)),dict(kind='project_action',changes=edits,cancel=True),dict(kind='project_action',changes=edits),dict(kind='undo'),dict(kind='redo'),dict(kind='save_action')]
    req=out/'request.json';req.write_text(json.dumps(dict(history_file=str(out/'history.json'),recovery_root=str(out/'recovery'),commands=commands)),encoding='utf8')
    child=q.run_child(r,'project',runtime/'FreeShipQt.exe',['--verify-startup',req],out/'project',platform='windows');r.data['production_project']=child
    observations=read(child['artifacts']['observations']);models=[read(row['model']) for row in observations]
    r.case('ui.cancel-exact',models[0]==models[1]);r.case('ui.author',models[2]['settings']['created_by']=='UI author')
    r.case('ui.undo',models[3]['settings']['created_by']==models[0]['settings']['created_by'])
    obs=f.observe(private);r.case('ui.save',obs['settings']['created_by']==b'UI author' and min(p[2] for p in obs['points'])==0)
    r.case('ui.backup',private.with_suffix('.Bak').read_bytes()==raw)
    for method,temp,expected,cancel in [('propeller4',0,1.82,False),('propeller4',30,.817,False),('propeller4',20,1.189,True),('maneuver',16.1,1.157,False)]:
        name=method+'-'+str(temp);folder=out/name;folder.mkdir()
        req=folder/'request.json';args=samples.PROPELLERS['propeller4'] if method=='propeller4' else {}
        req.write_text(json.dumps(dict(model=str(q.DEFAULT_MODELS[0]),inputs=dict(calculator=method,values=args,solver_directory=str(propulsion.SOLVERS)),water_temperature=temp,cancel_temperature=cancel,preset_roundtrip=True)),encoding='utf8')
        child=q.run_child(r,name,runtime/'FreeShipQt.exe',['--verify-performance',req],folder/'qt',platform='windows',timeout=150)
        r.data.setdefault('water_production',{})[name]=child
        actual=child['artifacts']['effective_inputs']['values']['WaterViscosity' if method=='propeller4' else 'Edit11']
        r.case(name+'.conversion',near(actual,expected),actual=actual,expected=expected)
        for kind in ('scene','dxf'):r.compare(name+'.readonly-'+kind,child['artifacts']['original_'+kind],child['artifacts']['after_'+kind])

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    runtime=(a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.monotonic();sources=q.current_sources(q.QT)
    originals={str(p):q.common.digest_file(p) for p in q.DEFAULT_MODELS}
    r.data.update(suite='qt-project-completion',runtime=str(runtime),qt_sources_sha256=sources,test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES})
    try:
        manifest=read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if {n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime mismatch')
        r.case('runtime-manifest',True);formats(r,runtime,out/'formats');model=backend(r,runtime,out/'service');water(r,runtime,out/'water');production(r,runtime,model,out/'production')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('models-protected',all(q.common.digest_file(Path(p))==h for p,h in originals.items()))
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.case('desktop-acceptance',status='not_run',reason='Hidden production events only; no physical desktop or file-dialog acceptance.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
