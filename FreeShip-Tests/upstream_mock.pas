program UpstreamMock;
{$mode objfpc}
uses SysUtils;
begin
 if Pos('mock-fail',LowerCase(ParamStr(0)))>0 then Halt(7);
 Halt(0);
end.
