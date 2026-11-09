"""Build a direct original form/FreeEdit numerical observer.

Original equations and operation entrypoints are untouched. Inputs are set just
before the original Calculate call. Only modal display and publication of
legacy shared files are disabled; the latter's obsolete row limit would reject
Holtrop's actual 20-row report after the original calculation finishes.
"""
import argparse,json,re,shutil
from pathlib import Path
import freeship_ui_test as common
import native_flicker_test as native
ROOT=Path(__file__).resolve().parent
ENTRIES=['Resistance_'+x for x in ('Delft','Holtr','Hollen','Oortmer','FungLeib','OST','RBHS','MH','Kaper','Planing')]
KEYS=('delft','holtrop','hollenbach','oortmerssen','fung','ost','rbhs','mh','kaper','planing')
def build(source,out):
    out.mkdir(parents=True,exist_ok=False);before=common.fingerprint(source)[0]
    r={'status':'error','source_sha256':before,'test_sources_sha256':{n:common.digest_file(ROOT/n) for n in ('qt_performance_oracle.py','PerformanceOracle.pas','PerformanceObserver.pas')}}
    try:
        with common.temporary_workspace('fs-performance-oracle-') as td:
            work=Path(td)
            def prepare(work):
                shutil.copy2(ROOT/'PerformanceObserver.pas',work/'Units/PerformanceObserver.pas')
                driver=work/'Tests/NativeLinesplanProbe.pas';text=driver.read_text('utf8');dispatch='\n'.join(f"if Method='{key}' then Ship.Edit.{entry} else" for key,entry in zip(KEYS,ENTRIES))+" raise Exception.Create('Unsupported oracle');"
                driver.write_text(text.replace('{ORACLE_DISPATCH}',dispatch),encoding='utf8')
                for entry in ENTRIES:
                    p=work/'Forms'/('Free'+entry+'Dlg.pas');s=p.read_text('utf-8-sig');s=re.sub(r'(implementation\s+uses)',r'\1 PerformanceObserver,',s,count=1,flags=re.I)
                    s=re.sub(r'(procedure TFree'+entry+r'\.(?:Calculate|doCalculate);.*?\bbegin\b)',r'\1\n  if OraclePreparing then Exit;',s,flags=re.S|re.I)
                    a=s.index('function TFree'+entry+'.Execute(');b=s.index('end;{TFree'+entry+'.Execute}',a);part=s[a:b]
                    part,n=re.subn(r'(\n\s*)(Calculate;\s*ShowModal;)',r'\1OracleInputs(Self);\1Calculate;\1OracleCapture(Self);',part,flags=re.I)
                    if n!=1:raise RuntimeError('Original Execute changed: '+entry)
                    s=s[:a]+part+s[b:];p.write_text(s,encoding='utf8')
                p=work/'Units/FreeCalculationFiles.pas';s=p.read_text('utf8').replace('procedure TFreeCalculationScope.Complete;\nbegin PublishFiles end;','procedure TFreeCalculationScope.Complete;\nbegin end; // observation only; no published cache')
                p.write_text(s,encoding='utf8')
                # Keep the observer's private working root stable while a form
                # updates scratch project settings. Numerical code is unchanged.
                p=work/'Units/FreeShip.inc';s=p.read_text('utf8');pos=s.index('function TFreeShip.CalculationRoot: string;');pos=s.index('\nbegin',pos)+6
                s=s[:pos]+"\n  Exit(IncludeTrailingPathDelimiter(ParamStr(3))+'oracle-cache');"+s[pos:];p.write_text(s,encoding='utf8')
            r['build']=native.build(source,work,common.DEFAULT_LAZARUS,out,'current','standalone',probe=ROOT/'PerformanceOracle.pas',prepare=prepare)
            shutil.copy2(work/'NativeLinesplanProbe.exe',out/'PerformanceOracle.exe')
            for name in ('locale','Themes','Languages'):shutil.copytree(source/name,out/name)
        if common.fingerprint(source)[0]!=before:raise RuntimeError('Source changed')
        r['status']='pass'
    except Exception as ex:r['error']=str(ex)
    (out/'report.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');return r
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE);p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=build(a.source.resolve(),a.output.resolve());print(json.dumps(r));raise SystemExit(0 if r['status']=='pass' else 1)
