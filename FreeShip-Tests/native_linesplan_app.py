"""Native complete-scene integration: isolated production service and Qt workflows.

DXF numbers are compared as exact Decimal values (no rounding or tolerance).
Scene coordinates, labels, colors and primitive counts are inspected separately.
The old plan-waterline projection defect is not a valid geometry expectation.
"""
from pathlib import Path
from collections import Counter
from decimal import Decimal
import struct
import argparse
import copy
import json
import os
import shutil
import subprocess
import sys
import time
import uuid

import qt_migration as q
import qt_editing as editing
import qt_editing_fixture as fixture

ROOT = Path(__file__).resolve().parent


def read_scene(path):
    data = path.read_bytes()
    if data[:8] != b'FSLP0001':
        raise ValueError('Unsupported scene')
    bounds = struct.unpack_from('<4d', data, 8)
    offset, items = 40, []
    while offset < len(data):
        kind, color, count = struct.unpack_from('<III', data, offset)
        offset += 12
        points = tuple(struct.unpack_from('<2d', data, offset + i * 16) for i in range(count))
        offset += count * 16
        pixels, text = None, None
        if kind == 3:
            pixels, length = struct.unpack_from('<II', data, offset)
            offset += 8
            text = data[offset:offset + length].decode('utf8')
            offset += length
        items.append((kind, color, points, pixels, text))
    if offset != len(data):
        raise ValueError('Truncated scene')
    return bounds, items


def numeric_entities(path):
    """Same complete-entity comparison as dxf_geometry, only numeric spelling normalized."""
    import dxf_geometry
    def normalize(value):
        if isinstance(value, tuple):
            if len(value) == 2 and value[0] in ('10', '20', '30', '11', '21', '31') and isinstance(value[1], str):
                return value[0], Decimal(value[1])
            return tuple(normalize(v) for v in value)
        return value
    # Canonicalize traversal after numeric normalization too: lexical text order
    # can differ solely because of appended zeros.
    result = Counter()
    for entity, count in dxf_geometry.entities(path).items():
        value = normalize(entity)
        if value[0] == 'POLYLINE':
            seq = value[-1]
            if value[-2] and seq:
                seq = min(s[i:] + s[:i] for s in (seq, tuple(reversed(seq))) for i in range(len(seq)))
            else:
                seq = min(seq, tuple(reversed(seq)))
            value = value[:-1] + (seq,)
        result[value] += count
    return result


def compare_artifacts(old, new):
    a, b = [numeric_entities(p / 'reference.dxf') for p in (old, new)]
    ab, ai = read_scene(old / 'scene.fslp')
    bb, bi = read_scene(new / 'scene.fslp')
    result = {'dxf_exact': a == b, 'dxf_missing': sum((a-b).values()), 'dxf_extra': sum((b-a).values()),
              'bounds': [ab, bb], 'counts': [dict(Counter(p[0] for p in items)) for items in (ai, bi)]}
    for kind in (1, 2, 3):
        first, second = [Counter(p for p in items if p[0] == kind) for items in (ai, bi)]
        result[str(kind)] = {'missing': sum((first-second).values()), 'extra': sum((second-first).values()),
            'examples': [list((first-second).keys())[:2], list((second-first).keys())[:2]]}
    return result


def subsequence(small, large):
    remaining = iter(large)
    return all(any(x == y for y in remaining) for x in small)


def reopened_geometry(model):
    """A new FBM load assigns new IDs; compare exact attributed topology instead.

    Unique point coordinates are required for the ID mapping in these fixtures.
    Coincident points are not silently merged or considered equivalent.
    """
    result=copy.deepcopy(q.geometry(model))
    point_keys={p['id']:tuple(p['p']) for p in result['control_points']}
    if len(set(point_keys.values()))!=len(point_keys):
        raise ValueError('Reopen comparison needs an unambiguous point mapping')
    def without_id(value):return {k:v for k,v in value.items() if k!='id'}
    normalized_points=[]
    for point in result['control_points']:
        point=without_id(point)
        for field in ('anchor','line_a','line_b'):
            if field in point:point[field]=point_keys[point[field]] if point[field] else None
        normalized_points.append(point)
    result['control_points']=sorted(normalized_points,key=lambda p:tuple(p['p']))
    result['control_edges']=sorted([
        {**without_id(e),'a':min(point_keys[e['a']],point_keys[e['b']]),'b':max(point_keys[e['a']],point_keys[e['b']])}
        for e in result['control_edges']],key=lambda e:json.dumps(e,sort_keys=True))
    faces=[]
    for face in result['control_faces']:
        points=tuple(point_keys[p] for p in face['points'])
        # A polygon may start at a different vertex; its winding is retained.
        points=min(points[i:]+points[:i] for i in range(len(points)))
        faces.append({**without_id(face),'points':points})
    result['control_faces']=sorted(faces,key=lambda f:json.dumps(f,sort_keys=True))
    return result


def compare_scene(report, prefix, old_path, new_path):
    """Strict geometry with explicitly identified display-only migration changes.

    Never treats arbitrary numerical closeness as equality. Added display corners
    must retain every old sample in order. The known old XY defect must match its
    exact wrong formula; new Y samples must retain the old ones in order too.
    Those corrections remain review, with full raw evidence, not forced pass.
    """
    ab, a = read_scene(old_path)
    bb, b = read_scene(new_path)
    report.case(prefix+'.bounds-exact', ab == bb, old=ab, native=bb)
    fills = [[p for p in items if p[0] == 1] for items in (a, b)]
    report.case(prefix+'.ordered-fill-exact', fills[0] == fills[1], count=[len(v) for v in fills])
    labels = [[p for p in items if p[0] == 3] for items in (a, b)]
    report.case(prefix+'.label-content-order-size-color-exact',
                [(p[1], p[3], p[4]) for p in labels[0]] == [(p[1], p[3], p[4]) for p in labels[1]])
    if labels[0] != labels[1]:
        report.case(prefix+'.label-anchors',status='review',
                    reason='Qt text metrics and continuous world anchors replace the old 1280x800 integer projection; label content, order, size and color checked separately.')
    first, second = [Counter(p for p in items if p[0] == 2) for items in (a, b)]
    missing, extra = list((first-second).elements()), list((second-first).elements())
    diagnostics = json.loads((new_path.parent/'native-report.json').read_text('utf8'))
    origin = diagnostics['origins'][3][1]
    changes, unknown = [], []
    for old in missing:
        kind = None
        match = next((p for p in extra if p[1] == old[1] and len(p[2]) > len(old[2]) and
                      p[2][0] == old[2][0] and p[2][-1] == old[2][-1] and subsequence(old[2], p[2])), None)
        if match is not None:
            kind = 'retained-samples-added-knuckles'
        elif len(old[2]) > 2 and all(editing.f32(origin+x) == y for x,y in old[2]):
            match = next((p for p in extra if p[1] == old[1] and len(p[2]) >= len(old[2]) and
                          subsequence(tuple(y for x,y in old[2]), tuple(y for x,y in p[2]))), None)
            if match is not None:
                kind = 'corrected-plan-xy-projection'
        if match is None:
            unknown.append(old)
        else:
            extra.remove(match)
            changes.append({'kind': kind, 'old': old, 'native': match})
    report.case(prefix+'.other-strokes-exact', not unknown and not extra,
                unmatched_old=len(unknown), unmatched_native=len(extra))
    if changes:
        report.case(prefix+'.display-curve-corrections', status='review', count=len(changes),
                    kinds=dict(Counter(c['kind'] for c in changes)),
                    reason='Known projection/corner corrections, independently covered by qt-native-linesplan and its API tests; complete DXF is checked without this allowance.')
    evidence = new_path.parent/(prefix.replace('.', '-')+'-scene-differences.json')
    evidence.write_text(json.dumps({'changes': changes, 'unknown_old':unknown, 'unknown_native':extra,
                                   'old_labels':labels[0], 'native_labels':labels[1]},ensure_ascii=False),encoding='utf8')


def capture(report, prefix, service, directory, method='snapshot', **fields):
    if method == 'load': fields.setdefault('document_id', str(uuid.uuid4()))
    response = q.require_ok(service.command(method, output=directory, **fields))
    result = q.validate_snapshot(report, prefix, response, directory)
    if service.native_linesplan:
        metadata = json.loads((directory/'native-report.json').read_text('utf8'))
        report.case(prefix+'.native-revision-artifacts-bound', metadata['engine']=='cpp-native' and
                    metadata['status']=='pass' and metadata['document_id']==response['document_id'] and
                    metadata['revision']==response['revision'] and
                    metadata['scene_sha256']==q.common.digest_file(directory/'scene.fslp') and
                    metadata['dxf_sha256']==q.common.digest_file(directory/'reference.dxf'))
    return result


def worker(runtime, source, output, cache=None, parent=None):
    output.mkdir(parents=True,exist_ok=False)
    command=[str(runtime/'FreeShipNativeLinesplan.exe'),str(source),str(output)]
    if cache is not None:command.append(str(cache))
    if parent is not None:command.append(str(parent))
    result=subprocess.run(command,cwd=output,env=q.private_environment(output),capture_output=True,
                          creationflags=q.common.FLAGS,timeout=120)
    (output/'process.log').write_bytes(result.stdout+result.stderr)
    data=json.loads((output/'native-report.json').read_text('utf8'))
    return result.returncode,data


def worker_checks(report, runtime, source, folder):
    folder.mkdir(parents=True)
    cache=folder/'cache'
    code,first=worker(runtime,source,folder/'cold',cache)
    code2,hot=worker(runtime,source,folder/'hot',cache)
    report.case('worker.cache-across-worker-restarts',code==code2==0 and not first['cache_hit'] and hot['cache_hit'])
    for name in ('scene.fslp','reference.dxf'):
        report.compare('worker.cache-exact-'+name,folder/'cold'/name,folder/'hot'/name)
    entry=cache/first['cache_key']
    (entry/'scene.fslp').write_bytes(b'corrupted-derived-cache')
    code,repaired=worker(runtime,source,folder/'repair',cache)
    report.case('worker.cache-corruption-regenerates',code==0 and not repaired['cache_hit'])
    report.compare('worker.cache-repair-exact',folder/'repair/scene.fslp',folder/'cold/scene.fslp')
    data=json.loads(source.read_text('utf8'))
    new=copy.deepcopy(data);new['document_id']=str(uuid.uuid4());new['revision']+=7
    path=folder/'new-revision.json';path.write_text(json.dumps(new,ensure_ascii=False),encoding='utf8')
    code,revision=worker(runtime,path,folder/'revision',cache)
    report.case('worker.cache-rebinds-current-revision',code==0 and revision['cache_hit'] and
                revision['document_id']==new['document_id'] and revision['revision']==new['revision'])
    mutations=[('point',lambda d:d['geometry']['points'][0]['position'].__setitem__(2,d['geometry']['points'][0]['position'][2]+.03125)),
               ('draft',lambda d:d['settings'].__setitem__('draft',d['settings']['draft']+.03125)),
               ('color',lambda d:d['geometry']['layers'][0].__setitem__('color',0x112233))]
    for name,mutate in mutations:
        value=copy.deepcopy(data);mutate(value);path=folder/(name+'.json');path.write_text(json.dumps(value,ensure_ascii=False),encoding='utf8')
        code,changed=worker(runtime,path,folder/name,cache)
        report.case('worker.cache-invalidates-'+name,code==0 and not changed['cache_hit'] and changed['cache_key']!=first['cache_key'])
    for name,mutate in [('bad-format',lambda d:d.__setitem__('format','invalid')),
                       ('missing-draft',lambda d:d['settings'].pop('draft')),
                       ('invalid-level',lambda d:d['geometry'].__setitem__('desired_level',9)),
                       ('bad-point-index',lambda d:d['geometry']['faces'][0]['points'].__setitem__(0,999999)),
                       ('zero-plane',lambda d:d.__setitem__('sections',[{'kind':1,'order':0,'plane':[0,0,0,0]}]))]:
        value=copy.deepcopy(data);mutate(value);path=folder/(name+'.json');path.write_text(json.dumps(value),encoding='utf8')
        code,failed=worker(runtime,path,folder/name,cache)
        report.case('worker.rejects-'+name,code!=0 and failed['status']=='error' and not (folder/name/'scene.fslp').exists())
    # More than sixteen owned entries forces bounded eviction; unrelated content
    # is deliberately placed beside the cache and must survive.
    sentinel=cache/'user-note.txt';sentinel.write_text('keep',encoding='utf8')
    for i in range(18):
        value=copy.deepcopy(data);value['texts']['base']='cache-test-'+str(i)
        path=folder/'evict-input.json';path.write_text(json.dumps(value),encoding='utf8')
        code,metadata=worker(runtime,path,folder/('evict-'+str(i)),cache)
        if code:raise RuntimeError('Eviction fixture generation failed')
    report.case('worker.cache-count-bounded',len([p for p in cache.iterdir() if p.is_dir()])<=16)
    report.case('worker.cache-keeps-unrelated-files',sentinel.read_text('utf8')=='keep')
    # Only our own sleeping helper is terminated; no user process is touched.
    parent=subprocess.Popen([sys.executable,'-c','import time;time.sleep(30)'],creationflags=q.common.FLAGS)
    try:
        parent.terminate();parent.wait(timeout=5)
        output=folder/'dead-parent';output.mkdir()
        result=subprocess.run([str(runtime/'FreeShipNativeLinesplan.exe'),str(source),str(output),str(cache),str(parent.pid)],
                              cwd=output,env=q.private_environment(output),capture_output=True,creationflags=q.common.FLAGS,timeout=10)
        report.case('worker.dead-parent-stops-generation',result.returncode!=0 and not (output/'scene.fslp').exists())
    finally:
        if parent.poll() is None:parent.kill();parent.wait()


def failure_checks(report, runtime, model, folder):
    # Missing and broken components are injected into a private runtime only.
    owned=folder/'runtime';shutil.copytree(runtime,owned)
    engine=owned/'FreeShipNativeLinesplan.exe';parked=owned/'FreeShipNativeLinesplan.parked'
    with q.Service(owned/'bridge/FreeShipGeometryBridge.exe',folder/'service',native_linesplan=True) as service:
        initial=capture(report,'failure.initial',service,folder/'initial','load',path=str(model))
        point=next(p for p in initial['model']['control_points'] if not p['locked'])
        state=dict(service.state);position=list(point['p']);position[2]+=.0625
        engine.rename(parked)
        try:
            response=service.command('move_point',output=folder/'missing',point_id=point['id'],position=position)
            report.case('failure.missing-worker-rejected',not response['ok'] and service.state==state,error=response.get('error'))
        finally:parked.rename(engine)
        restored=capture(report,'failure.restored',service,folder/'restored')
        for kind in ('scene','dxf'):report.compare('failure.model-history-preserved-'+kind,restored['paths'][kind],initial['paths'][kind])
        report.case('failure.control-net-preserved',q.geometry(restored['model'])==q.geometry(initial['model']))
        moved=capture(report,'failure.recovered-move',service,folder/'moved','move_point',point_id=point['id'],position=position)
        report.case('failure.next-edit-works',service.state['modified'] and service.state['revision']==state['revision']+1)
        undone=capture(report,'failure.undo',service,folder/'undo','undo')
        report.compare('failure.undo-restores-original',undone['paths']['scene'],initial['paths']['scene'])


def save_report(report, output):
    report.finish()
    (output/'report.json').write_text(json.dumps(report.data,ensure_ascii=False,indent=2,default=str),encoding='utf8')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE)
    parser.add_argument('--qt-source',type=Path,default=q.QT)
    parser.add_argument('--reuse-build',type=Path)
    parser.add_argument('--runtime',type=Path)
    parser.add_argument('--model',type=Path,action='append')
    args=parser.parse_args();args.source=args.source.resolve();args.qt_source=args.qt_source.resolve()
    out=args.output.resolve();models=[p.resolve(strict=True) for p in (args.model or q.DEFAULT_MODELS)]
    for protected in (args.source,args.qt_source,q.PILOT):
        if out==protected or protected in out.parents:parser.error('Output must be outside product sources')
    out.mkdir(parents=True,exist_ok=False);report=q.Report();report.data['suite']='qt-native-app'
    report.data['test_sources_sha256']={name:q.common.digest_file(q.ROOT/name) for name in (
        'native_linesplan_app.py','qt_migration.py','qt_editing.py','qt_editing_fixture.py',
        'dxf_geometry.py','run_tests.py')}
    report.data['limitations']=['FBM/edit state, four-view mesh and the historical hydrostatic crease split still come from Pascal.',
        'Complete lines-plan subdivision, sections, layout, fill, labels and DXF use C++; renderer is unchanged.',
        'Display XY/corner corrections and label anchor changes remain explicit review; export has no numerical tolerance.',
        'Hidden Qt workflows/images do not establish desktop mouse behavior or real display frame rate.',
        'Cache is bounded and revision-safe within the private model session; application-restart persistence is not claimed.']
    source_before=q.common.fingerprint(args.source)[0];qt_before=q.current_sources(args.qt_source)
    protected={str(p):q.common.digest_file(p) for p in [*models,args.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe',q.PILOT/'dist/bridge/FreeShipGeometryBridge.exe'] if p.exists()}
    try:
        bridge=q.prepare_bridge(args,out,q.bridge_builder(args.qt_source),report)
        runtime=q.prepare_runtime(args,out,bridge,report);q.validate_test_manifest(report.data['qt_build_manifest'],editing=True)
        if 'FreeShipNativeLinesplan.exe' not in report.data['qt_build_manifest']['binaries_sha256']:raise RuntimeError('Native worker has no build provenance')
        executable=runtime/'bridge/FreeShipGeometryBridge.exe'
        for index,model in enumerate(models,1):
            folder=out/f'model-{index}';folder.mkdir();private=folder/'input.fbm';shutil.copy2(model,private)
            entry={'model':str(model),'sha256':q.common.digest_file(model)};report.data['models'].append(entry)
            try:
                old=editing.recipe(report,f'{index}.legacy',executable,private,folder/'legacy',False)
                current=editing.recipe(report,f'{index}.native',executable,private,folder/'native',True)
                entry['protocol']={phase:{k:str(v) for k,v in stage['paths'].items()} for phase,stage in current.items()}
                for phase in editing.PHASES:
                    report.compare(f'{index}.{phase}.complete-dxf-exact',current[phase]['paths']['dxf'],old[phase]['paths']['dxf'])
                    compare_scene(report,f'{index}.{phase}',old[phase]['paths']['scene'],current[phase]['paths']['scene'])
                    if phase=='reopened':
                        report.case(f'{index}.{phase}.attributed-topology-exact',reopened_geometry(old[phase]['model'])==reopened_geometry(current[phase]['model']),
                            reason='IDs are document-scoped; exact uniquely mapped coordinates, point flags, edge multiplicities, face winding, layers, settings and mesh are compared.')
                    else:
                        report.case(f'{index}.{phase}.model-exact',q.geometry(old[phase]['model'])==q.geometry(current[phase]['model']))
                entry['production']=q.run_child(report,f'{index}.production',runtime/'FreeShipQt.exe',['--verify-editing',private],folder/'production')
                for phase in editing.PHASES:
                    for kind in ('scene','dxf'):
                        report.compare(f'{index}.qt-{phase}-{kind}',entry['production']['artifacts'][phase+'_'+kind],current[phase]['paths'][kind])
                entry['session']=q.run_child(report,f'{index}.session',runtime/'qt_session_tests.exe',[private],folder/'session')
                entry['window']=q.run_child(report,f'{index}.window',runtime/'FreeShipQt.exe',['--verify-workflow',private],folder/'window')
                if index==len(models):entry['renderer']=q.run_child(report,'renderer',runtime/'qt_lines_tests.exe',[current['original']['paths']['scene']],folder/'renderer')
                if index==1:
                    worker_checks(report,runtime,folder/'native/original/native-input.json',out/'worker')
                    failure_checks(report,runtime,private,out/'failures')
            except Exception as error:report.case(f'{index}.execution',status='error',reason=f'{type(error).__name__}: {error}')
            save_report(report,out)
        probe=fixture.build(args.source,args.qt_source,out/'fixture-build')
        seed=fixture.run(probe,models[0],out/'fixtures',mode='seed')
        for name in ('seed.fbm','system-colors.fbm','no-intersections.fbm'):
            for mode in (False,True):
                folder=out/'fixture-comparison'/name/str(mode)
                with q.Service(executable,folder/'process',native_linesplan=mode) as service:
                    capture(report,'fixture.'+name+'.'+str(mode),service,folder/'output','load',path=seed['artifacts'][name])
                    if name=='no-intersections.fbm':
                        model=json.loads((folder/'output/model.json').read_text('utf8'))
                        for layer in model['layers']:
                            capture(report,'fixture.hide.'+str(mode),service,folder/('hidden-'+str(layer['id'])),'update_layer',layer_id=layer['id'],changes={'show_in_linesplan':False})
                        capture(report,'fixture.hidden.'+str(mode),service,folder/'hidden')
            base=out/'fixture-comparison'/name
            for variant in ('output','hidden') if name=='no-intersections.fbm' else ('output',):
                old=base/'False'/variant;new=base/'True'/variant
                report.compare('fixture.'+name+'.'+variant+'.dxf',new/'reference.dxf',old/'reference.dxf')
                compare_scene(report,'fixture.'+name+'.'+variant,old/'scene.fslp',new/'scene.fslp')
    except Exception as error:report.case('suite.execution',status='error',reason=f'{type(error).__name__}: {error}')
    finally:
        report.case('legacy-source-unchanged',q.common.fingerprint(args.source)[0]==source_before)
        report.case('qt-source-unchanged-during-test',q.current_sources(args.qt_source)==qt_before)
        for name,digest in protected.items():report.case('protected-file-'+name,q.common.digest_file(Path(name))==digest)
        report.case('native-desktop-acceptance',status='not_run',reason='No desktop automation; manual experience is not inferred from hidden tests.')
        save_report(report,out)
    print(json.dumps({'status':report.data['status'],'counts':report.data['counts'],'report':str(out/'report.json')},ensure_ascii=False))
    return {'pass':0,'partial':2,'fail':1,'error':3}[report.data['status']]


if __name__=='__main__':raise SystemExit(main())
