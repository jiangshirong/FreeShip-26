"""Recheck the three diagnosed stages without repeating unchanged document tests.

Requires unchanged product/original source, immutable prior fixtures and no
other failing stage. Writes fresh stage reports and a per-case evidence index;
the original full report is never overwritten. This is an audit-specific
continuation, not permission to ignore arbitrary failures.
"""
import argparse
import copy
import json
import sys
import time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
import qt_migration as q
import qt_document as document
import qt_native_construction as construction
import qt_native_definitions as definitions
import qt_native_create as creation
import test_artifacts

def read(path):return json.loads(Path(path).read_text('utf-8-sig'))
def digest(path):return q.common.digest_file(Path(path))

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--core',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    a=parser.parse_args();core=read(a.core);prefixes=('construction-native.','definitions.','create.')
    if core['counts']['error'] or any(not c['id'].startswith(prefixes) for c in core['cases'] if c['status']=='fail'):
        raise RuntimeError('Unclassified failure outside the three audited stages')
    if core['qt_sources_sha256']!=q.current_sources(q.QT) or core['source_sha256']!=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]:
        raise RuntimeError('Product or original source changed')
    runtime=Path(core['runtime_path']);manifest=read(runtime/'build-manifest.json')
    for name,value in manifest['binaries_sha256'].items():
        if digest(runtime/name)!=value:raise RuntimeError('Runtime changed: '+name)
    old=core['test_sources_sha256'];now={f:digest(ROOT/f) for f in document.FILES}
    allowed={'qt_native_construction.py','qt_native_definitions.py','qt_native_create.py','fixtures/native-construction/plane-box/provenance.json'}
    allowed.update('fixtures/native-construction/plane-box/3/'+f for f in ('model.json','scene.fslp','reference.dxf','sections.json'))
    changed={f for f in old.keys()|now.keys() if old.get(f)!=now.get(f)}
    if not changed.issubset(allowed):raise RuntimeError('Unrelated test changed: '+str(changed-allowed))
    a.output.mkdir(parents=True,exist_ok=False);token=test_artifacts.claim(a.output);start=time.perf_counter();runs=[];cases=[]
    jobs=[('construction',construction,q.DEFAULT_MODELS[0]),('definitions',definitions,q.DEFAULT_MODELS[0]),('create',creation,q.DEFAULT_MODELS)]
    for name,module,inputs in jobs:
        report=q.Report();stamp=time.perf_counter()
        try:module.run(report,runtime,inputs,a.output/name)
        except Exception as ex:report.case(name+'.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
        report.finish();path=a.output/(name+'-report.json')
        report.data.update(runtime_path=str(runtime),test_sources_sha256=now,elapsed_seconds=time.perf_counter()-stamp)
        path.write_text(json.dumps(report.data,ensure_ascii=False,indent=2)+'\n','utf8')
        runs.append(dict(name=name,path=str(path.resolve()),sha256=digest(path),counts=report.data['counts']))
        cases.extend(dict(c,evidence_run=name+'-recheck') for c in report.data['cases'])
        print(json.dumps(runs[-1],ensure_ascii=False),flush=True)
    if any(digest(ROOT/f)!=value for f,value in now.items()):raise RuntimeError('Test changed during recheck')
    if any(run['counts']['fail'] or run['counts']['error'] for run in runs):raise RuntimeError('Recheck still has failures; original full result remains unresolved')
    effective=q.Report();effective.data=copy.deepcopy(core)
    effective.data['cases']=[dict(c,evidence_run='original-full') for c in core['cases'] if not c['id'].startswith(prefixes)]+cases
    effective.data.update(test_sources_sha256=now,elapsed_seconds=core['elapsed_seconds']+time.perf_counter()-start,
        validation_mode='Full document run plus three audited stage rechecks; every case names its evidence run.',
        validation_runs=[dict(name='original-full',path=str(a.core.resolve()),sha256=digest(a.core),counts=core['counts']),*runs],
        superseded_cases=[c for c in core['cases'] if c['id'].startswith(prefixes)],
        changed_test_sources=sorted(changed),consolidation_source_sha256=digest(__file__))
    effective.finish();path=a.output/'report.json';path.write_text(json.dumps(effective.data,ensure_ascii=False,indent=2)+'\n','utf8')
    retention=test_artifacts.finish(a.output,token)
    print(json.dumps({'counts':effective.data['counts'],'retired_bytes':retention['logical_bytes'],'report':str(path)},ensure_ascii=False))

if __name__=='__main__':main()
