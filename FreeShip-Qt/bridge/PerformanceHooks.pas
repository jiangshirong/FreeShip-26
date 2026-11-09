{ Isolated calculator lifecycle hooks; no changes to numerical formulas. }
unit PerformanceHooks;
{$mode objfpc}{$H+}{$codepage utf8}
interface
uses Classes,SysUtils,Dialogs,Controls;
type TPerformanceFppMatrix=array[1..8,1..70] of Single;
var PerformancePreparing:Boolean=True;
    PerformanceCancelFile:string='';PerformanceStarted:QWord=0;
    PerformanceWarnings:TStringList=nil;
    PerformanceOutput:string='';
    PerformanceCacheKey:string='';
procedure PerformanceCapture(const Directory:string);
procedure PerformanceProcessLog(const Exe,Output,Error:string);
procedure PerformanceReadFpp(const Filename:string;var Values:TPerformanceFppMatrix;out Count:Integer);
procedure PerformanceCheck;
function PerformanceCancelled:Boolean;
function PerformanceMessage(const Text:string;Kind:TMsgDlgType;Buttons:TMsgDlgButtons;Help:Longint):Integer;
procedure PerformanceNotice(const Text:string);
implementation
uses FileUtil,StrUtils,Math;
procedure PerformanceReadFpp(const Filename:string;var Values:TPerformanceFppMatrix;out Count:Integer);
var L,Tokens:TStringList;I,J,N:Integer;D:Double;Summary:Boolean;FS:TFormatSettings;
begin
  L:=TStringList.Create;Tokens:=TStringList.Create;Count:=0;Summary:=False;FS:=DefaultFormatSettings;FS.DecimalSeparator:='.';FillChar(Values,SizeOf(Values),0);
  try
    L.LoadFromFile(Filename);if (L.Count<27) or (L.Count>100) then raise Exception.Create('螺旋桨结果行数无效。');
    for I:=25 to L.Count-1 do begin
      if Trim(L[I])='' then Continue;
      Tokens.Clear;ExtractStrings([' ',#9],[],PChar(L[I]),Tokens);N:=Tokens.Count;
      if Summary or ((N<>7) and (N<>8)) or ((N=8) and (Count>=68)) then raise Exception.Create('螺旋桨结果格式无效。');
      if N=7 then Summary:=True else Inc(Count);
      for J:=0 to N-1 do begin
        if not TryStrToFloat(Tokens[J],D,FS) or IsNan(D) or IsInfinite(D) then raise Exception.Create('螺旋桨结果含无效数值。');
        if Summary then Values[J+1,Count+1]:=D else Values[J+1,Count]:=D;
      end;
    end;
    if not Summary or (Count<2) then raise Exception.Create('螺旋桨结果不完整。');
  finally L.Free;Tokens.Free end;
end;
procedure PerformanceProcessLog(const Exe,Output,Error:string);
var L:TStringList;Path:string;
begin
  if PerformanceOutput='' then Exit;
  Path:=IncludeTrailingPathDelimiter(PerformanceOutput)+'solver-files';ForceDirectories(Path);
  L:=TStringList.Create;try L.Text:=Output+LineEnding+Error;L.SaveToFile(IncludeTrailingPathDelimiter(Path)+ExtractFileName(Exe)+'.log') finally L.Free end;
end;
procedure PerformanceCapture(const Directory:string);
var Search:TSearchRec;Path,Extension:string;Total:Int64;
begin
  if (PerformanceOutput='') or (Directory='') then Exit;
  Path:=IncludeTrailingPathDelimiter(PerformanceOutput)+'solver-files';ForceDirectories(Path);Total:=0;
  if FindFirst(IncludeTrailingPathDelimiter(Directory)+'*',faAnyFile,Search)=0 then begin
    repeat
      Extension:=LowerCase(ExtractFileExt(Search.Name));
      if ((Search.Attr and faDirectory)=0) and (Search.Size<2*1024*1024) and (Total<16*1024*1024) and
        ((Extension='.tsk') or (Extension='.dat') or (Extension='.res') or (Extension='.tmp') or (Extension='.txt') or (Extension='') or (Extension='.')) then begin
        Inc(Total,Search.Size);FileUtil.CopyFile(IncludeTrailingPathDelimiter(Directory)+Search.Name,IncludeTrailingPathDelimiter(Path)+Search.Name);
      end;
    until FindNext(Search)<>0;FindClose(Search);
  end;
end;
function PerformanceCancelled:Boolean;
begin Result:=(PerformanceCancelFile<>'') and FileExists(PerformanceCancelFile) end;
procedure PerformanceCheck;
begin
  if PerformanceCancelled then raise EAbort.Create('计算已取消。');
  if (PerformanceStarted<>0) and (GetTickCount64-PerformanceStarted>110000) then raise EAbort.Create('计算超过时间限制。');
end;
function PerformanceMessage(const Text:string;Kind:TMsgDlgType;Buttons:TMsgDlgButtons;Help:Longint):Integer;
begin
  if (Kind=mtError) or (mbYes in Buttons) then raise Exception.Create(Text);
  if Assigned(PerformanceWarnings) then PerformanceWarnings.Add(Text);
  Result:=mrOK;
end;
procedure PerformanceNotice(const Text:string);
begin if Assigned(PerformanceWarnings) then PerformanceWarnings.Add(Text) end;
end.
