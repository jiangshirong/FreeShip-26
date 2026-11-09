"""Hydrostatics migration: analytic solids, balance residuals, production dialogs.
All jobs use private models/settings. Desktop interaction remains not_run.
"""
from pathlib import Path
import argparse,csv,io,json,math,shutil
import qt_migration as q
import qt_editing as e
import qt_backgrounds as backgrounds
import qt_hydro_cases as checks


def pdf_check(r,label,path):
    try:import fitz
    except ImportError:
        r.case(label+'.pdf-content',status='not_run',reason='PyMuPDF is not installed');return
    with fitz.open(path) as document:
        text=''.join(page.get_text() for page in document)
        r.case(label+'.pdf-pages',len(document)>0)
        r.case(label+'.pdf-readable-quantities','FreeShip_26' in text and '排水量' in text and '重心' in text,extracted_text=text)
        image=path.with_suffix('.png');document[0].get_pixmap(matrix=fitz.Matrix(1.2,1.2)).save(image)
        r.data.setdefault('pdf_previews',[]).append(str(image))


def production_request(model,out,mode='cross',**flags):
    out.mkdir(parents=True,exist_ok=True)
    inputs=dict(mode=mode,trim=0)
    if mode=='single':inputs.update(draft=2,heel=5)
    elif mode=='table':inputs.update(start=1,end=3,step=.5)
    else:inputs.update(angles=[0,5,10],displacements=[246,369],lcg=10,tcg=0,vcg=2,free_trim=True)
    request=out/'request.json';request.write_text(json.dumps({'model':str(model),'inputs':inputs,**flags}),encoding='utf8')
    return request


def production(r,runtime,model,out):
    out.mkdir(parents=True)
    for mode,flags in [('cross',{}),('single',{}),('table',{'save_settings':True}),('cross',{'cancel':True}),('single',{'repeat_cancel':True})]:
        label=mode+('-'+next(iter(flags)) if flags else '')
        request=production_request(model,out/label,mode,**flags)
        # WA_DontShowOnScreen keeps these dialogs hidden. Use the Windows font
        # backend: the offscreen plugin substitutes AcadEref for YaHei UI on
        # this host, corrupting visual glyphs even when extracted text is right.
        child=q.run_child(r,'production.'+label,runtime/'FreeShipQt.exe',['--verify-hydro',request],out/label/'workflow',timeout=240)
        if label=='cross':r.data['models'].append({'model':str(model),'sha256':q.common.digest_file(model),'production':child})
        if child['status']=='error':raise RuntimeError('Hydro dialog workflow failed: '+label)
        if any(c['status']=='not_run' and c.get('name')=='hydro_entry' for c in child.get('cases',[])):
            r.case('production.'+label+'.dependent-checks',status='not_run',reason='Action/field discovery unavailable');continue
        art=child['artifacts']
        if q.os.name=='nt':r.case('production.'+label+'.font-available',art['dialog_font']['resolved']==art['dialog_font']['requested'],font=art['dialog_font'])
        for kind in ('scene','dxf'):r.compare('production.'+label+'.readonly-'+kind,art['original_'+kind],art['after_'+kind])
        if flags.get('cancel'):continue
        result=json.loads(Path(art['report_json']).read_text('utf8'));rows=result['rows']
        r.case('production.'+label+'.valid-rows',bool(rows) and all(row['status']=='ok' for row in rows),rows=rows)
        expected_rows={'single':1,'table':5,'cross':6}[mode];r.case('production.'+label+'.row-count',len(rows)==expected_rows)
        exported=list(csv.reader(io.StringIO(Path(art['report_csv']).read_text('utf-8-sig'))))
        r.case('production.'+label+'.csv-row-count',len(exported)==expected_rows+1)
        # The CSV displacement column is found by its unit-bearing quantity,
        # without treating UI layout as a design rule.
        displacement=[i for i,h in enumerate(exported[0]) if h.startswith('排水量 (')]
        r.case('production.'+label+'.csv-mass-column',len(displacement)==1)
        if len(displacement)==1:
            r.case('production.'+label+'.csv-full-precision',all(float(line[displacement[0]])==row['displacement'] for line,row in zip(exported[1:],rows)))
        r.case('production.'+label+'.pdf',Path(art['report_pdf']).read_bytes().startswith(b'%PDF-'))
        pdf_check(r,'production.'+label,Path(art['report_pdf']))
        text=Path(art['report_txt']).read_text('utf8')
        r.case('production.'+label+'.human-report','重心' in text and 'native_linesplan' not in text and 'document_id' not in text)
        if mode=='table':
            for row in rows:checks.close(r,'production.table.analytic',row['volume'],120*row['draft'])
        if mode=='cross':
            for row in rows:
                depth=row['target_displacement']/(120*result['water_density']);phi=math.radians(row['heel']);bm=3/depth
                wanted=(depth/2+bm-2+.5*bm*math.tan(phi)**2)*math.sin(phi)
                checks.close(r,'production.cross.analytic',row['gz'],wanted)
    r.case('dialog-visual-parity',status='review',reason='Screenshots retained; native desktop interaction and overall parity require separate acceptance')

def service_cases(r,exe,model,out):
    out.mkdir(parents=True)
    with q.Service(exe,out/'process',native_linesplan=(exe.parent.parent/'FreeShipNativeLinesplan.exe').exists()) as s:
        base=e.take(r,'hydro',s,out,'base','load',path=str(model));initial=dict(s.state)
        def job(name,**inputs):
            folder=out/name;folder.mkdir();response=q.require_ok(s.command('hydrostatics',output=folder,**inputs))
            data=json.loads(Path(response['hydro']).read_text('utf8'))
            r.case(name+'.revision',data['revision']==initial['revision'] and data['document_id']==initial['document_id'])
            r.case(name+'.no-history-change',s.state==initial)
            return data
        defaults=q.require_ok(s.command('hydro_defaults'))['analysis']
        rows=job('box-table',mode='table',start=1,end=4,step=1,trim=0)['rows']
        r.case('box-table.count',len(rows)==4)
        for i,row in enumerate(rows,1):
            r.case(f'box-{i}.valid',row['status']=='ok',row=row)
            if row['status']!='ok':continue
            # Fixture: half box, 20 x 3 x 8, mirrored about Y=0.
            for key,wanted in [('volume',120*i),('waterplane_area',120),('lwl',20),('bwl',6),('midship_area',6*i)]:
                r.case(f'box-{i}.{key}',math.isclose(row[key],wanted,rel_tol=3e-5,abs_tol=1e-5),observed=row[key],analytic=wanted)
            r.case(f'box-{i}.buoyancy',all(math.isclose(a,b,rel_tol=3e-5,abs_tol=1e-5) for a,b in zip(row['buoyancy'],[10,0,i/2])))
        mass=rows[1].get('displacement',246)
        result=job('box-cross',mode='cross',angles=[0,5,10],displacements=[mass],trim=0,free_trim=False,lcg=10,tcg=0,vcg=2)
        for row in result['rows']:
            angle=row['heel'];r.case(f'cross-{angle}.valid',row['status']=='ok',row=row)
            if row['status']!='ok':continue
            # Wall-sided rectangular hull: GZ=(KB+BM-KG+.5*BM*tan(phi)^2)*sin(phi).
            phi=math.radians(angle);expected=(1+1.5-2+.75*math.tan(phi)**2)*math.sin(phi)
            r.case(f'cross-{angle}.gz-analytic',math.isclose(row['gz'],expected,abs_tol=3e-5),observed=row['gz'],analytic=expected)
            r.case(f'cross-{angle}.mass-residual',abs(row['mass_residual'])<=row['mass_tolerance'])
        result=job('box-free-trim',mode='cross',angles=[0],displacements=[mass],trim=1,free_trim=True,lcg=10,tcg=0,vcg=2)
        row=result['rows'][0];r.case('free-trim.valid',row['status']=='ok',row=row)
        if row['status']=='ok':r.case('free-trim.analytic-symmetry',abs(row['trim'])<1e-3 and abs(row['longitudinal_residual'])<=row['length_tolerance'])
        bad=job('impossible',mode='cross',angles=[0],displacements=[1e8],trim=0,free_trim=False,lcg=10,tcg=0,vcg=2)
        r.case('impossible.explicit-row-error',bad['rows'][0]['status']=='error')
        folder=out/'cancelled';folder.mkdir();(folder/'cancel.flag').write_text('cancel')
        response=s.command('hydrostatics',output=folder,mode='table',start=1,end=4,step=1,trim=0)
        r.case('cancelled.model-intact',s.state==initial)
        r.case('cancelled.reported',not response['ok'] or json.loads(Path(response['hydro']).read_text())['cancelled'])
        for name,fields in [('zero-step',dict(mode='table',start=1,end=4,step=0,trim=0)),('excess-rows',dict(mode='table',start=0,end=100,step=.001,trim=0)),('unsorted',dict(mode='cross',angles=[10,0],displacements=[mass],trim=0,free_trim=False,lcg=10,tcg=0,vcg=2))]:
            response=s.command('hydrostatics',output=out/name,**fields);r.case(name+'.rejected',not response['ok']);r.case(name+'.intact',s.state==initial)
        after=e.take(r,'hydro',s,out,'after','snapshot');e.same(r,'readonly-complete-geometry',after,base)
        q.require_ok(s.command('hydro_settings',output=out/'settings',mode='table',start=1,end=3,step=.5,trim=.25))
        changed=q.require_ok(s.command('hydro_defaults'))['analysis'];r.case('settings.values',all(changed[k]==v for k,v in dict(start=1,end=3,step=.5,trim=.25).items()))
        q.require_ok(s.command('undo',output=out/'undo'));r.case('settings.undo',q.require_ok(s.command('hydro_defaults'))['analysis']==defaults)
        q.require_ok(s.command('redo',output=out/'redo'));saved=e.saved_copy(s,out/'save')
        q.require_ok(s.command('load',output=out/'reopen',path=str(saved),document_id=str(q.uuid.uuid4())))
        r.case('settings.saved',q.require_ok(s.command('hydro_defaults'))['analysis']==changed)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--source',type=Path,default=q.common.DEFAULT_SOURCE);p.add_argument('--qt-source',type=Path,default=q.QT)
    p.add_argument('--service-executable',type=Path);p.add_argument('--reuse-build',type=Path);p.add_argument('--runtime',type=Path);p.add_argument('--reuse-probe',type=Path)
    p.add_argument('--verify-deployment',type=Path);p.add_argument('--workflow-reference',type=Path)
    a=p.parse_args();a.source=a.source.resolve();a.qt_source=a.qt_source.resolve();out=a.output.resolve()
    if a.verify_deployment:
        if not a.workflow_reference:p.error('--workflow-reference is required')
        return q.verify_deployment(a,[q.DEFAULT_MODELS[0]],out,'--verify-hydro',('original','after'),('scene','dxf'),
            workflow_prepare=lambda model,folder:production_request(model,folder/'request'))
    for root in (a.source,a.qt_source,q.PILOT):
        if out==root or root in out.parents:p.error('Use an isolated report directory')
    out.mkdir(parents=True,exist_ok=False)
    r=q.Report();r.data['suite']='qt-hydrostatics';before=q.common.fingerprint(a.source)[0];sources=q.current_sources(a.qt_source)
    tests=('qt_hydrostatics.py','qt_hydro_cases.py','QtEditingFixture.pas','qt_editing_fixture.py','qt_migration.py','qt_editing.py','qt_backgrounds.py','run_tests.py')
    r.data['source_sha256']=before;r.data['qt_sources_sha256']=sources;r.data['test_sources_sha256']={n:q.common.digest_file(q.ROOT/n) for n in tests}
    protected={str(path):q.common.digest_file(path) for path in [*q.DEFAULT_MODELS,a.source/'FreeShip.exe',q.PILOT/'dist/FreeShipQtPilot.exe'] if path.exists()}
    try:
        if a.service_executable:exe=a.service_executable.resolve();r.case('qt-production',status='not_run',reason='Service diagnostic only')
        else:
            bridge=q.prepare_bridge(a,out,q.bridge_builder(a.qt_source),r);runtime=q.prepare_runtime(a,out,bridge,r);exe=runtime/'FreeShipDocumentService.exe'
        service_cases(r,exe,q.DEFAULT_MODELS[0],out/'service')
        probe=backgrounds.prepare_oracle(a,out,r) if not a.service_executable or a.reuse_probe else None
        checks.analytic(r,exe,q.DEFAULT_MODELS[0],out/'analytic',probe)
        checks.settings_cases(r,exe,q.DEFAULT_MODELS[0],out/'settings',probe)
        if probe:checks.original_cases(r,exe,probe,q.DEFAULT_MODELS,out/'original')
        else:r.case('original-core-comparison',status='not_run',reason='Diagnostic without an independent original-core probe')
        if not a.service_executable:production(r,runtime,q.DEFAULT_MODELS[0],out/'production')
    except Exception as ex:r.case('execution',status='error',reason=f'{type(ex).__name__}: {ex}')
    finally:
        r.case('legacy-source-unchanged',q.common.fingerprint(a.source)[0]==before)
        r.case('qt-source-unchanged',q.current_sources(a.qt_source)==sources)
        r.case('test-source-unchanged',all(q.common.digest_file(q.ROOT/n)==h for n,h in r.data['test_sources_sha256'].items()))
        for path,digest in protected.items():r.case('protected.'+path,q.common.digest_file(Path(path))==digest)
        r.case('native-desktop',status='not_run',reason='Hidden calculations and Qt workflows only');e.write_report(r,out)
    print(json.dumps({'status':r.data['status'],'counts':r.data['counts'],'report':str(out/'report.json')}))
    return {'pass':0,'partial':2,'fail':1,'error':3}[r.data['status']]
if __name__=='__main__':raise SystemExit(main())
