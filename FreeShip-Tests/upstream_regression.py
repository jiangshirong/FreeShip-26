"""Behavioral upstream regression probes in an isolated FreeShip build."""
import argparse, json, os, shutil, subprocess, tempfile, time
from pathlib import Path
from collections import Counter
import freeship_ui_test as common
import freeship_window_test as window
from upstream_coverage import coverage_matrix
ROOT = Path(__file__).resolve().parent
FILES = ('upstream_regression.py', 'UiUpstreamProbe.pas', 'freeship_ui_test.py', 'freeship_window_test.py', 'UiWindowProbe.pas', 'UiWindowEvents.pas', 'upstream_issue_manifest.json', 'upstream_mock.pas', 'upstream_coverage.py')
def hashes(): return {f: common.digest_file(ROOT/f) for f in FILES}
def record_subcase_error(cases, issue, case_id, exc):
    cases[:] = [c for c in cases if c['id'] not in (issue+'.coverage',case_id)]
    cases.append({'id':case_id,'status':'error','expected':'probe executes and produces valid evidence',
                  'actual':f'{type(exc).__name__}: {exc}'})

def main():
    p=argparse.ArgumentParser(); p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE); p.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS); p.add_argument('--output',type=Path,default=ROOT/'reports'/('upstream-'+time.strftime('%Y%m%d-%H%M%S'))); p.add_argument('--timeout',type=int,default=240); a=p.parse_args()
    src=a.source.resolve(); out=a.output.resolve()
    if out == src or src in out.parents or out.exists(): raise SystemExit('Output must not exist and must be outside the source tree')
    out.mkdir(parents=True); manifest=json.loads((ROOT/'upstream_issue_manifest.json').read_text(encoding='utf8'))
    cases=[{'id':x['id']+'.coverage','status':'not_run','expected':x['title'],'actual':'No validated behavioral probe for this issue yet'} for x in manifest['cases'] if x['id'] not in ('05','06')]
    report={'status':'error','cases':cases,'test_code_sha256':hashes(),'fixture_sha256':common.digest_file(common.FIXTURE),'limitations':['Hidden in-process model operations; not a desktop interaction test. Other issue records are explicitly not_run.']}
    before=None; phase='fingerprint'
    try:
        before,count=common.fingerprint(src); report.update(source_sha256=before, source_file_count=count)
        tools=common.junction_for(a.lazarus.resolve(),Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-upstream-run-') as run:
            work=run/'source'; work.mkdir(); evidence=run/'evidence'; evidence.mkdir(); profile=run/'profile'; profile.mkdir()
            phase='shared window preparation and instrumentation'
            tree,project,copy_sha,_=window.prepare(src,work,'none')
            if copy_sha!=before or common.fingerprint(src)[0]!=before: raise RuntimeError('Source changed during snapshot')
            if common.digest_file(work/'fixture.fbm')!=report['fixture_sha256']: raise RuntimeError('Fixture copy differs from original')
            shutil.copy2(ROOT/'UiUpstreamProbe.pas',work/'Units'/'UiWindowProbe.pas')
            # Replace only the file chooser result in the disposable copy; actual decoding and object lifetime stay unchanged.
            phase='14 JPG chooser injection'
            bg=work/'Units'/'FreeBackgroundImage.inc'; content=bg.read_text(encoding='utf8')
            start=content.index('procedure TFreeBackgroundImage.Open('); end=content.index('end;{TFreeBackgroundImage.Open}',start)
            part=content[start:end]; anchor='  if Dialog.Execute then'
            if part.count(anchor)!=1: raise RuntimeError('JPG dialog adapter is ambiguous or missing')
            part=part.replace(anchor, "  Dialog.FileName := SysUtils.GetEnvironmentVariable('FS_UPSTREAM_JPEG');\n  if Dialog.FileName <> '' then")
            bg.write_text(content[:start]+part+content[end:],encoding='utf8')
            # Fault the real stream call in the disposable copy; normal loading remains untouched.
            phase='03 I/O injection'
            buffer=work/'Units'/'FreeFileBuffer.pas'
            common.replace_once(buffer,'Progress := Stream.Read(Data[ReadCount], Requested);',"if SysUtils.GetEnvironmentVariable('FS_UPSTREAM_IO_FAULT')='io-read' then Progress:=0 else Progress := Stream.Read(Data[ReadCount], Requested);")
            common.replace_once(buffer,'Progress := Stream.Write(PByte(@Buffer)[Written], Requested);',"if SysUtils.GetEnvironmentVariable('FS_UPSTREAM_IO_FAULT')='io-write' then Progress:=0 else Progress := Stream.Write(PByte(@Buffer)[Written], Requested);")
            phase='01 serialization injection'
            edit=work/'Units'/'FreeEdit.inc'
            common.replace_once(edit,'      FreeShip.SaveProject(Destination);',"      if SysUtils.GetEnvironmentVariable('FS_UPSTREAM_SAVE_FAULT')='1' then begin Windows.SetEnvironmentVariable('FS_UPSTREAM_SAVE_FAULT_SEEN','1'); raise Exception.Create('injected serialization failure') end;\n      FreeShip.SaveProject(Destination);")
            common.replace_once(edit,"        MessageDlg(rsUnableToSaveFile + ' ' + FreeShip.Filename + LineEnding +\n          E.Message, mtError, [mbOK], 0);", "        if SysUtils.GetEnvironmentVariable('FS_UPSTREAM_SUBCASE')='save-fault' then Windows.SetEnvironmentVariable('FS_UPSTREAM_SAVE_MESSAGE',PChar(E.Message))\n        else MessageDlg(rsUnableToSaveFile + ' ' + FreeShip.Filename + LineEnding +\n          E.Message, mtError, [mbOK], 0);")
            phase='08/09 rebuild and loading injection'
            surface=work/'Units'/'FreeSubdivisionSurface.inc'
            common.replace_once(surface,'  FIsBuilding:=true;',"  FIsBuilding:=true;\n  if SysUtils.GetEnvironmentVariable('FS_UPSTREAM_REBUILD_FAULT')='1' then raise Exception.Create('injected rebuild failure');")
            content=surface.read_text(encoding='utf8'); start=content.index('procedure TFreeSubdivisionSurface.LoadBinary('); end=content.index('end;{TFreeSubdivisionSurface.LoadBinary}',start); part=content[start:end]
            anchor='  FIsLoading := true;'
            if part.count(anchor)!=1: raise RuntimeError('load-event injection anchor changed')
            part=part.replace(anchor,anchor+"\n  if SysUtils.GetEnvironmentVariable('FS_UPSTREAM_LOAD_EVENT')='1' then ExecuteOnChangeItem(Self);")
            surface.write_text(content[:start]+part+content[end:],encoding='utf8')
            phase='02 invalid load modal adapter'
            ship=work/'Units'/'FreeShip.inc'
            invalid_dialog='MessageDlg(rs_This_is_not_a_valid_FREE_ship_file_ {UserString[189]}, mtError, [mbOK], 0);'
            if invalid_dialog in ship.read_text(encoding='utf8'):
                common.replace_once(ship,invalid_dialog,"if SysUtils.GetEnvironmentVariable('FS_UPSTREAM_SUBCASE')<>'load-invalid' then "+invalid_dialog)
            elif 'raise EReadError.Create(rs_This_is_not_a_valid_FREE_ship_file_);' not in ship.read_text(encoding='utf8'):
                raise RuntimeError('Invalid-header handling changed; review load adapter')
            phase='07 midship accessor'
            shipunit=work/'Units'/'FreeShipUnit.pas'; hyd=work/'Units'/'FreeHydrostaticCalc.inc'
            common.replace_once(shipunit,'    procedure CalculateMidshipLocation;', '    procedure CalculateMidshipLocation;\n    procedure UiSetModelBounds(MinX,MaxX:TFloatType);')
            common.replace_once(hyd,'procedure TFreeHydrostaticCalc.CalculateMidshipLocation;', 'procedure TFreeHydrostaticCalc.UiSetModelBounds(MinX,MaxX:TFloatType); begin FData.ModelMin.X:=MinX; FData.ModelMax.X:=MaxX end;\n\nprocedure TFreeHydrostaticCalc.CalculateMidshipLocation;')
            phase='13 STL chooser adapter'
            stl=edit.read_text(encoding='utf8'); start=stl.index('procedure TFreeEdit.File_ExportSTL;'); end=stl.index('end;{TFreeEdit.File_ExportSTL}',start); part=stl[start:end]
            if part.count('  if SaveDialog.Execute then')!=1: raise RuntimeError('STL chooser adapter anchor changed')
            part=part.replace('  if SaveDialog.Execute then',"  SaveDialog.FileName:=SysUtils.GetEnvironmentVariable('FS_UPSTREAM_STL');\n  if SaveDialog.FileName<>'' then")
            edit.write_text(stl[:start]+part+stl[end:],encoding='utf8')
            phase='12 draft instrumentation'
            hydroform=work/'Forms'/'FreeHydrostaticsFrm.pas'
            common.replace_once(hydroform,'  public    { Public declarations }', '  public    { Public declarations }\n    procedure UiSetShip(Ship:TFreeShip);')
            common.replace_once(hydroform,'  FreeHydrostaticsForm: TFreeHydrostaticsForm;', '  FreeHydrostaticsForm: TFreeHydrostaticsForm;\n  UiDraftTrace:TStringList;')
            common.replace_once(hydroform,'function TFreeHydrostaticsForm.FGetStartDraft: single;', 'procedure TFreeHydrostaticsForm.UiSetShip(Ship:TFreeShip); begin FFreeShip:=Ship end;\n\nfunction TFreeHydrostaticsForm.FGetStartDraft: single;')
            common.replace_once(hydroform,'        HydObject.Calculate;', '        if Assigned(UiDraftTrace) then UiDraftTrace.Add(FloatToStr(HydObject.Draft));\n        HydObject.Calculate;')
            common.replace_once(hydroform,'      ResultsDlg.Execute;', "      if SysUtils.GetEnvironmentVariable('FS_UPSTREAM_SUBCASE')<>'drafts' then ResultsDlg.Execute;")
            runtime=tools/'fpc'/'3.2.2'/'units'/'x86_64-win64'
            for item in tree.iter('OtherUnitFiles'): item.set('Value',item.get('Value','')+';'+';'.join(str(x) for x in sorted(runtime.iterdir()) if x.is_dir() and x.name!='fv'))
            for item in tree.findall('.//CompilerOptions/Target/Filename'): item.set('Value','upstream-probe')
            tree.write(project,encoding='utf8',xml_declaration=True); report['instrumented_source_sha256']=common.fingerprint(work)[0]
            env=os.environ.copy(); env.update(TEMP=str(run),TMP=str(run),APPDATA=str(profile),LOCALAPPDATA=str(profile),FS_UI_PROBE_OUTPUT=str(evidence),FS_UI_PROBE_MODEL=str(work/'fixture.fbm'))
            compiler=tools/'fpc'/'3.2.2'/'bin'/'x86_64-win64'/'fpc.exe'; report['compiler_sha256']=common.digest_file(compiler)
            cmd=[str(tools/'lazbuild.exe'),f'--lazarusdir={tools}',f'--compiler={compiler}',f'--pcp={run/"config"}','--build-mode=Win32 Debug','--ws=win32','--cpu=x86_64','--os=win64',str(project)]
            phase='shared compiler build'
            build=subprocess.run(cmd,cwd=work,env=env,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS); (out/'build.log').write_bytes(build.stdout+b'\n'+build.stderr)
            if build.returncode: raise RuntimeError('Build failed; see build.log')
            exe=work/'upstream-probe.exe'; report['instrumented_exe_sha256']=common.digest_file(exe)
            helper=run/'helpers'; helper.mkdir()
            helper_error=None
            try:
                mock=subprocess.run([str(compiler),'-o'+str(helper/'mock-ok.exe'),str(ROOT/'upstream_mock.pas')],cwd=helper,env=env,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS)
                (out/'mock-build.log').write_bytes(mock.stdout+b'\n'+mock.stderr)
                if mock.returncode: helper_error=f'Mock build failed ({mock.returncode}); see mock-build.log'
                else: shutil.copy2(helper/'mock-ok.exe',helper/'mock-fail.exe')
            except (OSError, subprocess.TimeoutExpired) as exc:
                helper_error=f'Mock build unavailable: {type(exc).__name__}: {exc}'
            env['FS_UPSTREAM_HELPERS']=str(helper)
            try:
                from PIL import Image
                jpeg=run/'valid.jpg'; Image.new('RGB',(8,6),(80,120,160)).save(jpeg,format='JPEG'); env['FS_UPSTREAM_JPEG']=str(jpeg)
            except ImportError: jpeg=None
            required={'05.water-density-metric-to-imperial','05.water-density-round-trip','06.station-metric-to-imperial','06.waterline-metric-to-imperial','06.buttock-metric-to-imperial','06.station-round-trip','06.waterline-round-trip','06.buttock-round-trip'}
            try:
                process=subprocess.run([str(exe)],cwd=work,env=env,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS); (out/'runtime.log').write_bytes(process.stdout+b'\n'+process.stderr)
                probe=evidence/'probe.json'
                if not probe.exists(): raise RuntimeError(f'No probe report; exit={process.returncode}')
                native=json.loads(probe.read_text(encoding='utf-8-sig')); cases.extend(native['cases'])
                if process.returncode or native.get('error'): raise RuntimeError(f'Probe failed: exit={process.returncode}, {native.get("error")}')
                required={'05.water-density-metric-to-imperial','05.water-density-round-trip','06.station-metric-to-imperial','06.waterline-metric-to-imperial','06.buttock-metric-to-imperial','06.station-round-trip','06.waterline-round-trip','06.buttock-round-trip'}
                if {x['id'] for x in native['cases']}!=required or len(native['cases'])!=len(required): raise RuntimeError('Incomplete or duplicate probe results')
            except Exception as exc:
                for case_id in sorted(required): record_subcase_error(cases,case_id[:2],case_id,exc)
            for sub in ('process','jpg'):
                try:
                    if sub=='jpg' and jpeg is None: continue
                    if sub=='process' and helper_error: raise RuntimeError(helper_error)
                    subdir=run/('evidence-'+sub); subdir.mkdir()
                    subenv=env.copy(); subenv.update(FS_UI_PROBE_OUTPUT=str(subdir),FS_UPSTREAM_SUBCASE=sub)
                    child=subprocess.run([str(exe)],cwd=work,env=subenv,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS)
                    (out/(sub+'-runtime.log')).write_bytes(child.stdout+b'\n'+child.stderr)
                    subreport=subdir/'probe.json'
                    if child.returncode or not subreport.exists(): raise RuntimeError(f'{sub} probe failed: exit={child.returncode}')
                    data=json.loads(subreport.read_text(encoding='utf-8-sig'))
                    if data.get('error'): raise RuntimeError(f'{sub} probe: {data["error"]}')
                    expected={'process':{'11.successful-process','11.nonzero-process'},'jpg':{'14.valid-jpg-open'}}[sub]
                    if {x['id'] for x in data['cases']}!=expected or len(data['cases'])!=len(expected): raise RuntimeError(f'{sub} probe incomplete')
                    cases.extend(data['cases'])
                    cases[:]=[x for x in cases if x['id'] not in ({'process':{'11.coverage'},'jpg':{'14.coverage'}}[sub])]
                except Exception as exc:
                    issue='11' if sub=='process' else '14'
                    record_subcase_error(cases,issue,issue+'.'+sub+'-probe-error',exc)
            for sub,issue,cid in (('save-fault','01','01.save-serialization-failure'),('load-invalid','02','02.invalid-header-preserves-model'),('rebuild-fault','08','08.rebuild-recovers-after-exception'),('undo-cancel','10','10.cancel-preserves-redo'),('load-notify','09','09.no-intermediate-change-event'),('jpeg-bounds','04','04.jpeg-length-bounds'),('midship','07','07.translated-bounds'),('stl','13','13.stl-layer-symmetry'),('hydro-files','15','15.hydro-no-implicit-files'),('drafts','12','12.no-duplicate-terminal-draft'),('patch','16','16.patch-error-observation'),('legacy','17','17.motorboat3-desktop-load')):
                try:
                    subdir=run/('evidence-'+sub); subdir.mkdir(); model=subdir/'fixture.fbm'
                    sample=src/'Ships'/'Database'/'Motorboat3.fbm' if sub=='legacy' else common.FIXTURE
                    if not sample.exists():
                        cases.append({'id':cid,'status':'not_run','expected':'sample available','actual':str(sample)}); continue
                    sample_sha=common.digest_file(sample); shutil.copy2(sample,model)
                    if common.digest_file(model)!=sample_sha: raise RuntimeError(f'{sub} sample copy mismatch')
                    subenv=env.copy(); subenv.update(FS_UI_PROBE_OUTPUT=str(subdir),FS_UI_PROBE_MODEL=str(model),FS_UPSTREAM_SUBCASE=sub,FS_UPSTREAM_STL=str(subdir/'export.stl'))
                    child=subprocess.run([str(exe)],cwd=subdir,env=subenv,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS)
                    (out/(sub+'-runtime.log')).write_bytes(child.stdout+b'\n'+child.stderr)
                    f=subdir/'probe.json'
                    if child.returncode or not f.exists(): raise RuntimeError(f'{sub} probe failed: exit={child.returncode}')
                    data=json.loads(f.read_text(encoding='utf-8-sig'))
                    if data.get('error') or len(data['cases'])!=1 or data['cases'][0]['id']!=cid: raise RuntimeError(f'{sub} probe incomplete: {data}')
                    if sub=='save-fault':
                        case=data['cases'][0]; case['original_sha256']=report['fixture_sha256']; case['after_sha256']=common.digest_file(model) if model.exists() else None
                        if case['status']!='not_run' and case['after_sha256']!=report['fixture_sha256']: case['status']='fail'
                    data['cases'][0]['fixture_sha256']=sample_sha
                    if common.digest_file(sample)!=sample_sha: raise RuntimeError(f'{sub} sample changed during run')
                    cases.extend(data['cases']); cases[:]=[x for x in cases if x['id']!=issue+'.coverage']
                except Exception as exc:
                    record_subcase_error(cases,issue,cid,exc)
            for sub in ('io-read','io-write'):
                try:
                    subdir=run/('evidence-'+sub); subdir.mkdir()
                    subenv=env.copy(); subenv.update(FS_UI_PROBE_OUTPUT=str(subdir),FS_UPSTREAM_SUBCASE=sub)
                    try:
                        child=subprocess.run([str(exe)],cwd=work,env=subenv,capture_output=True,timeout=8,creationflags=common.FLAGS)
                    except subprocess.TimeoutExpired:
                        stage=(subdir/'stage.txt').read_text(encoding='utf8') if (subdir/'stage.txt').exists() else ''
                        if stage!=sub+'-start': raise RuntimeError(f'{sub} timed out before fault injection: {stage}')
                        cases.append({'id':'03.'+sub+'-zero-progress','status':'fail','expected':'zero progress terminates and signals an error','actual':'injected zero progress; operation did not terminate within 8 seconds'})
                        continue
                    datafile=subdir/'probe.json'
                    if child.returncode or not datafile.exists(): raise RuntimeError(f'{sub} probe failed: exit={child.returncode}')
                    data=json.loads(datafile.read_text(encoding='utf-8-sig'))
                    if data.get('error') or len(data['cases'])!=1 or data['cases'][0]['id']!='03.'+sub+'-zero-progress': raise RuntimeError(f'{sub} probe incomplete: {data}')
                    cases.extend(data['cases'])
                except Exception as exc:
                    record_subcase_error(cases,'03','03.'+sub+'-zero-progress',exc)
            cases[:]=[x for x in cases if x['id']!='03.coverage']
            # Explicit gaps remain visible even when one scenario for an issue is covered.
            for cid,expected in (
                ('01.write-and-replace-failure','write interruption, replacement failure, reopening original and successful save'),
                ('02.truncated-and-missing-file','truncated and nonexistent files preserve filename, model and redo history'),
                ('04.face-index-and-large-length','invalid face indices, integer overflow and historic valid format'),
                ('07.translation-and-stations','physical hull translation, sectional area and auxiliary station variation'),
                ('08.other-exception-points','failures in later rebuild stages and subsequent drawing'),
                ('09.real-event-sequence','record naturally occurring callbacks and restoration on failed loads'),
                ('10.confirm-and-commit','confirmed unchanged operation and new committed edit'),
                ('12.off-grid-and-small-step','non-grid endpoint, small step and maximum count'),
                ('13.other-stl-geometries','asymmetric attachments, complete hull and symmetry-plane facets'),
                ('15.permissions-and-concurrency','read-only working directory and concurrent calculations'),
                ('16.scale-and-surface-comparison','scale comparison and planar, cylindrical and doubly curved samples'),
                ('17.other-versions-and-offsets','historical versions, byte-by-byte offsets and known-good round trips'),
                ('03.short-and-normal-io','short transfers, complete transfers and file handle cleanup'),
                ('05.displacement-and-volume','physical displacement and volume across unit changes'),
                ('06.additional-models','intersection conversion on additional models and existing imperial files'),
                ('11.timeout-and-cancel','hung child timeout, cancellation, startup failure and stale-output isolation'),
                ('14.corrupt-and-replacement','corrupt JPEG and repeated replacement preserve the previous image')):
                cases.append({'id':cid,'status':'not_run','expected':expected,'actual':'behavioral probe not implemented'})
            ids=[x['id'] for x in cases]
            if len(ids)!=len(set(ids)) or {x.split('.')[0] for x in ids}!={f'{n:02}' for n in range(1,18)}: raise RuntimeError('Duplicate cases or missing issue coverage')
            if common.fingerprint(src)[0]!=before or hashes()!=report['test_code_sha256'] or common.digest_file(common.FIXTURE)!=report['fixture_sha256']: raise RuntimeError('Source, test code or fixture changed during run')
            report['source_unchanged_during_run']=True; report['status']='error' if any(x['status']=='error' for x in cases) else common.classify_statuses([x['status'] for x in cases])
    except Exception as e:
        report['status']='error'; report['error']=f'{type(e).__name__}: {e}'
        report['infrastructure_diagnostic']={'phase':phase,'message':'Shared preparation, injection or build failed; individual probes were not accepted. Inspect the anchor and build.log.','instrumentation_matches':list(common.INSTRUMENTATION_MATCHES)}
        present={x['id'][:2] for x in cases}
        for issue in manifest['cases']:
            if issue['id'] not in present: cases.append({'id':issue['id']+'.coverage','status':'not_run','expected':issue['title'],'actual':'shared test infrastructure did not complete'})
    report['coverage_matrix']=coverage_matrix(cases, manifest)
    report['summary']=dict(Counter(x['status'] for x in cases)); (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':report['status'],'summary':report['summary'],'report':str(out/'report.json'),'error':report.get('error')},ensure_ascii=False))
    return {'pass':0,'fail':1,'partial':2,'error':3}[report['status']]
if __name__=='__main__': raise SystemExit(main())
