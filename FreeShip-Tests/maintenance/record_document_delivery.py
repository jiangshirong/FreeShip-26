"""Record the 2026-11-02 native document delivery; not a regression or updater.

Run from the workspace: python -B FreeShip-Tests/maintenance/record_document_delivery.py
Checks the complete installed package, source fingerprints and final reports.
Historical failed runs stay intact and are attributed separately in the receipt.
"""
import hashlib
import json
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import qt_migration as q


def read(path):
    return json.loads(path.read_text('utf-8-sig'))


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    installed = (ROOT.parent / 'FreeShip-Qt/dist').resolve(strict=True)
    previous = (ROOT.parent / 'FreeShip-Qt/build/dist-before-document-20261102').resolve(strict=True)
    base = Path('C:/FreeShip26Tests/reports')
    manifest_path = installed / 'build-manifest.json'
    manifest = read(manifest_path)
    q.validate_test_manifest(manifest)
    current = q.current_sources(q.QT)
    assert {k.replace('\\', '/'): v for k, v in manifest['source_sha256'].items()} == current
    expected_files = {k.replace('\\', '/'): v for k, v in manifest['binaries_sha256'].items()}
    actual_files = {}
    for path in installed.rglob('*'):
        assert installed in path.resolve().parents, 'Package artifact escapes installed root'
        if path.is_file() and path != manifest_path:
            actual_files[path.relative_to(installed).as_posix()] = digest(path)
    assert actual_files == expected_files, 'Installed inventory or hashes changed'
    legacy = q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy == manifest['legacy_source_sha256'] == '277a3c69392c86fe14c375e7dfc9c3197b4162e982673cbbcf4b4a2dfff0ab6b'
    bridge = read(installed / 'bridge/report.json')
    assert bridge['adapter_sha256'] == q.bridge_builder(q.QT).adapter_hashes()
    assert bridge['source_sha256'] == legacy
    assert bridge['build']['executable_sha256'] == digest(installed / 'bridge/FreeShipGeometryBridge.exe')
    assert manifest['package_script_sha256'] == digest(q.QT / 'package.ps1')
    expected_reports = {
        'qt-document-installed/qt-document': (1943, 2, 2),
        'qt-editing-document': (142, 0, 4),
        'qt-sections-document-final': (214, 0, 1),
        'qt-backgrounds-document': (214, 3, 4),
        'qt-advanced-document': (151, 3, 1),
        'qt-auxiliary-document': (151, 3, 1),
        'qt-display-document': (130, 3, 1),
        'qt-startup-document': (54, 0, 17),
    }
    reports = {}
    for name, wanted in expected_reports.items():
        path = base / name / 'report.json'
        data = read(path)
        counts = data['counts']
        assert counts['fail'] == counts['error'] == 0
        assert (counts['pass'], counts['review'], counts['not_run']) == wanted, (name, counts)
        assert not any(c['status'] in ('fail', 'error') for c in data['cases'])
        assert Path(data.get('runtime_path', data.get('deployment', data.get('installed')))).resolve() == installed
        if 'qt_sources_sha256' in data:
            assert data['qt_sources_sha256'] == current
        for file, h in data.get('test_sources_sha256', {}).items():
            assert digest(ROOT / file) == h, file
        if 'package_manifest' in data:
            assert data['package_manifest'] == manifest
        if 'workflow_reference' in data:
            assert digest(Path(data['workflow_reference']) / 'report.json') == data['workflow_reference_sha256']
        reports[name] = {'path': str(path), 'sha256': digest(path), 'status': data['status'], 'counts': counts}
        for key in ('elapsed_seconds', 'phase_seconds'):
            if key in data:
                reports[name][key] = data[key]
        child_ms = sum(m.get('production', {}).get('wall_ms', 0) for m in data.get('models', []))
        if child_ms:
            reports[name]['production_child_seconds_sum'] = child_ms / 1000
    core_path = base / 'qt-document-installed/qt-document/codec/report.json'
    codec = read(core_path)
    assert codec['status'] == 'pass' and codec['supplied_files'] == 80
    assert len(codec['cases']) == 146 and all(c['status'] == 'pass' for c in codec['cases'])
    section_reference_path = base / 'qt-sections-pre-document-reference/report.json'
    section_reference = read(section_reference_path)
    assert section_reference['counts'] == {'pass': 232, 'fail': 0, 'review': 0, 'not_run': 1, 'error': 0}
    assert section_reference['capture_source_sha256'] == digest(ROOT / 'maintenance/document_sections_reference.py')
    assert section_reference['package_manifest_sha256'] == digest(previous / 'build-manifest.json')
    diagnostics = {}
    for name, explanation in {
        'qt-document-a': 'Three idle-adapter recovery errors. Fixed by refreshing QProcess termination state before reuse; assertions unchanged.',
        'qt-document-b': 'Full core rerun after the recovery fix, before installation.',
        'qt-sections-document': '25 old-reference mismatches: preexisting added splines fields only. Original geometry exact. The final suite compares all bytes against the intact pre-document package, including splines.',
    }.items():
        path = base / name / 'report.json'
        data = read(path)
        diagnostics[name] = {'path': str(path), 'sha256': digest(path), 'counts': data['counts'],
                             'elapsed_seconds': data.get('elapsed_seconds'), 'explanation': explanation}
    now = datetime.now(timezone.utc)
    start = datetime.fromisoformat('2026-11-02T13:32:27+00:00')
    receipt = {
        'date': '2026-11-02', 'recorded_utc': now.isoformat(),
        'installed': str(installed), 'previous_installation': str(previous),
        'scope': 'C++ document authority, native FBM structural reader and byte-preserving save, guarded transactions, undo/redo, save points, stable IDs and adapter crash recovery.',
        'manifest_sha256': digest(manifest_path), 'verified_package_files': len(actual_files),
        'binaries_sha256': {n: digest(installed / n) for n in (
            'FreeShipQt.exe', 'FreeShipDocumentService.exe', 'qt_document_codec_tests.exe',
            'FreeShipNativeLinesplan.exe', 'bridge/FreeShipGeometryBridge.exe')},
        'legacy_source_sha256': legacy, 'bridge_build': 'C:/FreeShip26Tests/build/document-service-01',
        'reports': reports,
        'installed_counts': {k: sum(r['counts'][k] for r in reports.values())
                             for k in ('pass', 'fail', 'review', 'not_run', 'error')},
        'codec': {'path': str(core_path), 'sha256': digest(core_path), 'supplied_files': 80, 'pass': 146,
                  'note': 'Synthetic ordinals 0-44 are separate from the 18 versions represented by actual supplied files.'},
        'section_reference': {'path': str(section_reference_path), 'sha256': digest(section_reference_path),
                              'counts': section_reference['counts'], 'elapsed_seconds': section_reference['elapsed_seconds']},
        'retained_diagnostics': diagnostics,
        'time_accounting': {
            'first_observed_utc': start.isoformat(), 'receipt_utc': now.isoformat(),
            'elapsed_wall_seconds': (now - start).total_seconds(),
            'basis': 'Clock observations bracket this work, not an IDE activity trace. Core suite phases are measured. Independent installed suites ran concurrently; child duration sums are not total waiting time.',
            'reported_tool_wall_seconds': {'first_cpp_build': 6.43, 'recovery_fix_cpp_build': 5.14, 'package': 5.48},
            'unallocated': 'Reading, design, implementation, integration, diagnosis and tool interaction were not separately timed. Do not attribute the remaining elapsed time to test execution.'
        },
        'notes': [
            'Full FBM re-encoding after edits, geometry-edit algorithms, four-view mesh construction and professional computations still depend on Pascal. Native structural traversal and unchanged byte writing are not a complete native semantic encoder.',
            'Unknown trailing bytes are saved unchanged and editing is blocked to prevent adapter data loss. Some historical calculator layouts reuse version numbers and cannot be distinguished perfectly from truncation.',
            'The initial core recovery bug was introduced by this migration, not by the upstream project. Its failed report remains available.',
            'No test expectations were weakened for the section sidecar extension. A preserved pre-document executable supplied a fresh complete reference; all scene, DXF and section outputs then matched.',
            'Generic legacy limitation strings in older shared report templates predate native document ownership. Scope is defined by this receipt and NATIVE-DOCUMENT.md; Pascal remains the calculation/edit adapter.',
            'Hidden production Qt actions and screenshots are not physical desktop acceptance. Fourteen review and thirty-one not_run results remain explicit, including recent-action discovery and visual fidelity.',
            'Only adapter-process recovery is implemented here; whole-application crash recovery, persistent undo and restart caches remain separate work.',
            'The old source, QtPilot, user models and fs launcher are unchanged. Reports/builds are on C; reusable tests and one-time maintenance sources remain in FreeShip-Tests.'
        ],
        'maintenance_sources_sha256': {n: digest(ROOT / 'maintenance' / n) for n in (
            'promote_document.ps1', 'document_sections_reference.py', 'record_document_delivery.py')},
        'free_bytes_after': {d: shutil.disk_usage(d + ':/').free for d in ('C', 'D')}
    }
    target = ROOT / 'maintenance/document-installed-20261102.json'
    target.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps({'receipt': str(target), 'verified_package_files': len(actual_files),
                      'counts': receipt['installed_counts']}, ensure_ascii=False))


if __name__ == '__main__':
    main()
