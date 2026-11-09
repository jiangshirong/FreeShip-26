"""Production multi-curve charts against retained, exact numerical baselines.

Read historical results without modifying them. New outputs contain images,
hashes and small reports; large acceleration CSV/JSON files are not duplicated.
"""
import argparse,csv,hashlib,json,struct,time
from pathlib import Path
import qt_migration as q
import qt_editing as e
from maintenance import generate_calculation_charts as metadata
REFERENCE=Path('C:/Users/12968/FreeShip26-TestResults/calculation-layout-20261109/final-04/qt-calculation-layout/calculations')
METHODS=('delft','holtrop','hollenbach','oortmerssen','fung','ost','rbhs','mh','kaper','planing','propeller1','propeller2','propeller3','propeller4','propeller5','wind','acceleration')
FILES=('qt_calculation_charts.py','qt/calculation_chart_tests.cpp','maintenance/generate_calculation_charts.py','qt_migration.py','qt_editing.py','run_tests.py')
def read(p):return json.loads(Path(p).read_text('utf8'))
def numeric_hash(data):
    h=hashlib.sha256()
    for curve in data['series']:
        h.update(struct.pack('<Q',len(curve['points'])))
        for x,y in curve['points']:h.update(struct.pack('<dd',x,y))
    return h.hexdigest()
def baseline(reference,method):
    folder=reference/('fixture-'+method) if method in ('propeller1','propeller2','propeller3') else reference/('hydro' if method in ('wind','acceleration') else 'performance')/method
    data=folder/'qt/performance-report.json';request=folder/('request.json' if method in ('wind','acceleration') else 'performance-request.json')
    return data,request
def run(r,runtime,out,reference,installed=False,performance=False):
    out.mkdir();r.data['replays']={};r.data['production']={};r.data['references']={}
    styles,hashes=metadata.generate();r.case('original-style-source',read(q.QT/'src/app/calculation_charts-origin.json')==hashes)
    header=(q.QT/'src/app/calculation_charts.h').read_text('utf8').split('R"charts(',1)[1].split(')charts"',1)[0]
    r.case('declared-styles-retained',json.loads(header)==styles)
    r.data['synthetic']=q.run_child(r,'synthetic',runtime/'qt_calculation_chart_tests.exe',['-'],out/'synthetic',platform='windows')
    if performance:
        r.data['shutdown']=q.run_child(r,'shutdown',runtime/'qt_calculation_chart_tests.exe',['--warmup-exit'],out/'shutdown',platform='windows',timeout=30)
        for scale in ('1','1.5','2'):r.data['synthetic_scale_'+scale]=q.run_child(r,'synthetic.scale'+scale,runtime/'qt_calculation_chart_tests.exe',['-','--scale='+scale],out/('synthetic-scale-'+scale),platform='windows')
    methods=('acceleration',) if performance else ('holtrop','propeller1','propeller5','acceleration') if installed else METHODS
    for method in methods:
        source,request=baseline(reference,method);raw=read(source);expected=numeric_hash(raw)
        r.data['references'][method]=dict(result=str(source),sha256=q.common.digest_file(source),request=str(request),request_sha256=q.common.digest_file(request),numeric_sha256=expected)
        if not installed:
            replay=q.run_child(r,'replay.'+method,runtime/'qt_calculation_chart_tests.exe',[source],out/('replay-'+method),platform='windows',timeout=120);r.data['replays'][method]=replay
            r.case(method+'.every-coordinate-binary-exact',replay['numeric_sha256']==expected)
            for observation in replay['observations']:
                for curve in observation['curves']:
                    definitions=styles[method]['series'];index=curve['index'];definition=next((s for s in definitions if s['source']==curve['source']),definitions[index])
                    r.case(method+f'.original-color-{index}',curve['color']==definition['color'])
        # Reuse the actual previously executed input, not inferred parameters.
        old=read(request);args={k:old[k] for k in ('model','inputs','prepare') if k in old};args.update(verify_charts=True,charts_only=method not in ('delft','propeller1','propeller5'))
        folder=out/('production-'+method);folder.mkdir();req=folder/'request.json';req.write_text(json.dumps(args,ensure_ascii=False),encoding='utf8')
        child=q.run_child(r,'production.'+method,runtime/'FreeShipQt.exe',['--verify-performance',req],folder/'qt',platform='windows',timeout=180);r.data['production'][method]=child
        artifacts=child.get('artifacts',{});charts=artifacts.get('charts',[])
        r.case(method+'.production-chart-discovered',len(charts)==2)
        for i,chart in enumerate(charts):r.case(method+f'.production-coordinate-parity-{i}',chart['numeric_sha256']==expected)
        if method=='acceleration':
            timings=[]
            for i,chart in enumerate(charts):
                frames=[v for v in chart.get('render_history',[]) if v.get('points',0)>0]
                timings.append(dict(preparation_ms=chart.get('preparation_ms'),frames=frames))
                if frames and frames[0].get('gpu_layers',0):
                    r.case(f'acceleration.{i}.restore-avoids-coordinate-traversal',frames[-1]['mapping_ms']==0 and frames[-1]['stroke_ms']==0 and frames[-1]['layer_hits']>0)
                else:r.case(f'acceleration.{i}.hardware-acceleration',status='not_run',reason='OpenGL/MSAA unavailable; full-resolution software fallback used')
            r.data['acceleration_timings']=timings
        for kind in ('scene','dxf'):
            if 'after_'+kind in artifacts:r.compare(method+'.readonly-'+kind,artifacts['original_'+kind],artifacts['after_'+kind])
        if 'report_json' in artifacts:
            actual=read(artifacts['report_json']);r.case(method+'.export-coordinate-parity',numeric_hash(actual)==expected)
            rows=list(csv.reader(Path(artifacts['report_csv']).read_text('utf-8-sig').splitlines()));r.case(method+'.csv-precision',[[float(v[-2]),float(v[-1])] for v in rows[1:]]==[xy for s in raw['series'] for xy in s['points']])
            if method=='propeller1':
                diameter=next(s for s in actual['series'] if s['source']=='Series3');r.case('diameter.not-power',diameter['y_title']=='螺旋桨直径 (m)' and any('螺旋桨直径 (m)' in row for row in rows[1:]))
        r.case(method+'.historical-evidence-protected',q.common.digest_file(source)==r.data['references'][method]['sha256'])
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--reference',type=Path,default=REFERENCE);p.add_argument('--performance-only',action='store_true');a=p.parse_args()
    runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.monotonic();sources=q.current_sources(q.QT);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    originals={str(p):q.common.digest_file(p) for p in q.DEFAULT_MODELS};r.data.update(suite='qt-calculation-charts',runtime=str(runtime),qt_sources_sha256=sources,source_sha256=legacy,test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES})
    try:
        manifest=read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        if {n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime mismatch')
        r.case('runtime-manifest',True);run(r,runtime,out/'checks',a.reference.resolve(),bool(a.verify_deployment),a.performance_only)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==legacy);r.case('models-protected',all(q.common.digest_file(Path(p))==h for p,h in originals.items()));r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        if not a.performance_only:r.case('external-solver-limit',status='not_run',reason='Task1-3 use the retained CALCPROP protocol fixture; the supplied real executable still produces no result. Not engineering validation.')
        r.case('desktop-acceptance',status='not_run',reason='Hidden Qt events/rendering only; physical desktop input, printers and multi-monitor appearance remain separate.')
        r.case('visual-review',status='review',reason='Review retained production screenshots for chart units, legends and labels; fixed layout rules are not a complete visual verdict.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
