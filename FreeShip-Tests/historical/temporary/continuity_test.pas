program continuity_test;
{$mode objfpc}{$H+}
uses SysUtils,Math,Types,FreeLabelLayout;
var l:TFreeLabelLayout;a,b:TLabelPlacement;old:TPoint;i:integer;
begin
 l:=TFreeLabelLayout.Create;
 l.BeginFrame(Rect(0,0,500,400));
 a:=l.Place('first','1',Point(100,100),Point(100,100),30,18,lkPoint);
 b:=l.Place('second','2',Point(250,100),Point(250,100),30,18,lkPoint);l.EndFrame;
 old:=b.Position;
 // The first label now requests the second one's displayed location.
 for i:=1 to 30 do begin
  Sleep(16);l.BeginFrame(Rect(0,0,500+i,400));
  a:=l.Place('first',IntToStr(i),Point(100,100),Point(250,100),30,18,lkPoint);
  b:=l.Place('second',IntToStr(i+100),Point(250,100),Point(251,100),36,18,lkPoint);
  if not a.Visible or not b.Visible then Halt(1);
  if Hypot(b.Position.X-old.X,b.Position.Y-old.Y)>9 then Halt(2);
  if (a.Rect.Left<b.Rect.Right) and (a.Rect.Right>b.Rect.Left) and
     (a.Rect.Top<b.Rect.Bottom) and (a.Rect.Bottom>b.Rect.Top) then Halt(3);
  old:=b.Position;l.EndFrame;
 end;
 l.Free;WriteLn('PASS competing labels stay visible; value/size/preference/viewport changes preserve continuity');
end.
