program reference_identity_test;
{$mode objfpc}{$H+}
uses Types,SysUtils,FreeLabelLayout;
var l:TFreeLabelLayout;p:TLabelPlacement;i:integer;
begin
 l:=TFreeLabelLayout.Create;
 // A stale tick occupied the new reference label's neighbourhood.
 l.BeginFrame(Rect(0,0,400,240));l.Place('tick','94565',Point(140,80),Point(140,80),110,20,lkHorizontalTick);l.EndFrame;
 for i:=0 to 70 do begin
  Sleep(16);l.BeginFrame(Rect(0,0,400,240));
  p:=l.Place('reference:split:upper','split section:94565',Point(140,80),Point(140,80),110,20,lkNamedLine);
  if not p.Visible then Halt(1);
  if (p.Position.X<>140) or (p.Position.Y<>80) then Halt(2);
  if (i>30) and (p.Rect.Right-p.Rect.Left<>110) then Halt(3);
  // Identical text at a different endpoint disappears and returns independently.
  if (i<15) or (i>45) then l.Place('reference:split:lower','split section:94565',Point(140,210),Point(140,210),110,20,lkNamedLine);
  l.Place('tick','94565',Point(140,110),Point(140,110),110,20,lkHorizontalTick);
  l.EndFrame;
 end;
 l.Free;WriteLn('PASS reference survives stale tick reservation and identical endpoint visibility changes');
end.
