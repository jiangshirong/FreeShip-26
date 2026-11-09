unit UiExpanded;
{$mode objfpc}{$H+}{$codepage utf8}
interface
uses Main, fpjson;
procedure RunExpanded(Form: TMainForm; const Scope: string; Cases: TJSONArray; Root: TJSONObject);
implementation
uses FreeHydrostaticsDlg, freehullformwindow_panel, FreeProjectSettingsDlg, FreeIntersectionDlg, Graphics, UiNativeRender, SysUtils, Classes, Forms, LCLIntf, LMessages, Controls, StdCtrls, ExtCtrls, Menus, Buttons, ActnList,
  Grids, FreeTypes, FreeGeometry, FreeLayerVisibilityDlg, FreeControlPointFrm, UiDiscovery, UiLayers, UiInventory, UiLanguageMenus, UiSelectedDetails;

function ProbeDwmGet(Wnd:PtrUInt;Attribute:LongWord;Value:Pointer;Size:LongWord):LongInt;stdcall;
 external 'dwmapi.dll' name 'DwmGetWindowAttribute';
type TModalHandleCheck=class
  Dialog:TForm;
  OriginalHandle:PtrUInt;
  Stable:Boolean;
  procedure CloseModal(Sender:TObject);
end;
procedure TModalHandleCheck.CloseModal(Sender:TObject);
begin
  Stable:=PtrUInt(Dialog.Handle)=OriginalHandle;
  Dialog.ModalResult:=mrCancel;
end;
procedure Check(Cases: TJSONArray; const Id: string; Ok: Boolean;
  const Expected, Actual: string);
var O: TJSONObject;
begin
  O := TJSONObject.Create; O.Add('id', Id);
  if Ok then O.Add('status', 'pass') else O.Add('status', 'fail');
  O.Add('expected', Expected); O.Add('actual', Actual); Cases.Add(O);
end;

procedure NotRun(Cases: TJSONArray; const Id, Expected, Actual: string);
var O: TJSONObject;
begin
  O := TJSONObject.Create; O.Add('id',Id); O.Add('status','not_run');
  O.Add('expected',Expected); O.Add('actual',Actual); Cases.Add(O);
end;

function ButtonFor(Form: TMainForm; Action: TBasicAction): TSpeedButton;
var Seen:TFPList; Found:TSpeedButton; Count:Integer;
  procedure Walk(C:TComponent);
  var I:Integer;
  begin
    if (C=nil) or (Seen.IndexOf(C)>=0) then Exit;
    Seen.Add(C);
    if (C is TSpeedButton) and (TSpeedButton(C).Action=Action) then begin Inc(Count); Found:=TSpeedButton(C) end;
    for I:=0 to C.ComponentCount-1 do Walk(C.Components[I]);
  end;
begin
  if Action=nil then Exit(nil);
  Found:=nil; Count:=0; Seen:=TFPList.Create;
  try Walk(Form) finally Seen.Free end;
  if Count=1 then Result:=Found else Result:=nil;
end;

procedure RunExpanded(Form: TMainForm; const Scope: string; Cases: TJSONArray; Root: TJSONObject);
var HydroDialog:TFreeHydrostaticsDialog; NetBtn,PointBtn:TSpeedButton; WasVisible:Boolean;
    PointCount,NetMatches,PointMatches:Integer; SelectedPoint:TFreeSubdivisionControlPoint;
    NetAction,PointAction:TAction; Editor:TEdit; EditorMatches:Integer;
    LanguageIndex,WidthIndex,TargetWidth,SavedWidth,SavedHeight:Integer;
    SavedLanguage,StateName:string;SavedWindowState:TWindowState;
    ToolbarImage:Graphics.TBitmap;ToolbarImagePath:string;
    ViewIndex,OtherViewIndex:Integer;ViewWindow:TFreeHullWindow;ViewButton:TSpeedButton;
    ModalCheck:TModalHandleCheck;ModalTimer:TTimer;ModalIndex:Integer;CaptionTint:LongWord;
    ToolbarFixture:TPanel;SavedToolbarAlign:TAlign;OverflowFixture:TPopupMenu;

begin
  if (Scope='all') or (Scope='toolbar') or (Scope='chinese') or (Scope='language') then begin
    NetAction:=FindActionByHandler(Form,@Form.ShowControlNetExecute,NetMatches);
    PointAction:=FindActionByHandler(Form,@Form.AddPointExecute,PointMatches);
    if SysUtils.GetEnvironmentVariable('FS_UI_DYNAMIC_MAIN')<>'1' then begin
      NetAction:=nil; PointAction:=nil;
    end;
    NetBtn:=ButtonFor(Form,NetAction);
    PointBtn:=ButtonFor(Form,PointAction);
    if (Scope='all') or (Scope='toolbar') then begin
      HydroDialog:=TFreeHydrostaticsDialog.Create(nil);
      try
        HydroDialog.Edit.Lines.Text:='Volume : 42 m^3';
        HydroDialog.FormShow(HydroDialog);
        Check(Cases,'hydro.text-storage-hidden',not HydroDialog.Edit.Visible and not HydroDialog.Edit.Parent.Visible,
          'export memo and its container stay hidden after report preparation','memo visible='+SysUtils.BoolToStr(HydroDialog.Edit.Visible,True));
        Check(Cases,'hydro.text-storage-preserved',HydroDialog.Edit.Lines.Count>0,
          'export text remains available','lines='+IntToStr(HydroDialog.Edit.Lines.Count));
      finally HydroDialog.Free end;
      for ViewIndex:=0 to Form.MDIChildCount-1 do begin
        ViewWindow:=Form.GetMDIChildren(ViewIndex);
        ViewButton:=TSpeedButton(Form.FindComponent('StudioWindowToggle'+IntToStr(ViewIndex)));
        if ViewButton=nil then begin NotRun(Cases,'views.toggle-adapter','unique view toggle','known adapter not found');Continue end;
        ViewWindow.Show;
        ViewButton.Click;Check(Cases,'views.toggle-hide.'+IntToStr(ViewIndex),not ViewWindow.Visible and not ViewButton.Down,'toggle hides and releases state','visible='+SysUtils.BoolToStr(ViewWindow.Visible,True));
        ViewButton.Click;Check(Cases,'views.toggle-show.'+IntToStr(ViewIndex),ViewWindow.Visible and ViewButton.Down,'toggle shows and highlights','visible='+SysUtils.BoolToStr(ViewWindow.Visible,True));
        Check(Cases,'views.no-caption-buttons.'+IntToStr(ViewIndex),ViewWindow.CaptionButtons=[],'view has no title buttons',SysUtils.BoolToStr(ViewWindow.CaptionButtons=[],True));
        if Assigned(ViewWindow.CaptionPanel.OnDblClick) then begin
          ViewWindow.CaptionPanel.OnDblClick(ViewWindow.CaptionPanel);
          Check(Cases,'views.expand.'+IntToStr(ViewIndex),ViewWindow.WindowState=wsMaximized,'border double click expands','state='+IntToStr(Ord(ViewWindow.WindowState)));
          ViewWindow.CaptionPanel.OnDblClick(ViewWindow.CaptionPanel);
          Check(Cases,'views.restore.'+IntToStr(ViewIndex),ViewWindow.WindowState=wsNormal,'second double click restores','state='+IntToStr(Ord(ViewWindow.WindowState)));
        end;
      end;
      // User changed the live behavior: outside input must leave all views visible.
      for ViewIndex:=0 to Form.MDIChildCount-1 do begin
        Application.NotifyUserInputHandler(Form.GetMDIChildren(ViewIndex).Viewport,LM_LBUTTONDOWN);
        for OtherViewIndex:=0 to Form.MDIChildCount-1 do
          Check(Cases,'views.input-preserves-all.'+IntToStr(ViewIndex)+'.'+IntToStr(OtherViewIndex),
            Form.GetMDIChildren(OtherViewIndex).Visible,'clicking another view does not hide this view',
            'visible='+SysUtils.BoolToStr(Form.GetMDIChildren(OtherViewIndex).Visible,True));
      end;
      Application.NotifyUserInputHandler(TControl(Form.FindComponent('StudioToolbar')),LM_LBUTTONDOWN);
      for ViewIndex:=0 to Form.MDIChildCount-1 do
        Check(Cases,'views.toolbar-preserves.'+IntToStr(ViewIndex),Form.GetMDIChildren(ViewIndex).Visible,
          'toolbar outside input leaves view visible','visible='+SysUtils.BoolToStr(Form.GetMDIChildren(ViewIndex).Visible,True));
      CaptionTint:=0;
      if ProbeDwmGet(PtrUInt(Form.Handle),35,@CaptionTint,SizeOf(CaptionTint))>=0 then
        Check(Cases,'chrome.native-caption-color',CaptionTint=$00E8ECE0,
          'native title uses sea green tint',IntToHex(CaptionTint,8))
      else NotRun(Cases,'chrome.native-caption-color','custom title tint','DWM caption colors unsupported on this system');
      ModalCheck:=TModalHandleCheck.Create;ModalTimer:=TTimer.Create(nil);
      try
        ModalTimer.Enabled:=False;ModalTimer.Interval:=30;ModalTimer.OnTimer:=@ModalCheck.CloseModal;
        for ModalIndex:=0 to 1 do begin
          if ModalIndex=0 then ModalCheck.Dialog:=TFreeProjectSettingsDialog.Create(Form)
          else ModalCheck.Dialog:=TFreeIntersectionDialog.Create(Form);
          try
            ModalCheck.Dialog.Position:=poDesigned;ModalCheck.Dialog.SetBounds(-20000,-20000,900,700);
            ModalCheck.Dialog.ShowInTaskBar:=stNever;
            ModalCheck.Dialog.HandleNeeded;
            ModalCheck.OriginalHandle:=PtrUInt(ModalCheck.Dialog.Handle);ModalCheck.Stable:=False;
            ModalTimer.Enabled:=True;
            ModalCheck.Dialog.ShowModal;
            ModalTimer.Enabled:=False;
            Check(Cases,'dialog.modal-handle-stable.'+IntToStr(ModalIndex),ModalCheck.Stable,
              'dialog enters modal state without recreating native handle','stable='+SysUtils.BoolToStr(ModalCheck.Stable,True));
          finally ModalCheck.Dialog.Free end;
        end;
        NotRun(Cases,'dialog.cursor-visual','no cursor blink on physical toolbar click',
          'hidden modal handle check does not sample physical cursor or screen frames');
      finally ModalTimer.Free;ModalCheck.Free end;
      ToolbarFixture:=TPanel(Form.FindComponent('StudioToolbar'));
      if Assigned(ToolbarFixture) then begin
      Root.Add('toolbar_fixture_scope','Temporarily fixed-width toolbar panel inside the hidden test form; not native desktop acceptance');
      SavedToolbarAlign:=ToolbarFixture.Align;ToolbarFixture.Align:=alNone;
      SavedLanguage:=Form.FreeShip.Preferences.Language;
      SavedWidth:=Form.Width;SavedHeight:=Form.Height;SavedWindowState:=Form.WindowState;
      try
        Form.WindowState:=wsNormal;
        for LanguageIndex:=0 to 6 do begin
          case LanguageIndex of
            0:StateName:='Chinese';1:StateName:='Russian';2:StateName:='English';
            3:StateName:='French';4:StateName:='German';5:StateName:='Spanish';
            else StateName:='Japanese';
          end;
          Check(Cases,'toolbar.language-load.'+StateName,Form.FreeShip.Preferences.TrySetInterfaceLanguage(StateName),
            'selectable language loads before layout capture',StateName);
          for WidthIndex:=0 to 2 do begin
            case WidthIndex of 0:TargetWidth:=1920;1:TargetWidth:=1280;else TargetWidth:=900 end;
            Form.SetBounds(Form.Left,Form.Top,TargetWidth,1000);
            LCLIntf.SetWindowPos(Form.Handle,0,0,0,TargetWidth,1000,$0016);Form.Realign;
            ToolbarFixture.SetBounds(0,0,TargetWidth,ToolbarFixture.Height);
            Form.RefreshStudioLanguage;
            if TargetWidth=1920 then begin
              Check(Cases,'toolbar.project-group-visible.'+StateName,
                TControl(Form.FindComponent('StudioProjectGroup')).Visible,
                'project section remains directly visible at desktop width','language='+StateName);
            end;
            NetBtn:=ButtonFor(Form,Form.ShowControlNet);
            for ViewIndex:=0 to 3 do begin
              ViewButton:=TSpeedButton(Form.FindComponent('StudioWindowToggle'+IntToStr(ViewIndex)));
              Check(Cases,'toolbar.view-presentation.'+StateName+'.'+IntToStr(TargetWidth)+'.'+IntToStr(ViewIndex),
                (ViewButton.Layout=blGlyphTop) and (ViewButton.Spacing=NetBtn.Spacing) and
                (ViewButton.Margin=NetBtn.Margin) and (ViewButton.Top=NetBtn.Top) and (ViewButton.Height=NetBtn.Height),
                'view toggles share grid icon and caption alignment','language='+StateName);
            end;
            OverflowFixture:=TPopupMenu(Form.FindComponent('StudioOverflowMenu'));
            if Assigned(OverflowFixture) and Assigned(OverflowFixture.OnPopup) then OverflowFixture.OnPopup(OverflowFixture);
            CaptureInventory(Form,Root,'toolbar_fixture_'+StateName+'_'+IntToStr(TargetWidth));
            ToolbarImage:=Graphics.TBitmap.Create;
            try
              RenderNativeControl(ToolbarFixture,ToolbarImage);
              ToolbarImagePath:=IncludeTrailingPathDelimiter(GetEnvironmentVariable('FS_UI_PROBE_OUTPUT'))+
                'toolbar-'+StateName+'-'+IntToStr(TargetWidth)+'.bmp';
              ToolbarImage.SaveToFile(ToolbarImagePath);
            finally ToolbarImage.Free end;
          end;
        end;
      finally
        ToolbarFixture.Align:=SavedToolbarAlign;
        Form.FreeShip.Preferences.TrySetInterfaceLanguage(SavedLanguage);
        Form.SetBounds(Form.Left,Form.Top,SavedWidth,SavedHeight);Form.WindowState:=SavedWindowState;
        Form.RefreshStudioLanguage;
      end;
      end else NotRun(Cases,'toolbar.dimension-fixture','toolbar panel observations','known adapter unavailable');
      NotRun(Cases,'toolbar.redo-physical-shortcut','real Ctrl+R performs redo',
        'shortcut binding inspected without physical input or focus');
      if Assigned(NetAction) and NetAction.Enabled then begin
        WasVisible:=Form.FreeShip.Visibility.ShowControlNet;
        NetAction.Execute;
        Check(Cases,'toolbar.control-net-toggle',Form.FreeShip.Visibility.ShowControlNet <> WasVisible,
          'discovered control-net action toggles visibility', 'action='+NetAction.Name+'; '+SysUtils.BoolToStr(Form.FreeShip.Visibility.ShowControlNet, True));
        if Assigned(NetBtn) then Check(Cases,'toolbar.control-net-state',NetBtn.Down = Form.FreeShip.Visibility.ShowControlNet,
          'bound button state matches control-net visibility',SysUtils.BoolToStr(NetBtn.Down,True))
        else NotRun(Cases,'toolbar.control-net-state','bound button state','no button was discovered');
        NetAction.Execute;
        Check(Cases,'toolbar.control-net-restore',Form.FreeShip.Visibility.ShowControlNet = WasVisible,
          'second activation restores visibility',SysUtils.BoolToStr(Form.FreeShip.Visibility.ShowControlNet,True));
      end else begin
        NotRun(Cases,'toolbar.control-net-toggle','unique enabled control-net action','not found, ambiguous or disabled; matches='+IntToStr(NetMatches));
        NotRun(Cases,'toolbar.control-net-state','button state','button missing');
        NotRun(Cases,'toolbar.control-net-restore','visibility restored','button missing');
      end;
      if Assigned(PointAction) and PointAction.Enabled then begin
        Form.FreeShip.EditMode := emSelectItems; Form.UpdateMenu; PointAction.Execute;
        Check(Cases,'toolbar.point-mode-on',Form.FreeShip.EditMode = emAddPoint,
          'point button enters add-point mode',IntToStr(Ord(Form.FreeShip.EditMode)));
        if Assigned(PointBtn) then Check(Cases,'toolbar.point-button-state',PointBtn.Down,
          'bound button indicates active add-point mode',SysUtils.BoolToStr(PointBtn.Down,True))
        else NotRun(Cases,'toolbar.point-button-state','bound button state','no button was discovered');
        PointAction.Execute;
        Check(Cases,'toolbar.point-mode-off',Form.FreeShip.EditMode = emSelectItems,
          'second activation returns to select mode',IntToStr(Ord(Form.FreeShip.EditMode)));
      end else begin
        NotRun(Cases,'toolbar.point-mode-on','unique enabled point action','not found, ambiguous or disabled; matches='+IntToStr(PointMatches));
        NotRun(Cases,'toolbar.point-button-state','active point mode displayed','button missing');
        NotRun(Cases,'toolbar.point-mode-off','select mode restored','button missing');
      end;
      NotRun(Cases,'toolbar.actual-layout','screen layout and mouse activation',
        'hidden components and direct handlers only');
    end;
    if (Scope='all') or (Scope='chinese') then begin
      Check(Cases,'chinese.can-select',Form.FreeShip.Preferences.TrySetInterfaceLanguage('Chinese'),
        'Chinese language resource available and selectable',Form.FreeShip.Preferences.Language);
      Form.RefreshStudioLanguage;
      Check(Cases,'chinese.language',SameText(Form.FreeShip.Preferences.Language,'Chinese'),
        'Chinese selected in isolated test profile',Form.FreeShip.Preferences.Language);
      CaptureInventory(Form,Root,'chinese_toolbar');
      NotRun(Cases,'chinese.full-coverage','all UI translations and text rendering',
        'inventory records observed labels; full translation and rendering require review');
    end;
    if (Scope='all') or (Scope='language') then begin
      SelectedPoint:=Form.FreeShip.Surface.ControlPoint[5];
      Form.FreeShip.ControlpointForm.ActiveControlPoint:=SelectedPoint;
      Editor:=FindEditByHandler(Form.FreeShip.ControlpointForm,@Form.FreeShip.ControlpointForm.EditNameEditingDone,EditorMatches);
      if Assigned(Editor) then Editor.Text:='Probe-123';
      PointCount:=Form.FreeShip.Surface.NumberOfControlPoints;
      Check(Cases,'language.select-russian',Form.FreeShip.Preferences.TrySetInterfaceLanguage('Russian'),
        'Russian resource can be selected',Form.FreeShip.Preferences.Language);
      Form.RefreshStudioLanguage;
      CaptureInventory(Form,Root,'language_russian');
      if Assigned(Editor) then Check(Cases,'language.russian-preserves-input',Editor.Text='Probe-123',
        'discovered point name draft remains unchanged',Editor.Text)
      else NotRun(Cases,'language.russian-preserves-input','unique point name editor','matches='+IntToStr(EditorMatches));
      Check(Cases,'language.russian-preserves-model',
        (Form.FreeShip.Surface.NumberOfControlPoints=PointCount) and
        (Form.FreeShip.ControlpointForm.ActiveControlPoint=SelectedPoint),
        'model point count and editor binding remain unchanged',IntToStr(Form.FreeShip.Surface.NumberOfControlPoints));
      Check(Cases,'language.select-chinese',Form.FreeShip.Preferences.TrySetInterfaceLanguage('Chinese'),
        'Chinese resource can be selected again',Form.FreeShip.Preferences.Language);
      Form.RefreshStudioLanguage;
      CaptureInventory(Form,Root,'language_chinese');
      if Assigned(Editor) then Check(Cases,'language.chinese-preserves-input',Editor.Text='Probe-123',
        'discovered point name draft remains unchanged',Editor.Text)
      else NotRun(Cases,'language.chinese-preserves-input','unique point name editor','matches='+IntToStr(EditorMatches));
      Form.FreeShip.ControlpointForm.ActiveControlPoint:=nil;
      NotRun(Cases,'language.real-focused-input','focused editor and real input during language change',
        'control text checked without desktop focus or native keyboard input');
    end;
  end;
  if Scope='language' then begin RunLanguageMenus(Form,Cases,Root);RunSelectedDetails(Form,Cases,Root) end;
  if (Scope='all') or (Scope='layers') then RunLayers(Form,Cases,Root);
end;
end.


