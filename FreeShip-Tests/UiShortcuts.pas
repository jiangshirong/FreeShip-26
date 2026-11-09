unit UiShortcuts;
{$mode objfpc}{$H+}
interface
uses Main, fpjson;
procedure TestShortcutsBeforeModel(Form: TMainForm; const Scope: string; Cases: TJSONArray);
procedure TestShortcutsWithModel(Form: TMainForm; const Scope: string; Cases: TJSONArray);
implementation
uses SysUtils, Classes, Controls, Forms, LCLIntf,LCLType, LMessages, FreeGeometry,
  freehullformwindow_panel, StdCtrls, UiDiscovery, ActnList, Menus;

type
  TProbeKeyboardState=array[0..255] of Byte;
function ProbeGetKeyboardState(var State:TProbeKeyboardState):LongBool;stdcall;external 'user32.dll' name 'GetKeyboardState';
function ProbeSetKeyboardState(var State:TProbeKeyboardState):LongBool;stdcall;external 'user32.dll' name 'SetKeyboardState';
procedure PostControlKey(Control:TWinControl;Key:Word);
var Saved,Modified:TProbeKeyboardState;
begin
 if not ProbeGetKeyboardState(Saved) then raise Exception.Create('Cannot save private thread keyboard state');
 Modified:=Saved;Modified[VK_CONTROL]:=$80;Modified[VK_LCONTROL]:=$80;Modified[VK_RCONTROL]:=0;
 Modified[VK_SHIFT]:=0;Modified[VK_LSHIFT]:=0;Modified[VK_RSHIFT]:=0;
 Modified[VK_MENU]:=0;Modified[VK_LMENU]:=0;Modified[VK_RMENU]:=0;
 Modified[VK_LWIN]:=0;Modified[VK_RWIN]:=0;
 try
  if not ProbeSetKeyboardState(Modified) then raise Exception.Create('Cannot set private thread keyboard state');
  LCLIntf.PostMessage(Control.Handle,$0100,Key,1);Application.ProcessMessages;
  LCLIntf.PostMessage(Control.Handle,$0101,Key,$C0000001);Application.ProcessMessages;
 finally ProbeSetKeyboardState(Saved) end;
end;

type
  TKeyObserver = class
    Seen: Integer;
    ExpectedSender: TObject;
    SenderMatches: Boolean;
    procedure Observe(Sender: TObject; var Key: Word; Shift: TShiftState);
  end;

procedure TKeyObserver.Observe(Sender: TObject; var Key: Word; Shift: TShiftState);
begin
  if Key <> VK_F24 then Exit;
  Inc(Seen);
  SenderMatches := Sender = ExpectedSender;
  // Stop before the viewport's regular handler can change desktop focus.
  Key := 0;
end;

procedure Check(Cases: TJSONArray; const Id: string; Ok: Boolean;
  const Expected, Actual: string);
var O: TJSONObject;
begin
  O := TJSONObject.Create; O.Add('id', Id);
  if Ok then O.Add('status','pass') else O.Add('status','fail');
  O.Add('expected',Expected); O.Add('actual',Actual); Cases.Add(O);
end;

procedure NotRun(Cases: TJSONArray; const Id, Expected, Actual: string);
var O: TJSONObject;
begin
  O := TJSONObject.Create; O.Add('id',Id); O.Add('status','not_run');
  O.Add('expected',Expected); O.Add('actual',Actual); Cases.Add(O);
end;

procedure Configure(Form: TMainForm);
begin
  Form.FreeShip.Preferences.QuickLanguageFirst := 'Chinese';
  Form.FreeShip.Preferences.QuickLanguageSecond := 'Russian';
  Form.FreeShip.Preferences.QuickLanguageEnabled := True;
  if not Form.FreeShip.Preferences.TrySetInterfaceLanguage('Chinese') then
    raise Exception.Create('Chinese language resource unavailable for shortcut tests');
  Form.RefreshStudioLanguage;
end;

function FindLanguageShortcut(Form:TMainForm; out Key:Word; out Shift:TShiftState):Boolean;
var Items:TFPList; Event:TNotifyEvent; I,Count:Integer; Item:TMenuItem;
begin
  Result:=False; Key:=0; Shift:=[]; Count:=0; Item:=nil;
  if GetEnvironmentVariable('FS_UI_DYNAMIC_MAIN')<>'1' then Exit;
  Items:=TFPList.Create;
  try
    Event:=@Form.StudioQuickLanguageClick;
    FindComponentsByEvent(Form,'OnClick',TMethod(Event),Items);
    for I:=0 to Items.Count-1 do if TObject(Items[I]) is TMenuItem then begin Inc(Count); Item:=TMenuItem(Items[I]) end;
    if (Count=1) and Assigned(Item) and (Item.ShortCut<>0) then begin
      ShortCutToKey(Item.ShortCut,Key,Shift); Result:=Key<>0;
    end;
  finally Items.Free end;
end;

procedure TestSwitch(Form: TMainForm; Cases: TJSONArray; const Id: string; Sender: TObject);
var Key,OriginalKey: Word; ShortcutShift:TShiftState; FirstKey, SecondKey: Word; First, Second: string;
begin
  Configure(Form);
  if not FindLanguageShortcut(Form,OriginalKey,ShortcutShift) then begin NotRun(Cases,Id,'unique configured language shortcut','no unique bound menu shortcut'); Exit end;
  Key := OriginalKey;
  Application.NotifyKeyDownBeforeHandler(Sender,Key,ShortcutShift);
  First := Form.FreeShip.Preferences.Language; FirstKey := Key;
  Key := OriginalKey;
  Application.NotifyKeyDownBeforeHandler(Sender,Key,ShortcutShift);
  Second := Form.FreeShip.Preferences.Language; SecondKey := Key;
  Check(Cases,Id,SameText(First,'Russian') and SameText(Second,'Chinese') and
    (FirstKey=0) and (SecondKey=0),
    'registered application handler consumes each key; Chinese -> Russian -> Chinese',
    Format('%s -> %s; remaining keys=%d,%d',[First,Second,FirstKey,SecondKey]));
end;

procedure TestShortcutsBeforeModel(Form: TMainForm; const Scope: string; Cases: TJSONArray);
begin
  if (Scope<>'all') and (Scope<>'shortcuts') then Exit;
  Check(Cases,'shortcuts.no-model-state',not Form.FreeShip.ModelIsLoaded,
    'no model loaded before shortcut',SysUtils.BoolToStr(Form.FreeShip.ModelIsLoaded,True));
  TestSwitch(Form,Cases,'shortcuts.no-model',Form);
end;

procedure TestShortcutsWithModel(Form: TMainForm; const Scope: string; Cases: TJSONArray);
const Names: array[0..3] of string = ('bodyplan','profile','plan','perspective');
      ViewTypes: array[0..3] of TFreeViewType = (fvBodyplan,fvProfile,fvPlan,fvPerspective);
var I,J,VisiblePoints: Integer; BeforeOtherZoom,AfterZoom:Double; Key: Word; View: TFreeHullWindow; Zoom: Double; Observer: TKeyObserver;
    Sender: TWinControl; PointCount, EditorMatches, ActionMatches: Integer; BeforeText: string; Editor:TEdit; SelectAction:TAction; ShortcutKey:Word; ShortcutShift:TShiftState;
  procedure NoSwitch(const Id: string; InitialKey: Word; Shift: TShiftState);
  var K: Word; Lang: string;
  begin
    K:=InitialKey; Lang:=Form.FreeShip.Preferences.Language;
    Application.NotifyKeyDownBeforeHandler(Form,K,Shift);
    Check(Cases,Id,(K=InitialKey) and SameText(Lang,Form.FreeShip.Preferences.Language),
      'key remains available and language does not change',
      Format('key=%d; language=%s',[K,Form.FreeShip.Preferences.Language]));
  end;
  procedure TestMessagePath(const Id: string; Control: TWinControl);
  begin
    Observer.Seen:=0; Observer.ExpectedSender:=Control; Observer.SenderMatches:=False;
    Control.Perform(CN_KEYDOWN,VK_F24,0);
    Check(Cases,Id,(Observer.Seen=1) and Observer.SenderMatches,
      'one harmless LCL message reaches the registered application handler',
      Format('callbacks=%d; sender matches=%s',[Observer.Seen,SysUtils.BoolToStr(Observer.SenderMatches,True)]));
  end;
begin
  if (Scope<>'all') and (Scope<>'shortcuts') then Exit;
  Check(Cases,'shortcuts.with-model-state',Form.FreeShip.ModelIsLoaded,
    'fixture model loaded',SysUtils.BoolToStr(Form.FreeShip.ModelIsLoaded,True));
  TestSwitch(Form,Cases,'shortcuts.main',Form);
  if Form.MDIChildCount<4 then raise Exception.Create('Four viewports required for shortcut tests');
  for I:=0 to 3 do begin
    View:=Form.GetMDIChildren(I);
    Check(Cases,'shortcuts.view-type.'+Names[I],View.Viewport.ViewType=ViewTypes[I],
      'sender is the named view type',IntToStr(Ord(View.Viewport.ViewType)));
    TestSwitch(Form,Cases,'shortcuts.view.'+Names[I],View.Viewport);
    View.Viewport.ControlPointCoordinateAxis:=0;
    Key:=Ord('X');View.ViewportKeyDown(View.Viewport,Key,[ssCtrl,ssAlt]);
    Check(Cases,'shortcuts.axis-x-on.'+Names[I],(Key=0) and (View.Viewport.ControlPointCoordinateAxis=1),'Ctrl+Alt+X enables X','axis='+IntToStr(View.Viewport.ControlPointCoordinateAxis));
    Key:=Ord('X');View.ViewportKeyDown(View.Viewport,Key,[ssCtrl,ssAlt]);
    Check(Cases,'shortcuts.axis-x-off.'+Names[I],View.Viewport.ControlPointCoordinateAxis=0,'repeated axis shortcut disables axis','axis='+IntToStr(View.Viewport.ControlPointCoordinateAxis));
    Key:=Ord('Y');View.ViewportKeyDown(View.Viewport,Key,[ssCtrl,ssAlt]);
    Key:=Ord('Z');View.ViewportKeyDown(View.Viewport,Key,[ssCtrl,ssAlt]);
    Check(Cases,'shortcuts.axis-switch.'+Names[I],View.Viewport.ControlPointCoordinateAxis=3,'Y then Z switches directly to Z','axis='+IntToStr(View.Viewport.ControlPointCoordinateAxis));
    View.Viewport.ControlPointCoordinateAxis:=0;

    Zoom:=View.Viewport.Zoom;
    View.Viewport.Zoom:=1;
    Key:=Ord('I');View.ViewportKeyDown(View.Viewport,Key,[ssCtrl]);
    Check(Cases,'shortcuts.zoom-in.'+Names[I],(Key=0) and (View.Viewport.Zoom>1),
      'Ctrl+I zooms the target view and consumes the key','zoom='+FloatToStr(View.Viewport.Zoom));
    Key:=Ord('O');View.ViewportKeyDown(View.Viewport,Key,[ssCtrl]);
    Check(Cases,'shortcuts.zoom-out.'+Names[I],(Key=0) and (Abs(View.Viewport.Zoom-1)<0.00001),
      'Ctrl+O reverses the preceding zoom','zoom='+FloatToStr(View.Viewport.Zoom));
    View.Viewport.Zoom:=20;Key:=Ord('I');View.ViewportKeyDown(View.Viewport,Key,[ssCtrl]);
    Check(Cases,'shortcuts.zoom-bound.'+Names[I],View.Viewport.Zoom=20,
      'shortcut respects zoom upper limit',FloatToStr(View.Viewport.Zoom));
    View.Viewport.Zoom:=Zoom;

  end;

  Configure(Form);
  Form.FreeShip.Preferences.QuickLanguageEnabled:=False;
  if FindLanguageShortcut(Form,ShortcutKey,ShortcutShift) then NoSwitch('shortcuts.disabled',ShortcutKey,ShortcutShift)
  else NotRun(Cases,'shortcuts.disabled','configured shortcut disabled','no unique bound shortcut');
  Form.FreeShip.Preferences.QuickLanguageEnabled:=True;
  Form.FreeShip.Preferences.QuickLanguageSecond:='Chinese';
  if FindLanguageShortcut(Form,ShortcutKey,ShortcutShift) then NoSwitch('shortcuts.same-languages',ShortcutKey,ShortcutShift)
  else NotRun(Cases,'shortcuts.same-languages','same-language shortcut disabled','no unique bound shortcut');
  Form.FreeShip.Preferences.QuickLanguageSecond:='Russian';


  Form.FreeShip.ControlpointForm.ActiveControlPoint:=Form.FreeShip.Surface.ControlPoint[5];
  Editor:=FindEditByHandler(Form.FreeShip.ControlpointForm,
    @Form.FreeShip.ControlpointForm.EditNameEditingDone,EditorMatches);
  if Assigned(Editor) then begin
    Editor.Text:='Shortcut-Draft-123'; BeforeText:=Editor.Text;
    PointCount:=Form.FreeShip.Surface.NumberOfControlPoints;
    TestSwitch(Form,Cases,'shortcuts.editor',Editor);
    Check(Cases,'shortcuts.editor-preserves-draft',
      (BeforeText=Editor.Text) and (PointCount=Form.FreeShip.Surface.NumberOfControlPoints) and
      (Form.FreeShip.ControlpointForm.ActiveControlPoint=Form.FreeShip.Surface.ControlPoint[5]),
      'discovered editor draft, point binding and model remain unchanged',Editor.Text);
  end else begin
    NotRun(Cases,'shortcuts.editor','unique point name editor','matches='+IntToStr(EditorMatches));
    NotRun(Cases,'shortcuts.editor-preserves-draft','editor draft preserved','editor missing or ambiguous');
  end;
  Form.FreeShip.ControlpointForm.ActiveControlPoint:=nil;

  Observer:=TKeyObserver.Create;
  try
    Application.AddOnKeyDownBeforeHandler(@Observer.Observe,True);
    try
      TestMessagePath('shortcuts.lcl-message.main',Form);
      for I:=0 to 3 do begin
        Sender:=Form.GetMDIChildren(I).Viewport;
        TestMessagePath('shortcuts.lcl-message.'+Names[I],Sender);
      end;
    finally Application.RemoveOnKeyDownBeforeHandler(@Observer.Observe) end;
  finally Observer.Free end;

  Form.FreeShip.Visibility.ShowControlNet:=True;
  SelectAction:=FindActionByHandler(Form,@Form.SelectAllExecute,ActionMatches);
  if GetEnvironmentVariable('FS_UI_DYNAMIC_MAIN')<>'1' then SelectAction:=nil;
  for I:=0 to 3 do begin
    View:=Form.GetMDIChildren(I);
    Form.FreeShip.Surface.ClearSelection; Form.UpdateMenu;
    Zoom:=View.Viewport.Zoom;
    if Assigned(SelectAction) and SelectAction.Enabled and (SelectAction.ShortCut<>0) then begin
      ShortCutToKey(SelectAction.ShortCut,ShortcutKey,ShortcutShift); Key:=ShortcutKey;
      View.ViewportKeyDown(View.Viewport,Key,ShortcutShift);
      Check(Cases,'shortcuts.select-all.'+Names[I],(Key=0) and
        (Form.FreeShip.NumberOfSelectedControlPoints=8) and (Abs(View.Viewport.Zoom-Zoom)<0.000001),
        'discovered select-all shortcut consumes key, selects eight fixture points, and preserves zoom',
        Format('key=%d; selected=%d; zoom before/after=%.5f/%.5f',
          [Key,Form.FreeShip.NumberOfSelectedControlPoints,Zoom,View.Viewport.Zoom]));
    end else NotRun(Cases,'shortcuts.select-all.'+Names[I],
      'unique enabled select-all action with shortcut','missing, ambiguous, disabled or unapproved; matches='+IntToStr(ActionMatches));
  end;
  // Raw posted Win32 messages pass through the application's message pump,
  // modifier conversion and pre-menu handlers. Keyboard state is thread-local;
  // no SendInput, global cursor/focus change, or existing process is involved.
  VisiblePoints:=0;
  for J:=0 to Form.FreeShip.Surface.NumberOfControlPoints-1 do
    if Form.FreeShip.Surface.ControlPoint[J].Visible then Inc(VisiblePoints);
  for I:=0 to 3 do begin
    View:=Form.GetMDIChildren(I);Form.FreeShip.Edit.Selection_Clear;
    View.Viewport.Zoom:=1;BeforeOtherZoom:=Form.GetMDIChildren((I+1) mod 4).Viewport.Zoom;
    PostControlKey(View.Viewport,Ord('I'));AfterZoom:=View.Viewport.Zoom;
    Check(Cases,'shortcuts.posted.zoom-in.'+Names[I],AfterZoom>1,'posted Ctrl+I reaches the view before menu shortcuts','zoom='+FloatToStr(AfterZoom));
    PostControlKey(View.Viewport,Ord('O'));
    Check(Cases,'shortcuts.posted.zoom-out.'+Names[I],(View.Viewport.Zoom<AfterZoom) and
      (Abs(Form.GetMDIChildren((I+1) mod 4).Viewport.Zoom-BeforeOtherZoom)<0.000001),'posted Ctrl+O zooms only its target','zoom='+FloatToStr(View.Viewport.Zoom));
    PostControlKey(View.Viewport,Ord('A'));
    Check(Cases,'shortcuts.posted.select-all.'+Names[I],(VisiblePoints>0) and (Form.FreeShip.NumberOfSelectedControlPoints=VisiblePoints),'posted Ctrl+A selects visible fixture points','selected='+IntToStr(Form.FreeShip.NumberOfSelectedControlPoints));
    PostControlKey(View.Viewport,Ord('A'));
    Check(Cases,'shortcuts.posted.deselect-all.'+Names[I],Form.FreeShip.NumberOfSelectedControlPoints=0,'second posted Ctrl+A clears selection','selected='+IntToStr(Form.FreeShip.NumberOfSelectedControlPoints));
    Form.FreeShip.Surface.ControlPoint[0].Selected:=True;
    PostControlKey(View.Viewport,Ord('A'));
    Check(Cases,'shortcuts.posted.partial-to-all.'+Names[I],Form.FreeShip.NumberOfSelectedControlPoints=VisiblePoints,'partial selection expands to all','selected='+IntToStr(Form.FreeShip.NumberOfSelectedControlPoints));
  end;
  Form.FreeShip.Edit.Selection_Clear;
  Editor:=TEdit.Create(Form);
  try
    Editor.Parent:=Form;Editor.Text:='Keyboard editor draft';
    PostControlKey(Editor,Ord('A'));
    Check(Cases,'shortcuts.posted.editor-select-all',(Editor.SelText=Editor.Text) and
      (Form.FreeShip.NumberOfSelectedControlPoints=0),'text Ctrl+A selects text without touching geometry',Editor.SelText);
  finally Editor.Free end;
  Form.FreeShip.Surface.ClearSelection; Form.UpdateMenu;
  NotRun(Cases,'shortcuts.physical-input',
    'physical Ctrl+Alt+L/Ctrl+A delivered through Windows, focus and modifier conversion',
    'in-process handler simulation and separate harmless LCL message only; no real input or focus');
  NotRun(Cases,'shortcuts.modal',
    'shortcut suppressed while a real modal dialog is active',
    'modal windows and desktop focus are not exercised');
end;
end.
