unit FreeReportPresentation;
{$mode objfpc}{$H+}
interface
uses Classes,SysUtils,StdCtrls;
function ReportUnitPowers(const Value:string):string;
function ReportPlainText(Lines:TStrings):TStringList;
procedure PrepareReportMemo(Memo:TMemo);
implementation
uses Graphics,LCLIntf,LCLType;
{$IFDEF MSWINDOWS}
function ReportDialogBaseUnits:LongWord;stdcall;external 'user32.dll' name 'GetDialogBaseUnits';
{$ENDIF}
function ReportUnitPowers(const Value:string):string;
begin
 Result:=StringReplace(Value,'^2','²',[rfReplaceAll]);
 Result:=StringReplace(Result,'^3','³',[rfReplaceAll]);
 Result:=StringReplace(Result,'^4','⁴',[rfReplaceAll]);
end;
function ReportPlainText(Lines:TStrings):TStringList;
var I,P,W,N:Integer;S,Key,Value:string;
 function Columns(const Text:string):Integer;
 var U:UnicodeString;J,C:Integer;
 begin
  Result:=0;U:=UTF8Decode(Text);
  for J:=1 to Length(U) do begin
   C:=Ord(U[J]);
   if ((C>=$2E80) and (C<=$A4CF)) or ((C>=$AC00) and (C<=$D7A3)) or
      ((C>=$FF01) and (C<=$FF60)) then Inc(Result,2) else Inc(Result);
  end;
 end;
 function Separator(const Text:string;out Size:Integer):Integer;
 begin
  Result:=Pos(#9+': ',Text);Size:=3;
  if Result=0 then Result:=Pos(' : ',Text);
 end;
begin
 Result:=TStringList.Create;Result.Assign(Lines);W:=0;
 for I:=0 to Result.Count-1 do begin
  S:=ReportUnitPowers(Result[I]);Result[I]:=S;P:=Separator(S,N);
  if (P>0) and (Pos('|',S)=0) then begin
   Key:=Trim(Copy(S,1,P-1));N:=Columns(Key);if N>W then W:=N;
  end;
 end;
 for I:=0 to Result.Count-1 do begin
  S:=Result[I];P:=Separator(S,N);
  if (P>0) and (Pos('|',S)=0) then begin
   Key:=Trim(Copy(S,1,P-1));Value:=Trim(Copy(S,P+N,Length(S)));
   Result[I]:=Key+StringOfChar(' ',W-Columns(Key)+1)+': '+Value;
  end;
 end;
end;
procedure PrepareReportMemo(Memo:TMemo);
var I,P,W,Stop:Integer;S,Key,Value:string;Measure:TBitmap;Base:LongWord;
begin
 Measure:=TBitmap.Create;
 Memo.Lines.BeginUpdate;
 try
  Measure.SetSize(1,1);Measure.Canvas.Font.Assign(Memo.Font);W:=0;
  for I:=0 to Memo.Lines.Count-1 do begin
   S:=ReportUnitPowers(Memo.Lines[I]);P:=Pos(' : ',S);
   if (P>0) and (Pos('|',S)=0) then begin
    Key:=Trim(Copy(S,1,P-1));Value:=Trim(Copy(S,P+3,Length(S)));
    Memo.Lines[I]:=Key+#9+': '+Value;
   end else Memo.Lines[I]:=S;
  end;
  for I:=0 to Memo.Lines.Count-1 do begin
   S:=Memo.Lines[I];P:=Pos(#9,S);
   if P>0 then begin
    Stop:=Measure.Canvas.TextWidth(Copy(S,1,P-1));if Stop>W then W:=Stop;
   end;
  end;
  Memo.WordWrap:=False;
  {$IFDEF MSWINDOWS}
  // Native edit tab stops align by pixel position, independently of UTF-8
  // byte counts, Chinese full-width glyphs and the translated label length.
  Base:=ReportDialogBaseUnits and $FFFF;if Base=0 then Base:=8;
  Stop:=((W+32)*4+Integer(Base)-1) div Integer(Base);
  LCLIntf.SendMessage(Memo.Handle,$00CB,1,PtrInt(@Stop)); // EM_SETTABSTOPS
  {$ENDIF}
 finally Memo.Lines.EndUpdate;Measure.Free end;
 Memo.Invalidate;
end;
end.
