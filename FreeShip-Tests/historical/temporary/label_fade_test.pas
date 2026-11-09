program label_fade_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Graphics,FreeGeometry;
var v:TFreeViewport;b:TBitmap;i,j:integer;a,c,d:Int64;
function DrawAndMeasure:Int64;
var x,y:integer;
begin
 b.Canvas.Brush.Color:=clWhite;b.Canvas.FillRect(0,0,240,60);
 b.Canvas.Font.Color:=clBlack;v.TextOut(5,5,'123.45');
 if b.Canvas.Font.Color<>clBlack then Halt(2);
 result:=0;for x:=0 to 200 do for y:=0 to 45 do result:=result+255-(ColorToRGB(b.Canvas.Pixels[x,y]) and 255);
end;
begin
 Application.Initialize;v:=TFreeViewport.Create(nil);b:=TBitmap.Create;b.SetSize(240,60);
 v.DrawingCanvas:=b.Canvas;v.Color:=clWhite;v.FontName:='Arial';v.FontSize:=14;
 v.FinishProgressiveDisplay;a:=DrawAndMeasure;Sleep(240);c:=DrawAndMeasure;Sleep(220);d:=DrawAndMeasure;
 if not ((a<c) and (c<d)) then begin WriteLn('FAIL ',a,' ',c,' ',d);Halt(1) end;
 v.FinishProgressiveDisplay(True);if DrawAndMeasure<>d then Halt(3);
 WriteLn('PASS numeric labels progressively darken; font color restored; skip reveals immediately');
 b.Free;v.Free;
end.
