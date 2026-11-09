program marker_pixel_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Graphics,Types,FreeGeometry;
var a,b:TBitmap;v:TFreeViewport;i,r,x,y:Integer;
begin Application.Initialize;a:=TBitmap.Create;b:=TBitmap.Create;a.SetSize(50,50);b.SetSize(50,50);v:=TFreeViewport.Create(nil);v.DrawingCanvas:=b.Canvas;
for r:=1 to 6 do begin a.Canvas.Brush.Style:=bsSolid;a.Canvas.Brush.Color:=clWhite;a.Canvas.FillRect(0,0,50,50);b.Canvas.Brush.Style:=bsSolid;b.Canvas.Brush.Color:=clWhite;b.Canvas.FillRect(0,0,50,50);a.Canvas.Pen.Color:=clBlue;b.Canvas.Pen.Color:=clBlue;a.Canvas.Brush.Style:=bsClear;b.Canvas.Brush.Style:=bsClear;
for i:=1 to r do a.Canvas.Rectangle(25-i,25-i,25+i,25+i);v.DrawControlPointMarker(Point(25,25),r);
for y:=0 to 49 do for x:=0 to 49 do if a.Canvas.Pixels[x,y]<>b.Canvas.Pixels[x,y] then begin WriteLn('FAIL radius ',r);Halt(1) end;end;WriteLn('PASS marker optimization matches original pixels');v.Free;a.Free;b.Free;end.
