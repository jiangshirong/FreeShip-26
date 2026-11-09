unit UiLabels;
{$mode objfpc}{$H+}
interface
uses Main, fpjson;
procedure RunLabels(Form: TMainForm; const Scope, OutDir: string; Cases: TJSONArray);
implementation
uses SysUtils, Types, Controls, Graphics, FreeTypes, FreeGeometry, FreeLinesplanFrme, UiTrace;

procedure AddTrace(Cases: TJSONArray; const Id, Path: string; W,H: Integer; Img: TBitmap; Settled:Boolean);
var O:TJSONObject;
begin
  Img.SaveToFile(Path);
  O:=TJSONObject.Create; O.Add('id',Id); O.Add('status','pass');
  O.Add('expected','offscreen render completed');
  O.Add('actual','recorded drawing calls and placement decisions');
  O.Add('width',W); O.Add('height',H); O.Add('image',ExtractFileName(Path)); O.Add('layout_settled',Settled);
  O.Add('text_trace',EndTextTrace); O.Add('placement_trace',EndPlacementTrace);
  O.Add('leader_trace',EndLeaderTrace); Cases.Add(O);
  O.Add('segment_trace',EndSegmentTrace);
end;

procedure RunLabels(Form: TMainForm; const Scope, OutDir: string; Cases: TJSONArray);
const Names:array[0..2] of string=('profile','plan','bodyplan');
      Types:array[0..2] of TFreeViewType=(fvProfile,fvPlan,fvBodyplan);
      Languages:array[0..1] of string=('Chinese','Russian');
      Widths:array[0..1] of Integer=(800,480);
      Heights:array[0..1] of Integer=(500,300);
var I,J,K,Attempt:Integer; Settled:Boolean; V:TFreeViewport; B:TBitmap; Frame:TFreeLinesplanFrame;
    Min3D,Max3D:T3DCoordinate; Id,Path:string;
begin
  if (Scope<>'all') and (Scope<>'labels') then Exit;
  // Add distinct section positions to the isolated copy of the box fixture.
  for I:=1 to 6 do Form.FreeShip.Edit.Intersection_Add(fiStation,I*3);
  for I:=1 to 3 do Form.FreeShip.Edit.Intersection_Add(fiWaterline,I*2);
  Form.FreeShip.Edit.Intersection_Add(fiButtock,2);
  if Form.FreeShip.NumberOfDiagonals=0 then Form.FreeShip.Edit.Intersection_Add(fiDiagonal,5);
  Form.FreeShip.ProjectSettings.ProjectDraft:=4;
  Form.FreeShip.Visibility.ShowStations:=True; Form.FreeShip.Visibility.ShowWaterlines:=True;
  Form.FreeShip.Visibility.ShowButtocks:=True; Form.FreeShip.Visibility.ShowDiagonals:=True;
  Form.FreeShip.Visibility.ShowGrid:=True; Form.FreeShip.Visibility.ShowControlNet:=True;
  Form.FreeShip.Visibility.ShowHydrostaticData:=True;
  V:=Form.GetMDIChildren(0).Viewport;
  V.ViewportMode:=vmWireframe; V.ControlPointCoordinateAxis:=1;
  B:=TBitmap.Create;
  try
    for I:=0 to 1 do begin
      if not Form.FreeShip.Preferences.TrySetInterfaceLanguage(Languages[I]) then
        raise Exception.Create('Missing language for label test: '+Languages[I]);
      Form.RefreshStudioLanguage;
      for J:=0 to 1 do begin
        V.Align:=alNone; V.SetBounds(0,0,Widths[J],Heights[J]);
        B.SetSize(Widths[J],Heights[J]);
        for K:=0 to 2 do begin
          V.ViewType:=Types[K]; V.InitializeViewport(Form.FreeShip.Surface.Min,Form.FreeShip.Surface.Max);
          V.DrawingCanvas:=B.Canvas; Settled:=False;
          for Attempt:=1 to 30 do begin
            B.Canvas.Brush.Color:=clWhite; B.Canvas.FillRect(0,0,B.Width,B.Height);
            BeginTextTrace; Form.FreeShip.DrawToViewport(V);
            Settled:=not V.LabelLayout.Animating;
            if Settled or (Attempt=30) then Break;
            Sleep(20);
          end;
          Id:='labels.hull.'+LowerCase(Languages[I])+'.'+Names[K]+'.'+IntToStr(Widths[J]);
          Path:=IncludeTrailingPathDelimiter(OutDir)+StringReplace(Id,'.','-',[rfReplaceAll])+'.bmp';
          AddTrace(Cases,Id,Path,B.Width,B.Height,B,Settled);
        end;
        V.DrawingCanvas:=V.Canvas;
      end;
    end;
    V:=Form.GetMDIChildren(0).Viewport;
    V.Align:=alNone;V.SetBounds(0,0,800,500);B.SetSize(800,500);
    V.ViewType:=fvProfile;V.Pan:=Point(0,0);
    V.InitializeViewport(Form.FreeShip.Surface.Min,Form.FreeShip.Surface.Max);
    V.DrawingCanvas:=B.Canvas;
    for Attempt:=0 to 60 do begin
      if Attempt<30 then V.Pan:=Point(80-Attempt*8,0)
      else V.Pan:=Point(80+(Attempt-60)*8,0);
      Sleep(16);
      B.Canvas.Brush.Color:=clWhite;B.Canvas.FillRect(0,0,B.Width,B.Height);
      BeginTextTrace;Form.FreeShip.DrawToViewport(V);
      Id:='labels.pan.profile.'+IntToStr(Attempt);
      Path:=IncludeTrailingPathDelimiter(OutDir)+StringReplace(Id,'.','-',[rfReplaceAll])+'.bmp';
      AddTrace(Cases,Id,Path,800,500,B,not V.LabelLayout.Animating);
    end;
    V.DrawingCanvas:=V.Canvas;
    Frame:=TFreeLinesplanFrame.Create(nil);
    try
      Frame.FreeShip:=Form.FreeShip; Frame.ShowFillColor.Checked:=False;
      Frame.ShowMonochrome.Checked:=True; Frame.FontSize:=10;
      V:=Frame.Viewport; V.Align:=alNone;
      for I:=0 to 1 do begin
        if not Form.FreeShip.Preferences.TrySetInterfaceLanguage(Languages[I]) then
          raise Exception.Create('Missing language for lines plan: '+Languages[I]);
        Form.RefreshStudioLanguage;
        for J:=0 to 1 do begin
          Frame.SetBounds(0,0,Widths[J],Heights[J]); V.SetBounds(0,0,Widths[J],Heights[J]);
          B.SetSize(Widths[J],Heights[J]);
          Frame.ViewportRequestExtents(V,Min3D,Max3D); V.InitializeViewport(Min3D,Max3D);
          V.DrawingCanvas:=B.Canvas; Settled:=False;
          for Attempt:=1 to 30 do begin
            B.Canvas.Brush.Color:=clWhite; B.Canvas.FillRect(0,0,B.Width,B.Height);
            BeginTextTrace; Frame.ViewportRedraw(V);
            Settled:=not V.LabelLayout.Animating;
            if Settled or (Attempt=30) then Break;
            Sleep(20);
          end;
          Id:='labels.linesplan.'+LowerCase(Languages[I])+'.'+IntToStr(Widths[J]);
          Path:=IncludeTrailingPathDelimiter(OutDir)+StringReplace(Id,'.','-',[rfReplaceAll])+'.bmp';
          AddTrace(Cases,Id,Path,B.Width,B.Height,B,Settled);
        end;
      end;
      V.DrawingCanvas:=V.Canvas; Frame.FreeShip:=nil;
    finally Frame.Free end;
  finally B.Free end;
end;
end.
