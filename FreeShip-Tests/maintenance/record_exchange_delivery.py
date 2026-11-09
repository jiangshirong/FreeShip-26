"""One-time 2026-11-02 delivery/storage evidence; not an application test suite.

Run after the two deployment checks and verified report relocation. Reads all
installed package hashes and writes small receipt/index files in maintenance.
"""
import hashlib
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import qt_migration as q


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text('utf-8-sig'))


def write(path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf8')


def main():
    install = ROOT.parent / 'FreeShip-Qt/dist'
    storage = Path('C:/FreeShip26Tests')
    manifest_path = install / 'build-manifest.json'
    manifest = read(manifest_path)
    q.validate_test_manifest(manifest)
    assert {k.replace('\\', '/'): v for k, v in manifest['source_sha256'].items()} == q.current_sources(q.QT)
    for name, value in manifest['binaries_sha256'].items():
        assert digest(install / name) == value, name
    legacy = q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy == '277a3c69392c86fe14c375e7dfc9c3197b4162e982673cbbcf4b4a2dfff0ab6b'
    reports = {}
    for name, expected in {
        'qt-exchange-unified/qt-exchange': (271, 1, 1),
        'qt-exchange-final': (277, 1, 1),
        'qt-migration-exchange-a': (2154, 27, 18),
        'qt-exchange-package-final': (280, 1, 1),
        'qt-plates-exchange-package': (34, 0, 1),
        'qt-performance-exchange-package': (29, 0, 1),
        'qt-exchange-delivery': (280, 1, 1),
        'qt-migration-exchange-delivery': (85, 0, 4),
    }.items():
        path = storage / 'reports' / name / 'report.json'
        report = read(path)
        counts = report['counts']
        assert (counts['pass'], counts['review'], counts['not_run']) == expected, name
        assert counts['fail'] == 0 and counts['error'] == 0, name
        reports[name] = {'path': str(path), 'status': report['status'], 'counts': counts, 'report_sha256': digest(path)}
    write(ROOT / 'maintenance/exchange-installed-20261102.json', {
        'date': '2026-11-02',
        'scope': 'OBJ/STL/FEF model import and export, native format validation, Qt dialogs and transactional candidate import.',
        'installed': str(install),
        'previous_installation': str(ROOT.parent / 'FreeShip-Qt/build/dist-before-exchange-20261102'),
        'binaries_sha256': {n: digest(install/n) for n in ('FreeShipQt.exe', 'bridge/FreeShipGeometryBridge.exe')},
        'package_manifest_sha256': digest(manifest_path),
        'package_files_verified': len(manifest['binaries_sha256']),
        'legacy_source_sha256': legacy,
        'bridge_build': str(ROOT.parent / 'FreeShip-Qt/build/exchange-service-04'),
        'reports': reports,
        'notes': [
            'Final candidate and installed-directory exchange checks each contain 280 pass, 0 fail, 1 review and 1 not_run.',
            'The full 2154-pass migration run preceded only the new imperial-import default density fix and FEF unit-display fix; actual-directory edit/save/reopen checks passed afterward.',
            'FEF roundtrip on real 9a compares point/edge/layer attributes and the full derived triangle geometry. OBJ/STL checks use independent readers and mathematical fixtures.',
            'Current model ownership, candidate construction, FBM and professional calculation still use the Pascal bridge. The whole native core migration is not complete.',
            'Hidden Qt callbacks do not cover the native chooser, real mouse/focus, external CAD interoperability or full original UI acceptance.',
            'FEF is control-model exchange, not full FBM project persistence. Material libraries, STL colors and unsupported OBJ geometry are not silently imported.',
            'All reusable tests remain under FreeShip-Tests and qt-exchange is integrated in run_tests.py.',
            'Legacy fs, QtPilot, user models and preferences were not replaced.',
        ],
    })
    relocation_path = storage / 'maintenance/relocation-20261102/final-paths.json'
    relocation = read(relocation_path)
    assert relocation['status'] == 'complete'
    for row in relocation['items']:
        dest = Path(row['destination'])
        if row['d_junction']:
            assert Path(row['d_junction']).resolve() == dest.resolve(), row['name']
        for report in row['reports']:
            assert digest(dest / report['relative']).upper() == report['sha256'], row['name']
    default_report = storage / 'reports/combined-20261102-184439/report.json'
    default_data = read(default_report)
    assert default_data['status'] == 'pass'
    assert default_data['cases'][0]['id'] == 'historical-assets'
    assert Path(default_data['cases'][0]['report']).is_relative_to(storage / 'reports')
    assert Path(read(ROOT / 'output-location.json')['report_root']).resolve() == storage / 'reports'
    write(ROOT / 'maintenance/test-output-storage-20261102.json', {
        'date': '2026-11-02', 'status': 'complete',
        'report_root': str(storage / 'reports'),
        'source_remains': str(ROOT),
        'd_bytes_relocated': relocation['d_bytes_relocated'],
        'd_directories_relocated': sum(bool(r['d_junction']) for r in relocation['items']),
        'relocation_receipt': str(relocation_path),
        'relocation_receipt_sha256': digest(relocation_path),
        'default_runner_report': str(default_report),
        'default_runner_report_sha256': digest(default_report),
        'default_runner_status': default_data['status'],
        'free_bytes_after': {drive: shutil.disk_usage(drive + ':/').free for drive in ('C', 'D')},
        'notes': [
            'Every file was SHA256-verified against its D copy before that duplicate was removed; original manifests are retained beside the relocation receipt.',
            'Final C-to-C move preserved counts and report hashes. Both historical D paths and the initial Codex-local C path remain accessible through junctions.',
            'The default unified runner was exercised without --output. Explicit output paths still override the local configuration.',
            'These one-time storage/delivery scripts are retained for audit and are not application regression tests or automatic cleanup tasks.',
        ],
    })
    print(json.dumps({'installed_files_verified': len(manifest['binaries_sha256']),
                      'd_gib_relocated': relocation['d_bytes_relocated']/1024**3,
                      'storage': str(storage), 'receipts': 'written'}))


if __name__ == '__main__':
    main()
