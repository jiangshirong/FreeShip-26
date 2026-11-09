"""GHS native export: analytic geometry, failure preservation and real Qt callbacks."""
import argparse,copy,json,os,subprocess,uuid,math
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_exchange as x
import ghs_reader

FILES=('qt_ghs.py','ghs_reader.py','qt_migration.py','qt_exchange.py','qt/exchange_tests.cpp','run_tests.py')

def worker(runtime,model,input_data,out,units='m',close=True,request_change=None):
    out.mkdir(parents=True);mp=out/'model.json';ip=out/'input.json';dest=out/'船体 with spaces.gf'
    x.dump(mp,model);x.dump(ip,input_data);dest.write_bytes(b'preserve')
    request={'model':str(mp),'input':str(ip),'output':str(dest),'document_id':model['document_id'],'revision':model['revision'],'units':units,'close_deck':close}
    if request_change:request.update(request_change)
    rp=out/'request.json';x.dump(rp,request)
    with (out/'worker.log').open('wb') as log:
        run=subprocess.run([str(runtime/'FreeShipGeometryExport.exe'),str(rp),str(out/'result.json'),str(os.getpid())],stdout=log,stderr=subprocess.STDOUT,timeout=100,creationflags=q.common.FLAGS)
    result=json.loads((out/'result.json').read_text('utf8'));result['exit_code']=run.returncode
    if result['status']=='pass':result['geometry']=ghs_reader.read(dest)
    else:result['preserved']=dest.read_bytes()==b'preserve'
    return result,dest

def prism(base,input_base,open_deck=False):
    m=copy.deepcopy(base);d=copy.deepcopy(input_base);m['units']='metric';m['settings'].update(units='metric',length=5,beam=4,water_density=1.025,particulars_set=True)
    d['settings']['imperial']=False;g=d['geometry'];g.update(subdivision_mode=0,precision=1,desired_level=2,points=[],faces=[],edges=[],curves=[])
    layer=copy.deepcopy(g['layers'][0]);layer.update(symmetric=True,use_in_hydrostatics=True,surface_visible=True,use_for_intersections=False)
    g['layers']=[layer];cross=[(0,-1),(2,-1),(2,3),(0,3)]
    for xx in (-2,0,3):
        for y,z in cross:g['points'].append({'position':[xx,y,z],'type':3})
    for strip in range(2):
        for j in range(2 if open_deck else 3):g['faces'].append({'points':[4*strip+j,4*(strip+1)+j,4*(strip+1)+j+1,4*strip+j+1],'layer':0})
    edges=set()
    for f in g['faces']:
        for a,b in zip(f['points'],f['points'][1:]+f['points'][:1]):edges.add(tuple(sorted((a,b))))
    g['edges']=[{'points':list(ends),'crease':True,'curve':-1} for ends in sorted(edges)]
    d['sections']=[{'kind':1,'order':i,'plane':[1,0,0,-v]} for i,v in enumerate((-1,0,2))]
    return m,d

def analytic(r,runtime,base,input_base,out):
    m,d=prism(base,input_base);result,path=worker(runtime,m,d,out/'box');r.case('analytic.box',result['status']=='pass',result=result)
    capped=copy.deepcopy(d);g=capped['geometry']
    for strip in range(2):g['faces'].append({'points':[4*strip+3,4*(strip+1)+3,4*(strip+1),4*strip],'layer':0})
    edges={tuple(sorted(e['points'])) for e in g['edges']}
    for f in g['faces']:
        for a,b in zip(f['points'],f['points'][1:]+f['points'][:1]):edges.add(tuple(sorted((a,b))))
    g['edges']=[{'points':list(pair),'crease':True,'curve':-1} for pair in sorted(edges)]
    cap,p=worker(runtime,m,capped,out/'centerplane-face');r.case('analytic.centerplane-face',cap['status']=='pass' and all(st['area']==8 and st['centroid']==[1,1] for st in cap['geometry']['stations']),result=cap)
    if result['status']=='pass':
        geo=result['geometry'];r.case('analytic.origin-axis-stations',[v['x'] for v in geo['stations']]==[-2,0,1] and all(min(p[1] for p in st['points'])==-1 for st in geo['stations']))
        r.case('analytic.area-centroid',all(abs(st['area']-8)<1e-10 and max(abs(a-b) for a,b in zip(st['centroid'],[1,1]))<1e-10 for st in geo['stations']))
        vol=sum((b['x']-a['x'])*(a['area']+b['area']) for a,b in zip(geo['stations'],geo['stations'][1:]));r.case('analytic.full-displacement-volume',abs(vol-48)<1e-10,volume=vol)
    for label,change in [('hidden',lambda d:d['geometry']['layers'][0].update(surface_visible=False)),('negative-side',lambda d:[p['position'].__setitem__(1,-p['position'][1]) for p in d['geometry']['points']]),('simplify',lambda d:d['settings'].update(simplify=True))]:
        data=copy.deepcopy(d);change(data);got,p=worker(runtime,m,data,out/label);r.case('analytic.'+label,got['status']=='pass' and got['geometry']==result['geometry'],result=got)
    got,p=worker(runtime,m,d,out/'feet',units='ft');r.case('analytic.m-to-ft',got['status']=='pass' and got['geometry']['header']['P']=='F' and all(math.isclose(a['area'],b['area']/.3048**2,rel_tol=1e-12) for a,b in zip(got['geometry']['stations'],result['geometry']['stations'])))
    imp=copy.deepcopy(m);imp['units']='imperial';imp['settings']['units']='imperial';imp['settings']['water_density']=1.025*1000*.3048**3/.45359237;data=copy.deepcopy(d);data['settings']['imperial']=True
    got,p=worker(runtime,imp,data,out/'imperial',units='ft');r.case('analytic.imperial-density',got['status']=='pass' and math.isclose(got['geometry']['density'],1.025,rel_tol=1e-14) and got['geometry']['stations']==result['geometry']['stations'])
    opened,od=prism(base,input_base,True);got,p=worker(runtime,opened,od,out/'close-deck');r.case('analytic.explicit-deck',got['status']=='pass' and len(got['closed_decks'])==3 and all(st['area']==8 and st['centroid']==[1,1] for st in got['geometry']['stations']))
    data=copy.deepcopy(d);data['sections'].append({'kind':1,'order':9,'plane':[1,0,0,10]});got,p=worker(runtime,m,data,out/'outside');r.case('analytic.empty-station-reported',got['status']=='pass' and got['empty_stations']==[-10] and len(got['geometry']['stations'])==3)
    cases=[('open-without-consent',opened,od,{'close':False}),('invalid-units',m,d,{'units':'mm'}),('stale-request',m,d,{'request_change':{'revision':9999}})]
    for label,change in [('asymmetric',lambda d:d['geometry']['layers'][0].update(symmetric=False)),('no-hydro',lambda d:d['geometry']['layers'][0].update(use_in_hydrostatics=False)),('stale-input',lambda d:d.update(revision=9999)),('no-stations',lambda d:d.update(sections=[])),('duplicate-station',lambda d:d['sections'].append(copy.deepcopy(d['sections'][0]))),('bottom-gap',lambda d:[p['position'].__setitem__(1,p['position'][1]+.001) for p in d['geometry']['points']]),('cross-center',lambda d:d['geometry']['points'][1]['position'].__setitem__(1,-2))]:
        data=copy.deepcopy(d);change(data);cases.append((label,m,data,{}))
    # A separated second half-hull must not be bridged to the first.
    data=copy.deepcopy(d);g=data['geometry'];count=len(g['points']);extra=copy.deepcopy(g['points'])
    for p in extra:p['position'][1]+=5
    g['points']+=extra;g['edges'] += [dict(v,points=[i+count for i in v['points']]) for v in list(g['edges'])];g['faces'] += [dict(v,points=[i+count for i in v['points']]) for v in list(g['faces'])]
    cases.append(('disjoint',m,data,{}))
    # A bulbous-bow-like pair of vertically separated lobes meets centerline
    # on both ends. The connector must add exactly zero area or moment.
    data=copy.deepcopy(d);g=data['geometry'];count=len(g['points']);extra=copy.deepcopy(g['points'])
    for p in extra:p['position'][2]+=6
    g['points']+=extra;g['edges'] += [dict(v,points=[i+count for i in v['points']]) for v in list(g['edges'])];g['faces'] += [dict(v,points=[i+count for i in v['points']]) for v in list(g['faces'])]
    got,p=worker(runtime,m,data,out/'centerline-lobes');r.case('analytic.centerline-lobes-area-moment',got['status']=='pass' and len(got['centerline_lobes'])==3 and all(st['area']==16 and st['centroid']==[1,4] for st in got['geometry']['stations']),result=got)
    # A visible asymmetric non-hydrostatic layer is deliberately excluded.
    data['geometry']['layers'].append(dict(data['geometry']['layers'][0],use_in_hydrostatics=False,symmetric=False))
    for face in data['geometry']['faces'][len(d['geometry']['faces']):]:face['layer']=1
    got,p=worker(runtime,m,data,out/'non-hydro-excluded');r.case('analytic.hydro-layer-scope',got['status']=='pass' and got['geometry']==result['geometry'],result=got)
    # Self-crossing section geometry must fail even when its endpoints are on CL.
    crossed=copy.deepcopy(d)
    for pt in crossed['geometry']['points']:
        y,z=pt['position'][1:]
        if y==2 and z==-1:pt['position'][2]=3
        elif y==2 and z==3:pt['position'][2]=-1
    cases.append(('self-intersecting',m,crossed,{}))
    for label,model,data,options in cases:
        got,p=worker(runtime,model,data,out/label,**options);r.case('reject.'+label,got['status']=='error' and got.get('preserved') and got['exit_code']!=0,result=got)
    requests=[{'name':'natural','method':'ghs_curve','points':[[0,0,0],[0,1,1],[0,0,2]],'knuckles':[False]*3,'tolerance':.0001}]
    requests += [dict(requests[0],name='knuckles',knuckles=[True]*3),dict(requests[0],name='invalid-tolerance',tolerance=0)]
    curves=x.probe(runtime/'qt_exchange_tests.exe',requests,out/'cubic-probe');r.case('curve.reject-zero-tolerance',not curves['invalid-tolerance']['ok'])
    r.case('curve.preserve-knuckles',curves['knuckles']['ok'] and curves['knuckles']['points']==[[0,0],[1,1],[0,2]])
    def dist(p,a,b):
        dy=b[0]-a[0];dz=b[1]-a[1];den=dy*dy+dz*dz;t=max(0,min(1,((p[0]-a[0])*dy+(p[1]-a[1])*dz)/den))
        return math.hypot(p[0]-a[0]-t*dy,p[1]-a[1]-t*dz)
    pts=curves['natural'].get('points',[]);error=0
    if pts:
        for j in range(2001):
            z=j/1000;t=z if z<=1 else 2-z;p=(1.5*t-.5*t**3,z)
            error=max(error,min(dist(p,a,b) for a,b in zip(pts,pts[1:])))
    r.case('curve.independent-cubic-polynomial',curves['natural']['ok'] and error<=.0001001 and [1,1] in pts,maximum_error=error,points=len(pts))

def exercise(r,runtime,model,out):
    out.mkdir(parents=True)
    with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'service') as service:
        before=service.command('load',path=str(model),document_id=str(uuid.uuid4()),output=out/'loaded');q.require_ok(before)
        m=json.loads(Path(before['model']).read_text('utf8'));inp=json.loads((Path(before['model']).parent/'native-input.json').read_text('utf8'))
        got,path=worker(runtime,m,inp,out/'export');has_stations=any(s['kind']==1 for s in inp['sections'])
        r.case('real-model.export' if has_stations else 'real-model.no-stations',got['status']==('pass' if has_stations else 'error'),result=got)
        if got['status']=='pass':
            repeat,repeat_path=worker(runtime,m,inp,out/'repeat');r.case('real-model.repeat-deterministic',repeat['status']=='pass' and path.read_bytes()==repeat_path.read_bytes())
        after=service.command('snapshot',output=out/'after');q.require_ok(after)
        r.case('real-model.state-unchanged',all(before[k]==after[k] for k in q.STATE_KEYS))
        for k in ('model','scene','dxf','sections'):r.compare('real-model.unchanged-'+k,before[k],after[k])
        if has_stations and got['status']=='pass':
            first=next(p for p in m['control_points'] if not p['locked'] and p.get('batch_movable',True))
            position=list(first['p']);position[2]+=.01
            moved=service.command('move_point',point_id=first['id'],position=position,output=out/'moved');q.require_ok(moved)
            mm=json.loads(Path(moved['model']).read_text('utf8'));md=json.loads((Path(moved['model']).parent/'native-input.json').read_text('utf8'))
            changed,cp=worker(runtime,mm,md,out/'edited-export');r.case('edit.refreshes-export',changed['status']=='pass' and changed['geometry']!=got['geometry'],result=changed)
            undo=service.command('undo',output=out/'undo');q.require_ok(undo)
            um=json.loads(Path(undo['model']).read_text('utf8'));ud=json.loads((Path(undo['model']).parent/'native-input.json').read_text('utf8'))
            restored,rp=worker(runtime,um,ud,out/'undo-export');r.case('edit.undo-restores-export',restored['status']=='pass' and restored['geometry']==got['geometry'])
    qt=out/'qt';qt.mkdir();dest=qt/'用户 船体.gf';protected=qt/'preserved.fbm';protected.write_bytes(b'preserve')
    req=out/'workflow.json';commands=[
        {'name':'ghs','method':'export','format':'ghs','path':str(dest),'options':{'close_deck':True},'expect_failure':not has_stations},
        {'name':'bad-extension','method':'export','format':'ghs','path':str(protected),'expect_failure':True}]
    if has_stations:commands.append({'name':'cancel-ghs','method':'export','format':'ghs','path':str(qt/'cancelled.gf'),'options':{'close_deck':True},'cancel':True})
    x.dump(req,{'model':str(model),'commands':commands})
    child=q.run_child(r,'production-ghs',runtime/'FreeShipQt.exe',['--verify-exchange',req],qt,timeout=300,platform='windows')
    r.case('ui.protected-model',protected.read_bytes()==b'preserve')
    if has_stations:r.case('ui.cancel-no-output',not(qt/'cancelled.gf').exists())
    if has_stations and dest.exists() and got['status']=='pass':r.case('ui.same-as-native-worker',ghs_reader.read(dest)==got['geometry'])
    if child:
        for k in ('model','scene','dxf'):r.compare('ui.unchanged-'+k,child['artifacts']['ghs_'+k],child['artifacts']['original_'+k])
    return m,inp

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT);p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--model',type=Path,action='append')
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data['suite']='qt-ghs';before=q.common.fingerprint(a.source)[0];sources=q.current_sources(a.qt_source);hashes={n:q.common.digest_file(q.ROOT/n) for n in FILES};r.data.update(source_sha256=before,qt_sources_sha256=sources,test_sources_sha256=hashes)
    models=[f.resolve() for f in (a.model or q.DEFAULT_MODELS)];protected={str(f):q.common.digest_file(f) for f in [*models,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe'] if f.exists()}
    try:
        if a.verify_deployment:
            runtime=a.verify_deployment.resolve();manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
            if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Installed source manifest stale')
            for n,h in manifest['binaries_sha256'].items():
                if q.common.digest_file(runtime/n)!=h:raise RuntimeError('Installed binary mismatch '+n)
        else:runtime=q.prepare_runtime(a,out,q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),r),r)
        r.data['runtime_path']=str(runtime)
        for i,model in enumerate(models):
            try:
                m,d=exercise(r,runtime,model,out/('model-'+str(i+1)))
                if i==0:analytic(r,runtime,m,d,out/'analytic')
            except Exception as ex:r.case('model-'+str(i+1)+'.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==before);r.case('qt-source-unchanged',q.current_sources(a.qt_source)==sources);r.case('test-sources-unchanged',all(q.common.digest_file(q.ROOT/n)==h for n,h in hashes.items()))
        for f,h in protected.items():r.case('protected.'+f,q.common.digest_file(Path(f))==h)
        r.case('visual-fidelity',status='review',reason='Hidden production Windows dialogs, not physical desktop acceptance')
        r.case('commercial-ghs',status='not_run',reason='GHS application and current full Appendix A unavailable; independent subset reader is not GHS certification')
        e.write_report(r,out)
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'report':str(out/'report.json')}));return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]
if __name__=='__main__':raise SystemExit(main())
