"""One-time native point-edit delivery receipt; no application mutation.

Run: python -B FreeShip-Tests/maintenance/record_point_delivery.py
Validates the installed inventory, tested binaries, source and report hashes.
This is a delivery audit, not a replacement for the qt-document regression.
"""
import hashlib
import json
import shutil
import statistics
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import qt_migration as q


def read(path):return json.loads(path.read_text('utf-8-sig'))


def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()


def main():
    installed=(q.QT/'dist').resolve(strict=True)
    backup=(q.QT/'build/dist-before-point-20261102').resolve(strict=True)
    base=Path('C:/FreeShip26Tests/reports')
    manifest_path=installed/'build-manifest.json';manifest=read(manifest_path)
    q.validate_test_manifest(manifest)
    sources=q.current_sources(q.QT)
    assert {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}==sources
    files={}
    for path in installed.rglob('*'):
        assert installed in path.resolve().parents
        if path.is_file() and path!=manifest_path:files[path.relative_to(installed).as_posix()]=digest(path)
    assert files=={k.replace('\\','/'):v for k,v in manifest['binaries_sha256'].items()}
    assert digest(q.QT/'package.ps1')==manifest['package_script_sha256']
    legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy==manifest['legacy_source_sha256']=='277a3c69392c86fe14c375e7dfc9c3197b4162e982673cbbcf4b4a2dfff0ab6b'
    bridge=read(installed/'bridge/report.json')
    assert bridge['source_sha256']==legacy and bridge['adapter_sha256']==q.bridge_builder(q.QT).adapter_hashes()
    assert digest(installed/'bridge/FreeShipGeometryBridge.exe')==bridge['build']['executable_sha256']
    core_path=base/'qt-point-b/qt-document/report.json';core=read(core_path)
    assert core['counts']=={'pass':2193,'fail':0,'review':2,'not_run':2,'error':0}
    assert core['qt_sources_sha256']==sources
    for file,h in core['test_sources_sha256'].items():assert digest(ROOT/file)==h,file
    tested=Path(core['runtime_path'])
    binaries=('FreeShipQt.exe','FreeShipDocumentService.exe','FreeShipNativeLinesplan.exe',
              'qt_document_codec_tests.exe','bridge/FreeShipGeometryBridge.exe')
    for name in binaries:assert digest(tested/name)==digest(installed/name),name
    reports={'qt-document':{'path':str(core_path),'sha256':digest(core_path),'counts':core['counts'],
                            'scope':'Full isolated core suite; all relevant binaries identical to installed package.',
                            'phase_seconds':core['phase_seconds'],'elapsed_seconds':core['elapsed_seconds']}}
    for name,wanted in {'editing':(142,0,4),'sections':(214,0,1),'auxiliary':(151,3,1),
                         'advanced':(151,3,1),'hydro':(26,0,1)}.items():
        path=base/f'qt-{name}-point-installed/report.json';data=read(path);counts=data['counts']
        assert counts['fail']==counts['error']==0
        assert (counts['pass'],counts['review'],counts['not_run'])==wanted,(name,counts)
        assert Path(data['deployment']).resolve()==installed
        assert data['package_manifest']==manifest
        assert digest(Path(data['workflow_reference'])/'report.json')==data['workflow_reference_sha256']
        reports[name]={'path':str(path),'sha256':digest(path),'counts':counts,'scope':'Actual installed production Qt callbacks.'}
    codec=read(base/'qt-point-b/qt-document/codec/report.json')
    assert codec['status']=='pass' and codec['supplied_files']==80
    encoded=sum(bool(c.get('coordinate_write')) for c in codec['cases'])
    assert encoded==77
    skipped=[{'fixture':c['fixture'],'points':c['inspection']['counts'].get('control_points'),
              'opaque_tail_bytes':c['inspection']['opaque_tail_bytes']}
             for c in codec['cases'] if 'fixture' in c and not c.get('coordinate_write')]
    timing_path=base/'point-latency-01/report.json';timing=read(timing_path)
    assert timing['script_sha256']==digest(ROOT/'maintenance/point_latency.py')
    assert digest(backup/'FreeShipDocumentService.exe')==timing['executables']['baseline']['sha256']
    assert digest(installed/'FreeShipDocumentService.exe')==timing['executables']['candidate']['sha256']
    kernel=[v['point_edit_ms'] for v in core['native_point_timings']]
    first=base/'qt-point-a/qt-document/report.json'
    now=datetime.now(timezone.utc);start=datetime.fromisoformat('2026-11-02T15:00:06+00:00')
    receipt={
        'recorded_utc':now.isoformat(),'scope':'Native single/batch absolute point coordinates, anchor/line dependencies, FBM coordinate encoding and derived-cache invalidation.',
        'installed':str(installed),'previous_installation':str(backup),
        'manifest_sha256':digest(manifest_path),'verified_package_files':len(files),
        'binaries_sha256':{n:digest(installed/n) for n in binaries},'legacy_source_sha256':legacy,
        'reports':reports,'combined_counts':{k:sum(r['counts'][k] for r in reports.values()) for k in ('pass','fail','review','not_run','error')},
        'codec':{'cases':len(codec['cases']),'real_files_read':80,'real_files_coordinate_written':encoded,'coordinate_write_skipped':skipped,
                 'note':'45 synthetic version branches also check exact preservation of unrelated blocks and complete invalidation of serialized section/flowline caches.'},
        'latency':{'path':str(timing_path),'sha256':digest(timing_path),'models':timing['models'],
                   'kernel_ms':{'min':min(kernel),'median':statistics.median(kernel),'max':max(kernel)},
                   'note':'Paired complete document requests, three moves/model, order alternated. This is not desktop frame rate or a statistically established speedup. Baseline path in the probe now resides in the backup directory.'},
        'development_report':{'path':str(first),'sha256':digest(first),'counts':read(first)['counts'],
                              'note':'Correct first implementation reloaded the adapter after each edit. Replaced with absolute coordinate deltas; final core and installed checks cover that path.'},
        'time_accounting':{'first_observed_utc':start.isoformat(),'receipt_utc':now.isoformat(),'elapsed_wall_seconds':(now-start).total_seconds(),
                           'core_first_run_s':read(first)['elapsed_seconds'],'core_final_run_s':core['elapsed_seconds'],
                           'note':'Installed window suites ran concurrently. Reading/design/coding/integration/diagnosis were not separately timed; do not label remaining elapsed time as test waiting.'},
        'notes':[
            'Coordinate computation and FBM coordinate writes execute in C++; the adapter rejects delegated move_point and move_points. Its private apply_document_points publishes already-computed coordinates without propagating constraints again.',
            'The existing adapter work copy is reused. Cold recovery and undo still reload a full state. The immutable C++ candidate is committed only after returned coordinates and derived artifacts validate.',
            'Remaining Pascal dependencies include topology, transform-menu semantics, constraint definitions, other FBM fields, four-view mesh generation and professional calculations. This is not complete native FBM re-encoding.',
            'Old format/unknown-tail constraints still apply; no test expectation or numeric tolerance was weakened. One corpus file with an opaque tail and two files with no control points are intentionally not coordinate-written.',
            'Tests use hidden actual Qt callbacks. Physical desktop interaction and full visual fidelity remain review/not_run; no computer-use acceptance was performed.',
            'Reusable tests are retained in FreeShip-Tests. Reports and disposable builds are on C. User models, old FreeShip, QtPilot and fs are preserved.'
        ],
        'maintenance_sources_sha256':{n:digest(ROOT/'maintenance'/n) for n in ('promote_runtime.ps1','point_latency.py','record_point_delivery.py')},
        'free_bytes_after':{d:shutil.disk_usage(d+':/').free for d in ('C','D')}
    }
    target=ROOT/'maintenance/point-installed-20261102.json'
    target.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'receipt':str(target),'counts':receipt['combined_counts'],'verified_package_files':len(files)},ensure_ascii=False))


if __name__=='__main__':main()
