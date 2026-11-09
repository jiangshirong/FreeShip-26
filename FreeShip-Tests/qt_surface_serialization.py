"""Exact old/new surface wire-format comparison and paired uncached timings.

Uses a frozen pre-change runtime as oracle; never updates expected results from
the new implementation. Missing oracle is reported separately, not as a pass.
"""
import argparse
import copy
import json
import statistics
from pathlib import Path
import qt_lines_cache as c
import qt_native_project as project
q,e=c.q,c.e
FILES=('qt_surface_serialization.py','qt_lines_cache.py','qt_native_project.py','qt_migration.py','qt_editing.py','run_tests.py')

def run(r,runtime,oracle,out):
    samples=[]
    models=list(q.DEFAULT_MODELS);fixture=out/'curved-box.fbm';project.fixture(fixture,rich=True);models.append(fixture)
    for i,model in enumerate(models):
        with q.Service(runtime/'FreeShipDocumentService.exe',out/f'input-{i}/process',native_linesplan=True) as service:
            reply=q.require_ok(service.command('load',path=str(model),document_id=str(q.uuid.uuid4()),lines_cache_root='',output=out/f'input-{i}/load'))
            value=c.read(Path(reply['model']).parent/'native-input.json')
            samples.append((f'model-{i}',value))
            if i==3:
                reply=service.command('auxiliary',operation='create_flow',view=1,source=[2.25,.4],lines_cache_root='',output=out/'flow')
                if reply['ok']:
                    samples.append(('flow',c.read(Path(reply['model']).parent/'native-input.json')))
                    r.case('nonempty-flow-sample',any(f['spline']['points'] for f in c.read(reply['surface'])['flowlines']))
                else:r.case('flow-sample',status='not_run',reason=reply.get('error','No accepted seed'))
    box=samples[0][1];real=samples[2][1]
    for level in (1,2,3,4):
        data=copy.deepcopy(box);data['geometry'].update(precision=level-1,desired_level=level)
        data['surface_display']['show_normals']=True;samples.append((f'box-level-{level}-normals',data))
    data=copy.deepcopy(real);data['surface_display']['show_normals']=True;samples.append(('real-normals',data))
    data=copy.deepcopy(box)
    data['geometry'].update(points=[],edges=[],faces=[],curves=[])
    data['surface_display'].update(faces=[],curves=[],flows=[],layer_faces=[[] for _ in data['geometry']['layers']])
    samples.append(('empty',data))
    # Repeated real-ship runs alternate execution order to reduce startup bias.
    samples.extend((f'real-repeat-{i}',copy.deepcopy(real)) for i in (1,2))
    timings=[]
    for i,(name,value) in enumerate(samples):
        reports={}
        for tag,package in ([('old',oracle),('new',runtime)] if i%2==0 else [('new',runtime),('old',oracle)]):
            reports[tag]=c.worker(r,package,value,out/name/tag,None)
        c.equal(r,name+'.exact',out/name/'old',out/name/'new')
        r.case(name+'.uncached',not reports['old']['cache_hit'] and not reports['new']['cache_hit'])
        timings.append(dict(case=name,old_surface_ms=reports['old']['surface_display_ms'],new_surface_ms=reports['new']['surface_display_ms'],
                            old_total_ms=reports['old']['total_ms'],new_total_ms=reports['new']['total_ms']))
    r.data['paired_timings']=timings
    real_times=[v for v in timings if v['case']=='model-2' or v['case'].startswith('real-repeat')]
    r.data['real_ship_medians']={key:statistics.median(v[key] for v in real_times) for key in ('old_surface_ms','new_surface_ms','old_total_ms','new_total_ms')}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--oracle-runtime',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    runtime=(a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False)
    backup=q.QT/'build/dist-before-surface-serialization-20261105'
    oracle=(a.oracle_runtime or (backup if backup.exists() else q.QT/'dist')).resolve()
    r=q.Report();sources=q.current_sources(q.QT);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    protected={str(m):q.common.digest_file(m) for m in q.DEFAULT_MODELS}
    r.data.update(suite='qt-surface-serialization',runtime=str(runtime),oracle_runtime=str(oracle),qt_sources_sha256=sources,source_sha256=legacy,
        test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES},limitations=['Paired hidden workers are not desktop interaction acceptance.','Timing medians describe this machine and sample, not all models.'])
    try:
        manifest=c.read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if sources!={n.replace('\\','/'):v for n,v in manifest['source_sha256'].items()}:raise RuntimeError('Stale source manifest')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime file mismatch')
        old=c.read(oracle/'build-manifest.json');exe='FreeShipNativeLinesplan.exe'
        old_hash=q.common.digest_file(oracle/exe)
        if old_hash!=old['binaries_sha256'][exe]:raise RuntimeError('Oracle worker mismatch')
        for name in ('Qt6Core.dll','Qt6Gui.dll'):
            if q.common.digest_file(oracle/name)!=old['binaries_sha256'][name]:raise RuntimeError('Oracle Qt dependency mismatch')
        r.data['oracle_worker_sha256']=old_hash
        if old_hash==q.common.digest_file(runtime/exe):
            r.case('independent-oracle',status='not_run',reason='Supply a distinct pre-change runtime with --oracle-runtime.')
        else:run(r,runtime,oracle,out)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==legacy)
        r.case('original-models-protected',all(q.common.digest_file(Path(n))==h for n,h in protected.items()))
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.case('desktop-acceptance',status='not_run',reason='No desktop automation.');e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],medians=r.data.get('real_ship_medians'),report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
