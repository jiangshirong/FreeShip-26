unit SaveSafetyHooks;
{$mode objfpc}{$H+}
interface
uses SysUtils, Classes, Dialogs, Controls;
var Fault, LastMessage, SelectedName: string;
    FaultReached: Boolean;
    FlushCount: Integer;
procedure Trip(const Phase: string);
function SaveMessage(const Msg: string; Kind: TMsgDlgType;
  Buttons: TMsgDlgButtons; Help: LongInt): Integer;
implementation
procedure Trip(const Phase: string);
begin
  if Phase='flush' then
  begin
    Inc(FlushCount);
    if (Fault='backup-flush') and (FlushCount=2) then
    begin FaultReached:=True; raise EWriteError.Create('injected backup-flush') end;
  end;
  if Fault=Phase then
  begin FaultReached:=True; raise EInOutError.Create('injected '+Phase) end;
end;
function SaveMessage(const Msg: string; Kind: TMsgDlgType;
  Buttons: TMsgDlgButtons; Help: LongInt): Integer;
begin
  if Kind=mtError then LastMessage:=Msg;
  Result:=mrYes;
end;
end.
