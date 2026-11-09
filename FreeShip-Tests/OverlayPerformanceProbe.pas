program overlay_perf_bench;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Windows,SysUtils,Graphics,Types,FreeOverlayReveal;
var r:TOverlayReveal;b:TBitmap;i,j:Integer;t0,t1,freq:Int64;cold:Double;times:array[0..59]of Double;tmp:Double;
procedure Frame(p:Double);var k:Integer;begin b.Canvas.Brush.Color:=clWhite;b.Canvas.FillRect(0,0,960,540);r.BeginFrame;for k:=0 to 15 do begin r.AddLine(10,10+k*31,950,10+k*31,clBlue,2,psSolid);r.AddLine(10+k*56,10,10+k*56,530,clRed,2,psSolid) end;r.Draw(b.Canvas,p,Point(480,270)) end;
procedure Behavior;
var Test:TOverlayReveal;Image:TBitmap;Before,After,I:Integer;
 procedure Check(OK:Boolean;const Message:string);
 begin if not OK then begin WriteLn('FAIL ',Message);Halt(1) end;WriteLn('PASS ',Message) end;
 function Ink:Integer;
 var X,Y:Integer;
 begin Result:=0;for Y:=0 to Image.Height-1 do for X:=0 to Image.Width-1 do
  if ColorToRGB(Image.Canvas.Pixels[X,Y])<>ColorToRGB(clWhite) then Inc(Result) end;
 procedure Draw(P:Double);
 begin Image.Canvas.Brush.Color:=clWhite;Image.Canvas.FillRect(0,0,400,220);Test.Draw(Image.Canvas,P,Point(100,100)) end;
 begin
  Test:=TOverlayReveal.Create;Image:=TBitmap.Create;Image.SetSize(400,220);
  Test.BeginFrame;Test.AddLine(0,100,200,100,clBlue,2,psSolid);
  Test.AddLine(100,0,100,200,clRed,2,psSolid);
  Test.AddLine(200,100,250,100,clBlue,2,psSolid);Test.AddLine(300,100,350,100,clRed,2,psSolid);
  Draw(0);Check(Ink=0,'zero progress empty');Check(Test.LevelAt(100,100)=0,'crossing source preserved');
  Draw(0.1);Before:=Ink;Check(Before>0,'front grows');Check(ColorToRGB(Image.Canvas.Pixels[0,100])=ColorToRGB(clWhite),'far endpoint waits');
  Image.SaveToFile(IncludeTrailingPathDelimiter(ParamStr(1))+'front.bmp');
  Draw(1);After:=Ink;Check(After>Before,'graph completes');Check(ColorToRGB(Image.Canvas.Pixels[320,100])<>ColorToRGB(clWhite),'disconnected branch completes');
  Check(ColorToRGB(Image.Canvas.Pixels[275,100])=ColorToRGB(clWhite),'no fabricated bridge');
  Image.SaveToFile(IncludeTrailingPathDelimiter(ParamStr(1))+'complete.bmp');
  Check(Test.BuildCount=1,'stable graph not rebuilt');Image.Free;Test.Free;
 end;
begin Application.Initialize;Behavior;QueryPerformanceFrequency(freq);b:=TBitmap.Create;b.SetSize(960,540);r:=TOverlayReveal.Create;
QueryPerformanceCounter(t0);Frame(0.01);QueryPerformanceCounter(t1);cold:=1000*(t1-t0)/freq;
for i:=0 to 59 do begin QueryPerformanceCounter(t0);for j:=1 to 4 do Frame((i+1)/60);QueryPerformanceCounter(t1);times[i]:=1000*(t1-t0)/freq end;
for i:=0 to 58 do for j:=i+1 to 59 do if times[i]>times[j] then begin tmp:=times[i];times[i]:=times[j];times[j]:=tmp end;
if r.BuildCount<>1 then begin WriteLn('FAIL rebuilt stable graph ',r.BuildCount);Halt(1) end;WriteLn('PASS stable graph built once');
WriteLn('cold_ms=',cold:0:3,' p50_ms=',times[29]:0:3,' p95_ms=',times[56]:0:3,' max_ms=',times[59]:0:3);r.Free;b.Free;end.
