unit FreeExceptionDlg;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs, ExtCtrls, StdCtrls,
  LCLTranslator;

type

  { TExceptionDlg }

  TExceptionDlg = class(TForm)
    Message: TMemo;
  private

  public

  end;

//var ExceptionDlg: TExceptionDlg;

implementation

{$R *.lfm}

end.

