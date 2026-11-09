unit FreeSafeFile;

{$mode objfpc}{$H+}

interface

uses Classes, SysUtils, FreeFileBuffer;

// Writes and closes a complete sibling file before replacing the public name.
procedure SaveProjectFile(Buffer: TFreeFileBuffer; const FileName: string);
procedure ReplaceFileAtomically(Stream: TStream; const FileName: string);

implementation

uses
  {$IFDEF MSWINDOWS}Windows{$ELSE}BaseUnix{$ENDIF};

type
  TSiblingFile = class
  private
    FName: string;
    FHandle: THandle;
    FStream: THandleStream;
  public
    constructor Create(const Target: string);
    destructor Destroy; override;
    procedure Seal;
    procedure Install(const Target: string; ReplaceExisting: Boolean);
    property Stream: THandleStream read FStream;
  end;

procedure FileError(const Operation, Name: string; ErrorCode: Integer);
begin
  raise EInOutError.Create(Operation + ': ' + Name + LineEnding +
    SysErrorMessage(ErrorCode));
end;

function LastFileError: Integer;
begin
  {$IFDEF MSWINDOWS}Result := Windows.GetLastError;
  {$ELSE}Result := fpGetErrNo;{$ENDIF}
end;

constructor TSiblingFile.Create(const Target: string);
var
  Token: TGuid;
  Attempt, Code: Integer;
  Candidate: string;
begin
  inherited Create;
  FHandle := THandle(-1);
  for Attempt := 1 to 20 do
  begin
    if CreateGUID(Token) <> 0 then
      raise EInOutError.Create('Cannot create a temporary file name');
    Candidate := ExtractFilePath(Target) + '.' + ExtractFileName(Target) +
      '.' + GUIDToString(Token) + '.tmp';
    {$IFDEF MSWINDOWS}
    FHandle := CreateFileW(PWideChar(UTF8Decode(Candidate)), GENERIC_WRITE,
      0, nil, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    {$ELSE}
    FHandle := fpOpen(PChar(Candidate), O_WRONLY or O_CREAT or O_EXCL, &600);
    {$ENDIF}
    if FHandle <> THandle(-1) then
    begin
      FName := Candidate;
      FStream := THandleStream.Create(FHandle);
      Exit;
    end;
    Code := LastFileError;
    {$IFDEF MSWINDOWS}
    if (Code <> ERROR_FILE_EXISTS) and (Code <> ERROR_ALREADY_EXISTS) then
    {$ELSE}
    if Code <> ESysEEXIST then
    {$ENDIF}
      FileError('Cannot create temporary file', Candidate, Code);
  end;
  raise EInOutError.Create('Cannot reserve a unique temporary file: ' + Target);
end;

destructor TSiblingFile.Destroy;
begin
  FStream.Free;
  if FHandle <> THandle(-1) then
    {$IFDEF MSWINDOWS}CloseHandle(FHandle){$ELSE}fpClose(FHandle){$ENDIF};
  if FName <> '' then
    {$IFDEF MSWINDOWS}DeleteFileW(PWideChar(UTF8Decode(FName)))
    {$ELSE}fpUnlink(PChar(FName)){$ENDIF};
  inherited Destroy;
end;

procedure TSiblingFile.Seal;
var Code: Integer;
begin
  {$IFDEF MSWINDOWS}
  if not FlushFileBuffers(FHandle) then
  {$ELSE}
  if fpFsync(FHandle) <> 0 then
  {$ENDIF}
    FileError('Cannot flush temporary file', FName, LastFileError);
  FreeAndNil(FStream);
  {$IFDEF MSWINDOWS}
  if not CloseHandle(FHandle) then
  {$ELSE}
  if fpClose(FHandle) <> 0 then
  {$ENDIF}
  begin
    Code := LastFileError;
    FHandle := THandle(-1);
    FileError('Cannot close temporary file', FName, Code);
  end;
  FHandle := THandle(-1);
end;

procedure TSiblingFile.Install(const Target: string; ReplaceExisting: Boolean);
{$IFDEF MSWINDOWS}
const MoveFileWriteThrough = $00000008; // Missing from older FPC Windows headers.
var Flags: DWORD;
{$ENDIF}
begin
  if FHandle <> THandle(-1) then
    raise EInOutError.Create('Temporary file is still open: ' + FName);
  {$IFDEF MSWINDOWS}
  Flags := MoveFileWriteThrough;
  if ReplaceExisting then Flags := Flags or MOVEFILE_REPLACE_EXISTING;
  if not MoveFileExW(PWideChar(UTF8Decode(FName)),
      PWideChar(UTF8Decode(Target)), Flags) then
    FileError('Cannot replace file', Target, LastFileError);
  {$ELSE}
  if ReplaceExisting then
  begin
    if fpRename(PChar(FName), PChar(Target)) <> 0 then
      FileError('Cannot replace file', Target, LastFileError);
  end
  else
  begin
    // link refuses to overwrite a file created since the initial existence test.
    if fpLink(PChar(FName), PChar(Target)) <> 0 then
      FileError('Cannot create file', Target, LastFileError);
    fpUnlink(PChar(FName));
  end;
  {$ENDIF}
  FName := '';
end;

procedure SaveProjectFile(Buffer: TFreeFileBuffer; const FileName: string);
var
  Pending, Backup: TSiblingFile;
  Original: THandleStream;
  OriginalHandle: THandle;
  Target: string;
  HadOriginal: Boolean;
begin
  Target := ExpandFileName(FileName);
  {$IFDEF MSWINDOWS}
  HadOriginal := GetFileAttributesW(PWideChar(UTF8Decode(Target))) <> INVALID_FILE_ATTRIBUTES;
  {$ELSE}HadOriginal := FileExists(Target);{$ENDIF}
  Pending := TSiblingFile.Create(Target);
  try
    Buffer.SaveToStream(Pending.Stream);
    Pending.Seal;
    if HadOriginal then
    begin
      Backup := TSiblingFile.Create(ChangeFileExt(Target, '.Bak'));
      try
        {$IFDEF MSWINDOWS}
        OriginalHandle := CreateFileW(PWideChar(UTF8Decode(Target)), GENERIC_READ,
          FILE_SHARE_READ, nil, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        {$ELSE}OriginalHandle := fpOpen(PChar(Target), O_RDONLY);{$ENDIF}
        if OriginalHandle = THandle(-1) then
          FileError('Cannot read original file for backup', Target, LastFileError);
        try
          Original := THandleStream.Create(OriginalHandle);
          try
            Backup.Stream.CopyFrom(Original, 0);
          finally
            Original.Free;
          end;
        finally
          {$IFDEF MSWINDOWS}CloseHandle(OriginalHandle)
          {$ELSE}fpClose(OriginalHandle){$ENDIF};
        end;
        Backup.Seal;
        Backup.Install(ChangeFileExt(Target, '.Bak'), True);
      finally
        Backup.Free;
      end;
    end;
    Pending.Install(Target, HadOriginal);
  finally
    Pending.Free;
  end;
end;

procedure ReplaceFileAtomically(Stream: TStream; const FileName: string);
var Pending: TSiblingFile;
begin
  Pending := TSiblingFile.Create(ExpandFileName(FileName));
  try
    Pending.Stream.CopyFrom(Stream, 0);
    Pending.Seal;
    Pending.Install(ExpandFileName(FileName), True);
  finally Pending.Free end;
end;

end.
