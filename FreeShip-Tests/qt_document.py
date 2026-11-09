"""Native FBM/document authority: mixed edits, history, failure recovery and UI.

No desktop automation. Reuses existing independent modeling expectations.
Outputs are private; source models and installed programs are fingerprinted.
"""
import argparse,ctypes,json,os,subprocess,time,uuid
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_modeling as modeling
import native_linesplan_app as native
import qt_point_edit
import qt_native_topology
import qt_native_modeling
import qt_native_transforms
import qt_native_construction
import qt_native_lackenby
import qt_native_attributes
import qt_native_face_tools
import qt_native_project
import qt_native_definitions
import qt_native_auxiliary
import qt_native_background
import qt_native_surface
import qt_native_create
import qt_native_analysis
import qt_native_flowlines
import qt_native_hydrostatics
from maintenance import generate_fbm_codepages

FILES=('qt_document.py','qt_point_edit.py','qt_native_topology.py','qt_native_modeling.py','qt_native_transforms.py','qt_native_construction.py','qt_native_lackenby.py','qt_advanced.py','qt_migration.py','qt_modeling.py','qt_modeling_extra.py','qt_editing.py','native_linesplan_app.py','qt/fbm_codec_tests.cpp','run_tests.py')
FILES+=tuple(p.relative_to(q.ROOT).as_posix() for p in sorted((q.ROOT/'fixtures/native-construction').rglob('*')) if p.is_file())
FILES+=('qt/lackenby_probe.cpp','qt/lackenby_math_probe.pas','maintenance/lackenby_trace.py','maintenance/lackenby_math.py')
FILES+=('qt_native_attributes.py','qt_native_face_tools.py','qt/face_tools_probe.cpp','maintenance/generate_fbm_codepages.py')
FILES+=('qt_native_project.py','qt_native_definitions.py','qt/project_edit_probe.cpp','qt/project_units_math.pas','qt/definition_edit_probe.cpp')
FILES+=('qt_native_auxiliary.py','qt_native_background.py','qt/auxiliary_edit_probe.cpp','qt/background_edit_probe.cpp')
FILES+=('qt_native_surface.py','qt/auxiliary_tests.cpp','maintenance/inspect_surface_normals.py')
FILES+=('qt_native_create.py','qt/document_create_probe.cpp')
FILES+=('qt_native_analysis.py','qt_native_flowlines.py','qt/analysis_edit_probe.cpp','qt/flowline_probe.cpp','maintenance/generate_analysis_fields.py','qt_performance_checks.py','qt_performance_cases.py','qt_hydrostatics.py','qt_hydro_cases.py','qt_backgrounds.py','solver_launch_probe.py')
FILES+=('qt_native_hydrostatics.py','fbm_preview_bytes.py')

def stop_owned_adapter(pid,service):
    # PID comes from the private service just started by this fixture. Open a
    # process handle, verify its executable path, then terminate that handle.
    if os.name!='nt':raise RuntimeError('Owned adapter fault injection is Windows-only')
    api=ctypes.WinDLL('kernel32',use_last_error=True)
    api.OpenProcess.restype=ctypes.c_void_p;api.OpenProcess.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32]
    api.QueryFullProcessImageNameW.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_wchar_p,ctypes.POINTER(ctypes.c_uint32)]
    api.TerminateProcess.argtypes=[ctypes.c_void_p,ctypes.c_uint32];api.WaitForSingleObject.argtypes=[ctypes.c_void_p,ctypes.c_uint32];api.CloseHandle.argtypes=[ctypes.c_void_p]
    handle=api.OpenProcess(0x1000|0x1|0x100000,False,pid)
    if not handle:raise OSError(ctypes.get_last_error(),'Cannot open private adapter')
    try:
        name=ctypes.create_unicode_buffer(32768);size=ctypes.c_uint32(len(name))
        if not api.QueryFullProcessImageNameW(handle,0,name,ctypes.byref(size)):raise OSError('Cannot identify private adapter')
        if Path(name.value).resolve()!=service.resolve():raise RuntimeError('Refusing to stop an unrelated process')
        if not api.TerminateProcess(handle,77):raise OSError('Cannot terminate owned adapter')
        api.WaitForSingleObject(handle,5000)
    finally:api.CloseHandle(handle)

def protocol(r,runtime,model,out):
    out.mkdir(parents=True);identity=str(uuid.uuid4());saved_states=[]
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'native',native_linesplan=True) as service, q.Service(runtime/'bridge/FreeShipGeometryBridge.exe',out/'oracle',native_linesplan=True) as oracle:
        counter=0
        def both(label,method='snapshot',**fields):
            nonlocal counter
            counter+=1;results=[]
            for tag,s in [('native',service),('oracle',oracle)]:
                folder=out/f'{counter:02}-{label}'/tag
                response=s.command(method,output=folder,**fields);q.require_ok(response)
                results.append((response,json.loads(Path(response['model']).read_text('utf8'))))
            a,b=results
            r.case(label+'.native-owner',a[0].get('document_engine')=='cpp-document')
            if method in ('move_point','move_points'):r.case(label+'.native-point-edit',a[0].get('edit_engine')=='cpp-points')
            r.case(label+'.state-parity',all(a[0][k]==b[0][k] for k in q.STATE_KEYS))
            r.case(label+'.complete-model-parity',modeling.geometry(a[1])==modeling.geometry(b[1]))
            r.compare(label+'.dxf-parity',a[0]['dxf'],b[0]['dxf'])
            r.compare(label+'.scene-parity',a[0]['scene'],b[0]['scene'])
            return a
        initial,m=both('load','load',path=str(model),document_id=identity)
        state=q.require_ok(service.command('document_status'));r.case('inspect.has-native-blocks',state['fbm'].get('format')=='FreeShipFbmInspection' or bool(state['fbm'].get('blocks')),inspection=state['fbm'])
        pristine=q.require_ok(service.command('save',output=out/'pristine'))
        r.case('save.unedited-byte-identical',Path(pristine['saved_file']).read_bytes()==model.read_bytes())
        first=next(p for p in m['control_points'] if p.get('batch_movable',not p['locked']));point=first['id'];position=list(first['p']);position[2]+=.125
        moved,mm=both('move','move_point',point_id=point,position=position)
        r.case('move.single-float',next(p['p'] for p in mm['control_points'] if p['id']==point)==[e.f32(v) for v in position])
        for tag,s in [('native',service),('oracle',oracle)]:
            q.require_ok(s.command('save',output=out/'confirmed-save'/tag));q.require_ok(s.command('mark_saved'))
        clean=dict(service.state)
        changed,cm=both('metadata','update_project',changes={'name':'Native document session'})
        changed,cm=both('display','display',changes={'both_sides':not cm['display']['both_sides']})
        both('undo-display','undo');both('undo-to-savepoint','undo')
        r.case('history.undo-to-saved-clean',not service.state['modified'] and service.state['can_redo'])
        old=dict(service.state);both('noop-preserves-redo','move_point',point_id=point,position=[e.f32(v) for v in position]);r.case('history.noop-exact-state',service.state==old)
        edge=next(v for v in mm['control_edges'] if all(next(p for p in mm['control_points'] if p['id']==k).get('batch_movable',True) for k in (v['a'],v['b'])))
        split,sm=both('branch-split','topology',operation='split_edge',edge_id=edge['id'])
        new_ids={p['id'] for p in sm['control_points']}-{p['id'] for p in mm['control_points']}
        r.case('history.branch-discards-redo',not service.state['can_redo'] and len(new_ids)==1)
        both('undo-split','undo');branch,bm=both('resplit','topology',operation='split_edge',edge_id=edge['id'])
        other={p['id'] for p in bm['control_points']}-{p['id'] for p in mm['control_points']}
        r.case('history.ids-never-reused',min(other)>max(new_ids))
        before=dict(service.state);output_file=out/'not-a-directory';output_file.write_text('preserve','ascii')
        bad_model=out/'broken.fbm';bad_model.write_bytes(b'broken')
        for label,method,fields in [
            ('stale','move_point',{'point_id':point,'position':position,'expected_revision':1}),
            ('bad-load','load',{'path':str(bad_model),'document_id':str(uuid.uuid4())}),
            ('bad-save','save',{'output':output_file}),
            ('bad-publish','move_point',{'point_id':point,'position':[0,0,1],'output':output_file}),
            ('bad-coordinate','move_point',{'point_id':point,'position':[1e100,0,0]})]:
            fields=dict(fields);where=fields.pop('output',out/label);response=service.command(method,output=where,**fields)
            r.case('failure.'+label,not response['ok'] and all(response.get(k)==before[k] for k in q.STATE_KEYS),error=response.get('error'))
        r.case('failure.output-preserved',output_file.read_text('ascii')=='preserve')
        # Rejections already stop the adapter. Native publication must remain
        # independent; inject a kill only if this owned process is still alive.
        recovered,_=both('recover-after-rejection')
        status=q.require_ok(service.command('document_status'))
        if status['adapter_pid']:stop_owned_adapter(int(status['adapter_pid']),runtime/'bridge/FreeShipGeometryBridge.exe')
        r.case('recovery.native-publication',recovered.get('snapshot_engine')=='cpp-publication')
        saved=q.require_ok(service.command('save',output=out/'save-without-adapter'));r.case('recovery.save-without-pascal',Path(saved['saved_file']).is_file() and service.state==before)
        both('recover-after-kill');both('undo-after-recovery','undo');both('redo-after-recovery','redo')
        saved=q.require_ok(service.command('save',output=out/'saved'))
        loaded=q.require_ok(oracle.command('load',path=saved['saved_file'],document_id=str(uuid.uuid4()),output=out/'reopened'))
        reopened=json.loads(Path(loaded['model']).read_text('utf8'))
        r.case('save.old-reader-geometry',native.reopened_geometry(bm)==native.reopened_geometry(reopened))
        r.compare('save.old-reader-dxf',branch['dxf'],loaded['dxf'])
        # Opaque extensions must remain byte-identical until understood. Their
        # presence cannot silently turn a normal edit into data loss.
        opaque=out/'opaque.fbm';opaque.write_bytes(model.read_bytes()+b'FreeShip26 unknown extension\x00\xff')
        response=q.require_ok(service.command('load',path=str(opaque),document_id=str(uuid.uuid4()),output=out/'opaque-load'))
        saved=q.require_ok(service.command('save',output=out/'opaque-save'));r.case('opaque.exact-save',Path(saved['saved_file']).read_bytes()==opaque.read_bytes())
        old=dict(service.state);rejected=service.command('update_project',changes={'name':'must not lose extension'},output=out/'opaque-edit')
        r.case('opaque.edit-preserves-state',not rejected['ok'] and service.state==old,error=rejected.get('error'))
    return initial

def run_codec(r,runtime,models,out):
    output=out/'codec';output.mkdir();report=output/'report.json'
    with (output/'process.log').open('wb') as log:
        run=subprocess.run([str(runtime/'qt_document_codec_tests.exe'),'--output',str(report),*[str(p) for p in models],str(q.common.DEFAULT_SOURCE/'Ships')],stdout=log,stderr=subprocess.STDOUT,timeout=90,creationflags=q.common.FLAGS)
    data=json.loads(report.read_text('utf8'));r.case('codec.process',run.returncode==0,report=str(report),status_value=data.get('status'))
    for c in data['cases']:
        item=dict(c);name=item.pop('id',item.pop('name','unnamed'));r.data['cases'].append({'id':'codec.'+name,**item})
    r.data['codec_report']=str(report)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT);p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path);p.add_argument('--verify-deployment',type=Path);p.add_argument('--model',type=Path,action='append')
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);r=q.Report();r.data.update(suite='qt-document',models=[]);start=time.perf_counter()
    r.data['limitations'][0]='Document ownership, editing, geometry, publication, preference/caption bootstrap and migrated professional calculations use C++; explicit diagnostic fallback remains. Hidden callbacks are not desktop acceptance.'
    source=q.common.fingerprint(a.source)[0];sources=q.current_sources(a.qt_source);hashes={f:q.common.digest_file(q.ROOT/f) for f in FILES};r.data.update(source_sha256=source,qt_sources_sha256=sources,test_sources_sha256=hashes)
    models=[m.resolve() for m in (a.model or q.DEFAULT_MODELS)];protected={str(f):q.common.digest_file(f) for f in [*models,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe'] if f.exists()}
    phases={}
    def stage(name,action):
        # Each stage owns its processes and fixtures. An exception must remain
        # an error, while allowing the independent later stages to be exercised.
        stamp=time.perf_counter();first=len(r.data['cases'])
        try:action()
        except Exception as ex:r.case(name+'.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
        finally:
            phases[name+'_s']=time.perf_counter()-stamp
            print(json.dumps({'phase':name,'seconds':round(phases[name+'_s'],3),
                              'checks':len(r.data['cases'])-first,
                              'issues':[c['id'] for c in r.data['cases'][first:] if c['status'] in ('fail','error')]}),flush=True)
    try:
        if a.verify_deployment:
            runtime=a.verify_deployment.resolve();manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
            if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Runtime manifest stale')
            for name,h in manifest['binaries_sha256'].items():
                if q.common.digest_file(runtime/name)!=h:raise RuntimeError('Runtime binary mismatch: '+name)
        else:runtime=q.prepare_runtime(a,out,q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),r),r)
        r.data['runtime_path']=str(runtime);phases['runtime_ready_s']=time.perf_counter()-start;stamp=time.perf_counter()
        stage('codec',lambda:run_codec(r,runtime,models,out))
        r.case('attributes.codepage-provenance',generate_fbm_codepages.TARGET.read_text('utf8')==generate_fbm_codepages.generate())
        for i,model in enumerate(models):
            try:protocol(r,runtime,model,out/('model-'+str(i+1)))
            except Exception as ex:r.case('model-'+str(i+1)+'.execution',status='error',reason=f'{type(ex).__name__}: {ex}')
        phases['protocol_s']=time.perf_counter()-stamp;stamp=time.perf_counter()
        jobs=[('native-points',qt_point_edit,models[0]),('native-topology',qt_native_topology,models[0]),
              ('native-modeling',qt_native_modeling,models[0]),('native-transforms',qt_native_transforms,models[0]),
              ('native-construction',qt_native_construction,models[0]),('native-lackenby',qt_native_lackenby,models[0]),
              ('native-attributes',qt_native_attributes,models[0]),('native-face-tools',qt_native_face_tools,models[0]),
              ('native-project',qt_native_project,models[0]),('native-definitions',qt_native_definitions,models[0]),
              ('native-auxiliary',qt_native_auxiliary,models[0]),('native-background',qt_native_background,models[0]),
              ('native-surface',qt_native_surface,models),('native-create',qt_native_create,models),
              ('native-analysis',qt_native_analysis,models[0]),('native-flowlines',qt_native_flowlines,models[0]),
              ('native-hydrostatics',qt_native_hydrostatics,models)]
        for name,module,inputs in jobs:stage(name,lambda:module.run(r,runtime,inputs,out/name))
        # Reuse complete modeling tests, including groups, constraints, markers,
        # topology and property roundtrips, through the new authority.
        stage('modeling-service',lambda:modeling.services(r,runtime/'FreeShipDocumentService.exe',models[0],out/'modeling-service'))
        stage('modeling-extra',lambda:modeling.extra.run(r,runtime/'FreeShipDocumentService.exe',models[0],out/'modeling-extra'))
        stage('production-ui',lambda:modeling.production(r,runtime,models[0],out/'production'))
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==source);r.case('qt-source-unchanged',q.current_sources(a.qt_source)==sources)
        r.case('test-sources-unchanged',all(q.common.digest_file(q.ROOT/f)==h for f,h in hashes.items()))
        for f,h in protected.items():r.case('protected.'+f,q.common.digest_file(Path(f))==h)
        r.case('physical-desktop',status='not_run',reason='Production Qt callbacks ran in hidden widgets; no desktop automation or user visual acceptance.')
        r.data['phase_seconds']=phases;r.data['elapsed_seconds']=time.perf_counter()-start;e.write_report(r,out)
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'seconds':r.data['elapsed_seconds'],'report':str(out/'report.json')}));return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]
if __name__=='__main__':raise SystemExit(main())
