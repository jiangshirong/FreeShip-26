{ Direct original FreeEdit entrypoints. Does not use ModelService/PerformanceEngine. }
program PerformanceOracle;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Classes,Windows,Main,FreeShipUnit,FreeFileBuffer,
  FreeSafeLanguage,PerformanceObserver;
var Buffer:TFreeFileBuffer;Ship:TFreeShip;Method:string;
begin
  try
    DefaultFormatSettings.DecimalSeparator:='.';
    SetMultiByteConversionCodePage(CP_UTF8);SetMultiByteRTLFileSystemCodePage(CP_UTF8);
    Application.Initialize;ApplyLanguageSafely('cn',ExtractFilePath(ParamStr(0))+'locale');
    Application.CreateForm(TMainForm,MainForm);Ship:=MainForm.FreeShip;
    LoadOracleRequest(ParamStr(2));
    Buffer:=TFreeFileBuffer.Create;try Buffer.LoadFromFile(ParamStr(1));Ship.LoadProject(Buffer,False) finally Buffer.Free end;
    Ship.Preferences.TempDirectory:=ParamStr(3)+'/cache';ForceDirectories(Ship.Preferences.TempDirectory);
    Ship.Preferences.ExecDirectory:=OracleRequest.Get('solver_directory','');Method:=OracleRequest.Get('calculator','');
    ForceDirectories(Ship.CalculationDirectory);
    {ORACLE_DISPATCH}
    Halt(0);
  except on E:Exception do begin WriteLn(E.ClassName+': '+E.Message);DumpExceptionBackTrace(StdOut);Halt(1) end end;
end.
