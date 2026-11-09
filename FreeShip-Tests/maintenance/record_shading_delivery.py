"""Installation identity audit, not a regression. Does not install or remove files.

Run after promotion and installed qt-shading. Retains measured edge differences
as review; no geometry or pixel tolerance is changed by this receipt writer.
"""
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest
from record_viewport_delivery import passed


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('core', 'viewport', 'publication', 'installed', 'previous', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    installed = (q.QT / 'dist').resolve()
    manifest = read(installed / 'build-manifest.json')
    q.validate_test_manifest(manifest)
    sources = q.current_sources(q.QT)
    assert {k.replace('\\', '/'): v for k, v in manifest['source_sha256'].items()} == sources
    assert all(digest(installed / n) == h for n, h in manifest['binaries_sha256'].items())
    legacy = q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy == read(ROOT / 'maintenance/viewport-installed-20261104.json')['source_sha256']
    previous = read(a.previous.resolve(strict=True) / 'build-manifest.json')
    prior_sources = {k.replace('\\', '/'): v for k, v in previous['source_sha256'].items()}
    def numerical(name):
        return (name.startswith(('src/native/', 'src/document/', 'bridge/')) and name != 'src/native/surface_display.cpp') or name in ('src/scene.cpp', 'src/gpu_fill.cpp', 'src/view.cpp')
    unchanged = {k: h for k, h in sources.items() if numerical(k)}
    assert unchanged == {k: h for k, h in prior_sources.items() if numerical(k)}
    reports, data = {}, {}
    counts = {key: 0 for key in ('pass', 'fail', 'review', 'not_run', 'error')}
    for name in ('core', 'viewport', 'publication', 'installed'):
        path = getattr(a, name).resolve(strict=True)
        r = read(path)
        data[name] = r
        assert r['counts']['fail'] == r['counts']['error'] == 0, name
        for file, h in r.get('test_sources_sha256', {}).items():
            assert digest(ROOT / file) == h, file
        runtime = Path(r['runtime']).resolve(strict=True)
        other = read(runtime / 'build-manifest.json')
        assert {k.replace('\\', '/'): v for k, v in other['source_sha256'].items()} == sources
        executables = ['FreeShipQt.exe', 'FreeShipDocumentService.exe', 'FreeShipNativeLinesplan.exe']
        if name in ('core', 'installed'):
            executables.append('qt_shading_tests.exe')
        elif name == 'viewport':
            executables.append('qt_camera_lighting_tests.exe')
        for exe in executables:
            assert digest(runtime / exe) == digest(installed / exe), (name, exe)
        reports[name] = dict(path=str(path), sha256=digest(path), counts=r['counts'])
        for key in counts:
            counts[key] += r['counts'][key]
    for name in ('core', 'installed'):
        r = data[name]
        for prefix in ('production-offscreen', 'production-windows'):
            for case in ('analysis-is-lazy-on-open', 'modes-do-not-edit-document', 'edit-rebuilds-analysis', 'undo-restores-analysis',
                         'read-only-model', 'read-only-scene', 'read-only-dxf', 'fbm-after-undo', 'undo-restores-scene', 'undo-restores-dxf'):
                passed(r, prefix + '.' + case)
            for mode in (1, 2, 3):
                passed(r, prefix + '.mode-export-' + str(mode))
        for case in ('flat-interior-angle-deficit-zero', 'saddle-angle-deficit-minus-120', 'closed-sphere-total-angle-deficit-720',
                     'cylinder-interior-is-developable', 'coincident-disconnected-vertices-stay-distinct', 'nonmanifold-edge-is-unknown',
                     'tampered-topology-rejected', 'gpu-independent-vertex-color-interpolation'):
            passed(r, 'windows.' + case)
        for mode in (1, 2, 3):
            passed(r, 'windows.actual-gpu-cpu-interiors-' + str(mode))
    for i in (1, 2, 3):
        for case in ('complete-snapshot-exact', 'worker-input-exact', 'scene-exact', 'dxf-exact'):
            passed(data['publication'], f'model-{i}.{case}')
    assert Path(data['installed']['runtime']).resolve() == installed
    for prefix in ('production-offscreen', 'production-windows'):
        run = data['installed'][prefix]
        assert Path(run['executed_path']).resolve() == installed / 'FreeShipQt.exe'
        assert run['executable_sha256'] == digest(installed / 'FreeShipQt.exe')
    record = dict(created_utc=datetime.now(timezone.utc).isoformat(), status='partial',
        scope='Gaussian angle-deficit display, developability and zebra shading, indexed topology, lazy shared analysis and actual exports',
        installed=str(installed), previous=str(a.previous.resolve()), source_sha256=legacy,
        package_manifest_sha256=digest(installed / 'build-manifest.json'), unchanged_numerical_sources=len(unchanged),
        reports=reports, counts=counts, audit_script_sha256=digest(Path(__file__)), limitations=[
            'Counts include repeated installed checks and reused tests, not unique scenarios.',
            'Shared raster edge color differences remain review; full locations and device subpixel precision are retained.',
            'Hidden events and screenshots do not certify desktop mouse, native dialogs, multi-monitor DPI or physical printing.',
            'Angle deficit is not area-normalized curvature; unknown regions are not certified developable.',
            'Dedicated tool windows and other migration/visual work remain. Language work belongs to the separate translation model.'])
    a.output.write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps(dict(receipt=str(a.output), counts=counts), ensure_ascii=False))


if __name__ == '__main__':
    main()
