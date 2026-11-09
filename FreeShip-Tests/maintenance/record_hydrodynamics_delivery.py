"""Audit an installed Hydrodyn delivery. Explicit maintenance, not a regression.

Does not install, delete files, use desktop controls or modify user settings.
"""
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest
from record_viewport_delivery import passed


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('core', 'publication', 'installed', 'previous', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    installed = (q.QT / 'dist').resolve(strict=True)
    manifest = read(installed / 'build-manifest.json')
    q.validate_test_manifest(manifest)
    sources = q.current_sources(q.QT)
    assert {k.replace('\\', '/'): v for k, v in manifest['source_sha256'].items()} == sources
    assert all(digest(installed / n) == h for n, h in manifest['binaries_sha256'].items())
    legacy = q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy == read(ROOT / 'maintenance/shading-installed-20261104.json')['source_sha256']
    previous = read(a.previous.resolve(strict=True) / 'build-manifest.json')
    prior = {k.replace('\\', '/'): v for k, v in previous['source_sha256'].items()}
    def geometry(name):
        return name.startswith(('src/native/', 'src/core/', 'src/render/', 'bridge/')) or name in ('src/scene.cpp', 'src/gpu_fill.cpp', 'src/view.cpp')
    unchanged = {k: h for k, h in sources.items() if geometry(k)}
    assert unchanged == {k: h for k, h in prior.items() if geometry(k)}
    reports, data = {}, {}
    counts = {key: 0 for key in ('pass', 'fail', 'review', 'not_run', 'error')}
    for name in ('core', 'publication', 'installed'):
        path = getattr(a, name).resolve(strict=True)
        r = read(path)
        data[name] = r
        assert r['counts']['fail'] == r['counts']['error'] == 0, name
        for file, h in r.get('test_sources_sha256', {}).items():
            assert digest(ROOT / file) == h, file
        runtime = Path(r['runtime']).resolve(strict=True)
        other = read(runtime / 'build-manifest.json')
        assert {k.replace('\\', '/'): v for k, v in other['source_sha256'].items()} == sources
        for exe in ('FreeShipQt.exe', 'FreeShipDocumentService.exe', 'FreeShipNativeLinesplan.exe'):
            assert digest(runtime / exe) == digest(installed / exe), (name, exe)
        reports[name] = dict(path=str(path), sha256=digest(path), counts=r['counts'])
        for key in counts:
            counts[key] += r['counts'][key]
    for name in ('core', 'installed'):
        r = data[name]
        assert r['source_sha256'] == legacy
        for method in ('wind', 'maneuver', 'acceleration'):
            for case in ('native', 'settings', 'independent-fbm-field', 'undo', 'fbm-roundtrip'):
                passed(r, method + '.' + case)
            for case in ('calculation-delivered', 'unchanged-scene', 'unchanged-dxf', 'csv', 'pdf'):
                passed(r, 'production.' + method + '.' + case)
        for case in ('wind.yaw-length-factor', 'wind.relative-vector', 'wind.port-starboard',
                     'calculations.no-pascal', 'calculations.readonly', 'acceleration.extract-volume-not-mass',
                     'solver.running-cancel', 'solver.cancel-child-exited', 'acceleration.old-revision-rejected',
                     'production.preset.parameter-file-roundtrip', 'production.fixture-db.database-viewer-exit'):
            passed(r, case)
        for i in range(5):
            passed(r, 'production.acceleration.table-full-series-' + str(i))
        for i in range(1, 4):
            passed(r, 'acceleration.' + str(i) + '.raw-curves')
        for run in r['production'].values():
            runtime = Path(r['runtime']).resolve(strict=True)
            assert Path(run['executed_path']).resolve() == runtime / 'FreeShipQt.exe'
            assert run['executable_sha256'] == digest(installed / 'FreeShipQt.exe')
    assert Path(data['installed']['runtime']).resolve() == installed
    for i in (1, 2, 3):
        for case in ('complete-snapshot-exact', 'worker-input-exact', 'scene-exact', 'dxf-exact'):
            passed(data['publication'], f'model-{i}.{case}')
    solver_dir = ROOT / 'fixtures/solvers35/unpacked/Freeship/Exec'
    record = dict(created_utc=datetime.now(timezone.utc).isoformat(), status='partial',
        scope='Three Hydrodyn workflows and Qt dialogs, full result table, FBM settings, database viewer launcher',
        installed=str(installed), previous=str(a.previous.resolve()), source_sha256=legacy,
        package_manifest_sha256=digest(installed / 'build-manifest.json'), unchanged_geometry_sources=len(unchanged),
        changed_sources=sorted(k for k, h in sources.items() if prior.get(k) != h),
        external_solvers={n: digest(solver_dir / n) for n in ('Ishercof.exe', 'ManeuvPP.EXE', 'RVRSSHIP.EXE', 'PropPred.EXE')},
        reports=reports, counts=counts, audit_script_sha256=digest(Path(__file__)), limitations=[
            'Counts include repeated installed checks, not unique scenarios.',
            'Type 13 wind coefficient and engine inertia units retain unresolved legacy-source conflicts.',
            'Original external solvers remain dependencies; protocol checks are not engineering certification.',
            'Real DBFview.EXE is unavailable; the isolated process fixture is not database acceptance.',
            'Full dialog visual fidelity and real desktop input/printing are not certified.',
            'Other migration directions remain. Language support belongs to the separate translation model.'])
    a.output.write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps(dict(receipt=str(a.output), counts=counts), ensure_ascii=False))


if __name__ == '__main__':
    main()
