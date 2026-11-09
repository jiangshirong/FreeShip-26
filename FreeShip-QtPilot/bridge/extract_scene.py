"""Reproduce the reviewed window-free extraction; does not edit the product tree.

Arithmetic/projection/sort/export bodies are retained from their current source.
Raster, cache, UI and collision-layout operations are removed explicitly.
The generated Pascal is checked in beside this recipe for normal compilation.
"""
from pathlib import Path
import hashlib,json,re,tempfile,shutil
import build_bridge

ROOT=Path(__file__).resolve().parent
SOURCE=ROOT.parent.parent/'FreeShip-Plus-Lazarus'

def cut(s,a,b):return s[s.index(a):s.index(b,s.index(a))]
def remove(s,a,b):return s[:s.index(a)]+s[s.index(b,s.index(a)):]

def extract():
    raw=(SOURCE/'Forms/FreeLinesplanFrme.pas').read_text('utf-8-sig')
    vp=(SOURCE/'Units/FreeViewport.inc').read_text('utf-8-sig')
    with tempfile.TemporaryDirectory(prefix='fs-scene-extract-') as t:
        w=Path(t);(w/'Forms').mkdir();(w/'Units').mkdir();(w/'Forms/FreeLinesplanFrme.pas').write_text(raw,encoding='utf8')
        build_bridge.adapt(w)
        observed=(w/'Forms/FreeLinesplanFrme.pas').read_text('utf8')
    ext=cut(raw,'procedure TFreeLinesplanFrame.ViewportRequestExtents','procedure TFreeLinesplanFrame.SpinEdit1Change')
    ext=ext.replace('TFreeLinesplanFrame.ViewportRequestExtents(Sender: TObject;','TLinesplanScene.CalculateExtents(')
    drawing=cut(observed,'procedure TFreeLinesplanFrame.ViewportRedraw','procedure TFreeLinesplanFrame.ZoomExtentsExecute')
    drawing=drawing.replace('TFreeLinesplanFrame.ViewportRedraw(Sender: TObject)','TLinesplanScene.EmitScene')
    a=drawing.index('  procedure DrawLineLabel(');b=drawing.index('  procedure PlaceLineLabel(',a)
    drawing=drawing[:a]+'''  procedure DrawLineLabel(X,Y:Integer;const Text:string;Vertical:Boolean;Outward:Integer);
  var Origin:TPoint;
  begin
    Origin:=Layout.Project(ZERO);
    PilotLabel((X-Origin.X)/(Layout.Scale*Layout.Zoom),
      (Origin.Y-Y)/(Layout.Scale*Layout.Zoom),Layout.Font.Color,Layout.TextHeight('Mg'),Text);
  end;

'''+drawing[b:]
    drawing=remove(drawing,'  // Register the projected feature edges','  procedure DrawLineAtt(')
    drawing=remove(drawing,'  procedure QueueKnuckle(','  procedure DrawLine(P1')
    drawing=remove(drawing,'  {$ifdef NOUSE}','  {$endif NOUSE}').replace('  {$endif NOUSE}','',1)
    # No view-dependent tessellation changes: retain original spline values.
    a=drawing.index('  procedure DrawTriangles(');b=drawing.index('  procedure PrepareFillAtlas;',a)
    part=drawing[a:b]
    a1=part.index('    procedure SetFillColor(');b1=part.index('    function ProjectFillPoint(',a1)
    part=part[:a1]+'''    procedure SetFillColor(Color:TColor);
    begin Col:=Color end;

'''+part[b1:]
    a2=part.index('      if VectorRecording then Result:=');b2=part.index('    end;',a2)
    part=part[:a2]+'      Result:=Types.Point(0,0);\n'+part[b2:]
    part=re.sub(r'\s*if VectorRecording then FFillVector.Polygon\(Pts,Col\)\s*else Viewport.Polygon\(Pts\);','',part)
    drawing=drawing[:a]+part+drawing[b:]
    drawing=remove(drawing,'  procedure PrepareFillAtlas;','begin\n  if Freeship = nil then')
    a=drawing.index('  PaintStarted :=');b=drawing.index('  WlPlane.a :=',a)
    drawing=drawing[:a]+'  Steps:=200;\n'+drawing[b:]
    a=drawing.index('  Viewport.BrushStyle := bsSolid;');b=drawing.index('      for I := 1 to Freeship.Surface.NumberOfLayers',a)
    drawing=drawing[:a]+'''  Below.Count:=0;Below.Capacity:=0;Above.Count:=0;Above.Capacity:=0;
  Layout.PenColor:=clSilver;Layout.Font.Name:='Arial';Layout.Font.Color:=clBlack;
  Layout.Font.Size:=7;
'''+drawing[b:]
    a=drawing.index('      if VectorRecording then begin');b=drawing.index('    // Keep fixed-width strokes',a)
    drawing=drawing[:a]+drawing[b:]
    a=drawing.index('    if not Viewport.Printing and not Viewport.ExportingBitmap then',drawing.index('    DrawPendingLabels;'))
    drawing=drawing[:a]+'end;\n\n'
    drawing=drawing.replace('      PrepareFillAtlas;','').replace('DrawFillTriangles(', 'DrawTriangles(')
    drawing=drawing.replace('    CollectFeatureEdges;','').replace('    FlushKnuckles;','')
    drawing=re.sub(r'^\s*QueueKnuckle\(Pts\[0\],Pts\[1\]\);\s*$', '',drawing,flags=re.M)
    drawing=build_bridge.replace_once(drawing,'      if ShowFillColor.Checked then\n        Viewport.SetPenWidth(1 * PenwidthFactor)\n      else\n        Viewport.SetPenWidth(2 * PenwidthFactor);','')
    drawing=re.sub(r'^\s*Viewport\.(Polyline|MoveTo|LineTo|SetPenWidth)\([^\n]*\);\s*$', '',drawing,flags=re.M)
    drawing=re.sub(r'^\s*Viewport\.(BrushStyle|PenWidth|PenStyle)\s*:=.*?;\s*$', '',drawing,flags=re.M)
    drawing=drawing.replace('    if Viewport.Printing then begin Viewport.TextOut(X,Y,Text);Exit end;','')
    # Remove raster/cache-only locals, not numerical geometry variables.
    a=drawing.index('  Prevcursor: TCursor;');b=drawing.index('  PendingLabels:',a)
    drawing=drawing[:a]+'''  SubmColor:TColor;
  Pn,Pn1,Pn2:Integer;
'''+drawing[b:]
    drawing=drawing.replace(';LabelIndex:=PendingLabels[N].Index-1;',';')
    # Direct world-coordinate line/triangle capture needs no integer projection.
    drawing=re.sub(r'^\s*Pts\[i\] := Viewport.Project\(Pr\);\s*$', '',drawing,flags=re.M)
    drawing=drawing.replace('          Pts[K] := Viewport.Project(P);','')
    drawing=drawing.replace('Viewport.', 'Layout.')
    for name in ('FontName','FontSize','FontColor'):
        drawing=drawing.replace('Layout.'+name,'Layout.Font.'+{'FontName':'Name','FontSize':'Size','FontColor':'Color'}[name])
    # Original action flags become explicit scene configuration, not controls.
    for name,value in (('ShowMonochrome','False'),('ShowFillColor','True'),('UseLights','True'),('MirrorPlanview','False')):
        drawing=re.sub(name+r'\.Checked',value,drawing,flags=re.I)
        ext=re.sub(name+r'\.Checked',value,ext,flags=re.I)
    dxf=cut(raw,'procedure TFreeLinesplanFrame.ExportDXFExecute','\nend.\n')
    dxf=dxf.replace('TFreeLinesplanFrame.ExportDXFExecute(Sender: TObject)','TLinesplanScene.ExportDXF(const Filename:string)')
    dxf=dxf.replace('  SaveDialog: TSaveDialog;','')
    a=dxf.index('  FinishInteraction(nil);');b=dxf.index('    SplitSectionLocation :=',a)
    dxf=dxf[:a]+dxf[b:]
    dxf=dxf.replace("Strings.SaveToFile(ChangeFileExt(SaveDialog.FileName, '.dxf'));",'Strings.SaveToFile(Filename);')
    dxf=build_bridge.replace_once(dxf,'  end;\n  FreeAndNil(SaveDialog);\nend;','end;')
    dxf=re.sub(r'MirrorPlanView\.Checked','False',dxf,flags=re.I)
    init=cut(vp,'procedure TFreeViewport.InitializeViewport','procedure TFreeViewport.Print(')
    init=init.replace('TFreeViewport.InitializeViewport','TSceneProjection.Initialize')
    init=remove(init,'  if FViewtype = fvPerspective then','  FCameraLocation.X :=')
    init=init.replace('  FCameraLocation.X :=','  FDistance:=1e8;\n  FCameraLocation.X :=',1)
    init=remove(init,'  FLightIntensity :=','  FCosAngle :=')
    init=remove(init,'  if assigned(BackgroundImage) then','end;{TSceneProjection.Initialize}')
    proj=cut(vp,'function TFreeViewport.Project(P:','function TFreeViewport.ProjectBackAsLine').replace('TFreeViewport.Project','TSceneProjection.Project')
    rot=cut(vp,'function TFreeViewport.RotatedPoint(P:','function TFreeViewport.RotatedPointBack').replace('TFreeViewport.RotatedPoint','TSceneProjection.RotatedPoint')
    header=raw[:raw.index('unit FreeLinesplanFrme;')]
    text=header+(ROOT/'SceneHeader.inc').read_text('utf8')+'\n'+init+proj+rot+ext+drawing+dxf+'\nend.\n'
    for invalid in ('Viewport.','DrawingCanvas','TFreeLinesplanForm','TMainForm','Screen.Cursor','PrepareFrame','TSaveDialog'):
        if invalid in text.replace('TFreeViewport.',''):raise RuntimeError('Raster/UI dependency survived: '+invalid)
    (ROOT/'FreeLinesplanScene.pas').write_text(text,encoding='utf8')
    inputs=['Forms/FreeLinesplanFrme.pas','Units/FreeViewport.inc','Units/FreeTypes.pas']
    (ROOT/'scene-origin.json').write_text(json.dumps({n:hashlib.sha256((SOURCE/n).read_bytes()).hexdigest() for n in inputs},indent=2),encoding='utf8')
    print('Extracted',len(text.splitlines()),'lines; no UI instance or raster drawing entry')

if __name__=='__main__':extract()
