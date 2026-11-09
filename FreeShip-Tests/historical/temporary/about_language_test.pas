program about_language_test;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,Classes,StdCtrls,FreeAboutDlg,FreeStringsUnit,FreeUiChoices,FreeSafeLanguage;
var a:TFreeAboutDlg;c:TComboBox;m:TStringList;i:integer;k:Word;
procedure CheckLang(const lang,credit,version,copyword:string);
begin
 if not ApplyLanguageSafely(lang,'K:\locale') then Halt(1);
 a:=TFreeAboutDlg.Create(nil);
 if a.StringGridCredits.Cells[1,1]<>credit then begin WriteLn('credit ',a.StringGridCredits.Cells[1,1]);Halt(2) end;
 if a.StringGridVersionInfo.Cells[0,0]<>version then Halt(3);
 if Pos(copyword,a.StringGridCopyrights.Cells[0,0])<>1 then Halt(4);
 if a.StringGridCredits.Cells[2,7]<>'蒋世融 (Jiang Shirong)' then Halt(5);
 if a.FindComponent('LabelFreeShipProjectLink')<>nil then Halt(6);
 if (a.StringGridCredits.Cells[0,2]<>'2007') or (a.StringGridCredits.Cells[0,3]<>'') or (a.StringGridCredits.Cells[0,4]<>'2015') or (a.StringGridCredits.Cells[0,5]<>'') or (a.StringGridCredits.Cells[0,6]<>'') then Halt(16);
 if (a.StringGridCredits.ColCount<>3) or (a.StringGridCredits.Cells[0,7]<>'2026') then Halt(12);
 if (a.FindComponent('Button1')<>nil) or (a.FindComponent('Panel4')<>nil) then Halt(10);
 k:=27;a.OnKeyDown(a,k,[]);if k<>0 then Halt(11);
 a.Free;WriteLn('PASS about language and close controls ',lang);
end;
begin
 Application.Initialize;
 CheckLang('cn','原始作者','产品版本','版权所有');
 CheckLang('ru','Первоначальный автор','Версия продукта','Авторские права');
 CheckLang('ja','Initial Author','Product version','Copyright');
 CheckLang('en','Initial Author','Product version','Copyright');
 c:=TComboBox.Create(nil);m:=TStringList.Create;
 c.Items.CommaText:='Chinese,Russian,English,French,German,Spanish,Japanese,Czech,Italian,Dutch,Norwegian,Finnish,Ukrainian,Vietnamese';
 SetLanguageChoices(c,m,'Czech');
 if c.Items.Count<>7 then Halt(7);
 for i:=0 to c.Items.Count-1 do if not IsVisibleInterfaceLanguage(LanguageChoiceKey(m,c.Items[i])) then Halt(8);
 if LanguageCode('Japanese')<>'ja' then Halt(9);
 WriteLn('PASS seven choices; legacy mappings retained; Japanese resets Russian to English');
 m.Free;c.Free;
end.
