unit NativeAppCaptureProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure BeginNativeAppProbe(Form:TMainForm);
procedure ConfigureNativeAppProbe(Form:TMainForm);
implementation
uses Windows, Forms, Classes, SysUtils, ExtCtrls, Graphics, Controls;
type TIconController=class(TComponent)
 public
 Form:TMainForm; Timer:TTimer;Step:Integer;
 procedure Tick(Sender:TObject);
 procedure SaveIconImage(const AName:string;I:HICON);
end;
procedure TIconController.SaveIconImage(const AName:string;I:HICON);
var B:TBitmap;
begin
 B:=TBitmap.Create;try B.SetSize(32,32);B.Canvas.Brush.Color:=clWhite;B.Canvas.FillRect(Rect(0,0,32,32));
 if I<>0 then DrawIconEx(B.Canvas.Handle,0,0,I,32,32,0,0,DI_NORMAL);
 B.SaveToFile(ParamStr(2)+'.'+AName+'.bmp');finally B.Free end;
end;
procedure TIconController.Tick(Sender:TObject);
var S:TStringList;N:string;Reference:TIcon;
begin
 if not Form.Visible then Exit;
 Timer.Enabled:=False;N:=IntToStr(Step);
 Reference:=TIcon.Create;
 try Reference.LoadFromFile(ExtractFilePath(ParamStr(0))+'FreeShip.ico');SaveIconImage('expected',Reference.Handle);finally Reference.Free end;
 SaveIconImage('resource-main',LoadIcon(HInstance,'MAINICON'));
 SaveIconImage('main-class-'+N,HICON(GetClassLongPtr(Form.Handle,GCL_HICON)));
 SaveIconImage('app-class-'+N,HICON(GetClassLongPtr(Application.Handle,GCL_HICON)));
 SaveIconImage('main-big-'+N,HICON(SendMessage(Form.Handle,WM_GETICON,ICON_BIG,0)));
 SaveIconImage('main-small-'+N,HICON(SendMessage(Form.Handle,WM_GETICON,ICON_SMALL,0)));
 SaveIconImage('app-big-'+N,HICON(SendMessage(Application.Handle,WM_GETICON,ICON_BIG,0)));
 if Step=0 then begin
  Inc(Step);RecreateWnd(Form);Timer.Enabled:=True;Exit;
 end;
 S:=TStringList.Create;try S.Text:='{"pid":'+IntToStr(GetCurrentProcessId)+',"hwnd":'+IntToStr(Form.Handle)+'}';S.SaveToFile(ParamStr(2));finally S.Free end;
 Application.Terminate;
end;
procedure BeginNativeAppProbe(Form:TMainForm);
var C:TIconController;
begin C:=TIconController.Create(Application);C.Form:=Form;C.Timer:=TTimer.Create(C);C.Timer.Interval:=500;C.Timer.OnTimer:=@C.Tick end;
procedure ConfigureNativeAppProbe(Form:TMainForm);
var Root:string;
begin
 Root:=ExtractFilePath(ParamStr(0));
 with Form.FreeShip.Preferences do begin
  GlobalAppDataDirectory:=Root;GlobalOpenDirectory:=Root+'Ships';
  GlobalImportDirectory:=Root+'Import';ManualsDirectory:=Root+'Manuals';
  ExecDirectory:=Root+'Exec';LanguagesDirectory:=Root+'locale';
  OpenDirectory:=Root+'UserData/Ships';SaveDirectory:=Root+'UserData/Ships';
  ImportDirectory:=Root+'UserData/Import';ExportDirectory:=Root+'UserData/Export';
  TempDirectory:=GetEnvironmentVariable('TEMP');
 end;
end;
end.
