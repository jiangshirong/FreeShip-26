program copyright_language_test;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,FreeAboutDlg,FreeSafeLanguage;
var a:TFreeAboutDlg;i:integer;langs:array[0..2]of string=('cn','ru','en');expected:array[0..2]of string=('版权所有 © 2026 蒋世融 (Jiang Shirong) — RongShip 修改与改进','Авторские права © 2026 蒋世融 (Jiang Shirong) — модификации RongShip','Copyright © 2026 蒋世融 (Jiang Shirong) — RongShip modifications');
begin
 Application.Initialize;
 for i:=0 to 2 do begin
 if not ApplyLanguageSafely(langs[i],'K:\locale') then Halt(1);
 a:=TFreeAboutDlg.Create(nil);
 if a.StringGridCopyrights.Cells[0,3]<>expected[i] then Halt(2);
 a.Free;WriteLn('PASS copyright ',langs[i]);
 end;
end.
