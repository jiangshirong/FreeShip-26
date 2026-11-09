"""Audit the camera/light installation; retained maintenance tool, not a regression.

Reads manifests, reports and protected source hashes, then writes a receipt.
Does not install, delete, run a desktop workflow or change user settings.
"""
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def passed(report, name):
    assert any(c['id'] == name and c['status'] == 'pass' for c in report['cases']), name


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('core', 'model-ui', 'installed', 'previous', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    installed = (q.QT / 'dist').resolve()
    manifest = read(installed / 'build-manifest.json')
    q.validate_test_manifest(manifest)
    sources = q.current_sources(q.QT)
    assert {k.replace('\\', '/'): v for k, v in manifest['source_sha256'].items()} == sources
    assert all(digest(installed / n) == h for n, h in manifest['binaries_sha256'].items())
    legacy = q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy == read(ROOT / 'maintenance/model-ui-installed-20261104.json')['source_sha256']
    prior = read(a.previous.resolve(strict=True) / 'build-manifest.json')
    prior_sources = {k.replace('\\', '/'): v for k, v in prior['source_sha256'].items()}
    computation = ('src/native/', 'src/core/', 'src/document/', 'bridge/')
    unchanged = {k: h for k, h in sources.items() if k.startswith(computation)}
    assert unchanged == {k: h for k, h in prior_sources.items() if k.startswith(computation)}
    reports, data = {}, {}
    counts = {key: 0 for key in ('pass', 'fail', 'review', 'not_run', 'error')}
    for name in ('core', 'model_ui', 'installed'):
        path = getattr(a, name).resolve(strict=True)
        r = read(path)
        data[name] = r
        assert r['counts']['fail'] == r['counts']['error'] == 0, name
        for file, h in r.get('test_sources_sha256', {}).items():
            assert digest(ROOT / file) == h, file
        runtime = Path(r['runtime']).resolve(strict=True)
        other = read(runtime / 'build-manifest.json')
        assert {k.replace('\\', '/'): v for k, v in other['source_sha256'].items()} == sources
        for exe in ('FreeShipQt.exe', 'FreeShipDocumentService.exe', 'FreeShipNativeLinesplan.exe',
                    'qt_camera_lighting_tests.exe', 'qt_model_ui_tests.exe'):
            assert digest(runtime / exe) == digest(installed / exe), (name, exe)
        reports[name] = dict(path=str(path), sha256=digest(path), counts=r['counts'])
        for key in counts:
            counts[key] += r['counts'][key]
    for name in ('core', 'installed'):
        r = data[name]
        for prefix in ('production', 'production-windows'):
            for case in ('opening-light-does-not-change-view', 'light-fields-show-actual-values',
                         'light-fields-update-current-view', 'context-toggle-syncs-open-dialog',
                         'reopening-restores-view-values', 'camera-and-light-are-view-local',
                         'image-export-includes-selected-lens-and-light', 'view-settings-do-not-edit-document',
                         'view-settings-keep-model', 'view-settings-keep-scene', 'view-settings-keep-dxf', 'fbm-byte-identical'):
                passed(r, prefix + '.' + case)
        for focal in (28, 50, 90, 130, 200):
            passed(r, 'windows.gpu-software-lens-' + str(focal))
        passed(r, 'windows.gpu-independent-flat-lambert-value')
        for case in ('protected-models-unchanged', 'source-stable', 'runtime-unchanged'):
            passed(r, case)
    for case in ('native.order-preserves-stable-geometry', 'native.reopen-preserves-face-layer-ownership',
                 'native.failed-publication-rolls-back-alpha', 'production.context-export-is-exact-current-view',
                 'production.placed-point-selected-in-all-views',
                 'production-flow.flow-tool-remains-active-after-document-update'):
        passed(data['model_ui'], case)
    assert Path(data['installed']['runtime']).resolve() == installed
    for prefix in ('production', 'production-windows'):
        run = data['installed'][prefix]
        assert Path(run['executed_path']).resolve() == installed / 'FreeShipQt.exe'
        assert run['executable_sha256'] == digest(installed / 'FreeShipQt.exe')
    record = dict(
        created_utc=datetime.now(timezone.utc).isoformat(), status='partial',
        scope='Five perspective lenses and per-view light controls, GPU/software rendering and current-view export',
        installed=str(installed), previous=str(a.previous.resolve()),
        package_manifest_sha256=digest(installed / 'build-manifest.json'), source_sha256=legacy,
        unchanged_computation_source_count=len(unchanged), reports=reports, counts=counts,
        audit_script_sha256=digest(Path(__file__)), limitations=[
            'Counts include repeated installed checks and shared regressions, not unique scenarios.',
            'Hidden Qt events and images do not certify desktop mouse, focus, native dialogs, printing or multi-monitor DPI.',
            'View settings are not stored in FBM or across restart.',
            'Gaussian curvature, zebra and developability shading remain pending.',
            'Language support belongs to the separate translation workstream.'])
    a.output.write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps(dict(receipt=str(a.output), counts=counts), ensure_ascii=False))


if __name__ == '__main__':
    main()
