unit UiSelectedDetails;
{$mode objfpc}{$H+}{$codepage utf8}
interface
uses Main, fpjson;
procedure RunSelectedDetails(Form:TMainForm; Cases:TJSONArray; Root:TJSONObject);
implementation
uses SysUtils, Classes, ActnList, ComCtrls, FreeSelectedDlg, FreeGeometry, UiDiscovery;
procedure RunSelectedDetails(Form:TMainForm; Cases:TJSONArray; Root:TJSONObject);
const Keys:array[0..6] of string=('Chinese','Russian','French','German','Spanish','Japanese','English');
var Dlg:TFormSelected;P:TFreeSubdivisionControlPoint;WasSelected,Found,OK:Boolean;
    I,Matches:Integer;Action:TAction;Evidence:TJSONArray;O:TJSONObject;
  procedure Add(const Id,Status,Expected,Actual:string);
  var Entry:TJSONObject;
  begin Entry:=TJSONObject.Create;Entry.Add('id',Id);Entry.Add('status',Status);Entry.Add('expected',Expected);Entry.Add('actual',Actual);Cases.Add(Entry) end;
  procedure Check(const Id:string;Pass:Boolean;const Expected,Actual:string);
  begin if Pass then Add(Id,'pass',Expected,Actual) else Add(Id,'fail',Expected,Actual) end;
  procedure FindPointData(C:TComponent);
  var J:Integer;
  begin
    if C is TListView then for J:=0 to TListView(C).Items.Count-1 do
      if TListView(C).Items[J].Data=Pointer(P) then Found:=True;
    for J:=0 to C.ComponentCount-1 do FindPointData(C.Components[J]);
  end;
begin
  // Known functional adapter: TFormSelected.onSelectionUpdate refreshes selected-object lists.
  // Fixed naming checks below are a separate explicit user-requested source revision, not a layout rule.
  Action:=FindActionByHandler(Form,@Form.SelectionDialogExecute,Matches);
  if Action=nil then begin Add('selected-details.entry','not_run','unique action bound to the reviewed handler','matches='+IntToStr(Matches));Exit end;
  Evidence:=TJSONArray.Create;Root.Add('selected_details_evidence',Evidence);
  Dlg:=TFormSelected.Create(Form);P:=Form.FreeShip.Surface.ControlPoint[5];WasSelected:=P.Selected;
  try
    Dlg.FreeShip:=Form.FreeShip;P.Selected:=True;Dlg.onSelectionUpdate(P);Found:=False;FindPointData(Dlg);
    Check('selected-details.current-selection-data',Found,'selected point present as object data in a discovered list','point found='+SysUtils.BoolToStr(Found,True));
    for I:=0 to High(Keys) do begin
      OK:=Form.FreeShip.Preferences.TrySetInterfaceLanguage(Keys[I]);Form.RefreshStudioLanguage;
      Check('selected-details.select.'+Keys[I],OK,Keys[I],Form.FreeShip.Preferences.Language);
      O:=TJSONObject.Create;O.Add('language',Keys[I]);O.Add('action',Action.Name);O.Add('entry_caption',Action.Caption);
      O.Add('window_class',Dlg.ClassName);O.Add('window_caption',Dlg.Caption);O.Add('native_window_shown',False);Evidence.Add(O);
      // Acceptance criteria explicitly established in the current user request.
      if Keys[I]='Chinese' then begin
        Check('selected-details.requested-chinese-entry',Action.Caption='已选对象详情','已选对象详情',Action.Caption);
        Check('selected-details.requested-chinese-window',Dlg.Caption='已选对象详情','已选对象详情',Dlg.Caption);
      end;
      if Keys[I]='English' then begin
        Check('selected-details.reviewed-english-entry',Action.Caption='Selected Object Details','Selected Object Details',Action.Caption);
        Check('selected-details.reviewed-english-window',Dlg.Caption='Selected Object Details','Selected Object Details',Dlg.Caption);
      end;
    end;
  finally P.Selected:=WasSelected;Dlg.Free;Form.FreeShip.Preferences.TrySetInterfaceLanguage('Chinese');Form.RefreshStudioLanguage end;
end;
end.
