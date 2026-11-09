program full_animation_bench;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Windows,SysUtils,Graphics,FreeGeometry,FreeShipUnit,FreeTypes,Main;
var s:TFreeShip;v:array[0..3]of TFreeViewport;b:array[0..3]of TBitmap;i,j,k:Integer;t0,t1,freq:Int64;times:array[0..19]of Double;tmp:Double;
begin Application.Initialize;Application.CreateForm(TMainForm,MainForm);s:=MainForm.FreeShip;s.Edit.File_Load(GetEnvironmentVariable('FREESHIP_TEST_MODEL'));s.RebuildModel;QueryPerformanceFrequency(freq);
for i:=0 to 3 do begin b[i]:=TBitmap.Create;b[i].SetSize(960,500);v[i]:=TFreeViewport.Create(nil);v[i].SetBounds(0,0,960,500);v[i].DestinationWidth:=960;v[i].DestinationHeight:=500;v[i].DrawingCanvas:=b[i].Canvas;v[i].Color:=clWhite;v[i].ViewType:=TFreeViewType(i);v[i].ViewportMode:=vmWireframe;v[i].InitializeViewport(s.Surface.Min,s.Surface.Max);v[i].FinishProgressiveDisplay(False) end;
if ParamCount>0 then for i:=0 to 3 do begin v[i].BeginProgressiveDisplay;s.DrawToViewport(v[i]);v[i].FinishProgressiveDisplay(False) end;
for k:=0 to 19 do begin QueryPerformanceCounter(t0);for i:=0 to 3 do begin b[i].Canvas.Brush.Color:=clWhite;b[i].Canvas.FillRect(0,0,960,500);s.DrawToViewport(v[i]) end;QueryPerformanceCounter(t1);times[k]:=1000*(t1-t0)/freq;end;
for i:=0 to 3 do WriteLn('view ',i,' builds=',v[i].OverlayBuildCount);
WriteLn('first_four_views_ms=',times[0]:0:3);for i:=1 to 18 do for j:=i+1 to 19 do if times[i]>times[j] then begin tmp:=times[i];times[i]:=times[j];times[j]:=tmp end;WriteLn('four_views_p50_ms=',times[10]:0:3,' p95_ms=',times[19]:0:3);
for i:=0 to 3 do begin v[i].Free;b[i].Free end;MainForm.Free;end.
