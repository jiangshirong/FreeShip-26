"""Read-only save-protection delivery audit; does not build, install or prune."""
import argparse
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
from record_topology_delivery import ROOT, q, read, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('save', 'recovery', 'startup'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    installed = (q.QT / 'dist').resolve(strict=True)
    backup = (q.QT / 'build/dist-before-save-protection-20261104').resolve(strict=True)
    manifest = read(installed / 'build-manifest.json')
    sources = q.current_sources(q.QT)
    q.validate_test_manifest(manifest)
    assert sources == {n.replace('\\', '/'): h for n, h in manifest['source_sha256'].items()}
    legacy = q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    assert legacy == manifest['legacy_source_sha256'] == read(q.QT / 'docs/migration/BASELINE.json')['legacy_source_sha256']
    files = {f.relative_to(installed).as_posix(): digest(f) for f in installed.rglob('*')
             if f.is_file() and f.name != 'build-manifest.json'}
    assert files == manifest['binaries_sha256']
    reports = {}

    def passed(data, name):
        assert any(c['id'] == name and c['status'] == 'pass' for c in data['cases']), name

    for name in ('save', 'recovery', 'startup'):
        path = getattr(args, name)
        data = read(path)
        assert data['counts']['fail'] == data['counts']['error'] == 0
        for source, value in data['test_sources_sha256'].items():
            assert digest(ROOT / source) == value, source
        runtime = Path(data['installed'] if name == 'startup' else data['runtime']).resolve(strict=True)
        if name in ('save', 'startup'):
            assert runtime == installed
        tested = read(runtime / 'build-manifest.json')
        assert sources == {n.replace('\\', '/'): h for n, h in tested['source_sha256'].items()}
        for file, value in tested['binaries_sha256'].items():
            if Path(file).suffix.lower() in ('.exe', '.dll'):
                assert digest(runtime / file) == value == digest(installed / file), file
        if name == 'startup':
            passed(data, 'installed-source-and-file-manifest')
            passed(data, 'installed-files-unchanged')
            for child in data['production_runs']:
                assert Path(child['executed_path']).resolve() == installed / 'FreeShipQt.exe'
                assert child['executable_sha256'] == digest(installed / 'FreeShipQt.exe')
        else:
            assert data['qt_sources_sha256'] == sources and data['source_sha256'] == legacy
            passed(data, 'legacy-protected')
            if name == 'save':
                for case in ('core.failed-commit-rolls-back-existing-backup',
                             'core.second-editor-cannot-silently-overwrite',
                             'core.conflicted-editor-can-save-as',
                             'core.committed-before-confirmation-interruption',
                             'core.interruption-keeps-previous-file-and-recovery',
                             'core.reopen-after-confirmation-interruption',
                             'production.dirty-revisions-saved',
                             'production.reopened-backup.scene',
                             'production.reopened-backup.dxf', 'original-model-protected'):
                    passed(data, case)
            else:
                for case in ('save-commits-exact-fbm', 'normal-close-cancel-keeps-dirty-session-and-copy',
                             'normal-close-discard-cleans-its-copy', 'real-ship.latest-full-fbm-exact'):
                    passed(data, case)
        reports[name] = dict(path=str(path.resolve()), sha256=digest(path), counts=data['counts'])
    receipt = dict(
        created_utc=datetime.now(timezone.utc).isoformat(), status='partial',
        scope='Previous-version .Bak, conflicting-writer rejection, backup rollback, save confirmation interruption and installed startup.',
        installed=str(installed), previous_installation=str(backup), verified_files=len(files),
        package_manifest_sha256=digest(installed / 'build-manifest.json'), legacy_source_sha256=legacy,
        executable_sha256={n: h for n, h in files.items() if Path(n).suffix.lower() == '.exe'},
        reports=reports,
        counts={key: sum(report['counts'][key] for report in reports.values())
                for key in ('pass', 'fail', 'review', 'not_run', 'error')},
        limits=['Hidden production actions do not establish native desktop/file-picker acceptance.',
                'Unrelated writers ignoring the shared lock can race the final check and replacement.',
                'Target and backup are separate filesystem names; no hardware power-loss acceptance.'],
        free_bytes_after={drive: shutil.disk_usage(drive + ':/').free for drive in ('C', 'D')},
        audit_script_sha256=digest(Path(__file__)))
    args.output.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps(dict(receipt=str(args.output), counts=receipt['counts'], verified_files=len(files)), ensure_ascii=False))


if __name__ == '__main__':
    main()
