unit UiModels;
{$mode objfpc}{$H+}
interface
uses Main, fpjson;
procedure RunModels(Form:TMainForm; Root:TJSONObject; Cases:TJSONArray; const OutDir:string);
implementation
uses SysUtils, Controls, Graphics, FreeGeometry, UiTrace;
procedure Add(Cases:TJSONArray; const Id,Status,Expected,Actual:string);
var O:TJSONObject;
begin O:=TJSONObject.Create; O.Add('id',Id); O.Add('status',Status);
  O.Add('expected',Expected); O.Add('actual',Actual); Cases.Add(O) end;
procedure RunModels(Form:TMainForm; Root:TJSONObject; Cases:TJSONArray; const OutDir:string);
const Names:array[0..3] of string=('bodyplan','profile','plan','perspective');
      Kinds:array[0..3] of TFreeViewType=(fvBodyplan,fvProfile,fvPlan,fvPerspective);
var Path,Id:string; I,J,Count,VisibleCount,ExpectedCount,Wrong:Integer;
    V:TFreeViewport; B:TBitmap; P:TFreeSubdivisionControlPoint;
    Trace,Placement,Leaders,Segments:TJSONArray; O:TJSONObject; Seen:array of Boolean;
begin
  Path:=SysUtils.GetEnvironmentVariable('FS_UI_PROBE_MODEL'); Form.FormShow(Form);
  if not FileExists(Path) then raise Exception.Create('Copied model missing');
  Root.Add('model_load_started',True); Form.LoadNamedFile(Path); Root.Add('model_load_completed',True);
  if not Form.FreeShip.ModelIsLoaded or (Form.MDIChildCount<4) then
    raise Exception.Create('Model did not load with four viewports');
  for I:=0 to Form.MDIChildCount-1 do
    if Form.GetMDIChildren(I).Viewport.ProgressivePreview then
      Form.GetMDIChildren(I).Viewport.FinishProgressiveDisplay(True);
  Count:=Form.FreeShip.Surface.NumberOfControlPoints;
  Root.Add('model_points',Count); Root.Add('model_layers',Form.FreeShip.NumberOfLayers);
  if Count=0 then begin Add(Cases,'model.loaded','fail','model contains control points','zero control points'); Exit end;
  Add(Cases,'model.loaded','pass','model contains control points and four views',IntToStr(Count));
  if Count>300 then begin
    Form.FreeShip.Visibility.ShowControlNet:=False;
    B:=TBitmap.Create;
    try
      B.SetSize(800,500);
      for I:=0 to 3 do begin
        V:=Form.GetMDIChildren(I).Viewport; V.Align:=alNone; V.SetBounds(0,0,800,500);
        V.ViewType:=Kinds[I]; V.ViewportMode:=vmWireframe; V.ControlPointCoordinateAxis:=0;
        V.InitializeViewport(Form.FreeShip.Surface.Min,Form.FreeShip.Surface.Max);
        B.Canvas.Brush.Color:=clWhite; B.Canvas.FillRect(0,0,800,500); V.DrawingCanvas:=B.Canvas;
        Form.FreeShip.DrawToViewport(V);
        Add(Cases,'model.dense.'+Names[I],'pass','dense model draws offscreen without coordinate labels','render completed');
        Cases.Objects[Cases.Count-1].Add('image',Names[I]+'.bmp');
        B.SaveToFile(IncludeTrailingPathDelimiter(OutDir)+Names[I]+'.bmp'); V.DrawingCanvas:=V.Canvas;
      end;
    finally B.Free end;
    Add(Cases,'model.render-limit','not_run','render coordinate labels',
      'more than 300 points; coordinate labels were not tested');
    Add(Cases,'model.visual','not_run','physical screen and label legibility','offscreen images only'); Exit;
  end;
  Form.FreeShip.Visibility.ShowControlNet:=True;
  B:=TBitmap.Create;
  try
    B.SetSize(800,500);
    for I:=0 to 3 do begin
      V:=Form.GetMDIChildren(I).Viewport; V.Align:=alNone; V.SetBounds(0,0,800,500);
      V.ViewType:=Kinds[I]; V.ViewportMode:=vmWireframe; V.ControlPointCoordinateAxis:=1;
      V.InitializeViewport(Form.FreeShip.Surface.Min,Form.FreeShip.Surface.Max);
      ExpectedCount:=0;
      for J:=0 to Count-1 do if Form.FreeShip.Surface.ControlPoint[J].Visible then Inc(ExpectedCount);
      B.Canvas.Brush.Color:=clWhite; B.Canvas.FillRect(0,0,800,500); V.DrawingCanvas:=B.Canvas;
      BeginTextTrace; Form.FreeShip.DrawToViewport(V);
      Trace:=EndTextTrace; Placement:=EndPlacementTrace; Leaders:=EndLeaderTrace; Segments:=EndSegmentTrace;
      SetLength(Seen,Count); for J:=0 to Count-1 do Seen[J]:=False;
      VisibleCount:=0; Wrong:=0; Id:='model.view.'+Names[I];
      for J:=0 to Placement.Count-1 do if Placement.Objects[J].Get('category','')='point' then begin
        Inc(VisibleCount);
        if (Placement.Objects[J].Get('point_index',-1)<0) or
           (Placement.Objects[J].Get('point_index',-1)>=Count) then Inc(Wrong)
        else begin
          P:=Form.FreeShip.Surface.ControlPoint[Placement.Objects[J].Get('point_index',-1)];
          if Seen[Placement.Objects[J].Get('point_index',-1)] or
            (Placement.Objects[J].Get('text','')<>FormatFloat('0.000',P.Coordinate.X)) then Inc(Wrong);
          Seen[Placement.Objects[J].Get('point_index',-1)]:=True;
        end;
      end;
      if (VisibleCount=ExpectedCount) and (Wrong=0) and (ExpectedCount>0) then
        Add(Cases,Id,'pass','each visible control point requests its X value',Format('visible=%d; requests=%d; wrong=%d',[ExpectedCount,VisibleCount,Wrong]))
      else Add(Cases,Id,'fail','each visible control point requests its X value',Format('visible=%d; requests=%d; wrong=%d',[ExpectedCount,VisibleCount,Wrong]));
      O:=Cases.Objects[Cases.Count-1]; O.Add('text_trace',Trace); O.Add('placement_trace',Placement);
      O.Add('leader_trace',Leaders); O.Add('segment_trace',Segments);
      O.Add('image',Names[I]+'.bmp'); B.SaveToFile(IncludeTrailingPathDelimiter(OutDir)+Names[I]+'.bmp');
      V.DrawingCanvas:=V.Canvas;
    end;
  finally B.Free end;
  Add(Cases,'model.visual','not_run','physical screen and label legibility','offscreen images and calls only');
end;
end.
