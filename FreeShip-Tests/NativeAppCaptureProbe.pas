unit NativeAppCaptureProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure BeginNativeAppProbe(Form:TMainForm);
procedure ConfigureNativeAppProbe(Form:TMainForm);
implementation
uses Windows, Forms, Classes, SysUtils, ExtCtrls, Controls, LCLTranslator,
  FreeLinesplanFrm, FreeStringsUnit;
type
 TNativeAppController=class(TComponent)
 private
  MainWindow:TMainForm;
  Timer:TTimer;
  ChartHWND:HWND;
  procedure Tick(Sender:TObject);
 public
  constructor CreateFor(Form:TMainForm);
 end;
constructor TNativeAppController.CreateFor(Form:TMainForm);
begin
 inherited Create(Application);MainWindow:=Form;
 Timer:=TTimer.Create(Self);Timer.Interval:=200;Timer.OnTimer:=@Tick;
end;
procedure TNativeAppController.Tick(Sender:TObject);
var Backdrop:TForm;Chart:TFreeLinesplanForm;I:Integer;Ready:TStringList;Counter,Frequency:Int64;
begin
 if ChartHWND<>0 then begin
  if not Windows.IsWindow(ChartHWND) then Application.Terminate;
  Exit;
 end;
 if not MainWindow.FreeShip.ModelIsLoaded then Exit;
 Timer.Enabled:=False;
 if ParamStr(5)<>'' then begin
  SetDefaultLang(ParamStr(5),ExtractFilePath(ParamStr(0))+'locale','FreeShip',True);
  SetUserStringsLang;MainWindow.RefreshStudioLanguage;
 end;
 Backdrop:=TForm.CreateNew(Application);Backdrop.BorderStyle:=bsNone;
 Backdrop.Color:=RGB(112,112,112);Backdrop.ShowInTaskBar:=stNever;
 Backdrop.WindowState:=wsMaximized;Backdrop.Show;Backdrop.Update;
 // This is the real, unchanged main-window action used by the toolbar.
 MainWindow.ShowLinesplanExecute(nil);
 Chart:=nil;
 for I:=0 to Screen.FormCount-1 do
  if (Screen.Forms[I] is TFreeLinesplanForm) and
     (TFreeLinesplanForm(Screen.Forms[I]).LinesplanFrame.FreeShip=MainWindow.FreeShip) then begin
    if Chart<>nil then raise Exception.Create('Ambiguous private linesplan window');
    Chart:=TFreeLinesplanForm(Screen.Forms[I]);
  end;
 if Chart=nil then raise Exception.Create('Main action did not create linesplan');
 Chart.Caption:=ParamStr(3);Chart.Position:=poDesigned;
 Chart.SetBounds(100,80,1422,900);Chart.PrepareForDisplay;Chart.Show;
 ChartHWND:=Chart.Handle;
 QueryPerformanceFrequency(Frequency);QueryPerformanceCounter(Counter);
 Ready:=TStringList.Create;
 try
  Ready.Text:='{"pid":'+IntToStr(GetCurrentProcessId)+',"hwnd":'+IntToStr(ChartHWND)+
   ',"backdrop_hwnd":'+IntToStr(Backdrop.Handle)+',"viewport_hwnd":'+IntToStr(Chart.Viewport.Handle)+
   ',"tick_ms":'+IntToStr(GetTickCount64)+',"qpc":'+IntToStr(Counter)+
   ',"qpc_frequency":'+IntToStr(Frequency)+',"actual_main_action":true}';
  Ready.SaveToFile(ParamStr(2));
 finally Ready.Free end;
 Timer.Enabled:=True;
end;
procedure BeginNativeAppProbe(Form:TMainForm);
begin TNativeAppController.CreateFor(Form) end;
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
