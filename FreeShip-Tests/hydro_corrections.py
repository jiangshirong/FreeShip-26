"""Isolated real-code hydrostatic regressions (issues 07, 12 and 15)."""
from __future__ import annotations
import argparse, csv, hashlib, io, json, os, shutil, stat, subprocess, tempfile, time
from collections import Counter
from pathlib import Path
import xml.etree.ElementTree as ET
import freeship_ui_test as common
import file_safety as build

ROOT=Path(__file__).resolve().parent
CODE=('hydro_corrections.py','HydroCorrectionsProbe.pas','file_safety.py','freeship_ui_test.py')
NAMES=('SACs.txt','Bonjean.txt','Weights.txt','Sterns.txt')
LEGACY_REVISION='1de17e99fb2b1cc559cdf27c5cafb595a627a641'
def hashes(): return {n:common.digest_file(ROOT/n) for n in CODE}

def prepare(src,work,tools):
    project,_,snapshot=build.prepare(src,work,tools,False)
    shutil.copy2(ROOT/'HydroCorrectionsProbe.pas',work/'Units/HydroCorrectionsProbe.pas')
    build.replace(work/'FreeShip.dpr',"  Main in 'Forms/Main.pas' {MainForm},",
                  "  HydroCorrectionsProbe in 'Units/HydroCorrectionsProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
    build.replace(work/'FreeShip.dpr','  Application.Run;','  HydroCorrectionsProbe.RunHydroCorrectionsProbe(MainForm);')
    f=work/'Forms/FreeHydrostaticsFrm.pas'
    build.replace(f,'  public    { Public declarations }',
                  '  public    { Public declarations }\n    procedure ProbeAttachShip(Ship:TFreeShip);')
    build.replace(f,'  FreeHydrostaticsForm: TFreeHydrostaticsForm;',
                  '  FreeHydrostaticsForm: TFreeHydrostaticsForm;\n  HydroProbeDrafts,HydroProbeRows:TStringList;')
    build.replace(f,'function TFreeHydrostaticsForm.FGetStartDraft: single;',
                  'procedure TFreeHydrostaticsForm.ProbeAttachShip(Ship:TFreeShip); begin FFreeShip:=Ship end;\n\nfunction TFreeHydrostaticsForm.FGetStartDraft: single;')
    build.replace(f,'        HydObject.Calculate;',
                  '        if Assigned(HydroProbeDrafts) then HydroProbeDrafts.Add(FloatToStr(HydObject.Draft));\n        HydObject.Calculate;')
    build.replace(f,'      ResultsDlg.Execute;',
                  '      if Assigned(HydroProbeRows) then begin HydroProbeRows.Clear;\n'
                  '        for I:=2 to ResultsDlg.Grid.RowCount-1 do HydroProbeRows.Add(ResultsDlg.Grid.Cells[0,I]); end;')
    f=work/'Forms/FreeHydrostaticsDlg.pas'
    build.replace(f,'  FreeHydrostaticsDialog: TFreeHydrostaticsDialog;',
                  '  FreeHydrostaticsDialog: TFreeHydrostaticsDialog;\n  HydroProbeSaveAccepted,HydroProbeBlockFile:Boolean;\n  HydroProbeMessages:TStringList;')
    build.replace(f,'procedure TFreeHydrostaticsDialog.FormCreate(Sender: TObject);',
        'function HydroProbeMessage(const Msg:string; DlgType:TMsgDlgType; Buttons:TMsgDlgButtons; HelpCtx:LongInt):Integer;\n'
        'begin if Assigned(HydroProbeMessages) then HydroProbeMessages.Add(IntToStr(Ord(DlgType))+\':\'+Msg); Result:=mrOK end;\n\n'
        'procedure TFreeHydrostaticsDialog.FormCreate(Sender: TObject);')
    build.replace(f,'if not SaveDialog.Execute then Exit;','if not HydroProbeSaveAccepted then Exit;')
    t=f.read_text(encoding='utf-8-sig').replace('MessageDlg(', 'HydroProbeMessage(');f.write_text(t,encoding='utf8')
    build.replace(f,'    for I := 0 to FAdditionalReports.Count - 1 do\n      TStrings(',
        "    if HydroProbeBlockFile then CreateDir(IncludeTrailingPathDelimiter(Candidate) + 'Bonjean.txt');\n"
        '    for I := 0 to FAdditionalReports.Count - 1 do\n      TStrings(')
    # Use the checked-in legacy implementations as a migration oracle, only in
    # this disposable build. Their writes run exclusively in a private directory.
    oldcalc=subprocess.check_output(['git','show',LEGACY_REVISION+':Units/FreeHydrostaticCalc.inc'],cwd=src).decode('utf8')
    oldlayer=subprocess.check_output(['git','show',LEGACY_REVISION+':Units/FreeSubdivisionLayer.inc'],cwd=src).decode('utf8')
    oldadd=oldcalc[oldcalc.index('procedure TFreeHydrostaticCalc.AddData('):oldcalc.index('procedure TFreeHydrostaticCalc.AddHeader(')]
    oldcalc=oldcalc[oldcalc.index('procedure TFreeHydrostaticCalc.Calculate;'):oldcalc.index('procedure TFreeHydrostaticCalc.CalculateVolume(')]
    oldlayer=oldlayer[oldlayer.index('function TFreeSubdivisionLayer.FGetSurfaceProperties:'):oldlayer.index('procedure TFreeSubdivisionLayer.FSetFDevelopable(')]
    oracle=hashlib.sha256((oldcalc+oldadd+oldlayer).encode()).hexdigest()
    oldcalc=oldcalc.replace('procedure TFreeHydrostaticCalc.Calculate;','procedure TFreeHydrostaticCalc.LegacyCalculate;')
    oldadd=oldadd.replace('procedure TFreeHydrostaticCalc.AddData(', 'procedure TFreeHydrostaticCalc.LegacyAddData(').replace('.SurfaceProperties','.LegacySurfaceProperties')
    oldlayer=oldlayer.replace('function TFreeSubdivisionLayer.FGetSurfaceProperties:', 'function TFreeSubdivisionLayer.LegacySurfaceProperties:')
    with (work/'Units/FreeHydrostaticCalc.inc').open('a',encoding='utf8') as fh:fh.write('\n'+oldcalc+'\n'+oldadd)
    with (work/'Units/FreeSubdivisionLayer.inc').open('a',encoding='utf8') as fh:fh.write('\n'+oldlayer)
    build.replace(work/'Units/FreeShipUnit.pas','    function BuildAuxiliaryReports(Reports: TStringList): string;',
        '    function BuildAuxiliaryReports(Reports: TStringList): string;\n    procedure LegacyCalculate;\n    procedure LegacyAddData(Strings:TStringList; Mode:TFreeHydrostaticsMode; Separator:char);')
    build.replace(work/'Units/FreeGeometry.pas','    function CalculateSurfaceProperties(Report: TStrings): TLayerProperties;',
        '    function CalculateSurfaceProperties(Report: TStrings): TLayerProperties;\n    function LegacySurfaceProperties:TLayerProperties;')
    tree=ET.parse(project)
    for node in tree.findall('.//CompilerOptions/Target/Filename'): node.set('Value','hydro-corrections-probe')
    tree.write(project,encoding='utf8',xml_declaration=True)
    return project,work/'hydro-corrections-probe.exe',snapshot,oracle

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    ap.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS)
    ap.add_argument('--output',type=Path,default=ROOT/'reports'/('hydro-corrections-'+time.strftime('%Y%m%d-%H%M%S')))
    ap.add_argument('--timeout',type=int,default=240)
    a=ap.parse_args();src=a.source.resolve();out=a.output.resolve()
    if out.exists() or out==src or src in out.parents:raise SystemExit('Output must be new and outside source')
    out.mkdir(parents=True);cases=[];report={'status':'error','cases':cases,'test_code_sha256':hashes()}
    try:
        before,count=common.fingerprint(src);report.update(source_sha256=before,source_file_count=count)
        tools=common.junction_for(a.lazarus.resolve(),Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-hydro-corrections-') as run:
            work=run/'source';work.mkdir();profile=run/'profile';profile.mkdir()
            project,exe,snapshot,oracle=prepare(src,work,tools)
            report['legacy_output_oracle_sha256']=oracle
            report['legacy_output_revision']=LEGACY_REVISION
            if snapshot!=before or common.fingerprint(src)[0]!=before:raise RuntimeError('Source changed during snapshot')
            env=os.environ.copy();env.update(TEMP=str(run),TMP=str(run),APPDATA=str(profile),LOCALAPPDATA=str(profile))
            report['compiler_sha256']=build.compile_source(project,tools,run,env,out,a.timeout)
            report['instrumented_exe_sha256']=common.digest_file(exe)

            def execute(label,mode,cwd,filemode='',parallel=False):
                trial=run/label;trial.mkdir();pr=trial/'profile';pr.mkdir()
                e=env.copy();e.update(APPDATA=str(pr),LOCALAPPDATA=str(pr),FS_HYDRO_MODE=mode,
                    FS_HYDRO_FILE_MODE=filemode,FS_HYDRO_CWD=str(cwd),FS_HYDRO_REPORT=str(trial/'probe.json'),
                    FS_HYDRO_PO=str(work/'locale/FreeShip.cn.po'))
                if parallel:
                    return subprocess.Popen([str(exe)],cwd=cwd,env=e,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=common.FLAGS),trial
                p=subprocess.run([str(exe)],cwd=cwd,env=e,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS)
                collect(label,trial,p.returncode,p.stdout,p.stderr,mode)

            def collect(label,trial,code,stdout,stderr,mode):
                (out/(label+'.log')).write_bytes(stdout+b'\n'+stderr)
                data=json.loads((trial/'probe.json').read_text(encoding='utf-8-sig'))
                (out/(label+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
                for artifact in trial.glob('*.txt'):shutil.copy2(artifact,out/(label+'-'+artifact.name))
                cases.extend(data['cases'])
                if mode=='files':
                    for c in cases[-len(data['cases']):]:c['id']=label+'.'+c['id']
                if code or data.get('error'):raise RuntimeError(f'{label}: exit={code}; {data.get("error")}')
                expected={'files':6,'export':25,'numeric':106}[mode]
                if len(data['cases'])!=expected:raise RuntimeError(f'{label}: expected {expected} assertions, got {len(data["cases"])}')

            execute('numeric','numeric',work)
            export=run/'export-cwd';export.mkdir();execute('explicit-export','export',export)
            empty=run/'empty-cwd';empty.mkdir();execute('files-empty','files',empty,'empty')
            protected=run/'protected-cwd';protected.mkdir()
            for name in NAMES:
                p=protected/name;p.write_text('user-owned '+name,encoding='utf8');p.chmod(stat.S_IREAD)
            try:
                execute('files-protected','files',protected,'protected')
                tasks=[(label,*execute(label,'files',protected,'concurrent',True)) for label in ('concurrent-a','concurrent-b')]
                for label,p,trial in tasks:
                    stdout,stderr=p.communicate(timeout=a.timeout);collect(label,trial,p.returncode,stdout,stderr,'files')
                sid=next(csv.reader(io.StringIO(subprocess.check_output(['whoami','/user','/fo','csv','/nh'],text=True,creationflags=common.FLAGS))))[1]
                deny=subprocess.run(['icacls',str(protected),'/deny',f'*{sid}:(WD,AD)'],capture_output=True,creationflags=common.FLAGS)
                try:
                    if deny.returncode:raise RuntimeError('Unable to establish read-only directory ACL')
                    try:
                        (protected/'write-test.tmp').write_text('should not be writable')
                    except PermissionError:pass
                    else:raise RuntimeError('Directory remains writable; readonly test would be invalid')
                    execute('files-readonly-directory','files',protected,'readonly')
                finally:
                    subprocess.run(['icacls',str(protected),'/remove:d',f'*{sid}'],capture_output=True,check=True,creationflags=common.FLAGS)
            finally:
                for name in NAMES:(protected/name).chmod(stat.S_IREAD|stat.S_IWRITE)
            if common.fingerprint(src)[0]!=before:raise RuntimeError('Source changed during run')
            if hashes()!=report['test_code_sha256']:raise RuntimeError('Test code changed during run')
            report['source_unchanged_during_run']=True
            cases.append({'id':'hydro.native-ui','status':'not_run','expected':'native desktop dialog and report display acceptance',
                          'actual':'AGENTS.md prohibits desktop-operation acceptance; real calculation/button methods tested in an isolated hidden process'})
            report['status']=common.classify_statuses([c['status'] for c in cases])
    except Exception as exc:report.update(status='error',error=f'{type(exc).__name__}: {exc}')
    report['summary']=dict(Counter(c['status'] for c in cases));(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':report['status'],'summary':report['summary'],'error':report.get('error'),'report':str(out/'report.json')},ensure_ascii=False))
    return 0 if report['status']=='pass' else 2 if report['status']=='partial' else 1
if __name__=='__main__':raise SystemExit(main())
