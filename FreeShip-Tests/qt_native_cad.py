"""Native IGES preparation: legacy geometry oracle and document ownership.

Used by qt-cad; --runtime/--output also runs the focused geometry diagnostic.
All generated models and processes are private. No installed/user files change.
"""
import argparse
import copy
import json
import math
import time
from collections import Counter
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_exchange as x
import qt_document


def data(response):
    q.require_ok(response)
    return json.loads(Path(response['cad']).read_text('utf8'))


def compare(r, key, native, legacy):
    a, b = native['patches'], legacy['patches']
    fields = ('layer', 'cols', 'rows', 'degree_u', 'degree_v')
    signature = lambda p: tuple(p[k] for k in fields)
    r.case(key+'.patch-layout', Counter(map(signature,a))==Counter(map(signature,b)),
           native=len(a), legacy=len(b))
    available = list(b); differences=[]; missing=[]
    for p in a:
        candidates=[v for v in available if signature(p)==signature(v)]
        if not candidates:missing.append(signature(p));continue
        def distance(v):return max(abs(c-d) for u,w in zip(p['points'],v['points']) for c,d in zip(u,w))
        v=min(candidates,key=distance);available.remove(v)
        examples=[dict(index=i,native=u,legacy=w) for i,(u,w) in enumerate(zip(p['points'],v['points'])) if u!=w]
        differences.append(dict(maximum_error=distance(v),knots_equal=p['u']==v['u'] and p['v']==v['v'],
                                cols=p['cols'],rows=p['rows'],examples=examples[:8]))
    r.case(key+'.control-points-and-knots',not missing and not available and
           all(v['maximum_error']==0 and v['knots_equal'] for v in differences),
           maximum_error=max((v['maximum_error'] for v in differences),default=0),
           missing=missing,unmatched_legacy=len(available),differences=differences)


def paired(r,runtime,model,out):
    values=[];times=[]
    for label,exe in (('native',runtime/'FreeShipDocumentService.exe'),('legacy',runtime/'bridge/FreeShipGeometryBridge.exe')):
        with q.Service(exe,out/label/'process',native_linesplan=True) as s:
            before=q.require_ok(s.command('load',path=str(model),document_id=str(q.uuid.uuid4()),output=out/label/'load'))
            start=time.perf_counter();response=s.command('cad_iges',output=out/label/'cad');times.append((time.perf_counter()-start)*1000)
            value=data(response);values.append(value)
            if label=='native':r.case(out.name+'.native-engine',response.get('analysis_engine')=='cpp-cad')
            r.case(out.name+'.'+label+'.readonly',all(s.state[k]==before[k] for k in q.STATE_KEYS))
    compare(r,out.name,*values)
    r.data.setdefault('native_cad_timings',[]).append(dict(model=str(model),native_ms=times[0],legacy_ms=times[1]))


def fixture(path,points,faces,layers=None):
    """Independent FEF: smooth internal edges, creased boundary, explicit types."""
    layers=layers or [(True,False)]
    uses=Counter(tuple(sorted((a,b))) for f,_ in faces for a,b in zip(f,f[1:]+f[:1]))
    crease={p:0 for p in range(len(points))}
    for (a,b),count in uses.items():
        if count==1:crease[a]+=1;crease[b]+=1
    # Interior grid points are regular; polygon vertices on a straight boundary
    # are crease points, with the four actual corners retained as corners.
    lines=['Native CAD fixture','Designer','Author','analytic geometry','10 4 1 1.025 1 0 -1 0',str(len(layers))]
    for i,(visible,symmetric) in enumerate(layers):lines += [f'Layer {i+1}',f'{i+1} {255+i*256} {-int(visible)} 0 {-int(symmetric)} -1 -1 -1 0 0']
    lines.append(str(len(points)))
    for i,p in enumerate(points):
        neighbors=[b if a==i else a for (a,b),n in uses.items() if n==1 and i in (a,b)]
        straight=len(neighbors)==2 and math.dist(points[neighbors[0]],points[neighbors[1]])>=math.dist(points[neighbors[0]],p)+math.dist(points[neighbors[1]],p)-1e-10
        kind=0 if not crease[i] else 1 if straight else 3
        lines.append(' '.join(map(str,(*p,kind,0))))
    lines.append(str(len(uses)))
    for (a,b),n in uses.items():lines.append(f'{a} {b} {int(n==1)} 0')
    lines.append(str(len(faces)))
    for f,layer in faces:lines.append(' '.join(map(str,(len(f),*f,layer,0))))
    path.write_text('\n'.join(lines)+'\n',encoding='utf8')


def fixtures(r,runtime,out):
    out.mkdir(parents=True);sources={}
    for name in ('plane','saddle','hole','layers','hidden'):
        points=[(x,y,0 if name!='saddle' else .125*x*y) for y in range(4) for x in range(4)]
        faces=[([y*4+x,y*4+x+1,(y+1)*4+x+1,(y+1)*4+x],int(x>=1) if name in ('layers','hidden') else 0) for y in range(3) for x in range(3) if name!='hole' or (x,y)!=(1,1)]
        layer_flags=[(True,False),(name!='hidden',True)] if name in ('layers','hidden') else None
        path=out/(name+'.fef');fixture(path,points,faces,layer_flags);sources[name]=path
    for n in (3,5):
        name=f'polygon-{n}';path=out/(name+'.fef');fixture(path,[(math.cos(2*math.pi*i/n),math.sin(2*math.pi*i/n),.1*(i%2)) for i in range(n)],[(list(range(n)),0)]);sources[name]=path
    requests=[dict(name=name,method='read',format='fef',path=str(path)) for name,path in sources.items()]
    parsed=x.probe(runtime/'qt_exchange_tests.exe',requests,out/'parse');models={}
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        for name in sources:
            r.case('fixture.'+name,parsed[name]['ok']);payload=out/(name+'.json');x.dump(payload,parsed[name]['data'])
            q.require_ok(s.command('import_model',path=str(payload),document_id=str(q.uuid.uuid4()),output=out/name/'import'))
            saved=q.require_ok(s.command('save',output=out/name/'save'));models[name]=Path(saved['saved_file'])
    return models


def ownership(r,runtime,models,out):
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process',native_linesplan=True) as s:
        loaded=q.require_ok(s.command('load',path=str(models['saddle']),document_id=str(q.uuid.uuid4()),output=out/'load'))
        a=data(s.command('cad_iges',output=out/'a'));state=dict(s.state)
        before=q.require_ok(s.command('save',output=out/'save-before'))
        b=data(s.command('cad_iges',output=out/'repeat'))
        after=q.require_ok(s.command('save',output=out/'save-after'))
        r.case('native.repeat-identical',a==b);r.compare('native.fbm-readonly',before['saved_file'],after['saved_file'])
        model=json.loads(Path(loaded['model']).read_text('utf8'));point=model['control_points'][5];p=point['p'];p[2]+=.25
        q.require_ok(s.command('move_point',point_id=point['id'],position=p,output=out/'move'))
        b=data(s.command('cad_iges',output=out/'edited'));r.case('native.edit-recomputed',a['patches']!=b['patches'])
        q.require_ok(s.command('undo',output=out/'undo'));c=data(s.command('cad_iges',output=out/'undone'))
        r.case('native.undo-restores',a['patches']==c['patches'])
        q.require_ok(s.command('redo',output=out/'redo'));d=data(s.command('cad_iges',output=out/'redone'))
        r.case('native.redo-restores',b['patches']==d['patches'])
        state=dict(s.state);snap=q.require_ok(s.command('snapshot',output=out/'snapshot'))
        detached=data(s.command('cad_iges',output=out/'detached'))
        r.case('native.without-pascal',snap.get('adapter_pid')==0 and snap.get('adapter_requests')==0 and detached==d)
        blocked=out/'blocked';(blocked/'cad.json').mkdir(parents=True);marker=blocked/'cad.json/keep';marker.write_text('keep')
        failure=s.command('cad_iges',output=blocked);r.case('native.atomic-failure',not failure['ok'] and marker.read_text()=='keep' and s.state==state)
        retry=data(s.command('cad_iges',output=out/'retry'));r.case('native.retry',retry==d)
    with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'guard-process') as s:
        q.require_ok(s.command('load',path=str(models['plane']),document_id=str(q.uuid.uuid4()),output=out/'guard-load'))
        response=s.command('cad_iges',external_document=True,output=out/'guard')
        r.case('native.forbid-pascal-fallback',not response['ok'] and 'must execute in C++' in response.get('error',''))


def layers(r,runtime,models,out,oracle):
    from qt_cad import verify_iges,run_oracle
    for name in ('layers','hidden'):
        folder=out/name;folder.mkdir(parents=True)
        with q.Service(runtime/'FreeShipDocumentService.exe',folder/'process') as s:
            loaded=q.require_ok(s.command('load',path=str(models[name]),document_id=str(q.uuid.uuid4()),output=folder/'load'))
            response=s.command('cad_iges',output=folder/'cad');value=data(response)
            m=json.loads(Path(loaded['model']).read_text('utf8'))
            r.case(name+'.retain-layer-ownership',{p['layer'] for p in value['patches']}=={l['id'] for l in m['layers']})
            for hidden in (False,True):
                label='all' if hidden else 'visible';path=folder/(label+'.igs');opts={'include_hidden':hidden,'full_hull':False}
                result=x.probe(runtime/'qt_exchange_tests.exe',[dict(name=label,method='write',format='iges',path=str(path),model=loaded['model'],patches=response['cad'],options=opts)],folder/label)
                r.case(name+'.'+label+'.write',result[label]['ok']);verify_iges(r,name+'.'+label,path,value,m,opts)
                run_oracle(r,name+'.'+label,path,folder,oracle)
                occt=folder/(name+'.'+label+'-occt.json')
                if occt.exists():
                    area=9 if hidden or name=='layers' else 3
                    r.case(name+'.'+label+'.analytic-area',math.isclose(json.loads(occt.read_text('utf8'))['area_mm2'],area*1e6,rel_tol=1e-6))


def run(r,runtime,out,oracle):
    models=fixtures(r,runtime,out/'fixtures')
    for name in ('plane','saddle','hole','polygon-3','polygon-5'):paired(r,runtime,models[name],out/name)
    ownership(r,runtime,models,out/'ownership')
    layers(r,runtime,models,out/'layers',oracle)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,action='append')
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data['suite']='qt-native-cad-diagnostic'
    try:
        for i,m in enumerate(a.model or q.DEFAULT_MODELS):paired(r,a.runtime,m,out/('model-'+str(i+1)))
        run(r,a.runtime,out/'native',Path('C:/FreeShip26Tests/tools/cad-oracle/Scripts/python.exe'))
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    e.write_report(r,out);print(json.dumps({'counts':r.data['counts'],'report':str(out/'report.json')}))
    return 1 if r.data['counts'].get('fail',0) or r.data['counts'].get('error',0) else 0

if __name__=='__main__':raise SystemExit(main())
