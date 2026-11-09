program zoom_bounds_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Types,SysUtils,FreeGeometry;
var v:TFreeViewport;i:Integer;p:TPoint;
procedure Check(ok:Boolean;const msg:string);begin if not ok then begin WriteLn('FAIL ',msg);Halt(1) end;WriteLn('PASS ',msg) end;
begin Application.Initialize;v:=TFreeViewport.Create(nil);
v.Zoom:=1000;Check(v.Zoom=20,'upper bound');v.Pan:=Point(31,47);p:=v.Pan;for i:=1 to 1000 do v.ZoomIn;Check((v.Zoom=20) and (v.Pan.X=p.X) and (v.Pan.Y=p.Y),'upper bound does not drift');
v.Zoom:=0;Check(Abs(v.Zoom-0.1)<1E-8,'lower bound');v.Pan:=Point(31,47);p:=v.Pan;for i:=1 to 1000 do v.ZoomOut;Check((Abs(v.Zoom-0.1)<1E-8) and (v.Pan.X=p.X) and (v.Pan.Y=p.Y),'lower bound does not drift');
v.ZoomIn;Check(v.Zoom>0.1,'can reverse at lower bound');v.Zoom:=20;v.ZoomOut;Check(v.Zoom<20,'can reverse at upper bound');v.Free;end.
