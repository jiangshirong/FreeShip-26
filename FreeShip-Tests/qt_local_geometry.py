"""Exact local subdivision/fill updates, process restarts and damaged-cache recovery.

Uses real worker and document-service binaries. Retains inputs, hashes, timings
and compact reports; deletes only successful comparisons' generated large output
files in this new run. Never writes historical baselines or user caches.
"""
import argparse
import copy
import hashlib
import json
import subprocess
import statistics
from pathlib import Path
import native_subdivision_cases as fixtures
import qt_lines_cache as cache_tests
import qt_migration as q
import qt_editing as e
import test_artifacts

FILES=('qt_local_geometry.py','qt_lines_cache.py','native_subdivision_cases.py',
       'qt/native_subdivision_tests.cpp','qt_migration.py','qt_editing.py','test_artifacts.py','run_tests.py')

def read(path):return json.loads(Path(path).read_text('utf-8-sig'))

def retire(folder,root,names=cache_tests.ARTIFACTS):
    """Only known, freshly generated outputs; no directory traversal or links."""
    folder=Path(folder)
    if root.resolve() not in folder.resolve().parents:raise RuntimeError('Output outside current run')
    for name in names:
        path=folder/name
        if path.is_file() and not path.is_symlink():path.unlink()

def hashes(folder):return {n:q.common.digest_file(folder/n) for n in cache_tests.ARTIFACTS}

def retire_equal(root,*reports):
    if not reports or any(x['artifact_hashes']!=reports[0]['artifact_hashes'] for x in reports):return False
    for report in reports:retire(Path(report['artifact_directory']),root)
    return True

def storage(r,out):
    out.mkdir();token=test_artifacts.claim(out);(out/'report.json').write_text('{"status":"pass"}',encoding='utf8')
    folder=out/'native-lines-cache/sections';folder.mkdir(parents=True)
    (folder/'cuts.fscache').write_bytes(b'test');(folder/'local-geometry.fscache').write_bytes(b'test')
    unknown=folder/'unrecognized-user-file';unknown.write_bytes(b'keep')
    first=test_artifacts.finish(out,token)
    r.case('storage.unknown-content-preserved',first['removed_files']==0 and unknown.read_bytes()==b'keep')
    unknown.unlink();baseline=out/'retained-baseline';baseline.write_bytes(b'baseline')
    before=q.common.digest_file(baseline);second=test_artifacts.finish(out,token)
    r.case('storage.known-caches-retired',second['removed_files']==2 and not folder.exists())
    r.case('storage.reports-and-baselines-retained',q.common.digest_file(baseline)==before and (out/'report.json').exists())
    pair=[]
    for name in ('left','right'):
        directory=out/name;directory.mkdir()
        for artifact in cache_tests.ARTIFACTS:(directory/artifact).write_text(name,encoding='ascii')
        pair.append(dict(artifact_directory=str(directory),artifact_hashes=hashes(directory)))
    r.case('storage.failed-comparison-keeps-evidence',not retire_equal(out,*pair) and all((out/name/'scene.fslp').exists() for name in ('left','right')))
    for artifact in cache_tests.ARTIFACTS:(out/'right'/artifact).write_text('left',encoding='ascii')
    pair[1]['artifact_hashes']=hashes(out/'right')
    r.case('storage.equal-comparison-retires-output',retire_equal(out,*pair) and not (out/'left/scene.fslp').exists() and not (out/'right/scene.fslp').exists())

def kernel(r,runtime,out):
    out.mkdir();request=fixtures.cases();request['incremental_only']=True
    for case in request['cases']:case['levels']=[0,1,2,3,4]
    source=out/'input.json';source.write_text(json.dumps(request),encoding='utf8')
    target=out/'result.json'
    child=subprocess.run([str(runtime/'native_subdivision_tests.exe'),str(source),str(target)],
        cwd=out,env=q.private_environment(out/'env'),capture_output=True,timeout=120,creationflags=q.common.FLAGS)
    result=read(target);r.case('kernel.execution',child.returncode==0 and 'error' not in result)
    for case in result.get('cases',[]):
        r.case('kernel.'+case['id'],case['status']=='ok',reason=case.get('error',''))
        for row in case.get('checks',[]):
            r.case('kernel.'+case['id']+'.'+str(row['level'])+'.'+row['edit'],row['exact'] and row['zero_work'],**row)
    for row in result.get('api_checks',[]):r.case('full-generator.'+row['id'],status=row['status'])

def matrix(r,runtime,value,out,root,baseline=None):
    cache=out/'native-lines-cache'
    def execute(name,data,cached=True,reference=None):
        folder=out/name;report=cache_tests.worker(r,reference or runtime,data,folder,cache if cached else None)
        report['artifact_hashes']=hashes(folder);report['artifact_directory']=str(folder);return report
    cold=execute('cold',value);warm=execute('warm',value)
    r.case('restart.full-cache',not cold['cache_hit'] and warm['cache_hit'] and cold['artifact_hashes']==warm['artifact_hashes'])
    retire_equal(root,cold,warm)
    r.case('cold.local-cache-active',cold.get('local_geometry_cache_active') is True)
    local=copy.deepcopy(value);local['geometry']['points'][0]['position'][2]+=.125
    local['revision']+=1
    variants=[('local-point',local)]
    def variant(name,change):
        data=copy.deepcopy(local);change(data);data['revision']+=len(variants);variants.append((name,data))
    variant('bounds',lambda d:d['geometry']['points'][0]['position'].__setitem__(0,30+d['geometry']['points'][0]['position'][0]))
    variant('batch',lambda d:[p['position'].__setitem__(1,p['position'][1]+.125) for p in d['geometry']['points'][::4]])
    variant('draft',lambda d:d['settings'].__setitem__('draft',d['settings']['draft']+.25))
    variant('split',lambda d:d['settings'].__setitem__('split',d['settings']['split']+.5))
    variant('crease',lambda d:d['geometry']['edges'][0].__setitem__('crease',not d['geometry']['edges'][0]['crease']))
    variant('layer-color',lambda d:d['geometry']['layers'][0].__setitem__('color',d['geometry']['layers'][0]['color']^255))
    variant('layer-symmetry',lambda d:d['geometry']['layers'][0].__setitem__('symmetric',not d['geometry']['layers'][0]['symmetric']))
    variant('precision',lambda d:d['geometry'].update(precision=0,desired_level=1))
    variant('precision-high',lambda d:d['geometry'].update(precision=3,desired_level=4))
    variant('normals',lambda d:d.__setitem__('show_normals',not d.get('show_normals',False)))
    # Compare complete serialized artifacts, including DXF bytes: no tolerance.
    for name,data in [('baseline',value)]+variants:
        cached=execute(name+'-cached',data);full=execute(name+'-full',data,False)
        compared=[cached,full]
        r.case(name+'.all-artifacts-exact',cached['artifact_hashes']==full['artifact_hashes'],cached=cached['artifact_hashes'],full=full['artifact_hashes'])
        if name=='local-point':
            r.case('local-point.partial-subdivision',cached['local_geometry_cache_hit'] and 0<cached['subdivision_evaluated']<cached['subdivision_operations'])
            r.case('local-point.partial-fill',0<cached['fill_roots_reused'] and 0<cached['fill_roots_rebuilt'])
            r.case('local-point.no-redundant-checkpoint-write',cached['local_geometry_save_ms']==0)
            r.data['local_point_comparison']={'cached':cached,'full':full}
        if name=='batch':r.case('batch.refreshes-checkpoint',cached['local_geometry_cache_hit'] and cached['local_geometry_save_ms']>0)
        if baseline and name in ('baseline','local-point','batch','draft','layer-symmetry'):
            old=execute(name+'-previous',data,False,baseline)
            compared.append(old)
            r.case(name+'.previous-installed-exact',old['artifact_hashes']==full['artifact_hashes'],previous=old['artifact_hashes'],current=full['artifact_hashes'])
        retire_equal(root,*compared)
    path=cache/'sections/local-geometry.fscache'
    r.case('local-cache.bounded',path.is_file() and path.stat().st_size<=64*1024**2+40,size=path.stat().st_size if path.exists() else None)
    # Unique text prevents the whole-result cache from hiding this path.
    damaged=copy.deepcopy(local)
    def damage(name,payload):
        path.write_bytes(payload)
        damaged['texts']['baseline']=name
        a=execute(name,damaged);b=execute(name+'-full',damaged,False)
        r.case(name+'.safe-exact-fallback',not a['cache_hit'] and not a['local_geometry_cache_hit'] and a['artifact_hashes']==b['artifact_hashes'])
        retire_equal(root,a,b)
    damage('truncated-local-cache',b'FSGEO002bad')
    saved=path.read_bytes();payload=bytearray(saved[40:])
    payload[32:36]=(0xffffffff).to_bytes(4,'little')
    damage('invalid-count-with-valid-checksum',b'FSGEO002'+hashlib.blake2b(payload,digest_size=32).digest()+payload)
    damaged['texts']['baseline']='incompatible-engine';saved=path.read_bytes();payload=bytearray(saved[40:]);payload[0]^=1
    damage('incompatible-topology-key',b'FSGEO002'+hashlib.blake2b(payload,digest_size=32).digest()+payload)
    saved=path.read_bytes();payload=bytearray(saved[40:]);first_derived=36+28*(len(local['geometry']['points'])+1)
    payload[first_derived+4:first_derived+8]=(0xffffffff).to_bytes(4,'little')
    damage('invalid-dependency-with-valid-checksum',b'FSGEO002'+hashlib.blake2b(payload,digest_size=32).digest()+payload)
    blocked=out/'blocked-cache';blocked.write_text('occupied',encoding='ascii')
    folder=out/'cache-unavailable';a=cache_tests.worker(r,runtime,value,folder,blocked)
    if r.case('cache-unavailable.exact',hashes(folder)==cold['artifact_hashes'] and not a.get('local_geometry_cache_active')):retire(folder,root)
    # Paired measurements, alternating execution order, distinct edits (never
    # whole-result hits). Timing is evidence, not a brittle machine-speed gate.
    samples=[]
    previous_cache=out/'previous/native-lines-cache'
    if baseline:
        folder=out/'previous-warm';previous=cache_tests.worker(r,baseline,value,folder,previous_cache)
        previous.update(artifact_hashes=hashes(folder),artifact_directory=str(folder));retire_equal(root,cold,previous)
    for i in range(6):
        data=copy.deepcopy(value);data['geometry']['points'][0]['position'][2]+=.1875+i*.015625
        data['revision']+=100+i;row={}
        for mode in (('local','full') if i%2==0 else ('full','local')):
            row[mode]=execute('timing-'+str(i)+'-'+mode,data,mode=='local')
        if baseline:
            folder=out/('timing-'+str(i)+'-previous')
            row['previous']=cache_tests.worker(r,baseline,data,folder,previous_cache)
            row['previous'].update(artifact_hashes=hashes(folder),artifact_directory=str(folder))
            r.case('timing-'+str(i)+'.previous-exact',row['previous']['artifact_hashes']==row['local']['artifact_hashes'])
        r.case('timing-'+str(i)+'.exact',row['local']['artifact_hashes']==row['full']['artifact_hashes'] and row['local']['local_geometry_cache_hit'])
        retire_equal(root,*row.values())
        samples.append(row)
    fields=('total_ms','subdivision_ms','local_geometry_compute_ms','local_geometry_load_ms','local_geometry_save_ms','fill_prepare_ms','fill_sort_project_ms')
    r.data['benchmark']={mode:{f:statistics.median(row[mode][f] for row in samples) for f in fields} for mode in ('local','full')}
    if baseline:r.data['benchmark']['previous']={f:statistics.median(row['previous'][f] for row in samples) for f in ('total_ms','subdivision_ms','generation_ms')}
    r.data['benchmark_samples']=samples

def run(r,runtime,out):
    storage(r,out/'storage')
    kernel(r,runtime,out/'kernel')
    model=q.DEFAULT_MODELS[-1];cache=out/'session/native-lines-cache'
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'session/process',native_linesplan=True) as service:
        def command(method,name,**fields):return q.require_ok(service.command(method,output=out/'session'/name,lines_cache_root=str(cache),**fields))
        original=command('load','load',path=str(model),document_id=str(q.uuid.uuid4()))
        value=read(Path(original['model']).parent/'native-input.json');doc=read(original['model'])
        point=next(p for p in doc['control_points'] if p['batch_movable']);xyz=list(point['p']);xyz[2]+=.125
        moved=command('move_point','move',point_id=point['id'],position=xyz)
        local=read(moved['native_report']);r.data['service_edit']=local
        r.case('service.real-edit-local-update',local['local_geometry_cache_hit'] and local['fill_roots_reused']>0 and 0<local['subdivision_evaluated']<local['subdivision_operations'])
        full=q.require_ok(service.command('snapshot',output=out/'session/full',lines_cache_root=''))
        service_exact=r.case('service.real-edit-exact',hashes(Path(moved['model']).parent)==hashes(Path(full['model']).parent))
        undo=command('undo','undo');redo=command('redo','redo')
        service_exact=r.case('service.undo-exact',hashes(Path(original['model']).parent)==hashes(Path(undo['model']).parent)) and service_exact
        service_exact=r.case('service.redo-exact',hashes(Path(moved['model']).parent)==hashes(Path(redo['model']).parent)) and service_exact
    if service_exact:
        for name in ('load','move','full','undo','redo'):retire(out/'session'/name,out)
    baseline=q.QT/'dist';old=baseline/'FreeShipNativeLinesplan.exe'
    if old.exists() and q.common.digest_file(old)!=q.common.digest_file(runtime/old.name):
        r.data['previous_installed_worker_sha256']=q.common.digest_file(old)
    else:baseline=None
    matrix(r,runtime,value,out/'matrix',out,baseline)
    cache_tests.application(r,runtime,model,out/'application')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for protected in (q.ROOT,q.QT,q.PILOT,q.common.DEFAULT_SOURCE):
        if out==protected or protected in out.parents:p.error('Use a new isolated output directory')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();sources=q.current_sources(q.QT);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    models={str(m):q.common.digest_file(m) for m in q.DEFAULT_MODELS}
    test_sources={n:q.common.digest_file(q.ROOT/n) for n in FILES}
    r.data.update(suite='qt-local-geometry',runtime=str(runtime),qt_sources_sha256=sources,test_sources_sha256=test_sources,limitations=[
        'Exact local subdivision and root-face clipping/lighting. Global depth sorting, layout and final serialization remain complete.',
        'Hidden production processes do not replace desktop visual or interaction acceptance.',
        'Successful large output files are retired after retaining hashes. Inputs and diagnostic reports remain reproducible.'])
    try:
        manifest=read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if sources!={n.replace('\\','/'):v for n,v in manifest['source_sha256'].items()}:raise RuntimeError('Stale source manifest')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime hash mismatch')
        run(r,runtime,out)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==legacy)
        r.case('original-models-protected',all(q.common.digest_file(Path(n))==h for n,h in models.items()))
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in test_sources.items()))
        r.case('desktop-acceptance',status='not_run',reason='No desktop automation.');e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
