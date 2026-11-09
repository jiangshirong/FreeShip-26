program credit_group_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,FreeAboutDlg;
var a:TFreeAboutDlg;i,n,years,total:integer;
begin
 Application.Initialize;a:=TFreeAboutDlg.Create(nil);a.OnResize(a);
 n:=0;years:=0;total:=2;
 for i:=1 to a.StringGridCredits.RowCount-1 do begin
 if a.StringGridCredits.Cells[1,i]='' then inc(n);
 if a.StringGridCredits.Cells[0,i]<>'' then inc(years);
 end;
 if (n<>3) or (years<>4) or (a.StringGridCredits.RowCount<>11) then Halt(1);
 if (a.StringGridCredits.Cells[0,3]<>'2007') or (a.StringGridCredits.Cells[0,4]<>'') then Halt(2);
 if (a.StringGridCredits.Cells[0,6]<>'2015') or (a.StringGridCredits.Cells[0,7]<>'') or (a.StringGridCredits.Cells[0,8]<>'') then Halt(3);
 for i:=0 to 10 do inc(total,a.StringGridCredits.RowHeights[i]+1);
 if total>a.StringGridCredits.Height then Halt(4);
 WriteLn('PASS four year groups, three blank bands; repeated years empty; complete content fits');a.Free;
end.
