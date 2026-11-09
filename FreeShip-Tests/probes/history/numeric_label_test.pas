program numeric_label_test;
{$mode objfpc}{$H+}
uses Types,FreeLabelLayout;
var l:TFreeLabelLayout;p:TLabelPlacement;i:integer;
begin
 l:=TFreeLabelLayout.Create;
 for i:=1 to 2 do begin
 l.BeginFrame(Rect(0,0,500,300));l.Reserve(Rect(298,95,306,125));
 p:=l.Place('tick','8680',Point(300,100),Point(300,100),40,18,lkHorizontalTick,1);
 if not p.Visible then Halt(1);
 if p.Rect.Right-p.Rect.Left<>40 then Halt(2);
 if p.Rect.Left<308 then Halt(3);
 l.EndFrame;
 end;
 l.Free;WriteLn('PASS internal control-point obstacle moves entire 8680 label; no suffix clipping');
end.
