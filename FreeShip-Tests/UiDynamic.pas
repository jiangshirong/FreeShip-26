unit UiDynamic;
{$mode objfpc}{$H+}
interface
uses Main, fpjson;
procedure RunDynamic(Form:TMainForm; Root:TJSONObject; Cases:TJSONArray);
implementation
uses SysUtils, Classes, Controls, ExtCtrls, StdCtrls, Menus, Buttons, Graphics, freehullformwindow_panel, UiDiscovery;

type TComboFixture=class(TComponent)
  published
    procedure Populate(Sender:TObject);
end;
procedure TComboFixture.Populate(Sender:TObject);
begin TComboBox(Sender).Items.Add('dynamic-one'); TComboBox(Sender).Items.Add('dynamic-two') end;

function DispatchCallback(Event:TNotifyEvent; Sender:TObject; Allowed:Boolean):Boolean;
begin
  Result:=Allowed and Assigned(Event);
  if Result then Event(Sender);
end;

function CallbackIdentity(Event:TNotifyEvent):string;
begin
  Result:='';
  if Assigned(TMethod(Event).Data) and Assigned(TMethod(Event).Code) then
    Result:=TObject(TMethod(Event).Data).ClassName+'.'+TObject(TMethod(Event).Data).MethodName(TMethod(Event).Code);
end;

function DispatchDropdown(Event:TNotifyEvent; Sender:TObject; const Approved:string):Boolean;
var Key:string;
begin
  Key:=CallbackIdentity(Event);
  Result:=DispatchCallback(Event,Sender,(Key<>'') and (Pos(';'+Key+';', ';'+Approved+';')>0));
end;

function MenuItems(Item:TMenuItem; Form:TMainForm; ResolveOverflow:Boolean):TJSONArray;
var I:Integer; O,Delegate:TJSONObject; Child:TMenuItem; Target:TSpeedButton;
begin
  Result:=TJSONArray.Create;
  for I:=0 to Item.Count-1 do begin
    Child:=Item.Items[I]; O:=TJSONObject.Create;
    O.Add('name',Child.Name); O.Add('caption',Child.Caption); O.Add('tag',Child.Tag);
    O.Add('visible',Child.Visible); O.Add('enabled',Child.Enabled); O.Add('checked',Child.Checked);
    if Assigned(Child.Action) then O.Add('action',Child.Action.Name);
    if ResolveOverflow then begin
      Target:=Form.UiProbeOverflowButton(Child.Tag);
      if Assigned(Target) then begin
        Delegate:=TJSONObject.Create; Delegate.Add('name',Target.Name); Delegate.Add('caption',Target.Caption);
        Delegate.Add('class',Target.ClassName); Delegate.Add('enabled',Target.Enabled);
        if Assigned(Target.Action) then Delegate.Add('action',Target.Action.Name);
        Delegate.Add('has_click_handler',Assigned(Target.OnClick)); O.Add('delegates_to',Delegate);
      end;
    end;
    O.Add('has_click_handler',Assigned(Child.OnClick)); O.Add('children',MenuItems(Child,Form,False)); Result.Add(O);
  end;
end;

function ComboItems(C:TComboBox):TJSONArray;
var I:Integer;
begin Result:=TJSONArray.Create; for I:=0 to C.Items.Count-1 do Result.Add(C.Items[I]) end;

function ComboTextMeasurements(C:TComboBox):TJSONArray;
var I:Integer; B:TBitmap; O:TJSONObject;
begin
  Result:=TJSONArray.Create; B:=TBitmap.Create;
  try B.Canvas.Font.Assign(C.Font);
    for I:=0 to C.Items.Count-1 do begin
      O:=TJSONObject.Create; O.Add('index',I); O.Add('text',C.Items[I]);
      O.Add('measured_width',B.Canvas.TextWidth(C.Items[I])); Result.Add(O);
    end;
  finally B.Free end;
end;

procedure RunDynamic(Form:TMainForm; Root:TJSONObject; Cases:TJSONArray);
var Seen:TFPList; Events:TJSONArray; I,Sequence:Integer; AllowMain,AllowHull:Boolean;
    Fixture:TComboFixture; Combo:TComboBox; O:TJSONObject; Event,Known:TNotifyEvent;
  procedure Capture(const State:string);
  var Visited:TFPList;
    procedure Walk(C:TComponent; const Path:string);
    var I,J:Integer; Child:TComponent; Key,Reason,Status:string; M:TPopupMenu;
        CB:TComboBox; Entry:TJSONObject; E,Expected:TNotifyEvent; V:TFreeHullWindow;
        Allowed:Boolean; Bar:TPanel; SavedWidth:Integer; SavedAlign:TAlign; Candidates:TFPList; ResizeHandler:TNotifyEvent; Count:Integer;
    begin
      if (C=nil) or (Visited.IndexOf(C)>=0) then Exit;
      Visited.Add(C);
      if (C is TPopupMenu) and Assigned(TPopupMenu(C).OnPopup) then begin
        M:=TPopupMenu(C); E:=M.OnPopup; Allowed:=False; Reason:='callback not audited'; Key:='';
        for J:=0 to Form.MDIChildCount-1 do begin
          V:=Form.GetMDIChildren(J); Expected:=@V.PopupMenuHullPopup;
          if (TMethod(E).Code=TMethod(Expected).Code) and (TMethod(E).Data=Pointer(V)) then begin
            Key:='hull-popup'; Allowed:=AllowHull; Reason:='hull popup source fingerprint not approved'; Break;
          end;
        end;
        Expected:=Form.UiProbeOverflowHandler;
        if (Key='') and Assigned(Expected) and (TMethod(E).Code=TMethod(Expected).Code) and (TMethod(E).Data=TMethod(Expected).Data) then begin
          Key:='overflow-popup'; Allowed:=AllowMain; Reason:='main form source fingerprint not approved';
        end;
        Entry:=TJSONObject.Create; Entry.Add('state',State); Entry.Add('kind','popup');
        Entry.Add('component_path',Path); Entry.Add('name',M.Name); Entry.Add('adapter',Key);
        Entry.Add('before',MenuItems(M.Items,Form,Key='overflow-popup')); Status:='not_run';
        if Allowed then begin
          try
            Bar:=nil; SavedWidth:=0; SavedAlign:=alNone;
            if Key='overflow-popup' then begin
              Candidates:=TFPList.Create;
              try
                ResizeHandler:=Form.UiProbeBarResizeHandler;
                FindComponentsByEvent(Form,'OnResize',TMethod(ResizeHandler),Candidates); Count:=0;
                for J:=0 to Candidates.Count-1 do if TObject(Candidates[J]) is TPanel then begin Inc(Count); Bar:=TPanel(Candidates[J]) end;
              finally Candidates.Free end;
              if Count<>1 then begin Bar:=nil; Allowed:=False; Reason:='resize control missing or ambiguous' end;
              if not Allowed then begin Entry.Add('after',MenuItems(M.Items,Form,True)); Entry.Add('status','not_run'); Entry.Add('reason',Reason); Events.Add(Entry); Exit end;
              SavedWidth:=Bar.Width; SavedAlign:=Bar.Align; Bar.Align:=alNone; Bar.Width:=450;
              if Assigned(Bar.OnResize) then Bar.OnResize(Bar);
              Entry.Add('scenario','hidden logical width 450');
            end;
            try DispatchCallback(E,M,Allowed); Status:='executed'; Reason:='audited callback invoked directly; no popup window opened';
            finally
              Entry.Add('after',MenuItems(M.Items,Form,Key='overflow-popup'));
              if Assigned(Bar) then begin Bar.Width:=SavedWidth; Bar.Align:=SavedAlign;
                if Assigned(Bar.OnResize) then Bar.OnResize(Bar) end;
            end;
          except on Ex:Exception do begin Status:='error'; Reason:=Ex.ClassName+': '+Ex.Message end end;
        end;
        if Entry.Find('after')=nil then Entry.Add('after',MenuItems(M.Items,Form,Key='overflow-popup'));
        Entry.Add('status',Status); Entry.Add('reason',Reason); Events.Add(Entry);
      end;
      if (C is TComboBox) and Assigned(TComboBox(C).OnDropDown) then begin
        CB:=TComboBox(C); Entry:=TJSONObject.Create; Entry.Add('state',State); Entry.Add('kind','combo');
        Entry.Add('component_path',Path); Entry.Add('name',CB.Name); Entry.Add('before',ComboItems(CB));
        E:=CB.OnDropDown; Key:=CallbackIdentity(E);
        Entry.Add('callback_identity',Key);
        Status:='not_run'; Reason:='dropdown callback discovered but no audited hash-matched adapter';
        if (Key<>'') and (Pos(';'+Key+';', ';'+SysUtils.GetEnvironmentVariable('FS_UI_DYNAMIC_DROPDOWNS')+';')>0) then begin
          try DispatchDropdown(E,CB,SysUtils.GetEnvironmentVariable('FS_UI_DYNAMIC_DROPDOWNS')); Status:='executed'; Reason:='audited dropdown callback invoked directly';
          except on Ex:Exception do begin Status:='error'; Reason:=Ex.ClassName+': '+Ex.Message end end;
        end;
        Entry.Add('status',Status); Entry.Add('reason',Reason);
        Entry.Add('after',ComboItems(CB)); Entry.Add('option_measurements_after',ComboTextMeasurements(CB));
        Entry.Add('measurement_note','LCL canvas using combo font; actual dropdown width and native rendering are unknown'); Events.Add(Entry);
      end;
      for I:=0 to C.ComponentCount-1 do begin
        Child:=C.Components[I]; Key:=Child.Name; if Key='' then Key:=Child.ClassName+'#'+IntToStr(I);
        Walk(Child,Path+'/'+Key);
      end;
    end;
  begin
    Visited:=TFPList.Create;
    try Walk(Form,Form.Name) finally Visited.Free end;
  end;
begin
  AllowMain:=SysUtils.GetEnvironmentVariable('FS_UI_DYNAMIC_MAIN')='1';
  AllowHull:=SysUtils.GetEnvironmentVariable('FS_UI_DYNAMIC_HULL')='1';
  Events:=TJSONArray.Create; Root.Add('dynamic_callbacks',Events);
  Root.Add('dynamic_scope','isolated process; approved callbacks invoked directly; unknown callbacks not invoked');
  Form.FormShow(Form); Capture('no_model');
  Form.LoadNamedFile(SysUtils.GetEnvironmentVariable('FS_UI_PROBE_MODEL'));
  if not Form.FreeShip.ModelIsLoaded then raise Exception.Create('fixture failed to load');
  for I:=0 to Form.MDIChildCount-1 do
    if Form.GetMDIChildren(I).Viewport.ProgressivePreview then Form.GetMDIChildren(I).Viewport.FinishProgressiveDisplay(True);
  Form.UpdateMenu; Capture('model_loaded');
  if Form.FreeShip.Surface.NumberOfControlPoints>0 then begin
    Form.FreeShip.Surface.ControlPoint[0].Selected:=True; Form.UpdateMenu; Capture('point_selected');
    Form.FreeShip.Surface.ClearSelection; Form.UpdateMenu;
  end;
  if Form.FreeShip.Preferences.TrySetInterfaceLanguage('Russian') then begin
    Form.RefreshStudioLanguage; Form.UpdateMenu; Capture('russian_model');
  end;
  if Form.FreeShip.Preferences.TrySetInterfaceLanguage('Chinese') then begin
    Form.RefreshStudioLanguage; Form.UpdateMenu; Capture('chinese_model');
  end;
  Fixture:=TComboFixture.Create(nil); Combo:=TComboBox.Create(nil);
  try
    Combo.OnDropDown:=@Fixture.Populate; Event:=Combo.OnDropDown; Known:=@Fixture.Populate;
    DispatchDropdown(Event,Combo,'');
    O:=TJSONObject.Create; O.Add('id','dynamic.synthetic-deny');
    if Combo.Items.Count=0 then O.Add('status','pass') else O.Add('status','fail');
    O.Add('expected','unapproved callback is not invoked'); O.Add('actual',IntToStr(Combo.Items.Count)); Cases.Add(O);
    DispatchDropdown(Event,Combo,'TComboFixture.Populate');
    O:=TJSONObject.Create; O.Add('id','dynamic.synthetic-combo');
    if (Combo.Items.Count=2) and (Combo.Items[0]='dynamic-one') and (Combo.Items[1]='dynamic-two') then O.Add('status','pass') else O.Add('status','fail');
    O.Add('expected','isolated synthetic dropdown callback populates two options'); O.Add('actual',ComboItems(Combo)); Cases.Add(O);
  finally Combo.Free; Fixture.Free end;
  O:=TJSONObject.Create; O.Add('id','dynamic.coverage'); O.Add('status','not_run');
  O.Add('expected','all dynamically generated items and native popup behavior');
  O.Add('actual','unapproved handlers, unconstructed dialogs and native desktop events are not exercised'); Cases.Add(O);
end;
end.
