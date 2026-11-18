unit PlateHooks;
{$mode objfpc}{$H+}{$codepage utf8}
interface
var PlateCancelFile:string=''; PlateStarted:QWord=0;
procedure PlateCheckpoint;
implementation
uses SysUtils;
procedure PlateCheckpoint;
begin
  if (PlateCancelFile<>'') and FileExists(PlateCancelFile) then
    raise Exception.Create('板材展开已取消。');
  if (PlateStarted<>0) and (GetTickCount64-PlateStarted>110000) then
    raise Exception.Create('板材展开超时，请降低模型精度后重试。');
end;
end.
