"""Recheck only the original-oracle construction phase of a complete run.

This is deliberately narrow: product source/binaries and every other test must
be unchanged, and all prior failures must belong to that phase. The raw report
is immutable. A new effective report records both runs and per-case provenance.
"""
import argparse,copy,json,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
import qt_migration as q
import qt_document as document
import qt_native_construction as construction


def read(path):return json.loads(path.read_text('utf8'))
def digest(path):return q.common.digest_file(path)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--core',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    core=read(a.core);prefix='construction-native.';runtime=Path(core['runtime_path']);source=q.DEFAULT_MODELS[0]
    assert not core['counts']['error']
    assert all(c['id'].startswith(prefix) for c in core['cases'] if c['status']=='fail')
    assert core['qt_sources_sha256']==q.current_sources(q.QT)
    assert core['source_sha256']==q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    manifest=read(runtime/'build-manifest.json')
    for name,h in manifest['binaries_sha256'].items():assert digest(runtime/name)==h,name
    old=core['test_sources_sha256'];now={f:digest(ROOT/f) for f in document.FILES}
    allowed={'qt_document.py','qt_native_construction.py','fixtures/native-construction/plane-box/provenance.json'}
    allowed.update('fixtures/native-construction/plane-box/2/'+f for f in ('model.json','scene.fslp','reference.dxf','sections.json'))
    changed={f for f in set(old)|set(now) if old.get(f)!=now.get(f)}
    assert changed.issubset(allowed),changed-allowed
    # Existing native baseline and both original fixtures are immutable here.
    assert all(now[f]==h for f,h in old.items() if f.startswith(('fixtures/native-construction/plane-box/0/','fixtures/native-construction/plane-box/1/')))
    a.output.mkdir(parents=True,exist_ok=False);stamp=time.perf_counter();r=q.Report()
    construction.run(r,runtime,source,a.output/'construction');r.finish();elapsed=time.perf_counter()-stamp
    assert all(digest(ROOT/f)==h for f,h in now.items())
    r.data.update(qt_sources_sha256=core['qt_sources_sha256'],test_sources_sha256=now,runtime_path=str(runtime),elapsed_seconds=elapsed)
    phase=a.output/'construction-report.json';phase.write_text(json.dumps(r.data,ensure_ascii=False,indent=2),'utf8')
    assert not r.data['counts']['fail'] and not r.data['counts']['error'],r.data['counts']
    effective=q.Report();effective.data=copy.deepcopy(core)
    effective.data['cases']=[dict(c,evidence_run='original-full') for c in core['cases'] if not c['id'].startswith(prefix)]
    effective.data['cases'] += [dict(c,evidence_run='construction-recheck') for c in r.data['cases']]
    effective.data.update(test_sources_sha256=now,elapsed_seconds=core['elapsed_seconds']+elapsed,
        codec_report=str((a.core.parent/'codec/report.json').resolve()),
        validation_mode='Complete original run plus construction-only recheck; original report remains unchanged.',
        validation_runs=[dict(path=str(a.core.resolve()),sha256=digest(a.core),counts=core['counts']),dict(path=str(phase.resolve()),sha256=digest(phase),counts=r.data['counts'])],
        superseded_cases=[c for c in core['cases'] if c['id'].startswith(prefix)],
        recheck_seconds=elapsed,changed_test_sources=sorted(changed),
        consolidation_source_sha256=digest(Path(__file__)))
    effective.data['limitations'][0]='Background/display/auxiliary-definition edits are native; auxiliary spline/flow tracing and remaining numerical analysis still use the read-only Pascal adapter.'
    effective.finish();output=a.output/'report.json';output.write_text(json.dumps(effective.data,ensure_ascii=False,indent=2),'utf8');print(effective.data['counts'])


if __name__=='__main__':main()
