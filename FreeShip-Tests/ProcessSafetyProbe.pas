program ProcessSafetyProbe;
{$mode objfpc}{$H+}
uses Interfaces, Classes, SysUtils, Windows, Forms, fpjson, FreeProcess;

type
  TCanceller = class
    StartTick: QWord;
    DelayMS: Cardinal;
    function Check: Boolean;
  end;
type
  TViewerWitness = class
    Count, Code: Integer;
    procedure Done(ExitCode: Integer);
  end;
procedure TViewerWitness.Done(ExitCode: Integer);
begin Inc(Count); Code := ExitCode end;
function TCanceller.Check: Boolean;
begin Result := GetTickCount64 - StartTick >= DelayMS end;

var Cases: TJSONArray; Failures: Integer;
procedure Check(const ID, Expected: string; OK: Boolean; const Detail: string = '');
var O: TJSONObject;
begin
  O := TJSONObject.Create; O.Add('id', ID); O.Add('expected', Expected);
  if OK then O.Add('status', 'pass') else begin O.Add('status','fail'); Inc(Failures) end;
  O.Add('actual', Detail); Cases.Add(O);
end;
function Ended(PID: Integer): Boolean;
var H: THandle;
begin
  if PID <= 0 then Exit(False);
  H := OpenProcess(SYNCHRONIZE, False, PID);
  if H = 0 then Exit(GetLastError = ERROR_INVALID_PARAMETER);
  try Result := WaitForSingleObject(H, 0) = WAIT_OBJECT_0 finally CloseHandle(H) end;
end;
procedure ViewerCases(const Directory: string);
var Owner: TComponent; Witness: TViewerWitness; PID: Integer; Started: QWord;
    BeforeDirectory: string; Raised: Boolean;
begin
  BeforeDirectory := GetCurrentDir;
  Owner := TComponent.Create(nil); Witness := TViewerWitness.Create;
  try
    Started := GetTickCount64;
    PID := LaunchFreePluginViewer(Owner, Directory,
      IncludeTrailingPathDelimiter(Directory)+'process-viewer.exe', @Witness.Done);
    Check('11.viewer.nonblocking','interactive viewer starts without blocking for its lifetime',
      (GetTickCount64-Started<250) and not Ended(PID));
    while (GetTickCount64-Started<3000) and (Witness.Count=0) do
    begin Application.ProcessMessages; Sleep(1) end;
    Application.ProcessMessages;
    Check('11.viewer.completion','completion occurs once after successful exit',
      (Witness.Count=1) and (Witness.Code=0) and Ended(PID));
    Check('11.viewer.cwd','viewer launch leaves parent directory unchanged',GetCurrentDir=BeforeDirectory);
    FreeAndNil(Owner); Owner:=TComponent.Create(nil); Witness.Count:=0;
    PID:=LaunchFreePluginViewer(Owner,Directory,
      IncludeTrailingPathDelimiter(Directory)+'process-viewer.exe',@Witness.Done);
    FreeAndNil(Owner); Started:=GetTickCount64;
    while (GetTickCount64-Started<3000) and not Ended(PID) do
    begin Application.ProcessMessages; Sleep(1) end;
    Application.ProcessMessages;
    Check('11.viewer.owner-lifetime','destroyed owner receives no late callback',
      (Witness.Count=0) and Ended(PID));
    Owner:=TComponent.Create(nil); Raised:=False;
    try LaunchFreePluginViewer(Owner,Directory,IncludeTrailingPathDelimiter(Directory)+'missing-viewer.exe');
    except on E:Exception do Raised:=True end;
    Check('11.viewer.start-failure','viewer start failure releases its owned process component',
      Raised and (Owner.ComponentCount=0));
  finally Owner.Free; Witness.Free end;
end;

procedure Run;
var Directory, Helper, BeforeDirectory, OldConfig: string;
    R: TFreeProcessResult; Cancel: TCanceller;
    ConsumerReached, Caught: Boolean; S: Integer;
begin
  Directory := ParamStr(1); Helper := IncludeTrailingPathDelimiter(Directory)+'process-helper.exe';
  BeforeDirectory := GetCurrentDir;
  OldConfig := SysUtils.GetEnvironmentVariable(FreePluginTimeoutVariable);
  Windows.SetEnvironmentVariable(PChar(FreePluginTimeoutVariable), nil);
  try
    R := RunFreeProcess(Directory, Helper, ['success'], 3000);
    Check('11.success.state','zero exit is success', (R.State=fpsSuccess) and (R.ExitCode=0));
    Check('11.success.output','stdout is fully captured', Trim(R.OutputText)='ok', R.OutputText);
    Check('11.success.exited','successful child no longer running',Ended(R.ProcessID));
    R := RunFreeProcess(IncludeTrailingPathDelimiter(Directory)+'中文 Женя',
      IncludeTrailingPathDelimiter(Directory)+'中文 Женя'+DirectorySeparator+'计算 helper.exe', ['success'], 3000);
    Check('11.unicode-path','Chinese/Russian directories and executable spaces launch correctly',R.State=fpsSuccess,R.Detail);
    R := RunFreeProcess(Directory, Helper, ['fail'], 3000);
    Check('11.nonzero.state','exit 7 is an explicit failure', (R.State=fpsExitError) and (R.ExitCode=7),IntToStr(R.ExitCode));
    Check('11.nonzero.stderr','failure stderr is captured',Pos('deliberate failure',R.ErrorText)>0,R.ErrorText);
    R := RunFreeProcess(Directory, IncludeTrailingPathDelimiter(Directory)+'missing.exe', [], 3000);
    Check('11.missing.state','missing executable is a start error', (R.State=fpsStartError) and (R.ProcessID=0) and (R.Detail<>''),R.Detail);
    R := RunFreeProcess(Directory+'\missing-directory', Helper, [], 3000);
    Check('11.invalid-directory.state','invalid work directory is a start error',R.State=fpsStartError,R.Detail);
    R := RunFreeProcess(Directory, Helper, ['flood'], 10000);
    Check('11.flood.exits','simultaneously busy stdout/stderr terminate normally',R.State=fpsSuccess,R.Detail);
    Check('11.flood.drain','both pipes are fully drained', (R.OutputBytes=2097152) and (R.ErrorBytes=2097152),IntToStr(R.OutputBytes)+'/'+IntToStr(R.ErrorBytes));
    Check('11.flood.bounded','retained output bounded separately with explicit truncation',
      (Length(R.OutputText)=FreePluginCaptureLimit) and (Length(R.ErrorText)=FreePluginCaptureLimit) and R.OutputTruncated and R.ErrorTruncated);
    R := RunFreeProcess(Directory, Helper, ['wait'], 150);
    Check('11.timeout.state','hanging child reports timeout',R.State=fpsTimedOut);
    Check('11.timeout.bounded','configured timeout returns within 2 seconds',R.ElapsedMS<2000,IntToStr(R.ElapsedMS));
    Check('11.timeout.terminated','timed-out child is terminated',Ended(R.ProcessID));
    Check('11.timeout.partial-output','diagnostic output before timeout is kept',Pos('waiting',R.OutputText)>0,R.OutputText);
    Cancel := TCanceller.Create;
    try
      Cancel.StartTick := GetTickCount64; Cancel.DelayMS:=100;
      R := RunFreeProcess(Directory, Helper, ['wait'], 3000, @Cancel.Check);
      Check('11.cancel.state','callback cancellation is distinguishable from timeout',R.State=fpsCancelled);
      Check('11.cancel.terminated','cancelled child is terminated',Ended(R.ProcessID));
      Cancel.StartTick := GetTickCount64; Cancel.DelayMS:=0;
      R := RunFreeProcess(Directory, Helper, ['wait'], 3000, @Cancel.Check);
      Check('11.cancel.before-start','cancellation before launch starts no child', (R.State=fpsCancelled) and (R.ProcessID=0));
    finally Cancel.Free end;
    R := RunFreeProcess(Directory, Helper, ['stdin'], 3000);
    Check('11.stdin.closed','file-based plugin receives EOF on stdin', (R.State=fpsSuccess) and (Trim(R.OutputText)='input closed'),R.OutputText);
    Windows.SetEnvironmentVariable(PChar(FreePluginTimeoutVariable),'150');
    R := RunFreeProcess(Directory, Helper, ['wait']);
    Check('11.timeout.config','environment config controls unchanged call sites', (R.State=fpsTimedOut) and (R.ElapsedMS<2000));
    Windows.SetEnvironmentVariable(PChar(FreePluginTimeoutVariable),'bad');
    R := RunFreeProcess(Directory, Helper, ['success']);
    Check('11.timeout.invalid-config','invalid limit is reported before launch', (R.State=fpsStartError) and (R.ProcessID=0) and (Pos(FreePluginTimeoutVariable,R.Detail)>0));
    Windows.SetEnvironmentVariable(PChar(FreePluginTimeoutVariable),nil);
    ConsumerReached:=False; Caught:=False;
    try
      ExecuteFreePlugin(Directory,IncludeTrailingPathDelimiter(Directory)+'process-fail.exe',3000);
      ConsumerReached:=True;
    except on E:EFreePluginError do Caught:=(E.State=fpsExitError) and (E.ExitCode=7) end;
    Check('11.wrapper.prevent-consumption','legacy callers cannot read output after failure',Caught and not ConsumerReached);
    Caught:=False;
    try ExecuteFreePlugin(Directory,IncludeTrailingPathDelimiter(Directory)+'process-wait.exe',150);
    except on E:EFreePluginError do Caught:=(E.State=fpsTimedOut) and (Pos(FreePluginTimeoutVariable,E.Message)>0) end;
    Check('11.wrapper.timeout-detail','timeout exception names configurable limit',Caught);
    S:=ExecuteFreePlugin(Directory,Helper,3000);
    Check('11.wrapper.success','legacy successful wrapper still returns zero',S=0);
    ViewerCases(Directory);
    Check('11.cwd-preserved','runner never changes the parent working directory',GetCurrentDir=BeforeDirectory);
  finally Windows.SetEnvironmentVariable(PChar(FreePluginTimeoutVariable),PChar(OldConfig)) end;
end;
var Report:TJSONObject; Text:TStringList;
begin
  Cases:=TJSONArray.Create; Report:=TJSONObject.Create; Report.Add('cases',Cases);
  try
    try Run except on E:Exception do Report.Add('error',E.ClassName+': '+E.Message) end;
    Text:=TStringList.Create;
    try Text.Text:=Report.FormatJSON; Text.SaveToFile(ParamStr(2)) finally Text.Free end;
  finally Report.Free end;
  if Failures>0 then Halt(1);
end.
