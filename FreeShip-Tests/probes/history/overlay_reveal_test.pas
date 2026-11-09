program overlay_reveal_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Graphics,FreeGeometry;
var v:TFreeViewport;b:TBitmap;c0,c1:TColor;
begin Application.Initialize;b:=TBitmap.Create;b.SetSize(300,200);v:=TFreeViewport.Create(nil);v.DrawingCanvas:=b.Canvas;v.Color:=clWhite;
v.FinishProgressiveDisplay(False);v.PenColor:=clBlue;c0:=ColorToRGB(v.PenColor);if c0=ColorToRGB(clBlue) then Halt(1);Sleep(150);v.PenColor:=clBlue;c1:=ColorToRGB(v.PenColor);if c1=c0 then Halt(2);Sleep(200);v.PenColor:=clBlue;if ColorToRGB(v.PenColor)<>ColorToRGB(clBlue) then Halt(3);
v.FinishProgressiveDisplay(True);v.PenColor:=clRed;if ColorToRGB(v.PenColor)<>ColorToRGB(clRed) then Halt(4);v.Free;b.Free;WriteLn('PASS overlay gradual entry and final colors');end.
