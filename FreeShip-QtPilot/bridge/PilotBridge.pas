program PilotBridge;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Classes,Windows,FreeShipUnit,FreeFileBuffer,
 FreeLinesplanScene,FreeGeometry,FreeTypes,FreeSafeLanguage;
var S:TFreeShip;B:TFreeFileBuffer;Scene:TLinesplanScene;Start,NowTick,Freq:Int64;
 Info:TStringList;N:Integer;P:TFreeSubdivisionControlPoint;C:T3DCoordinate;
procedure Mark(const Name:string);
begin QueryPerformanceCounter(NowTick);Info.Add(Name+'='+FloatToStr(1000*(NowTick-Start)/Freq));QueryPerformanceCounter(Start) end;
begin
 try
  DefaultFormatSettings.DecimalSeparator:='.';QueryPerformanceFrequency(Freq);QueryPerformanceCounter(Start);
  Info:=TStringList.Create;Application.Initialize;S:=TFreeShip.Create(nil);GlobalFreeShip:=S;
  // Match the original bridge's isolated preferences/theme without a main form.
  S.Preferences.Load;
  ApplyLanguageSafely('cn',ExtractFilePath(ParamStr(0))+'locale');
  B:=TFreeFileBuffer.Create;
  try B.LoadFromFile(ParamStr(1));S.LoadProject(B) finally B.Free end;
  S.RebuildModel;
  if ParamStr(3)='move-point' then begin
   P:=nil;for N:=0 to S.Surface.NumberOfControlPoints-1 do
    if not S.Surface.ControlPoint[N].Locked then begin P:=S.Surface.ControlPoint[N];Break end;
   if P=nil then raise Exception.Create('No unlocked control point');
   C:=P.Coordinate;C.Z:=C.Z+0.01;P.Coordinate:=C;S.Built:=False;S.RebuildModel;
  end;
  Mark('load_rebuild_ms');
  Scene:=TLinesplanScene.Create(S);
  Scene.Generate(ParamStr(2)+'/scene.fslp');Mark('scene_generate_ms');
  Scene.ExportDXF(ParamStr(2)+'/reference.dxf');Mark('dxf_generate_ms');
  Info.Add('forms_created='+IntToStr(Screen.FormCount));
  Info.Add('viewports_created='+IntToStr(S.NumberOfViewports));
  if Screen.FormCount<>0 then begin
   for N:=0 to Screen.FormCount-1 do WriteLn('Unexpected form: '+Screen.Forms[N].ClassName);
   raise Exception.Create('Scene generator created a form');
  end;
  Info.SaveToFile(ParamStr(2)+'/bridge-timing.txt');Scene.Free;S.Free;Info.Free;
  Halt(0);
 except on E:Exception do begin WriteLn(E.ClassName+': '+E.Message);Halt(1) end end;
end.
