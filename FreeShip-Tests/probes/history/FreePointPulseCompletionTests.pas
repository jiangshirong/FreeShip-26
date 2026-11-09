program pulse_completion_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Graphics,Types,SysUtils,FreeGeometry;
var v:TFreeViewport;b:TBitmap;c:TColor;
procedure Check(ok:Boolean;const msg:string);begin if not ok then begin WriteLn('FAIL ',msg);Halt(1) end;WriteLn('PASS ',msg) end;
begin
Application.Initialize;v:=TFreeViewport.Create(nil);b:=TBitmap.Create;b.SetSize(200,200);v.SetBounds(0,0,200,200);v.DrawingCanvas:=b.Canvas;
v.FinishProgressiveDisplay(False);Sleep(1100);
v.BeginOverlayFrame;c:=v.ControlPointEntranceColor(Point(100,100),clAqua,1);v.EndOverlayFrame;
Check(c<>clAqua,'first visible point is highlighted');
Sleep(200);v.BeginOverlayFrame;c:=v.ControlPointEntranceColor(Point(100,100),clAqua,1);v.EndOverlayFrame;
Check(c<>clAqua,'point still transitioning on last rendered frame');
Sleep(500);v.ProgressiveTick(nil);
Check(v.ControlPointEntranceColor(Point(100,100),clAqua,1)=c,'timer cannot discard unrendered color transition');
v.BeginOverlayFrame;c:=v.ControlPointEntranceColor(Point(100,100),clAqua,1);v.EndOverlayFrame;
Check(c=clAqua,'final frame reaches exact resting color');v.ProgressiveTick(nil);
Check(v.ControlPointEntranceColor(Point(100,100),clAqua,1)=c,'cleanup has zero color discontinuity');
v.Free;b.Free;
end.
