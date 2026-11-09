"""Resistance/propulsion service and real Qt callbacks. Private models only.

Source algorithms are retained, not certified for naval design by this suite.
All diagnostic modes are kept here; native desktop acceptance is not performed.
"""
import argparse,json,math,csv,io,re,time
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_performance_cases as samples
import qt_performance_checks as checks
import qt_native_propulsion
import qt_native_resistance

METHODS=('delft','holtrop','hollenbach','oortmerssen','fung','ost','rbhs','mh','kaper','planing',
         'propeller1','propeller2','propeller3','propeller4','propeller5')
SOLVERS=q.ROOT/'fixtures/solvers35/unpacked/Freeship/Exec'

def payload(s,method,out,**values):
    response=s.command(method,output=out,**values)
    q.require_ok(response)
    return json.loads(Path(response['performance']).read_text('utf8'))

def discovery(r,exe,out):
    out.mkdir(parents=True,exist_ok=True)
    all={}
    with q.Service(exe,out/'process') as s:
        q.require_ok(s.command('load',output=out/'loaded',path=str(q.DEFAULT_MODELS[0]),document_id=str(q.uuid.uuid4())))
        initial=dict(s.state)
        for method in METHODS:
            try:
                info=payload(s,'performance_defaults',out/method,calculator=method)
                all[method]=info
                r.case(method+'.defaults',bool(info['fields']))
                r.case(method+'.unique-fields',len({f['key'] for f in info['fields']})==len(info['fields']))
                unresolved=[f['key'] for f in info['fields'] if f['label']==f['key'] or re.fullmatch(r'[\d.+-]+',f['label'])]
                r.case(method+'.labels',not unresolved,unresolved=unresolved)
            except Exception as ex:r.case(method+'.defaults',False,reason=str(ex))
            r.case(method+'.readonly',s.state==initial)
        (out/'catalog.json').write_text(json.dumps(all,ensure_ascii=False,indent=2),encoding='utf8')
    return all

def service_samples(r,exe,catalog,out):
    out.mkdir(parents=True,exist_ok=True)
    blocked=set()
    with q.Service(exe,out/'process') as s:
        q.require_ok(s.command('load',output=out/'loaded',path=str(q.DEFAULT_MODELS[0]),document_id=str(q.uuid.uuid4())))
        before=dict(s.state)
        for method in METHODS:
            response=s.command('performance',output=out/method,calculator=method,values=samples.sample(method,catalog),solver_directory=str(SOLVERS))
            if not response['ok'] and checks.dependency_failure(r,method,response,out/method,SOLVERS):blocked.add(method)
            else:r.case(method+'.sample',response['ok'],reason=response.get('error',''))
            if response['ok']:
                data=json.loads(Path(response['performance']).read_text('utf8'))
                r.case(method+'.finite',all(math.isfinite(x) for curve in data['series'] for xy in curve['points'] for x in xy))
                r.case(method+'.curve-count',len(data['series'])>0)
            r.case(method+'.sample-readonly',s.state==before)
    return blocked

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT)
    p.add_argument('--service-executable',type=Path);p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path)
    p.add_argument('--oracle',type=Path);p.add_argument('--samples-only',action='store_true');p.add_argument('--branches-only',action='store_true')
    p.add_argument('--verify-deployment',type=Path);p.add_argument('--workflow-reference',type=Path)
    p.add_argument('--discover-only',action='store_true');a=p.parse_args();a.source=a.source.resolve();a.qt_source=a.qt_source.resolve();out=a.output.resolve()
    if a.verify_deployment:
        if not a.workflow_reference:p.error('--workflow-reference is required')
        # Reuse exactly the parameters actually exercised by the retained Qt run.
        def request(model,folder):
            reference=json.loads((a.workflow_reference/'report.json').read_text('utf8'))
            art=reference['models'][0]['production']['artifacts'];path=folder/'request.json'
            path.write_text(json.dumps({'model':str(model),'inputs':art['effective_inputs']}),encoding='utf8');return path
        return q.verify_deployment(a,[q.DEFAULT_MODELS[0]],out,'--verify-performance',('original','after'),('scene','dxf'),workflow_prepare=request)
    for root in (a.source,a.qt_source,q.PILOT):
        if out==root or root in out.parents:p.error('Use an isolated report directory')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data['suite']='qt-performance';started=time.perf_counter()
    before=q.common.fingerprint(a.source)[0];r.data['source_sha256']=before;r.data['qt_sources_sha256']=q.current_sources(a.qt_source)
    tests=('qt_performance.py','qt_performance_cases.py','qt_performance_checks.py','solver_launch_probe.py','qt_performance_oracle.py','PerformanceOracle.pas','PerformanceObserver.pas','qt_migration.py','qt_editing.py','run_tests.py')
    tests+=('qt_native_propulsion.py','qt/propulsion_solver_fixture.cpp','maintenance/generate_propulsion_tables.py','maintenance/port_propulsion_reports.py','fixtures/propulsion/schema.json')
    tests+=('qt_native_resistance.py','qt/resistance_solver_fixture.cpp','qt/resistance_math.pas','maintenance/port_resistance.py','maintenance/resistance_math.py','maintenance/probe_resistance_repeat.py','fixtures/resistance/schema.json')
    r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in tests}
    protected={str(path):q.common.digest_file(path) for path in [*q.DEFAULT_MODELS,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe'] if path.exists()}
    try:
        if a.service_executable:exe=a.service_executable.resolve();r.case('production-callbacks',status='not_run',reason='Service diagnostic only')
        else:
            bridge=q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),r);runtime=q.prepare_runtime(a,out,bridge,r);exe=runtime/'FreeShipDocumentService.exe'
            r.data['runtime_path']=str(runtime)
        catalog=discovery(r,exe,out/'discovery')
        if a.branches_only:checks.branches(r,exe,catalog,out/'branches',SOLVERS)
        elif not a.discover_only:
            blocked=service_samples(r,exe,catalog,out/'samples')
            if not a.samples_only:
                checks.service_contract(r,exe,catalog,out/'contracts',SOLVERS)
                checks.raw_outputs(r,out/'samples',out/'direct-solvers',SOLVERS)
                checks.branches(r,exe,catalog,out/'branches',SOLVERS)
                if not a.service_executable:qt_native_propulsion.run(r,runtime,q.DEFAULT_MODELS,out/'native-propulsion',contracts=False)
                if not a.service_executable:qt_native_resistance.run(r,runtime,q.DEFAULT_MODELS,out/'native-resistance')
                if not a.oracle and not a.service_executable:
                    import qt_performance_oracle
                    built=qt_performance_oracle.build(a.source,out/'original-build')
                    if built['status']!='pass':raise RuntimeError('Original observer build failed')
                    a.oracle=out/'original-build/PerformanceOracle.exe'
                if a.oracle:checks.original_oracle(r,exe,a.oracle.resolve(),catalog,out/'oracle',SOLVERS)
                else:r.case('original-oracle',status='not_run',reason='Use --oracle with qt_performance_oracle.py output')
                if not a.service_executable:checks.production(r,runtime,catalog,out/'production',SOLVERS,blocked)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.data['elapsed_seconds']=time.perf_counter()-started
        r.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==before)
        r.case('qt-source-unchanged',q.current_sources(a.qt_source)==r.data['qt_sources_sha256'])
        r.case('test-source-unchanged',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        for path,digest in protected.items():r.case('protected.'+path,q.common.digest_file(Path(path))==digest)
        r.case('native-desktop',status='not_run',reason='Hidden Qt and calculator service only');e.write_report(r,out)
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'report':str(out/'report.json')}))
    return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]
if __name__=='__main__':raise SystemExit(main())
