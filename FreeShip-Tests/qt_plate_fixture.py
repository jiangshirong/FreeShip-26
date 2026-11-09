"""Original-core fixtures and observer; inject only a point-count accessor."""
import argparse,json,shutil
from pathlib import Path
import freeship_ui_test as common
import native_flicker_test as native
ROOT=Path(__file__).resolve().parent
def build(source,out):
    out.mkdir(parents=True,exist_ok=False);before=common.fingerprint(source)[0]
    report={'status':'error','source_sha256':before,'test_sources_sha256':{n:common.digest_file(ROOT/n) for n in ('qt_plate_fixture.py','QtPlateFixture.pas')}}
    try:
        with common.temporary_workspace('fs-plate-oracle-') as folder:
            work=Path(folder)
            def prepare(work):
                p=work/'Units/FreeGeometry.pas';s=p.read_text('utf-8-sig');anchor='    property NormalizedError: extended read FNormalizedError;'
                if s.count(anchor)!=1:raise RuntimeError('Point-count accessor anchor changed')
                p.write_text(s.replace(anchor,anchor+'\n    function ProbePointCount:Integer;'),encoding='utf8')
                p=work/'Units/FreeDevelopedPatch.inc';p.write_text('function TFreeDevelopedPatch.ProbePointCount:Integer; begin Result:=FPoints.Count end;\n'+p.read_text('utf-8-sig'),encoding='utf8')
            report['build']=native.build(source,work,common.DEFAULT_LAZARUS,out,'current','standalone',probe=ROOT/'QtPlateFixture.pas',prepare=prepare)
            shutil.copy2(work/'NativeLinesplanProbe.exe',out/'QtPlateFixture.exe')
            for name in ('locale','Themes','Languages'):shutil.copytree(source/name,out/name)
        if common.fingerprint(source)[0]!=before:raise RuntimeError('Original source changed')
        report['status']='pass'
    except Exception as ex:report['error']=str(ex)
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');return report
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE);p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=build(a.source.resolve(),a.output.resolve());print(json.dumps({'status':r['status'],'error':r.get('error')}));raise SystemExit(0 if r['status']=='pass' else 1)
