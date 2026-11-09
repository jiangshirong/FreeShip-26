"""Verify the installed route using the production flow from qt-project-hydro.

Delivery driver only: assertions live in the reusable unified suite. Avoids
rerunning all format/math cases after a byte-verified package copy.
"""
import argparse,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import qt_project_hydro as suite
q=suite.q

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);runtime=(q.QT/'dist').resolve();r=q.Report()
    sources=q.current_sources(q.QT);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    protected={str(p):q.common.digest_file(p) for p in q.DEFAULT_MODELS}
    files=(*suite.FILES,'maintenance/verify_project_hydro_installed.py')
    r.data.update(suite='qt-project-hydro-installed-route',runtime=str(runtime),qt_sources_sha256=sources,source_sha256=legacy,test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in files})
    try:suite.production(r,runtime,q.DEFAULT_MODELS[0],out/'production')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('sources-stable',q.current_sources(q.QT)==sources and q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==legacy)
        r.case('models-protected',all(q.common.digest_file(Path(p))==h for p,h in protected.items()))
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        suite.e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))

if __name__=='__main__':raise SystemExit(main())
