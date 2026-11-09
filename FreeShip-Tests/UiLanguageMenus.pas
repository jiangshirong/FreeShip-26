unit UiLanguageMenus;
{$mode objfpc}{$H+}{$codepage utf8}
interface
uses Main, fpjson;
procedure RunLanguageMenus(Form:TMainForm; Cases:TJSONArray; Root:TJSONObject);
implementation
uses SysUtils, Classes, Menus, ActnList, Translations, StdCtrls, FreeControlPointFrm,
  freehullformwindow_panel, FreeGeometry, UiDiscovery;

procedure RunLanguageMenus(Form:TMainForm; Cases:TJSONArray; Root:TJSONObject);
const Keys:array[0..7] of string=('Chinese','Russian','French','German','Spanish','Japanese','English','Chinese');
      Codes:array[0..7] of string=('cn','ru','fr','de','es','ja','en','cn');
var L,VIndex,BeforeCount,Matches:Integer; View:TFreeHullWindow;
    Po:TPOFile; Evidence:TJSONArray; Selected:TFreeSubdivisionControlPoint; Editor:TEdit;
    SavedType,SavedMode,SavedAxis:Integer; OK:Boolean; Dir:string;
  procedure Check(const Id:string; Pass:Boolean; const Expected,Actual:string);
  var O:TJSONObject;
  begin
    O:=TJSONObject.Create;O.Add('id',Id);
    if Pass then O.Add('status','pass') else O.Add('status','fail');
    O.Add('expected',Expected);O.Add('actual',Actual);Cases.Add(O);
  end;
  procedure Walk(Item:TMenuItem; const Path:string);
  var I:Integer; Entry:TPOFileItem; Ref,Expected,ItemPath:string; O:TJSONObject;
  begin
    ItemPath:=Path+'/'+Item.Name;
    if Item.Name<>'' then begin
      Ref:=LowerCase(View.ClassName+'.'+Item.Name+'.Caption');Entry:=Po.FindPoItem(Ref);
      if (Entry=nil) and (Item.Action is TAction) then begin
        Ref:=LowerCase(View.ClassName+'.'+Item.Action.Name+'.Caption');Entry:=Po.FindPoItem(Ref);
      end;
      if Entry<>nil then begin
        if Codes[L]='en' then Expected:=Entry.Original else Expected:=Entry.Translation;
        if Expected<>'' then begin
          Check('language.popup.'+Codes[L]+'.'+IntToStr(L)+'.'+IntToStr(VIndex)+'.'+Item.Name,Item.Caption=Expected,Expected,Item.Caption);
          O:=TJSONObject.Create;O.Add('language',Keys[L]);O.Add('view',VIndex);O.Add('menu_path',ItemPath);
          O.Add('identifier',Ref);O.Add('caption',Item.Caption);O.Add('expected',Expected);
          O.Add('hint',Item.Hint);O.Add('checked',Item.Checked);O.Add('enabled',Item.Enabled);Evidence.Add(O);
        end;
      end;
    end;
    for I:=0 to Item.Count-1 do Walk(Item.Items[I],ItemPath);
  end;
begin
  Evidence:=TJSONArray.Create;Root.Add('language_popup_evidence',Evidence);
  BeforeCount:=Form.FreeShip.Surface.NumberOfControlPoints;Selected:=Form.FreeShip.Surface.ControlPoint[5];
  Form.FreeShip.ControlpointForm.ActiveControlPoint:=Selected;
  Editor:=FindEditByHandler(Form.FreeShip.ControlpointForm,@Form.FreeShip.ControlpointForm.EditNameEditingDone,Matches);
  if Editor=nil then raise Exception.Create('Cannot uniquely discover point name editor for language menu probe');
  Editor.Text:='Popup-language-draft-草稿';
  Dir:=ExtractFilePath(ParamStr(0))+'locale'+DirectorySeparator;
  for L:=Low(Keys) to High(Keys) do begin
    OK:=Form.FreeShip.Preferences.TrySetInterfaceLanguage(Keys[L]);Form.RefreshStudioLanguage;
    Check('language.popup.select.'+IntToStr(L),OK and SameText(Form.FreeShip.Preferences.Language,Keys[L]),Keys[L],Form.FreeShip.Preferences.Language);
    if Codes[L]='en' then Po:=TPOFile.Create(Dir+'FreeShip.cn.po',True)
    else Po:=TPOFile.Create(Dir+'FreeShip.'+Codes[L]+'.po',True);
    try
      for VIndex:=0 to Form.MDIChildCount-1 do begin
        View:=Form.GetMDIChildren(VIndex);SavedType:=Ord(View.Viewport.ViewType);
        SavedMode:=Ord(View.Viewport.ViewportMode);SavedAxis:=View.Viewport.ControlPointCoordinateAxis;
        // Audited source: updates menu state and coordinate captions only.
        // Direct callback invocation, not a native popup or mouse event.
        View.PopupMenuHullPopup(View.PopupMenuHull);Walk(View.PopupMenuHull.Items,'hull');
        Check('language.popup.view-state.'+IntToStr(L)+'.'+IntToStr(VIndex),
          (SavedType=Ord(View.Viewport.ViewType)) and (SavedMode=Ord(View.Viewport.ViewportMode)) and
          (SavedAxis=View.Viewport.ControlPointCoordinateAxis),'view type, render mode, coordinate axis unchanged','checked');
      end;
    finally Po.Free end;
    Check('language.popup.editor.'+IntToStr(L),Editor.Text='Popup-language-draft-草稿','draft retained',Editor.Text);
    Check('language.popup.model.'+IntToStr(L),(BeforeCount=Form.FreeShip.Surface.NumberOfControlPoints) and
      (Form.FreeShip.ControlpointForm.ActiveControlPoint=Selected),'model size and selected editor binding unchanged','checked');
  end;
  Form.FreeShip.ControlpointForm.ActiveControlPoint:=nil;
end;
end.
