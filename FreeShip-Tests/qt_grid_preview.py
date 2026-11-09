"""Actual sampled offsets -> native preview -> Qt export/calculation routes.

Independent wedge dimensions and wire-grid checks; no user desktop automation.
The parser for exported records is separate from the preview producer.
"""
import argparse, copy, hashlib, json, math, os, subprocess, time, uuid
from pathlib import Path
import qt_migration as q
import qt_editing as e
import qt_exchange as x
import qt_legacy_exchange as legacy
import qt_added_mass as added

FILES=('qt_grid_preview.py','qt/grid_preview_tests.cpp','qt_legacy_exchange.py',
       'qt_added_mass.py','qt_migration.py','qt_editing.py','qt_exchange.py','run_tests.py')

def worker(runtime,out,made,fmt,refs,preview=True,revision=None):
    out.mkdir(parents=True);target=out/'output';target.write_bytes(b'preserve')
    model=json.loads(Path(made['model']).read_text('utf8'))
    request=dict(format=fmt,model=made['model'],input=str(Path(made['model']).parent/'native-input.json'),output=str(target),document_id=model['document_id'],revision=model['revision'] if revision is None else revision,units='m',close_deck=False,references=refs,preview=preview)
    x.dump(out/'request.json',request)
    p=subprocess.run([str(runtime/'FreeShipGeometryExport.exe'),str(out/'request.json'),str(out/'report.json'),str(os.getpid())],capture_output=True,creationflags=q.common.FLAGS,timeout=90)
    (out/'process.log').write_bytes(p.stdout+p.stderr)
    return json.loads((out/'report.json').read_text('utf8')),target

def compare_grid(r,name,report,path):
    r.case(name+'.worker',report['status']=='pass',reason=report.get('error'))
    if report['status']!='pass':return
    preview=report['preview'];nx,nz=preview['stations'],preview['waterlines']
    if path.read_bytes().startswith(b'#'):
        _,hulls=legacy.read_michlet(path);tables=[h['table'] for h in hulls]
        r.case(name+'.hull-placement',len(hulls)==preview['hulls'] and all(h['y']==i*preview['separation_m'] for i,h in enumerate(hulls)))
    else:
        lines=path.read_text('utf8').splitlines();tables=[[list(map(float,row.split())) for row in lines[6:6+nx]]]
    r.case(name+'.every-ordinate-in-file',all(table==preview['half_breadths_m'] for table in tables))
    r.case(name+'.grid-dimensions',len(preview['half_breadths_m'])==nx and all(len(row)==nz for row in preview['half_breadths_m']))
    r.case(name+'.source-identity',preview['document_id']==report['document_id'] and preview['revision']==report['revision'])
    r.case(name+'.input-sha256',report['sha256']==hashlib.sha256(path.read_bytes()).hexdigest())

def service(r,runtime,out):
    out.mkdir();part=out/'template.part';part.write_bytes(legacy.part_fixture())
    template=x.probe(runtime/'qt_exchange_tests.exe',[dict(name='template',method='read',format='part',path=str(part),options={})],out/'codec')['template']['data']
    template['project'].update(name='Preview wedge',units='metric',length=8,beam=4,draft=2,water_density=1.025,precision=0);template['particulars_set']=True;template.pop('control_curves',None)
    template['layers'][0].update(symmetric=True,use_in_hydrostatics=True,use_for_intersections=True,show_in_linesplan=True)
    def hull(points,faces):
        data=copy.deepcopy(template);data['points']=[dict(p=p,vertex_type=3,locked=False) for p in points];data['faces']=[dict(layer=0,points=f) for f in faces]
        data['edges']=[[*edge,True] for edge in sorted({tuple(sorted((a,b))) for f in faces for a,b in zip(f,f[1:]+f[:1])})];return data
    mono=hull([[0,0,0],[8,0,0],[0,2,0],[0,0,4],[8,0,4],[0,2,4]],[[0,2,1],[2,5,4,1],[0,3,5,2]])
    cat=hull([[0,3,0],[0,7,0],[8,5,0],[0,3,4],[0,7,4],[8,5,4]],[[0,1,2],[1,4,5,2],[0,2,5,3],[0,3,4,1]])
    saved={};expected=[[v]*5 for v in (0,.5,1,1.5,2)]
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process') as s:
        def make(name,data):
            path=out/(name+'.json');x.dump(path,data);return q.require_ok(s.command('import_model',path=str(path),document_id=str(uuid.uuid4()),output=out/name))
        made=make('mono',mono);saved['mono']=e.saved_copy(s,out/'mono-saved');state=copy.deepcopy(s.state)
        for name,fmt,refs in [('single','michlet',{}),('duplicate','michlet',dict(hull_mode='duplicate',separation=10)),('addmass','addmass',{})]:
            refs=dict(stations=5,waterlines=5)|refs;report,path=worker(runtime,out/name,made,fmt,refs);compare_grid(r,name,report,path)
            # The unchanged native spline evaluates Single coordinates; use
            # its existing legacy-exchange tolerance for the analytic shape.
            r.case(name+'.independent-halfbreadths',all(math.isclose(a,b,abs_tol=2e-6) for row,want in zip(report['preview']['half_breadths_m'],expected) for a,b in zip(row,want)))
            r.case(name+'.actual-dimensions',report['preview']['length_m']==8 and report['preview']['draft_m']==2)
            normal,normalpath=worker(runtime,out/(name+'-no-preview'),made,fmt,refs,False)
            r.case(name+'.preview-keeps-original-file',normal['status']=='pass' and normalpath.read_bytes()==path.read_bytes() and 'preview' not in normal)
        report,path=worker(runtime,out/'stale',made,'michlet',dict(stations=5,waterlines=5),True,state['revision']+1)
        r.case('worker.stale-revision-rejected',report['status']=='error' and path.read_bytes()==b'preserve')
        report,path=worker(runtime,out/'invalid-option',made,'michlet',dict(stations=6,waterlines=5))
        r.case('worker.no-invalid-grid',report['status']=='error' and 'preview' not in report and path.read_bytes()==b'preserve')
        after=q.require_ok(s.command('snapshot',output=out/'after'));r.case('preview.read-only-state',s.state==state)
        for key in ('model','scene','dxf'):r.compare('preview.unchanged-'+key,made[key],after[key])
        made=q.require_ok(s.command('update_project',changes=dict(units='imperial'),output=out/'feet'));saved['feet']=e.saved_copy(s,out/'feet-saved')
        report,path=worker(runtime,out/'feet-grid',made,'michlet',dict(stations=5,waterlines=5,hull_mode='duplicate',separation=10));compare_grid(r,'feet',report,path)
        r.case('feet.physical-grid',math.isclose(report['preview']['length_m'],8,rel_tol=2e-7) and math.isclose(report['preview']['draft_m'],2,rel_tol=2e-7) and all(math.isclose(a,b,abs_tol=2e-6) for row,want in zip(report['preview']['half_breadths_m'],expected) for a,b in zip(row,want)))
        made=make('cat',cat);saved['cat']=e.saved_copy(s,out/'cat-saved');report,path=worker(runtime,out/'cat-grid',made,'michlet',dict(stations=5,waterlines=5,hull_mode='catamaran',separation=10));compare_grid(r,'cat',report,path)
        r.case('cat.own-centre-halfbreadths',all(math.isclose(a,b,abs_tol=2e-6) for row,want in zip(report['preview']['half_breadths_m'],expected) for a,b in zip(row,want)) and report['preview']['hulls']==2 and report['preview']['separation_m']==10)
        real=q.DEFAULT_MODELS[2];before=q.common.digest_file(real);made=q.require_ok(s.command('load',path=str(real),document_id=str(uuid.uuid4()),output=out/'real-ship'))
        report,path=worker(runtime,out/'real-preview',made,'addmass',dict(stations=21,waterlines=11));compare_grid(r,'real-ship',report,path)
        r.case('real-ship-protected',q.common.digest_file(real)==before)
    return saved

def production(r,runtime,out,saved):
    commands=[]
    for name,fmt,refs,flags in [('mono','michlet',{},{}),('duplicate','michlet',dict(hull_mode='duplicate',separation=10),{}),
            ('change','michlet',{},dict(preview_changes=dict(stations=7))),('invalid','michlet',{},dict(preview_changes=dict(depth=1),expect_failure=True)),
            ('cancel','michlet',{},dict(preview_cancel=True)),('addmass','addmass',{}, {})]:
        target=out/(name+'.'+('mlt' if fmt=='michlet' else 'txt'));target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(b'preserve')
        commands.append(dict(name=name,method='export',format=fmt,path=str(target),preview=True,confirm_overwrite=True,options=dict(references=dict(stations=5,waterlines=5)|refs),**flags))
    req=out/'request.json';x.dump(req,dict(model=str(saved['mono']),commands=commands))
    child=q.run_child(r,'production',runtime/'FreeShipQt.exe',['--verify-exchange',req],out/'qt',timeout=240,platform='windows');r.data['production']=child;art=child.get('artifacts',{})
    for command in commands:
        name=command['name'];preview=art.get(name+'_preview',{});target=Path(command['path'])
        if name=='invalid':r.case('production.failed-preview-keeps-file',target.read_bytes()==b'preserve' and preview.get('current') is False and preview.get('data',{}).get('stations')==5 and bool(preview.get('error')));continue
        if name=='cancel':r.case('production.preview-cancel-independent-of-export',target.read_bytes()!=b'preserve' and preview.get('current') is False);continue
        r.case('production.'+name+'.same-input',preview.get('current') is True and preview.get('input_sha256')==q.common.digest_file(target))
        if name=='change':r.case('production.parameter-regeneration',preview['data']['stations']==7 and art[name+'_preview_before']['data']['stations']==5 and preview['launches']==2)
        if name=='duplicate':r.case('production.twin-layout',preview['data']['hulls']==2 and preview['data']['separation_m']==10)
        if name+'_scene' in art:
            for key in ('scene','dxf'):r.compare('production.'+name+'.unchanged-'+key,art['original_'+key],art[name+'_'+key])
    for key in ('cat','feet'):
        folder=out/key;folder.mkdir();path=folder/'hull.mlt';req=folder/'request.json';x.dump(req,dict(model=str(saved[key]),commands=[dict(name=key,method='export',format='michlet',path=str(path),preview=True,options=dict(references=dict(stations=5,waterlines=5,hull_mode='catamaran' if key=='cat' else 'duplicate',separation=10)))]))
        child=q.run_child(r,'production.'+key,runtime/'FreeShipQt.exe',['--verify-exchange',req],folder/'qt',timeout=150,platform='windows');r.data['production_'+key]=child
        preview=child.get('artifacts',{}).get(key+'_preview',{});r.case('production.'+key+'.same-input',preview.get('current') is True and preview.get('input_sha256')==q.common.digest_file(path))

def calculation(r,runtime,out):
    out.mkdir(parents=True)
    model=added.model_fixture(runtime,out/'model')
    with q.Service(runtime/'FreeShipDocumentService.exe',out/'process') as s:
        made=q.require_ok(s.command('import_model',path=str(model),document_id=str(uuid.uuid4()),output=out/'load'));saved=e.saved_copy(s,out/'saved')
        # Preview's UI choices are zero-based; the input writer uses 1-based codes.
        report,path=worker(runtime,out/'expected-input',made,'addmass',dict(panels=400))
        expected=report['sha256']
    req=out/'request.json';x.dump(req,dict(model=str(saved),preview=True,inputs=dict(calculator='addmass',values=dict(panels=400),solver_directory=str(added.SOLVERS))))
    child=q.run_child(r,'production.calculation',runtime/'FreeShipQt.exe',['--verify-performance',req],out/'qt',timeout=150,platform='windows');r.data['calculation']=child
    art=child.get('artifacts',{});preview=art.get('input_grid_preview',{});r.case('calculation.same-input',preview.get('current') is True and preview.get('input_sha256')==expected)
    result=json.loads(Path(art['report_json']).read_text('utf8'));r.case('calculation.still-delivers-matrix',len(result['matrix'])==6 and all(len(row)==6 for row in result['matrix']))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();runtime=(a.runtime or q.QT/'dist').resolve();out=a.output.resolve()
    for root in (q.QT,q.PILOT,q.common.DEFAULT_SOURCE,q.ROOT):
        if out==root or root in out.parents:p.error('Use isolated output')
    out.mkdir(parents=True,exist_ok=False);r=q.Report();sources=q.current_sources(q.QT);before=q.common.fingerprint(q.common.DEFAULT_SOURCE)[0];start=time.monotonic()
    r.data.update(suite='qt-grid-preview',runtime=str(runtime),qt_sources_sha256=sources,source_sha256=before,test_sources_sha256={n:q.common.digest_file(q.ROOT/n) for n in FILES},limitations=['Hidden Qt events/screenshots do not establish desktop frame rate or physical interaction acceptance.','Offset preview is the sampled input wire grid, not the solver-generated panel mesh or a reconstruction of its result.'])
    try:
        manifest=json.loads((runtime/'build-manifest.json').read_text('utf8'));q.validate_test_manifest(manifest)
        if {k.replace('\\','/'):v for k,v in manifest['source_sha256'].items()}!=sources:raise RuntimeError('Stale runtime source')
        if any(q.common.digest_file(runtime/n)!=h for n,h in manifest['binaries_sha256'].items()):raise RuntimeError('Runtime file mismatch')
        r.case('runtime-manifest',True);r.data['view']=q.run_child(r,'view',runtime/'qt_grid_preview_tests.exe',[],out/'view',timeout=60,platform='windows')
        saved=service(r,runtime,out/'service');production(r,runtime,out/'production',saved);calculation(r,runtime,out/'calculation')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('source-stable',q.current_sources(q.QT)==sources);r.case('legacy-protected',q.common.fingerprint(q.common.DEFAULT_SOURCE)[0]==before)
        r.case('test-source-stable',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        r.case('full-old-dialog-layout',status='review',reason='Restored wire-grid preview in current StudioLight tabs; all old dialog layout details are not certified.')
        r.case('native-desktop',status='not_run',reason='Private hidden production widgets and saved screenshots only; no desktop automation.')
        r.data['elapsed_s']=time.monotonic()-start;e.write_report(r,out)
    print(json.dumps(dict(counts=r.data['counts'],report=str(out/'report.json'))));return int(bool(r.data['counts']['fail'] or r.data['counts']['error']))
if __name__=='__main__':raise SystemExit(main())
