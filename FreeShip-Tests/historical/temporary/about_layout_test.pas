program about_layout_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Classes,Controls,Graphics,Buttons,ExtCtrls,FreeAboutDlg,FreeSafeLanguage,Types;
var a:TFreeAboutDlg;v,l:TBitBtn;f,d:TPanel;b:TBitmap;i,j:integer;oldHandle:PtrUInt;r:TRect;
begin
 Application.Initialize;ApplyLanguageSafely('cn','K:\locale');a:=TFreeAboutDlg.Create(nil);
 a.HandleNeeded;a.Realign; Application.ProcessMessages;
 v:=a.FindComponent('AboutVersionToggle') as TBitBtn;l:=a.FindComponent('AboutCopyrightToggle') as TBitBtn;
 f:=a.FindComponent('AboutFooter') as TPanel;d:=a.FindComponent('AboutDetails') as TPanel;
 if (v.Glyph.Width=0) or (l.Glyph.Width=0) then Halt(1);
 if d.Visible then Halt(2);
 for i:=0 to 2 do begin
  case i of 0:a.Width:=1040;1:a.Width:=840;2:a.Width:=1280 end;
  a.Realign;a.OnResize(a);
  WriteLn('root ',a.ClientWidth,' align ',Ord(f.Align)); WriteLn('bounds ',i,' ',v.Left,',',v.Width,' legal ',l.Left,',',l.Width,' footer ',f.Width);
  if (v.Parent<>f) or (l.Parent<>f) or (v.Left+v.Width>=l.Left) then Halt(3);
  if l.Left+l.Width>f.ClientWidth then Halt(4);
  v.Click;if not a.StringGridVersionInfo.Visible or not d.Visible then Halt(5);
  l.Click;if a.StringGridVersionInfo.Visible or not a.StringGridCopyrights.Visible then Halt(6);
  l.Click;if d.Visible then Halt(7);
 end;
 a.Width:=1040;a.OnResize(a);a.HandleNeeded;
 b:=TBitmap.Create;b.SetSize(a.ClientWidth,a.ClientHeight);a.PaintTo(b.Canvas,0,0);
 b.SaveToFile(ExtractFilePath(ParamStr(0))+'about-redesign-hidden.bmp');
 WriteLn('PASS footer controls, icons, widths 840/1040/1280, exclusive panels, collapse');
 WriteLn('footer ',f.Left,',',f.Top,' ',f.Width,'x',f.Height,' credits ',a.StringGridCredits.Width,'x',a.StringGridCredits.Height);
 b.SetSize(a.StringGridCredits.Width,a.StringGridCredits.Height);
 oldHandle:=a.StringGridCredits.Canvas.Handle;
 a.StringGridCredits.Canvas.Handle:=b.Canvas.Handle;
 try
 for i:=0 to a.StringGridCredits.RowCount-1 do for j:=0 to 1 do begin
 r:=Rect(0,i*a.StringGridCredits.DefaultRowHeight,a.StringGridCredits.ColWidths[0],(i+1)*a.StringGridCredits.DefaultRowHeight);
 if j=1 then begin r.Left:=r.Right;r.Right:=r.Left+a.StringGridCredits.ColWidths[1] end;
 a.StringGridCredits.OnDrawCell(a.StringGridCredits,j,i,r,[]);end;
 finally a.StringGridCredits.Canvas.Handle:=oldHandle end;
 b.SaveToFile(ExtractFilePath(ParamStr(0))+'about-credits-render.bmp');
 b.Free;a.Free;
end.
