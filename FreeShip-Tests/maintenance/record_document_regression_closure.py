"""Record this document-regression audit against the unchanged installed build.

Delivery maintenance only. Requires every document stage and zero failures or
errors; retains review/not_run and the original failing evidence separately.
Run with --results pointing at the new document-regressions result directory.
"""
import argparse
import json
import sys
from datetime import datetime,timezone
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import qt_migration as q

def read(path):return json.loads(Path(path).read_text('utf-8-sig'))

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--results',type=Path,required=True)
    args=parser.parse_args();root=args.results.resolve(strict=True);runtime=q.QT/'dist'
    previous=ROOT/'maintenance/save-confirmation-delivery-20261109.json';prior=read(previous)
    manifest=runtime/'build-manifest.json';build=read(manifest)
    if q.common.digest_file(manifest)!=prior['build_manifest_sha256']:raise RuntimeError('Installed package changed')
    if q.current_sources(q.QT)!={k.replace('\\','/'):v for k,v in build['source_sha256'].items()}:raise RuntimeError('Product source changed')
    for name,digest in build['binaries_sha256'].items():
        if q.common.digest_file(runtime/name)!=digest:raise RuntimeError('Runtime file changed: '+name)
    reports={}
    for name,folder in [('construction','construction'),('lackenby','lackenby'),('face-tools','face-tools'),('full','rechecked')]:
        path=root/folder/'report.json';report=read(path)
        if report['counts']['fail'] or report['counts']['error']:raise RuntimeError('Unresolved checks: '+name)
        reports[name]={'path':str(path),'sha256':q.common.digest_file(path),'counts':report['counts']}
    full=read(reports['full']['path'])
    # Reconstruct the combined evidence, rather than trusting an edited count.
    raw_path=root/'full/qt-document/report.json';raw=read(raw_path);prefixes=('construction-native.','definitions.','create.')
    if raw['counts']['error'] or any(not c['id'].startswith(prefixes) for c in raw['cases'] if c['status']=='fail'):
        raise RuntimeError('Unclassified full-run failures')
    if full['validation_runs'][0]['sha256']!=q.common.digest_file(raw_path):raise RuntimeError('Original full report changed')
    expected=[dict(c,evidence_run='original-full') for c in raw['cases'] if not c['id'].startswith(prefixes)]
    for run in full['validation_runs'][1:]:
        if q.common.digest_file(Path(run['path']))!=run['sha256']:raise RuntimeError('Stage report changed')
        stage=read(run['path'])
        if stage['counts']['fail'] or stage['counts']['error']:raise RuntimeError('Failed stage recheck')
        expected.extend(dict(c,evidence_run=run['name']+'-recheck') for c in stage['cases'])
    if expected!=full['cases']:raise RuntimeError('Combined cases differ from their evidence')
    reports['initial-full']={'path':str(raw_path),'sha256':q.common.digest_file(raw_path),'counts':raw['counts']}
    for name,digest in full['test_sources_sha256'].items():
        if q.common.digest_file(ROOT/name)!=digest:raise RuntimeError('Changed regression source: '+name)
    if Path(full['runtime_path']).resolve()!=runtime.resolve():raise RuntimeError('Not the installed route')
    required={'codec','protocol','native-points','native-topology','native-modeling','native-transforms',
              'native-construction','native-lackenby','native-attributes','native-face-tools','native-project',
              'native-definitions','native-auxiliary','native-background','native-surface','native-create',
              'native-analysis','native-flowlines','native-hydrostatics','modeling-service','modeling-extra','production-ui'}
    if not {name+'_s' for name in required}.issubset(full['phase_seconds']):raise RuntimeError('Missing document stages')
    diagnostic=root/'plane-diagnostic.json'
    if set(read(diagnostic)['fields'])!={'settings'}:raise RuntimeError('Unexpected frozen geometry differences')
    value={'created_utc':datetime.now(timezone.utc).isoformat(),
           'scope':'Correct historical snapshot/oracle expectations and complete document regression',
           'product_runtime_unchanged':True,'runtime':str(runtime),'build_manifest_sha256':q.common.digest_file(manifest),
           'previous_delivery':{'path':str(previous),'sha256':q.common.digest_file(previous)},
           'original_failures':prior['reports']['document'],
           'reports':reports,'phase_seconds':full['phase_seconds'],'validation_runs':full['validation_runs'],
           'validation_mode':full['validation_mode'],
           'test_sources_sha256':full['test_sources_sha256'],
           'diagnostic':{'path':str(diagnostic),'sha256':q.common.digest_file(diagnostic)},
           'diagnostic_sources_sha256':{p.name:q.common.digest_file(p) for p in
                [Path(__file__),*[ROOT/'maintenance'/name for name in ('audit_construction_regression.py',
                    'plane_oracle_replay.py','capture_plane_oracle_variant.py','recheck_document_regressions.py')]]},
           'boundaries':['Known exact legacy pointer-order variants remain review.',
                         'Hidden callbacks are not real desktop acceptance.',
                         'No product binary, native algorithm, frozen geometry or export tolerance changed.']}
    path=ROOT/'maintenance/document-regression-closure-20261109.json'
    if path.exists():raise RuntimeError('Receipt already exists')
    path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps({'receipt':str(path),'counts':full['counts'],'phases':len(required)},ensure_ascii=False))

if __name__=='__main__':main()
