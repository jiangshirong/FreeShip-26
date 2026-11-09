program boundary_continuity_test;
{$mode objfpc}{$H+}
uses Types,SysUtils,FreeLabelLayout;
var l:TFreeLabelLayout;p:TLabelPlacement;i,x,lastx:integer;hadVisible,disappeared:boolean;
begin
 l:=TFreeLabelLayout.Create;lastx:=-1;hadVisible:=false;disappeared:=false;
 for i:=0 to 90 do begin
  Sleep(16);x:=330+i;
  l.TranslateHistory(1,0);l.BeginFrame(Rect(0,0,400,240));
  p:=l.Place('right-tick','113478',Point(x,100),Point(x,100),50,18,lkHorizontalTick);
  if p.Visible then begin
   if disappeared then Halt(1);
   if hadVisible and (p.Position.X<lastx) then begin WriteLn('snap ',lastx,' -> ',p.Position.X);Halt(2) end;
   if (x>=365) and (p.Position.X<=x) then Halt(3);
   lastx:=p.Position.X;hadVisible:=true;
  end else if hadVisible then disappeared:=true;
  l.EndFrame;
 end;
 if not disappeared then Halt(4);
 for i:=90 downto 0 do begin
  Sleep(16);x:=330+i;l.TranslateHistory(-1,0);l.BeginFrame(Rect(0,0,400,240));
  p:=l.Place('right-tick','113478',Point(x,100),Point(x,100),50,18,lkHorizontalTick);
  if p.Visible and (x>=365) and (p.Position.X<=x) then Halt(5);
  l.EndFrame;
 end;
 l.Free;WriteLn('PASS numeric edge capture/release; no un-attracted reset or reappearance after exit');
end.
