program NativeLinesplanProbe;
{$mode objfpc}{$H+}{$APPTYPE CONSOLE}
uses Windows, Interfaces, LMessages, Forms, SysUtils, Classes, Controls, Graphics, ExtCtrls, LCLTranslator,
  FreeShipUnit, FreeFileBuffer, FreeLinesplanFrm;
{$R ../FreeShip.res}
type
  TNativeChart=class(TFreeLinesplanForm)
  private
    Fault:TPanel;
    FaultTimer:TTimer;
    procedure EndFault(Sender:TObject);
  protected
    procedure WndProc(var Msg:TLMessage);override;
  end;
procedure TNativeChart.EndFault(Sender:TObject);
begin
  FaultTimer.Enabled:=False;Fault.Visible:=False;
end;
procedure TNativeChart.WndProc(var Msg:TLMessage);
begin
  if Msg.Msg=WM_APP+201 then begin
    if Fault=nil then begin
      Fault:=TPanel.Create(Self);Fault.Parent:=Self;Fault.Align:=alClient;
      Fault.BevelOuter:=bvNone;Fault.ParentBackground:=False;
      FaultTimer:=TTimer.Create(Self);FaultTimer.Interval:=160;
      FaultTimer.Enabled:=False;FaultTimer.OnTimer:=@EndFault;
    end;
    if Msg.WParam=1 then Fault.Color:=clWhite else Fault.Color:=clBlack;
    Fault.Visible:=True;Fault.BringToFront;FaultTimer.Enabled:=True;
    Msg.Result:=1;Exit;
  end;
  inherited WndProc(Msg);
end;
var Chart:TNativeChart;Backdrop:TForm;Ship:TFreeShip;Buffer:TFreeFileBuffer;Ready:TStringList;Frequency,Counter:Int64;
begin
  try
    Application.Initialize;
    if ParamStr(5)<>'' then SetDefaultLang(ParamStr(5),ExtractFilePath(ParamStr(0))+'locale','FreeShip',True);
    Ship:=TFreeShip.Create(nil);GlobalFreeShip:=Ship;
    Ship.Preferences.GlobalAppDataDirectory:=ExtractFilePath(ParamStr(0));
    Ship.Preferences.LoadTheme('StudioLight');
    Buffer:=TFreeFileBuffer.Create;
    try Buffer.LoadFromFile(ParamStr(1));Ship.LoadProject(Buffer) finally Buffer.Free end;
    Ship.RebuildModel;
    // Own the entire background visible through the native maximize animation.
    // No existing user window is hidden, moved, resized or captured.
    Backdrop:=TForm.CreateNew(nil);Backdrop.BorderStyle:=bsNone;
    Backdrop.Position:=poDesigned;Backdrop.Color:=RGB(112,112,112);
    Backdrop.SetBounds(Screen.PrimaryMonitor.WorkareaRect.Left,Screen.PrimaryMonitor.WorkareaRect.Top,
      Screen.PrimaryMonitor.WorkareaRect.Right-Screen.PrimaryMonitor.WorkareaRect.Left,
      Screen.PrimaryMonitor.WorkareaRect.Bottom-Screen.PrimaryMonitor.WorkareaRect.Top);
    Backdrop.ShowInTaskBar:=stNever;Backdrop.WindowState:=wsMaximized;Backdrop.Show;Backdrop.Update;
    Application.CreateForm(TNativeChart,Chart);
    Chart.LinesplanFrame.FreeShip:=Ship;
    Chart.Caption:=ParamStr(3);Chart.Position:=poDesigned;
    Chart.SetBounds(100,80,1422,900);
    Chart.PrepareForDisplay;Chart.Show;
    QueryPerformanceFrequency(Frequency);QueryPerformanceCounter(Counter);
    Ready:=TStringList.Create;
    try
      Ready.Text:='{"pid":'+IntToStr(GetCurrentProcessId)+',"hwnd":'+IntToStr(Chart.Handle)+
        ',"backdrop_hwnd":'+IntToStr(Backdrop.Handle)+
        ',"viewport_hwnd":'+IntToStr(Chart.Viewport.Handle)+',"tick_ms":'+IntToStr(GetTickCount64)+
        ',"qpc":'+IntToStr(Counter)+',"qpc_frequency":'+IntToStr(Frequency)+'}';
      Ready.SaveToFile(ParamStr(2));
    finally Ready.Free end;
    Application.Run;
  except
    on E:Exception do begin WriteLn(E.ClassName,': ',E.Message);Halt(1) end;
  end;
end.

