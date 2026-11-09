"""Replay a retained full-resolution chart and record render stages (no exports).

Diagnostic only: timings and images need comparison on the same machine. The
qt-calculation-charts suite supplies numerical and interaction regression.
"""
import argparse,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import qt_migration as q
import qt_editing as e
from qt_calculation_charts import REFERENCE,baseline

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--sample',type=Path)
    p.add_argument('--renderers',action='store_true',help='Compare full-resolution raster and OpenGL painter backends')
    p.add_argument('--production',action='store_true',help='Use the real calculation dialog and retained acceleration request')
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False);r=q.Report()
    sample=a.sample or baseline(REFERENCE,'acceleration')[0]
    r.data['sample_sha256']=q.common.digest_file(sample)
    if a.production:
        old=json.loads(baseline(REFERENCE,'acceleration')[1].read_text('utf8'));request={k:old[k] for k in ('model','inputs','prepare') if k in old};request.update(verify_charts=True,charts_only=True)
        path=a.output/'request.json';path.write_text(json.dumps(request,ensure_ascii=False),encoding='utf8')
        r.data['profile']=q.run_child(r,'production',a.runtime/'FreeShipQt.exe',['--verify-performance',path],a.output/'qt',platform='windows',timeout=120)
        r.data['profile']['observations']=r.data['profile']['artifacts']['charts']
    else:r.data['profile']=q.run_child(r,'full-resolution',a.runtime/'qt_calculation_chart_tests.exe',[sample]+(['--renderers'] if a.renderers else []),a.output/'chart',platform='windows',timeout=120)
    e.write_report(r,a.output)
    print(json.dumps(r.data['profile'].get('measurements') or [dict((k,v.get(k)) for k in ('points','render_ms','range_ms','mapping_ms','stroke_ms')) for v in r.data['profile']['observations']],ensure_ascii=False))
    return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
