"""Behavior checks, original-core observer and raw solver evidence for Qt performance.

Fixture inputs are protocol examples, not engineering validation. Preserve exact
curve coordinates; old undefined array data is not a valid numerical oracle.
"""
import csv,io,json,math,subprocess,shutil
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_performance_cases as samples
import solver_launch_probe as direct

BROKEN_CALCPROP='947757d7bf58eb4a087033b1206f15613053057d0ccf79f307bdfe6183b0f3a8'

def values(schema):return {f['key']:f['value'] for f in schema['fields']}
def curves(data):return {s['source']:s['points'] for s in data['series']}
def read(response):return json.loads(Path(q.require_ok(response)['performance']).read_text('utf8'))

def dependency_failure(r,method,response,folder,solvers):
    """Only this independently observed executable is a known blocked dependency.
    A different error/hash is a regression, never automatically downgraded.
    """
    expected=method in ('propeller1','propeller2','propeller3') and q.common.digest_file(solvers/'CALCPROP.EXE')==BROKEN_CALCPROP
    expected=expected and 'CALCPROP.EXE: Missing calculation output:' in response.get('error','')
    if not expected:return False
    probe=direct.probe(solvers/'CALCPROP.EXE',folder/'solver-files',folder/'independent-solver')
    absent=f'RES{method[-1]}.tsk' not in probe['output_sha256']
    r.case(method+'.independent-dependency-failure',probe.get('exit_code')==0 and absent)
    r.case(method+'.numerical-result',status='not_run',reason='Pinned historical CALCPROP exits zero without RES output in independent ASCII workspace; no fallback formula.',error=response['error'],probe=str(folder/'independent-solver/probe.json'))
    return True

def service_contract(r,exe,catalog,out,solvers):
    out.mkdir(parents=True)
    with q.Service(exe,out/'process') as s:
        base=e.take(r,'performance',s,out,'base','load',path=str(q.DEFAULT_MODELS[0]));initial=dict(s.state)
        for name,method,inputs in [
            ('unknown-method','performance_defaults',dict(calculator='missing')),
            ('unknown-field','performance',dict(calculator='delft',values={'not_a_field':1})),
            ('wrong-type','performance',dict(calculator='delft',values={'LwlBox':'bad'})),
            ('overflow','performance',dict(calculator='delft',values={'LwlBox':'EXPONENT_OVERFLOW'},raw_nonfinite=True)),
            ('zero-step','performance',dict(calculator='delft',values=dict(samples.sample('delft',catalog),StepSpeedBox=0))),
            ('stale-document','performance_defaults',dict(calculator='delft',expected_document_id='stale')),
            ('stale-revision','performance',dict(calculator='delft',expected_revision=0)),
            ('missing-directory','performance',dict(calculator='holtrop',solver_directory=str(out/'missing'))),
            ('invalid-choice','performance',dict(calculator='propeller5',values={'ComboBox':99})),
            ('missing-almeter','performance',dict(calculator='planing',values=dict(samples.sample('planing',catalog),ComboBox=9))),
            ('planing-too-few-points','performance',dict(calculator='planing',values=dict(samples.sample('planing',catalog),ComboBox=3),solver_directory=str(solvers))),
        ]:
            response=s.command(method,output=out/name,**inputs)
            r.case(name+'.reject',not response['ok'],error=response.get('error'))
            if name=='planing-too-few-points':r.case(name+'.clear-error','不足四个有效计算点' in response.get('error',''))
            r.case(name+'.state',s.state==initial)
        cancel=out/'cancelled';cancel.mkdir();(cancel/'cancel.flag').write_text('cancel')
        response=s.command('performance',output=cancel,calculator='delft',values=samples.sample('delft',catalog))
        r.case('cancelled.rejected',not response['ok']);r.case('cancelled.state',s.state==initial)
        empty=out/'empty-solvers';empty.mkdir()
        response=s.command('performance',output=out/'missing-exe',calculator='fung',values=samples.sample('fung',catalog),solver_directory=str(empty))
        r.case('missing-exe.reject',not response['ok']);r.case('missing-exe.state',s.state==initial)
        # Full supplied objects may be sorted differently by Qt/JSON. Dependencies
        # (propeller family -> series -> numeric parameters) must give equal inputs.
        requested=dict(samples.sample('propeller5',catalog),ComboBox=2,ComboBox1=2)
        a=read(s.command('performance_defaults',output=out/'order-a',calculator='propeller5',values=requested))
        b=read(s.command('performance_defaults',output=out/'order-b',calculator='propeller5',values=dict(reversed(list(requested.items())))))
        r.case('parameter-order-independent',values(a)==values(b))
        choice=next(f for f in a['fields'] if f['key']=='ComboBox1')
        r.case('dependent-series-label',choice['choices'][2]=='M-4-85')
        extracted=read(s.command('performance_defaults',output=out/'hull-extracted',calculator='holtrop',extract=True))
        r.case('extracted-hull-remains-editable',next(f for f in extracted['fields'] if f['key']=='LwlBox')['enabled'])
        after=e.take(r,'performance',s,out,'after');e.same(r,'readonly-geometry',base,after)
        # Resistance data may be extracted only for the same document revision.
        first=s.command('performance_defaults',output=out/'no-resistance',calculator='propeller2',extract=True)
        r.case('extract-needs-resistance',not first['ok'])
        read(s.command('performance',output=out/'holtrop',calculator='holtrop',values=samples.sample('holtrop',catalog)))
        extracted=s.command('performance_defaults',output=out/'propeller-extracted',calculator='propeller2',extract=True)
        r.case('resistance-to-propeller',extracted['ok'],error=extracted.get('error'))
        originals={};changed={}
        for method in catalog:
            defaults=read(s.command('performance_defaults',output=out/(method+'-before'),calculator=method));originals[method]=values(defaults)
            target='Edit2' if method.startswith('propeller') or method in ('kaper','planing') else 'LwlBox'
            key=next(f['key'] for f in defaults['fields'] if f['key'].lower()==target.lower());number=24
            q.require_ok(s.command('performance_settings',output=out/(method+'-set'),calculator=method,values={key:number}))
            change=read(s.command('performance_defaults',output=out/(method+'-changed'),calculator=method));changed[method]=(key,values(change)[key])
            r.case(method+'.settings-change',values(change)[key]==number)
            q.require_ok(s.command('undo',output=out/(method+'-undo')))
            actual=read(s.command('performance_defaults',output=out/(method+'-undone'),calculator=method))
            r.case(method+'.settings-undo',values(actual)==originals[method])
            q.require_ok(s.command('redo',output=out/(method+'-redo')))
        saved=e.saved_copy(s,out/'save')
        # A new revision invalidates old resistance even if settings-only; no
        # ambiguous reuse across a changed document is admitted.
        stale=s.command('performance_defaults',output=out/'stale-resistance',calculator='propeller2',extract=True)
        r.case('extract-rejects-old-revision',not stale['ok'])
        q.require_ok(s.command('load',output=out/'reopen',path=str(saved),document_id=str(q.uuid.uuid4())))
        for method,(key,wanted) in changed.items():
            actual=read(s.command('performance_defaults',output=out/(method+'-reopened'),calculator=method))
            r.case(method+'.settings-fbm-roundtrip',values(actual)[key]==wanted)

def original_oracle(r,exe,oracle,catalog,out,solvers):
    out.mkdir(parents=True)
    report=json.loads((oracle.parent/'report.json').read_text('utf8'))
    r.case('oracle.original-source',report['source_sha256']==q.common.fingerprint(q.common.DEFAULT_SOURCE)[0])
    expected=report['test_sources_sha256'];r.case('oracle.current-observer',all(q.common.digest_file(q.ROOT/n)==h for n,h in expected.items()))
    r.case('oracle.executable',q.common.digest_file(oracle)==report['build']['executable_sha256'])
    with q.Service(exe,out/'process') as s:
        q.require_ok(s.command('load',output=out/'load',path=str(q.DEFAULT_MODELS[0]),document_id=str(q.uuid.uuid4())))
        for method in ('delft','holtrop','hollenbach','oortmerssen','fung','kaper','planing'):
            folder=out/method;folder.mkdir();v=samples.sample(method,catalog)
            # Original Hollenbach UI incorrectly caps maximum speed at 10.
            # Use a common valid domain for numerical parity; 20 kn is tested
            # separately by the production sample after fixing that UI cap.
            if method=='hollenbach':v.update(EndSpeedBox=8,StepSpeedBox=9)
            req=folder/'input.json';req.write_text(json.dumps(dict(calculator=method,values=v,solver_directory=str(solvers)),ensure_ascii=False),encoding='utf8')
            p=subprocess.run([str(oracle),str(q.DEFAULT_MODELS[0]),str(req),str(folder)],cwd=folder,env=q.private_environment(folder),capture_output=True,timeout=35,creationflags=q.common.FLAGS)
            (folder/'oracle.log').write_bytes(p.stdout+p.stderr)
            r.case('oracle.'+method+'.exit',p.returncode==0,exit=p.returncode)
            if p.returncode or not (folder/'oracle.json').exists():continue
            old=json.loads((folder/'oracle.json').read_text('utf8'))
            new=read(s.command('performance',output=folder/'current',calculator=method,values=v,solver_directory=str(solvers)))
            r.case('oracle.'+method+'.exact-coordinates',bool(old['series']) and curves(old)==curves(new))

def production_request(model,folder,method='holtrop',catalog=None,solvers=None,**flags):
    folder.mkdir(parents=True,exist_ok=True);p=folder/'performance-request.json'
    # A fixed fixture is used by actual-directory deployment verification too.
    if catalog:v=samples.sample(method,catalog)
    else:v={k:v for k,v in samples.CONVENTIONAL.items() if k not in ('LoSbox',)}
    inputs={'calculator':method,'values':v}
    if solvers:inputs['solver_directory']=str(solvers)
    p.write_text(json.dumps(dict(model=str(model),inputs=inputs,**flags),ensure_ascii=False),encoding='utf8');return p

def production(r,runtime,catalog,out,solvers,blocked):
    out.mkdir(parents=True);exe=runtime/'FreeShipQt.exe';model=q.DEFAULT_MODELS[0]
    cases=[(m,m,dict(expect_error=True) if m in blocked else {}) for m in catalog]
    cases += [('cancel','delft',{'cancel':True}),('repeat-cancel','holtrop',{'repeat_cancel':True}),('save-settings','delft',{'save_settings':True}),('preset','propeller5',{'preset_roundtrip':True})]
    for label,method,flags in cases:
        folder=out/label;request=production_request(model,folder,method,catalog,solvers,**flags)
        child=q.run_child(r,'production.'+label,exe,['--verify-performance',request],folder/'qt',timeout=90,platform='windows')
        # The deployment reference exercises the newly native route. Previous
        # reports containing Holtrop remain valid references for their own run.
        if label=='holtrop':r.data['models'].append({'model':str(model),'sha256':q.common.digest_file(model),'production':child})
        if any(c.get('status')=='not_run' for c in child.get('cases',[])):continue
        art=child['artifacts']
        for kind in ('scene','dxf'):r.compare('production.'+label+'.readonly-'+kind,art['original_'+kind],art['after_'+kind])
        if flags.get('expect_error') or flags.get('cancel'):continue
        result=json.loads(Path(art['report_json']).read_text('utf8'))
        rows=list(csv.reader(io.StringIO(Path(art['report_csv']).read_text('utf-8-sig'))))
        expected=[p for s in result['series'] for p in s['points']]
        r.case('production.'+label+'.csv-exact-coordinates',len(rows)==len(expected)+1 and [[float(x[-2]),float(x[-1])] for x in rows[1:]]==expected)
        r.case('production.'+label+'.pdf',Path(art['report_pdf']).read_bytes().startswith(b'%PDF-'))
        if label=='holtrop':
            import fitz
            with fitz.open(art['report_pdf']) as doc:
                text=''.join(page.get_text() for page in doc)
                r.case('production.pdf-readable','FreeShip_26' in text and '本次计算参数' in text and '水线长' in text)
                doc[0].get_pixmap(matrix=fitz.Matrix(1.2,1.2)).save(folder/'pdf-page-1.png')
        text=Path(art['report_txt']).read_text('utf8');r.case('production.'+label+'.actual-parameters-in-report','本次计算参数' in text)
    r.case('visual-parity',status='review',reason='Hidden production screenshots retained; desktop input and full original-window parity remain separate.')

def raw_outputs(r,results,out,solvers):
    """Compare an independent process run with original protocol output, then
    compare the corrected FPP parser with raw numeric rows (no shared parser).
    """
    mapping={'oortmerssen':'OORTMERS.EXE','fung':'fungleib.EXE','hollenbach':'HOLLENBH.EXE','ost':'hship.EXE','rbhs':'hship.EXE','mh':'hship.EXE','propeller4':'PropPred.EXE','propeller5':'FPPCALC.EXE'}
    for method,exe in mapping.items():
        folder=results/method/'solver-files'
        if not folder.exists():continue
        # Only compare actual result files generated by that solver.
        names=('ostres.tmp',) if method in ('ost','rbhs','mh') else ('OUT','POPGR') if method=='propeller4' else ('OUT',) if method=='propeller5' else ({'fung':'fungleib.res','hollenbach':'hollenba.res','oortmerssen':'oortmers.res'}[method],)
        p=direct.probe(solvers/exe,folder,out/method,enter=method=='propeller4',output_names=names)
        r.case('direct.'+method+'.exit',p.get('exit_code')==0)
        for name in names:
            old=next((x for x in folder.iterdir() if x.name.rstrip('.').lower()==name.lower()),None)
            new=next((x for x in (out/method).iterdir() if x.name.rstrip('.').lower()==name.lower()),None)
            r.case('direct.'+method+'.'+name,bool(old and new) and old.read_bytes()==new.read_bytes())
    fpp=results/'propeller5/performance.json';raw=results/'propeller5/solver-files'
    if fpp.exists():
        output=next(p for p in raw.iterdir() if p.name.rstrip('.').upper()=='OUT')
        lines=output.read_text('utf8',errors='replace').splitlines()[25:]
        rows=[list(map(float,line.split())) for line in lines if len(line.split())==8]
        data=json.loads(fpp.read_text('utf8'));actual=curves(data)
        for name,col,mult in [('Series1',3,1),('Series2',6,10),('Series3',7,1)]:
            # Reader stores original single-precision protocol values.
            wanted=[[e.f32(row[0]),e.f32(e.f32(row[col])*mult)] for i,row in enumerate(rows) if row[3]>=0 and (i==0 or row[0]>rows[i-1][0])]
            r.case('fpp.'+name+'.raw-file-coordinates',actual.get(name)==wanted)

def branches(r,exe,catalog,out,solvers):
    out.mkdir(parents=True)
    with q.Service(exe,out/'process') as s:
        q.require_ok(s.command('load',output=out/'load',path=str(q.DEFAULT_MODELS[0]),document_id=str(q.uuid.uuid4())))
        for method,indices in [('planing',range(9)),('propeller5',range(1,12))]:
            for index in indices:
                v=samples.sample(method,catalog);v['ComboBox']=index
                if method=='planing' and index in (3,4):v['Edit12']=65 if index==3 else 75
                if method=='propeller5':v['ComboBox1']=0
                name=f'{method}-{index}';res=s.command('performance',output=out/name,calculator=method,values=v,solver_directory=str(solvers))
                r.case('branch.'+name,res['ok'],error=res.get('error'))
                if res['ok']:
                    d=read(res);r.case('branch.'+name+'.finite',all(math.isfinite(x) for curve in d['series'] for point in curve['points'] for x in point))
