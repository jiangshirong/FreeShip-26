program overlay_perf_bench;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Windows,SysUtils,Graphics,Types,FreeOverlayReveal;
var r:TOverlayReveal;b:TBitmap;i,j:Integer;t0,t1,freq:Int64;cold:Double;times:array[0..59]of Double;tmp:Double;
procedure Frame(p:Double);var k:Integer;begin b.Canvas.Brush.Color:=clWhite;b.Canvas.FillRect(0,0,1000,600);r.BeginFrame;for k:=0 to 15 do begin r.AddLine(10,10+k*35,990,10+k*35,clBlue,2,psSolid);r.AddLine(10+k*60,10,10+k*60,590,clRed,2,psSolid) end;r.Draw(b.Canvas,p,Point(500,300)) end;
begin Application.Initialize;QueryPerformanceFrequency(freq);b:=TBitmap.Create;b.SetSize(1000,600);r:=TOverlayReveal.Create;
QueryPerformanceCounter(t0);Frame(0.01);QueryPerformanceCounter(t1);cold:=1000*(t1-t0)/freq;
for i:=0 to 59 do begin QueryPerformanceCounter(t0);Frame((i+1)/60);QueryPerformanceCounter(t1);times[i]:=1000*(t1-t0)/freq end;
for i:=0 to 58 do for j:=i+1 to 59 do if times[i]>times[j] then begin tmp:=times[i];times[i]:=times[j];times[j]:=tmp end;
if r.BuildCount<>1 then begin WriteLn('FAIL rebuilt stable graph ',r.BuildCount);Halt(1) end;WriteLn('PASS stable graph built once');
WriteLn('cold_ms=',cold:0:3,' p50_ms=',times[29]:0:3,' p95_ms=',times[56]:0:3,' max_ms=',times[59]:0:3);r.Free;b.Free;end.
