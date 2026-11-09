"""Native Archimedes and PAM export, independent geometry and Qt callback checks."""
import argparse,copy,json,math,os,subprocess,uuid
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_exchange as x
import qt_ghs as g
import ghs_reader
import ship_format_readers as readers

FILES=('qt_ship_formats.py','ship_format_readers.py','qt_ghs.py','ghs_reader.py','qt_migration.py','qt_exchange.py','run_tests.py')
FORMATS={'arch_hll':'hll','arch_app':'app','pam':'pam'}

def worker(runtime,model,input_data,out,fmt,**changes):
    out.mkdir(parents=True);mp=out/'model.json';ip=out/'input.json';dest=out/('船体 with spaces.'+FORMATS[fmt])
    x.dump(mp,model);x.dump(ip,input_data);dest.write_bytes(b'preserve')
    request={'model':str(mp),'input':str(ip),'output':str(dest),'document_id':model['document_id'],'revision':model['revision'],'units':'m','close_deck':True,'format':fmt}
    request.update(changes);rp=out/'request.json';x.dump(rp,request)
    with (out/'worker.log').open('wb') as log:
        run=subprocess.run([str(runtime/'FreeShipGeometryExport.exe'),str(rp),str(out/'result.json'),str(os.getpid())],stdout=log,stderr=subprocess.STDOUT,timeout=100,creationflags=q.common.FLAGS)
    result=json.loads((out/'result.json').read_text('utf8'));result['exit_code']=run.returncode
    if result['status']=='pass':result['geometry']=readers.read(dest,fmt)
    else:result['preserved']=dest.read_bytes()==b'preserve'
    return result,dest

def near(a,b,tol=1e-10):return math.isclose(a,b,rel_tol=tol,abs_tol=tol)

def analytic(r,runtime,base,input_base,out):
    m,d=g.prism(base,input_base);m['name']='船<&" Ж';m['settings'].update(name=m['name'],draft=2,waterline_z=1)
    for fmt in FORMATS:
        folder=out/fmt;got,path=worker(runtime,m,d,folder/'box',fmt);r.case(fmt+'.box',got['status']=='pass',result=got)
        if got['status']!='pass':continue
        geo=got['geometry'];pam=fmt=='pam';expected_x=[.5,2.5,3.5] if pam else [-1,0,2]
        r.case(fmt+'.reported-point-count',isinstance(got['half_section_points'],int) and got['half_section_points']>0 and got['points']==sum(len(s['points']) for s in geo['stations']))
        r.case(fmt+'.axis-origin', [s['x'] for s in geo['stations']]==expected_x and all(min(p[1] for p in s['points'])==(-2 if pam else -1) for s in geo['stations']))
        r.case(fmt+'.area-centroid',all(near(s['area'],8 if pam else 16) and near(s['centroid'][0],1 if pam else 0) and near(s['centroid'][1],0 if pam else 1) for s in geo['stations']))
        volume=sum((b['x']-a['x'])*(a['area']+b['area'])/2 for a,b in zip(geo['stations'],geo['stations'][1:]))*(2 if pam else 1)
        r.case(fmt+'.volume',near(volume,48),actual=volume)
        r.case(fmt+'.density',near(geo['density'],1025 if pam else 1.025))
        if fmt=='arch_hll':
            r.case('hll.xml-name-roundtrip',geo['title']==m['name'] and b'&amp;' in path.read_bytes())
            r.case('hll.longitudinal-marks',geo['references']=={'AP':0,'FP':5,'FWD_DRAUGHT_MARK':5,'MID_DRAUGHT_MARK':2.5,'AFT_DRAUGHT_MARK':0})
            invalid=copy.deepcopy(m);invalid['name']='Name\x01';invalid['settings']['name']=invalid['name']
            v,p=worker(runtime,invalid,d,folder/'invalid-xml-name',fmt);r.case('hll.reject-invalid-xml-character',v['status']=='error' and v.get('preserved'))
        if pam:r.case('pam.inherited-template',geo['parameters']==[1600,100,.2,2,1000,0] and geo['loading']==[0]*10 and geo['section_parameters']==[1,0])
        refs={'ap':-2,'fp':3,'aft_mark':-1.8,'mid_mark':.5,'fwd_mark':2.9,'midship':.5,'waterline':2.25}
        custom,p=worker(runtime,m,d,folder/'references',fmt,references=refs)
        r.case(fmt+'.explicit-references',custom['status']=='pass' and (custom['geometry']['references']['AP']==-2 and custom['geometry']['references']['FP']==3 if not pam else [v['x'] for v in custom['geometry']['stations']]==[-1.5,.5,1.5] and all(near(v['centroid'][1],-1.25) for v in custom['geometry']['stations'])),result=custom)
        imp=copy.deepcopy(m);imp['units']='imperial';imp['settings'].update(units='imperial',water_density=1.025*1000*.3048**3/.45359237);data=copy.deepcopy(d);data['settings']['imperial']=True
        # Explicitly interpret the unchanged simple box coordinates as feet.
        imp['settings']['waterline_z']=1
        converted,p=worker(runtime,imp,data,folder/'imperial',fmt)
        r.case(fmt+'.imperial-scale-and-density',converted['status']=='pass' and near(converted['geometry']['density'],geo['density']) and all(near(v['x'],w['x']*.3048) and near(v['centroid'][1],w['centroid'][1]*.3048) for v,w in zip(converted['geometry']['stations'],geo['stations'])) and all(near(s['area'],v['area']*.3048**2) for s,v in zip(converted['geometry']['stations'],geo['stations'])),result=converted)
        for label,change in [('hidden',lambda d:d['geometry']['layers'][0].update(surface_visible=False)),('negative-side',lambda d:[p['position'].__setitem__(1,-p['position'][1]) for p in d['geometry']['points']]),('simplify',lambda d:d['settings'].update(simplify=True))]:
            data=copy.deepcopy(d);change(data);v,p=worker(runtime,m,data,folder/label,fmt);r.case(fmt+'.'+label,v['status']=='pass' and v['geometry']==geo)
        # Closed modeled centerplane surface must not distort the reflected hull.
        data=copy.deepcopy(d);mesh=data['geometry']
        for strip in range(2):mesh['faces'].append({'points':[4*strip+3,4*(strip+1)+3,4*(strip+1),4*strip],'layer':0})
        edges={tuple(sorted(e['points'])) for e in mesh['edges']}
        for face in mesh['faces']:
            for a,b in zip(face['points'],face['points'][1:]+face['points'][:1]):edges.add(tuple(sorted((a,b))))
        mesh['edges']=[{'points':list(pair),'crease':True,'curve':-1} for pair in sorted(edges)]
        v,p=worker(runtime,m,data,folder/'centerplane',fmt);r.case(fmt+'.centerplane-area',v['status']=='pass' and all(near(s['area'],8 if pam else 16) for s in v['geometry']['stations']),result=v)
        data=copy.deepcopy(d);mesh=data['geometry'];offset=len(mesh['points']);extra=copy.deepcopy(mesh['points'])
        for p in extra:p['position'][2]+=6
        mesh['points']+=extra;mesh['edges']+=[dict(v,points=[i+offset for i in v['points']]) for v in list(mesh['edges'])];mesh['faces']+=[dict(v,points=[i+offset for i in v['points']]) for v in list(mesh['faces'])]
        v,p=worker(runtime,m,data,folder/'two-lobes',fmt)
        r.case(fmt+'.two-lobes-independent-moments',v['status']=='pass' and len(v['centerline_lobes'])==3 and all(near(s['area'],16 if pam else 32) and near(s['centroid'][0],1 if pam else 0) and near(s['centroid'][1],3 if pam else 4) for s in v['geometry']['stations']),result=v)
        opened,od=g.prism(m,d,True);v,p=worker(runtime,opened,od,folder/'closed-deck',fmt);r.case(fmt+'.explicit-close-deck',v['status']=='pass' and len(v['closed_decks'])==3 and all(near(s['area'],8 if pam else 16) for s in v['geometry']['stations']))
        cases=[('no-close-consent',opened,od,{'close_deck':False}),('bad-units',m,d,{'units':'ft'}),('stale',m,d,{'revision':9999}),('bad-reference',m,d,{'references':{'waterline':'bad'}}),('unknown-format',m,d,{'format':'unknown'})]
        if not pam:cases.append(('reversed-perpendiculars',m,d,{'references':{'ap':3,'fp':-2}}))
        for label,change in [('no-stations',lambda d:d.update(sections=[])),('asymmetric',lambda d:d['geometry']['layers'][0].update(symmetric=False)),('bottom-gap',lambda d:[p['position'].__setitem__(1,p['position'][1]+.001) for p in d['geometry']['points']])]:
            data=copy.deepcopy(d);change(data);cases.append((label,m,data,{}))
        for label,model,data,options in cases:
            v,p=worker(runtime,model,data,folder/label,fmt,**options);r.case(fmt+'.reject-'+label,v['status']=='error' and v.get('preserved') and v['exit_code']!=0,result=v)
        # Independent readers must reject a damaged count, not just read success files.
        broken=folder/('corrupt.'+FORMATS[fmt]);raw=path.read_text('utf8')
        if fmt=='arch_hll':raw=raw.replace('<SECTION_COUNT>3</SECTION_COUNT>','<SECTION_COUNT>4</SECTION_COUNT>')
        elif fmt=='arch_app':raw=raw.replace('NUMBER_OF_FRAMES\n3','NUMBER_OF_FRAMES\n4')
        else:raw=raw.replace('3 1. 0.','4 1. 0.')
        broken.write_text(raw,'utf8');rejected=False
        try:readers.read(broken,fmt)
        except (ValueError,IndexError,TypeError):rejected=True
        r.case(fmt+'.reader-negative-control',rejected)

def exercise(r,runtime,model,out):
    out.mkdir(parents=True);results={}
    with q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'service') as service:
        before=service.command('load',path=str(model),document_id=str(uuid.uuid4()),output=out/'loaded');q.require_ok(before)
        m=json.loads(Path(before['model']).read_text('utf8'));inp=json.loads((Path(before['model']).parent/'native-input.json').read_text('utf8'));has_stations=any(s['kind']==1 for s in inp['sections'])
        ghs,gp=g.worker(runtime,m,inp,out/'ghs-reference')
        for fmt in FORMATS:
            got,path=worker(runtime,m,inp,out/fmt,fmt);results[fmt]=got;r.case('real.'+fmt+'.export',got['status']==('pass' if has_stations else 'error'),result=got)
            if got['status']=='pass':
                repeat,rp=worker(runtime,m,inp,out/(fmt+'-repeat'),fmt);r.case('real.'+fmt+'.determinism',repeat['status']=='pass' and path.read_bytes()==rp.read_bytes())
                # Independently transform GHS station coordinates and polygon moments.
                gs={-s['x']:s for s in ghs['geometry']['stations']};scale=.3048 if m['units']=='imperial' else 1
                geometry=got['geometry'];ok=True
                for st in geometry['stations']:
                    source_x=m['settings']['length']*scale/2-st['x'] if fmt=='pam' else st['x']
                    src=min(gs.values(),key=lambda s:abs(-s['x']-source_x));full=1 if fmt=='pam' else 2
                    height=src['centroid'][1]-(m['settings']['waterline_z']*scale if fmt=='pam' else 0)
                    ok &= near(-src['x'],source_x) and near(st['area'],src['area']*full) and near(st['centroid'][1],height)
                r.case('real.'+fmt+'.section-integrals-agree',ok)
        if has_stations:
            first=next(p for p in m['control_points'] if not p['locked'] and p.get('batch_movable',True));position=list(first['p']);position[2]+=.01
            moved=service.command('move_point',point_id=first['id'],position=position,output=out/'moved');q.require_ok(moved)
            mm=json.loads(Path(moved['model']).read_text('utf8'));md=json.loads((Path(moved['model']).parent/'native-input.json').read_text('utf8'))
            for fmt in FORMATS:
                v,p=worker(runtime,mm,md,out/(fmt+'-edited'),fmt);r.case('edit.'+fmt+'.refresh',v['status']=='pass' and v['geometry']!=results[fmt]['geometry'])
            undo=service.command('undo',output=out/'undo');q.require_ok(undo);um=json.loads(Path(undo['model']).read_text('utf8'));ud=json.loads((Path(undo['model']).parent/'native-input.json').read_text('utf8'))
            for fmt in FORMATS:
                v,p=worker(runtime,um,ud,out/(fmt+'-undo'),fmt);r.case('edit.'+fmt+'.undo',v['status']=='pass' and v['geometry']==results[fmt]['geometry'])
        after=service.command('snapshot',output=out/'after');q.require_ok(after)
        # Undo increments revision; geometry, scene and DXF still restore exactly.
        for k in ('scene','dxf','sections'):r.compare('real.unchanged-'+k,before[k],after[k])
    qt=out/'qt';qt.mkdir();protected=qt/'preserved.fbm';protected.write_bytes(b'preserve');commands=[]
    for fmt,ext in FORMATS.items():
        commands += [{'name':fmt,'method':'export','format':fmt,'path':str(qt/('用户 船体.'+ext)),'options':{'close_deck':True},'expect_failure':not has_stations},
                     {'name':fmt+'-bad-extension','method':'export','format':fmt,'path':str(protected),'expect_failure':True}]
        if has_stations:
            commands += [{'name':fmt+'-cancel','method':'export','format':fmt,'path':str(qt/('cancelled.'+ext)),'options':{'close_deck':True},'cancel':True},
                         {'name':fmt+'-bad-position','method':'export','format':fmt,'path':str(qt/('invalid.'+ext)),'options':{'references':{'waterline' if fmt=='pam' else 'ap':'not-a-number'}},'expect_failure':True},
                         {'name':fmt+'-references','method':'export','format':fmt,'path':str(qt/('references.'+ext)),'options':{'close_deck':True,'references':{'midship':7,'waterline':3} if fmt=='pam' else {'ap':-2,'fp':200,'aft_mark':-1,'mid_mark':99,'fwd_mark':199}}}]
    req=out/'workflow.json';x.dump(req,{'model':str(model),'commands':commands});child=q.run_child(r,'production-ship',runtime/'FreeShipQt.exe',['--verify-exchange',req],qt,timeout=300,platform='windows')
    r.case('ui.protected-file',protected.read_bytes()==b'preserve')
    for fmt,ext in FORMATS.items():
        if has_stations:
            r.case('ui.'+fmt+'.no-cancel-output',not(qt/('cancelled.'+ext)).exists());r.case('ui.'+fmt+'.no-invalid-output',not(qt/('invalid.'+ext)).exists())
            r.case('ui.'+fmt+'.same-geometry',readers.read(qt/('用户 船体.'+ext),fmt)==results[fmt]['geometry'])
            custom=readers.read(qt/('references.'+ext),fmt)
            if fmt=='pam':ok=all(near(s['x']+src['x'],7) and near(s['centroid'][1],src['centroid'][1]-3) for s,src in zip(custom['stations'],reversed(results['arch_hll']['geometry']['stations'])))
            else:ok=custom['references']['AP']==-2 and custom['references']['FP']==200 and custom['stations']==results[fmt]['geometry']['stations']
            if fmt=='arch_hll':ok &= custom['references']['AFT_DRAUGHT_MARK']==-1 and custom['references']['MID_DRAUGHT_MARK']==99 and custom['references']['FWD_DRAUGHT_MARK']==199
            r.case('ui.'+fmt+'.edited-references',ok)
        if child:
            for k in ('model','scene','dxf'):r.compare('ui.'+fmt+'.unchanged-'+k,child['artifacts'][fmt+'_'+k],child['artifacts']['original_'+k])
    return m,inp

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT);p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--model',type=Path,action='append')
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data['suite']='qt-ship-formats';before=q.common.fingerprint(a.source)[0];sources=q.current_sources(a.qt_source);hashes={n:q.common.digest_file(q.ROOT/n) for n in FILES};r.data.update(source_sha256=before,qt_sources_sha256=sources,test_sources_sha256=hashes)
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
        r.case('archimedes-receiver',status='not_run',reason='Archimedes executables unavailable; XML/APP parsing and polygon moments are independently checked, not receiver certification')
        r.case('pam-receiver',status='not_run',reason='Legacy Windows PAM template; target solver and parameter specification unavailable. Not Add_Mass.EXE INA. protocol.')
        e.write_report(r,out)
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'report':str(out/'report.json')}));return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]
if __name__=='__main__':raise SystemExit(main())
