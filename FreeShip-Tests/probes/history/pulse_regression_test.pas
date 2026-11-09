program pulse_regression_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Graphics,Types,SysUtils,FreeGeometry;
var v:TFreeViewport;b:TBitmap;early,mid,last:TColor;i:Integer;
procedure Check(ok:Boolean;const Msg:String);begin if not ok then begin WriteLn('FAIL ',Msg);Halt(1) end;WriteLn('PASS ',Msg) end;
begin Application.Initialize;v:=TFreeViewport.Create(nil);b:=TBitmap.Create;b.SetSize(200,200);v.SetBounds(0,0,200,200);v.DrawingCanvas:=b.Canvas;
v.FinishProgressiveDisplay(False);Sleep(1500);v.ProgressiveTick(nil);
early:=v.ControlPointEntranceColor(Point(0,100),clAqua,1);Check(early<>clAqua,'late first frame still highlights point');
v.BeginOverlayFrame;early:=v.ControlPointEntranceColor(Point(0,100),clAqua,2);Sleep(120);mid:=v.ControlPointEntranceColor(Point(0,100),clAqua,2);Check(mid=early,'all colors use one frame clock');v.EndOverlayFrame;
Sleep(180);v.BeginOverlayFrame;mid:=v.ControlPointEntranceColor(Point(0,100),clAqua,1);Check((mid<>early) and (mid<>clAqua),'highlight gradually returns');v.EndOverlayFrame;
Sleep(400);v.BeginOverlayFrame;last:=v.ControlPointEntranceColor(Point(0,100),clAqua,1);Check(last=clAqua,'individual point reaches original color');v.EndOverlayFrame;
v.FinishProgressiveDisplay(True);Check(v.ControlPointEntranceColor(Point(0,0),clRed,2)=clRed,'explicit skip remains immediate');
for i:=1 to 6 do begin b.Canvas.Brush.Color:=clWhite;b.Canvas.FillRect(0,0,200,200);b.Canvas.Pen.Color:=clBlue;b.Canvas.Brush.Style:=bsClear;v.DrawControlPointMarker(Point(100,100),i);Check(b.Canvas.Pixels[100,100]=clBlue,'marker filled');end;
v.Free;b.Free;end.
