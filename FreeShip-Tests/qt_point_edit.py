"""Native point editing regression, included by the unified qt-document suite.

Uses the old service as a numerical oracle, plus explicit offset, projection,
atomicity and native-dispatch expectations. No desktop automation or user files
are modified. All outputs live in the suite's private report directory.
"""
import json
import random
import uuid
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_modeling as modeling


def run(report, runtime, source, out):
    out.mkdir(parents=True)
    identity = str(uuid.uuid4())
    timings = []
    with q.Service(runtime/'FreeShipDocumentService.exe', out/'native', native_linesplan=True) as native, \
         q.Service(runtime/'bridge/FreeShipGeometryBridge.exe', out/'oracle', native_linesplan=True) as oracle:
        step = 0

        def both(label, method='snapshot', **fields):
            nonlocal step
            step += 1
            results = []
            for tag, service in [('native', native), ('oracle', oracle)]:
                folder = out/f'{step:03}-{label}'/tag
                response = q.require_ok(service.command(method, output=folder, **fields))
                model = json.loads(Path(response['model']).read_text('utf8'))
                results.append((response, model))
            (a, am), (b, bm) = results
            report.case('points.'+label+'.state', all(a[k] == b[k] for k in q.STATE_KEYS))
            report.case('points.'+label+'.complete-geometry', modeling.geometry(am) == modeling.geometry(bm))
            report.compare('points.'+label+'.scene', a['scene'], b['scene'])
            report.compare('points.'+label+'.dxf', a['dxf'], b['dxf'])
            report.compare('points.'+label+'.sections', a['sections'], b['sections'])
            if method in ('move_point', 'move_points'):
                report.case('points.'+label+'.native-algorithm-and-writer',
                            a.get('edit_engine') == 'cpp-points' and a.get('fbm_write_engine') == 'cpp-coordinate-writer'
                            and a.get('adapter_operation')=='none' and a.get('snapshot_engine')=='cpp-publication')
                timings.append({'label': label, 'point_edit_ms': a['point_edit_ms'],
                                'total_ms': a['document_total_ms']})
            return am

        def reject(label, method, **fields):
            for tag, service in [('native', native), ('oracle', oracle)]:
                before = dict(service.state)
                result = service.command(method, output=out/f'reject-{label}-{tag}', **fields)
                report.case(f'points.reject-{label}.{tag}', result.get('ok') is False and service.state == before,
                            reason=result.get('error'))

        model = both('load', 'load', path=str(source), document_id=identity)
        free = [p for p in model['control_points'] if p['batch_movable']][:2]
        moves = [{'point_id': p['id'], 'position': [p['p'][0]+.125, p['p'][1]-.0625, p['p'][2]+.25]} for p in free]
        model = both('batch', 'move_points', moves=moves)
        report.case('points.batch-independent-coordinates', all(e.points(model)[m['point_id']] == [e.f32(x) for x in m['position']] for m in moves))
        both('undo-batch', 'undo')
        noop = [{'point_id': p['id'], 'position': p['p']} for p in free]
        before = dict(native.state)
        both('batch-noop-keeps-redo', 'move_points', moves=noop)
        report.case('points.noop-history', native.state == before and native.state['can_redo'])
        both('redo-batch', 'redo')
        reject('duplicate', 'move_points', moves=[moves[0], moves[0]])
        reject('late-invalid-id', 'move_points', moves=[moves[0], {'point_id': 999999, 'position': [0,0,0]}])
        reject('late-overflow', 'move_points', moves=[moves[0], {'point_id': free[1]['id'], 'position': [1e100,0,0]}])
        reject('fractional-id', 'move_point', point_id=1.25, position=[0,0,0])
        ids = []
        positions = [[30.125,1.25,2.5], [33.75,4.5,7.25], [36.5,6.25,9.5],
                     [40.125,-2.5,4.25], [42.25,3.75,7.125], [46.5,8.25,10.75]]
        for i, pos in enumerate(positions):
            old = set(e.points(model))
            model = both('add-'+str(i), 'topology', operation='add_point', position=pos)
            ids.append(next(iter(set(e.points(model))-old)))
        model = both('hard-anchor', 'modeling', operation='anchor', ids=ids[:2], hard=True)
        model = both('anchor-chain', 'modeling', operation='anchor', ids=ids[1:3], hard=False)
        old = e.points(model)
        model = both('move-chain-root', 'move_point', point_id=ids[0], position=[31.125,3.25,5.5])
        report.case('points.chain-independent-offsets', all(e.points(model)[i] == [old[i][0]+1, old[i][1]+2, old[i][2]+3] for i in ids[:3]))
        reject('hard-anchor-direct', 'move_point', point_id=ids[1], position=[0,0,0])
        reject('dependent-batch', 'move_points', moves=[{'point_id': free[0]['id'], 'position': [0,0,0]}, {'point_id': ids[0], 'position': [1,2,3]}])
        model = both('line-constraint', 'modeling', operation='linear_constraint', ids=ids[3:])
        rng = random.Random(260203)
        # Fractional, oblique projections expose any float evaluation-order drift.
        for i in range(12):
            pos = [rng.uniform(37,48), rng.uniform(-5,10), rng.uniform(1,13)]
            point = ids[3+i%3]
            model = both('oblique-'+str(i), 'move_point', point_id=point, position=pos)
        both('undo-projection', 'undo'); model = both('redo-projection', 'redo')
        model = both('lock-dependent', 'topology', operation='set_locked', ids=[ids[2]], locked=True)
        old = e.points(model)
        target = [old[ids[0]][0]+.5, old[ids[0]][1]-.25, old[ids[0]][2]+.75]
        model = both('chain-respects-lock', 'move_point', point_id=ids[0], position=target)
        report.case('points.locked-dependent-independent', e.points(model)[ids[2]] == old[ids[2]])
        reject('locked-direct', 'move_point', point_id=ids[2], position=[0,0,0])
        # Read the C++-encoded result through both implementations. Original IDs
        # do not survive a new load, so compare normalized complete saved models.
        saved = q.require_ok(native.command('save', output=out/'saved'))
        loaded = both('reopen-native-fbm', 'load', path=saved['saved_file'], document_id=str(uuid.uuid4()))
        import native_linesplan_app as nla
        report.case('points.save-reopen-complete-geometry', nla.reopened_geometry(loaded) == nla.reopened_geometry(model))
        # No external-document client can silently send point edits back to the
        # old implementation. Its direct oracle mode remains available.
        for method, fields in [('move_point', {'point_id':1,'position':[0,0,0]}),
                               ('move_points', {'moves':[{'point_id':1,'position':[0,0,0]}]})]:
            result = oracle.command(method, output=out/('forbidden-'+method), external_document=True, **fields)
            report.case('points.adapter-forbids-'+method, not result['ok'] and 'C++' in result.get('error',''))
    report.data['native_point_timings'] = timings
