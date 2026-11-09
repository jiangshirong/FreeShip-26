"""Calculation pages: production Qt events, input retention and result regressions.

Layout observations are separate from original-LFM page comparisons. Screenshots
are hidden windows; neither this suite nor its probes claim desktop acceptance.
"""
import argparse,json,shutil,time
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_performance as performance
import qt_performance_checks as pc
import qt_performance_cases as samples
import qt_hydrodynamics as hydro
import qt_added_mass as mass
import qt_native_propulsion as propulsion
from maintenance import generate_calculation_pages as metadata

METHODS=performance.METHODS+hydro.METHODS+('addmass',)
FILES=('qt_calculation_layout.py','maintenance/generate_calculation_pages.py',
       'qt_migration.py','qt_editing.py','qt_performance.py','qt_performance_checks.py',
       'qt_performance_cases.py','qt_hydrodynamics.py','qt_added_mass.py','run_tests.py',
       'qt_native_propulsion.py','solver_launch_probe.py','qt/propulsion_solver_fixture.cpp','qt/hydrodynamics_solver_fixture.cpp')
def read(path):return json.loads(Path(path).read_text('utf8'))

def production(r,runtime,out,methods=METHODS,images=True):
    out.mkdir();model=out/'input.fbm';shutil.copy2(q.DEFAULT_MODELS[0],model)
    catalog={};mapping,hashes=metadata.generate()
    r.case('original-form-provenance',read(q.QT/'src/app/calculation_pages-origin.json')==hashes)
    generated=(q.QT/'src/app/calculation_pages.h').read_text('utf8').split('R"pages(',1)[1].split(')pages"',1)[0]
    r.case('generated-page-map',json.loads(generated)==mapping)
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'service',native_linesplan=True) as service:
        q.require_ok(service.command('load',output=out/'loaded',path=str(model),document_id=str(q.uuid.uuid4())))
        for method in methods:
            schema=pc.read(service.command('performance_defaults',output=out/('schema-'+method),calculator=method));catalog[method]=schema
            folder=out/method;folder.mkdir();request=folder/'request.json'
            request.write_text(json.dumps(dict(model=str(model),inputs=dict(calculator=method,solver_directory=str(performance.SOLVERS)),
                describe_only=True,inspect_layout=True,layout_images=images and method in ('holtrop','propeller4','maneuver','addmass'))),encoding='utf8')
            child=q.run_child(r,'layout.'+method,runtime/'FreeShipQt.exe',['--verify-performance',request],folder/'qt',platform='windows',timeout=120)
            r.data.setdefault('layout',{})[method]=child
            art=child.get('artifacts',{});observations=art.get('layout_observations',[])
            r.case(method+'.layout-observed',len(observations)==6)
            expected={f['key'] for f in schema['fields']}
            for obs in observations:
                name=f"{method}.font{obs['font']}.{'compact' if obs['compact'] else 'normal'}"
                fields=obs['fields'];visible=[f for f in fields if f['declared_visible']]
                r.case(name+'.fields-once',len(fields)==len(expected) and {f['key'] for f in fields}==expected)
                r.case(name+'.inputs-retained',obs['inputs_unchanged'])
                bad=[f for f in visible if not f.get('reachable') or not f.get('label_fits')]
                r.case(name+'.accessible-fields',not bad,unreachable=bad)
                r.case(name+'.actions-contained',all(a['contained'] for a in obs['actions']),actions=obs['actions'])
                r.case(name+'.fits-window',obs['actual_size']==obs['requested_size'],actual=obs['actual_size'],requested=obs['requested_size'])
                differences=[f for f in fields if f['page']!=mapping.get(method,{}).get(f['key'],'general')]
                r.case(name+'.original-page-observation',status='review' if differences else 'pass',differences=differences,
                       basis='Preserved original LFM General/Added data ownership; not a complete visual or desktop verdict.')
            r.case(method+'.readonly-input',model.read_bytes()==q.DEFAULT_MODELS[0].read_bytes())
        # Extraction is a real callback. Compare its result with the same public
        # service operation, including all off-page fields, not rounded widgets.
        if 'holtrop' in methods:
            folder=out/'extract';request=pc.production_request(model,folder,'holtrop',catalog,performance.SOLVERS,describe_only=True,extract_before=True)
            child=q.run_child(r,'extract',runtime/'FreeShipQt.exe',['--verify-performance',request],folder/'qt',platform='windows');r.data['extract']=child
            art=child.get('artifacts',{});before=art['before_extract']
            expected=pc.read(service.command('performance_defaults',output=folder/'expected',extract=True,**before))
            r.case('extract.all-pages-retained',art['effective_inputs']['values']==pc.values(expected))
    return catalog,model

def calculations(r,runtime,out,catalog,model,smoke=False):
    out.mkdir()
    blocked=performance.service_samples(r,runtime/'FreeShipDocumentService.exe',catalog,out/'service') if not smoke else set()
    pc.production(r,runtime,{m:catalog[m] for m in (('delft','holtrop','propeller5') if smoke else performance.METHODS)},out/'performance',performance.SOLVERS,blocked)
    folder=out/'additional-preset';request=pc.production_request(model,folder,'holtrop',catalog,performance.SOLVERS,preset_roundtrip=True,start_page='additional')
    r.data['additional_preset']=q.run_child(r,'additional-preset',runtime/'FreeShipQt.exe',['--verify-performance',request],folder/'qt',platform='windows')
    if smoke:return
    if blocked:
        fixture=propulsion.fixture_solver(out/'propulsion-fixture')
        for method in sorted(blocked):
            flags={'prepare':dict(calculator='holtrop',values=samples.sample('holtrop',catalog))} if method=='propeller3' else {}
            folder=out/('fixture-'+method);request=pc.production_request(model,folder,method,catalog,fixture,**flags)
            r.data.setdefault('fixture_production',{})[method]=q.run_child(r,'fixture.'+method,runtime/'FreeShipQt.exe',['--verify-performance',request],folder/'qt',platform='windows')
        r.case('fixture-result-limit',status='review',reason='CALCPROP protocol fixture covers Qt result/table callbacks only; it does not supply validated engineering results.')
    fixture=hydro.fixture(out/'hydro-fixture')
    hydro.production(r,runtime,out/'hydro',catalog,samples.sample('holtrop',catalog),fixture)
    # Use the retained independent ellipsoid fixture for matrix/preview tests.
    shape=mass.model_fixture(runtime,out/'ellipsoid');folder=out/'addmass-model';folder.mkdir()
    with q.Service(runtime/'FreeShipDocumentService.exe',folder/'service',native_linesplan=True) as service:
        q.require_ok(service.command('import_model',output=folder/'load',path=str(shape),document_id=str(q.uuid.uuid4())))
        saved=Path(q.require_ok(service.command('save',output=folder/'save'))['saved_file'])
        values=pc.values(pc.read(service.command('performance_defaults',output=folder/'schema',calculator='addmass')))
        values['panels']=400
    mass.production(r,runtime,out/'addmass',saved,values,None)
    request=out/'preview.json';request.write_text(json.dumps(dict(model=str(saved),inputs=dict(calculator='addmass',values=values),preview=True,describe_only=True)),encoding='utf8')
    r.data['preview']=q.run_child(r,'preview',runtime/'FreeShipQt.exe',['--verify-performance',request],out/'preview',platform='windows')
    for method in ('propeller4','maneuver'):
        for cancel in (False,True):
            folder=out/f'temperature-{method}-{cancel}';folder.mkdir();request=folder/'request.json'
            request.write_text(json.dumps(dict(model=str(model),inputs=dict(calculator=method),describe_only=True,water_temperature=30,cancel_temperature=cancel)),encoding='utf8')
            child=q.run_child(r,f'temperature.{method}.{cancel}',runtime/'FreeShipQt.exe',['--verify-performance',request],folder/'qt',platform='windows')
            if not cancel:r.case(method+'.temperature-after-repage',abs(child['artifacts']['effective_inputs']['values']['Edit11' if method=='maneuver' else 'WaterViscosity']-.817)<1e-6)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--layout-only',action='store_true');a=p.parse_args()
    runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.monotonic();sources=q.current_sources(q.QT)
    protected=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0];originals={str(p):q.common.digest_file(p) for p in q.DEFAULT_MODELS}
    r.data.update(suite='qt-calculation-layout',runtime=str(runtime),qt_sources_sha256=sources,source_sha256=protected,
                  test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES})
    try:
        manifest=read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if {n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime mismatch')
        r.case('runtime-manifest',True)
        methods=('delft','holtrop','propeller5','addmass') if a.verify_deployment else METHODS
        catalog,model=production(r,runtime,out/'layout',methods,images=not bool(a.verify_deployment))
        if not a.layout_only:calculations(r,runtime,out/'calculations',catalog,model,smoke=bool(a.verify_deployment))
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==protected)
        r.case('models-protected',all(q.common.digest_file(Path(p))==h for p,h in originals.items()))
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.case('desktop-acceptance',status='not_run',reason='Hidden production events/screenshots; physical mouse, focus and system dialogs are not tested.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
