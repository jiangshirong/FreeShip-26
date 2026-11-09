"""Surface analysis: analytic shapes, true production actions and unchanged export.

Hidden Qt runs only. Angle deficit is the legacy display measure in degrees;
it is not an engineering certificate of plate developability.
"""
import argparse
import json
import shutil
import time
from pathlib import Path
import qt_migration as q
import qt_editing as editing


def read(path):
    return json.loads(Path(path).read_text('utf8'))


def run(r, runtime, out):
    model = out / 'private-model.fbm'
    shutil.copy2(q.DEFAULT_MODELS[1], model)
    with q.Service(runtime / 'FreeShipDocumentService.exe', out / 'service', native_linesplan=True) as s:
        response = q.require_ok(s.command('load', path=str(model), document_id=str(q.uuid.uuid4()), output=out / 'load'))
        for platform in ('offscreen', 'windows'):
            r.data.setdefault('render_runs', []).append(q.run_child(r, platform, runtime / 'qt_shading_tests.exe',
                [response['model'], response['surface']], out / platform, platform=platform))
    for platform in ('offscreen', 'windows'):
        prefix = 'production-' + platform
        result = q.run_child(r, prefix, runtime / 'FreeShipQt.exe', ['--verify-shading', model], out / prefix, platform=platform)
        r.data[prefix] = result
        artifacts = result.get('artifacts', {})
        if 'saved_file' not in artifacts:
            r.case(prefix + '.postconditions', status='not_run', reason='Production sequence incomplete; inspect discovery and earlier errors.')
            continue
        for name in ('model', 'scene', 'dxf'):
            r.compare(prefix + '.read-only-' + name, artifacts['after_' + name], artifacts['before_' + name])
        r.compare(prefix + '.fbm-after-undo', artifacts['saved_file'], model)
        for name in ('scene', 'dxf'):
            r.compare(prefix + '.undo-restores-' + name, artifacts['restored_' + name], artifacts['before_' + name])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', type=Path)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    runtime = (a.runtime or q.QT / 'dist').resolve()
    out = a.output.resolve()
    for protected in (q.QT, q.PILOT, q.common.DEFAULT_SOURCE, q.ROOT):
        if out == protected or protected in out.parents:
            p.error('Use private output')
    out.mkdir(parents=True, exist_ok=False)
    r = q.Report()
    r.data.update(suite='qt-shading', runtime=str(runtime), limitations=[
        'Angle deficit follows the legacy degree measure, not area-normalized Gaussian curvature.',
        'Open boundaries, nonmanifold fans and degenerate inputs are unknown, not certified developable.',
        'Hidden events and images are not desktop, native dialog or physical printing acceptance.',
        'Image timings include readback and do not measure screen frame rate.'])
    sources = q.current_sources(q.QT)
    protected = {path: q.common.digest_file(path) for path in q.DEFAULT_MODELS}
    r.data['test_sources_sha256'] = {n: q.common.digest_file(q.ROOT / n) for n in
        ('qt_shading.py', 'qt/shading_tests.cpp', 'qt_migration.py', 'run_tests.py')}
    start = time.perf_counter()
    try:
        manifest = read(runtime / 'build-manifest.json')
        q.validate_test_manifest(manifest)
        if {k.replace('\\', '/'): v for k, v in manifest['source_sha256'].items()} != sources:
            raise RuntimeError('Stale runtime source manifest')
        if any(q.common.digest_file(runtime / n) != h for n, h in manifest['binaries_sha256'].items()):
            raise RuntimeError('Runtime hash mismatch')
        r.case('runtime-manifest', True)
        run(r, runtime, out)
        r.case('protected-models-unchanged', all(q.common.digest_file(path) == h for path, h in protected.items()))
        r.case('source-stable', q.current_sources(q.QT) == sources)
        r.case('runtime-unchanged', all(q.common.digest_file(runtime / n) == h for n, h in manifest['binaries_sha256'].items()))
    except Exception as error:
        r.case('execution', status='error', reason=f'{type(error).__name__}: {error}')
    r.data['elapsed_s'] = time.perf_counter() - start
    editing.write_report(r, out)
    print(json.dumps(dict(counts=r.data['counts'], report=str(out / 'report.json'))))
    return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))


if __name__ == '__main__':
    raise SystemExit(main())
