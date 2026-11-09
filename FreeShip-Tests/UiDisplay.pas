unit UiDisplay;
{$mode objfpc}{$H+}
interface
uses Main, fpjson;
procedure RunDisplay(Form: TMainForm; const Scope: string; Cases: TJSONArray);
implementation
uses SysUtils, Classes, ActnList, FreeTypes, FreeGeometry, freehullformwindow_panel, UiDiscovery;

procedure Check(Cases: TJSONArray; const Id: string; Ok: Boolean;
  const Expected, Actual: string);
var O: TJSONObject;
begin
  O:=TJSONObject.Create; O.Add('id',Id);
  if Ok then O.Add('status','pass') else O.Add('status','fail');
  O.Add('expected',Expected); O.Add('actual',Actual); Cases.Add(O);
end;

procedure NotRun(Cases: TJSONArray; const Id, Expected, Actual: string);
var O:TJSONObject;
begin
  O:=TJSONObject.Create; O.Add('id',Id); O.Add('status','not_run');
  O.Add('expected',Expected); O.Add('actual',Actual); Cases.Add(O);
end;

function VisibilityFor(Form: TMainForm; I: Integer): Boolean;
begin
  case I of
    0: Result:=Form.FreeShip.Visibility.ShowStations;
    1: Result:=Form.FreeShip.Visibility.ShowWaterlines;
    2: Result:=Form.FreeShip.Visibility.ShowButtocks;
    3: Result:=Form.FreeShip.Visibility.ShowDiagonals;
    else Result:=Form.FreeShip.Visibility.ShowGrid;
  end;
end;

procedure RunDisplay(Form: TMainForm; const Scope: string; Cases: TJSONArray);
var View,Other: TFreeHullWindow; V:TFreeViewport; OtherType:TFreeViewType;
    I,Matches: Integer; A,PointAction,NetAction,ClearAction:TAction; Before,After:Boolean;
    Names:array[0..4] of string = ('stations','waterlines','buttocks','diagonals','grid');
    Actions:array[0..4] of TAction;
    ViewNames:array[0..3] of string = ('profile','plan','bodyplan','perspective');
    ViewTypes:array[0..3] of TFreeViewType = (fvProfile,fvPlan,fvBodyplan,fvPerspective);
    ViewActions:array[0..3] of TAction;
    Modes:array[0..4] of TFreeViewportMode = (vmWireframe,vmShade,vmShadeGauss,vmShadeDevelopable,vmShadeZebra);
    ModeNames:array[0..4] of string = ('wireframe','shade','gaussian','developable','zebra');
    ModeActions:array[0..4] of TAction;
begin
  if (Scope<>'all') and (Scope<>'display') then Exit;
  View:=Form.GetMDIChildren(0); Other:=Form.GetMDIChildren(1); V:=View.Viewport;
  OtherType:=Other.Viewport.ViewType;
  // The former per-view handlers no longer exist; do not invent a mapping.
  for I:=0 to 3 do ViewActions[I]:=nil;
  for I:=0 to 3 do begin
    if not Assigned(ViewActions[I]) then begin
      NotRun(Cases,'display.view.'+ViewNames[I],'view action changes viewport','action not uniquely discovered');
      NotRun(Cases,'display.background-availability.'+ViewNames[I],'background state','view action not uniquely discovered');
      Continue;
    end;
    ViewActions[I].Execute; View.UpdateMenu;
    Check(Cases,'display.view.'+ViewNames[I], (V.ViewType=ViewTypes[I]) and ViewActions[I].Checked,
      'view action sets and checks the requested view',
      Format('action=%s; type=%d; checked=%s',[ViewActions[I].Name,Ord(V.ViewType),SysUtils.BoolToStr(ViewActions[I].Checked,True)]));
    Check(Cases,'display.background-availability.'+ViewNames[I],
      View.ImportBackGround.Enabled=(ViewTypes[I]<>fvPerspective),
      'background import available only in orthographic views',SysUtils.BoolToStr(View.ImportBackGround.Enabled,True));
  end;
  Check(Cases,'display.other-view-unchanged',Other.Viewport.ViewType=OtherType,
    'switching one view does not alter another view',IntToStr(Ord(Other.Viewport.ViewType)));
  ModeActions[0]:=FindActionByHandler(View,@View.ShowWireFrameExecute,Matches);
  ModeActions[1]:=FindActionByHandler(View,@View.ShowFlatShadeExecute,Matches);
  ModeActions[2]:=FindActionByHandler(View,@View.ShowGaussCurvatureExecute,Matches);
  ModeActions[3]:=FindActionByHandler(View,@View.ShowDevelopablityExecute,Matches);
  ModeActions[4]:=FindActionByHandler(View,@View.ShadeZebraExecute,Matches);
  if SysUtils.GetEnvironmentVariable('FS_UI_DYNAMIC_HULL')<>'1' then
    for I:=0 to 4 do ModeActions[I]:=nil;
  for I:=0 to 4 do begin
    if not Assigned(ModeActions[I]) then begin
      NotRun(Cases,'display.mode.'+ModeNames[I],'display action selects mode','action not uniquely discovered'); Continue;
    end;
    ModeActions[I].Execute; View.UpdateMenu;
    Check(Cases,'display.mode.'+ModeNames[I],(V.ViewportMode=Modes[I]) and ModeActions[I].Checked,
      'display mode selected and checked',
      Format('action=%s; mode=%d; checked=%s',[ModeActions[I].Name,Ord(V.ViewportMode),SysUtils.BoolToStr(ModeActions[I].Checked,True)]));
  end;
  V.ViewportMode:=vmWireframe; View.UpdateMenu;
  Actions[0]:=FindActionByHandler(Form,@Form.ShowStationsExecute,Matches);
  Actions[1]:=FindActionByHandler(Form,@Form.ShowWaterlinesExecute,Matches);
  Actions[2]:=FindActionByHandler(Form,@Form.ShowButtocksExecute,Matches);
  Actions[3]:=FindActionByHandler(Form,@Form.ShowDiagonalsExecute,Matches);
  Actions[4]:=FindActionByHandler(Form,@Form.ShowGridExecute,Matches);
  if SysUtils.GetEnvironmentVariable('FS_UI_DYNAMIC_MAIN')<>'1' then
    for I:=0 to 4 do Actions[I]:=nil;
  Form.UpdateMenu;
  for I:=0 to 4 do
    if Assigned(Actions[I]) then Check(Cases,'display.empty-disabled.'+Names[I],not Actions[I].Enabled,
      'display action disabled when fixture has no intersections',SysUtils.BoolToStr(Actions[I].Enabled,True))
    else NotRun(Cases,'display.empty-disabled.'+Names[I],'action state','action not uniquely discovered');
  Form.FreeShip.Edit.Intersection_Add(fiStation,10);
  Form.FreeShip.Edit.Intersection_Add(fiWaterline,4);
  Form.FreeShip.Edit.Intersection_Add(fiButtock,1);
  Form.FreeShip.Edit.Intersection_Add(fiDiagonal,4);
  Form.UpdateMenu;
  for I:=0 to 4 do begin
    if not Assigned(Actions[I]) then begin
      NotRun(Cases,'display.populated-enabled.'+Names[I],'action state','action not uniquely discovered');
      NotRun(Cases,'display.toggle.'+Names[I],'toggle behavior','action not uniquely discovered');
      NotRun(Cases,'display.restore.'+Names[I],'restore behavior','action not uniquely discovered'); Continue;
    end;
    Check(Cases,'display.populated-enabled.'+Names[I],Actions[I].Enabled,
      'display action enabled when corresponding intersections exist',SysUtils.BoolToStr(Actions[I].Enabled,True));
    if not Actions[I].Enabled then begin
      NotRun(Cases,'display.toggle.'+Names[I],'action toggles and its checked state follows','action disabled');
      NotRun(Cases,'display.restore.'+Names[I],'second action restores state','action disabled');
      Continue;
    end;
    Before:=VisibilityFor(Form,I); Actions[I].Execute; After:=VisibilityFor(Form,I);
    Check(Cases,'display.toggle.'+Names[I],(After<>Before) and (Actions[I].Checked=After),
      'action toggles visibility and checked state follows',
      Format('before=%s; after=%s; checked=%s',[SysUtils.BoolToStr(Before,True),SysUtils.BoolToStr(After,True),SysUtils.BoolToStr(Actions[I].Checked,True)]));
    Actions[I].Execute;
    Check(Cases,'display.restore.'+Names[I],VisibilityFor(Form,I)=Before,
      'second action restores original visibility',SysUtils.BoolToStr(VisibilityFor(Form,I),True));
  end;
  Form.FreeShip.Visibility.ShowControlNet:=True; Form.UpdateMenu;
  PointAction:=FindActionByHandler(Form,@Form.AddPointExecute,Matches);
  NetAction:=FindActionByHandler(Form,@Form.ShowControlNetExecute,Matches);
  if SysUtils.GetEnvironmentVariable('FS_UI_DYNAMIC_MAIN')<>'1' then begin PointAction:=nil; NetAction:=nil end;
  if Assigned(PointAction) and Assigned(NetAction) then begin
    Check(Cases,'display.point-enabled',PointAction.Enabled,
      'add-point action available with control net shown',SysUtils.BoolToStr(PointAction.Enabled,True));
    NetAction.Execute;
    Check(Cases,'display.point-disabled',not PointAction.Enabled,
      'add-point action disabled with control net hidden',SysUtils.BoolToStr(PointAction.Enabled,True));
    NetAction.Execute; Form.UpdateMenu;
  end else begin
    NotRun(Cases,'display.point-enabled','point action state','action not uniquely discovered');
    NotRun(Cases,'display.point-disabled','point action state','action not uniquely discovered');
  end;
  Form.FreeShip.Surface.ClearSelection; Form.FreeShip.ActiveControlPoint:=nil; Form.UpdateMenu;
  ClearAction:=FindActionByHandler(Form,@Form.DeselectAllExecute,Matches);
  if SysUtils.GetEnvironmentVariable('FS_UI_DYNAMIC_MAIN')<>'1' then ClearAction:=nil;
  if not Assigned(ClearAction) then begin
    NotRun(Cases,'display.deselect-disabled','deselect action state','action not uniquely discovered');
    NotRun(Cases,'display.deselect-enabled','deselect action state','action not uniquely discovered');
    NotRun(Cases,'display.deselect-action','selection cleared','action not uniquely discovered');
    NotRun(Cases,'display.actual-render-modes','real window rendering and human input','hidden controls only'); Exit;
  end;
  Check(Cases,'display.deselect-disabled',not ClearAction.Enabled,
    'deselect disabled with no selection',SysUtils.BoolToStr(ClearAction.Enabled,True));
  Form.FreeShip.Surface.ControlPoint[5].Selected:=True; Form.UpdateMenu;
  Check(Cases,'display.deselect-enabled',ClearAction.Enabled,
    'deselect enabled when a control point is selected',SysUtils.BoolToStr(ClearAction.Enabled,True));
  ClearAction.Execute; Form.UpdateMenu;
  Check(Cases,'display.deselect-action',(Form.FreeShip.NumberOfSelectedControlPoints=0) and not ClearAction.Enabled,
    'deselect clears selection and disables action',
    Format('selected=%d; enabled=%s',[Form.FreeShip.NumberOfSelectedControlPoints,SysUtils.BoolToStr(ClearAction.Enabled,True)]));
  NotRun(Cases,'display.actual-render-modes','real window rendering and human input',
    'mode state and action handlers checked; physical display and native input not exercised');
end;
end.
