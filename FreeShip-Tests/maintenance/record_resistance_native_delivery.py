"""Check the installed resistance migration and retain its completed evidence.

This maintenance command reads completed reports and package manifests. It does
not run tests, deploy a package, or turn review/not_run into passing coverage.
"""
import argparse
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    names = ('core', 'hydro', 'installed_workflow', 'startup')
    for name in names:
        parser.add_argument('--' + name.replace('_', '-'), type=Path, required=True)
    parser.add_argument('--output', type=Path,
                        default=ROOT/'maintenance/resistance-native-installed-20261103.json')
    args = parser.parse_args()
    installed = (q.QT/'dist').resolve(strict=True)
    previous = (q.QT/'build/dist-before-resistance-native-20261103').resolve(strict=True)
    manifest = read(installed/'build-manifest.json')
    sources = q.current_sources(q.QT)
    q.validate_test_manifest(manifest)
    assert {k.replace('\\', '/'): v for k, v in manifest['source_sha256'].items()} == sources
    files = {}
    for path in installed.rglob('*'):
        assert installed in path.resolve().parents
        if path.is_file() and path.name != 'build-manifest.json':
            files[path.relative_to(installed).as_posix()] = digest(path)
    assert files == {k.replace('\\', '/'): v for k, v in manifest['binaries_sha256'].items()}
    assert manifest['package_script_sha256'] == digest(q.QT/'package.ps1')
    legacy = q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy == manifest['legacy_source_sha256'] == '277a3c69392c86fe14c375e7dfc9c3197b4162e982673cbbcf4b4a2dfff0ab6b'
    bridge = read(installed/'bridge/report.json')
    assert bridge['source_sha256'] == legacy
    assert bridge['adapter_sha256'] == q.bridge_builder(q.QT).adapter_hashes()
    assert bridge['build']['executable_sha256'] == digest(installed/'bridge/FreeShipGeometryBridge.exe')

    reports, data = {}, {}
    for name in names:
        path = getattr(args, name)
        report = read(path)
        data[name] = report
        assert report['counts']['fail'] == report['counts']['error'] == 0, name
        for key, value in report.get('test_sources_sha256', {}).items():
            assert digest(ROOT/key) == value, key
        reports[name] = dict(path=str(path.resolve()), sha256=digest(path), counts=report['counts'])
    core, hydro, workflow, startup = (data[name] for name in names)
    assert core['qt_sources_sha256'] == sources
    binaries = ('FreeShipQt.exe', 'FreeShipDocumentService.exe', 'FreeShipNativeLinesplan.exe',
                'FreeShipGeometryExport.exe', 'bridge/FreeShipGeometryBridge.exe')
    for name in binaries:
        assert digest(Path(core['runtime_path'])/name) == digest(installed/name), name

    def passed(report, case):
        assert any(row['id'] == case and row['status'] == 'pass' for row in report['cases']), case

    methods = ('delft', 'holtrop', 'hollenbach', 'oortmerssen', 'fung',
               'ost', 'rbhs', 'mh', 'kaper', 'planing')
    required = ['delft.independent-power-law', 'estimated-area.independent-formula',
                'powerprediction.direct-output', 'resistance.cache.published',
                'resistance.cache.failure-invalidates', 'resistance.cache.no-stale-propeller',
                'resistance.cache.planing-failure-invalidates',
                'resistance.cancel-running', 'resistance.cancel-child-exited',
                'resistance.detached.formula']
    required += [method + '.curves' for method in methods]
    required += [f'resistance.{prefix}.{method}' for method in methods
                 for prefix in ('forbid-old', 'detached')]
    required += ['resistance.solver.' + mode
                 for mode in ('missing', 'truncated', 'nonfinite', 'extra', 'exit')]
    from maintenance import port_resistance
    generated, _ = port_resistance.generate()
    required += ['resistance.provenance.' + name for name in generated]
    for case in required:
        passed(core, case)
    for case in ('estimated-area.old-draft-order', 'powerprediction.old-eof-protocol'):
        assert any(row['id'] == case and row['status'] == 'review' for row in core['cases']), case
    for i in range(len(q.DEFAULT_MODELS) + 1):
        for suffix in ('native-defaults', 'cached', 'invalidated', 'undo', 'without-pascal'):
            passed(hydro, f'hydro.{i}.{suffix}')

    assert Path(workflow['deployment']).resolve() == installed
    assert workflow['workflow_reference_sha256'] == digest(args.core)
    assert workflow['package_manifest'] == manifest
    for case in ('package-current-source-test-and-binary-manifest', 'actual-deployment-files-unchanged',
                 'model-1.deployment.performance-equals-tested-runtime',
                 'model-1.deployment.performance-csv-equals-tested-runtime'):
        passed(workflow, case)
    for case in ('installed-source-and-file-manifest', 'installed-files-unchanged'):
        passed(startup, case)
    assert Path(startup['installed']).resolve() == installed
    for child in [*[model['production'] for model in workflow['models']], *startup['production_runs']]:
        assert Path(child['executed_path']).resolve() == installed/'FreeShipQt.exe'
        assert child['executable_sha256'] == digest(installed/'FreeShipQt.exe')
    actual = workflow['models'][0]['production']['artifacts']
    assert actual['effective_inputs']['calculator'] == 'holtrop'
    assert read(Path(actual['report_json']))['calculator'] == 'holtrop'

    receipt = dict(
        recorded_utc=datetime.now(timezone.utc).isoformat(),
        scope='Ten native C++ resistance workflows, existing Qt dialogs and original external solvers.',
        installed=str(installed), previous_installation=str(previous),
        manifest_sha256=digest(installed/'build-manifest.json'), verified_package_files=len(files),
        legacy_source_sha256=legacy, binaries_sha256={name: digest(installed/name) for name in binaries},
        reports=reports,
        combined_counts={key: sum(row['counts'][key] for row in reports.values())
                         for key in ('pass', 'fail', 'review', 'not_run', 'error')},
        core_elapsed_seconds=core['elapsed_seconds'],
        notes=[
            'Ten primary resistance curves and available series branches match the isolated original exactly; no tolerance was widened.',
            'Parameter extraction, saved values, imperial inputs, protocol failures, cancellation and revision-scoped cache invalidation are covered.',
            'Delft power and estimated wetted area have independent formula checks; PowerPrd output also matches a direct process invocation.',
            'Eight resistance branch samples have no original numerical result: three Oortmerssen-family cases and five unavailable MH series. These remain not_run.',
            'The original uninitialized draft and PowerPrd EOF-wait paths remain review, with separately verified native repairs.',
            'CLEMBLOU emits NaN in long working directories: four short and four long independent runs reproduced the difference with identical inputs. Native uses a short private workspace; the original curve comparison is staged in a short path too, without retries or skipped coordinates.',
            'Hydrostatics was rerun after adding entrance angle, bulb-section and waterline-bound fields. Its diagnostic report is retained separately.',
            'The full performance suite also covers all five previous propulsion workflows; its external CALCPROP limitations remain explicit.',
            'The installed Holtrop Qt callback, report/CSV and geometry are compared with the tested package; startup uses private models and preferences.',
            'Hidden production callbacks are not actual desktop acceptance. Remaining Pascal modules and full visual parity are outside this delivery.',
            'Generators and test sources are retained in FreeShip-Tests. Current outputs are on D following the user correction; historical C evidence paths are junctions. External solver binaries are not redistributed.'
        ],
        maintenance_sources_sha256={name: digest(ROOT/'maintenance'/name) for name in
                                   ('record_resistance_native_delivery.py', 'record_topology_delivery.py', 'promote_runtime.ps1')},
        free_bytes_after={drive: shutil.disk_usage(drive + ':/').free for drive in ('C', 'D')})
    args.output.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n', 'utf8')
    print(json.dumps(dict(receipt=str(args.output), counts=receipt['combined_counts'],
                          verified_package_files=len(files)), ensure_ascii=False))


if __name__ == '__main__':
    main()
