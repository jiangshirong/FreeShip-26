program test_rongship;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Classes,Controls,ExtCtrls,Buttons,Menus,ActnList,Main;
var bar:TPanel;menu:TPopupMenu;i,j,n:integer;b:TSpeedButton;
function hasAction(item:TMenuItem;act:TAction):boolean;var k:integer;begin if item.Action=act then exit(true);for k:=0 to item.Count-1 do if hasAction(item.Items[k],act) then exit(true);result:=false end;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);MainForm.SetCaption;
 if Pos('RongShip',MainForm.Caption)<>1 then Halt(1);
 bar:=MainForm.FindComponent('StudioToolbar') as TPanel;n:=0;
 for i:=0 to bar.ControlCount-1 do if bar.Controls[i] is TPanel then
 for j:=0 to TPanel(bar.Controls[i]).ControlCount-1 do begin
  b:=TSpeedButton(TPanel(bar.Controls[i]).Controls[j]);inc(n);
  if (b.Action=MainForm.NewModel) or (b.Action=MainForm.LoadFile) or (b.Action=MainForm.Redo) then Halt(2);
  if b.ImageIndex in [12,13] then Halt(3);
 end;
 if n<>12 then begin WriteLn('FAIL count ',n);Halt(4) end;
 menu:=MainForm.FindComponent('StudioMainMenu') as TPopupMenu;
 if not hasAction(menu.Items,MainForm.NewModel) or not hasAction(menu.Items,MainForm.LoadFile) or not hasAction(menu.Items,MainForm.Redo) then Halt(5);
 WriteLn('PASS 12 toolbar buttons; removed commands retained in menu; RongShip title');
 MainForm.Free;
end.
