"""One-time pre-document package reference capture; not a new regression suite.

The 2026-11-01 section reference predates the spline sidecar extension. Preserve
its failures, identify the extension, then run the intact pre-document package.
The normal installed suite compares every byte of that fresh reference, including
the extra spline data. No production source or historical expectation is edited.
Run: python -B FreeShip-Tests/maintenance/document_sections_reference.py
"""
import json
import shutil
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import qt_migration as q
from qt_sections_editing import PHASES


def read(path):
    return json.loads(path.read_text('utf-8-sig'))


def main():
    started = time.perf_counter()
    previous = ROOT.parent / 'FreeShip-Qt/build/dist-before-document-20261102'
    installed = ROOT.parent / 'FreeShip-Qt/dist'
    base = Path('C:/FreeShip26Tests/reports')
    out = base / 'qt-sections-pre-document-reference'
    out.mkdir(parents=True, exist_ok=False)
    report = q.Report()
    report.data['mode'] = 'preserved-pre-document-production-reference'
    report.data['limitations'][0] = 'This reference intentionally uses the pre-document Pascal state owner.'
    report.data['reference_executable'] = str(previous / 'FreeShipQt.exe')
    manifest = read(previous / 'build-manifest.json')
    report.data['package_manifest_sha256'] = q.common.digest_file(previous / 'build-manifest.json')
    report.data['capture_source_sha256'] = q.common.digest_file(Path(__file__))
    expected = {k.replace('\\', '/'): v for k, v in manifest['binaries_sha256'].items()}
    actual = {p.relative_to(previous).as_posix(): q.common.digest_file(p)
              for p in previous.rglob('*') if p.is_file() and p.name != 'build-manifest.json'}
    assert actual == expected, 'Preserved package inventory changed'
    report.case('preserved-package-manifest', True)
    renderer = 'FreeShipNativeLinesplan.exe'
    report.case('renderer-identical-before-document-migration',
                q.common.digest_file(previous / renderer) == q.common.digest_file(installed / renderer))
    failures = read(base / 'qt-sections-document/report.json')
    extensions = []
    for case in failures['cases']:
        if case['status'] != 'fail':
            continue
        a, b = read(Path(case['actual'])), read(Path(case['expected']))
        # This is diagnosis only. The subsequent installed check keeps full,
        # byte-for-byte equality and does not use this projection as its oracle.
        added = []
        for section in a['sections']:
            if 'splines' in section:
                added.append({'kind': section['kind'], 'index': section['index'],
                              'splines': section.pop('splines')})
        same = a == b and bool(added)
        report.case(case['id'] + '.only-preexisting-schema-extension', same)
        extensions.append({'case': case['id'], 'original_data_exact': same,
                           'extra_spline_sections': len(added)})
    report.data['old_reference_diagnosis'] = extensions
    legacy = q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    protected = {p: q.common.digest_file(p) for p in q.DEFAULT_MODELS}
    try:
        for i, source in enumerate(q.DEFAULT_MODELS, 1):
            folder = out / f'model-{i}'
            folder.mkdir()
            private = folder / 'input.fbm'
            shutil.copy2(source, private)
            production = q.run_child(report, f'model-{i}.production', previous / 'FreeShipQt.exe',
                                     ['--verify-sections', private], folder / 'production', timeout=300)
            report.data['models'].append({'model': str(source), 'sha256': protected[source],
                                          'production': production})
            for state in PHASES:
                for kind in ('scene', 'dxf', 'sections'):
                    path = Path(production['artifacts'][state + '_' + kind]).resolve(strict=True)
                    assert folder.resolve() in path.parents
                    report.case(f'model-{i}.{state}.{kind}.captured', path.stat().st_size > 0)
    except Exception as error:
        report.case('capture', status='error', reason=str(error))
    finally:
        report.case('legacy-source-preserved', q.common.fingerprint(q.common.DEFAULT_SOURCE)[0] == legacy)
        for path, digest in protected.items():
            report.case('input-preserved.' + path.name, q.common.digest_file(path) == digest)
        report.case('desktop-acceptance', status='not_run', reason='Hidden production callbacks only.')
        report.finish()
        report.data['elapsed_seconds'] = time.perf_counter() - started
        (out / 'report.json').write_text(json.dumps(report.data, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps({'status': report.data['status'], 'counts': report.data['counts'],
                      'report': str(out / 'report.json')}, ensure_ascii=False))
    return 1 if report.data['counts']['fail'] or report.data['counts']['error'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
