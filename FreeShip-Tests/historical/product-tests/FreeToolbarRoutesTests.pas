program toolbar_routes_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Classes,SysUtils,Controls,Graphics,ExtCtrls,Menus,ActnList,Main,FreeSafeLanguage;
var f:TMainForm;bar:TPanel;b:TBitmap;i,j:Integer;m:TPopupMenu;seen:TStringList;
procedure Check(ok:Boolean;const msg:string);begin if not ok then begin WriteLn('FAIL ',msg);Halt(1) end;WriteLn('PASS ',msg) end;
procedure Scan(item:TMenuItem);
var n:Integer;k:string;
begin
 if not item.Visible then Exit;
 if Assigned(item.Action) then begin k:=item.Action.Name;Check(seen.IndexOf(k)<0,'unique '+k);seen.Add(k) end;
 for n:=0 to item.Count-1 do Scan(item.Items[n]);
end;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);f:=MainForm;
 seen:=TStringList.Create;
 for i:=0 to f.ComponentCount-1 do if f.Components[i] is TPopupMenu then begin
  m:=TPopupMenu(f.Components[i]);
  if (m.Name='StudioMainMenu') or (m.Name='StudioCalculationsMenu') or (Pos('StudioFeature',m.Name)=1) or (m.Name='StudioChecksMenu') then Scan(m.Items);
 end;
 Check(seen.IndexOf('Undo')<0,'no undo menu');Check(seen.IndexOf('Redo')<0,'no redo menu');Check(seen.IndexOf('Delete')<0,'no delete menu');
 Check(seen.IndexOf('ShowControlNet')<0,'control net only toolbar');Check(seen.IndexOf('IntersectionDialog')<0,'sections only toolbar');Check(seen.IndexOf('ShowLinesplan')<0,'lines plan only toolbar');
 for i:=0 to 1 do begin
  if i=0 then ApplyLanguageSafely('cn','K:\locale') else ApplyLanguageSafely('ru','K:\locale');f.RefreshStudioLanguage;
  for j:=0 to 1 do begin
   if j=0 then f.Width:=1920 else f.Width:=1100;
   bar:=TPanel(f.FindComponent('StudioToolbar'));b:=TBitmap.Create;b.SetSize(bar.Width,bar.Height);bar.PaintTo(b.Canvas,0,0);b.SaveToFile(GetTempDir+'FreeShipLabelIntegration/toolbar-routes-'+IntToStr(i)+'-'+IntToStr(j)+'.bmp');b.Free;
  end;
 end;
 seen.Free;f.Free;
end.
