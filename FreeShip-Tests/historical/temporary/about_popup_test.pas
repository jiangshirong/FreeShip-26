program about_popup_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Classes,Controls,StdCtrls,Grids,Buttons,ExtCtrls,FreeAboutDlg,FreeSafeLanguage;
var a:TFreeAboutDlg;p:TForm;g:TStringGrid;f:TPanel;v,l:TBitBtn;i,j:integer;k:Word;
begin
 Application.Initialize;ApplyLanguageSafely('ru','K:\locale');a:=TFreeAboutDlg.Create(nil);
 a.HandleNeeded; f:=a.FindComponent('AboutFooter') as TPanel;
 v:=a.FindComponent('AboutVersionToggle') as TBitBtn;l:=a.FindComponent('AboutCopyrightToggle') as TBitBtn;
 for i:=0 to 2 do begin
  case i of 0:a.SetBounds(0,0,1040,760);1:a.SetBounds(0,0,840,620);2:a.SetBounds(0,0,1280,850) end;
  a.OnResize(a);
  if a.StringGridCredits.ScrollBars<>ssNone then Halt(1);
  if a.StringGridCredits.Height < a.StringGridCredits.RowCount*(a.StringGridCredits.DefaultRowHeight+1) then Halt(2);
  if (v.Parent<>f) or (l.Parent<>f) or (v.Left+v.Width>=l.Left) then Halt(3);
  if a.StringGridCredits.Top+a.StringGridCredits.Height>a.Panel2.ClientHeight then Halt(4);
 end;
 for i:=0 to 1 do begin
  p:=a.CreateInformationWindow(i=0);
  g:=p.FindComponent('InformationGrid') as TStringGrid;
  if (g.ScrollBars<>ssNone) or (g.Top+g.Height>p.ClientHeight) then Halt(5);
  if g.Height < g.RowCount*(g.DefaultRowHeight+1) then Halt(6);
  for j:=0 to g.RowCount-1 do
   begin WriteLn('row ',i,' ',j,' ',g.Cells[0,j],' / ',g.Cells[1,j]); if g.Cells[0,j]='' then Halt(7) end;
  if (i=1) and (g.RowCount<>4) then Halt(9);
  k:=27;p.OnKeyDown(p,k,[]);if k<>0 then Halt(8);
  p.Free;
 end;
 WriteLn('PASS complete credit rows at 3 sizes; no scroll containers/bars; separate version/license windows; Esc handler');
 a.Free;
end.

