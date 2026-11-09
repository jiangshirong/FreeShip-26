program bfs_overlay_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Graphics,Types,FreeOverlayReveal;
var r:TOverlayReveal;b:TBitmap;i,n0,n1,n2:Integer;
procedure Clear;begin b.Canvas.Brush.Color:=clWhite;b.Canvas.FillRect(0,0,400,220) end;
function Count:Integer;var x,y:Integer;begin Result:=0;for y:=0 to 219 do for x:=0 to 399 do if ColorToRGB(b.Canvas.Pixels[x,y])<>ColorToRGB(clWhite) then Inc(Result) end;
procedure Check(ok:Boolean;const msg:string);begin if not ok then begin WriteLn('FAIL ',msg);Halt(1) end;WriteLn('PASS ',msg) end;
begin Application.Initialize;r:=TOverlayReveal.Create;b:=TBitmap.Create;b.SetSize(400,220);r.BeginFrame;
r.AddLine(0,100,200,100,clBlue,2,psSolid);r.AddLine(100,0,100,200,clRed,2,psSolid);r.AddLine(200,100,250,100,clBlue,2,psSolid);r.AddLine(300,100,350,100,clRed,2,psSolid);
Clear;r.Draw(b.Canvas,0,Point(100,100));n0:=Count;Check(n0=0,'no instantaneous full overlays');Check(r.LevelAt(100,100)=0,'interior crossing is source');Check(r.LevelAt(200,100)=4,'long line split into bounded BFS steps');Check(r.LevelAt(250,100)=6,'branch follows connected endpoint');Check(r.LevelAt(300,100)>6,'disconnected component joins later');
Clear;r.Draw(b.Canvas,0.1,Point(100,100));n1:=Count;Check(n1>n0,'segments grow from source');Check(ColorToRGB(b.Canvas.Pixels[0,100])=ColorToRGB(clWhite),'far endpoint not drawn prematurely');
Clear;r.Draw(b.Canvas,1,Point(100,100));n2:=Count;Check(n2>n1,'complete graph arrives');Check(ColorToRGB(b.Canvas.Pixels[320,100])<>ColorToRGB(clWhite),'disconnected lines complete');
b.SaveToFile('C:\Users\12968\AppData\Local\Temp\FreeShipLabelIntegration\bfs-final.bmp');b.Free;r.Free;end.
