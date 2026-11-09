"""Native resistance calculations versus the isolated original form engine.

Retains full numerical/protocol evidence; these are deterministic examples,
not ship trials or real desktop interaction. Invoked by qt-performance too.
"""
import argparse,json,math,re,uuid,os,subprocess,threading,time,tempfile,shutil
from pathlib import Path
import qt_migration as q
import qt_performance_cases as samples
import qt_performance_checks as checks
from qt_native_propulsion import compare_schema as original_schema
from maintenance import port_resistance

METHODS=tuple(port_resistance.METHODS)
SOLVERS=q.ROOT/'fixtures/solvers35/unpacked/Freeship/Exec'

def compare_schema(r,name,a,b):
    # Empty labels in disabled MH fields used the old edit's text ("1.00")
    # as a title. Keep meaningful field names; this is a documented UI repair.
    b=json.loads(json.dumps(b))
    if a['calculator']=='mh':
        actual={f['key']:f for f in a['fields']}
        for f in b['fields']:
            if f['key'] in ('NfBox','NaBox') and not f['enabled'] and re.fullmatch(r'-?\d+\.\d+',f['label']):
                key=f['key'];expected={'NfBox':'艉部前端形式','NaBox':'艉部后端形式'}[key]
                r.case(name+'.meaningful-label.'+key,actual[key]['label']==expected,old=f['label']);f['label']=expected
    original_schema(r,name,a,b)

def parity(r,runtime,models,out,quick=False):
    out.mkdir(parents=True,exist_ok=True);catalog={}
    for index,model in enumerate(models):
        folder=out/str(index);identity=str(uuid.uuid4())
        with q.Service(runtime/'FreeShipDocumentService.exe',folder/'native') as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',folder/'oracle') as oracle:
            for name,s in [('native',native),('oracle',oracle)]:q.require_ok(s.command('load',output=folder/name/'load',path=str(model),document_id=identity))
            initial=dict(native.state)
            for method in METHODS:
                for extract in (False,True) if not quick else (False,):
                    name=f'{method}-'+('extract' if extract else 'defaults')
                    ar=native.command('performance_defaults',output=folder/'native'/name,calculator=method,extract=extract)
                    br=oracle.command('performance_defaults',output=folder/'oracle'/name,calculator=method,extract=extract)
                    r.case(f'{index}.{name}.ok',ar['ok']==br['ok'],actual=ar,expected=br)
                    if ar['ok'] and br['ok']:
                        a=checks.read(ar);b=checks.read(br);compare_schema(r,f'{index}.{name}',a,b)
                        if not extract:catalog[method]=b
                    elif not extract:continue
                if index or method not in catalog:continue
                arguments=dict(calculator=method,values=samples.sample(method,catalog),solver_directory=str(SOLVERS))
                ar=native.command('performance',output=folder/'samples'/method,**arguments)
                br=oracle.command('performance',output=folder/'oracle-samples'/method,**arguments)
                r.case(method+'.sample.ok',ar['ok'] and br['ok'],actual=ar,expected=br)
                if not ar['ok'] or not br['ok']:continue
                a=checks.read(ar);b=checks.read(br)
                r.case(method+'.native-owner',ar.get('analysis_engine')=='cpp-resistance')
                r.case(method+'.curves',a['series']==b['series'],actual=a['series'],expected=b['series'])
                compare_schema(r,method+'.parameters',a['parameters'],b['parameters'])
                r.case(method+'.help',a['help'].replace('\r\n','\n')==b['help'].replace('\r\n','\n'))
                r.case(method+'.state',native.state==initial)
                if method=='delft':
                    curves=checks.curves(a)
                    r.case('delft.independent-power-law',all(abs(power-resistance*kn*1852/3600)<=max(1e-5,abs(power)*3e-7) for (kn,resistance),(_,power) in zip(curves['Series3'],curves['Series4'])))
    return catalog

def branches(r,runtime,catalog,out):
    out.mkdir(parents=True,exist_ok=True)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native-process') as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle-process') as oracle:
        for label,s in [('native',native),('oracle',oracle)]:q.require_ok(s.command('load',output=out/label/'load',path=str(q.DEFAULT_MODELS[0]),document_id=str(uuid.uuid4())))
        cases=[]
        for method,count in [('oortmerssen',6),('ost',6),('rbhs',9),('mh',9),('planing',9)]:
            for index in range(count):
                v=samples.sample(method,catalog);v['ComboBox']=index
                if method=='planing' and index in (3,4):v['Edit12']=65 if index==3 else 75
                if method in ('rbhs','mh') and index==0:
                    v.update({f'Edit17_{i}':.2+i*.1 for i in range(1,6)});v.update({f'Edit18_{i}':.003+i*.0002 for i in range(1,6)})
                cases.append((f'{method}-{index}',method,v))
        for flag in ('EstimateBox','Estimate2Box','Estimate3Box','Estimate4Box'):
            v=samples.sample('holtrop',catalog);v[flag]=True;cases.append(('holtrop-'+flag,'holtrop',v))
        for name,method,values in cases:
            args=dict(calculator=method,values=values,solver_directory=str(SOLVERS))
            a=native.command('performance',output=out/'native'/name,**args)
            if name=='holtrop-Estimate3Box':
                # PowerPrd waits for Enter even after producing OUT. The old
                # caller sends EOF. Exercise the binary directly, with bounded
                # runtime, instead of spending 110 seconds on every regression.
                import solver_launch_probe as direct
                inputs=out/'native'/name/'solver-files'
                probe=direct.probe(SOLVERS/'PowerPrd.EXE',inputs,out/'power-direct',enter=True,output_names=('OUT',))
                r.case('powerprediction.native-result',a['ok'],error=a.get('error'))
                r.case('powerprediction.direct-result',probe.get('exit_code')==0)
                r.case('powerprediction.direct-output',q.common.digest_file(inputs/'OUT')==q.common.digest_file(out/'power-direct/OUT'))
                r.case('powerprediction.old-eof-protocol',status='review',reason='Old caller closes stdin without Enter; retained source and resistance-dev-04 show it waits after writing OUT. Native sends Enter, then validates the process and report.')
                continue
            if name=='planing-4':
                # CLEMBLOU's legacy executable produces NaN in a long working
                # directory, with identical input bytes that work in a short
                # one. The retained repeat probe isolates this from the C++
                # calculation. Keep the original equations/curve comparator;
                # stage only its working directory, and retain every artifact.
                temp_root=Path(out.anchor)/'FreeShip26Tests/t'
                temp_root.mkdir(parents=True,exist_ok=True)
                with tempfile.TemporaryDirectory(prefix='ro-',dir=temp_root) as temp:
                    b=oracle.command('performance',output=Path(temp)/'run',**args)
                    retained=out/'oracle'/name
                    shutil.copytree(temp,retained)
                    if b['ok']:b['performance']=str(retained/'run/performance.json')
                r.case('clemblou.legacy-working-directory',status='review',reason='Original numerical comparison uses a short workspace: the pinned CLEMBLOU binary emits NaN with the same inputs in a long directory. Reproduced by maintenance/probe_resistance_repeat.py; native calculation already uses an independent short workspace.')
            else:
                b=oracle.command('performance',output=out/'oracle'/name,**args)
            if name=='holtrop-EstimateBox' and not b['ok'] and b.get('error')=='Floating point division by zero':
                r.case('estimated-area.initialized-draft',a['ok'],error=a.get('error'))
                r.case('estimated-area.old-draft-order',status='review',reason='Old CorrectInputData divides by member Tc before assigning draft; native initializes it before estimating wetted area.',error=b['error'])
                if a['ok']:
                    report=checks.read(a);line=next(x for x in report['text'].splitlines() if '湿表面积' in x and '估算' in x)
                    am=8500/(120*.65);cm=am/(18*6);cwp=1800/(120*18)
                    c23=.453+.443*.65*cm-.286*cm-.00347*18/6+.37*cwp
                    factor=.616*c23+.111*cm**3+.245*c23/cm-.0228;expected=factor*120*(12+18)*math.sqrt(cm)
                    r.case('estimated-area.independent-formula',abs(float(re.search(r':\s*([0-9.]+)',line)[1])-expected)<.02,expected=expected,line=line)
                continue
            r.case('branch.'+name+'.availability',a['ok']==b['ok'],native=a.get('error'),oracle=b.get('error'))
            if a['ok'] and b['ok']:
                aa=checks.read(a);bb=checks.read(b);r.case('branch.'+name+'.curves',aa['series']==bb['series'],actual=aa['series'],expected=bb['series'])
            elif not b['ok']:r.case('branch.'+name+'.numeric-domain',status='not_run',reason='Original solver rejects this sample; no numerical result to compare.',error=b.get('error'))
        for method in METHODS:
            # Stored values and undo must use the native document record.
            field=next(f for f in catalog[method]['fields'] if f['kind']=='number' and f['key'] not in ('DensityBox',))['key']
            for label,s in [('native',native),('oracle',oracle)]:
                q.require_ok(s.command('performance_settings',output=out/label/(method+'-stored'),calculator=method,values={field:1}))
            a=checks.read(native.command('performance_defaults',output=out/'native'/(method+'-stored-default'),calculator=method));b=checks.read(oracle.command('performance_defaults',output=out/'oracle'/(method+'-stored-default'),calculator=method))
            # The old form writes every getter back when saving one value,
            # normalizing unrelated zeros to defaults. Native sparse edits
            # preserve unrelated bytes (covered by native-analysis regressions).
            r.case(method+'.persisted-value',checks.values(a)[field]==checks.values(b)[field],field=field,native=checks.values(a)[field],oracle=checks.values(b)[field])
            for label,s in [('native',native),('oracle',oracle)]:q.require_ok(s.command('undo',output=out/label/(method+'-undo')))
        for label,s in [('native',native),('oracle',oracle)]:q.require_ok(s.command('update_project',output=out/label/'imperial',changes={'units':'imperial'}))
        for method in ('delft','kaper'):
            for extract in (False,True):
                args=dict(calculator=method,extract=extract);name=f'imperial-{method}-{extract}'
                a=checks.read(native.command('performance_defaults',output=out/'native'/name,**args));b=checks.read(oracle.command('performance_defaults',output=out/'oracle'/name,**args));compare_schema(r,name,a,b)

def run(r,runtime,models,out):
    out.mkdir(parents=True,exist_ok=False)
    generated,_=port_resistance.generate()
    for name,text in generated.items():r.case('resistance.provenance.'+name,(q.QT/'src/document'/name).read_text('utf8')==text)
    catalog=parity(r,runtime,models,out/'parity')
    branches(r,runtime,catalog,out/'branches')
    protocols(r,runtime,catalog,out/'protocols')
    with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'guard-oracle') as s:
        q.require_ok(s.command('load',output=out/'guard-load',path=str(models[0]),document_id=str(uuid.uuid4())))
        for method in METHODS:
            response=s.command('performance_defaults',output=out/('guard-'+method),calculator=method,external_document=True)
            r.case('resistance.forbid-old.'+method,not response['ok'] and 'must execute in C++' in response.get('error',''))
    return catalog

def protocols(r,runtime,catalog,out):
    out.mkdir(parents=True,exist_ok=True);solvers=out/'solvers';solvers.mkdir()
    compiler=Path(os.environ['LOCALAPPDATA'])/'FreeShip26QtTools/mingw1310_64/bin/g++.exe';env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH']
    p=subprocess.run([str(compiler),'-std=c++17','-O2','-static',str(q.ROOT/'qt/resistance_solver_fixture.cpp'),'-o',str(solvers/'HOLLENBH.EXE')],env=env,capture_output=True,timeout=90,creationflags=q.common.FLAGS)
    (out/'fixture-build.log').write_bytes(p.stdout+p.stderr)
    if p.returncode:raise RuntimeError('Resistance failure fixture compilation failed')
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native-process') as s:
        q.require_ok(s.command('load',output=out/'load',path=str(q.DEFAULT_MODELS[0]),document_id=str(uuid.uuid4())));state=dict(s.state)
        checks.read(s.command('performance',output=out/'good',calculator='holtrop',values=samples.sample('holtrop',catalog)))
        cache=out/'performance-cache'/('doc-'+state['document_id'].replace('-','')+'-r'+str(state['revision']))/'current'
        r.case('resistance.cache.published',cache.is_file())
        for mode in ('missing','truncated','nonfinite','extra','exit'):
            (solvers/'CONF.PRO').write_text(mode+'\n','ascii');folder=out/mode
            response=s.command('performance',output=folder,calculator='hollenbach',values=samples.sample('hollenbach',catalog),solver_directory=str(solvers))
            r.case('resistance.solver.'+mode,not response['ok'] and not (folder/'performance.json').exists(),error=response.get('error'))
            r.case('resistance.solver.'+mode+'.readonly',s.state==state)
        r.case('resistance.cache.failure-invalidates',not cache.exists())
        reply=s.command('performance_defaults',output=out/'stale-propeller',calculator='propeller2',extract=True)
        r.case('resistance.cache.no-stale-propeller',not reply['ok'],error=reply.get('error'))
        checks.read(s.command('performance',output=out/'before-planing',calculator='holtrop',values=samples.sample('holtrop',catalog)))
        reply=s.command('performance',output=out/'invalid-planing',calculator='planing',values=dict(samples.sample('planing',catalog),ComboBox=3),solver_directory=str(SOLVERS))
        r.case('resistance.cache.planing-failure-invalidates',not reply['ok'] and '不足四个有效计算点' in reply.get('error','') and not cache.exists(),error=reply.get('error'))
        folder=out/'cancel-running';folder.mkdir();marker=out/'started.txt';(solvers/'CONF.PRO').write_text('wait\n'+str(marker)+'\n','ascii')
        def cancel():
            deadline=time.monotonic()+8
            while not marker.exists() and time.monotonic()<deadline:time.sleep(.02)
            (folder/'cancel.flag').write_text('cancel','ascii')
        worker=threading.Thread(target=cancel);worker.start();started=time.perf_counter()
        reply=s.command('performance',output=folder,calculator='hollenbach',values=samples.sample('hollenbach',catalog),solver_directory=str(solvers));worker.join();elapsed=time.perf_counter()-started
        r.case('resistance.cancel-running',not reply['ok'] and '取消' in reply.get('error','') and elapsed<5,elapsed=elapsed,error=reply.get('error'))
        if marker.exists():
            import ctypes
            api=ctypes.WinDLL('kernel32',use_last_error=True);api.OpenProcess.restype=ctypes.c_void_p;api.OpenProcess.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32];api.CloseHandle.argtypes=[ctypes.c_void_p];api.GetExitCodeProcess.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_ulong)]
            handle=api.OpenProcess(0x1000,False,int(marker.read_text()));code=ctypes.c_ulong(259)
            if handle:api.GetExitCodeProcess(handle,ctypes.byref(code));api.CloseHandle(handle)
            r.case('resistance.cancel-child-exited',not handle or code.value!=259)
        snap=q.require_ok(s.command('snapshot',output=out/'snapshot'))
        if snap.get('adapter_pid'):
            import qt_document
            qt_document.stop_owned_adapter(int(snap['adapter_pid']),runtime/'bridge/FreeShipGeometryBridge.exe')
        for method in METHODS:
            response=s.command('performance_defaults',output=out/('detached-'+method),calculator=method)
            r.case('resistance.detached.'+method,response['ok'] and response.get('analysis_engine')=='cpp-resistance')
        response=s.command('performance',output=out/'detached-formula',calculator='delft',values=samples.sample('delft',catalog))
        r.case('resistance.detached.formula',response['ok'] and response.get('analysis_engine')=='cpp-resistance')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--quick',action='store_true');a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=True);r=q.Report();r.data['limitations']=['No desktop interaction; formula and protocol parity is not engineering validation against trials.']
    try:
        catalog=parity(r,a.runtime.resolve(),q.DEFAULT_MODELS[:1] if a.quick else q.DEFAULT_MODELS,a.output/'parity',a.quick)
        if not a.quick:
            branches(r,a.runtime.resolve(),catalog,a.output/'branches')
            protocols(r,a.runtime.resolve(),catalog,a.output/'protocols')
    except Exception as ex:
        import traceback
        r.case('execution',status='error',reason=str(ex),traceback=traceback.format_exc())
    r.finish();(a.output/'report.json').write_text(json.dumps(r.data,ensure_ascii=False,indent=2),'utf8');print(r.data['counts']);return 1 if r.data['counts']['fail'] or r.data['counts']['error'] else 0
if __name__=='__main__':raise SystemExit(main())
