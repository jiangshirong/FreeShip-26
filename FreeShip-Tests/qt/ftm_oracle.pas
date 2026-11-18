program FtmOracle;
{$mode objfpc}{$H+}
uses Interfaces, Forms, SysUtils, Classes, Main, FreeShipUnit, FreeFileBuffer;
var Ship:TFreeShip; InputBuffer,OutputBuffer:TFreeFileBuffer;
begin
 try
  Application.Initialize;Application.CreateForm(TMainForm,MainForm);
  Ship:=MainForm.FreeShip;Ship.IsRebuildAsyncDisabled:=True;
  if LowerCase(ExtractFileExt(ParamStr(1)))='.ftm' then InputBuffer:=TFreeTextBuffer.Create
  else InputBuffer:=TFreeFileBuffer.Create;
  try InputBuffer.LoadFromFile(ParamStr(1));Ship.LoadProject(InputBuffer,False) finally InputBuffer.Free end;
  if not Ship.ModelIsLoaded then raise Exception.Create('Original loader rejected model');
  if LowerCase(ExtractFileExt(ParamStr(2)))='.ftm' then OutputBuffer:=TFreeTextBuffer.Create
  else OutputBuffer:=TFreeFileBuffer.Create;
  try Ship.SaveProject(OutputBuffer);if not OutputBuffer.SaveToFile(ParamStr(2)) then raise Exception.Create('Save failed') finally OutputBuffer.Free end;
  WriteLn('pass');Halt(0);
 except on E:Exception do begin WriteLn(E.ClassName+': '+E.Message);Halt(1) end end;
end.
