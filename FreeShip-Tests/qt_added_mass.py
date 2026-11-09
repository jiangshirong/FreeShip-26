"""AddMass native workflow, original solver, independent protocols and Qt callbacks.

No desktop automation. Solvers run only in private directories. Raw comparisons
establish orchestration fidelity, not independent engineering certification.
"""
import argparse, copy, csv, io, json, math, os, shutil, subprocess, threading, time, uuid
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_exchange as x
import qt_legacy_exchange as legacy

SOLVERS=q.ROOT/'fixtures/solvers35/unpacked/Freeship/Exec'
VALUES=dict(depth=10000,drift=0,velocity_knots=10,omega=0,object_type=0)
FILES=('qt_added_mass.py','qt/added_mass_probe.cpp','qt/added_mass_solver_fixture.cpp',
       'maintenance/probe_addmass.py','qt_migration.py','qt_editing.py','qt_exchange.py','qt_legacy_exchange.py','run_tests.py')

def payload(reply):
    q.require_ok(reply)
    return json.loads(Path(reply['performance']).read_text('utf8'))

def ellipsoid_input():
    rows=['Ellipsoid L=10 B=4 T=1.5','1025 400 100 .2 1 10000 0 10 0 1',
          '0 0 0 0 0 0 0 0 0 0','10 1.5','21 1. 0.','11']
    for i in range(21):
        rows.append(' '.join(format(2*math.sqrt(max(0,1-(-1+i/10)**2))*math.sqrt(max(0,1-(-1+j/10)**2)),'.17g') for j in range(11)))
    return ('\r\n'.join(rows)+'\r\n\\*\r\n').encode('ascii')

def solver_run(folder,raw):
    folder.mkdir(parents=True);(folder/'INA').write_bytes(raw);(folder/'input.txt').write_bytes(raw)
    env=q.private_environment(folder/'env');env['GFORTRAN_CONVERT_UNIT']='native'
    p=subprocess.run([str(SOLVERS/'ADD_MASS.EXE')],cwd=folder,env=env,input=b'',capture_output=True,timeout=115,creationflags=q.common.FLAGS)
    (folder/'solver.log').write_bytes(p.stdout+p.stderr)
    if p.returncode:raise RuntimeError('Original AddMass failed: '+str(p.returncode))
    return (folder/'OUTA').read_bytes()

def probe(r,runtime,out,raw):
    out.mkdir();requests=[]
    def parse(name,data,ok=True,values=None):
        path=out/(name+'.txt');path.write_bytes(data)
        requests.append(dict(name=name,method='parse',path=str(path),values=values or VALUES,ok=ok))
    parse('real',raw)
    lines=raw.splitlines()
    for name,data in [('short',b'\n'.join(lines[:-1])),('extra',raw+b'1 2 3\n'),('nul',raw+b'\0'),('nonfinite',raw.replace(b'4318.714',b'     NaN')),('wrong-row',raw.replace(b'| 1 |',b'| 7 |')),('wrong-column',raw.replace(b'| N |',b'| X |')),('bad-divisor',raw.replace(lines[11],b'480 0')),('bad-panels',raw.replace(lines[11],b'1.5 1'))]:parse(name,data,False)
    parse('stale-condition',raw,False,VALUES|dict(depth=100))
    ell=lines.copy();ell[16]=b'NaN 2 3';parse('undefined-ellipsoid',b'\n'.join(ell))
    ell=lines.copy();ell[16]=b'1D+2 2d-1 3';parse('fortran-exponent',b'\n'.join(ell))
    cases=[('sea',2,60,10,2,.7,VALUES),('sea-full',2,60,10,2,.8,VALUES),
           ('river-rest',3,60,10,2,.8,VALUES|dict(velocity_knots=0)),
           ('river-moving',3,60,10,2,.8,VALUES|dict(depth=10,drift=15,velocity_knots=8,omega=.03)),
           ('river-lower',3,40,10,10/3.7,.5,VALUES),('river-upper',3,85,10,10/14,.93,VALUES),
           ('all-domain-errors',3,8,4,2,1,VALUES|dict(depth=2)),('bad-geometry',2,0,4,2,.7,VALUES)]
    for name,t,L,B,T,cb,v in cases:requests.append(dict(name=name,method='empirical',type=t,L=L,B=B,T=T,cb=cb,values=v,ok=name!='bad-geometry'))
    x.dump(out/'requests.json',requests)
    p=subprocess.run([str(runtime/'qt_added_mass_probe.exe'),str(out/'requests.json')],capture_output=True,timeout=30,creationflags=q.common.FLAGS)
    (out/'process.log').write_bytes(p.stderr);(out/'results.json').write_bytes(p.stdout)
    if p.returncode:raise RuntimeError('AddMass probe failed')
    results={v['name']:v for v in json.loads(p.stdout)}
    for req in requests:r.case('probe.'+req['name'],results[req['name']]['ok']==req['ok'],error=results[req['name']].get('error'))
    parsed=results['real']['result'];raw_matrix=[[float(c.strip()) for c in line.decode().split('|')[2:8]] for line in lines[4:10]]
    r.case('parser.all-36-values',parsed['matrix']==raw_matrix)
    r.case('parser.scaling',parsed['panel_count']==480 and parsed['scale_divisor']==1)
    r.case('parser.undefined-not-zero',results['undefined-ellipsoid']['result']['ellipsoid'][4]['values'][0] is None)
    r.case('parser.fortran-exponent',results['fortran-exponent']['result']['ellipsoid'][4]['values']==[100,.2,3])
    sea=results['sea']['result']['table']['values'][0]
    # Independent hand-reduced L/B=6, B/T=5 values.
    r.case('formula.sea-hand-values',all(math.isclose(a,b,abs_tol=1e-14) for a,b in zip(sea,[.0198,11/30,221/750])))
    full=results['sea-full']['result']['table']['values'][1]
    r.case('formula.sea-full-factor',all(math.isclose(a,b*c,abs_tol=1e-14) for a,b,c in zip(full,sea,[1.4,1.3,1.3])))
    rest=results['river-rest']['result'];rows=rest['table']['values']
    r.case('formula.zero-motion',rows[0]==rows[1]==rows[2])
    r.case('formula.no-uninitialized-river-correction',len(rows)==3 and 'warning' in rest)
    moving=results['river-moving']['result']['table']['values']
    r.case('formula.river-positive-motion',all(a>b for a,b in zip(moving[1],moving[0])) and all(a>b for a,b in zip(moving[2],moving[1])))
    r.case('formula.all-domain-errors',len(results['all-domain-errors']['result']['reasons'])==4 and 'table' not in results['all-domain-errors']['result'])
    r.case('formula.inclusive-boundaries',all(results[name]['result']['status']=='ok' for name in ('river-lower','river-upper')))
    return parsed

def model_fixture(runtime,out):
    # Independent polyhedral ellipsoid half-body, with outward winding. The
    # native generator may subdivide it; comparisons use its actual INA bytes.
    out.mkdir();part=out/'template.part';part.write_bytes(legacy.part_fixture())
    data=x.probe(runtime/'qt_exchange_tests.exe',[dict(name='template',method='read',format='part',path=str(part),options={})],out/'codec')['template']['data']
    data['project'].update(name='AddMass ellipsoid fixture',units='metric',length=10,beam=4,draft=1.5,water_density=1.025,precision=0);data['particulars_set']=True
    points=[[10,0,1.5]];rings=[]
    for i in range(1,20):
        phi=math.pi*i/20;ring=[]
        for j in range(13):
            theta=math.pi*j/12;ring.append(len(points));points.append([5+5*math.cos(phi),0 if j in (0,12) else 2*math.sin(phi)*math.sin(theta),1.5-1.5*math.sin(phi)*math.cos(theta)])
        rings.append(ring)
    end=len(points);points.append([0,0,1.5]);faces=[]
    for j in range(12):faces.append([0,rings[0][j],rings[0][j+1]]);faces.append([end,rings[-1][j+1],rings[-1][j]])
    for a,b in zip(rings,rings[1:]):
        for j in range(12):faces.append([a[j],b[j],b[j+1],a[j+1]])
    for f in faces:
        n=x.normal([points[k] for k in f[:3]]);c=[sum(points[k][d] for k in f)/len(f)-[5,0,1.5][d] for d in range(3)]
        if sum(a*b for a,b in zip(n,c))<0:f.reverse()
    data['points']=[dict(p=p,vertex_type=3,locked=False) for p in points];data['faces']=[dict(layer=0,points=f) for f in faces]
    edges=sorted({tuple(sorted((a,b))) for f in faces for a,b in zip(f,f[1:]+f[:1])});data['edges']=[[*edge,True] for edge in edges];data.pop('control_curves',None)
    data['layers'][0].update(symmetric=True,use_in_hydrostatics=True,use_for_intersections=True,show_in_linesplan=True)
    x.dump(out/'ellipsoid.json',data);return out/'ellipsoid.json'

def fixture(out,raw):
    out.mkdir();compiler=Path(os.environ['LOCALAPPDATA'])/'FreeShip26QtTools/mingw1310_64/bin/g++.exe';env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH']
    p=subprocess.run([str(compiler),'-std=c++17','-O2','-static',str(q.ROOT/'qt/added_mass_solver_fixture.cpp'),'-o',str(out/'ADD_MASS.EXE')],env=env,capture_output=True,timeout=60,creationflags=q.common.FLAGS)
    (out/'compile.log').write_bytes(p.stdout+p.stderr)
    if p.returncode:raise RuntimeError('Process fixture compile failed')
    (out/'result.txt').write_bytes(raw);return out

def service(r,runtime,out,model,fixtures):
    out.mkdir()
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process') as s:
        before=q.require_ok(s.command('import_model',output=out/'load',path=str(model),document_id=str(uuid.uuid4())))
        saved=e.saved_copy(s,out/'saved');state=dict(s.state)
        schema=payload(s.command('performance_defaults',output=out/'defaults',calculator='addmass'))
        r.case('defaults.no-false-fbm-persistence',schema['persistable'] is False)
        values={f['key']:f['value'] for f in schema['fields']};values['panels']=400
        good=payload(s.command('performance',output=out/'actual',calculator='addmass',values=values,solver_directory=str(SOLVERS)))
        r.case('actual.native-workflow',good['engine'].startswith('C++') and len(good['matrix'])==6)
        r.case('actual.finite-matrix',all(math.isfinite(v) for row in good['matrix'] for v in row))
        raw=(out/'actual/solver-files/OUTA').read_bytes();direct=solver_run(out/'direct',(out/'actual/solver-files/INA').read_bytes())
        r.case('actual.independent-run-identical',raw==direct)
        lines=raw.splitlines();r.case('actual.every-matrix-entry',good['matrix']==[[float(c) for c in line.split(b'|')[2:8]] for line in lines[4:10]])
        r.case('actual.private-input-preserved',(out/'actual/solver-files/INA').exists())
        # The Calculate route and the existing export route must feed the
        # same sampled geometry and parameters to the external program.
        refs=values|dict(panel_type=values['panel_type']+1,object_type=values['object_type']+1)
        export=out/'manual-INA';x.dump(out/'export-request.json',dict(format='addmass',model=before['model'],input=str(Path(before['model']).parent/'native-input.json'),output=str(export),document_id=s.state['document_id'],revision=s.state['revision'],units='m',close_deck=False,references=refs))
        p=subprocess.run([str(runtime/'FreeShipGeometryExport.exe'),str(out/'export-request.json'),str(out/'export-report.json'),str(os.getpid())],capture_output=True,timeout=60,creationflags=q.common.FLAGS)
        r.case('actual.existing-export-identical',p.returncode==0 and export.read_bytes()==(out/'actual/solver-files/INA').read_bytes())
        sea=payload(s.command('performance',output=out/'sea',calculator='addmass',values=values|dict(object_type=2),solver_directory=str(SOLVERS)))
        r.case('actual.sea-postprocess',sea['empirical']['status']=='ok' and len(sea['tables'])==3)
        river=payload(s.command('performance',output=out/'river-domain',calculator='addmass',values=values|dict(object_type=3),solver_directory=str(SOLVERS)))
        r.case('actual.river-domain-no-fake-values',river['empirical']['status']=='out_of_domain' and len(river['tables'])==2 and bool(river['warnings']))
        for name,patch in [('even',dict(stations=6)),('fraction',dict(panels=400.5)),('shallow',dict(depth=1)),('bad-type',dict(object_type=4)),('unknown',dict(garbage=1)),('text',dict(draft='2'))]:
            reply=s.command('performance',output=out/name,calculator='addmass',values=values|patch,solver_directory=str(SOLVERS));r.case('reject.'+name,not reply['ok'] and not (out/name/'performance.json').exists())
        stale=s.command('performance',output=out/'stale',calculator='addmass',expected_revision=state['revision']+1);r.case('reject.stale-revision',not stale['ok'])
        missing=s.command('performance',output=out/'missing-exe',calculator='addmass',solver_directory=str(out/'absent'));r.case('reject.missing-executable',not missing['ok'])
        for mode in ('exit','missing','truncated','nonfinite'):
            (fixtures/'mode.txt').write_text(mode);reply=s.command('performance',output=out/('fault-'+mode),calculator='addmass',values=values,solver_directory=str(fixtures));r.case('solver.'+mode,not reply['ok'] and not (out/('fault-'+mode)/'performance.json').exists(),reason=reply.get('error'))
        marker=out/'cancel-pid';folder=out/'cancel';folder.mkdir();(fixtures/'mode.txt').write_text('wait\n'+str(marker))
        def cancel():
            for _ in range(1000):
                if marker.exists():(folder/'cancel.flag').write_text('cancel');return
                time.sleep(.01)
        thread=threading.Thread(target=cancel);thread.start();start=time.monotonic();reply=s.command('performance',output=folder,calculator='addmass',values=values,solver_directory=str(fixtures));thread.join()
        r.case('solver.cancel-started-process',marker.exists() and not reply['ok'] and '取消' in reply.get('error','') and time.monotonic()-start<12)
        if marker.exists():
            import ctypes
            api=ctypes.WinDLL('kernel32');api.OpenProcess.restype=ctypes.c_void_p;api.CloseHandle.argtypes=[ctypes.c_void_p];api.GetExitCodeProcess.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_ulong)]
            h=api.OpenProcess(0x1000,False,int(marker.read_text()));code=ctypes.c_ulong(259)
            if h:api.GetExitCodeProcess(h,ctypes.byref(code));api.CloseHandle(h)
            r.case('solver.cancel-child-exited',not h or code.value!=259)
        r.case('service.readonly-history',s.state==state)
        after=q.require_ok(s.command('snapshot',output=out/'unchanged'))
        for key in ('scene','dxf','model'):r.compare('service.preserves-'+key,before[key],after[key])
        q.require_ok(s.command('update_project',changes=dict(units='imperial'),output=out/'feet'))
        feet=payload(s.command('performance',output=out/'imperial',calculator='addmass',values=dict(panels=400),solver_directory=str(SOLVERS)))
        r.case('actual.imperial-metric-geometry',all(math.isclose(good['geometry'][key],feet['geometry'][key],rel_tol=2e-6) for key in ('length_m','draft_m','volume_m3')))
        r.case('actual.imperial-finite-matrix',all(math.isfinite(v) for row in feet['matrix'] for v in row))
        # The user's retained real ship, read only, including its Unicode name.
        real=q.DEFAULT_MODELS[2];digest=q.common.digest_file(real)
        q.require_ok(s.command('load',output=out/'real-load',path=str(real),document_id=str(uuid.uuid4())))
        result=payload(s.command('performance',output=out/'real-ship',calculator='addmass',values=dict(panels=400,object_type=2),solver_directory=str(SOLVERS)))
        r.case('actual.real-ship-finite',all(math.isfinite(v) for row in result['matrix'] for v in row))
        direct=solver_run(out/'real-direct',(out/'real-ship/solver-files/INA').read_bytes())
        r.case('actual.real-ship-original-output',direct==(out/'real-ship/solver-files/OUTA').read_bytes())
        r.case('actual.real-ship-untouched',q.common.digest_file(real)==digest)
    return saved,values

def production(r,runtime,out,model,values,fixtures):
    for name,flags,solver in [('actual',dict(repeat_cancel=True),SOLVERS),('preset',dict(preset_roundtrip=True),SOLVERS),('missing',dict(expect_error=True),out/'absent')]:
        folder=out/name;folder.mkdir(parents=True);req=folder/'request.json';x.dump(req,dict(model=str(model),inputs=dict(calculator='addmass',values=values,solver_directory=str(solver)),**flags))
        child=q.run_child(r,'production.'+name,runtime/'FreeShipQt.exe',['--verify-performance',req],folder/'qt',timeout=150,platform='windows');r.data.setdefault('production',{})[name]=child;art=child.get('artifacts',{})
        if 'after_scene' in art:
            for key in ('scene','dxf'):r.compare('production.'+name+'.preserves-'+key,art['original_'+key],art['after_'+key])
        if 'report_json' in art:
            result=json.loads(Path(art['report_json']).read_text('utf8'));rows=list(csv.reader(io.StringIO(Path(art['report_csv']).read_text('utf-8-sig'))))
            expected=[value for table in result['tables'] for row in table['values'] for value in row];r.case('production.'+name+'.csv-values',len(rows)==1+len(expected) and all((float(row[3])==value if value is not None else row[3]=='') for row,value in zip(rows[1:],expected)))
            r.case('production.'+name+'.report-matrix',all(str(row[0]) in Path(art['report_txt']).read_text('utf8') for row in result['matrix'] if row[0]!=0))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--service-only',action='store_true');a=p.parse_args()
    runtime=(a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data.update(suite='qt-added-mass',runtime=str(runtime),limitations=['Native C++ document and AddMass workflow; original external numerical solver retained.','Hidden production Qt callbacks are separate from real desktop acceptance.','Raw results and independent formula checks do not certify engineering accuracy.']);sources=q.current_sources(q.QT);before=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0];start=time.monotonic()
    r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in FILES};r.data['qt_sources_sha256']=sources;r.data['source_sha256']=before
    try:
        manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime hash mismatch')
        r.case('runtime-manifest',True);raw=solver_run(out/'independent-ellipsoid',ellipsoid_input());probe(r,runtime,out/'protocol',raw)
        fixtures=fixture(out/'solver-fixture',raw);model=model_fixture(runtime,out/'model');saved,values=service(r,runtime,out/'service',model,fixtures)
        if not a.service_only:production(r,runtime,out/'production',saved,values,fixtures)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==before)
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.case('engineering-validation',status='review',reason='Original external solver and upstream empirical formulae; no independent engineering certification. Raw output precision remains unchanged.')
        r.case('desktop',status='not_run',reason='Hidden production Qt callbacks/screenshots only; physical desktop acceptance and exact legacy-dialog visual parity remain separate.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
