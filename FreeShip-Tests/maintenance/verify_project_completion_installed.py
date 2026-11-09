"""Run the retained production actions at the actual installed path, isolated."""
import argparse,json,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import qt_migration as q
import qt_editing as e
import qt_project_completion as feature

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);runtime=(q.QT/'dist').resolve();r=q.Report();start=time.monotonic()
    sources=q.current_sources(q.QT);r.data.update(suite='installed-project-completion',runtime=str(runtime),qt_sources_sha256=sources,
        test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in (*feature.FILES,'maintenance/verify_project_completion_installed.py')})
    def files():return {p.relative_to(runtime).as_posix():q.common.digest_file(p) for p in runtime.rglob('*') if p.is_file()}
    before=files()
    try:
        manifest=feature.read(runtime/'build-manifest.json');q.validate_test_manifest(manifest)
        r.case('installed-current-sources',sources=={n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()})
        r.case('installed-current-files',{n:h for n,h in before.items() if n!='build-manifest.json'}==manifest['binaries_sha256'])
        model=out/'fixture.fbm';feature.elevated(model);feature.production(r,runtime,model,out/'production')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('installed-unchanged',files()==before);r.case('source-unchanged',sources==q.current_sources(q.QT))
        r.case('desktop',status='not_run',reason='Hidden production callbacks, not physical desktop acceptance.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
