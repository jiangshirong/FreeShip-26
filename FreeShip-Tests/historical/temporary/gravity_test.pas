program gravity_test;
{$mode objfpc}{$H+}
uses Types,FreeLabelLayout;
var p:TPoint;i,previous:integer;
begin
 previous:=0;
 for i:=40 downto -15 do begin
 p:=TFreeLabelLayout.EdgeAttraction(Rect(0,0,400,300),Rect(i,120,i+60,140));
 if (p.X>0) or (abs(p.X)<previous) then Halt(1);previous:=abs(p.X);
 end;
 p:=TFreeLabelLayout.EdgeAttraction(Rect(0,0,400,300),Rect(340,120,400,140));if p.X<=0 then Halt(2);
 p:=TFreeLabelLayout.EdgeAttraction(Rect(0,0,400,300),Rect(120,0,180,20));if p.Y>=0 then Halt(3);
 p:=TFreeLabelLayout.EdgeAttraction(Rect(0,0,400,300),Rect(120,280,180,300));if p.Y<=0 then Halt(4);
 WriteLn('PASS four-edge directions; attraction increases continuously near boundary');
end.
