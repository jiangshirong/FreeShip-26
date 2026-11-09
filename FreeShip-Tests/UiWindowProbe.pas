unit UiWindowProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure RunWindowProbe(Form:TMainForm);
implementation
uses SysUtils, Classes, Math, Types, Windows, Controls, Forms, Graphics, LMessages, LCLType,
  fpjson, ExtCtrls, MDIPanel, FreeTypes, FreeGeometry, freehullformwindow_panel, UiWindowEvents;
type
  TObserver=class
    Original:TNotifyEvent; Count:Integer; Milliseconds:Double;
    procedure Redraw(Sender:TObject);
  end;
procedure TObserver.Redraw(Sender:TObject);
var A,B,F:Int64;
begin
  QueryPerformanceFrequency(F); QueryPerformanceCounter(A); Inc(Count);
  try if Assigned(Original) then Original(Sender)
  finally QueryPerformanceCounter(B); if F>0 then Milliseconds:=Milliseconds+(B-A)*1000.0/F end;
end;
procedure AddCase(Cases:TJSONArray; const Id,Status,Expected:string; Actual:TJSONData);
var O:TJSONObject;
begin O:=TJSONObject.Create; O.Add('id',Id); O.Add('status',Status); O.Add('expected',Expected); O.Add('actual',Actual); Cases.Add(O) end;
procedure Check(Cases:TJSONArray; const Id:string; Ok:Boolean; const Expected:string; Actual:TJSONData);
begin if Ok then AddCase(Cases,Id,'pass',Expected,Actual) else AddCase(Cases,Id,'fail',Expected,Actual) end;
function RJson(const R:TRect):TJSONObject;
begin Result:=TJSONObject.Create; Result.Add('left',R.Left); Result.Add('top',R.Top); Result.Add('right',R.Right); Result.Add('bottom',R.Bottom) end;
function Contains(const Outer,Inner:TRect):Boolean;
begin Result:=(Outer.Left<=Inner.Left) and (Outer.Top<=Inner.Top) and (Outer.Right>=Inner.Right) and (Outer.Bottom>=Inner.Bottom) end;
function Counts(Obs:array of TObserver):TJSONArray;
var I:Integer;
begin Result:=TJSONArray.Create; for I:=0 to High(Obs) do Result.Add(Obs[I].Count) end;
function AllFrozen(Form:TMainForm; Wanted:Boolean):Boolean;
var I:Integer;
begin Result:=True; for I:=0 to Form.MDIChildCount-1 do if Form.GetMDIChildren(I).Viewport.PresentationFrozen<>Wanted then Exit(False) end;
procedure RunWindowProbe(Form:TMainForm);
var Root,Obj,Sample,Event,RectJson:TJSONObject; Cases,Samples,Events,DirtyEvents,Siblings:TJSONArray;
    Views:array[0..4] of TFreeHullWindow; Obs:array[0..4] of TObserver;
    I,J,CountBefore,N,SnapX,SnapY:Integer; SavedCursor:TCursor; First,After:TRect; V:TFreeViewport; OutDir,Err:string;
    OldBounds:array[0..3] of TRect; Unique,Ok,Matched:Boolean; PosData:TWindowPos; ModelBefore,CacheBefore,CacheAfter,Fraction:Double; StartProgressive,StartReveal,ClockBefore,ClockElapsed,DeltaProgressive,DeltaReveal:QWord; OldAlign:TAlign; PanelOld:TRect; PointItem:TFreeSubdivisionControlPoint; OriginalName:String; Tick0,Tick1,Freq:Int64; TMove,TPaint,TModel:Double; EraseResult:PtrInt; Text:TStringList;
begin
  Root:=TJSONObject.Create; Cases:=TJSONArray.Create; Root.Add('cases',Cases);
  Root.Add('scope','in-process state, hidden widget painting, model redraw and redraw requests; no desktop input or real compositor');
  OutDir:=SysUtils.GetEnvironmentVariable('FS_UI_PROBE_OUTPUT');
  for I:=0 to 4 do begin Views[I]:=nil; Obs[I]:=nil end;
  try
    try
      Form.FormShow(Form); Form.LoadNamedFile(SysUtils.GetEnvironmentVariable('FS_UI_PROBE_MODEL'));
      if not Form.FreeShip.ModelIsLoaded or (Form.MDIChildCount<4) then raise Exception.Create('fixture or four viewports unavailable');
      if SysUtils.GetEnvironmentVariable('FS_UI_WINDOW_SUBCASE')<>'new-view' then begin
      Ok:=not Assigned(Application.OnActivate);
      if not Ok then Ok:=(TMethod(Application.OnActivate).Code<>TMethod(Form.OnActivate).Code) or
        (TMethod(Application.OnActivate).Data<>TMethod(Form.OnActivate).Data);
      Check(Cases,'window.application-activation-routing',Ok,
        'application reactivation must not dispatch the main-window activation callback and steal an independent window selection',
        TJSONBoolean.Create(Ok));
      end;
      if SysUtils.GetEnvironmentVariable('FS_UI_WINDOW_SUBCASE')<>'new-view' then
      for I:=0 to 3 do begin
        V:=Form.GetMDIChildren(I).Viewport;SavedCursor:=V.Cursor;
        Form.GetMDIChildren(I).ViewportMouseMove(V,[],6,V.ClientHeight div 2);
        Check(Cases,'window.resize-hit-left.'+IntToStr(I),V.Cursor=crSizeWE,'six pixels inside the left drawing edge shows resize cursor',TJSONIntegerNumber.Create(Ord(V.Cursor)));
        Form.GetMDIChildren(I).ViewportMouseMove(V,[],V.ClientWidth-7,V.ClientHeight div 2);
        Check(Cases,'window.resize-hit-right.'+IntToStr(I),V.Cursor=crSizeWE,'six pixels inside the right drawing edge shows resize cursor',TJSONIntegerNumber.Create(Ord(V.Cursor)));
        Form.GetMDIChildren(I).ViewportMouseMove(V,[],V.ClientWidth div 2,V.ClientHeight-7);
        Check(Cases,'window.resize-hit-bottom.'+IntToStr(I),V.Cursor=crSizeNS,'six pixels inside bottom drawing edge shows resize cursor',TJSONIntegerNumber.Create(Ord(V.Cursor)));
        Form.GetMDIChildren(I).ViewportMouseMove(V,[],V.ClientWidth div 2,V.ClientHeight div 2);
        Check(Cases,'window.resize-hit-release.'+IntToStr(I),V.Cursor=SavedCursor,'leaving the edge restores the original cursor',TJSONIntegerNumber.Create(Ord(V.Cursor)));
      end;
      if SysUtils.GetEnvironmentVariable('FS_UI_WINDOW_SUBCASE')='new-view' then begin
      try
        Form.NewWindowExecute(Form); N:=Form.MDIChildCount;
        if N>=5 then begin
          Views[4]:=Form.GetMDIChildren(N-1);
          if not Assigned(Views[4].Viewport) then
            AddCase(Cases,'window.new-view-lifecycle','not_run','newly created view participates in freeze/unfreeze',TJSONString.Create('new child has no viewport in hidden test process'))
          else begin
            Views[4].UiWindowMotion(wpsMoving); Ok:=AllFrozen(Form,True);
            Views[4].UiWindowMotion(wpsNone); Ok:=Ok and AllFrozen(Form,False);
            Check(Cases,'window.new-view-lifecycle',Ok,'newly created view participates in freeze/unfreeze',TJSONIntegerNumber.Create(N));
          end;
        end else AddCase(Cases,'window.new-view-lifecycle','not_run','new view participates',TJSONString.Create('new view not created'));
      except on E:Exception do begin
        Obj:=TJSONObject.Create; Obj.Add('exception',E.ClassName+': '+E.Message);
        Obj.Add('mdi_children_after',Form.MDIChildCount);
        if Form.MDIChildCount>=5 then begin
          Views[4]:=Form.GetMDIChildren(Form.MDIChildCount-1);
          Obj.Add('new_viewport_assigned',Assigned(Views[4].Viewport));
          Obj.Add('new_parent_assigned',Assigned(Views[4].Parent));
        end;
        AddCase(Cases,'window.new-view-lifecycle','fail','new view creation and motion should complete in hidden test process',Obj);
      end end;
      end else begin
      Form.MainClientPanel.HandleNeeded;
      for I:=0 to 3 do begin
        Views[I]:=Form.GetMDIChildren(I); V:=Views[I].Viewport;
        V.FinishProgressiveDisplay(True); Views[I].HandleNeeded; V.HandleNeeded;
        Obs[I]:=TObserver.Create; Obs[I].Original:=V.OnRedraw; V.OnRedraw:=@Obs[I].Redraw;
        V.DrawingCanvas:=V.Canvas; V.UiWindowPaint;
      end;
      Ok:=True; for I:=0 to 3 do Ok:=Ok and Views[I].Viewport.UiWindowBufferValid and (Obs[I].Count>0);
      Check(Cases,'window.initial-real-model-buffer',Ok,'four viewports render the fixture to valid buffers',Counts(Slice(Obs,4)));
      Events:=TJSONArray.Create; for I:=0 to 3 do Events.Add(Obs[I].Milliseconds); Root.Add('initial_model_redraw_ms',Events);
      for I:=0 to 3 do Views[I].Viewport.UiWindowBuffer.SaveToFile(IncludeTrailingPathDelimiter(OutDir)+'window-buffer-before-'+IntToStr(I)+'.bmp');
      EraseResult:=Views[0].Viewport.Perform(LM_ERASEBKGND,0,0);
      Check(Cases,'window.erase-handled',EraseResult=1,'hidden viewport claims background erase to avoid an early blank',TJSONIntegerNumber.Create(EraseResult));
      FillChar(PosData,SizeOf(PosData),0);
      Views[0].Perform(LM_WINDOWPOSCHANGING,0,LPARAM(@PosData));
      Check(Cases,'window.position-no-copy-bits',(PosData.flags and SWP_NOCOPYBITS)<>0,
        'in-process window position message requests no stale bit copying',TJSONIntegerNumber.Create(PosData.flags));
      SnapX:=Views[1].Left-3; SnapY:=3;
      if Views[0].UiWindowSnap(SnapX,SnapY) then begin
        Obj:=TJSONObject.Create; Obj.Add('x',SnapX); Obj.Add('y',SnapY); Obj.Add('sibling_x',Views[1].Left);
        Check(Cases,'window.snap-neighbor',(SnapX=Views[1].Left) and (SnapY=0),'nearby sibling and parent edges attract the moving view',Obj);
        SnapX:=65; SnapY:=73; Views[0].UiWindowSnap(SnapX,SnapY);
        Obj:=TJSONObject.Create; Obj.Add('x',SnapX); Obj.Add('y',SnapY);
        Check(Cases,'window.snap-free-move',(SnapX=65) and (SnapY=73),'position away from nearby edges remains free',Obj);
      end else begin
        AddCase(Cases,'window.snap-neighbor','not_run','discover supported snapping behavior',TJSONString.Create('snap adapter unavailable'));
        AddCase(Cases,'window.snap-free-move','not_run','discover supported snapping behavior',TJSONString.Create('snap adapter unavailable'));
      end;
      Views[0].UiWindowMotion(wpsMoving);
      Check(Cases,'window.motion-start-all',AllFrozen(Form,True),'moving a view freezes all current viewports',TJSONBoolean.Create(AllFrozen(Form,True)));
      Samples:=TJSONArray.Create; DirtyEvents:=TJSONArray.Create; Root.Add('move_samples',Samples); Root.Add('redraw_requests',DirtyEvents);
      QueryPerformanceFrequency(Freq); CountBefore:=0; for I:=0 to 3 do Inc(CountBefore,Obs[I].Count);
      Ok:=True;
      for I:=1 to 40 do begin
        First:=Views[0].BoundsRect; ModelBefore:=0; CacheBefore:=0;
        for J:=0 to 3 do begin ModelBefore:=ModelBefore+Obs[J].Milliseconds; CacheBefore:=CacheBefore+CacheCopyMilliseconds(Views[J].Viewport) end;
        ResetWindowEvents; QueryPerformanceCounter(Tick0);
        Views[0].SetBounds(20+(I mod 2)*30,20+(I mod 3)*15,Views[0].Width,Views[0].Height);
        QueryPerformanceCounter(Tick1); TMove:=(Tick1-Tick0)*1000.0/Freq; After:=Views[0].BoundsRect;
        Events:=TakeWindowEvents; Matched:=False;
        Sample:=TJSONObject.Create; Sample.Add('iteration',I); Sample.Add('move_ms',TMove);
        Sample.Add('old_bounds',RJson(First)); Sample.Add('new_bounds',RJson(After));
        Siblings:=TJSONArray.Create; for J:=1 to 3 do Siblings.Add(RJson(Views[J].BoundsRect)); Sample.Add('sibling_bounds',Siblings);
        Sample.Add('requests',Events.Clone); DirtyEvents.Add(Events);
        QueryPerformanceCounter(Tick0);
        for J:=0 to 3 do Views[J].Viewport.UiWindowPaint;
        QueryPerformanceCounter(Tick1); TPaint:=(Tick1-Tick0)*1000.0/Freq;
        TModel:=0; for J:=0 to 3 do TModel:=TModel+Obs[J].Milliseconds;
        CacheAfter:=0; for J:=0 to 3 do CacheAfter:=CacheAfter+CacheCopyMilliseconds(Views[J].Viewport);
        Sample.Add('four_paints_ms',TPaint); Sample.Add('model_redraw_ms',TModel-ModelBefore); Sample.Add('cache_copy_ms',CacheAfter-CacheBefore);
        Sample.Add('redraw_counts',Counts(Slice(Obs,4)));
        Events:=TJSONArray.Create; for J:=0 to 3 do Events.Add(PaintCount(Views[J].Viewport)); Sample.Add('paint_counts',Events);
        Events:=TJSONArray.Create; for J:=0 to 3 do Events.Add(CacheCopyCount(Views[J].Viewport)); Sample.Add('cache_copy_counts',Events);
        Samples.Add(Sample);
      end;
      N:=0; for I:=0 to 3 do Inc(N,Obs[I].Count);
      Obj:=TJSONObject.Create; Obj.Add('before',CountBefore); Obj.Add('after',N); Obj.Add('per_view',Counts(Slice(Obs,4)));
      Check(Cases,'window.frozen-reuses-model-buffer',N=CountBefore,'moving with valid buffers does not repeat model redraws',Obj);
      Views[0].UiWindowMotion(wpsNone);
      Check(Cases,'window.motion-end-all',AllFrozen(Form,False),'ending motion unfreezes all viewports',TJSONBoolean.Create(AllFrozen(Form,False)));
      CountBefore:=Obs[0].Count; Views[0].Viewport.UiWindowPaint;
      Check(Cases,'window.unfreeze-redraw',Obs[0].Count>CountBefore,'post-motion paint redraws the model',TJSONIntegerNumber.Create(Obs[0].Count-CountBefore));
      for I:=0 to 3 do Views[I].Viewport.UiWindowBuffer.SaveToFile(IncludeTrailingPathDelimiter(OutDir)+'window-buffer-after-'+IntToStr(I)+'.bmp');
      Views[0].UiWindowMotion(wpsResizing);
      V:=Views[0].Viewport; CountBefore:=Obs[0].Count;
      First:=Views[0].BoundsRect; ResetWindowEvents;
      Views[0].SetBounds(Views[0].Left,Views[0].Top,Views[0].Width+50,Views[0].Height+30);
      After:=Views[0].BoundsRect; Events:=TakeWindowEvents;
      Sample:=TJSONObject.Create; Sample.Add('old_bounds',RJson(First)); Sample.Add('new_bounds',RJson(After));
      Sample.Add('requests',Events); Root.Add('resize_sample',Sample);
      V.UiWindowPaint;
      Check(Cases,'window.resize-frozen-cache',Obs[0].Count=CountBefore,'resizing with valid buffer reuses model rendering',TJSONIntegerNumber.Create(Obs[0].Count-CountBefore));
      Views[0].UiWindowMotion(wpsNone); V.UiWindowPaint;
      Obj:=TJSONObject.Create; Obj.Add('width',V.UiWindowBuffer.Width); Obj.Add('height',V.UiWindowBuffer.Height); Obj.Add('client_width',V.ClientWidth); Obj.Add('client_height',V.ClientHeight);
      Check(Cases,'window.resize-rebuild-size',(V.UiWindowBuffer.Width=V.ClientWidth) and (V.UiWindowBuffer.Height=V.ClientHeight),
        'unfrozen buffer is rebuilt at the current viewport size',Obj);
      V.FinishProgressiveDisplay(True); V.UiWindowInvalidateBuffer;
      Views[0].UiWindowMotion(wpsMoving); CountBefore:=Obs[0].Count; V.UiWindowPaint;
      Check(Cases,'window.invalid-cache-first-redraw',Obs[0].Count=CountBefore+1,'test-invalidated buffer triggers one normal model redraw while frozen',TJSONIntegerNumber.Create(Obs[0].Count-CountBefore));
      CountBefore:=Obs[0].Count; V.UiWindowPaint;
      Check(Cases,'window.invalid-cache-subsequent-reuse',Obs[0].Count=CountBefore,'subsequent frozen paint reuses the rebuilt buffer',TJSONIntegerNumber.Create(Obs[0].Count-CountBefore));
      Views[0].UiWindowMotion(wpsNone); V.FinishProgressiveDisplay(True);
      V.BeginProgressiveDisplay; V.PreviewFrameReady; Ok:=V.UiWindowPreviewTimerEnabled;
      Views[0].UiWindowMotion(wpsMoving); Ok:=Ok and not V.UiWindowPreviewTimerEnabled;
      Views[0].UiWindowMotion(wpsNone); Ok:=Ok and V.UiWindowPreviewTimerEnabled;
      Check(Cases,'window.preview-timer-state',Ok,'preview timer pauses and resumes around in-process motion',TJSONBoolean.Create(Ok));
      V.BeginProgressiveDisplay; Fraction:=V.ProgressiveFraction; V.FinishProgressiveDisplay(False);
      StartProgressive:=V.UiWindowProgressiveStart; StartReveal:=V.UiWindowRevealStart;
      ClockBefore:=SysUtils.GetTickCount64; Views[0].UiWindowMotion(wpsMoving); Sleep(80); Views[0].UiWindowMotion(wpsNone);
      ClockElapsed:=SysUtils.GetTickCount64-ClockBefore;
      DeltaProgressive:=V.UiWindowProgressiveStart-StartProgressive; DeltaReveal:=V.UiWindowRevealStart-StartReveal;
      Obj:=TJSONObject.Create; Obj.Add('elapsed_ms',Int64(ClockElapsed)); Obj.Add('progressive_shift_ms',Int64(DeltaProgressive)); Obj.Add('reveal_shift_ms',Int64(DeltaReveal));
      Ok:=(StartProgressive<>0) and (StartReveal<>0) and (DeltaProgressive>=60) and (DeltaReveal>=60) and
        (DeltaProgressive<=ClockElapsed+15) and (DeltaReveal<=ClockElapsed+15);
      Check(Cases,'window.timer-pause-compensation',Ok,'animation time origins advance by the in-process frozen duration',Obj);
      V.FinishProgressiveDisplay(True);
      // Change only a point name in the private fixture; no save or desktop cursor changes.
      PointItem:=Form.FreeShip.Surface.ControlPoint[5]; OriginalName:=PointItem.Name;
      Form.FreeShip.Visibility.ShowControlNet:=True; V.ControlPointCoordinateAxis:=0;
      PointItem.Name:='Cache-Before'; ClearDrawnText(V); V.UiWindowPaint;
      Obj:=TJSONObject.Create; Obj.Add('before',HasDrawnText(V,'Cache-Before'));
      V.UiWindowBuffer.SaveToFile(IncludeTrailingPathDelimiter(OutDir)+'window-model-before.bmp');
      Views[0].UiWindowMotion(wpsMoving); PointItem.Name:='Cache-After-Changed'; ClearDrawnText(V); V.UiWindowPaint;
      Obj.Add('frozen',HasDrawnText(V,'Cache-After-Changed'));
      V.UiWindowBuffer.SaveToFile(IncludeTrailingPathDelimiter(OutDir)+'window-model-frozen.bmp');
      Views[0].UiWindowMotion(wpsNone); ClearDrawnText(V); V.UiWindowPaint;
      Obj.Add('thawed',HasDrawnText(V,'Cache-After-Changed')); Root.Add('model_text_evidence',Obj);
      V.UiWindowBuffer.SaveToFile(IncludeTrailingPathDelimiter(OutDir)+'window-model-thawed.bmp');
      PointItem.Name:=OriginalName;
      for I:=0 to 3 do begin Views[I].WindowState:=wsNormal; OldBounds[I]:=Views[I].BoundsRect end;
      for I:=0 to 3 do Views[I].WindowState:=wsMinimized;
      Unique:=True; for I:=0 to 3 do for J:=I+1 to 3 do if (Views[I].Left=Views[J].Left) and (Views[I].Top=Views[J].Top) then Unique:=False;
      Obj:=TJSONObject.Create; for I:=0 to 3 do Obj.Add('view'+IntToStr(I),RJson(Views[I].BoundsRect));
      Check(Cases,'window.minimize-distinct',Unique,'four minimized views have distinct positions',Obj);
      for I:=0 to 3 do Views[I].WindowState:=wsNormal;
      Ok:=True; for I:=0 to 3 do Ok:=Ok and EqualRect(Views[I].BoundsRect,OldBounds[I]);
      Check(Cases,'window.restore-original-bounds',Ok,'restoring minimized views preserves normal bounds',TJSONBoolean.Create(Ok));
      Views[1].BringToFront; Views[0].WindowState:=wsMaximized;
      if (GetParent(Views[0].Handle)=Form.MainClientPanel.Handle) then
        Check(Cases,'window.maximize-z-order',GetWindow(Form.MainClientPanel.Handle,GW_CHILD)=Views[0].Handle,
          'maximized view is first in hidden parent child z-order',TJSONBoolean.Create(GetWindow(Form.MainClientPanel.Handle,GW_CHILD)=Views[0].Handle))
      else AddCase(Cases,'window.maximize-z-order','not_run','hidden child z-order',TJSONString.Create('unexpected native parent'));
      Views[0].WindowState:=wsMaximized; Views[0].WindowState:=wsNormal;
      Check(Cases,'window.repeated-maximize-restores',EqualRect(Views[0].BoundsRect,OldBounds[0]),'repeated maximize preserves original normal bounds',RJson(Views[0].BoundsRect));
      Views[0].WindowState:=wsMinimized; Views[1].WindowState:=wsMaximized; Form.TileWindowExecute(Form);
      Ok:=True; for I:=0 to 3 do Ok:=Ok and (Views[I].WindowState=wsNormal);
      Check(Cases,'window.tile-restores-state',Ok,'tile returns the original four views to normal state',TJSONBoolean.Create(Ok));
      // Exercise resize of a hidden parent while one view is minimized and another maximized.
      Form.TileWindowExecute(Form);
      if Assigned(Form.MainClientPanel.OnResize) then Form.MainClientPanel.OnResize(Form.MainClientPanel);
      Views[0].WindowState:=wsMinimized; Views[1].WindowState:=wsMaximized;
      OldAlign:=Form.MainClientPanel.Align; PanelOld:=Form.MainClientPanel.BoundsRect;
      Form.MainClientPanel.Align:=alNone;
      Form.MainClientPanel.SetBounds(PanelOld.Left,PanelOld.Top,Max(300,PanelOld.Right-PanelOld.Left-150),Max(200,PanelOld.Bottom-PanelOld.Top-80));
      if Assigned(Form.MainClientPanel.OnResize) then Form.MainClientPanel.OnResize(Form.MainClientPanel);
      Obj:=TJSONObject.Create; Obj.Add('maximized_bounds',RJson(Views[1].BoundsRect)); Obj.Add('minimized_bounds',RJson(Views[0].BoundsRect));
      Obj.Add('parent_width',Form.MainClientPanel.ClientWidth); Obj.Add('parent_height',Form.MainClientPanel.ClientHeight);
      Ok:=(Views[1].Left=0) and (Views[1].Top=0) and (Views[1].Width=Form.MainClientPanel.ClientWidth) and
        (Views[1].Height=Form.MainClientPanel.ClientHeight) and (Views[0].Left>=0) and (Views[0].Top>=0) and
        (Views[0].Left+Views[0].Width<=Form.MainClientPanel.ClientWidth) and (Views[0].Top+Views[0].Height<=Form.MainClientPanel.ClientHeight);
      Check(Cases,'window.resize-parent-mixed-states',Ok,'maximized and minimized views fit the resized hidden parent',Obj);
      Form.MainClientPanel.SetBounds(PanelOld.Left,PanelOld.Top,PanelOld.Right-PanelOld.Left,PanelOld.Bottom-PanelOld.Top);
      Form.MainClientPanel.Align:=OldAlign; Form.TileWindowExecute(Form);
      AddCase(Cases,'window.real-stacking','not_run','verify real on-screen stacking after maximize',TJSONString.Create('hidden child z-order does not prove desktop composition'));
      AddCase(Cases,'window.capture-cancel-focus','not_run','capture loss, cancellation, titlebar exit and focus paths unfreeze',TJSONString.Create('motion state was invoked directly; no mouse capture, focus, or native desktop input'));
      AddCase(Cases,'window.caption-prepaint','not_run','no blank background or inherited caption is presented before cached frame',TJSONString.Create('hidden buffer and erase return code do not reveal intermediate on-screen frames'));
      AddCase(Cases,'window.compositor-trails-flicker','not_run','screen contains no trails or blank flicker during real motion',TJSONString.Create('hidden control, redraw requests and cached bitmaps do not prove DWM composition'));
      AddCase(Cases,'window.print-export','not_run','printing and export do not reuse frozen screen cache',TJSONString.Create('no printer or export workflow was exercised'));
      AddCase(Cases,'window.timer-recovery','not_run','animation timers recover in a real event loop after motion',TJSONString.Create('no normal desktop event loop was exercised'));
      AddCase(Cases,'window.performance-threshold','review','assess hidden-process measurements against an agreed reference',TJSONString.Create('no hardware-specific performance threshold or real pointer motion provided'));
      Root.Add('model_redraw_ms',TJSONFloatNumber.Create(Obs[0].Milliseconds));
      end;
    except on E:Exception do begin Root.Add('error',E.ClassName+': '+E.Message); AddCase(Cases,'window.probe','error','probe completes',TJSONString.Create(E.ClassName+': '+E.Message)) end end;
    for I:=0 to 4 do if Assigned(Obs[I]) then begin if Assigned(Views[I]) then Views[I].Viewport.OnRedraw:=Obs[I].Original; Obs[I].Free end;
    Text:=TStringList.Create; try Text.Text:=Root.FormatJSON; Text.SaveToFile(IncludeTrailingPathDelimiter(OutDir)+'probe.json') finally Text.Free end;
  finally Root.Free end;
end;
end.
