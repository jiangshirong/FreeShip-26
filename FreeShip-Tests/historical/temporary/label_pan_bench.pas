program label_pan_bench;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,Graphics,Types,FreeTypes,FreeGeometry,FreeShipUnit,Main;
var s:TFreeShip;v:TFreeViewport;b:TBitmap;t,total:QWord;i,a,k:integer;times:array[0..19]of QWord;j:integer;tmp:QWord;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);s:=MainForm.FreeShip;
 s.Edit.File_Load(GetTempDir+'FreeShipPathCheck\current-validation\验证-Женя\输入-Женя.fbm');s.RebuildModel;
 WriteLn('points=',s.Surface.NumberOfControlPoints,' edges=',s.Surface.NumberOfControlEdges);
 b:=TBitmap.Create;b.SetSize(1000,600);
 for k:=0 to 1 do for a:=0 to 1 do begin
 v:=TFreeViewport.Create(nil);v.SetBounds(0,0,1000,600);v.DestinationWidth:=1000;v.DestinationHeight:=600;v.DrawingCanvas:=b.Canvas;
 v.ViewportMode:=vmWireframe;if k=0 then v.ViewType:=fvProfile else v.ViewType:=fvBodyplan;
 if a=0 then v.ControlPointCoordinateAxis:=0 else v.ControlPointCoordinateAxis:=2;
 s.Visibility.ShowControlNet:=true;s.Visibility.ShowGrid:=true;v.InitializeViewport(s.Surface.Min,s.Surface.Max);
 for i:=0 to 3 do s.DrawToViewport(v);
 total:=0;
 for i:=0 to 19 do begin v.Pan:=Point(v.Pan.X+2,v.Pan.Y+1);t:=GetTickCount64;s.DrawToViewport(v);times[i]:=GetTickCount64-t;inc(total,times[i]);end;
 for i:=0 to 18 do for j:=i+1 to 19 do if times[i]>times[j] then begin tmp:=times[i];times[i]:=times[j];times[j]:=tmp;end;
 WriteLn('view=',k,' labels=',a,' total_ms=',total,' p50=',times[9],' p95=',times[18],' max=',times[19]);v.Free;
 end;
 b.Free;MainForm.Free;
end.

