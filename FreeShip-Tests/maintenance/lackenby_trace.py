"""Build a private original-core Lackenby numerical trace; diagnostic only.

No user source changes. Compile native lackenby_edit.cpp with
FREESHIP_LACKENBY_TRACE to obtain matching stderr rows. Use qt_native_lackenby's
--probe with this runtime for the same input/targets. Reports remain separate
from normal regression because these binaries are instrumented.
"""
import argparse,json,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT.parent/'FreeShip-Qt/bridge'))
import freeship_ui_test as common
import native_flicker_test as native
import build_bridge


def instrument(work):
    build_bridge.adapt_scene(work)
    path=work/'Units/LackenbyEngine.pas';text=path.read_text('utf8')
    text=text.replace('implementation\n','''implementation
procedure Trace(const Kind:string;const Values:array of Double);
var I:Integer;
begin
  Write(StdErr,Kind);
  for I:=Low(Values) to High(Values) do Write(StdErr,' ',FloatToStrF(Values[I],ffExponent,18,0));
  WriteLn(StdErr);
end;
''',1)
    text=text.replace('    SimpsonData[I - 1].Area := Area;',"    Trace('station',[Station.Plane.d,Area,COG.X]);\n    SimpsonData[I - 1].Area := Area;")
    text=text.replace('  Stations.Capacity := Stations.Capacity;',"  Trace('part',[Prop.Displacement,Prop.LCB,Prop.Cp,Prop.Length,Prop.k]);\n  Stations.Capacity := Stations.Capacity;")
    text=text.replace('    Prop.k := Prop.k + Prod * SimpsonData[I - 1].Distance * SimpsonData[I - 1].Distance;',"    Prop.k := Prop.k + Prod * SimpsonData[I - 1].Distance * SimpsonData[I - 1].Distance;\n    Trace('accum',[SimpsonData[I-1].Area,SimpsonData[I-1].Distance,SimpsonData[I-1].SF,Prod,Prop.LCB]);")
    text=text.replace('          for I := 1 to Points.Count do',"          Trace('iteration',[Iteration,ConvFactor,dz,AftProperties.A,AftProperties.B,AftProperties.dCp,ForeProperties.A,ForeProperties.B,ForeProperties.dCp]);\n          for I := 1 to Points.Count do")
    path.write_text(text,'utf8')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--resources',type=Path,required=True);a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);work=out/'source';work.mkdir()
    result=native.build(common.DEFAULT_SOURCE,work,common.DEFAULT_LAZARUS,out,'current','standalone',probe=ROOT.parent/'FreeShip-Qt/bridge/PilotBridge.pas',prepare=instrument)
    runtime=out/'runtime';shutil.copytree(a.resources,runtime);shutil.copy2(work/'NativeLinesplanProbe.exe',runtime/'bridge/FreeShipGeometryBridge.exe')
    (out/'report.json').write_text(json.dumps(result,indent=2),'utf8');print(runtime)


if __name__=='__main__':main()
