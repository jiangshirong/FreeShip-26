program label_space_test;
{$mode objfpc}{$H+}
uses Types,SysUtils,FreeLabelLayout;
var l:TFreeLabelLayout;p:TLabelPlacement;i,w,oldWidth,blockX:integer;
begin
 l:=TFreeLabelLayout.Create;oldWidth:=0;
 for i:=0 to 45 do begin
  Sleep(16);blockX:=60+i*2;
  l.BeginFrame(Rect(0,0,300,150));l.Reserve(Rect(blockX,35,280,80));
  p:=l.Place('line','design waterline',Point(20,40),Point(20,40),100,20,lkNamedLine);
  if not p.Visible or (p.Position.X<>20) then Halt(1);
  w:=p.Rect.Right-p.Rect.Left;
  if (w<oldWidth) or (w-oldWidth>16) or (p.Rect.Right>blockX-2) then begin WriteLn(i,' widths ',oldWidth,' ',w,' block ',blockX,' right ',p.Rect.Right);Halt(2) end;
  oldWidth:=w;l.EndFrame;
 end;
 for i:=0 to 35 do begin
  Sleep(16);blockX:=150-i*2;
  l.BeginFrame(Rect(0,0,300,150));l.Reserve(Rect(blockX,35,280,80));
  p:=l.Place('line','design waterline',Point(20,40),Point(20,40),100,20,lkNamedLine);
  if not p.Visible or (p.Position.X<>20) or (p.Rect.Right>blockX-2) then Halt(3);
  l.EndFrame;
 end;
 WriteLn('PASS expanding/restricting free space; stable anchor, partial text, no obstacle overlap');l.Free;
end.
