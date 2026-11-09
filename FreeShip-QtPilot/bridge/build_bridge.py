"""Build the version-checked adapter in an isolated copy; product sources stay intact."""
import argparse, json, shutil, sys, zipfile
from pathlib import Path
PILOT=Path(__file__).resolve().parents[1]
TESTS=PILOT.parent/'FreeShip-Tests'
sys.path.insert(0,str(TESTS))
import freeship_ui_test as common
import native_flicker_test as native

def replace_once(s,a,b):
    if s.count(a)!=1:raise RuntimeError('Bridge adapter no longer matches: '+a)
    return s.replace(a,b,1)

def adapt(work):
    shutil.copy2(PILOT/'bridge/PilotCapture.pas',work/'Units/PilotCapture.pas')
    path=work/'Forms/FreeLinesplanFrme.pas';s=path.read_text('utf-8-sig')
    s=replace_once(s,'uses FreeStringsUnit;','uses FreeStringsUnit, PilotCapture;')
    s=replace_once(s,'  UsingInteractionPreview := DrawInteractionPreview;',
        '  if PilotActive then UsingInteractionPreview:=False else UsingInteractionPreview := DrawInteractionPreview;')
    # Observe coordinates before any screen or atlas integer rounding.
    a=s.index('  procedure DrawLineAtt(');b=s.index('  procedure DrawDiagonalLine(',a)
    part=s[a:b];part=replace_once(part,'    Viewport.Polyline(Pts);',
        '    PilotLine(Proj1.X,Proj1.Y,Proj2.X,Proj2.Y,Viewport.PenColor);\n    Viewport.Polyline(Pts);')
    s=s[:a]+part+s[b:]
    a=s.index('  procedure DrawDiagonalLine(');b=s.index('  procedure QueueKnuckle(',a)
    part=s[a:b].replace('    Pt := Viewport.Project(P);','    PilotPoint(P.X,P.Y);\n    Pt := Viewport.Project(P);')
    part=part.replace('    Viewport.LineTo(Pt.X, Pt.Y);','    PilotFlush(2,Viewport.PenColor);\n    Viewport.LineTo(Pt.X, Pt.Y);')
    s=s[:a]+part+s[b:]
    a=s.index('  procedure DrawLine(P1');b=s.index('  {$ifdef NOUSE}',a)
    part=s[a:b].replace('QueueKnuckle(Pts[0],Pts[1]);',
        'PilotLine(Proj1.X,Proj1.Y,Proj2.X,Proj2.Y,Viewport.PenColor);\n      QueueKnuckle(Pts[0],Pts[1]);')
    s=s[:a]+part+s[b:]
    a=s.index('  procedure DrawSpline(',s.index('{$endif NOUSE}'));b=s.index('  procedure DrawIntersection(',a)
    part=s[a:b].replace('Pts[i] := Viewport.Project(Pr);','PilotPoint(Pr.X,Pr.Y);\n              Pts[i] := Viewport.Project(Pr);')
    part=part.replace('Viewport.Polyline(Pts);','PilotFlush(2,Viewport.PenColor);\n      Viewport.Polyline(Pts);')
    s=s[:a]+part+s[b:]
    a=s.index('  procedure DrawTriangles(');b=s.index('  procedure PrepareFillAtlas;',a)
    part=s[a:b];part=replace_once(part,'      if VectorRecording then Result:=',
        '      PilotPoint(P.X,P.Y);\n      if VectorRecording then Result:=')
    part=part.replace('if VectorRecording then FFillVector.Polygon(Pts,Col)',
        'PilotFlush(1,Col);\n        if VectorRecording then FFillVector.Polygon(Pts,Col)')
    s=s[:a]+part+s[b:]
    # Diagonal curves use a separate path outside DrawSpline.
    a=s.index('    // Draw diagonals',s.index('  PaintStarted :='));b=s.index('    DrawPendingLabels;',a)
    part=s[a:b].replace('Pts[K] := Viewport.Project(P);','PilotPoint(P.X,P.Y);\n          Pts[K] := Viewport.Project(P);')
    part=replace_once(part,'        Viewport.Polyline(Pts);','        PilotFlush(2,Viewport.PenColor);\n        Viewport.Polyline(Pts);')
    s=s[:a]+part+s[b:]
    s=replace_once(s,'  var Placement: TLabelPlacement; Kind: TLabelKind;',
        '  var Placement: TLabelPlacement; Kind: TLabelKind; Origin:TPoint;')
    s=replace_once(s,'    Inc(LabelIndex);',
        "    Origin:=Viewport.Project(ZERO);\n    if PilotActive then PilotLabel((X-Origin.X)/(Viewport.Scale*Viewport.Zoom),\n      (Origin.Y-Y)/(Viewport.Scale*Viewport.Zoom),Viewport.FontColor,Viewport.TextHeight('Mg'),Text);\n    Inc(LabelIndex);")
    a=s.index('procedure TFreeLinesplanFrame.ExportDXFExecute');part=s[a:]
    part=replace_once(part,'  if SaveDialog.Execute then',
        "  SaveDialog.FileName:=ParamStr(2)+'/reference.dxf';\n  if True then")
    s=s[:a]+part;path.write_text(s,encoding='utf8')

def adapt_scene(work):
    origin=json.loads((PILOT/'bridge/scene-origin.json').read_text('utf8'))
    for name,sha in origin.items():
        if common.digest_file(work/name)!=sha:raise RuntimeError('Scene extraction is stale: '+name)
    for name in ('FreeLinesplanScene.pas','PilotCapture.pas'):
        shutil.copy2(PILOT/'bridge'/name,work/'Units'/name)
    # This process has no editing UI. Avoid constructing the two eager dialogs
    # in the legacy model constructor (including LoadProject's candidate model).
    # All numerical initialization and the transactional loader stay intact.
    path=work/'Units/FreeShip.inc';s=path.read_text('utf-8-sig')
    s=replace_once(s,'  if not (csDesigning in ComponentState) then\n    FIntersectionDialog := TFreeIntersectionDialog.Create(self);',
        '  FIntersectionDialog := nil; // scene-only bridge')
    s=replace_once(s,'  if not (csDesigning in ComponentState) then\n  begin\n    FControlpointForm := TFreeControlPointForm.Create(Self);\n    FControlpointForm.FreeShip := self;\n  end;',
        '  FControlpointForm := nil; // scene-only bridge')
    path.write_text(s,encoding='utf8')

    # Errors must reach QProcess, not a modal exception window in a hidden child.
    path=work/'Units/FreeLogger.pas';s=path.read_text('utf-8-sig')
    s=replace_once(s,'  ExceptionDlg := TExceptionDlg.Create(nil);','  ExceptionDlg := nil; // scene-only bridge')
    s=replace_once(s,'  if ExceptionDlg<>nil then','  if ExceptionDlg=nil then raise Exception.Create(Report);\n  if ExceptionDlg<>nil then')
    path.write_text(s,encoding='utf8')


def build(source,out,reference=False):
    out.mkdir(parents=True,exist_ok=False)
    before=common.fingerprint(source)[0]
    report={'source_sha256':before,'status':'error','adapter_sha256':adapter_hashes(),'reference':reference}
    try:
        with common.temporary_workspace('fs-qtbridge-') as td:
            work=Path(td)
            report['build']=native.build(source,work,common.DEFAULT_LAZARUS,out,'current','standalone',
                probe=TESTS/'QtLegacyBridge.pas' if reference else PILOT/'bridge/PilotBridge.pas',
                prepare=adapt if reference else adapt_scene)
            shutil.copy2(work/'NativeLinesplanProbe.exe',out/'FreeShipGeometryBridge.exe')
            # Rebuildable baseline, including original licenses and dirty-tree files.
            with zipfile.ZipFile(out/'legacy-source.zip','w',zipfile.ZIP_DEFLATED) as z:
                for f in common.source_files(source):z.write(f,f.relative_to(source))
                for name in ('COPYING','README.txt','Whatsnew.txt'):
                    if (source/name).exists():z.write(source/name,name)
            for name in ('locale','Themes','Languages'):shutil.copytree(source/name,out/name)
            (out/'source-id.txt').write_text(before,encoding='ascii')
        if common.fingerprint(source)[0]!=before:raise RuntimeError('Source changed during snapshot/build')
        report['status']='pass'
    except Exception as e:report['error']=str(e)
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    return report

def adapter_hashes():
    files=[PILOT/'bridge'/n for n in ('build_bridge.py','PilotBridge.pas','PilotCapture.pas','FreeLinesplanScene.pas','scene-origin.json','extract_scene.py','SceneHeader.inc')]
    files += [TESTS/'native_flicker_test.py',TESTS/'freeship_ui_test.py',TESTS/'QtLegacyBridge.pas']
    return {str(f.relative_to(PILOT.parent)):common.digest_file(f) for f in files}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE);p.add_argument('--output',type=Path,required=True);p.add_argument('--reference',action='store_true');a=p.parse_args()
    r=build(a.source.resolve(),a.output.resolve(),a.reference);print(json.dumps(r));sys.exit(0 if r['status']=='pass' else 1)
