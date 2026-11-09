"""Real undo and background-image operations in a disposable FreeShip build.

Only chooser answers, the dialog's intermediate edits/result, and snapshot-fault
sites are adapted. This tests transaction orchestration, not Lackenby's numeric
transformation or native dialog interaction.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import file_safety as build
import freeship_ui_test as common

ROOT=Path(__file__).resolve().parent
CODE=('undo_background_safety.py','UndoBackgroundProbe.pas','file_safety.py','freeship_ui_test.py')

def hashes(): return {name:common.digest_file(ROOT/name) for name in CODE}

DIALOG_ANSWER = r'''
function UiLackenbyAnswer(Ship:TFreeShip; var Modified:Boolean):Boolean;
var Mode:string; I,J:Integer; P:TFreeSubdivisionControlPoint;
  XYZ:T3DCoordinate; Snapshot:TFreeUndoObject;
begin
  Mode:=SysUtils.GetEnvironmentVariable('FS_UB_DIALOG');
  if Mode='' then raise Exception.Create('missing controlled dialog answer');
  Modified:=False;
  if (Pos('edited',Mode)>0) or (Mode='accept-fault') then
  begin
    for J:=1 to 2 do
    begin
      Snapshot:=Ship.Edit.CreateUndoObject('dialog intermediate',False);
      P:=nil;
      for I:=0 to Ship.Surface.NumberOfControlPoints-1 do
        if Ship.Surface.ControlPoint[I].Name='UndoBackgroundWitness' then P:=Ship.Surface.ControlPoint[I];
      if P=nil then raise Exception.Create('named point missing in dialog adapter');
      XYZ:=P.Coordinate; XYZ.X:=XYZ.X+0.25; P.Coordinate:=XYZ;
      Ship.FileChanged:=True; Snapshot.Accept;
    end;
    if Mode='throw-edited' then raise Exception.Create('injected dialog failure after edits');
    Modified:=True;
  end;
  if Pos('unreported',Mode)>0 then
    for I:=0 to Ship.Surface.NumberOfControlPoints-1 do
      if Ship.Surface.ControlPoint[I].Name='UndoBackgroundWitness' then Ship.Surface.ControlPoint[I].Locked:=False;
  if Mode='accept-fault' then Windows.SetEnvironmentVariable('FS_UB_ACCEPT_FAULT','1');
  Result:=(Pos('unchanged',Mode)=1) or (Mode='accept-edited') or (Mode='accept-fault');
end;

'''

def prepare(source,work,tools,mutation):
    project,exe,snapshot=build.prepare(source,work,tools,instrument=False)
    shutil.copy2(ROOT/'UndoBackgroundProbe.pas',work/'Units/UndoBackgroundProbe.pas')
    build.replace(work/'FreeShip.dpr',"  Main in 'Forms/Main.pas' {MainForm},",
                  "  UndoBackgroundProbe in 'Units/UndoBackgroundProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
    build.replace(work/'FreeShip.dpr','  Application.Run;',
                  '  Application.ShowMainForm := False;\n  UndoBackgroundProbe.RunUndoBackgroundProbe(MainForm);')
    edit=work/'Units/FreeEdit.inc'
    build.replace(edit,'procedure TFreeEdit.Model_LackenbyTransformation;',DIALOG_ANSWER+'procedure TFreeEdit.Model_LackenbyTransformation;')
    build.replace(edit,'        Accepted := Dialog.Execute(FreeShip, Modified);','        Accepted := UiLackenbyAnswer(FreeShip, Modified);')
    text=edit.read_text(encoding='utf8')
    for name in ('CreateUndoObject','CreateRedoObject'):
        start=text.index('function TFreeEdit.'+name)
        end=text.index('end;{TFreeEdit.'+name+'}',start)
        part=text[start:end]
        anchor='      FreeShip.SaveProject(UndoObject.FUndoData);'
        if part.count(anchor)!=1: raise RuntimeError('snapshot fault adapter changed: '+name)
        part=part.replace(anchor,"      if SysUtils.GetEnvironmentVariable('FS_UB_SNAPSHOT_FAULT')='1' then raise Exception.Create('injected snapshot serialization failure');\n"+anchor)
        text=text[:start]+part+text[end:]
    edit.write_text(text,encoding='utf8',newline='\n')
    build.replace(work/'Units/FreeUndoObject.inc','  if Owner.FUndoObjects.IndexOf(Self) <> -1 then Exit;',
        "  if SysUtils.GetEnvironmentVariable('FS_UB_ACCEPT_FAULT')='1' then raise EOutOfMemory.Create('injected undo capacity failure');\n"
        '  if Owner.FUndoObjects.IndexOf(Self) <> -1 then Exit;')
    text=edit.read_text(encoding='utf8')
    start=text.index('procedure TFreeEdit.BackgroundImage_Open('); end=text.index('end;{TFreeEdit.BackgroundImage_Open}',start)
    part=text[start:end]
    part,n=re.subn(r'if MessageDlg\([\s\S]*?= mrNo then','if False then',part)
    if n!=1: raise RuntimeError('background replacement confirmation adapter changed')
    anchor='  if Dialog.Execute then'
    if part.count(anchor)!=1: raise RuntimeError('background menu chooser adapter changed')
    part=part.replace(anchor,"  Dialog.FileName := SysUtils.GetEnvironmentVariable('FS_UB_IMAGE');\n  if Dialog.FileName<>'' then")
    edit.write_text(text[:start]+part+text[end:],encoding='utf8',newline='\n')
    background=work/'Units/FreeBackgroundImage.inc'; text=background.read_text(encoding='utf8')
    start=text.index('procedure TFreeBackgroundImage.Open('); end=text.index('end;{TFreeBackgroundImage.Open}',start)
    part=text[start:end]; anchor='  if Dialog.Execute then'
    if part.count(anchor)!=1: raise RuntimeError('background chooser adapter changed')
    part=part.replace(anchor,"  Dialog.FileName := SysUtils.GetEnvironmentVariable('FS_UB_IMAGE');\n  if Dialog.FileName<>'' then")
    background.write_text(text[:start]+part+text[end:],encoding='utf8',newline='\n')
    if mutation=='eager-redo':
        build.replace(edit,'    // Preparing a snapshot must not discard the existing redo branch.',
            '    while FreeShip.FUndoObjects.Count > FreeShip.FUndoPosition do FreeShip.UndoObject[FreeShip.FUndoObjects.Count-1].Delete;\n'
            '    // Preparing a snapshot must not discard the existing redo branch.')
    elif mutation=='freed-jpeg':
        build.replace(background,'      NewQuality := JPEGImage.CompressionQuality;',
            '      FreeAndNil(JPEGImage);\n      NewQuality := JPEGImage.CompressionQuality;')
    elif mutation=='early-clear-menu':
        build.replace(edit,'    Loaded := TFreeBackgroundImageData.Create(FFreeShip);',
            '    if Data <> nil then Data.Clear;\n    Loaded := TFreeBackgroundImageData.Create(FFreeShip);')
    return project,exe,snapshot

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    parser.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS)
    parser.add_argument('--output',type=Path,default=ROOT/'reports'/('undo-background-'+time.strftime('%Y%m%d-%H%M%S')))
    parser.add_argument('--timeout',type=int,default=240)
    parser.add_argument('--mutation',choices=('none','eager-redo','freed-jpeg','early-clear-menu'),default='none',help='Restore a known fault only in the disposable copy; failures are expected')
    args=parser.parse_args(); source=args.source.resolve(); output=args.output.resolve()
    if output.exists() or output==source or source in output.parents: raise SystemExit('Output must be new and outside the source')
    output.mkdir(parents=True)
    report={'status':'error','cases':[],'mutation':args.mutation,'test_code_sha256':hashes(),'fixture_sha256':common.digest_file(common.FIXTURE),
            'limitations':['Hidden in-process execution, not native dialog or desktop acceptance.',
                           'Lackenby dialog answers and two accepted intermediate point edits are simulated; numerical transformation is not exercised.']}
    before=None
    try:
        before,count=common.fingerprint(source); report.update(source_sha256=before,source_file_count=count)
        tools=common.junction_for(args.lazarus.resolve(),Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-undo-background-') as run:
            work=run/'source'; work.mkdir(); evidence=run/'evidence'; evidence.mkdir(); profile=run/'profile'; profile.mkdir()
            project,exe,snapshot=prepare(source,work,tools,args.mutation)
            if before!=snapshot or common.fingerprint(source)[0]!=before: raise RuntimeError('Source changed during snapshot')
            fixture=run/'fixture.fbm'; shutil.copy2(common.FIXTURE,fixture)
            from PIL import Image
            for name,size in [('first.jpg',(19,11)),('second.jpg',(32,7)),('valid.bmp',(6,9))]:
                img=Image.new('RGB',size)
                img.putdata([((x*17+y*3)%256,(x*5+y*21)%256,(x*11+y*9)%256) for y in range(size[1]) for x in range(size[0])])
                img.save(evidence/name)
            (evidence/'broken.jpg').write_bytes(b'not a JPEG\x00\xff')
            report['image_fixture_sha256']={p.name:common.digest_file(p) for p in evidence.iterdir()}
            report['instrumented_source_sha256']=common.fingerprint(work)[0]
            env=os.environ.copy(); env.update(TEMP=str(run),TMP=str(run),APPDATA=str(profile),LOCALAPPDATA=str(profile),
                FS_UB_FIXTURE=str(fixture),FS_UB_OUTPUT=str(evidence),FS_UB_IMAGES=str(evidence))
            report['compiler_sha256']=build.compile_source(project,tools,run,env,output,args.timeout)
            report['executable_sha256']=common.digest_file(exe)
            process=subprocess.run([str(exe)],cwd=work,env=env,capture_output=True,timeout=90,creationflags=common.FLAGS)
            (output/'runtime.log').write_bytes(process.stdout+b'\n'+process.stderr)
            shutil.copytree(evidence,output/'evidence')
            raw=json.loads((evidence/'probe.json').read_text(encoding='utf-8-sig')); report['cases']=raw['cases']
            if raw.get('error') or process.returncode: raise RuntimeError(f'Probe error: exit {process.returncode}: {raw.get("error")}')
            if len(report['cases'])!=59 or len({x['id'] for x in report['cases']})!=59: raise RuntimeError('Incomplete or duplicate scenarios')
            if common.digest_file(fixture)!=report['fixture_sha256']: raise RuntimeError('Isolated fixture changed')
            report['status']=common.classify_statuses(x['status'] for x in report['cases'])
    except Exception as exc: report.update(status='error',error=f'{type(exc).__name__}: {exc}')
    if before and common.fingerprint(source)[0]!=before: report.update(status='error',error='Source changed during validation')
    if hashes()!=report['test_code_sha256'] or common.digest_file(common.FIXTURE)!=report['fixture_sha256']:
        report.update(status='error',error='Test code or fixture changed')
    report['counts']=dict(Counter(x['status'] for x in report['cases']))
    (output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':report['status'],'counts':report['counts'],'error':report.get('error'),'report':str(output/'report.json')},ensure_ascii=False))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': raise SystemExit(main())
