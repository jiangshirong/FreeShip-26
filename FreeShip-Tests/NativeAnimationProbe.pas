unit NativeAppCaptureProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure BeginNativeAppProbe(Form:TMainForm);
procedure ConfigureNativeAppProbe(Form:TMainForm);
implementation
uses Windows,Forms,Classes,SysUtils,ExtCtrls,Controls,UiAnimationProfile,FreeGeometry,freehullformwindow_panel;
type TAnimationController=class(TComponent)
 public
 MainWindow:TMainForm;Timer:TTimer;Ready:Boolean;Started,Frequency:Int64;
 DrawHandlers:array[0..3] of TNotifyEvent;Rows:TStringList;
 procedure Failed(Sender:TObject;E:Exception);
 procedure Tick(Sender:TObject);
 procedure Draw(Sender:TObject);
 constructor CreateFor(Form:TMainForm);
end;
constructor TAnimationController.CreateFor(Form:TMainForm);
begin inherited Create(Application);MainWindow:=Form;Rows:=TStringList.Create;Application.OnException:=@Failed;
 QueryPerformanceFrequency(Frequency);Timer:=TTimer.Create(Self);Timer.Interval:=25;Timer.OnTimer:=@Tick end;
procedure TAnimationController.Failed(Sender:TObject;E:Exception);
var L:TStringList;begin L:=TStringList.Create;L.Text:=E.ClassName+': '+E.Message;L.SaveToFile(ParamStr(2)+'.error');L.Free;Application.Terminate end;
procedure TAnimationController.Draw(Sender:TObject);
var I:Integer;A,B:Int64;
begin
 for I:=0 to 3 do if Sender=MainWindow.GetMDIChildren(I).Viewport then begin
 QueryPerformanceCounter(A);DrawHandlers[I](Sender);QueryPerformanceCounter(B);
 if Started<>0 then Rows.Add(IntToStr(I)+','+IntToStr(A)+','+IntToStr(B-A));Exit end;
end;
procedure TAnimationController.Tick(Sender:TObject);
var I:Integer;Info:TStringList;NowTick:Int64;
begin
 if not Ready then begin
 if not MainWindow.FreeShip.ModelIsLoaded or (MainWindow.MDIChildCount<>4) then Exit;
 MainWindow.Caption:=ParamStr(3);MainWindow.WindowState:=wsMaximized;
 for I:=0 to 3 do begin MainWindow.GetMDIChildren(I).Viewport.FinishProgressiveDisplay(True);
 DrawHandlers[I]:=MainWindow.GetMDIChildren(I).Viewport.OnRedraw;
 MainWindow.GetMDIChildren(I).Viewport.OnRedraw:=@Draw end;
 Info:=TStringList.Create;Info.Text:='{"pid":'+IntToStr(GetCurrentProcessId)+',"hwnd":'+IntToStr(MainWindow.Handle)+',"frequency":'+IntToStr(Frequency)+',"viewports":[';
 for I:=0 to 3 do begin if I>0 then Info.Text:=Trim(Info.Text)+',';Info.Text:=Trim(Info.Text)+IntToStr(MainWindow.GetMDIChildren(I).Viewport.Handle) end;
 Info.Text:=Trim(Info.Text)+']}';Info.SaveToFile(ParamStr(2));Info.Free;Ready:=True;Exit end;
 if Started=0 then begin
 if not FileExists(ParamStr(2)+'.start') then Exit;
 QueryPerformanceCounter(Started);ProfileRows.Clear;ProfileEnabled:=True;
 for I:=0 to 3 do MainWindow.GetMDIChildren(I).Viewport.FinishProgressiveDisplay(False);
 Exit end;
 QueryPerformanceCounter(NowTick);
 if (NowTick-Started)/Frequency>4.5 then begin
 ProfileEnabled:=False;ProfileRows.SaveToFile(ParamStr(2)+'.profile');
 Timer.Enabled:=False;Rows.SaveToFile(ParamStr(2)+'.frames');Rows.Free;
 Info:=TStringList.Create;Info.Text:=IntToStr(Started);Info.SaveToFile(ParamStr(2)+'.done');Info.Free;
 end;
end;
procedure BeginNativeAppProbe(Form:TMainForm);
begin TAnimationController.CreateFor(Form) end;
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
