"""Unit switching regression: real setters, intersection geometry and hydrostatics."""
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

import file_safety as build
import freeship_ui_test as common

ROOT=Path(__file__).resolve().parent
FILES=('units_conversion.py','UnitsConversionProbe.pas','file_safety.py','freeship_ui_test.py')


def hashes():
    return {name:common.digest_file(ROOT/name) for name in FILES}


def instrument(work, mutation):
    shutil.copy2(ROOT/'UnitsConversionProbe.pas',work/'Units'/'UnitsConversionProbe.pas')
    build.replace(work/'FreeShip.dpr',"  Main in 'Forms/Main.pas' {MainForm},",
                  "  UnitsConversionProbe in 'Units/UnitsConversionProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
    build.replace(work/'FreeShip.dpr','  Application.Run;','  UnitsConversionProbe.RunUnitsConversion(MainForm);')
    build.replace(work/'Units'/'FreeShipUnit.pas','    // User input of mainparticulars and project setting',
                  '    procedure UiSeedUnitInputs;\n    function UiUnitInput(Index:Integer):Double;\n    // User input of mainparticulars and project setting')
    build.replace(work/'Units'/'FreeProjectSettings.inc','procedure TFreeProjectSettings.Clear;', '''procedure TFreeProjectSettings.UiSeedUnitInputs;
begin
  FMinimumDisplacement:=100; FMaximumDisplacement:=1000; FDisplIncrement:=25;
  FNoDisplacements:=2; SetLength(FDisplacements,2); FDisplacements[0]:=125; FDisplacements[1]:=700;
  FVCG:=2.4; FNoStabTrims:=2; SetLength(FStabTrims,2); FStabTrims[0]:=-0.3; FStabTrims[1]:=0.6;
  FNoAngles:=2; SetLength(FAngles,2); FAngles[0]:=15; FAngles[1]:=30;
end;
function TFreeProjectSettings.UiUnitInput(Index:Integer):Double;
begin
  case Index of
    0:Result:=FMinimumDisplacement; 1:Result:=FMaximumDisplacement; 2:Result:=FDisplIncrement;
    3:Result:=FDisplacements[0]; 4:Result:=FDisplacements[1]; 5:Result:=FVCG;
    6:Result:=FStabTrims[0]; 7:Result:=FStabTrims[1]; 8:Result:=FAngles[0]; 9:Result:=FAngles[1];
    else raise Exception.Create('Unknown unit test field');
  end;
end;
procedure TFreeProjectSettings.Clear;''')
    build.replace(work/'Units'/'FreeShipUnit.pas','    property ProjectSettings: TFreeProjectSettings read FProjectSettings;',
                  '    procedure UiSeedResistanceInputs;\n    function UiResistanceInput(Index:Integer):Double;\n    property ProjectSettings: TFreeProjectSettings read FProjectSettings;')
    build.replace(work/'Units'/'FreeShip.inc','procedure TFreeShip.Clear;', '''procedure TFreeShip.UiSeedResistanceInputs;
begin
  FResistanceKaperData.Displacement:=23.75; FResistanceKaperData.Extract:=False;
  FResistancePlaningData.Displacement:=125.5; FResistancePlaningData.Extract:=False;
  FResistanceDelftData.Displacement:=75;
  FResistanceKaperData.Lwl:=12; FResistancePlaningData.Lwl:=20;
end;
function TFreeShip.UiResistanceInput(Index:Integer):Double;
begin
  case Index of
    0:Result:=FResistanceKaperData.Displacement; 1:Result:=FResistancePlaningData.Displacement;
    2:Result:=FResistanceDelftData.Displacement; 3:Result:=Ord(FResistanceKaperData.Extract);
    4:Result:=Ord(FResistancePlaningData.Extract); 5:Result:=FResistanceKaperData.Lwl;
    6:Result:=FResistancePlaningData.Lwl;
    else raise Exception.Create('Unknown resistance test field');
  end;
end;
procedure TFreeShip.Clear;''')
    # Exercise real command-to-form binding and Execute initialization, then
    # observe its inputs before the calculation/modal UI boundary.
    for name in ('Kaper','Planing'):
        form=work/'Forms'/f'FreeResistance_{name}Dlg.pas'
        build.replace(form,f'  FreeResistance_{name}: TFreeResistance_{name};',
                      f'  FreeResistance_{name}: TFreeResistance_{name};\n  UnitsProbeDisplacement:Double;\n  UnitsProbeAutoExtract:Boolean;\n  UnitsProbeCalls:Integer;')
        build.replace(form,'  Calculate;\n  ShowModal;\n  Result := modalResult = mrOk;',
                      '  UnitsProbeDisplacement:=Displ;\n  UnitsProbeAutoExtract:=AutoExtract;\n  Inc(UnitsProbeCalls);\n  Result:=False;')
    if mutation=='old-density':
        build.replace(work/'Units'/'FreeGeometry.pas',
                      '  DensityConversionFactor = MetricTonneKilograms * Foot * Foot * Foot / PoundKilograms;',
                      '  DensityConversionFactor = MetricTonneKilograms * Foot * Foot * Foot / 0.44642857;')
    elif mutation=='fixed-planes':
        for plural,singular in [('Stations','Station'),('Buttocks','Buttock'),('Waterlines','Waterline'),('Diagonals','Diagonal')]:
            build.replace(work/'Units'/'FreeProjectSettings.inc',
                          f'    for I := 0 to FreeShip.NumberOf{plural} - 1 do\n      FreeShip.{singular}[I].FPlane.d := FreeShip.{singular}[I].FPlane.d * UnitConversionFactor;',
                          f'    {{ mutation: leave {plural} in original units }}')
    elif mutation=='old-resistance-mass':
        for name in ('Kaper','Planing'):
            build.replace(work/'Units'/'FreeProjectSettings.inc',
                          f'    FreeShip.FResistance{name}Data.Displacement := {name}Displacement * MassFactor;',
                          f'    {{ mutation: retain geometric volume factor for {name} mass }}')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    parser.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS)
    parser.add_argument('--output',type=Path,default=ROOT/'reports'/('unit-conversion-'+time.strftime('%Y%m%d-%H%M%S')))
    parser.add_argument('--mutation',choices=['none','old-density','fixed-planes','old-resistance-mass'],default='none')
    parser.add_argument('--timeout',type=int,default=240)
    args=parser.parse_args(); src=args.source.resolve(); out=args.output.resolve()
    if out.exists() or src==out or src in out.parents:
        raise SystemExit('Output must be new and outside production source')
    out.mkdir(parents=True)
    report={'status':'error','cases':[],'mutation':args.mutation,'test_code_sha256':hashes(),
            'fixture_sha256':common.digest_file(common.FIXTURE),
            'limitations':['Hidden compiled application; no real desktop interaction.',
                           'Box fixture tests hull geometry, four cut-plane types, hydrostatic volume/displacement, layer weight and persisted settings.',
                           'Kaper/Planing manual displacement tests reach real form initialization but bypass calculation and modal display; they do not validate either resistance algorithm.',
                           'Historical imperial density is preserved; external calculation program inputs and every historical model are not validated.']}
    before=None
    try:
        before,count=common.fingerprint(src); report.update(source_sha256=before,source_file_count=count)
        tools=common.junction_for(args.lazarus.resolve(),Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-unit-conversion-') as run:
            work=run/'source'; work.mkdir(); profile=run/'profile'; profile.mkdir(); evidence=run/'evidence'; evidence.mkdir()
            project,exe,snapshot=build.prepare(src,work,tools,instrument=False)
            if snapshot!=before or common.fingerprint(src)[0]!=before: raise RuntimeError('Source changed during snapshot')
            instrument(work,args.mutation)
            fixture=run/'fixture.fbm'; shutil.copy2(common.FIXTURE,fixture)
            env=os.environ.copy(); env.update(TEMP=str(run),TMP=str(run),APPDATA=str(profile),LOCALAPPDATA=str(profile),FS_UNITS_FIXTURE=str(fixture),FS_UNITS_OUTPUT=str(evidence))
            report['instrumented_source_sha256']=common.fingerprint(work)[0]
            report['compiler_sha256']=build.compile_source(project,tools,run,env,out,args.timeout)
            report['executable_sha256']=common.digest_file(exe)
            child=subprocess.run([str(exe)],cwd=work,env=env,capture_output=True,timeout=90,creationflags=common.FLAGS)
            (out/'runtime.log').write_bytes(child.stdout+b'\n'+child.stderr)
            shutil.copytree(evidence,out/'evidence')
            data=json.loads((evidence/'probe.json').read_text(encoding='utf-8-sig')); report['cases']=data['cases']
            if child.returncode or data.get('error'): raise RuntimeError(f'Probe error: {data.get("error")}, exit={child.returncode}')
            if len(report['cases'])<200 or len({c['id'] for c in report['cases']})!=len(report['cases']): raise RuntimeError('Missing/duplicate observations')
            report['status']=common.classify_statuses([c['status'] for c in report['cases']])
            if common.digest_file(fixture)!=report['fixture_sha256']: raise RuntimeError('Isolated fixture changed')
    except Exception as exc:
        report['status']='error'; report['error']=f'{type(exc).__name__}: {exc}'
    if before and common.fingerprint(src)[0]!=before:
        report['status']='error'; report['source_changed']=True; report['error']='Source changed during validation'
    if hashes()!=report['test_code_sha256'] or common.digest_file(common.FIXTURE)!=report['fixture_sha256']:
        report['status']='error'; report['test_or_fixture_changed']=True; report['error']='Test code or original fixture changed'
    report['counts']=dict(Counter(c['status'] for c in report['cases']))
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':report['status'],'counts':report['counts'],'error':report.get('error'),'report':str(out/'report.json')},ensure_ascii=False))
    return 0 if report['status']=='pass' else 1


if __name__=='__main__': raise SystemExit(main())
