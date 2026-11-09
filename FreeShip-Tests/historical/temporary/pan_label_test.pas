program pan_label_test;
{$mode objfpc}{$H+}
uses Types,FreeLabelLayout;
var l:TFreeLabelLayout;p:TLabelPlacement;i,j,dx,pan,x:integer;
begin
 l:=TFreeLabelLayout.Create;pan:=0;
 for i:=0 to 80 do begin
  if i=0 then dx:=0 else if i<=40 then dx:=10 else dx:=-10;
  inc(pan,dx);l.TranslateHistory(dx,0);l.BeginFrame(Rect(0,0,400,200));
  for j:=0 to 2 do begin x:=20+j*80+pan;
   p:=l.Place(chr(65+j),'42',Point(x,60),Point(x,60),40,18,lkNamedLine);
   if (x<400) and (x+40>0) then begin
    if not p.Visible then Halt(1);
    if p.Position.X<>x then Halt(2);
   end;
  end;l.EndFrame;
 end;l.Free;WriteLn('PASS 81 pan frames; partial clipping and re-entry retain anchor positions');
end.
