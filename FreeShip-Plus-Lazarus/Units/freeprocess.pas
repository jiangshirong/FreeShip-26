unit FreeProcess;

{$mode objfpc}{$H+}

interface

uses Classes, SysUtils, Process;

const
  DefaultFreePluginTimeoutMS = 120000;
  FreePluginTimeoutVariable = 'FREESHIP_PLUGIN_TIMEOUT_MS';
  FreePluginCaptureLimit = 1024 * 1024;

type
  TFreeProcessState = (fpsSuccess, fpsStartError, fpsExitError, fpsTimedOut,
    fpsCancelled, fpsIOError);
  TFreeProcessCancelCheck = function: Boolean of object;
  TFreeViewerCompleted = procedure(ExitCode: Integer) of object;
  TFreeProcessResult = record
    State: TFreeProcessState;
    ExitCode, ProcessID: Integer;
    OutputText, ErrorText, Detail: string;
    OutputBytes, ErrorBytes: Int64;
    OutputTruncated, ErrorTruncated: Boolean;
    ElapsedMS: QWord;
  end;
  EFreePluginError = class(Exception)
  private
    FState: TFreeProcessState;
    FExitCode: Integer;
  public
    constructor Create(const ExecutableName: string; const Run: TFreeProcessResult);
    property State: TFreeProcessState read FState;
    property ExitCode: Integer read FExitCode;
  end;

// The cancellation check must be non-blocking. No UI message pumping or reentry
// is performed while another process owns its calculation files.
function RunFreeProcess(const Directory, ExecutableName: string;
  const Arguments: array of string; TimeoutMS: Cardinal = 0;
  CancelCheck: TFreeProcessCancelCheck = nil): TFreeProcessResult;
// Existing callers ignore the Integer result, so errors must raise before they
// proceed to parse result files left by an unsuccessful calculation.
function ExecuteFreePlugin(TempDirectory, ExecFullName: string;
  TimeoutMS: Cardinal = 0;
  CancelCheck: TFreeProcessCancelCheck = nil): Integer;

// Interactive database viewers do not have the calculation deadline. Their
// completion callback is cancelled when the owning form is destroyed.
function LaunchFreePluginViewer(Owner: TComponent; const Directory, ExecutableName: string;
  OnCompleted: TFreeViewerCompleted = nil): Integer;

implementation

uses Pipes, FreeLogger, AsyncProcess, Forms;

type
  TFreeViewerProcess = class(TAsyncProcess)
  private
    FCompleted: TFreeViewerCompleted;
    procedure Finished(Sender: TObject);
    procedure ReleaseProcess(Data: PtrInt);
  public
    destructor Destroy; override;
  end;

procedure TFreeViewerProcess.ReleaseProcess(Data: PtrInt);
begin Free end;

destructor TFreeViewerProcess.Destroy;
begin
  Application.RemoveAsyncCalls(Self);
  inherited Destroy;
end;

procedure TFreeViewerProcess.Finished(Sender: TObject);
var Completed: TFreeViewerCompleted; Code: Integer;
begin
  Completed := FCompleted;
  Code := ExitStatus;
  Application.QueueAsyncCall(@ReleaseProcess, 0);
  if Assigned(Completed) then Completed(Code);
end;

function LaunchFreePluginViewer(Owner: TComponent; const Directory, ExecutableName: string;
  OnCompleted: TFreeViewerCompleted): Integer;
var Viewer: TFreeViewerProcess;
begin
  Viewer := TFreeViewerProcess.Create(Owner);
  try
    Viewer.Executable := ExecutableName;
    Viewer.CurrentDirectory := Directory;
    Viewer.Options := [];
    Viewer.FCompleted := OnCompleted;
    Viewer.OnTerminate := @Viewer.Finished;
    Viewer.Execute;
    Result := Viewer.ProcessID;
  except
    Viewer.Free;
    raise;
  end;
end;


resourcestring
  rsPluginStartFailed = 'Cannot start external calculation';
  rsPluginExitFailed = 'External calculation failed with exit code %d';
  rsPluginTimeout = 'External calculation exceeded the time limit';
  rsPluginCancelled = 'External calculation was cancelled';
  rsPluginIOFailed = 'Cannot collect external calculation output';

constructor EFreePluginError.Create(const ExecutableName: string;
  const Run: TFreeProcessResult);
var Reason: string;
begin
  FState := Run.State;
  FExitCode := Run.ExitCode;
  case Run.State of
    fpsStartError: Reason := rsPluginStartFailed;
    fpsExitError: Reason := Format(rsPluginExitFailed, [Run.ExitCode]);
    fpsTimedOut: Reason := rsPluginTimeout + ' (' + IntToStr(Run.ElapsedMS) +
      ' ms; ' + FreePluginTimeoutVariable + ')';
    fpsCancelled: Reason := rsPluginCancelled;
    else Reason := rsPluginIOFailed;
  end;
  Reason := Reason + ': ' + ExecutableName;
  if Run.Detail <> '' then Reason := Reason + LineEnding + Run.Detail;
  // Keep the user-facing exception bounded even for an excessively noisy child.
  if Run.ErrorText <> '' then Reason := Reason + LineEnding + Copy(Run.ErrorText, 1, 4096);
  inherited Create(Reason);
end;

function RunFreeProcess(const Directory, ExecutableName: string;
  const Arguments: array of string; TimeoutMS: Cardinal;
  CancelCheck: TFreeProcessCancelCheck): TFreeProcessResult;
const
  ReadChunkSize = 16384;
var
  Child: TProcess;
  Started: Boolean;
  StartTick: QWord;
  I: Integer;
  ConfiguredTimeout: Int64;
  TimeoutText: string;
  HadOutput, HadError: Boolean;

  function DrainChunk(Stream: TInputPipeStream; var Text: string;
    var Total: Int64; var Truncated: Boolean): Boolean;
  var Available, ReadCount, Keep, OldLength: Integer;
      Bytes: array[0..ReadChunkSize - 1] of Byte;
  begin
    Result := False;
    if Stream = nil then Exit;
    Available := Stream.NumBytesAvailable;
    if Available <= 0 then Exit;
    if Available > SizeOf(Bytes) then Available := SizeOf(Bytes);
    ReadCount := Stream.Read(Bytes[0], Available);
    if (ReadCount <= 0) or (ReadCount > Available) then
      raise EReadError.Create('External calculation pipe made no progress');
    Inc(Total, ReadCount);
    OldLength := Length(Text);
    Keep := FreePluginCaptureLimit - OldLength;
    if Keep > ReadCount then Keep := ReadCount;
    if Keep > 0 then
    begin
      SetLength(Text, OldLength + Keep);
      Move(Bytes[0], Text[OldLength + 1], Keep);
    end;
    if Keep < ReadCount then Truncated := True;
    Result := True;
  end;

  procedure StopChild;
  begin
    if Started and Child.Running then
    begin
      if not Child.Terminate(1) then
        Result.Detail := Result.Detail + LineEnding + 'Could not terminate external calculation';
      if not Child.WaitOnExit(1000) then
        Result.Detail := Result.Detail + LineEnding + 'External calculation did not confirm termination';
    end;
  end;

begin
  Result.State := fpsStartError;
  Result.ExitCode := -1;
  Result.ProcessID := 0;
  Result.OutputText := '';
  Result.ErrorText := '';
  Result.Detail := '';
  Result.OutputBytes := 0;
  Result.ErrorBytes := 0;
  Result.OutputTruncated := False;
  Result.ErrorTruncated := False;
  Result.ElapsedMS := 0;
  StartTick := GetTickCount64;
  Started := False;
  Child := TProcess.Create(nil);
  try
    try
      if TimeoutMS = 0 then
      begin
        TimeoutText := GetEnvironmentVariable(FreePluginTimeoutVariable);
        if TimeoutText = '' then TimeoutMS := DefaultFreePluginTimeoutMS
        else
        begin
          if not TryStrToInt64(TimeoutText, ConfiguredTimeout) or
             (ConfiguredTimeout < 1) or (ConfiguredTimeout > High(Cardinal)) then
            raise EConvertError.Create('Invalid ' + FreePluginTimeoutVariable + ': ' + TimeoutText);
          TimeoutMS := Cardinal(ConfiguredTimeout);
        end;
      end;
      if Assigned(CancelCheck) and CancelCheck() then
      begin
        Result.State := fpsCancelled;
        Exit;
      end;
      Child.Executable := ExecutableName;
      Child.CurrentDirectory := Directory;
      Child.Options := [poUsePipes, poNoConsole];
      Child.PipeBufferSize := 65536;
      for I := Low(Arguments) to High(Arguments) do Child.Parameters.Add(Arguments[I]);
      Child.Execute;
      Started := True;
      Result.ProcessID := Child.ProcessID;
      // Plugins use files, not an interactive stdin session. Closing stdin also
      // prevents a child waiting for input from keeping the application blocked.
      Child.CloseInput;
      Result.State := fpsIOError;
      while True do
      begin
        if Assigned(CancelCheck) and CancelCheck() then
        begin
          Result.State := fpsCancelled;
          Break;
        end;
        if GetTickCount64 - StartTick >= TimeoutMS then
        begin
          Result.State := fpsTimedOut;
          Break;
        end;
        // Drain both pipes on every iteration. A busy stdout must not starve
        // stderr, and retained output is bounded without stopping pipe reads.
        HadOutput := DrainChunk(Child.Output, Result.OutputText,
          Result.OutputBytes, Result.OutputTruncated);
        HadError := DrainChunk(Child.Stderr, Result.ErrorText,
          Result.ErrorBytes, Result.ErrorTruncated);
        if not Child.Running and not HadOutput and not HadError then
        begin
          Result.ExitCode := Child.ExitStatus;
          if Result.ExitCode = 0 then Result.State := fpsSuccess
          else Result.State := fpsExitError;
          Break;
        end;
        if not HadOutput and not HadError then Sleep(1);
      end;
      if Result.State in [fpsTimedOut, fpsCancelled] then StopChild;
    except
      on E: Exception do
      begin
        if Started then Result.State := fpsIOError else Result.State := fpsStartError;
        Result.Detail := E.ClassName + ': ' + E.Message;
        StopChild;
      end;
    end;
  finally
    Result.ElapsedMS := GetTickCount64 - StartTick;
    Child.Free;
  end;
end;

function ExecuteFreePlugin(TempDirectory, ExecFullName: string;
  TimeoutMS: Cardinal; CancelCheck: TFreeProcessCancelCheck): Integer;
var Run: TFreeProcessResult;
begin
  Run := RunFreeProcess(TempDirectory, ExecFullName, [], TimeoutMS, CancelCheck);
  Logger.Info('Output of ' + ExecFullName);
  Logger.Info(Run.OutputText);
  Logger.Info('Error: ' + Run.ErrorText);
  Logger.Info('ExitStatus ' + IntToStr(Run.ExitCode));
  if Run.State <> fpsSuccess then raise EFreePluginError.Create(ExecFullName, Run);
  Result := 0;
end;

end.
