program spring_motion_test;
{$mode objfpc}{$H+}
uses Types,SysUtils,FreeLabelLayout;
var l:TFreeLabelLayout;p:TLabelPlacement;i,oldx:integer;
begin
 l:=TFreeLabelLayout.Create;l.BeginFrame(Rect(0,0,800,600));p:=l.Place('a','value',Point(300,200),Point(300,200),70,20,lkNamedLine);l.EndFrame;oldx:=p.Position.X;
 for i:=1 to 100 do begin Sleep(16);l.BeginFrame(Rect(0,0,800,600));p:=l.Place('a','value',Point(300,200),Point(380,200),70,20,lkNamedLine);l.EndFrame;
 if not p.Visible or (p.Position.X<oldx) or (p.Position.X>380) or (p.Position.X-oldx>12) then Halt(1);oldx:=p.Position.X;end;
 if abs(p.Position.X-380)>1 then Halt(2);
 l.Free;WriteLn('PASS damped movement is continuous, bounded and settles without overshoot');
end.
