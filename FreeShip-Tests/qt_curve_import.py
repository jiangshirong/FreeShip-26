"""Hull/Carene/chine/surface inputs, independent fitting, native storage and Qt.

Fixture grammars are taken from the original input routines. Expected fitting
uses a 60-digit dense Gaussian solve, independent of the production tridiagonal
solver. A straight source segment has known spline values; no production writer
is used to make its own input or expected control grid.
"""
import argparse
import copy
import json
import time
import uuid
from collections import Counter
from decimal import Decimal, localcontext
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_exchange as x

FILES=('qt_curve_import.py','qt_exchange.py','qt_migration.py','qt_editing.py','qt/exchange_tests.cpp','run_tests.py','test_artifacts.py')
CHINES=[[[8,0,0],[0,0,0]],[[8,2,1],[0,2,1]],[[8,3,3],[0,3,3]]]
SURFACE=[[[0,0,0],[0,2,2]],[[6,0,0],[6,2,2]]]


def text_curves(curves,units=0):
    return str(units)+'\n'+'\n'.join(''.join(' '.join(map(str,p))+'\n' for p in curve) for curve in curves)+'EOF\n'


def hull_fixture():
    # 5 primary + 13 intermediate stations, inch coordinates stored Y/Z/X.
    # Intermediate breadth deliberately differs, so toggling cannot be a no-op.
    rows=['3'];tables=[]
    for size,breadth in ((5,20),(13,24)):
        curves=[[],[],[]]
        for i in range(size):
            for j,(y,z) in enumerate(((0,0),(breadth,10),(30,30))):
                p=[100*i/(size-1),y,z];curves[j].append(p)
                rows += [str(y),str(z),format(p[0],'.17g')]
        tables.append(curves)
        if size==5:rows.append('intermediate stations')
    stations=[0,10,25,40,50,65,80,100]
    rows += list(map(str,stations))+['rig record '+str(i) for i in range(9)]+['Hull Designer','Independent Hull']
    return '\n'.join(rows)+'\n',tables,stations


def fixtures(out):
    out.mkdir();requests=[];hull,tables,stations=hull_fixture()
    def add(name,format,contents,ok=True,options=None):
        path=out/(name+'.'+dict(hull='hul',carene='xyz',chines='txt',surface='txt')[format])
        path.write_bytes(contents if isinstance(contents,bytes) else contents.encode('utf8'))
        requests.append(dict(name=name,method='read',format=format,path=str(path),options=options or {},expected_ok=ok))
    basic=text_curves(CHINES);surface=text_curves(SURFACE)
    carene='Independent Carene sample\n'+''.join('LINE '+str(i)+'\nheading\n'+''.join(' '.join(map(str,p))+'\n' for p in c)+'\n' for i,c in enumerate(CHINES))
    for n in (2,3,4,8,33):add('chine-'+str(n),'chines',basic,options=dict(columns=n))
    add('chine-feet','chines',text_curves(CHINES,1));add('chine-tabs','chines','\ufeff'+basic.replace(' ','\t').replace('\n','\r\n'))
    add('chine-no-eof','chines',basic.replace('EOF\n',''));add('中文路径','chines',basic)
    add('carene','carene',carene);add('carene-mm','carene',carene,options=dict(units='mm'));add('carene-ft','carene',carene,options=dict(units='ft'))
    add('hull-13','hull',hull);add('hull-5','hull',hull,options=dict(intermediate_bulkheads=False))
    add('surface','surface',surface,options=dict(columns=3,rows=3,symmetric=False))
    add('surface-fine','surface',surface,options=dict(columns=5,rows=7,symmetric=True))
    add('surface-auto','surface',surface);add('surface-feet','surface',text_curves(SURFACE,1),options=dict(columns=3,rows=3))
    nonlinear=[[[0,0,0],[0,1,.2],[0,3,2]],[[3,0,.3],[3,1.5,.8],[3,3,2.8]],[[6,0,0],[6,1,1],[6,3,3]]]
    add('surface-curved','surface',text_curves(nonlinear),options=dict(columns=5,rows=6))
    for name,contents in {
        'bad-unit':basic.replace('0\n','2\n',1), 'one-curve':text_curves(CHINES[:1]),
        'single-point':text_curves([CHINES[0][:1],CHINES[1]]), 'nan':basic.replace('8 0 0','nan 0 0'),
        'infinite':basic.replace('8 0 0','8 inf 0'), 'overflow':basic.replace('8 0 0','1e12 0 0'),
        'extra-coordinate':basic.replace('8 0 0','8 0 0 1'), 'bad-record':basic.replace('8 0 0','x y z'),
        'tail':basic+'unexpected\n', 'negative-half':basic.replace('8 2 1','8 -2 1'),
        'duplicate-sample':text_curves([CHINES[0]+[CHINES[0][-1]],CHINES[1]]),
        'duplicate-curves':text_curves([CHINES[0],CHINES[0]]), 'too-many-curves':text_curves([CHINES[0]]*257),
        'bad-utf8':b'0\n\xff', 'nul':basic+'\0', 'long-line':basic+'X'*16385, 'many-lines':'\n'*400001,
    }.items():add(name,'chines',contents,False)
    for n in (-1,1,101):add('bad-columns-'+str(n),'chines',basic,False,dict(columns=n))
    for name,opts in [('bad-rows',dict(rows=2)),('bad-surface-columns',dict(columns=2)),('large-rows',dict(rows=101))]:add(name,'surface',surface,False,opts)
    add('surface-empty-middle','surface','0\n0 0 0\n\n0 1 0\n',False)
    add('carene-outside-section','carene','name\n0 0 0\n',False)
    add('carene-no-geometry','carene','name\nLINE\nheading\n',False)
    add('hull-bad-count','hull',hull.replace('3\n','0\n',1),False)
    add('hull-truncated','hull','\n'.join(hull.splitlines()[:20]),False)
    add('hull-extra','hull',hull+'99\n',False)
    add('hull-nonfinite','hull',hull.replace('0\n','NaN\n',1),False)
    return requests,tables,stations


def segment_samples(curve,n):
    # Natural cubic interpolation of two points is the straight segment. Retain
    # the existing native spline's Single weights, products and output storage.
    result=[]
    for i in range(n):
        a=x.f32(1-i/(n-1));b=x.f32(i/(n-1))
        result.append([x.f32(x.f32(a*v)+x.f32(b*w)) for v,w in zip(*curve)])
    return result


def exact_fit(samples):
    with localcontext() as context:
        context.prec=60;n=len(samples)
        matrix=[[Decimal(0)]*n+[Decimal(float(p)) for p in row] for row in samples]
        matrix[0][0]=matrix[-1][n-1]=Decimal(1)
        for i in range(1,n-1):
            matrix[i][i-1]=matrix[i][i+1]=Decimal(1)/6;matrix[i][i]=Decimal(2)/3
        # Dense Gauss-Jordan elimination is independent of the O(n) C++ solve.
        for col in range(n):
            pivot=matrix[col][col];matrix[col]=[v/pivot for v in matrix[col]]
            for row in range(n):
                if row!=col:
                    factor=matrix[row][col];matrix[row]=[v-factor*w for v,w in zip(matrix[row],matrix[col])]
        return [[x.f32(float(v)) for v in row[n:]] for row in matrix]


def model_extras(model):
    points={p['id']:p['p'] for p in model['control_points']}
    return dict(curves=[[points[p] for p in c['controls']] for c in model['control_curves']],
                markers=model['markers'],sections=model['sections'],settings=model['settings'])


def codec_checks(r,runtime,out):
    requests,tables,stations=fixtures(out/'fixtures');values=x.probe(runtime/'qt_exchange_tests.exe',requests,out/'codecs')
    for req in requests:r.case('parse.'+req['name'],values[req['name']]['ok']==req['expected_ok'],error=values[req['name']].get('error'))
    def data(name):return values[name]['data']
    for n in (2,3,4,8,33):
        d=data('chine-'+str(n));points=[v['p'] for v in d['points']]
        expected=[exact_fit(segment_samples(c,n)) for c in CHINES]
        r.case('chine-'+str(n)+'.independent-fit',[[points[i] for i in curve] for curve in d['control_curves']]==expected)
        r.case('chine-'+str(n)+'.source-markers',d['markers']==CHINES)
        r.case('chine-'+str(n)+'.layers',[(l['name'],l['developable'],l['symmetric']) for l in d['layers']]==[('Strake 1',True,True),('Strake 2',True,True),('Close hull',False,True)])
        crease={tuple(sorted(e[:2])):e[2] for e in d['edges']}
        r.case('chine-'+str(n)+'.longitudinal-creases',all(crease[tuple(sorted(pair))] for curve in d['control_curves'] for pair in zip(curve,curve[1:])))
        curves=d['control_curves']
        r.case('chine-'+str(n)+'.cross-strake-smooth',all(not crease[tuple(sorted((curves[i][j],curves[i+1][j])))] for i in range(len(curves)-1) for j in range(1,n-1)))
        r.case('chine-'+str(n)+'.smooth-interior',n==2 or any(not edge[2] for edge in d['edges']))
        r.case('chine-'+str(n)+'.no-invented-draft',d['project']['draft']==0 and not d['particulars_set'])
    r.case('carene.same-geometry',data('carene')['points']==data('chine-8')['points'] and data('carene')['faces']==data('chine-8')['faces'])
    r.case('carene.title',data('carene')['project']['name']=='Independent Carene')
    r.case('carene.mm-source',data('carene-mm')['markers']==[[[x.f32(v*.001) for v in point] for point in curve] for curve in CHINES])
    for name in ('chine-feet','carene-ft','surface-feet'):
        d=data(name);r.case(name+'.units',d['project']['units']=='imperial')
        r.case(name+'.density',d['project']['water_density']==x.f32(x.f32(1.025)*1000*.3048**3/.45359237))
    for name,cols,rows in [('surface',3,3),('surface-fine',5,7)]:
        d=data(name);across=[segment_samples(c,rows) for c in SURFACE]
        grid=[exact_fit(segment_samples([across[0][i],across[1][i]],cols)) for i in range(rows)]
        vertical=[exact_fit([row[col] for row in grid]) for col in range(cols)]
        expected=[vertical[col][row] for row in range(rows) for col in range(cols)]
        r.case(name+'.independent-tensor-fit',[p['p'] for p in d['points']]==expected)
        r.case(name+'.quad-count',len(d['faces'])==(cols-1)*(rows-1) and all(len(f['points'])==4 for f in d['faces']))
        r.case(name+'.winding',[f['points'] for f in d['faces']]==[[(row-1)*cols+col-1,(row-1)*cols+col,row*cols+col,row*cols+col-1] for row in range(1,rows) for col in range(1,cols)])
        r.case(name+'.smooth-edges',sum(not e[2] for e in d['edges'])==(cols-2)*(rows-1)+(rows-2)*(cols-1))
        r.case(name+'.source-markers',d['markers']==SURFACE)
    for name,table in [('hull-5',tables[0]),('hull-13',tables[1])]:
        d=data(name);converted=[[[x.f32(x.f32(v)/12) for v in point] for point in curve] for curve in table]
        maximum=max(p[0] for curve in converted for p in curve)
        for curve in converted:
            for p in curve:p[0]=x.f32(maximum-p[0])
        r.case(name+'.inch-coordinates-and-axis',d['markers']==converted)
        r.case(name+'.stations-inch-factor',d['stations']==[x.f32(maximum-x.f32(x.f32(v)/12)) for v in stations])
        r.case(name+'.imperial-project',d['project']['units']=='imperial')
        r.case(name+'.freshwater-default',d['project']['water_density']==x.f32(1000*.3048**3/.45359237))
        r.case(name+'.metadata',d['project']['name']=='Independent Hull' and d['project']['designer']=='Hull Designer')
        r.case(name+'.sample-option',all(len(c)==(5 if name=='hull-5' else 13) for c in d['control_curves']))
    r.case('hull.intermediate-is-used',data('hull-5')['markers'][1][0][1]!=data('hull-13')['markers'][1][0][1])
    return values


def service_checks(r,runtime,values,out):
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process') as s:
        for name in ('chine-8','carene','hull-5','hull-13','surface','surface-curved','chine-feet'):
            folder=out/name;folder.mkdir();source=folder/'exchange.json';x.dump(source,values[name]['data'])
            made=q.require_ok(s.command('import_model',path=str(source),document_id=str(uuid.uuid4()),output=folder/'import'))
            model=json.loads(Path(made['model']).read_text('utf8'));d=values[name]['data']
            r.case(name+'.native-unsaved',s.state['modified'] and not s.state['can_undo'])
            r.case(name+'.point-coordinates',[p['p'] for p in model['control_points']]==[p['p'] for p in d['points']])
            r.case(name+'.curve-count',len(model['control_curves'])==len(d.get('control_curves',[])))
            r.case(name+'.marker-count',len(model['markers'])==len(d['markers']))
            r.case(name+'.original-marker-coordinates',[m['spline']['points'] for m in model['markers']]==d['markers'])
            lookup={p['id']:p['p'] for p in model['control_points']}
            r.case(name+'.control-curve-bindings',[[lookup[i] for i in c['controls']] for c in model['control_curves']]==[[d['points'][i]['p'] for i in c] for c in d.get('control_curves',[])])
            r.case(name+'.station-definitions',sorted(v['distance'] for v in model['sections'] if v['kind']==1)==sorted(set(d.get('stations',[]))))
            initial=model_extras(model)
            point=model['control_points'][0];moved=list(point['p']);moved[0]+=.125
            q.require_ok(s.command('move_point',point_id=point['id'],position=moved,output=folder/'move'))
            undo=q.require_ok(s.command('undo',output=folder/'undo'));current=json.loads(Path(undo['model']).read_text('utf8'))
            x.compare_geometry(r,name+'.edit-undo',model,current,folder/'undo-diff.json');r.case(name+'.undo-extras',model_extras(current)==initial)
            saved=q.require_ok(s.command('save',output=folder/'save'));reopened=q.require_ok(s.command('load',path=saved['saved_file'],document_id=str(uuid.uuid4()),output=folder/'reopen'))
            current=json.loads(Path(reopened['model']).read_text('utf8'));x.compare_geometry(r,name+'.fbm-roundtrip',model,current,folder/'roundtrip-diff.json')
            r.case(name+'.roundtrip-extras',model_extras(current)==initial)
            r.case(name+'.full-derived-surface',Counter(x.oriented(t['p']) for t in current['triangles'])==Counter(x.oriented(t['p']) for t in model['triangles']))
            for kind in ('scene','dxf'):r.compare(name+'.roundtrip-'+kind,reopened[kind],made[kind])
        state=dict(s.state);before=q.require_ok(s.command('snapshot',output=out/'before-invalid'))
        d=values['hull-5']['data']
        mutations=[('bad-curve-index',lambda c:c['control_curves'][0].__setitem__(0,999999)),
                   ('missing-curve-edge',lambda c:c['control_curves'].__setitem__(0,[0,len(c['points'])-1])),
                   ('reused-curve',lambda c:c['control_curves'].append(c['control_curves'][0])),
                   ('bad-marker',lambda c:c['markers'][0][0].__setitem__(0,'nan')),
                   ('bad-station',lambda c:c['stations'].__setitem__(0,'nan'))]
        for name,mutate in mutations:
            bad=copy.deepcopy(d);mutate(bad);path=out/(name+'.json');x.dump(path,bad)
            response=s.command('import_model',path=str(path),document_id=str(uuid.uuid4()),output=out/name)
            r.case(name+'.document-preserved',not response['ok'] and s.state==state,error=response.get('error'))
        after=q.require_ok(s.command('snapshot',output=out/'after-invalid'))
        for kind in ('scene','dxf','model'):r.compare('rejected-extras.preserve-'+kind,after[kind],before[kind])
        r.case('native.without-pascal',after.get('adapter_pid')==0 and after.get('adapter_requests')==0)


def production(r,runtime,fixtures,out):
    out.mkdir();commands=[dict(name='bad',method='import',format='chines',path=str(fixtures/'bad-unit.txt'),expect_failure=True)]
    for fmt,fixture,opts in [('hull','hull-5.hul',dict(intermediate_bulkheads=False)),('carene','carene.xyz',dict(columns=4)),('chines','中文路径.txt',dict(columns=3)),('surface','surface.txt',dict(columns=3,rows=3,symmetric=False))]:
        commands += [dict(name=fmt,method='import',format=fmt,path=str(fixtures/fixture),options=opts),
                     dict(name=fmt+'-save',method='save',path=str(out/(fmt+'.fbm'))),dict(name=fmt+'-reopen',method='reopen',path=str(out/(fmt+'.fbm')))]
    req=out/'request.json';x.dump(req,dict(model=str(q.DEFAULT_MODELS[0]),commands=commands))
    child=q.run_child(r,'production',runtime/'FreeShipQt.exe',['--verify-exchange',req],out/'qt',timeout=180,platform='windows');r.data['production']=child
    art=child['artifacts']
    for kind in ('scene','dxf','model'):r.compare('production.failure-'+kind,art['bad_'+kind],art['original_'+kind])
    for fmt in ('hull','carene','chines','surface'):
        a=json.loads(Path(art[fmt+'_model']).read_text('utf8'));b=json.loads(Path(art[fmt+'-reopen_model']).read_text('utf8'))
        x.compare_geometry(r,'production.'+fmt+'.fbm-roundtrip',a,b,out/(fmt+'-diff.json'));r.case('production.'+fmt+'.extras',model_extras(a)==model_extras(b))
    r.case('production.hull-option',len(json.loads(Path(art['hull_model']).read_text('utf8'))['control_curves'][0]['controls'])==5)
    r.case('production.chine-count',len(json.loads(Path(art['chines_model']).read_text('utf8'))['control_curves'][0]['controls'])==3)
    r.case('production.surface-grid',len(json.loads(Path(art['surface_model']).read_text('utf8'))['control_points'])==9)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--reuse-build',type=Path)
    a=p.parse_args();runtime=(a.verify_deployment or a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    if a.verify_deployment and (a.runtime or a.reuse_build):p.error('Deployment verification does not build or copy a runtime')
    a.source=q.common.DEFAULT_SOURCE;a.qt_source=q.QT
    if any(root==out or root in out.parents for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT)):p.error('Use private output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();start=time.monotonic();source=q.current_sources(q.QT);legacy=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]
    tests={name:q.common.digest_file(q.ROOT/name) for name in FILES}
    r.data.update(suite='qt-curve-import',runtime=str(runtime),qt_sources_sha256=source,source_sha256=legacy,test_sources_sha256=tests,
        limitations=['Fitted subdivision control nets retain source samples as markers; not a lossless arbitrary surface exchange.',
                    'Synthetic files follow the current Plus readers. External Carlson Hull/Carene application files are not supplied.',
                    'Hidden Windows production callbacks and screenshots are separate from full original UI parity and physical desktop acceptance.'])
    protected={str(f):q.common.digest_file(f) for f in [*q.DEFAULT_MODELS,q.common.DEFAULT_SOURCE/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe']}
    try:
        if a.reuse_build:
            bridge=q.prepare_bridge(a,out,q.bridge_builder(q.QT),r);runtime=q.prepare_runtime(a,out,bridge,r);r.data['runtime']=str(runtime)
        manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=source:raise RuntimeError('Stale runtime source')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime hash mismatch')
        r.data['runtime_sha256']=manifest['binaries_sha256'];r.case('runtime-manifest',True)
        values=codec_checks(r,runtime,out);service_checks(r,runtime,values,out/'service');production(r,runtime,out/'fixtures',out/'production')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==source);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==legacy)
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in tests.items()));r.case('models-protected',all(q.common.digest_file(Path(f))==h for f,h in protected.items()))
        r.case('external-files',status='not_run',reason='No real external Carlson Hull or Carene exported sample supplied.')
        r.case('visual-parity',status='review',reason='Hidden Qt dialog screenshots retained; full original-window layout parity is separate.')
        r.case('desktop',status='not_run',reason='No physical mouse/focus, native chooser or printer acceptance.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]


if __name__=='__main__':raise SystemExit(main())
