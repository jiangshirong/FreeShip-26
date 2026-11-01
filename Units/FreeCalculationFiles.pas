unit FreeCalculationFiles;

{$mode objfpc}{$H+}

interface

uses Classes, SysUtils, FreeProcess;

type
  TFreeCalculationProtocol = (fcAddedMass, fcManeuv, fcRvrs, fcIshercof,
    fcPropeller1, fcPropeller2, fcPropeller3, fcPropeller4, fcPropol, fcFppcalc,
    fcFung, fcHollen, fcSeaMargin, fcPowerPrediction, fcHship, fcOortmer,
    fcClemPoup, fcClemBlou, fcBunkov, fcCompton, fcWolfson, fcRadojcic);
  EFreeCalculationOutput = class(EInOutError);
  TFreeCalculationScope = class
  private
    FRoot, FDirectory, FPreviousDirectory: string;
    FLock: TFileStream;
    FOwned: Boolean;
    FPreviousCursor: Integer;
    procedure PublishFiles;
  public
    constructor Create(const Root, ExecutableDirectory: string;
      const PublishedOutputs: array of string);
    destructor Destroy; override;
    procedure Complete;
    property Directory: string read FDirectory;
  end;

// Outside a calculation this resolves an atomically published generation.
// During one it returns its private working directory. Preferences are unchanged.
function FreeCalculationDirectory(const Root: string): string;
function ExecuteFreeCalculation(const Directory, ExecutableName: string;
  Protocol: TFreeCalculationProtocol; TimeoutMS: Cardinal = 0): Integer;
procedure ValidateFreeCalculationOutput(const Directory: string;
  Protocol: TFreeCalculationProtocol);

implementation

uses Math, StrUtils, FileUtil, LazFileUtils, SHA1, FreeSafeFile, FreeLogger, Forms;

const
  SharedNames: array[0..2] of string = ('RESIST.dat', 'RESISTp.dat', 'Propres.dat');
  Inputs: array[TFreeCalculationProtocol] of string = (
    'INA.', 'INM.', 'TMP4.tsk', 'INO.', 'TMP1.tsk', 'TMP2.tsk', 'TMP3.tsk',
    'INO.', 'INO.', 'INO.', 'fungdata.dat', 'TMP5.tsk', 'TMPke.txt', 'IN.',
    'ostdat.tmp', 'TMP6.tsk', 'clempoup.dat', 'clemblou.dat', 'bunkdata.dat',
    'cmptdata.dat', 'wolfdata.dat', 'radodata.dat');
  Outputs: array[TFreeCalculationProtocol] of string = (
    'OUTA.', 'OUT.', 'RVRSRES.dat', 'OUT.', 'RES1.tsk', 'RES2.tsk', 'RES3.tsk',
    'OUT.', 'OUT.', 'OUT.', 'fungleib.res', 'hollenba.res', 'OUT.TXT', 'OUT.',
    'ostres.tmp', 'oortmers.res', 'clempoup.res', 'clemblou.res', 'bunkov00.res',
    'Compton0.res', 'Wolfson0.res', 'Radojcic.res');
  MaxResultBytes = 2 * 1024 * 1024;

var ActiveScope: TFreeCalculationScope;

function ChildPath(const Dir, Name: string): string;
begin Result := IncludeTrailingPathDelimiter(Dir) + Name end;

function UniqueDirectory(const Root, Prefix: string): string;
var ID: TGuid;
begin
  if not ForceDirectoriesUTF8(Root) then
    raise EInOutError.Create('Cannot create calculation directory: ' + Root);
  if CreateGUID(ID) <> 0 then raise EInOutError.Create('Cannot create calculation ID');
  Result := ChildPath(Root, Prefix + GUIDToString(ID));
  if not CreateDirUTF8(Result) then
    raise EInOutError.Create('Cannot reserve calculation directory: ' + Result);
end;

procedure CopyRequired(const Source, Target: string);
begin
  if not FileUtil.CopyFile(Source, Target) then
    raise EInOutError.Create('Cannot copy calculation file: ' + Source);
end;

function ReadLines(const Name: string): TStringList;
var S: TFileStream;
begin
  Result := TStringList.Create;
  try
    if not FileExistsUTF8(Name) then raise EFreeCalculationOutput.Create('Missing calculation output: ' + Name);
    S := TFileStream.Create(Name, fmOpenRead or fmShareDenyWrite);
    try
      if (S.Size = 0) or (S.Size > MaxResultBytes) then
        raise EFreeCalculationOutput.Create('Invalid calculation output size: ' + Name);
      Result.LoadFromStream(S);
    finally S.Free end;
  except Result.Free; raise end;
end;

function PublishedDirectory(const Root: string): string;
var L: TStringList; Token: string;
begin
  Result := Root;
  if not FileExistsUTF8(ChildPath(Root, 'current')) then Exit;
  L := ReadLines(ChildPath(Root, 'current'));
  try
    Token := Trim(L.Text);
    if (Length(Token) <> 42) or (Copy(Token,1,4) <> 'set-') or
      (Pos('/',Token)>0) or (Pos('\',Token)>0) or (Pos('..',Token)>0) then
      raise EInOutError.Create('Invalid calculation cache generation');
    Result := ChildPath(Root,Token);
    if not DirectoryExistsUTF8(Result) then raise EInOutError.Create('Missing calculation cache generation');
  finally L.Free end;
end;

function FreeCalculationDirectory(const Root: string): string;
begin
  if Assigned(ActiveScope) then Result := ActiveScope.Directory
  else begin
    if not ForceDirectoriesUTF8(Root) then raise EInOutError.Create('Cannot create calculation cache');
    Result := PublishedDirectory(Root);
  end;
end;

constructor TFreeCalculationScope.Create(const Root, ExecutableDirectory: string;
  const PublishedOutputs: array of string);
var I: Integer; Source, Name: string; H: THandle;
begin
  inherited Create;
  if Assigned(ActiveScope) then raise EInOutError.Create('A calculation is already running');
  FRoot := ExpandFileNameUTF8(Root);
  FPreviousDirectory := GetCurrentDirUTF8;
  FPreviousCursor := Screen.Cursor;
  if not ForceDirectoriesUTF8(FRoot) then raise EInOutError.Create('Cannot create calculation cache');
  Name := ChildPath(FRoot,'calculation.lock');
  if not FileExistsUTF8(Name) then begin
    H := FileCreateUTF8(Name);
    if H = THandle(-1) then raise EInOutError.Create('Cannot create calculation lock');
    FileClose(H);
  end;
  // An OS-held lock is released even if the application crashes. The lock file
  // itself is persistent. Another process may calculate a different model.
  FLock := TFileStream.Create(Name, fmOpenReadWrite or fmShareExclusive);
  FDirectory := UniqueDirectory(FRoot, 'job-');
  FOwned := True;
  Source := PublishedDirectory(FRoot);
  for I := Low(SharedNames) to High(SharedNames) do
    if FileExistsUTF8(ChildPath(Source,SharedNames[I])) then
      CopyRequired(ChildPath(Source,SharedNames[I]),ChildPath(FDirectory,SharedNames[I]));
  for I := Low(PublishedOutputs) to High(PublishedOutputs) do begin
    Name := PublishedOutputs[I];
    if (Name <> SharedNames[0]) and (Name <> SharedNames[1]) and (Name <> SharedNames[2]) then
      raise EInOutError.Create('Undeclared calculation cache output');
    if FileExistsUTF8(ChildPath(FDirectory,Name)) and not DeleteFileUTF8(ChildPath(FDirectory,Name)) then
      raise EInOutError.Create('Cannot invalidate previous calculation output');
  end;
  // Invalidate results being recomputed, retaining the other declared inputs.
  if Length(PublishedOutputs) > 0 then PublishFiles;
  for I := 0 to 1 do begin
    if I=0 then Name:='CONF.PRO' else Name:='RESIST.CNF';
    if FileExistsUTF8(ChildPath(ExecutableDirectory,Name)) then
      CopyRequired(ChildPath(ExecutableDirectory,Name),ChildPath(FDirectory,Name));
  end;
  // hship's distributed RESIST.CNF uses the sibling BD directory.
  Source := ChildPath(ExtractFileDir(ExcludeTrailingPathDelimiter(ExecutableDirectory)),'BD');
  if DirectoryExistsUTF8(Source) then begin
    if not CreateDirUTF8(ChildPath(FDirectory,'BD')) then raise EInOutError.Create('Cannot stage solver database');
    for I:=1 to 4 do begin
      Name:='RESIST'+IntToStr(I)+'.BD';
      CopyRequired(ChildPath(Source,Name),ChildPath(ChildPath(FDirectory,'BD'),Name));
    end;
  end;
  if not SetCurrentDirUTF8(FDirectory) then raise EInOutError.Create('Cannot enter calculation directory');
  ActiveScope := Self;
end;

destructor TFreeCalculationScope.Destroy;
begin
  if ActiveScope = Self then ActiveScope := nil;
  if FPreviousDirectory <> '' then begin
    SetCurrentDirUTF8(FPreviousDirectory); Screen.Cursor := FPreviousCursor;
  end;
  // Only this instance's GUID directory is removed, never the shared root.
  if FOwned and (ExtractFileDir(FDirectory)=FRoot) and
    (Copy(ExtractFileName(FDirectory),1,4)='job-') then
    if not DeleteDirectory(FDirectory,False) then Logger.Error('Cannot remove calculation workspace: '+FDirectory);
  FLock.Free;
  inherited Destroy;
end;

function NumericCount(const Text, Name: string): Integer;
var I, Start: Integer; Token: string; V: Double; FS: TFormatSettings;
begin
  Result := 0; I:=1; FS:=DefaultFormatSettings; FS.DecimalSeparator:='.';
  while I<=Length(Text) do begin
    while (I<=Length(Text)) and (Text[I]<= ' ') do Inc(I);
    if I>Length(Text) then Break;
    Start:=I;
    while (I<=Length(Text)) and (Text[I]>' ') do Inc(I);
    Token:=Copy(Text,Start,I-Start);
    // Pascal's legacy consumers read Single, so reject overflow there as well.
    if not TryStrToFloat(Token,V,FS) or IsNan(V) or IsInfinite(V) or (Abs(V)>3.402823466E38) then
      raise EFreeCalculationOutput.Create('Invalid numeric calculation output: '+Name);
    Inc(Result);
  end;
end;

procedure RequireNumbers(const Text, Name: string; MinCount, MaxCount: Integer; Multiple: Integer=1);
var N: Integer;
begin
  N:=NumericCount(Text,Name);
  if (N<MinCount) or (N>MaxCount) or (N mod Multiple<>0) then
    raise EFreeCalculationOutput.CreateFmt('Incomplete calculation output %s: %d numbers, expected %d..%d in groups of %d',
      [Name,N,MinCount,MaxCount,Multiple]);
end;

procedure RequireLines(L: TStrings; const Name: string; Minimum, Maximum: Integer);
begin
  if (L.Count<Minimum) or (L.Count>Maximum) then
    raise EFreeCalculationOutput.CreateFmt('Incomplete calculation report %s: %d lines, expected %d..%d',
      [Name,L.Count,Minimum,Maximum]);
end;

procedure ValidateShared(const Name: string);
var L: TStringList; I: Integer; S: string;
begin
  L:=ReadLines(Name);
  try
    S:='';
    for I:=0 to L.Count-1 do
      if not AnsiStartsStr('#',Trim(L[I])) then S:=S+' '+L[I];
    case LowerCase(ExtractFileName(Name)) of
      'resist.dat': RequireNumbers(S,Name,25,25);
      'resistp.dat': begin
        RequireLines(L,Name,4,13);
        if not AnsiStartsStr('#',Trim(L[0])) or not AnsiStartsStr('#',Trim(L[2])) then
          raise EFreeCalculationOutput.Create('Missing resistance report headers: '+Name);
        RequireNumbers(L[1],Name,5,6);
        for I:=3 to L.Count-1 do RequireNumbers(L[I],Name,5,5);
      end;
      'propres.dat': begin
        RequireLines(L,Name,2,2);
        if not AnsiStartsStr('#',Trim(L[0])) then
          raise EFreeCalculationOutput.Create('Missing propeller report header: '+Name);
        RequireNumbers(L[1],Name,15,15);
      end;
    end;
  finally L.Free end;
end;

procedure TFreeCalculationScope.PublishFiles;
var Generation, Name, Token: string; I: Integer; Stream: TStringStream;
begin
  // Validate all members before making a generation visible. Readers observe
  // one immutable directory, including when a second process is running.
  for I:=Low(SharedNames) to High(SharedNames) do
    if FileExistsUTF8(ChildPath(FDirectory,SharedNames[I])) then
      ValidateShared(ChildPath(FDirectory,SharedNames[I]));
  Generation:=UniqueDirectory(FRoot,'set-');
  try
    for I:=Low(SharedNames) to High(SharedNames) do begin
      Name:=SharedNames[I];
      if FileExistsUTF8(ChildPath(FDirectory,Name)) then CopyRequired(ChildPath(FDirectory,Name),ChildPath(Generation,Name));
    end;
    Token:=ExtractFileName(Generation)+LineEnding;
    Stream:=TStringStream.Create(Token);
    try ReplaceFileAtomically(Stream,ChildPath(FRoot,'current')) finally Stream.Free end;
  except
    DeleteDirectory(Generation,False);
    raise;
  end;
end;

procedure TFreeCalculationScope.Complete;
begin PublishFiles end;

procedure RejectInvalidReportNumbers(L:TStrings; const Name:string);
var I,J,K:Integer; S,T:string;
begin
  for I:=0 to L.Count-1 do begin
    S:=UpperCase(L[I]); J:=1;
    while J<=Length(S) do begin
      while (J<=Length(S)) and (S[J] in [' ',#9,'=','|',',','(',')']) do Inc(J);
      K:=J;
      while (J<=Length(S)) and not (S[J] in [' ',#9,'=','|',',','(',')']) do Inc(J);
      T:=Copy(S,K,J-K);
      if (T='NAN') or (T='+NAN') or (T='-NAN') or
        (T='INF') or (T='+INF') or (T='-INF') or
        (T='INFINITY') or (Pos('#IND',T)>0) or (Pos('#INF',T)>0) then
        raise EFreeCalculationOutput.Create('Non-finite calculation report: '+Name);
    end;
  end;
end;

procedure ValidateFreeCalculationOutput(const Directory: string; Protocol: TFreeCalculationProtocol);
var L, Graph: TStringList; Name, S: string; I, N, SeriesNo: Integer;
    V: Double; FS:TFormatSettings;
begin
  Name:=ChildPath(Directory,Outputs[Protocol]); L:=ReadLines(Name);
  try
    RejectInvalidReportNumbers(L,Name);
    case Protocol of
      fcPropeller1: RequireNumbers(L.Text,Name,45,45);
      fcPropeller2: RequireNumbers(L.Text,Name,70,70);
      fcPropeller3: RequireNumbers(L.Text,Name,165,165);
      fcFung,fcOortmer: RequireNumbers(L.Text,Name,80,80);
      fcHollen: RequireNumbers(L.Text,Name,90,90);
      fcSeaMargin: RequireNumbers(L.Text,Name,1,1);
      fcClemPoup: RequireNumbers(L.Text,Name,60,60);
      fcClemBlou: RequireNumbers(L.Text,Name,12,48,6);
      fcBunkov: RequireNumbers(L.Text,Name,96,96);
      fcCompton: RequireNumbers(L.Text,Name,66,66);
      fcWolfson: RequireNumbers(L.Text,Name,12,60,6);
      fcRadojcic: RequireNumbers(L.Text,Name,56,63,7);
      fcRvrs: RequireNumbers(L.Text,Name,6,MaxResultBytes div 2,6);
      fcHship: begin
        RequireNumbers(L.Text,Name,245,265);
        S:=Trim(L.Text); I:=1; while (I<=Length(S)) and (S[I]>' ') do Inc(I);
        FS:=DefaultFormatSettings; FS.DecimalSeparator:='.';
        if not TryStrToFloat(Copy(S,1,I-1),V,FS) or (V<>Trunc(V)) then
          raise EFreeCalculationOutput.Create('Invalid hship series');
        SeriesNo:=Round(V);
        if SeriesNo<0 then raise EFreeCalculationOutput.Create('Invalid hship series');
        if SeriesNo<=1 then N:=245 else if SeriesNo=2 then N:=255 else N:=265;
        RequireNumbers(L.Text,Name,N,N);
      end;
      fcManeuv: RequireLines(L,Name,193,200);
      fcPowerPrediction: RequireLines(L,Name,85,125);
      fcAddedMass: begin
        RequireLines(L,Name,20,32);
        RequireNumbers(L[11],Name,2,2);
        RequireNumbers(L[19],Name,5,5);
      end;
      fcIshercof: begin
        RequireLines(L,Name,54,70);
        for I:=35 to 53 do RequireNumbers(L[I],Name,7,7);
      end;
      fcPropeller4: begin
        RequireLines(L,Name,59,70);
        for I:=46 to 55 do begin
          S:=L[I]; N:=LastDelimiter('=',S);
          if N>0 then S:=Copy(S,N+1,Length(S));
          RequireNumbers(S,Name,1,1);
        end;
        Graph:=ReadLines(ChildPath(Directory,'POPGR.'));
        try
          RequireLines(Graph,'POPGR.',3,52);
          RequireNumbers(Graph[0],'POPGR.',2,2);
          for I:=1 to Graph.Count-1 do RequireNumbers(Graph[I],'POPGR.',4,4);
        finally Graph.Free end;
      end;
      fcPropol,fcFppcalc: begin
        RequireLines(L,Name,27,95);
        N:=L.Count;
        if Protocol=fcPropol then begin
          RequireLines(L,Name,30,98);
          for I:=L.Count-2 to L.Count-1 do begin
            S:=StringReplace(L[I],'KT =','',[rfReplaceAll]);
            S:=StringReplace(S,'KQ =','',[rfReplaceAll]);
            S:=StringReplace(S,'*J','',[rfReplaceAll]);
            RequireNumbers(S,Name,4,4);
          end;
          Dec(N,3);
        end;
        for I:=25 to N-1 do RequireNumbers(L[I],Name,8,8);
      end;
    end;
  finally L.Free end;
end;

function ExecuteFreeCalculation(const Directory, ExecutableName: string;
  Protocol: TFreeCalculationProtocol; TimeoutMS: Cardinal): Integer;
var Input, Output, Extra: string;
begin
  if not Assigned(ActiveScope) or (ExpandFileNameUTF8(Directory)<>ActiveScope.Directory) then
    raise EInOutError.Create('External calculation requires a private workspace');
  Input:=ChildPath(Directory,Inputs[Protocol]); Output:=ChildPath(Directory,Outputs[Protocol]);
  if not FileExistsUTF8(Input) then raise EInOutError.Create('Missing calculation input: '+Input);
  if FileExistsUTF8(Output) and not DeleteFileUTF8(Output) then raise EInOutError.Create('Cannot remove previous result: '+Output);
  if Protocol=fcPropeller4 then begin
    Extra:=ChildPath(Directory,'POPGR.');
    if FileExistsUTF8(Extra) and not DeleteFileUTF8(Extra) then raise EInOutError.Create('Cannot remove previous propeller graph');
  end;
  Logger.Info('Calculation protocol='+IntToStr(Ord(Protocol))+' directory='+Directory+
    ' input_sha1='+SHA1Print(SHA1File(Input))+' solver_sha1='+SHA1Print(SHA1File(ExecutableName)));
  Result:=ExecuteFreePlugin(Directory,ExecutableName,TimeoutMS);
  try
    ValidateFreeCalculationOutput(Directory,Protocol);
  except
    on E:EFreeCalculationOutput do
      raise EFreeCalculationOutput.Create(ExtractFileName(ExecutableName)+': '+E.Message);
  end;
  // Old callers used removal of the input as a completion marker. The child
  // has exited and all outputs have passed validation, so polling is redundant.
  if FileExistsUTF8(Input) and not DeleteFileUTF8(Input) then raise EInOutError.Create('Cannot retire completed calculation input');
end;

end.
