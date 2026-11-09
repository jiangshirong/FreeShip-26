program brand_credit_test;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,FreeSafeLanguage,FreeAboutDlg;
var d:TFreeAboutDlg;i,r:Integer;codes:array[0..6]of string=('cn','en','ru','fr','de','es','ja');
begin Application.Initialize;
for i:=0 to 6 do begin
 if not ApplyLanguageSafely(codes[i],'K:\locale') then Halt(2);
 d:=TFreeAboutDlg.Create(nil);r:=d.StringGridCredits.RowCount-1;
 if d.Caption<>'FreeShip_26' then Halt(3);
 if Pos('|',d.StringGridCredits.Cells[1,r])=0 then Halt(4);
 if (codes[i]='cn') and (d.StringGridCredits.Cells[1,r]<>'美化 | 极大的性能优化') then Halt(5);
 if (codes[i]='ru') and (Pos('Jiang Shirong / Женя',d.StringGridCredits.Cells[2,r])=0) then Halt(6);
 if (codes[i]<>'ru') and (Pos('/ Женя',d.StringGridCredits.Cells[2,r])<>0) then Halt(7);
 WriteLn('PASS ',codes[i],' brand and contributor');d.Free;
end;end.
