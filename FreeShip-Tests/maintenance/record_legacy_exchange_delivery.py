"""Audit this installed exchange batch and retained reports; no application mutation."""
import argparse
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installed-report', type=Path, required=True)
    parser.add_argument('--regression-report', type=Path, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--delivery', choices=('legacy-exchange','multihull'), default='legacy-exchange')
    args = parser.parse_args()
    installed = (q.QT / 'dist').resolve(strict=True)
    backup = (q.QT / ('build/dist-before-'+args.delivery+'-20261104')).resolve(strict=True)
    manifest_path = installed / 'build-manifest.json'
    manifest = read(manifest_path)
    sources = q.current_sources(q.QT)
    assert sources == {n.replace('\\', '/'): h for n, h in manifest['source_sha256'].items()}
    q.validate_test_manifest(manifest)
    legacy = q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy == manifest['legacy_source_sha256'] == read(q.QT / 'docs/migration/BASELINE.json')['legacy_source_sha256']
    files = {}
    for path in installed.rglob('*'):
        assert installed in path.resolve().parents
        if path.is_file() and path != manifest_path:
            files[path.relative_to(installed).as_posix()] = digest(path)
    assert files == manifest['binaries_sha256']
    assert manifest['package_script_sha256'] == digest(q.QT / 'package.ps1')
    bridge = read(installed / 'bridge/report.json')
    assert bridge['source_sha256'] == legacy and bridge['adapter_sha256'] == q.bridge_builder(q.QT).adapter_hashes()
    assert digest(installed / 'bridge/FreeShipGeometryBridge.exe') == bridge['build']['executable_sha256']
    reports = {}
    for path in [args.installed_report, *args.regression_report]:
        data = read(path)
        assert data['counts']['fail'] == data['counts']['error'] == 0
        for name, value in data['test_sources_sha256'].items():
            assert digest(ROOT / name) == value, name
        tested = Path(data.get('runtime', data.get('runtime_path'))).resolve(strict=True)
        tested_manifest = read(tested / 'build-manifest.json')
        assert sources == {n.replace('\\', '/'): h for n, h in tested_manifest['source_sha256'].items()}
        if 'qt_sources_sha256' in data:
            assert data['qt_sources_sha256'] == sources
        if 'source_sha256' in data:
            assert data['source_sha256'] == legacy
        for name, value in tested_manifest['binaries_sha256'].items():
            if Path(name).suffix.lower() in ('.exe', '.dll'):
                assert digest(tested / name) == value == digest(installed / name), name
                if 'runtime_sha256' in data:
                    assert data['runtime_sha256'][name] == value, name
        if path == args.installed_report:
            assert tested == installed and data['suite'] == 'qt-legacy-exchange'
        reports[data['suite']] = dict(path=str(path.resolve()), sha256=digest(path), counts=data['counts'], tested_runtime=str(tested))
    assert set(reports) == {'qt-legacy-exchange', 'qt-curve-import', 'qt-exchange', 'qt-publication'}
    result = dict(created_utc=datetime.now(timezone.utc).isoformat(), status='partial',
        scope=('Michlet single/duplicated/modelled twin hulls, complete wave-domain parameters, explicit wave import origin and input-contract corrections.' if args.delivery=='multihull' else 'Part and wave append, static VRML import, part export, Michlet/AddMass single-hull input, rectangular offsets and crease DXF.'),
        installed=str(installed), previous_installation=str(backup),
        package_manifest_sha256=digest(manifest_path), verified_files=len(files),
        executable_sha256={n: h for n, h in files.items() if Path(n).suffix.lower() == '.exe'},
        legacy_source_sha256=legacy, reports=reports,
        counts={key: sum(item['counts'][key] for item in reports.values()) for key in ('pass', 'fail', 'review', 'not_run', 'error')},
        limits=['Six bundled VRML files are rejected for degenerate faces at the existing Single precision; no faces were silently dropped.',
                ('Michlet covers the three legacy hull modes; arbitrary different demihulls and receiving-solver execution are not covered. AddMass calculation/results workflow remains pending.' if args.delivery=='multihull' else 'Michlet/AddMass are symmetric single-hull input writers. Multihull placement and actual receiving solvers remain pending.'),
                'Hidden Qt callbacks/screenshots do not establish physical desktop, native file-picker, printer or complete old-UI acceptance.'],
        retained_sources=['qt_legacy_exchange.py', 'qt/exchange_tests.cpp', 'run_tests.py',
            'maintenance/finalize_legacy_exchange_package.py', 'maintenance/record_legacy_exchange_delivery.py'],
        free_bytes_after={drive: shutil.disk_usage(drive + ':/').free for drive in ('C', 'D')})
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps(dict(receipt=str(args.output), counts=result['counts'], verified_files=len(files)), ensure_ascii=False))


if __name__ == '__main__':
    main()
