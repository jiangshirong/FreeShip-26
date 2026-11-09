program view_state_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Types,ExtCtrls,MDIPanel,LCLType,LMessages;
var f:TForm;host:TPanel;m:TMDIPanelManager;p:array[0..3]of TMDIPanel;i,j,sx,sy:integer;pos:TWindowPos;original:array[0..3]of TRect;
begin
 Application.Initialize;f:=TForm.CreateNew(nil);host:=TPanel.Create(f);host.Parent:=f;host.SetBounds(0,0,1000,700);m:=TMDIPanelManager.Create;
 for i:=0 to 3 do begin p[i]:=TMDIPanel.Create(f);p[i].Parent:=host;m.Add(p[i]);p[i].SetBounds((i mod 2)*500,(i div 2)*350,500,350);original[i]:=p[i].BoundsRect;end;
 for i:=0 to 3 do p[i].Visible:=True;
 sx:=493;sy:=5;p[0].SnapMovePosition(sx,sy);if (sx<>500) or (sy<>0) then Halt(7);
 sx:=30;sy:=50;p[0].SnapMovePosition(sx,sy);if (sx<>30) or (sy<>50) then Halt(8);
 FillChar(pos,SizeOf(pos),0);p[0].Perform(LM_WINDOWPOSCHANGING,0,LPARAM(@pos));
 if (pos.flags and SWP_NOCOPYBITS)=0 then Halt(6);
 for i:=0 to 3 do p[i].WindowState:=wsMinimized;
 for i:=0 to 3 do begin
  if p[i].Top=0 then Halt(1);
  for j:=i+1 to 3 do if p[i].BoundsRect=p[j].BoundsRect then Halt(2);
 end;
 p[1].WindowState:=wsMaximized;
 if (p[1].Width<>host.ClientWidth) or (p[1].Height<>host.ClientHeight) then Halt(3);
 p[1].WindowState:=wsNormal;if p[1].BoundsRect<>original[1] then Halt(4);
 m.Tile;
 for i:=0 to 3 do if (p[i].WindowState<>wsNormal) or (p[i].Width<400) or (p[i].Height<300) then Halt(5);
 WriteLn('PASS four minimized views do not stack; maximize/restore bounds; tiling restores normal states');m.Free;f.Free;
end.
