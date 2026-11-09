"""Private original-core fixtures/oracles for Qt project and layer editing.

The helper injects only observation and seeding entry points into a temporary
copy. It never imports the new ModelService snapshot serializer as an oracle.
"""
from __future__ import annotations
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import re
import xml.etree.ElementTree as ET

import freeship_ui_test as common
import native_flicker_test as native

ROOT = Path(__file__).resolve().parent

DECLARATIONS = '''    procedure QtEditingSeedAux;
    procedure QtEditingClearAux;
    function QtEditingFlowPoint(Index:Integer):T2DCoordinate;
    function QtEditingFlowView(Index:Integer):TFreeViewType;
    property ProjectSettings: TFreeProjectSettings read FProjectSettings;'''

IMPLEMENTATION = '''procedure TFreeShip.QtEditingClearAux;
var I:Integer;
begin
  for I:=FBackgroundImages.Count-1 downto 0 do FBackgroundImages[I].Free;
  FBackgroundImages.Clear;
  while NumberOfFlowlines>0 do Flowline[0].Delete;
end;

procedure TFreeShip.QtEditingSeedAux;
var Data:TFreeBackgroundImageData;Flow:TFreeFlowline;Bitmap:TBitmap;
begin
  QtEditingClearAux;
  Data:=TFreeBackgroundImageData.Create(Self);Data.Clear;
  Data.FAssignedView:=fvProfile;Data.FOrigin:=Point(7,-11);
  Data.FScale:=0.125;Data.FQuality:=100;Data.FVisible:=True;
  Bitmap:=TBitmap.Create;
  try
    Bitmap.PixelFormat:=pf24bit;Bitmap.SetSize(8,6);
    Bitmap.Canvas.Brush.Color:=clWhite;Bitmap.Canvas.FillRect(Rect(0,0,8,6));
    Bitmap.Canvas.Brush.Color:=clBlue;Bitmap.Canvas.FillRect(Rect(0,0,4,3));
    Data.FImageData.Assign(Bitmap);
  finally Bitmap.Free end;
  FBackgroundImages.Add(Data);
  Flow:=TFreeFlowline.Create(Self);Flow.FProjectionPoint.X:=1.25;
  Flow.FProjectionPoint.Y:=2.5;Flow.FProjectionView:=fvProfile;
  Flow.FBuild:=False;FFlowlines.Add(Flow);
end;

function TFreeShip.QtEditingFlowPoint(Index:Integer):T2DCoordinate;
begin Result:=Flowline[Index].FProjectionPoint end;
function TFreeShip.QtEditingFlowView(Index:Integer):TFreeViewType;
begin Result:=Flowline[Index].FProjectionView end;

procedure TFreeShip.Clear;'''


def _builder(qt_source):
    spec=importlib.util.spec_from_file_location('qt_editing_bridge_adapter',qt_source/'bridge/build_bridge.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def build(source:Path,qt_source:Path,out:Path)->Path:
    source,qt_source,out=source.resolve(),qt_source.resolve(),out.resolve()
    if source==out or source in out.parents or qt_source==out or qt_source in out.parents:
        raise ValueError('Fixture output must be outside product sources')
    out.mkdir(parents=True,exist_ok=False)
    adapter=_builder(qt_source)
    before=common.fingerprint(source)[0]
    hashes=adapter.adapter_hashes()
    tests={name:common.digest_file(ROOT/name) for name in ('QtEditingFixture.pas','qt_editing_fixture.py')}
    report={'status':'error','source_sha256':before,'adapter_sha256':hashes,'test_sha256':tests,
            'scope':'Direct original-core object inspection and private fixture construction.'}
    try:
        def prepare(work):
            adapter.adapt_scene(work)
            path=work/'Units/FreeShipUnit.pas';text=path.read_text('utf-8-sig')
            text=adapter.replace_once(text,'    property ProjectSettings: TFreeProjectSettings read FProjectSettings;',DECLARATIONS)
            path.write_text(text,encoding='utf8')
            path=work/'Units/FreeShip.inc';text=path.read_text('utf-8-sig')
            path.write_text(adapter.replace_once(text,'procedure TFreeShip.Clear;',IMPLEMENTATION),encoding='utf8')
            # Retain the same deterministic crease exporter used by the service.
            # This observer is not launched with --serve, so select it explicitly.
            path=work/'Units/FreeLinesplanScene.pas';text=path.read_text('utf-8-sig')
            path.write_text(text.replace("if ParamStr(1)='--serve' then ExtractStableEdgeLoops",'if True then ExtractStableEdgeLoops'),encoding='utf8')
            # Test-only symbols make numerical failures attributable, not anonymous addresses.
            path=work/'FreeShip.lpi';tree=ET.parse(path)
            mode=next(n for n in tree.findall('.//BuildModes/*') if n.get('Name')=='Win64 Release')
            debug=mode.find('./CompilerOptions/Linking/Debugging')
            for key,value in [('GenerateDebugInfo','True'),('UseLineInfoUnit','True'),('StripSymbols','False'),('DebugInfoType','dsDwarf2')]:
                node=debug.find(key)
                if node is None:node=ET.SubElement(debug,key)
                node.set('Value',value)
            tree.write(path,encoding='utf8',xml_declaration=True)
        with common.temporary_workspace('fs-qt-editing-fixture-') as temporary:
            work=Path(temporary)
            report['build']=native.build(source,work,common.DEFAULT_LAZARUS,out,'current','standalone',
                probe=ROOT/'QtEditingFixture.pas',prepare=prepare)
            executable=out/'QtEditingFixture.exe'
            shutil.copy2(work/'NativeLinesplanProbe.exe',executable)
        for name in ('locale','Themes','Languages'):
            shutil.copytree(source/name,out/name)
        if common.fingerprint(source)[0]!=before:raise RuntimeError('Original source changed during fixture build')
        if adapter.adapter_hashes()!=hashes:raise RuntimeError('Bridge adapter changed during fixture build')
        if any(common.digest_file(ROOT/name)!=digest for name,digest in tests.items()):
            raise RuntimeError('Fixture source changed during build')
        report['status']='pass'
    except Exception as error:
        report['error']=f'{type(error).__name__}: {error}'
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    if report['status']!='pass':raise RuntimeError('Fixture build failed: '+str(out/'report.json')+'; '+report.get('error',''))
    return executable


def run(exe:Path,model:Path,out:Path,mode='inspect',request=None)->dict:
    if mode not in ('inspect','seed','imperial','metric','curvature','flow','advanced_math','advanced_core','advanced_seed','legacy_exports','hydro'):
        raise ValueError('Unknown fixture mode')
    exe,model,out=exe.resolve(strict=True),model.resolve(strict=True),out.resolve()
    out.mkdir(parents=True,exist_ok=False)
    digest=common.digest_file(model)
    private=out/'input.fbm';shutil.copy2(model,private)
    if mode in ('curvature','flow','advanced_math','advanced_core','hydro'):(out/'request.json').write_text(json.dumps(request,ensure_ascii=False),encoding='utf8')
    env=os.environ.copy()
    for key in ('APPDATA','LOCALAPPDATA','TEMP','TMP'):
        directory=out/key;directory.mkdir();env[key]=str(directory)
    result=subprocess.run([str(exe),str(private),str(out),mode],cwd=out,env=env,
        capture_output=True,timeout=180,creationflags=common.FLAGS)
    (out/'fixture.log').write_bytes(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(f'Fixture {mode} failed ({result.returncode}): {out/"fixture.log"}')
    data=json.loads((out/'state.json').read_text('utf8'))
    if mode=='legacy_exports':
        symbolizer=Path(os.environ['LOCALAPPDATA'])/'FreeShip26QtTools/mingw1310_64/bin/addr2line.exe'
        addresses=re.findall(rb'\$([0-9A-Fa-f]{16})',result.stderr)
        if symbolizer.is_file() and addresses:
            trace=subprocess.run([str(symbolizer),'-f','-e',str(exe),*['0x'+a.decode('ascii') for a in addresses]],capture_output=True,timeout=30,creationflags=common.FLAGS)
            (out/'symbolized-stack.txt').write_bytes(trace.stdout+trace.stderr)
    if common.digest_file(model)!=digest or common.digest_file(private)!=digest:
        raise RuntimeError('Fixture changed its input model')
    data['artifacts']={name:str(out/name) for name in
        ('seed.fbm','constraints.fbm','legacy.fbm','no-intersections.fbm','system-colors.fbm','converted.fbm','advanced.fbm',
         'constraints-state.json','legacy-state.json','no-intersections-state.json',
         'system-colors-state.json') if (out/name).exists()}
    data['input_sha256']=digest
    return data


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    parser.add_argument('--qt-source',type=Path,default=ROOT.parent/'FreeShip-Qt')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--model',type=Path,default=common.FIXTURE)
    parser.add_argument('--mode',choices=('inspect','seed','imperial','metric'),default='seed')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    report={'status':'error'}
    try:
        executable=build(args.source,args.qt_source,out/'build')
        report['state']=run(executable,args.model,out/'run',args.mode)
        report['status']='pass'
    except Exception as error:report['error']=str(error)
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':report['status'],'report':str(out/'report.json'),'error':report.get('error')},ensure_ascii=False))
    return 0 if report['status']=='pass' else 3


if __name__=='__main__':raise SystemExit(main())
