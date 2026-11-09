program exception_close_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Controls,StdCtrls,FreeExceptionDlg;
var d:TExceptionDlg;i:Integer;
begin Application.Initialize;d:=TExceptionDlg.Create(nil);
for i:=0 to d.ComponentCount-1 do if d.Components[i] is TButton then Halt(1);
d.Show;d.Close;if d.Visible then Halt(2);d.Free;WriteLn('PASS no duplicate close button; standard form Close works');end.
