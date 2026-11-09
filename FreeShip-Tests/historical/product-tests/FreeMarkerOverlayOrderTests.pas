program marker_order_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Graphics,Types,SysUtils,FreeGeometry;
var v:TFreeViewport;b:TBitmap;i:Integer;
procedure Frame(animated:Boolean);
begin
 b.Canvas.Brush.Color:=clWhite;b.Canvas.FillRect(0,0,200,200);
 if animated then v.BeginOverlayFrame;
 v.PenColor:=clRed;v.Line(10,100,190,100);
 v.PenColor:=clAqua;v.DrawControlPointMarker(Point(100,100),3);
 if animated then v.EndOverlayFrame;
 if ColorToRGB(b.Canvas.Pixels[100,100])<>ColorToRGB(clAqua) then begin WriteLn('FAIL marker covered');Halt(1) end;
end;
begin
 Application.Initialize;v:=TFreeViewport.Create(nil);b:=TBitmap.Create;b.SetSize(200,200);v.SetBounds(0,0,200,200);v.DrawingCanvas:=b.Canvas;
 v.FinishProgressiveDisplay(False);Sleep(1350);
 for i:=1 to 3 do Frame(True);
 v.FinishProgressiveDisplay(True);Frame(False);
 WriteLn('PASS marker stays above red reveal line and resting line');v.Free;b.Free;
end.
