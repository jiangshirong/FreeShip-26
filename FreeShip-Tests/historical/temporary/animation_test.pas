program animation_test;
{$mode objfpc}{$H+}
uses SysUtils,Math,Types,FreeLabelLayout;
var l:TFreeLabelLayout;p,q:TLabelPlacement;i:integer;old:TPoint;blocked:TRect;
begin
 l:=TFreeLabelLayout.Create;try
 l.BeginFrame(Rect(0,0,500,400));p:=l.Place('a','42',Point(200,200),Point(208,203),40,18,lkPoint);l.EndFrame;
 blocked:=p.Rect;
 for i:=1 to 90 do begin
  Sleep(16);old:=p.Position;l.BeginFrame(Rect(0,0,500,400));
  l.AddSegment(Point(blocked.Left,blocked.Top+5),Point(blocked.Right,blocked.Top+5));
  p:=l.Place('a','42',Point(200,200),Point(208,203),40,18,lkPoint);l.EndFrame;
  if not p.Visible then begin WriteLn('FAIL hidden');Halt(1) end;
  if Hypot(p.Position.X-old.X,p.Position.Y-old.Y)>9 then begin WriteLn('FAIL jumped');Halt(2) end;
 end;
 if l.Animating then begin WriteLn('FAIL not settled');Halt(3) end;
 WriteLn('PASS bounded transition and settles after repeated frames');
 finally l.Free end;
end.
