program PilotBridge;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Controls,SysUtils,Classes,Windows,Main,FreeShipUnit,FreeFileBuffer,
 FreeLinesplanFrm,FreeLinesplanFrme,FreeGeometry,FreeTypes,PilotCapture,FreeSafeLanguage;
var S:TFreeShip;B:TFreeFileBuffer;L:TFreeLinesplanForm;Start,NowTick,Freq:Int64;
 MinP,MaxP:T3DCoordinate;Info:TStringList;N:Integer;P:TFreeSubdivisionControlPoint;C:T3DCoordinate;
procedure Mark(const Name:string);
begin QueryPerformanceCounter(NowTick);Info.Add(Name+'='+FloatToStr(1000*(NowTick-Start)/Freq));QueryPerformanceCounter(Start) end;
begin
 try
  DefaultFormatSettings.DecimalSeparator:='.';QueryPerformanceFrequency(Freq);QueryPerformanceCounter(Start);
  Info:=TStringList.Create;Application.Initialize;Application.CreateForm(TMainForm,MainForm);S:=MainForm.FreeShip;
  ApplyLanguageSafely('cn',ExtractFilePath(ParamStr(0))+'locale');
  B:=TFreeFileBuffer.Create;B.LoadFromFile(ParamStr(1));S.LoadProject(B);B.Free;S.RebuildModel;
  if ParamStr(3)='move-point' then begin
   P:=nil;for N:=0 to S.Surface.NumberOfControlPoints-1 do
    if not S.Surface.ControlPoint[N].Locked then begin P:=S.Surface.ControlPoint[N];Break end;
   if P=nil then raise Exception.Create('No unlocked control point');
   C:=P.Coordinate;C.Z:=C.Z+0.01;P.Coordinate:=C;S.Built:=False;S.RebuildModel;
  end;
  Mark('load_rebuild_ms');
  L:=TFreeLinesplanForm.Create(nil);L.ClientWidth:=1280;L.ClientHeight:=830;
  L.LinesplanFrame.FreeShip:=S;
  L.LinesplanFrame.Align:=alNone;L.LinesplanFrame.SetBounds(0,0,1280,830);
  L.Viewport.Align:=alNone;L.Viewport.SetBounds(0,30,1284,804);
  L.PrepareForDisplay;Mark('legacy_prepare_ms');
  Info.Add('viewport_width='+IntToStr(L.Viewport.ClientWidth));Info.Add('viewport_height='+IntToStr(L.Viewport.ClientHeight));
  L.Viewport.GetDrawingBuffer.SaveToFile(ParamStr(2)+'/legacy.bmp');
  MinP:=L.Viewport.Min3D;MaxP:=L.Viewport.Max3D;
  L.LinesplanFrame.InvalidateRenderCache;
  PilotBegin(ParamStr(2)+'/scene.fslp',MinP.X,MinP.Y,MaxP.X,MaxP.Y);
  L.LinesplanFrame.ViewportRedraw(L.Viewport);PilotEnd;Mark('capture_redraw_ms');
  L.Viewport.GetDrawingBuffer.SaveToFile(ParamStr(2)+'/captured.bmp');
  L.LinesplanFrame.ExportDXFExecute(nil);Mark('legacy_dxf_ms');
  Info.SaveToFile(ParamStr(2)+'/bridge-timing.txt');Info.Free;
  Halt(0);
 except on E:Exception do begin WriteLn(E.ClassName+': '+E.Message);Halt(1) end end;
end.
