program about_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Classes,Controls,ExtCtrls,Buttons,StdCtrls,Menus,Main,FreeAboutDlg;
var bar:TPanel;b:TSpeedButton;a:TFreeAboutDlg;toggle:TButton;i:integer;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);
 if MainForm.FindComponent('Help1')<>nil then Halt(1);
 if MainForm.FindComponent('HelpAction')<>nil then Halt(2);
 if MainForm.FindComponent('ActionCheckUpdates')<>nil then Halt(3);
 bar:=MainForm.FindComponent('StudioToolbar') as TPanel;
 b:=MainForm.FindComponent('StudioAbout') as TSpeedButton;
 bar.Width:=1400;bar.OnResize(bar);
 if b.Action<>MainForm.AboutAction then Halt(4);
 if b.Left+b.Width>bar.ClientWidth then Halt(5);
 for i:=0 to bar.ControlCount-1 do if (bar.Controls[i]<>b) and bar.Controls[i].Visible then
 if bar.Controls[i].Left+bar.Controls[i].Width>b.Left then Halt(6);
 if MainForm.FindComponent('StudioButton6')<>nil then Halt(12);
 a:=TFreeAboutDlg.Create(nil);
 if a.StringGridVersionInfo.Visible then Halt(13);
 toggle:=a.FindComponent('AboutVersionToggle') as TButton;
 toggle.Click;if not a.StringGridVersionInfo.Visible then Halt(14);
 toggle.Click;if a.StringGridVersionInfo.Visible then Halt(15);
 if (a.Width<1000) or (a.Height<750) or (a.Font.Size<12) then Halt(7);
 toggle:=a.FindComponent('AboutCopyrightToggle') as TButton;
 if a.StringGridCopyrights.Visible then Halt(8);
 toggle.Click;if not a.StringGridCopyrights.Visible then Halt(9);
 if a.StringGridCopyrights.RowCount<3 then Halt(10);
 toggle.Click;if a.StringGridCopyrights.Visible then Halt(11);
 WriteLn('PASS help removed; about rightmost and action bound; larger window/font; legal details collapse/expand');
 a.Free;MainForm.Free;
end.


