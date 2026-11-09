"""Record this local-geometry delivery and retire only its first owned run's cache.

Delivery maintenance, not a regression test. Reports, inputs, failed comparison
artifacts, all historical evidence and the rollback package remain intact.
"""
import argparse
import json
import os
import stat
import sys
from datetime import datetime,timezone
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import qt_migration as q
import test_artifacts

def read(path):return json.loads(Path(path).read_text('utf-8-sig'))

def size(root):
    total=0;pending=[root]
    while pending:
        with os.scandir(pending.pop()) as entries:
            for entry in entries:
                info=entry.stat(follow_symlinks=False)
                if info.st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT:continue
                if entry.is_dir(follow_symlinks=False):pending.append(Path(entry.path))
                elif entry.is_file(follow_symlinks=False):total+=info.st_size
    return total

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--results',type=Path,required=True);args=parser.parse_args()
    results=args.results.resolve(strict=True)
    expected=Path('C:/Users/12968/FreeShip26-TestResults/incremental-geometry-20261109').resolve(strict=True)
    if results!=expected or test_artifacts.is_link(results.lstat()):raise RuntimeError('Unexpected delivery result root')
    runtime=q.QT/'dist';manifest=read(runtime/'build-manifest.json')
    if q.current_sources(q.QT)!={n.replace('\\','/'):h for n,h in manifest['source_sha256'].items()}:raise RuntimeError('Installed source mismatch')
    q.validate_test_manifest(manifest)
    if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Installed binary mismatch')
    reports={}
    for name,relative in [('candidate','checkpoint-candidate/qt-local-geometry'),('installed','installed-final/qt-local-geometry'),
                          ('cache','cache-regression/qt-lines-cache'),('pascal-publication','publication-regression/qt-publication')]:
        path=results/relative/'report.json';value=read(path)
        if value['counts']['fail'] or value['counts']['error']:raise RuntimeError('Unresolved regression: '+str(path))
        reports[name]={'path':str(path),'sha256':q.common.digest_file(path),'counts':value['counts']}
    installed=read(Path(reports['installed']['path']))
    if Path(installed['runtime']).resolve()!=runtime.resolve():raise RuntimeError('Installed route not exercised')
    if any(q.common.digest_file(ROOT/n)!=h for n,h in installed['test_sources_sha256'].items()):raise RuntimeError('Final test sources changed')
    # The first development run predates support for these two derived-cache
    # filenames. Use the same ownership marker and containment checks now.
    first=results/'candidate-check';owner=read(first/test_artifacts.MARKER)
    cleanup=test_artifacts.finish(first,owner['token'])
    candidate=read(Path(reports['candidate']['path']))
    receipt={'created_utc':datetime.now(timezone.utc).isoformat(),'scope':'Exact local subdivision and root-face fill generation',
        'runtime':str(runtime),'build_manifest_sha256':q.common.digest_file(runtime/'build-manifest.json'),
        'source_sha256':manifest['source_sha256'],'test_sources_sha256':installed['test_sources_sha256'],
        'reports':reports,'benchmark':candidate['benchmark'],'service_edit':candidate['service_edit'],
        'previous_worker_sha256':candidate['previous_installed_worker_sha256'],
        'rollback_package':str(q.QT/'build/dist-before-local-geometry-20261109'),
        'storage':{'root':str(results),'retained_bytes':size(results),'first_run_retired_bytes':cleanup['logical_bytes']},
        'boundaries':['Global layout, depth sorting and final serialization still complete.',
                      'A disk checkpoint is refreshed for broad edits; small edits reuse a consistent earlier checkpoint.',
                      'Cold dependency construction adds work. Measured worker latency is not desktop interaction latency.',
                      'Native desktop acceptance remains not_run. No Lazarus, QtPilot, user model or launcher was modified.']}
    target=ROOT/'maintenance/local-geometry-delivery-20261109.json'
    if target.exists():raise RuntimeError('Delivery receipt already exists')
    target.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'receipt':str(target),'reports':{k:v['counts'] for k,v in reports.items()},'storage':receipt['storage']},ensure_ascii=False))

if __name__=='__main__':main()
