program ProcessSafetyHelper;
{$mode objfpc}{$H+}
uses SysUtils;
var Mode, Chunk: string; I: Integer;
begin
  Mode := LowerCase(ExtractFileName(ParamStr(0)));
  if ParamCount > 0 then Mode := ParamStr(1);
  if Pos('viewer', Mode) > 0 then begin Sleep(300); Halt(0) end;
  if Pos('fail', Mode) > 0 then
  begin
    WriteLn(StdErr, 'deliberate failure'); Flush(StdErr); Halt(7);
  end;
  if Pos('wait', Mode) > 0 then
  begin
    WriteLn('waiting'); Flush(Output);
    while True do Sleep(100);
  end;
  if Pos('flood', Mode) > 0 then
  begin
    Chunk := StringOfChar('A', 8192);
    for I := 1 to 256 do
    begin
      Write(Output, Chunk); Flush(Output);
      Write(StdErr, Chunk); Flush(StdErr);
    end;
    Halt(0);
  end;
  if Pos('stdin', Mode) > 0 then
  begin
    if EOF(Input) then WriteLn('input closed') else Halt(9);
    Halt(0);
  end;
  WriteLn('ok');
end.
