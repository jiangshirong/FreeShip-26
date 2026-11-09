program smooth_batch_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Graphics,FreeSmoothLines;
var a,b:TBitmap;lines:array[0..3]of TSmoothSegment;i,x,y:Integer;
begin Application.Initialize;a:=TBitmap.Create;b:=TBitmap.Create;a.SetSize(240,150);b.SetSize(240,150);
a.Canvas.Brush.Color:=clWhite;a.Canvas.FillRect(0,0,240,150);b.Canvas.Brush.Color:=clWhite;b.Canvas.FillRect(0,0,240,150);a.Canvas.Pen.Color:=clBlue;b.Canvas.Pen.Color:=clBlue;a.Canvas.Pen.Width:=2;b.Canvas.Pen.Width:=2;
for i:=0 to 3 do begin lines[i].A.X:=10.2+i*13;lines[i].A.Y:=20.3+i*7;lines[i].B.X:=170.6-i*8;lines[i].B.Y:=100.4+i*8;DrawSmoothLine(a.Canvas,lines[i].A.X,lines[i].A.Y,lines[i].B.X,lines[i].B.Y) end;
if not DrawSmoothSegments(b.Canvas,lines) then Halt(1);
for y:=0 to 149 do for x:=0 to 239 do if a.Canvas.Pixels[x,y]<>b.Canvas.Pixels[x,y] then begin WriteLn('FAIL pixel mismatch');Halt(2) end;
b.Canvas.Pen.Style:=psDash;if DrawSmoothSegments(b.Canvas,lines) then Halt(3);a.Free;b.Free;WriteLn('PASS batch matches per-line pixels and preserves dashed fallback');end.
