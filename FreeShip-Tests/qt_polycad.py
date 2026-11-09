"""Polycad control grids: independent coordinates, strict parsing and real Qt imports.

The legacy reader's four section types define this compatibility scope. These
synthetic grids are not a claim of exact NURBS evaluation or external CAD QA.
"""
import argparse
import json
import time
import uuid
from collections import Counter
from pathlib import Path

import qt_migration as q
import qt_editing as e
import qt_exchange as exchange

FILES = ('qt_polycad.py', 'qt_exchange.py', 'qt_migration.py', 'qt_editing.py',
         'qt/exchange_tests.cpp', 'run_tests.py', 'test_artifacts.py')
GMSH = [q.common.DEFAULT_SOURCE / 'Ships' / folder / 'OUT.GEO'
        for folder in ('FromFastship', 'Database')]


def coordinates(points, columns, rows):
    # Fixtures are row-major; Polycad stores one complete column at a time.
    return ''.join(' '.join(format(x, '.17g') for x in points[v*columns+u])+'\n'
                   for u in range(columns) for v in range(rows))


def fixtures(folder):
    folder.mkdir(); requests = []
    # A twisted control grid is deliberately nonplanar; triangulating it here
    # would change the subdivision input. Face indices below are independent.
    points = [[x, y, .25*x*y] for y in range(3) for x in range(3)]
    body = coordinates(points, 3, 3)
    base = '[TBSplineSurface]\nName=Hull\nOrderU=3\nOrderV=3\nNumU=3\nNumV=3\n'+body

    def add(name, contents, ok=True, options=None):
        path=folder/(name+'.geo')
        path.write_bytes(contents if isinstance(contents, bytes) else contents.encode('utf8'))
        requests.append(dict(name=name, method='read', format='polycad', path=str(path),
                             expected_ok=ok, options=options or {}))

    add('surface', base)
    add('half', base, options=dict(symmetric=True))
    for unit in ('mm', 'cm', 'ft', 'in'):
        add('units-'+unit, base, options=dict(units=unit))
    add('中文路径', base.replace('Hull', 'Корпус'))
    add('unicode-name', base.replace('Hull', '船体'))
    add('whitespace', '\ufeff$POLYCADVERSION 5\n# fixture\n'+base.replace('=', ' = ').replace(' ', '\t').replace('\n', '\r\n'))
    add('knots', base.replace('NumU=', 'KnotType=1\nKnotU=0 0 0 1 1 1\nKnotV=0 0 0 1 1 1\nNumU='))
    add('yacht', base.replace('TBSplineSurface', 'TYachtLines').replace('Name=Hull', 'Loa=12\nBMax=4\nTc=1.5\nName=Yacht'))
    add('ship', base.replace('TBSplineSurface', 'TShipLines').replace('Name=Hull', 'Lbp=12000\nBeam=4000\nDraught=1500\nName=Ship'), options=dict(units='mm'))
    add('partial-particulars', base.replace('TBSplineSurface', 'TShipLines').replace('Name=Hull', 'Lbp=12\nName=Ship'))
    first = [[0,0,0],[1,0,0],[0,1,0],[1,1,0]]
    second = [[1,0,0],[2,0,0],[1,1,0],[2,1,0]]
    a, b = coordinates(first,2,2), coordinates(second,2,2)
    patches = '[TBSplineSurfaceList]\nNumSurfaces=2\nNumU=2\nNumV=2\nName=A\n'+a+'Name=B\n'+b
    add('patches', patches)
    add('sections', '[TYachtLines]\nLoa=12\nBMax=4\nTc=1.5\nNumU=2\nNumV=2\n'+a+'[TBSplineSurface]\nNumU=2\nNumV=2\n'+b)
    add('nearby', patches.replace(b, coordinates([[x,y,1e-6] for x,y,z in second],2,2)))
    collapsed = [[0,0,0],[0,0,0],[0,1,0],[1,1,0]]
    add('collapsed-pole', '[TBSplineSurface]\nNumU=2\nNumV=2\n'+coordinates(collapsed,2,2))
    cases = {
        'no-header': body, 'empty': '# empty\n', 'unknown-section': base+'[TUnknown]\n',
        'missing-rows': base.replace('NumV=3\n',''), 'missing-columns': base.replace('NumU=3\n',''),
        'negative-count': base.replace('NumU=3','NumU=-3'), 'huge-count': base.replace('NumV=3','NumV=100001'),
        'count-product': base.replace('NumU=3','NumU=100000'), 'fractional-count': base.replace('NumU=3','NumU=3.1'),
        'truncated': base.rsplit('\n',2)[0]+'\n', 'extra-coordinates': base+'0 0 0\n',
        'nan': base.replace('0 0 0','nan 0 0',1), 'infinite': base.replace('0 0 0','0 inf 0',1),
        'overflow': base.replace('0 0 0','1e10 0 0',1), 'weights': base.replace('0 0 0','0 0 0 1',1),
        'unknown-property': base.replace('NumU=3','Weights=1\nNumU=3'),
        'bad-order': base.replace('OrderU=3','OrderU=1'), 'bad-encoding': b'[TBSplineSurface]\nName=\xff',
        'bad-name': base.replace('Hull','X'*257), 'nul': base+'\0',
        'list-short': patches.replace('NumSurfaces=2','NumSurfaces=3'),
        'list-long': patches.replace('NumSurfaces=2','NumSurfaces=1'),
        'list-undeclared': patches.replace('NumSurfaces=2\n',''),
        'duplicate-count': patches.replace('NumSurfaces=2','NumSurfaces=2\nNumSurfaces=2'),
        'duplicate-face': patches.replace(b,a), 'zero-area': '[TBSplineSurface]\nNumU=2\nNumV=2\n'+('0 0 0\n'*4),
        'section-shape-leak': '[TBSplineSurface]\nNumU=2\nNumV=2\n'+a+'[TBSplineSurface]\n'+b,
        'empty-last-section': base+'[TShipLines]\n', 'pending-shape': patches+'NumU=2\n',
        'invalid-draft': '[TShipLines]\nDraught=-1\nNumU=2\nNumV=2\n'+a,
        'conflicting-particulars': '[TShipLines]\nLbp=10\nNumU=2\nNumV=2\n'+a+'[TShipLines]\nLbp=11\nNumU=2\nNumV=2\n'+b,
        'nonmanifold': patches.replace('NumSurfaces=2','NumSurfaces=3')+coordinates([[1,0,0],[1,0,1],[1,1,0],[1,1,1]],2,2),
    }
    for name, contents in cases.items(): add(name, contents, False)
    for i,path in enumerate(GMSH):
        requests.append(dict(name='gmsh-'+str(i), method='read', format='polycad', path=str(path), expected_ok=False))
    return requests, points


def codecs(r, runtime, out):
    requests, points = fixtures(out/'fixtures')
    result = exchange.probe(runtime/'qt_exchange_tests.exe', requests, out/'codecs')
    for req in requests:
        item=result[req['name']]
        r.case('parse.'+req['name'], item['ok']==req['expected_ok'], error=item.get('error'))
    def data(name): return result[name]['data']
    d=data('surface')
    faces=[[0,1,4,3],[1,2,5,4],[3,4,7,6],[4,5,8,7]]
    r.case('surface.exact-control-grid', [p['p'] for p in d['points']]==points)
    r.case('surface.quads-and-winding', [f['points'] for f in d['faces']]==faces)
    r.case('surface.corners', [p['vertex_type'] for p in d['points']]==[3,0,3,0,0,0,3,0,3])
    boundary={(0,1),(1,2),(2,5),(5,8),(7,8),(6,7),(3,6),(0,3)}
    r.case('surface.only-boundary-creases', len(d['edges'])==12 and all(bool(edge[2])==(tuple(sorted(edge[:2])) in boundary) for edge in d['edges']))
    r.case('surface.no-implicit-mirror', d['layers'][0]['symmetric'] is False)
    r.case('surface.explicit-half-hull', data('half')['layers'][0]['symmetric'] is True and data('half')['project']['beam']==4)
    for unit,scale in (('mm',.001),('cm',.01),('ft',1),('in',.0254)):
        current=data('units-'+unit)
        r.case('units.'+unit, [p['p'] for p in current['points']]==[[exchange.f32(x*scale) for x in p] for p in points])
        r.case('units.'+unit+'.project', current['project']['units']==('imperial' if unit=='ft' else 'metric'))
    for name in ('yacht','ship'):
        p=data(name)['project']
        r.case(name+'.main-particulars', [p[k] for k in ('length','beam','draft')]==[12,4,1.5] and data(name)['particulars_set'])
    r.case('partial-particulars.flag', not data('partial-particulars')['particulars_set'] and data('partial-particulars')['project']['length']==12)
    r.case('unicode.preserved',data('unicode-name')['layers'][0]['name']=='船体' and data('中文路径')['layers'][0]['name']=='Корпус')
    r.case('knots.do-not-change-control-net', data('knots')['points']==d['points'] and data('knots')['faces']==d['faces'])
    p=data('patches')
    r.case('patches.exact-seam-weld', len(p['points'])==6 and [f['points'] for f in p['faces']]==[[0,1,3,2],[1,4,5,3]])
    r.case('patches.shared-seam-smooth', len(p['edges'])==7 and sum(not edge[2] for edge in p['edges'])==1)
    r.case('patches.names-preserved', [l['name'] for l in p['layers']]==['A','B'])
    r.case('sections.do-not-consume-next-section', len(data('sections')['points'])==6 and len(data('sections')['layers'])==2)
    r.case('nearby.no-distance-welding', len(data('nearby')['points'])==8)
    pole=data('collapsed-pole')
    r.case('pole.triangle-kept', len(pole['points'])==3 and [f['points'] for f in pole['faces']]==[[0,2,1]])
    r.case('provenance.saved-without-codepage-loss', d['project']['comment'].isascii() and 'B-spline' in d['project']['comment'])
    return result


def service(r, runtime, values, out):
    with q.Service(runtime/'FreeShipDocumentService.exe', out/'process') as s:
        for name in ('surface','patches','collapsed-pole','yacht','ship','units-ft','中文路径','unicode-name','nearby'):
            folder=out/name;folder.mkdir();payload=folder/'exchange.json';exchange.dump(payload,values[name]['data'])
            preferences={'fbm_encoding':'UTF-8'} if name in ('中文路径','unicode-name') else {'fbm_encoding':'cp1252'}
            if name=='unicode-name':
                state=dict(s.state)
                rejected=s.command('import_model', path=str(payload), document_id=str(uuid.uuid4()), preferences={'fbm_encoding':'cp1252'}, output=folder/'rejected-codepage')
                r.case('unicode.unrepresentable-name-preserves-document', not rejected['ok'] and s.state==state)
            imported=q.require_ok(s.command('import_model', path=str(payload), document_id=str(uuid.uuid4()), preferences=preferences, output=folder/'import'))
            model=json.loads(Path(imported['model']).read_text('utf8'))
            r.case(name+'.unsaved', s.state['modified'] and not s.state['can_undo'])
            r.case(name+'.coordinates', [p['p'] for p in model['control_points']]==[p['p'] for p in values[name]['data']['points']])
            r.case(name+'.face-count', len(model['control_faces'])==len(values[name]['data']['faces']))
            p=model['control_points'][0];moved=list(p['p']);moved[2]+=.125
            q.require_ok(s.command('move_point',point_id=p['id'],position=moved,output=folder/'move'))
            undone=q.require_ok(s.command('undo',output=folder/'undo'))
            exchange.compare_geometry(r,name+'.editable-undo',model,json.loads(Path(undone['model']).read_text('utf8')),folder/'undo-diff.json')
            saved=q.require_ok(s.command('save',output=folder/'save'))
            reopened=q.require_ok(s.command('load',path=saved['saved_file'],document_id=str(uuid.uuid4()),preferences=preferences,output=folder/'reopen'))
            current=json.loads(Path(reopened['model']).read_text('utf8'))
            exchange.compare_geometry(r,name+'.fbm-roundtrip',current,model,folder/'roundtrip-diff.json')
            r.case(name+'.project-roundtrip',current['settings']==model['settings'])
            r.case(name+'.derived-surface-roundtrip',Counter(exchange.oriented(t['p']) for t in current['triangles'])==Counter(exchange.oriented(t['p']) for t in model['triangles']))
        snap=q.require_ok(s.command('snapshot',output=out/'snapshot'))
        r.case('native.no-pascal',snap.get('adapter_pid')==0 and snap.get('adapter_requests')==0)


def production(r,runtime,out,fixtures,empty=False):
    out.mkdir();target=out/'imported.fbm'
    commands=[] if empty else [dict(name='bad',method='import',format='polycad',path=str(fixtures/'list-short.geo'),expect_failure=True)]
    commands += [dict(name='import',method='import',format='polycad',path=str(fixtures/('ship.geo' if empty else 'patches.geo')),options=dict(units='mm' if empty else 'm',symmetric=False)),
                 dict(name='save',method='save',path=str(target)),dict(name='reopen',method='reopen',path=str(target))]
    request=dict(commands=commands)
    if not empty:request['model']=str(q.DEFAULT_MODELS[0])
    path=out/'request.json';exchange.dump(path,request)
    child=q.run_child(r,out.name,runtime/'FreeShipQt.exe',['--verify-exchange',path],out/'qt',timeout=120,platform='windows')
    r.data.setdefault('production',{})[out.name]=child
    art=child['artifacts']
    if not empty:
        for kind in ('scene','dxf','model'):r.compare('failure.preserves-'+kind,art['bad_'+kind],art['original_'+kind])
    a=json.loads(Path(art['import_model']).read_text('utf8'));b=json.loads(Path(art['reopen_model']).read_text('utf8'))
    exchange.compare_geometry(r,out.name+'.save-reopen',a,b,out/'roundtrip-diff.json')
    r.case(out.name+'.particulars',b['settings']['length']==(12 if empty else 2))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path)
    p.add_argument('--reuse-build',type=Path)
    a=p.parse_args();runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    if a.verify_deployment and (a.runtime or a.reuse_build):p.error('Deployment verification does not build or copy a runtime')
    a.source=q.common.DEFAULT_SOURCE;a.qt_source=q.QT
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if root==out or root in out.parents:p.error('Use a private output directory')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.monotonic()
    r.data.update(suite='qt-polycad',runtime=str(runtime),limitations=[
        'Control-grid interpretation follows the four Polycad sections handled by the old FreeShip reader, not exact B-spline/NURBS evaluation.',
        'Independent synthetic grids cover ordering, topology, units and failure handling; no external Polycad installation/sample has been supplied.',
        'Real Qt callbacks run in hidden private Windows instances, not physical desktop acceptance.'])
    source=q.current_sources(q.QT);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    protected={str(f):q.common.digest_file(f) for f in [*q.DEFAULT_MODELS,*GMSH,q.common.DEFAULT_SOURCE/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe']}
    tests={n:q.common.digest_file(q.ROOT/n) for n in FILES}
    r.data.update(qt_sources_sha256=source,source_sha256=legacy,test_sources_sha256=tests)
    try:
        if a.reuse_build:
            bridge=q.prepare_bridge(a,out,q.bridge_builder(q.QT),r)
            runtime=q.prepare_runtime(a,out,bridge,r);r.data['runtime']=str(runtime)
        manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=source:raise RuntimeError('Stale Qt source manifest')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime hash mismatch')
        r.data['runtime_sha256']=manifest['binaries_sha256'];r.case('runtime-manifest',True)
        values=codecs(r,runtime,out);service(r,runtime,values,out/'service')
        production(r,runtime,out/'loaded-start',out/'fixtures')
        production(r,runtime,out/'empty-start',out/'fixtures',empty=True)
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==source);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==legacy)
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in tests.items()))
        r.case('models-and-baselines-protected',all(q.common.digest_file(Path(f))==h for f,h in protected.items()))
        r.case('external-polycad',status='not_run',reason='No external Polycad installation or real exported Polycad fixture supplied. Repository OUT.GEO files are Gmsh, tested as rejection cases.')
        r.case('visual-parity',status='review',reason='Hidden Windows dialog screenshots retained; full original-window visual parity is not certified.')
        r.case('desktop',status='not_run',reason='No desktop mouse/focus or native file chooser acceptance.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))))
    return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]


if __name__=='__main__':raise SystemExit(main())
