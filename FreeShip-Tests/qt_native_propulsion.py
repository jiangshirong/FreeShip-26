"""Native propulsion entrypoints, solver protocols, numeric parity and failures.

Actual historical solvers and deterministic protocol fixtures are reported
separately. Fixture success is never engineering validation of a missing solver.
No desktop automation. Outputs and private child processes live under --output.
"""
import argparse,json,math,os,shutil,subprocess,threading,time,uuid
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_performance_cases as samples
import qt_performance_checks as checks
from maintenance import generate_propulsion_tables as metadata,port_propulsion_reports as reports

METHODS=tuple(f'propeller{n}' for n in range(1,6))
SOLVERS=q.ROOT/'fixtures/solvers35/unpacked/Freeship/Exec'

def read(reply):return checks.read(reply)
def compare_schema(r,name,a,b):
    for k in ('title','calculator'):r.case(name+'.'+k,a.get(k)==b.get(k))
    af={f['key']:f for f in a['fields']};bf={f['key']:f for f in b['fields']}
    if a['calculator']=='propeller4':
        extra=af.pop('WaterViscosity',{})
        r.case(name+'.explicit-viscosity',extra.get('value')==e.f32(1.189) and extra.get('minimum')==.01 and extra.get('maximum')==100)
    r.case(name+'.field-order',list(af)==list(bf))
    for key in af:r.case(name+'.'+key,af[key]==bf.get(key),actual=af[key],expected=bf.get(key))

def parity(r,runtime,models,out,quick=False,contracts=True):
    out.mkdir(parents=True);catalog={}
    for index,model in enumerate(models):
        folder=out/str(index);identity=str(uuid.uuid4())
        with q.Service(runtime/'FreeShipDocumentService.exe',folder/'native') as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',folder/'oracle') as oracle:
            for name,s in [('native',native),('oracle',oracle)]:q.require_ok(s.command('load',output=folder/name/'load',path=str(model),document_id=identity))
            initial=dict(native.state)
            for method in METHODS:
                a=read(native.command('performance_defaults',output=folder/'native'/method,calculator=method));b=read(oracle.command('performance_defaults',output=folder/'oracle'/method,calculator=method));catalog[method]=b
                compare_schema(r,f'defaults.{index}.{method}',a,b)
                # Sparse stored values must hydrate from FBM, not the static field contract.
                if not quick:
                    for s in (native,oracle):q.require_ok(s.command('performance_settings',output=folder/s.folder.name/(method+'-saved'),calculator=method,values={'Edit2':24.125}))
                    aa=read(native.command('performance_defaults',output=folder/'native'/(method+'-stored'),calculator=method));bb=read(oracle.command('performance_defaults',output=folder/'oracle'/(method+'-stored'),calculator=method))
                    r.case(f'stored.{index}.{method}',checks.values(aa)['Edit2']==checks.values(bb)['Edit2'])
                    for s in (native,oracle):q.require_ok(s.command('undo',output=folder/s.folder.name/(method+'-undo')))
            if index:continue
            for method in METHODS:
                arguments=dict(calculator=method,values=samples.sample(method,catalog),solver_directory=str(SOLVERS))
                aa=native.command('performance',output=folder/'samples'/method,**arguments)
                bb=oracle.command('performance',output=folder/'oracle-samples'/method,**arguments)
                if method in METHODS[:3]:
                    r.case(method+'.matching-dependency',not aa['ok'] and not bb['ok'] and 'Missing calculation output:' in aa.get('error','') and 'Missing calculation output:' in bb.get('error',''),native=aa.get('error'),oracle=bb.get('error'))
                    if not quick:checks.dependency_failure(r,method,aa,folder/'samples'/method,SOLVERS)
                else:
                    a=read(aa);b=read(bb);r.case(method+'.native-owner',aa.get('analysis_engine')=='cpp-propulsion')
                    r.case(method+'.curves',a['series']==b['series'],actual=a['series'],expected=b['series'])
                    compare_schema(r,method+'.parameters',a['parameters'],b['parameters'])
                    r.case(method+'.help',a['help'].replace('\r\n','\n')==b['help'].replace('\r\n','\n'))
                    for file in ('INO','OUT','POPGR') if method=='propeller4' else ('INO','OUT'):
                        na=folder/'samples'/method/'solver-files'/file;ob=folder/'oracle-samples'/method/'solver-files'/file
                        r.case(method+'.protocol.'+file,na.read_bytes()==ob.read_bytes())
                    if method=='propeller5':r.case(method+'.complete-report',a['text']==b['text'],actual=a['text'],expected=b['text'])
                    if method=='propeller4':
                        # The bridge omitted numbered captions. Compare the
                        # unchanged derived numbers separately from repaired labels.
                        import re
                        def derived(text):
                            tail=text.split('附加计算',1)[-1].split('Copyright',1)[0]
                            return re.findall(r'(?<![A-Za-z])\b\d+\.\d{3}\b',tail)
                        r.case('propeller4.derived-report',derived(a['text'])==derived(b['text']) and len(derived(a['text']))==5,actual=derived(a['text']),expected=derived(b['text']))
            for family in range(1,12):
                args=dict(calculator='propeller5',values=dict(samples.sample('propeller5',catalog),ComboBox=family,ComboBox1=0),solver_directory=str(SOLVERS))
                a=read(native.command('performance',output=folder/'family'/str(family),**args));b=read(oracle.command('performance',output=folder/'oracle-family'/str(family),**args))
                r.case('family.'+str(family)+'.curves',a['series']==b['series']);r.case('family.'+str(family)+'.report',a['text']==b['text'],actual=a['text'],expected=b['text'])
            checks.raw_outputs(r,folder/'samples',folder/'direct-solvers',SOLVERS)
            if quick:continue
            # Discover current resistance fields rather than depend on old reports.
            for method in ('delft','holtrop','hollenbach','oortmerssen','fung','ost','rbhs','mh','kaper','planing'):
                catalog[method]=read(native.command('performance_defaults',output=folder/'discovery'/method,calculator=method))
            if contracts:checks.service_contract(r,runtime/'FreeShipDocumentService.exe',catalog,out/'contracts',SOLVERS)
    return catalog

def fixture_solver(out):
    out.mkdir(parents=True);compiler=Path(os.environ['LOCALAPPDATA'])/'FreeShip26QtTools/mingw1310_64/bin/g++.exe';source=q.ROOT/'qt/propulsion_solver_fixture.cpp';exe=out/'CALCPROP.EXE'
    env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH']
    p=subprocess.run([str(compiler),'-std=c++17','-O2','-static',str(source),'-o',str(exe)],env=env,capture_output=True,timeout=90,creationflags=q.common.FLAGS)
    (out/'compile.log').write_bytes(p.stdout+p.stderr)
    if p.returncode:raise RuntimeError('Protocol fixture compile failed')
    shutil.copy2(exe,out/'FPPCALC.EXE');return out

def protocols(r,runtime,catalog,out):
    out.mkdir(parents=True);solvers=fixture_solver(out/'solvers');identity=str(uuid.uuid4())
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native-process') as native,q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle-process') as oracle:
        for name,s in [('native',native),('oracle',oracle)]:
            q.require_ok(s.command('load',output=out/name/'load',path=str(q.DEFAULT_MODELS[0]),document_id=identity))
            # This produces a real, revision-scoped resistance cache for Passport.
            read(s.command('performance',output=out/name/'resistance',calculator='holtrop',values=samples.sample('holtrop',catalog)))
        before=dict(native.state)
        for method in METHODS[:3]:
            args=dict(calculator=method,values=samples.sample(method,catalog),solver_directory=str(solvers))
            a=read(native.command('performance',output=out/'native'/method,**args));old=oracle.command('performance',output=out/'oracle'/method,**args)
            if method=='propeller3' and not old['ok']:
                r.case('fixture.passport.legacy-parser-error',old.get('error')=='Invalid input',error=old.get('error'))
                r.case('fixture.passport.legacy-postprocessing',status='review',reason='Unmodified old adapter rejects the fixed 165-number protocol with Invalid input; native matrix/linear expectations below are independent.',response=old)
                b=None
            else:b=read(old)
            if b:
                expected=b['series']
                if method=='propeller1':
                    # RES1 column 1 is diameter; the old shared chart title
                    # incorrectly labelled this series as engine power.
                    expected=[dict(s,y_title='螺旋桨直径 (m)') if s['source']=='Series3' else s for s in expected]
                    r.case('fixture.diameter-unit',next(s for s in a['series'] if s['source']=='Series3')['y_title']=='螺旋桨直径 (m)')
                r.case('fixture.'+method+'.curves',a['series']==expected,actual=a['series'],expected=expected)
            # Domain tables may now have labels where the old bridge left blanks.
            if b:compare_schema(r,'fixture.'+method+'.parameters',a['parameters'],b['parameters'])
            path='TMP'+method[-1]+'.tsk'
            aa=list(map(float,(out/'native'/method/'solver-files'/path).read_text().split()));bb=list(map(float,(out/'oracle'/method/'solver-files'/path).read_text().split()))
            if method=='propeller3':
                r.case('passport.actual-area-ratio',abs(aa[1]-.6)<1e-7 and abs(bb[1]-.98)<1e-7,actual=aa[1],legacy=bb[1]);aa[1]=bb[1]
            r.case('fixture.'+method+'.input-values',all(abs(x-y)<=max(1e-7,abs(y)*1e-8) for x,y in zip(aa,bb)) and len(aa)==len(bb),actual=aa,expected=bb)
            r.case('fixture.'+method+'.readonly',native.state==before)
            if method=='propeller3':
                curves=checks.curves(a)
                for g in range(5):
                    thrust=[[6+2*i+g,5*(100+10*i+5*g)] for i in range(1,6)]
                    power=[[6+2*i+g,-(100+200*i+150*g)] for i in range(1,6)]
                    r.case(f'passport.matrix.speed-{g}',curves[f'Series{2*g+1}']==thrust and curves[f'Series{2*g+2}']==power)
                for i in range(1,6):
                    thrust=[[6+2*i+g,5*(100+10*i+5*g)] for g in range(5)]
                    power=[[6+2*i+g,-(100+200*i+150*g)] for g in range(5)]
                    r.case(f'passport.matrix.advance-{i}',curves[f'Series_{2*i-1}']==thrust and curves[f'Series_{2*i}']==power)
                # All five fixture curves are straight lines. The independently
                # derived P-to-speed law is V=.01P+5-.5g, T=70+5V.
                expected=[]
                for g in range(4):
                    p=600*((150+25*g)/225)**2;v=.01*p+5-.5*g;expected.append([v,-p])
                expected += [[17,-1350],[19,-1550]]
                def close(x,y):return len(x)==len(y) and all(abs(a-b)<=3e-5*max(1,abs(b)) for p,q in zip(x,y) for a,b in zip(p,q))
                r.case('passport.engine-independent-line',close(curves['Series_11'],expected))
                r.case('passport.thrust-independent-line',close(curves['Series_12'],[[v,350+25*v] for v,_ in expected]))
        # Use the native Task2 published data in Task3; it must be a whole valid
        # generation, and survive no-op reads without requiring the old adapter.
        extract=read(native.command('performance_defaults',output=out/'native'/'passport-extract',calculator='propeller3',extract=True));values=checks.values(extract)
        r.case('fixture.match-to-passport',values['Edit2']==3 and abs(values['Edit4']-1.1)<1e-6 and values['Edit17']==1000)
        for mode in ('normal','weak'):
            (solvers/'CONF.PRO').write_text(mode+'\n','ascii')
            strength=read(native.command('performance',output=out/'native'/mode,calculator='propeller2',values=samples.sample('propeller2',catalog),solver_directory=str(solvers)))
            # Evaluate the displayed factors independently of the stress formula.
            row=next(line for line in strength['text'].splitlines() if '|SigmaB/4|' in line)
            factors=[float(s.strip()) for s in row.split('|')[4:] if s.strip()]
            r.case(f'strength.{mode}.has-eight-factors',len(factors)==8)
            for i,factor in enumerate(factors):
                verdict=strength['text'].split(f'材料 {i+1}\r\n',1)[1].split('\r\n')[1]
                r.case(f'strength.{mode}.material-{i+1}.literal-threshold',('> 1' in verdict)==(factor>1) and ('强度不足' in verdict)==(factor<=1),factor=factor,verdict=verdict)
        for mode in ('missing','exit','truncated','nonfinite','after-summary'):
            (solvers/'CONF.PRO').write_text(mode+'\n',encoding='ascii');folder=out/'native'/mode
            reply=native.command('performance',output=folder,calculator='propeller5',values=samples.sample('propeller5',catalog),solver_directory=str(solvers))
            r.case('solver.'+mode+'.reject',not reply['ok'],error=reply.get('error'));r.case('solver.'+mode+'.no-report',not (folder/'performance.json').exists());r.case('solver.'+mode+'.state',native.state==before)
        folder=out/'native'/'cancel-running';folder.mkdir();marker=out/'started.txt';(solvers/'CONF.PRO').write_text('wait\n'+str(marker)+'\n','ascii')
        def cancel():
            deadline=time.monotonic()+8
            while not marker.exists() and time.monotonic()<deadline:time.sleep(.02)
            (folder/'cancel.flag').write_text('cancel','ascii')
        worker=threading.Thread(target=cancel);worker.start();start=time.perf_counter()
        reply=native.command('performance',output=folder,calculator='propeller5',values=samples.sample('propeller5',catalog),solver_directory=str(solvers));worker.join();elapsed=time.perf_counter()-start
        r.case('solver.cancel-running',not reply['ok'] and '取消' in reply.get('error','') and elapsed<5,elapsed=elapsed,error=reply.get('error'))
        if marker.exists():
            import ctypes
            api=ctypes.WinDLL('kernel32',use_last_error=True);api.OpenProcess.restype=ctypes.c_void_p;api.OpenProcess.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32];api.CloseHandle.argtypes=[ctypes.c_void_p];api.GetExitCodeProcess.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_ulong)]
            handle=api.OpenProcess(0x1000,False,int(marker.read_text()));code=ctypes.c_ulong(259)
            if handle:api.GetExitCodeProcess(handle,ctypes.byref(code));api.CloseHandle(handle)
            r.case('solver.cancel-child-exited',not handle or code.value!=259)
        # Repeated request directories must not make a stale solver file usable.
        (solvers/'CONF.PRO').write_text('missing\n','ascii');reply=native.command('performance',output=out/'native'/'propeller1',calculator='propeller1',values=samples.sample('propeller1',catalog),solver_directory=str(solvers))
        r.case('solver.stale-result-rejected',not reply['ok'] and 'Missing calculation output:' in reply.get('error',''))
        # No Pascal process is needed from bootstrap through solver invocation.
        snap=q.require_ok(native.command('snapshot',output=out/'native'/'snapshot'))
        import qt_document
        r.case('detached.no-pascal-bootstrap',snap.get('adapter_pid')==0 and snap.get('adapter_requests')==0)
        for method in METHODS:
            response=native.command('performance_defaults',output=out/'native'/('detached-'+method),calculator=method)
            r.case('detached.'+method,response['ok'] and response.get('analysis_engine')=='cpp-propulsion')
        response=native.command('performance',output=out/'native'/'detached-real',calculator='propeller5',values=samples.sample('propeller5',catalog),solver_directory=str(SOLVERS))
        r.case('detached.real-solver',response['ok'] and response.get('analysis_engine')=='cpp-propulsion')
        q.require_ok(native.command('update_project',output=out/'native'/'change',changes={'name':'new revision'}))
        response=native.command('performance_defaults',output=out/'native'/'stale-cache',calculator='propeller3',extract=True)
        r.case('extract.old-revision-rejected',not response['ok'])
        for method in METHODS:
            response=oracle.command('performance_defaults',output=out/'oracle'/('forbidden-'+method),calculator=method,external_document=True)
            r.case('forbid-old.'+method,not response['ok'] and 'must execute in C++' in response.get('error',''))
    # A forced shutdown must also terminate the already-running solver, not
    # merely release the Python client's pipes. Hold a handle to this exact child.
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'forced-process',timeout=10) as service:
        q.require_ok(service.command('load',output=out/'forced-load',path=str(q.DEFAULT_MODELS[0]),document_id=str(uuid.uuid4())))
        marker=out/'forced-child.txt';(solvers/'CONF.PRO').write_text('wait\n'+str(marker)+'\n','ascii');result=[]
        def calculate():
            try:result.append(service.command('performance',output=out/'forced-calculate',calculator='propeller5',values=samples.sample('propeller5',catalog),solver_directory=str(solvers)))
            except Exception as ex:result.append(type(ex).__name__)
        worker=threading.Thread(target=calculate);worker.start();deadline=time.monotonic()+8
        while not marker.exists() and time.monotonic()<deadline:time.sleep(.02)
        r.case('solver.forced-parent.started',marker.exists())
        handle=api.OpenProcess(0x1000|1,False,int(marker.read_text())) if marker.exists() else None
        try:
            time.sleep(.1);service.process.kill();service.process.wait(timeout=3);worker.join(timeout=3)
            deadline=time.monotonic()+3;code=ctypes.c_ulong(259)
            while handle and time.monotonic()<deadline:
                api.GetExitCodeProcess(handle,ctypes.byref(code))
                if code.value!=259:break
                time.sleep(.02)
            r.case('solver.forced-parent.child-exited',bool(handle) and code.value!=259)
            r.case('solver.forced-parent.client-ended',not worker.is_alive() and result==['EOFError'],result=result)
        finally:
            if handle:
                if code.value==259:
                    api.TerminateProcess.argtypes=[ctypes.c_void_p,ctypes.c_uint32];api.TerminateProcess(handle,1)
                api.CloseHandle(handle)

def run(r,runtime,models,out,quick=False,contracts=True):
    generated,origin=metadata.generate()
    r.case('propulsion.table-provenance',(q.QT/'src/document/propulsion_tables.h').read_text('utf8')==generated and json.loads((q.QT/'src/document/propulsion-origin.json').read_text('utf8'))==origin)
    r.case('propulsion.report-provenance',(q.QT/'src/document/propulsion_reports.inc').read_text('utf8')==reports.generate())
    catalog=parity(r,runtime,models,out/'parity',quick,contracts)
    if not quick:protocols(r,runtime,catalog,out/'protocols')
    return catalog

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--quick',action='store_true');p.add_argument('--protocols-only',action='store_true');a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data['suite']='qt-native-propulsion';start=time.perf_counter()
    try:
        if a.protocols_only:
            with q.Service(a.runtime.resolve()/'FreeShipDocumentService.exe',out/'discovery-process') as service:
                q.require_ok(service.command('load',output=out/'load',path=str(q.DEFAULT_MODELS[0]),document_id=str(uuid.uuid4())))
                catalog={method:read(service.command('performance_defaults',output=out/'discovery'/method,calculator=method)) for method in (*METHODS,'holtrop')}
            protocols(r,a.runtime.resolve(),catalog,out/'protocols')
        else:run(r,a.runtime.resolve(),q.DEFAULT_MODELS[:1] if a.quick else q.DEFAULT_MODELS,out,a.quick)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:r.data['elapsed_seconds']=time.perf_counter()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],seconds=r.data['elapsed_seconds'],report=str(out/'report.json'))));return 1 if r.data['counts']['fail']+r.data['counts']['error'] else 0
if __name__=='__main__':raise SystemExit(main())
