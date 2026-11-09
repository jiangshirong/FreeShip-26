program HistoricalUiProbe;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces, Forms, Classes, SysUtils, Controls, Grids, fpjson,
  FreeAboutDlg, FreeSafeLanguage, FreeExceptionDlg, FreeHydrostaticsFrm;
var A:TFreeAboutDlg; F:TForm; E:TExceptionDlg; H:TFreeHydrostaticsForm;
    R:TJSONObject; Rows:TJSONArray; O:TJSONObject; G:TStringGrid;
    I,J,K,L:Integer; Key:Word; Action:TCloseAction; S:TStringList;
const Languages:array[0..3]of string=('cn','ru','en','ja');
procedure Capture(Form:TForm;const Kind:string);
var C,X,Y:Integer; Grid:TStringGrid; Row:TJSONArray; Item:TJSONObject;
begin
 for C:=0 to Form.ComponentCount-1 do if Form.Components[C] is TStringGrid then begin
  Grid:=TStringGrid(Form.Components[C]);Item:=TJSONObject.Create;
  Item.Add('kind',Kind);Item.Add('language',Languages[L]);Item.Add('name',Grid.Name);
  Item.Add('width',Grid.Width);Item.Add('height',Grid.Height);
  for Y:=0 to Grid.RowCount-1 do begin
   Row:=TJSONArray.Create;for X:=0 to Grid.ColCount-1 do Row.Add(Grid.Cells[X,Y]);
   Item.Add('row_'+IntToStr(Y),Row);
  end;
  Rows.Add(Item);
 end;
end;
begin
 Application.Initialize;R:=TJSONObject.Create;Rows:=TJSONArray.Create;R.Add('tables',Rows);
 try
  for L:=0 to High(Languages) do begin
   if not ApplyLanguageSafely(Languages[L],ExtractFilePath(ParamStr(0))+'locale') then
     raise Exception.Create('Cannot load requested language');
   A:=TFreeAboutDlg.Create(nil);
   try
    Capture(A,'about');
    for I:=0 to 1 do begin
     F:=A.CreateInformationWindow(I=0);
     try
      Capture(F,'information-'+IntToStr(I));Key:=27;
      if Assigned(F.OnKeyDown) then F.OnKeyDown(F,Key,[]);
      O:=TJSONObject.Create;O.Add('kind','escape');O.Add('language',Languages[L]);
      O.Add('handler_present',Assigned(F.OnKeyDown));O.Add('key_after',Key);Rows.Add(O);
     finally F.Free end;
    end;
   finally A.Free end;
  end;
  E:=TExceptionDlg.Create(nil);
  try E.Close;if E.Visible then raise Exception.Create('Hidden exception form became visible on close');finally E.Free end;
  H:=TFreeHydrostaticsForm.Create(nil);
  try
   H.Button1Click(H);J:=H.ModalResult;H.ModalResult:=mrCancel;
   H.CloseFromTitleBar(H,Action);
   if H.ModalResult<>J then raise Exception.Create('Title-bar close changed settings acceptance');
   R.Add('close_acceptance_equal',True);
  finally H.Free end;
  S:=TStringList.Create;try S.Text:=R.AsJSON;S.SaveToFile('observations.json');finally S.Free end;
  WriteLn('PASS hidden close acceptance; REVIEW about content and layout');
 finally R.Free end;
end.
