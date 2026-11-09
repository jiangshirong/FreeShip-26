program edge_clip_test;
{$mode objfpc}{$H+}
uses Types,FreeLabelLayout;
var l:TFreeLabelLayout;p:TLabelPlacement;i:integer;points:array[0..3]of TPoint;
begin
 points[0]:=Point(-20,40);points[1]:=Point(90,40);points[2]:=Point(40,-10);points[3]:=Point(40,95);
 l:=TFreeLabelLayout.Create;
 for i:=0 to 3 do begin l.Clear;l.BeginFrame(Rect(0,0,100,100));
 p:=l.Place('x','1234',points[i],points[i],40,20,lkNamedLine);
 if not p.Visible then Halt(1);
 if (p.Position.X<>points[i].X) or (p.Position.Y<>points[i].Y) then Halt(2);
 l.EndFrame;end;
 l.Clear;l.BeginFrame(Rect(0,0,100,100));p:=l.Place('x','1234',Point(-60,40),Point(-60,40),40,20,lkNamedLine);
 if p.Visible then Halt(3);l.Free;WriteLn('PASS four edges preserve partially visible text; wholly outside omitted');
end.
