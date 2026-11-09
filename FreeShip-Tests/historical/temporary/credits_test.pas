program credits_test;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,Translations,FreeAboutDlg;
var a:TFreeAboutDlg;i:integer;expected:string;
begin
 Application.Initialize;
 if ParamStr(1)='cn' then expected:='RongShip 开发与界面重设计'
 else if ParamStr(1)='ru' then expected:='Разработка RongShip и переработка интерфейса'
 else expected:='RongShip development and UI redesign';
 if ParamStr(1)<>'' then TranslateUnitResourceStrings('FreeAboutDlg','K:\locale\FreeShip.'+ParamStr(1)+'.po');
 a:=TFreeAboutDlg.Create(nil);
 i:=a.StringGridCredits.RowCount-1;
 if a.StringGridCredits.Cells[0,i]<>expected then begin WriteLn('FAIL translation ',a.StringGridCredits.Cells[0,i]);Halt(1) end;
 if a.StringGridCredits.Cells[1,i]<>'蒋世融 (Jiang Shirong), from 2026' then Halt(2);
 if i<>6 then Halt(3);
 WriteLn('PASS credit row and language ',ParamStr(1));a.Free;
end.
