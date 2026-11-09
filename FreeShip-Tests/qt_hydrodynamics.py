"""Hydrodyn native service, raw external protocols and actual Qt callbacks.

Private files and processes only. Mathematical checks and mock process faults
are separate from real legacy solver execution and desktop acceptance.
"""
import argparse,csv,io,json,math,os,shutil,struct,subprocess,threading,time,uuid
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_performance_cases as samples
import qt_performance_checks as checks
import qt_native_project as fbm_observer
import qt_native_propulsion
from maintenance import generate_hydrodynamics_tables as metadata

METHODS=('wind','maneuver','acceleration')
SOLVERS=q.ROOT/'fixtures/solvers35/unpacked/Freeship/Exec'

def data(reply):return checks.read(reply)
def defaults(schema):return {f['key']:f['value'] for f in schema['fields']}
def fixture(out):
    out.mkdir(parents=True);compiler=Path(os.environ['LOCALAPPDATA'])/'FreeShip26QtTools/mingw1310_64/bin/g++.exe'
    env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH']
    p=subprocess.run([str(compiler),'-std=c++17','-O2','-static',str(q.ROOT/'qt/hydrodynamics_solver_fixture.cpp'),'-o',str(out/'Ishercof.exe')],capture_output=True,env=env,timeout=90,creationflags=q.common.FLAGS)
    (out/'compile.log').write_bytes(p.stdout+p.stderr)
    if p.returncode:raise RuntimeError('Process fixture compilation failed')
    shutil.copy2(out/'Ishercof.exe',out/'DBFview.EXE');shutil.copy2(SOLVERS/'PropPred.EXE',out/'PropPred.EXE')
    return out

def direct(r,name,exe,inputs,result,out,expected):
    out.mkdir(parents=True)
    for key,value in inputs.items():(out/key).write_bytes(value)
    p=subprocess.run([str(exe)],cwd=out,env=q.private_environment(out/'env'),input=b'\r\n' if exe.name.lower()=='maneuvpp.exe' else b'',capture_output=True,timeout=115,creationflags=q.common.FLAGS)
    (out/'process.log').write_bytes(p.stdout+p.stderr)
    (out/'process-result.json').write_text(json.dumps(dict(exit=p.returncode,exe=str(exe),sha256=q.common.digest_file(exe))),encoding='utf8')
    r.case(name+'.raw-exit',p.returncode==0,exit=p.returncode)
    if (out/result).exists():r.compare(name+'.raw-output',out/result,expected)
    else:r.case(name+'.raw-output',False,reason='Independent solver produced no output')

def run_service(r,runtime,out,fixtures):
    out.mkdir();schemas,_,_=metadata.generate();catalog={}
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process') as s:
        initial=q.require_ok(s.command('load',output=out/'load',path=str(q.DEFAULT_MODELS[0]),document_id=str(uuid.uuid4())))
        before=dict(s.state)
        for m in METHODS:
            reply=s.command('performance_defaults',output=out/(m+'-defaults'),calculator=m);catalog[m]=data(reply)
            r.case(m+'.native',reply.get('analysis_engine')=='cpp-hydrodynamics')
            r.case(m+'.fields',[(x['key'],x['label']) for x in catalog[m]['fields']]==[(x['key'],x['label']) for x in schemas[m]['fields']])
            r.case(m+'.readonly',s.state==before)
        # Independent polynomial values, vector composition and dimensional law.
        wind=defaults(schemas['wind']);wind.update(Edit9=12,Edit10=0,Edit11=10,Edit12=90,Edit6=200,Edit2=100,Edit8=50)
        sample=data(s.command('performance',output=out/'wind-math',calculator='wind',values=wind))
        cx,cy,cm=[x['points'] for x in sample['series']]
        r.case('wind.zero-degree',abs(cx[0][1]-.42)<1e-7 and cy[0][1]==0 and cm[0][1]==0)
        r.case('wind.ninety-degree',abs(cx[18][1]-.05)<1e-6 and abs(cy[18][1]-1.31)<1e-6 and abs(cm[18][1]+1.04)<1e-6)
        actual=sample['scalars']
        r.case('wind.relative-vector',actual['relative_wind_m_s']==10 and actual['relative_wind_angle_deg']==90)
        r.case('wind.yaw-length-factor',abs(actual['yaw_moment_kNm']-(-.104*12*100))<.0001,actual=actual)
        wind['Edit12']=-90
        negative=data(s.command('performance',output=out/'wind-port',calculator='wind',values=wind))['scalars']
        r.case('wind.port-starboard',abs(negative['yaw_moment_kNm']+actual['yaw_moment_kNm'])<1e-10 and negative['longitudinal_force_kN']==actual['longitudinal_force_kN'])
        wind.update(Edit10=10,Edit11=10,Edit12=180)
        calm=data(s.command('performance',output=out/'wind-calm',calculator='wind',values=wind))['scalars']
        r.case('wind.opposing-equal-vectors',calm['relative_wind_m_s']<1e-12 and abs(calm['yaw_moment_kNm'])<1e-20)
        for ship in (12,13,14,15):
            result=data(s.command('performance',output=out/f'wind-type-{ship}',calculator='wind',values=dict(defaults(schemas['wind']),Edit9=ship)))
            r.case(f'wind.type-{ship}.finite',len(result['series'])==3 and all(math.isfinite(n) for c in result['series'] for xy in c['points'] for n in xy))
        r.case('wind.type13-coefficient-conflict',status='review',reason='Original executed coefficient .91 conflicts with source comment .091; executed value retained pending engineering reference.')
        # Original Isherwood program, all 11 type/mast options, private input.
        for ship in range(1,12):
            vals=dict(defaults(schemas['wind']),Edit9=ship);folder=out/f'isherwood-{ship}'
            result=data(s.command('performance',output=folder,calculator='wind',values=vals,solver_directory=str(SOLVERS)))
            rows=[list(map(float,line.split())) for line in (folder/'solver-files/OUT').read_text('latin1').splitlines()[35:54]]
            r.case(f'isherwood.{ship}.raw-coefficients',all(result['series'][j]['points'][2*i][1]==(e.f32(row[1+j*2]) if j<2 else e.f32(e.f32(row[5])*10)) for i,row in enumerate(rows) for j in range(3)))
            if ship==4:
                expected=('Merchant ship\r\n'+f'{110:8.4f}{15:8.4f}{1000:10.3f}{400:10.3f}{250:10.3f}{160:10.3f}{55:10.3f}{4:8.0f}'+'\r\n*** End of file ***\r\n').encode()
                r.case('isherwood.fixed-input',expected==(folder/'solver-files/INO').read_bytes())
                direct(r,'isherwood',SOLVERS/'Ishercof.exe',{'INO':expected},'OUT',out/'direct-wind',folder/'solver-files/OUT')
        # MPP options: linear, turning and state equations; no fake chart for a
        # text-only solver. Independently build its fixed-width protocol sample.
        for flags in ((False,False,False),(True,False,False),(True,True,False),(True,True,True)):
            vals=defaults(schemas['maneuver']);vals.update(dict(zip(('CheckBox3','CheckBox4','CheckBox5'),flags)))
            name='maneuver-'+''.join(str(int(v)) for v in flags);folder=out/name
            result=data(s.command('performance',output=folder,calculator='maneuver',values=vals,solver_directory=str(SOLVERS)))
            r.case(name+'.report',len(result['text'])>1000 and 'MANEUVER' in result['text'].upper() and result['series']==[])
        vals=defaults(schemas['maneuver']);vals.update(CheckBox3=True,CheckBox4=True,CheckBox5=True)
        def f(n,w,p):return f'{e.f32(n):{w}.{p}f}'
        text=''.join(f(n,w,p) for n,w,p in [(111,11,6),(20,10,6),(6,10,6),(5,10,6),(.6,10,6),(2,12,6),(.02,10,6),(0,2,0)])+'\r\n'
        text+=''.join(f(n,w,p) for n,w,p in [(49,10,6),(10,10,6),(.25,10,6),(50,12,6),(2.5,10,6),(0,10,5),(1,2,0),(1,2,0)])+'\r\n'
        text+=''.join(f(n,w,p) for n,w,p in [(10,10,6),(1025,12,6),(e.f32(e.f32(1.61)/1e6),14,11),(1,2,0),(1,2,0),(1,2,0)])+'\r\n'
        r.case('maneuver.fixed-input',(out/'maneuver-111/solver-files/INM').read_bytes()==text.encode())
        direct(r,'maneuver',SOLVERS/'ManeuvPP.EXE',{'INM':text.encode()},'OUT',out/'direct-maneuver',out/'maneuver-111/solver-files/OUT')
        # Acceleration consumes a genuine current-revision resistance result.
        hc=data(s.command('performance_defaults',output=out/'holtrop-schema',calculator='holtrop'))
        hv=samples.sample('holtrop',{'holtrop':hc})
        data(s.command('performance',output=out/'resistance',calculator='holtrop',values=hv))
        for task in (1,2,3):
            vals=dict(defaults(schemas['acceleration']),Edit18=task);folder=out/f'acceleration-{task}'
            result=data(s.command('performance',output=folder,calculator='acceleration',values=vals,solver_directory=str(SOLVERS)))
            rows=[list(map(float,line.split())) for line in (folder/'solver-files/RVRSRES.dat').read_text('latin1').splitlines() if line.strip()]
            r.case(f'acceleration.{task}.time',result['scalars']['time_s']==e.f32(rows[-1][0]))
            wanted=[[[e.f32(row[0]),e.f32(e.f32(row[i+1])/scale)] for row in rows if row[0]>0] for i,scale in enumerate((1852,.514444,10,1,1))]
            r.case(f'acceleration.{task}.raw-curves',[c['points'] for c in result['series']]==wanted)
            if task==1:
                numbers=[float(x) for x in (folder/'solver-files/TMP4.tsk').read_text('ascii').split()] if (folder/'solver-files/TMP4.tsk').exists() else []
                # The input is intentionally captured before execution even if
                # the original solver deletes its task file on success.
                r.case('acceleration.protocol-23',len(numbers)==23)
        r.case('acceleration.engine-inertia-unit',status='review',reason='Original LFM t*m2 conflicts with original report kg*m2. Numeric protocol retained; needs external solver documentation.')
        r.case('calculations.readonly',s.state==before)
        snap=q.require_ok(s.command('snapshot',output=out/'after-calculations'))
        for k in ('scene','dxf'):r.compare('calculations.unchanged-'+k,initial[k],snap[k])
        r.case('calculations.no-pascal',snap.get('adapter_pid')==0 and snap.get('adapter_requests')==0)
        hydro_reply=q.require_ok(s.command('hydrostatics',output=out/'hydro-extraction',mode='single',draft=json.loads(Path(initial['model']).read_text('utf8'))['settings']['draft'],heel=0,trim=0))
        hydro=json.loads(Path(hydro_reply['hydro']).read_text('utf8'))['rows'][0]
        for method in ('wind','maneuver'):
            extracted=defaults(data(s.command('performance_defaults',output=out/(method+'-extract'),calculator=method,extract=True)))
            r.case(method+'.extract-beam',extracted['Edit3']==e.f32(hydro['bwl']))
            r.case(method+'.extract-readonly',s.state==before)
        # Seed the existing propulsion protocol with its explicit deterministic
        # fixture. This tests extraction/units, not CALCPROP engineering results.
        prop_fixture=qt_native_propulsion.fixture_solver(out/'propulsion-fixture')
        prop_schema=data(s.command('performance_defaults',output=out/'propulsion-schema',calculator='propeller2'))
        data(s.command('performance',output=out/'propulsion-fixture-result',calculator='propeller2',values=samples.sample('propeller2',{'propeller2':prop_schema}),solver_directory=str(prop_fixture)))
        extracted=defaults(data(s.command('performance_defaults',output=out/'acceleration-extract',calculator='acceleration',extract=True)))
        r.case('acceleration.extract-volume-not-mass',extracted['Edit5']==e.f32(hydro['volume']) and abs(hydro['volume']-hydro['displacement'])>1e-4)
        r.case('acceleration.extract-propulsion',extracted['Edit8']==3 and abs(extracted['Edit9']-1.1)<1e-6)
        for method in METHODS:
            base=data(s.command('performance_defaults',output=out/(method+'-before'),calculator=method))
            vals=defaults(schemas[method]);vals['Edit2']=123.125
            q.require_ok(s.command('performance_settings',output=out/(method+'-save'),calculator=method,values=vals))
            actual=data(s.command('performance_defaults',output=out/(method+'-saved'),calculator=method))
            r.case(method+'.settings',defaults(actual)['Edit2']==123.125)
            saved=q.require_ok(s.command('save',output=out/(method+'-file')));path=Path(saved['saved_file'])
            raw=fbm_observer.observe(path)['blocks'][{'wind':'hydrodynamic_task1','maneuver':'maneuvering','acceleration':'reverse_propeller'}[method]]
            r.case(method+'.independent-fbm-field',struct.unpack_from('<f',raw)[0]==123.125)
            q.require_ok(s.command('undo',output=out/(method+'-undo')))
            restored=data(s.command('performance_defaults',output=out/(method+'-restored'),calculator=method))
            r.case(method+'.undo',defaults(restored)==defaults(base))
            with q.Service(runtime/'FreeShipDocumentService.exe',out/(method+'-reopen-process')) as other:
                q.require_ok(other.command('load',output=out/(method+'-reopen'),path=str(path),document_id=str(uuid.uuid4())))
                stored=data(other.command('performance_defaults',output=out/(method+'-roundtrip'),calculator=method))
                r.case(method+'.fbm-roundtrip',defaults(stored)['Edit2']==123.125)
        # All invalid inputs fail before publication; the committed model stays.
        for name,m,patch in [('unknown','wind',{'not_a_field':1}),('fractional-kind','wind',{'Edit9':1.5}),('out-of-domain','maneuver',{'Edit2':1}),('negative-speed','wind',{'Edit11':-1}),('zero-turn-angle','maneuver',dict(defaults(schemas['maneuver']),CheckBox4=True,Edit20=0))]:
            state=dict(s.state);folder=out/name;reply=s.command('performance',output=folder,calculator=m,values=patch,solver_directory=str(SOLVERS))
            r.case(name+'.rejected',not reply['ok'],error=reply.get('error'));r.case(name+'.no-result',not (folder/'performance.json').exists());r.case(name+'.readonly',s.state==state)
        for mode in ('missing','exit','truncated','nonfinite'):
            (fixtures/'mode.txt').write_text(mode,'ascii');folder=out/('fault-'+mode)
            reply=s.command('performance',output=folder,calculator='wind',values=defaults(schemas['wind']),solver_directory=str(fixtures))
            r.case('solver.'+mode,not reply['ok'] and not (folder/'performance.json').exists(),error=reply.get('error'))
        folder=out/'cancel';folder.mkdir();marker=fixtures/'started.txt';(fixtures/'mode.txt').write_text('wait\n'+str(marker),'ascii')
        def cancel():
            until=time.monotonic()+10
            while not marker.exists() and time.monotonic()<until:time.sleep(.02)
            (folder/'cancel.flag').write_text('cancel','ascii')
        thread=threading.Thread(target=cancel);thread.start();start=time.monotonic()
        reply=s.command('performance',output=folder,calculator='wind',values=defaults(schemas['wind']),solver_directory=str(fixtures));thread.join()
        r.case('solver.running-cancel',not reply['ok'] and '取消' in reply.get('error','') and time.monotonic()-start<5 and marker.exists())
        if marker.exists():
            import ctypes
            api=ctypes.WinDLL('kernel32',use_last_error=True);api.OpenProcess.restype=ctypes.c_void_p;api.OpenProcess.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32];api.CloseHandle.argtypes=[ctypes.c_void_p];api.GetExitCodeProcess.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_ulong)]
            handle=api.OpenProcess(0x1000,False,int(marker.read_text()));code=ctypes.c_ulong(259)
            if handle:api.GetExitCodeProcess(handle,ctypes.byref(code));api.CloseHandle(handle)
            r.case('solver.cancel-child-exited',not handle or code.value!=259)
        (fixtures/'mode.txt').write_text('missing','ascii')
        reply=s.command('performance',output=out/'old-revision',calculator='acceleration',solver_directory=str(SOLVERS))
        r.case('acceleration.old-revision-rejected',not reply['ok'] and '当前船模版本' in reply.get('error',''),error=reply.get('error'))
    return schemas,hv

def production(r,runtime,out,schemas,hv,fixtures):
    cases=[('wind','wind',{},{}),('maneuver','maneuver',{'CheckBox3':True,'CheckBox4':True},{}),
           ('acceleration','acceleration',{'Edit18':2},dict(prepare=dict(calculator='holtrop',values=hv))),
           ('cancel','wind',{},dict(cancel=True)),('preset','maneuver',{},dict(preset_roundtrip=True)),
           ('save','acceleration',{},dict(prepare=dict(calculator='holtrop',values=hv),save_settings=True)),
           ('missing-db','propeller4',samples.PROPELLERS['propeller4'],dict(database_viewer=True,expect_error=True)),
           ('fixture-db','propeller4',samples.PROPELLERS['propeller4'],dict(database_viewer=True))]
    for name,method,patch,flags in cases:
        folder=out/name;folder.mkdir(parents=True);vals=dict(defaults(schemas[method]),**patch) if method in schemas else patch
        solver=fixtures if name=='fixture-db' else SOLVERS
        req=folder/'request.json';req.write_text(json.dumps(dict(model=str(q.DEFAULT_MODELS[0]),inputs=dict(calculator=method,values=vals,solver_directory=str(solver)),**flags),ensure_ascii=False),encoding='utf8')
        child=q.run_child(r,'production.'+name,runtime/'FreeShipQt.exe',['--verify-performance',req],folder/'qt',timeout=150,platform='windows');r.data.setdefault('production',{})[name]=child
        art=child.get('artifacts',{})
        if 'after_scene' in art:
            for k in ('scene','dxf'):r.compare('production.'+name+'.unchanged-'+k,art['original_'+k],art['after_'+k])
        if 'report_json' in art and Path(art['report_json']).exists():
            result=json.loads(Path(art['report_json']).read_text('utf8'))
            rows=list(csv.reader(io.StringIO(Path(art['report_csv']).read_text('utf-8-sig'))))
            r.case('production.'+name+'.csv',[[float(x[-2]),float(x[-1])] for x in rows[1:]]==[xy for c in result['series'] for xy in c['points']])
            r.case('production.'+name+'.pdf',Path(art['report_pdf']).read_bytes().startswith(b'%PDF-'))
    r.case('database.real-viewer',status='not_run',reason='DBFview.EXE is absent in supplied solver package. Missing-file path and isolated fixture launch tested; no database supplied or modified.')
    r.case('ui.visual-parity',status='review',reason='Hidden Windows screenshots retained; full old dialog layout parity and desktop interaction remain separate.')
    r.case('desktop',status='not_run',reason='No desktop, native file dialog or physical printer acceptance.')
    # Existing calculator dialogs share the lazy table. Exercise their real
    # callbacks with the previous, independently retained input fixtures.
    for method in ('delft','holtrop'):
        folder=out/('regression-'+method);folder.mkdir()
        with q.Service(runtime/'FreeShipDocumentService.exe',folder/'schema-process') as s:
            q.require_ok(s.command('load',output=folder/'load',path=str(q.DEFAULT_MODELS[0]),document_id=str(uuid.uuid4())))
            schema=data(s.command('performance_defaults',output=folder/'schema',calculator=method))
        req=checks.production_request(q.DEFAULT_MODELS[0],folder,method,{method:schema},SOLVERS)
        child=q.run_child(r,'regression.'+method,runtime/'FreeShipQt.exe',['--verify-performance',req],folder/'qt',platform='windows');r.data['production']['regression-'+method]=child
        art=child.get('artifacts',{})
        if 'after_scene' in art:
            for k in ('scene','dxf'):r.compare('regression.'+method+'.unchanged-'+k,art['original_'+k],art['after_'+k])

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--service-only',action='store_true');a=p.parse_args()
    runtime=(a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use private output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data.update(suite='qt-hydrodynamics',runtime=str(runtime),limitations=[
        'Hydrodyn orchestration, FBM settings and publication use C++; Ishercof, ManeuvPP and RVRSSHIP remain external original solvers.',
        'Independent arithmetic checks and byte-exact solver protocols do not certify the empirical methods for engineering use.',
        'Type 13 wind coefficient and engine inertia units retain explicit unresolved source conflicts.',
        'DBFview.EXE is unavailable; the process fixture verifies callbacks, not a real database viewer.',
        'Qt production actions run in hidden private Windows instances. Native mouse, focus, dialogs, printing and full original layout parity are not certified.'])
    sources=q.current_sources(q.QT);protected=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in ('qt_hydrodynamics.py','qt/hydrodynamics_solver_fixture.cpp','maintenance/generate_hydrodynamics_tables.py','qt_migration.py','qt_editing.py','qt_performance_checks.py','qt_performance_cases.py','qt_native_project.py','qt_native_propulsion.py','qt/propulsion_solver_fixture.cpp','freeship_ui_test.py','run_tests.py')}
    r.data['source_sha256']=protected;r.data['qt_sources_sha256']=sources;start=time.monotonic()
    try:
        manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime source manifest')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime hash mismatch')
        r.case('runtime-manifest',True)
        checked=subprocess.run([os.sys.executable,'-B',str(q.ROOT/'maintenance/generate_hydrodynamics_tables.py'),'--check'],capture_output=True,creationflags=q.common.FLAGS);r.case('metadata-origin',checked.returncode==0,log=checked.stdout.decode()+checked.stderr.decode())
        fixtures=fixture(out/'solver-fixture');schemas,hv=run_service(r,runtime,out/'service',fixtures)
        if not a.service_only:production(r,runtime,out/'production',schemas,hv,fixtures)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==protected)
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))))
    return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
