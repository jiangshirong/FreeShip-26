unit UiProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure RunProbe(Form: TMainForm);
implementation
uses Windows, SysUtils, Classes, Types, Graphics, Controls, Menus, fpjson, TypInfo, LMessages, Math,
  StdCtrls, Spin, Buttons,
  FreeGeometry, FreeTypes, FreeStringsUnit, freehullformwindow_panel, FreeControlPointFrm, UiTrace, UiExpanded, UiDisplay, UiShortcuts, UiLabels, UiPerformance, UiPersistence, UiModels, UiHistory, UiInventory, UiDynamic, UiDiscovery;

procedure AddCase(Cases: TJSONArray; const Id, Status, Expected, Actual: string);
var O: TJSONObject;
begin
  O := TJSONObject.Create;
  O.Add('id', Id); O.Add('status', Status);
  O.Add('expected', Expected); O.Add('actual', Actual); Cases.Add(O);
end;

procedure Check(Cases: TJSONArray; const Id: string; Ok: Boolean;
  const Expected, Actual: string);
begin
  if Ok then AddCase(Cases, Id, 'pass', Expected, Actual)
  else AddCase(Cases, Id, 'fail', Expected, Actual);
end;

function HasText(Trace: TJSONArray; const S: string): Boolean;
var I: Integer;
begin
  Result := False;
  for I := 0 to Trace.Count - 1 do
    if Trace.Objects[I].Get('text', '') = S then Exit(True);
end;

procedure CheckCoordinateEditors(Form:TMainForm; Cases:TJSONArray; const Id:string; const Expected:array of Double);
var Items:TFPList; Event:TNotifyEvent; I,J,Count:Integer; Used:array of Boolean;
    S:string; Match:Boolean; V:Double;
begin
  Items:=TFPList.Create;
  try
    Event:=@Form.FreeShip.ControlpointForm.OrdinateEditorEditingDone;
    FindComponentsByEvent(Form.FreeShip.ControlpointForm,'OnEditingDone',TMethod(Event),Items);
    Count:=0; S:=''; SetLength(Used,Length(Expected)); Match:=True;
    for I:=0 to Items.Count-1 do if TObject(Items[I]) is TFloatSpinEdit then begin
      Inc(Count); V:=TFloatSpinEdit(Items[I]).Value;
      S:=S+TFloatSpinEdit(Items[I]).Name+'='+FloatToStr(V)+'; ';
      for J:=0 to High(Expected) do if (not Used[J]) and (Abs(V-Expected[J])<0.0001) then begin Used[J]:=True; Break end;
    end;
    if Count<>Length(Expected) then AddCase(Cases,Id,'not_run','unique coordinate editor set',
      'candidate count='+IntToStr(Count)+'; '+S)
    else begin
      for J:=0 to High(Expected) do if not Used[J] then Match:=False;
      Check(Cases,Id,Match,'fixture coordinate values as an unordered set',S);
    end;
  finally Items.Free end;
end;

procedure PrepareHiddenHandles(Control: TWinControl);
var I: Integer;
begin
  Control.HandleNeeded;
  for I := 0 to Control.ControlCount - 1 do
    if Control.Controls[I] is TWinControl then
      PrepareHiddenHandles(TWinControl(Control.Controls[I]));
end;

function IsLeaf(Control: TControl): Boolean;
begin
  Result := (Control is TCustomLabel) or (Control is TFloatSpinEdit) or
    (Control is TCustomEdit) or (Control is TCustomCheckBox) or (Control is TSpeedButton);
end;

procedure CollectControls(Control: TWinControl; X, Y: Integer; Items: TJSONArray; AncestorVisible: Boolean; const ParentClip:TRect);
var I,CX,CY:Integer; C:TControl; O,ClipJson:TJSONObject; Prop:PPropInfo; Clip,ChildClip:TRect;
begin
  for I := 0 to Control.ControlCount - 1 do begin
    C := Control.Controls[I]; CX := X + C.Left; CY := Y + C.Top;
    O := TJSONObject.Create;
    O.Add('name', C.Name); O.Add('class', C.ClassName); O.Add('parent', Control.Name);
    O.Add('x', CX); O.Add('y', CY); O.Add('width', C.Width); O.Add('height', C.Height);
    O.Add('visible', AncestorVisible and C.Visible); O.Add('visible_local', C.Visible);
    O.Add('enabled', C.Enabled); O.Add('leaf', IsLeaf(C));
    Clip:=Types.Rect(Max(CX,ParentClip.Left),Max(CY,ParentClip.Top),
      Min(CX+C.Width,ParentClip.Right),Min(CY+C.Height,ParentClip.Bottom));
    Clip.Right:=Max(Clip.Left,Clip.Right); Clip.Bottom:=Max(Clip.Top,Clip.Bottom);
    ClipJson:=TJSONObject.Create; ClipJson.Add('left',Clip.Left); ClipJson.Add('top',Clip.Top);
    ClipJson.Add('right',Clip.Right); ClipJson.Add('bottom',Clip.Bottom); O.Add('clip_rect',ClipJson);
    Prop := GetPropInfo(C, 'Caption');
    if Prop <> nil then O.Add('caption', GetStrProp(C, Prop));
    Prop := GetPropInfo(C, 'Text');
    if Prop <> nil then O.Add('text', GetStrProp(C, Prop));
    if C is TFloatSpinEdit then O.Add('value', TFloatSpinEdit(C).Value);
    Items.Add(O);
    if (C is TWinControl) and not IsLeaf(C) then begin
      ChildClip:=Clip;
      ChildClip.Right:=Min(ChildClip.Right,CX+TWinControl(C).ClientWidth);
      ChildClip.Bottom:=Min(ChildClip.Bottom,CY+TWinControl(C).ClientHeight);
      CollectControls(TWinControl(C), CX, CY, Items, AncestorVisible and C.Visible,ChildClip);
    end;
  end;
end;

procedure RenderHiddenChildren(Control: TWinControl; Canvas: TCanvas; X, Y: Integer; GraphicOnly: Boolean);
var I, CX, CY, Saved: Integer; C: TControl;
begin
  for I := 0 to Control.ControlCount - 1 do begin
    C := Control.Controls[I]; if not C.Visible then Continue;
    CX := X + C.Left; CY := Y + C.Top;
    if C is TWinControl then begin
      if not GraphicOnly then begin
      Saved := SaveDC(Canvas.Handle);
      try
        SetViewportOrgEx(Canvas.Handle, CX, CY, nil);
        IntersectClipRect(Canvas.Handle, 0, 0, C.Width, C.Height);
        Windows.SendMessage(TWinControl(C).Handle, WM_PRINT, WPARAM(Canvas.Handle),
          PRF_CLIENT or PRF_NONCLIENT or PRF_ERASEBKGND);
      finally RestoreDC(Canvas.Handle, Saved); end;
      end;
      if not IsLeaf(C) then RenderHiddenChildren(TWinControl(C), Canvas, CX, CY, GraphicOnly);
    end else if GraphicOnly and (C is TGraphicControl) then begin
      Saved := SaveDC(Canvas.Handle);
      try
        SetViewportOrgEx(Canvas.Handle, CX, CY, nil);
        IntersectClipRect(Canvas.Handle, 0, 0, C.Width, C.Height);
        C.Perform(LM_PAINT, WPARAM(Canvas.Handle), 0);
      finally RestoreDC(Canvas.Handle, Saved); end;
    end;
  end;
end;

procedure RenderSettled(Form:TMainForm; V:TFreeViewport; B:TBitmap;
  out Texts,Placements,Leaders:TJSONArray; out Settled:Boolean);
var Frame:Integer;
begin
  Texts:=nil; Placements:=nil; Leaders:=nil; Settled:=False;
  for Frame:=1 to 30 do begin
    B.Canvas.Brush.Color:=clWhite; B.Canvas.FillRect(0,0,B.Width,B.Height);
    V.DrawingCanvas:=B.Canvas; BeginTextTrace; Form.FreeShip.DrawToViewport(V);
    Texts:=EndTextTrace; Placements:=EndPlacementTrace; Leaders:=EndLeaderTrace;
    Settled:=not V.LabelLayout.Animating;
    if Settled or (Frame=30) then Exit;
    Texts.Free; Placements.Free; Leaders.Free; Sleep(20);
  end;
end;

procedure TestCoordinateVisibility(Form:TMainForm; Cases:TJSONArray; const WhichView:string; WhichAxis:Integer);
var V:TFreeViewport; B:TBitmap; P:TFreeSubdivisionControlPoint; T,Place,Leaders:TJSONArray;
    Count,Index:Integer; Selected,Visible,Settled:Boolean; LabelText,ExpectedLabel:string;
  procedure Capture(const Id:string; ExpectedCount,ExpectedIndex:Integer; ExpectSelected:Boolean);
  var I:Integer;
  begin
    RenderSettled(Form,V,B,T,Place,Leaders,Settled);
    Count:=0; Index:=-1; Selected:=False; Visible:=False; LabelText:='';
    for I:=0 to Place.Count-1 do if Place.Objects[I].Get('category','')='point' then begin
      Inc(Count); Index:=Place.Objects[I].Get('point_index',-1);
      Selected:=Place.Objects[I].Get('selected',False);
      Visible:=Place.Objects[I].Get('visible',False); LabelText:=Place.Objects[I].Get('text','');
    end;
    if not Settled and (ExpectedCount>0) and not Visible then
      AddCase(Cases,Id,'not_run','selected point label visible','layout still animating after bounded offscreen redraws')
    else Check(Cases,Id,(Count=ExpectedCount) and
      ((ExpectedCount=0) or ((Index=ExpectedIndex) and (Selected=ExpectSelected) and
       Visible and (LabelText=ExpectedLabel))),
      Format('point placements=%d; point index=%d; selected=%s',
        [ExpectedCount,ExpectedIndex,SysUtils.BoolToStr(ExpectSelected,True)]),
      Format('point placements=%d; point index=%d; selected=%s; visible=%s; text=%s',
        [Count,Index,SysUtils.BoolToStr(Selected,True),SysUtils.BoolToStr(Visible,True),LabelText]));
    Cases.Objects[Cases.Count-1].Add('text_trace',T);
    Cases.Objects[Cases.Count-1].Add('placement_trace',Place);
    Cases.Objects[Cases.Count-1].Add('leader_trace',Leaders);
  end;
begin
  V:=Form.GetMDIChildren(0).Viewport; P:=Form.FreeShip.Surface.ControlPoint[5];
  V.Align:=alNone; V.SetBounds(0,0,800,500);
  if WhichView='profile' then V.ViewType:=fvProfile
  else if WhichView='plan' then V.ViewType:=fvPlan
  else if WhichView='bodyplan' then V.ViewType:=fvBodyplan
  else V.ViewType:=fvPerspective;
  if WhichAxis<0 then WhichAxis:=1;
  case WhichAxis of
    0: ExpectedLabel:='P5'; 1: ExpectedLabel:='20.000';
    2: ExpectedLabel:='3.000'; 3: ExpectedLabel:='8.000';
  end;
  V.ViewportMode:=vmWireframe; V.ControlPointCoordinateAxis:=WhichAxis;
  V.InitializeViewport(Form.FreeShip.Surface.Min,Form.FreeShip.Surface.Max);
  Form.FreeShip.Surface.ClearSelection;
  B:=TBitmap.Create;
  try
    B.SetSize(800,500);
    Form.FreeShip.Visibility.ShowControlNet:=False;
    Capture('coordinates.visibility.control-net-off',0,-1,False);
    Form.FreeShip.Visibility.ShowControlNet:=True;
    Form.FreeShip.Layer[0].ControlNetVisible:=False;
    Capture('coordinates.visibility.layer-net-off',0,-1,False);
    P.Selected:=True;
    Capture('coordinates.visibility.selected-hidden-layer',1,5,True);
  finally
    P.Selected:=False; Form.FreeShip.Layer[0].ControlNetVisible:=True;
    Form.FreeShip.Visibility.ShowControlNet:=True;
    V.DrawingCanvas:=V.Canvas; B.Free;
  end;
end;

procedure RunProbe(Form: TMainForm);
var Root, CaseItem, PanelData: TJSONObject; Cases, Trace, ControlsData: TJSONArray;
    View: TFreeHullWindow; VP: TFreeViewport; P: TFreeSubdivisionControlPoint;
    Menu,AxisItem: TMenuItem; Img: TBitmap; I,J,K,CheckedCount, WhichAxis,Matches: Integer; S, ImagePath, OutDir, Scope, WhichView, Prefix: String;
    Expected: array[0..3] of string = ('P5','20.000','3.000','8.000');
    Views: array[0..3] of TFreeViewType = (fvPerspective,fvProfile,fvPlan,fvBodyplan);
    ViewNames: array[0..3] of string = ('perspective','profile','plan','bodyplan');
    Coord: T3DCoordinate; FileOut: TStringList; PreviewCleared,AllFound,Settled:Boolean; OriginalLanguage:String;
    ParentMenu:TMenu; PopupHandler,KnownPopup:TNotifyEvent;
    PlacementData,LeaderData:TJSONArray; LanguageFiles:TSearchRec; LanguageCodeName,LanguageKey:String; LanguageAttempts:TJSONArray; LanguageAttempt:TJSONObject; RussianCaptured,ChineseCaptured:Boolean;
begin
  OutDir := GetEnvironmentVariable('FS_UI_PROBE_OUTPUT');
  Scope := GetEnvironmentVariable('FS_UI_PROBE_CASE');
  WhichView := GetEnvironmentVariable('FS_UI_PROBE_VIEW');
  WhichAxis := StrToIntDef(GetEnvironmentVariable('FS_UI_PROBE_AXIS'), -1);
  Root := TJSONObject.Create;
  Cases := TJSONArray.Create; Root.Add('cases', Cases);
  Root.Add('scope', 'instrumented desktop source; hidden components; native mouse and display not exercised');
  try
    try
      if Scope='dynamic' then RunDynamic(Form,Root,Cases)
      else if Scope='models' then RunModels(Form,Root,Cases,OutDir)
      else if Scope='performance' then RunPerformance(Form,Root,Cases)
      else begin
      Form.FormShow(Form); // Install the desktop form's normal model listeners.
      if (Scope='all') or (Scope='inventory') or (Scope='toolbar') or (Scope='chinese') or (Scope='language') then CaptureInventory(Form,Root,'no_model');
      TestShortcutsBeforeModel(Form, Scope, Cases);
      Form.LoadNamedFile(GetEnvironmentVariable('FS_UI_PROBE_MODEL'));
      Check(Cases, 'model.fixture', Form.FreeShip.ModelIsLoaded and
        (Form.FreeShip.Surface.NumberOfControlPoints = 8),
        'box fixture loaded with 8 control points',
        'loaded=' + SysUtils.BoolToStr(Form.FreeShip.ModelIsLoaded, True) +
        ', points=' + IntToStr(Form.FreeShip.Surface.NumberOfControlPoints));
      if not Form.FreeShip.ModelIsLoaded or (Form.FreeShip.Surface.NumberOfControlPoints <> 8) then
        raise Exception.Create('Fixture unavailable; dependent checks not run');
      CheckedCount:=0; PreviewCleared:=True;
      for I:=0 to Form.MDIChildCount-1 do begin
        VP:=Form.GetMDIChildren(I).Viewport;
        if VP.ProgressivePreview then begin VP.FinishProgressiveDisplay(True); Inc(CheckedCount) end;
        PreviewCleared:=PreviewCleared and not VP.ProgressivePreview;
      end;
      Check(Cases,'model.preview-transition',(Form.MDIChildCount=4) and PreviewCleared,
        'four viewports prepared for final-frame offscreen tests',
        'views='+IntToStr(Form.MDIChildCount)+'; previews advanced='+IntToStr(CheckedCount));
      AddCase(Cases,'model.preview-real-timer','not_run','loading preview advances on real event loop',
        'test skips the animation using the in-process completion method; no desktop event loop');
      P := Form.FreeShip.Surface.ControlPoint[5];
      Coord := P.Coordinate;
      Check(Cases, 'model.fixture.point5', (Abs(Coord.X-20)<0.0001) and
        (Abs(Coord.Y-3)<0.0001) and (Abs(Coord.Z-8)<0.0001),
        'point 5 = (20, 3, 8)', Format('(%.4f, %.4f, %.4f)', [Coord.X,Coord.Y,Coord.Z]));
      P.Name := 'P5';
      if (Scope='all') or (Scope='inventory') or (Scope='toolbar') or (Scope='chinese') or (Scope='language') then begin
        Form.UpdateMenu; CaptureInventory(Form,Root,'model_loaded');
        P.Selected:=True; Form.UpdateMenu; CaptureInventory(Form,Root,'point_selected');
        P.Selected:=False; Form.UpdateMenu;
        OriginalLanguage:=Form.FreeShip.Preferences.Language; RussianCaptured:=False; ChineseCaptured:=False;
        if Form.FreeShip.Preferences.TrySetInterfaceLanguage('Russian') then begin
          Form.RefreshStudioLanguage; CaptureInventory(Form,Root,'russian_model'); RussianCaptured:=True;
        end;
        if Form.FreeShip.Preferences.TrySetInterfaceLanguage('Chinese') then begin
          Form.RefreshStudioLanguage; CaptureInventory(Form,Root,'chinese_model'); ChineseCaptured:=True;
        end;
        LanguageAttempts:=TJSONArray.Create; Root.Add('text_language_attempts',LanguageAttempts);
        if FindFirst(IncludeTrailingPathDelimiter(Form.FreeShip.Preferences.LanguagesDirectory)+'FreeShip.*.po',faAnyFile,LanguageFiles)=0 then begin
          repeat
            LanguageCodeName:=Copy(LanguageFiles.Name,Length('FreeShip.')+1,Length(LanguageFiles.Name)-Length('FreeShip.')-Length('.po'));
            LanguageKey:=LanguageName(LanguageCodeName);
            LanguageAttempt:=TJSONObject.Create; LanguageAttempt.Add('file',LanguageFiles.Name); LanguageAttempt.Add('code',LanguageCodeName); LanguageAttempt.Add('language',LanguageKey);
            if ((LanguageKey='Russian') and RussianCaptured) or ((LanguageKey='Chinese') and ChineseCaptured) then LanguageAttempt.Add('status','captured_existing_snapshot')
            else if (LanguageKey<>'') and Form.FreeShip.Preferences.TrySetInterfaceLanguage(LanguageKey) then begin
              Form.RefreshStudioLanguage; Form.UpdateMenu; CaptureInventory(Form,Root,'language_'+LanguageCodeName); LanguageAttempt.Add('status','captured');
            end else LanguageAttempt.Add('status','not_run');
            LanguageAttempts.Add(LanguageAttempt);
          until FindNext(LanguageFiles)<>0;
          FindClose(LanguageFiles);
        end;
        if Form.FreeShip.Preferences.TrySetInterfaceLanguage(OriginalLanguage) then Form.RefreshStudioLanguage;
        AddCase(Cases,'inventory.coverage','not_run','all dynamic menus, native focus and real input',
          'component snapshots only; popup and dropdown callbacks were not invoked');
      end;
      TestShortcutsWithModel(Form, Scope, Cases);
      if Form.MDIChildCount < 1 then raise Exception.Create('No hull viewport created');
      View := Form.GetMDIChildren(0); VP := View.Viewport;
      View.SetBounds(0,0,900,600); VP.Align := alNone; VP.SetBounds(0,0,800,500);
      VP.ViewType := fvPerspective; VP.ViewportMode := vmWireframe;
      VP.InitializeViewport(Form.FreeShip.Surface.Min, Form.FreeShip.Surface.Max);
      Form.FreeShip.Visibility.ShowControlNet := True;
      if (Scope='all') or (Scope='coordinates') then begin
      Img := TBitmap.Create;
      try
        Img.SetSize(800,500);
        for K := 0 to 3 do begin
        if (WhichView <> 'all') and (WhichView <> ViewNames[K]) then Continue;
        VP.ViewType := Views[K];
        VP.InitializeViewport(Form.FreeShip.Surface.Min, Form.FreeShip.Surface.Max);
        Prefix := 'view.'+ViewNames[K]+'.';
        for I := 0 to 3 do begin
          if (WhichAxis >= 0) and (I <> 0) and (I <> WhichAxis) then Continue;
          Menu:=nil; Matches:=0;
          if SysUtils.GetEnvironmentVariable('FS_UI_DYNAMIC_HULL')='1' then
            Menu:=FindMenuItemByHandler(View,View.UiProbeCoordinateHandler,I,Matches);
          if not Assigned(Menu) then begin
            AddCase(Cases,Prefix+'menu.axis.'+IntToStr(I),'not_run','unique coordinate handler and tag','matches='+IntToStr(Matches));
            AddCase(Cases,Prefix+'menu.checked.'+IntToStr(I),'not_run','selected axis checked','entry not uniquely discovered');
            AddCase(Cases,Prefix+'draw.label.'+IntToStr(I),'not_run','requested label drawn','entry not uniquely discovered');
            Continue;
          end;
          Menu.Click;
          Check(Cases, Prefix+'menu.axis.'+IntToStr(I), VP.ControlPointCoordinateAxis = I,
            IntToStr(I), IntToStr(VP.ControlPointCoordinateAxis));
          ParentMenu:=Menu.GetParentMenu; PopupHandler:=nil; KnownPopup:=@View.PopupMenuHullPopup;
          if ParentMenu is TPopupMenu then PopupHandler:=TPopupMenu(ParentMenu).OnPopup;
          if Assigned(PopupHandler) and (TMethod(PopupHandler).Code=TMethod(KnownPopup).Code) and
             (TMethod(PopupHandler).Data=TMethod(KnownPopup).Data) then PopupHandler(ParentMenu)
          else PopupHandler:=nil;
          CheckedCount := 0;
          for J := 0 to 3 do begin
            AxisItem:=FindMenuItemByHandler(View,View.UiProbeCoordinateHandler,J,Matches);
            if Assigned(AxisItem) and AxisItem.Checked then Inc(CheckedCount);
          end;
          if Assigned(PopupHandler) then
            Check(Cases, Prefix+'menu.checked.'+IntToStr(I), Menu.Checked and (CheckedCount=1),
              'selected axis checked exclusively', 'checked count='+IntToStr(CheckedCount))
          else AddCase(Cases,Prefix+'menu.checked.'+IntToStr(I),'not_run',
            'selected axis checked exclusively','parent refresh callback not recognized');
          RenderSettled(Form,VP,Img,Trace,PlacementData,LeaderData,Settled);
          S := Expected[I];
          if not Settled and not HasText(Trace,S) then
            AddCase(Cases,Prefix+'draw.label.'+IntToStr(I),'not_run',S,'layout still animating after bounded offscreen redraws')
          else Check(Cases, Prefix+'draw.label.'+IntToStr(I), HasText(Trace,S), S,
            'text_calls='+IntToStr(Trace.Count));
          ImagePath := IncludeTrailingPathDelimiter(OutDir)+ViewNames[K]+'-axis-'+IntToStr(I)+'.bmp';
          Img.SaveToFile(ImagePath);
          CaseItem := Cases.Objects[Cases.Count-1];
          CaseItem.Add('image', ExtractFileName(ImagePath)); CaseItem.Add('text_trace', Trace);
          CaseItem.Add('placement_trace',PlacementData); CaseItem.Add('leader_trace',LeaderData);
          CaseItem.Add('layout_settled',Settled);
          CaseItem.Add('viewport_client_width',VP.ClientWidth); CaseItem.Add('viewport_client_height',VP.ClientHeight);
          CaseItem.Add('bitmap_width',Img.Width); CaseItem.Add('bitmap_height',Img.Height);
        end;
        end;
        VP.DrawingCanvas := VP.Canvas;
      finally Img.Free; end;
      if Form.MDIChildCount >= 4 then begin
        AllFound:=True;
        for K:=0 to 3 do begin
          View:=Form.GetMDIChildren(K); Menu:=nil; Matches:=0;
          if SysUtils.GetEnvironmentVariable('FS_UI_DYNAMIC_HULL')='1' then
            Menu:=FindMenuItemByHandler(View,View.UiProbeCoordinateHandler,K,Matches);
          if Assigned(Menu) then Menu.Click else AllFound:=False;
        end;
        S:='';
        for K:=0 to 3 do S:=S+IntToStr(Form.GetMDIChildren(K).Viewport.ControlPointCoordinateAxis);
        if AllFound then Check(Cases, 'coordinates.independent-views', S='0123',
          'four views hold off/X/Y/Z independently',S)
        else AddCase(Cases,'coordinates.independent-views','not_run',
          'four unique entries independently exercised','observed='+S+'; missing or ambiguous entries cannot prove behavior');
      end else AddCase(Cases,'coordinates.independent-views','not_run','four independent views','fewer than four views');
      TestCoordinateVisibility(Form,Cases,WhichView,WhichAxis);
      end;
      if (Scope='all') or (Scope='panel') then begin
      Form.FreeShip.ControlpointForm.ActiveControlPoint := P;
      Check(Cases, 'panel.bound-point', Form.FreeShip.ControlpointForm.ActiveControlPoint = P,
        'point 5 selected in editor', 'point matches=' +
        SysUtils.BoolToStr(Form.FreeShip.ControlpointForm.ActiveControlPoint = P, True));
      CheckCoordinateEditors(Form,Cases,'panel.coordinates',[20,3,8]);
      ControlsData := TJSONArray.Create;
      CollectControls(Form.FreeShip.ControlpointForm, 0, 0, ControlsData, True,
        Types.Rect(0,0,Form.FreeShip.ControlpointForm.ClientWidth,Form.FreeShip.ControlpointForm.ClientHeight));
      PanelData := TJSONObject.Create; PanelData.Add('controls', ControlsData);
      PanelData.Add('client_width', Form.FreeShip.ControlpointForm.ClientWidth);
      PanelData.Add('client_height', Form.FreeShip.ControlpointForm.ClientHeight);
      Root.Add('panel', PanelData);
      Img := TBitmap.Create;
      try
        Img.SetSize(Form.FreeShip.ControlpointForm.ClientWidth, Form.FreeShip.ControlpointForm.ClientHeight);
        Img.Canvas.Brush.Color := clWhite;
        Img.Canvas.FillRect(0,0,Img.Width,Img.Height);
        // Ask this hidden form's own native widget to render to our bitmap.
        // LCL PaintTo requires the form to be visible; WM_PRINT without
        // PRF_CHECKVISIBLE permits rendering without showing a desktop window.
        PrepareHiddenHandles(Form.FreeShip.ControlpointForm);
        RenderHiddenChildren(Form.FreeShip.ControlpointForm, Img.Canvas, 0, 0, False);
        Img.SaveToFile(IncludeTrailingPathDelimiter(OutDir)+'panel.bmp');
        Img.Canvas.Brush.Color := clWhite; Img.Canvas.FillRect(0,0,Img.Width,Img.Height);
        RenderHiddenChildren(Form.FreeShip.ControlpointForm, Img.Canvas, 0, 0, True);
        Img.SaveToFile(IncludeTrailingPathDelimiter(OutDir)+'panel-labels.bmp');
      finally Img.Free; end;
      AddCase(Cases, 'panel.image-review', 'not_run',
        'editor is legible and correctly laid out', 'offscreen rendering requires visual review');
      Cases.Objects[Cases.Count-1].Add('image','panel.bmp');
      Cases.Objects[Cases.Count-1].Add('label_image','panel-labels.bmp');
      Form.FreeShip.ControlpointForm.ActiveControlPoint := Form.FreeShip.Surface.ControlPoint[0];
      CheckCoordinateEditors(Form,Cases,'panel.switch-point',[0,0,0]);
      Form.FreeShip.ControlpointForm.ActiveControlPoint := nil;
      Check(Cases, 'panel.clear-binding', Form.FreeShip.ControlpointForm.ActiveControlPoint = nil,
        'no bound point', SysUtils.BoolToStr(Form.FreeShip.ControlpointForm.ActiveControlPoint = nil, True));
      AddCase(Cases, 'desktop.popup-visibility', 'not_run',
        'native click opens and dismisses popup', 'native desktop input and focus not exercised');
      AddCase(Cases, 'desktop.window-layout', 'not_run',
        'actual on-screen window placement and DPI', 'offscreen viewport only');
      end;
      RunDisplay(Form, Scope, Cases);
      RunExpanded(Form, Scope, Cases, Root);
      RunLabels(Form, Scope, OutDir, Cases);
      RunPersistence(Form, Scope, Cases);
      RunHistory(Form, Scope, Cases);
      end;
    except on E: Exception do begin
      Root.Add('error', E.ClassName+': '+E.Message);
      AddCase(Cases, 'probe.execution', 'error', 'probe completes', E.ClassName+': '+E.Message);
    end; end;
    FileOut := TStringList.Create;
    try FileOut.Text := Root.FormatJSON; FileOut.SaveToFile(IncludeTrailingPathDelimiter(OutDir)+'probe.json');
    finally FileOut.Free; end;
  finally Root.Free; end;
end;
end.
