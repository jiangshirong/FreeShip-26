"""Application restart cache and local cut reuse against uncached full generation.

All mutations and damaged caches live under the explicit isolated output root.
No model file, user preference or user cache is changed.
"""
import argparse
import copy
import json
import subprocess
import time
from pathlib import Path
import qt_migration as q
import qt_editing as e

FILES=('qt_lines_cache.py','qt_migration.py','qt_editing.py','run_tests.py')
ARTIFACTS=('scene.fslp','reference.dxf','sections.json','surface.json')

def read(path):return json.loads(Path(path).read_text('utf8'))

def worker(r,runtime,value,out,cache):
    out.mkdir(parents=True);request=out/'input.json'
    request.write_text(json.dumps(value,ensure_ascii=False),encoding='utf8')
    start=time.perf_counter()
    with (out/'stdout.log').open('wb') as stdout,(out/'stderr.log').open('wb') as stderr:
        child=subprocess.run([str(runtime/'FreeShipNativeLinesplan.exe'),str(request),str(out),str(cache) if cache else ''],
            cwd=out,env=q.private_environment(out/'env'),stdout=stdout,stderr=stderr,timeout=120,creationflags=q.common.FLAGS)
    report=read(out/'native-report.json');report['wall_ms']=(time.perf_counter()-start)*1000
    if child.returncode or report['status']!='pass':raise RuntimeError(str(report))
    r.data.setdefault('timings',[]).append(dict(case=out.name,**report))
    return report

def equal(r,tag,a,b):
    for name in ARTIFACTS:r.compare(tag+'.'+name,a/name,b/name)

def matrix(r,runtime,value,out):
    cache=out/'native-lines-cache'
    cold=worker(r,runtime,value,out/'cold',cache)
    warm=worker(r,runtime,value,out/'warm',cache)
    r.case('worker.restart-hit',not cold['cache_hit'] and warm['cache_hit'])
    equal(r,'worker.restart-exact',out/'cold',out/'warm')
    variants=[]
    def variant(name,change):
        data=copy.deepcopy(value);change(data);variants.append((name,data))
    variant('local-point',lambda d:d['geometry']['points'][0]['position'].__setitem__(2,d['geometry']['points'][0]['position'][2]+.125))
    variant('point-crosses-bounds',lambda d:d['geometry']['points'][0]['position'].__setitem__(0,d['geometry']['points'][0]['position'][0]+30))
    variant('crease',lambda d:d['geometry']['edges'][0].__setitem__('crease',not d['geometry']['edges'][0]['crease']))
    variant('layer-intersections',lambda d:d['geometry']['layers'][0].__setitem__('use_for_intersections',False))
    variant('layer-symmetry',lambda d:d['geometry']['layers'][0].__setitem__('symmetric',not d['geometry']['layers'][0]['symmetric']))
    variant('draft',lambda d:d['settings'].__setitem__('draft',d['settings']['draft']+.25))
    variant('split',lambda d:d['settings'].__setitem__('split',d['settings']['split']+.25))
    variant('simplify',lambda d:d['settings'].__setitem__('simplify',not d['settings']['simplify']))
    variant('precision',lambda d:d['geometry'].update(precision=0,desired_level=1))
    def color(d):
        for item in d['colors'].values():item['value']=item['value']^255
    variant('colors',color)
    def text(d):
        for key in d['texts']:d['texts'][key]+='!'
    variant('texts',text)
    if value['sections']:
        variant('section-plane',lambda d:d['sections'][0]['plane'].__setitem__(3,d['sections'][0]['plane'][3]+.125))
    for name,data in variants:
        cached=worker(r,runtime,data,out/(name+'-cached'),cache)
        worker(r,runtime,data,out/(name+'-full'),None)
        r.case(name+'.complete-key-invalidated',not cached['cache_hit'])
        equal(r,name+'.incremental-exact',out/(name+'-cached'),out/(name+'-full'))
        if name=='local-point':r.case('local-point.only-affected-cuts-rebuilt',cached['section_cache_hits']>0 and cached['section_cache_misses']>0,**{k:cached[k] for k in ('section_cache_hits','section_cache_misses')})
    # Corrupt full output, then raw cut payloads. Both must regenerate exact data.
    base=cache/cold['cache_key'];(base/'scene.fslp').write_bytes(b'broken')
    repaired=worker(r,runtime,value,out/'damaged-full',cache)
    r.case('corrupt-full-regenerates',not repaired['cache_hit']);equal(r,'corrupt-full.exact',out/'cold',out/'damaged-full')
    (base/'scene.fslp').write_bytes(b'broken')
    cuts=cache/'sections/cuts.fscache'
    r.case('local-cache-written',cuts.is_file())
    cuts.write_bytes(b'FSCUT004\ncorrupt')
    repaired=worker(r,runtime,value,out/'damaged-cuts',cache)
    r.case('corrupt-cuts-regenerate',repaired['section_cache_misses']>0);equal(r,'corrupt-cuts.exact',out/'cold',out/'damaged-cuts')
    meta=read(base/'entry.json');meta['label_widths']={'obsolete-font':-1}
    (base/'entry.json').write_text(json.dumps(meta),encoding='utf8')
    report=worker(r,runtime,value,out/'changed-font',cache)
    r.case('font-metrics-revalidated',not report['cache_hit']);equal(r,'font-metrics.exact',out/'cold',out/'changed-font')
    blocker=out/'unwritable-cache';blocker.write_text('occupied',encoding='ascii')
    worker(r,runtime,value,out/'cache-unavailable',blocker)
    equal(r,'cache-optional',out/'cold',out/'cache-unavailable')

def application(r,runtime,model,out):
    cache=out/'native-lines-cache';rows=[]
    for name in ('cold','restart'):
        folder=out/name;folder.mkdir(parents=True)
        request=folder/'request.json';request.write_text(json.dumps(dict(history_file=str(folder/'history.json'),
            lines_cache_root=str(cache),commands=[dict(kind='startup',path=str(model))])),encoding='utf8')
        result=q.run_child(r,'application.'+name,runtime/'FreeShipQt.exe',['--verify-startup',request],folder/'window',platform='windows')
        path=result.get('artifacts',{}).get('observations')
        if not path:raise RuntimeError('Application startup observation absent')
        row=read(path)[0];rows.append(row)
        r.case('application.'+name+'.ready',row['ready'] and not row['error'])
        r.data.setdefault('application_timings',[]).append(dict(run=name,**row['native_report']))
    r.case('application.cross-process-cache-hit',not rows[0]['native_report']['cache_hit'] and rows[1]['native_report']['cache_hit'])
    for key in ('scene','dxf'):r.compare('application.restart.'+key,rows[0][key],rows[1][key])

def run(r,runtime,out):
    real=q.DEFAULT_MODELS[-1];cache=out/'session/native-lines-cache'
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'session/process',native_linesplan=True) as service:
        def command(method,name,**fields):return q.require_ok(service.command(method,output=out/'session'/name,lines_cache_root=str(cache),**fields))
        original=command('load','load',path=str(real),document_id=str(q.uuid.uuid4()))
        model=read(original['model']);point=next(p for p in model['control_points'] if p['batch_movable'])
        xyz=list(point['p']);xyz[2]+=.125
        moved=command('move_point','move',point_id=point['id'],position=xyz)
        report=read(moved['native_report']);r.data['real_edit']=report
        r.case('session.edit-reuses-unaffected-cuts',not report['cache_hit'] and report['section_cache_hits']>0 and report['section_cache_misses']>0)
        full=q.require_ok(service.command('snapshot',output=out/'session/full',lines_cache_root=''))
        equal(r,'session.edited-full-parity',Path(moved['model']).parent,Path(full['model']).parent)
        undo=command('undo','undo');redo=command('redo','redo')
        r.case('session.undo-redo-full-cache',read(undo['native_report'])['cache_hit'] and read(redo['native_report'])['cache_hit'])
        equal(r,'session.undo-exact',Path(original['model']).parent,Path(undo['model']).parent)
        equal(r,'session.redo-exact',Path(moved['model']).parent,Path(redo['model']).parent)
        value=read(Path(original['model']).parent/'native-input.json')
    matrix(r,runtime,value,out/'matrix')
    application(r,runtime,real,out/'application')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    runtime=(a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();sources=q.current_sources(q.QT);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    protected={str(m):q.common.digest_file(m) for m in q.DEFAULT_MODELS}
    r.data.update(suite='qt-lines-cache',runtime=str(runtime),qt_sources_sha256=sources,source_sha256=legacy,
        test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES},limitations=[
            'This suite covers whole-result and section caches. Local subdivision and fill generation are tested separately by qt-local-geometry; global layout, sorting and final output remain complete.',
            'Hidden production startup verifies real application wiring, not desktop input or visual acceptance.'])
    try:
        manifest=read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if sources!={n.replace('\\','/'):v for n,v in manifest['source_sha256'].items()}:raise RuntimeError('Stale source manifest')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime file mismatch')
        run(r,runtime,out)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==legacy)
        r.case('original-models-protected',all(q.common.digest_file(Path(n))==h for n,h in protected.items()))
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.case('desktop-acceptance',status='not_run',reason='No desktop automation.');e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
