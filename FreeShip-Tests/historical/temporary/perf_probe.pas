program perf_probe;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces, Forms, SysUtils, Graphics, FreeTypes, FreeGeometry, FreeShipUnit, Main;
var s:TFreeShip;v:TFreeViewport;b:TBitmap;t:qword;i,a:integer;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);s:=MainForm.FreeShip;
 t:=GetTickCount64;s.Edit.File_Load(GetTempDir+'FreeShipPathCheck\current-validation\验证-Женя\输入-Женя.fbm');WriteLn('load_ms=',GetTickCount64-t,' loaded=',s.ModelIsLoaded,' points=',s.Surface.NumberOfControlPoints,' edges=',s.Surface.NumberOfControlEdges);
 t:=GetTickCount64;s.RebuildModel;WriteLn('rebuild_ms=',GetTickCount64-t);
 v:=TFreeViewport.Create(nil);b:=TBitmap.Create;try
 v.SetBounds(0,0,800,500);v.DestinationWidth:=800;v.DestinationHeight:=500;b.SetSize(800,500);v.DrawingCanvas:=b.Canvas;v.ViewportMode:=vmWireframe;s.Visibility.ShowGrid:=true;s.Visibility.ShowControlNet:=true;
 for a:=0 to 1 do begin t:=GetTickCount64;for i:=0 to 3 do begin v.ViewType:=TFreeViewType(i);v.InitializeViewport(s.Surface.Min,s.Surface.Max);if a=0 then v.ControlPointCoordinateAxis:=0 else v.ControlPointCoordinateAxis:=2;s.DrawToViewport(v) end;WriteLn('four_draws_axis_',a,'_ms=',GetTickCount64-t) end;
 finally b.Free;v.Free;MainForm.Free end;
end.
