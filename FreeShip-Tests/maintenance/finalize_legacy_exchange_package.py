"""Refresh documentation/metadata in the already tested candidate, without rebuilding.

Retained delivery maintenance, not a test or an updater. Only the candidate is
written. Executables, bridge resources and existing installation remain intact.
"""
import argparse
import json
import shutil
import stat
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--bridge-build', type=Path, required=True)
    args = parser.parse_args()
    candidate = args.candidate.resolve(strict=True)
    assert candidate in {Path('D:/FreeShip26Tests/exchange-package').resolve(), Path('C:/FreeShip26-MultihullTests/runtime').resolve()}
    for path in [args.candidate, *candidate.rglob('*')]:
        assert not (path.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT), path
    manifest_path = candidate / 'build-manifest.json'
    manifest = read(manifest_path)
    assert q.current_sources(q.QT) == {n.replace('\\', '/'): h for n, h in manifest['source_sha256'].items()}
    q.validate_test_manifest(manifest)
    for name, value in manifest['binaries_sha256'].items():
        path = (candidate / name).resolve(strict=True)
        assert candidate in path.parents and digest(path) == value, name
    bridge = read(candidate / 'bridge/report.json')
    assert bridge == read(args.bridge_build / 'report.json') and bridge['status'] == 'pass'
    assert bridge['adapter_sha256'] == q.bridge_builder(q.QT).adapter_hashes()
    legacy = q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy == bridge['source_sha256'] == read(q.QT / 'docs/migration/BASELINE.json')['legacy_source_sha256']
    assert digest(candidate / 'bridge/FreeShipGeometryBridge.exe') == bridge['build']['executable_sha256']
    for name in ('COPYING', 'NOTICE.md', 'README.md'):
        shutil.copy2(q.QT / name, candidate / name)
    shutil.copytree(q.QT / 'docs', candidate / 'docs', dirs_exist_ok=True)
    shutil.copytree(q.QT / 'third_party', candidate / 'third_party', dirs_exist_ok=True)
    # Preserve the user's existing local solver reference, never copy solvers.
    config = q.QT / 'dist/calculation-tools.json'
    if config.exists():
        shutil.copy2(config, candidate / config.name)
    manifest.update(
        legacy_source_sha256=legacy,
        bridge_build=str(args.bridge_build.resolve(strict=True)),
        package_script_sha256=digest(q.QT / 'package.ps1'),
        finalized_utc=datetime.now(timezone.utc).isoformat(),
        finalizer_sha256=digest(Path(__file__)),
        package_scope='Local development build; public release compliance and native desktop acceptance remain pending.')
    manifest['binaries_sha256'] = {p.relative_to(candidate).as_posix(): digest(p)
        for p in sorted(candidate.rglob('*')) if p.is_file() and p.name != 'build-manifest.json'}
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps(dict(candidate=str(candidate), files=len(manifest['binaries_sha256'])), ensure_ascii=False))


if __name__ == '__main__':
    main()
