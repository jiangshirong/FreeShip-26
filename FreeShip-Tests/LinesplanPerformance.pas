program LinesplanPerformance;
{$mode objfpc}{$H+}
{$APPTYPE CONSOLE}
uses Windows, Interfaces, Forms, SysUtils, Classes, Graphics, Controls, Types, LMessages,
 LineplanTextMetrics, FreeTypes, FreeGeometry, FreeFileBuffer, FreeShipUnit, FreeLinesplanFrme, FreeLinesplanFrm, FreeLabelLayout, FreeVectorFillCache;
{$R ../FreeShip.res}
type TFormBackgroundProbe=class(TFreeLinesplanForm)
 public procedure RenderBackground(DC:HDC);
end;
TFrameBackgroundProbe=class(TFreeLinesplanFrame)
 public procedure RenderBackground(DC:HDC);
end;
procedure TFormBackgroundProbe.RenderBackground(DC:HDC);
begin EraseBackground(DC) end;
procedure TFrameBackgroundProbe.RenderBackground(DC:HDC);
begin EraseBackground(DC) end;
type TProbe = class(TFreeViewport) public procedure Render; procedure Wheel; procedure WheelOut; procedure PanDown; procedure PanMove(X,Y:Integer); procedure PanUp; procedure SyncProjection; end;
 TBackgroundProbe=class
  Calls:Integer;
  function Cost(const R:TRect):Integer;
 end;
function TBackgroundProbe.Cost(const R:TRect):Integer;
begin Inc(Calls);Result:=0 end;
type TPresentationCanvas=class(TCanvas)
 public
 Clears,Blits,ScaledBlits:Integer;
 procedure FillRect(const R:TRect);override;
 procedure StretchDraw(const R:TRect;G:TGraphic);override;
end;
procedure TPresentationCanvas.FillRect(const R:TRect);
begin Inc(Clears);inherited FillRect(R) end;
procedure TPresentationCanvas.StretchDraw(const R:TRect;G:TGraphic);
begin
 Inc(Blits);
 if ((R.Right-R.Left)<>G.Width) or ((R.Bottom-R.Top)<>G.Height) then Inc(ScaledBlits);
 inherited StretchDraw(R,G);
end;

type TResizeDrawCounter=class
 Calls:Integer;Original:TNotifyEvent;
 procedure Draw(Sender:TObject);
end;
procedure TResizeDrawCounter.Draw(Sender:TObject);
begin Inc(Calls);if Assigned(Original) then Original(Sender) end;
procedure TProbe.Render; begin
 if TFreeViewport(Self) is TLinesplanViewport then TLinesplanViewport(TFreeViewport(Self)).ComposeFrame else Paint
end;
procedure TProbe.SyncProjection; begin InitializeViewport(Min3D,Max3D) end;
procedure TProbe.Wheel; begin DoMouseWheel([],120,ClientToScreen(Point(600,380))) end;
procedure TProbe.WheelOut; begin DoMouseWheel([],-120,ClientToScreen(Point(ClientWidth div 2,ClientHeight div 2))) end;
procedure TProbe.PanDown; begin MouseDown(mbRight,[ssRight],100,100) end;
procedure TProbe.PanMove(X,Y:Integer); begin MouseMove([ssRight],X,Y) end;
procedure TProbe.PanUp; begin MouseUp(mbRight,[],196,136) end;
var NativeRect:record R:array[0..2] of TRect;Pos:Pointer end;
 NativePos:Windows.WINDOWPOS;NativeResult:LRESULT;
 Startup:TFreeLinesplanForm; StartupHidden:Boolean;
 Host:TForm; Ship:TFreeShip; Frame:TFreeLinesplanFrame; Buffer:TFreeFileBuffer;
 T:QWord; QStart,QEnd,QFrequency:Int64; I,GapMS,Checked,Errors,TestWidth,TestHeight,PanSteps:Integer; GuardZoom:Single; PreviousPan,StartPan:TPoint; Image:TBitmap; Coord:T3DCoordinate;
procedure CheckVectorResources;
var Cache:TVectorFillCache;Reference:TBitmap;Pts:array[0..2]of TPoint;
 I,J:Integer;BeforeCount,AfterCount:DWORD;
begin
 Cache:=TVectorFillCache.Create;Reference:=TBitmap.Create;
 try
  Reference.SetSize(64,64);
  for I:=-1 to 7 do begin
   if I=0 then begin GdiFlush;BeforeCount:=GetGuiResources(GetCurrentProcess,0) end;
   if not Cache.BeginRecord(Reference.Canvas,0,0,10,10) then raise Exception.Create('vector recording unavailable');
   Pts[0]:=Cache.Project(2,2);Pts[1]:=Cache.Project(8,2);Pts[2]:=Cache.Project(5,8);
   for J:=0 to 599 do Cache.Polygon(Pts,TColor(J*1009));
   if not Cache.EndRecord then raise Exception.Create('vector close failed');
   Cache.Clear;
  end;
  GdiFlush;AfterCount:=GetGuiResources(GetCurrentProcess,0);
  WriteLn('GDIRESOURCES ',BeforeCount,' ',AfterCount);
  WriteLn('STARTUP vector_cache_resources_released ',Ord(AfterCount<=BeforeCount+4));
 finally Cache.Free;Reference.Free end;
end;

procedure CheckVectorGeometry;
var Cache:TVectorFillCache;Reference,ResultImage:TBitmap;Pts:array[0..2]of TPoint;
 X,Y,I:Integer;Painted,Expected:Boolean;Errors:Integer;R:TRect;
begin
 Cache:=TVectorFillCache.Create;Reference:=TBitmap.Create;ResultImage:=TBitmap.Create;
 try
  Reference.SetSize(64,64);
  if not Cache.BeginRecord(Reference.Canvas,-5,-2,15,8) then raise Exception.Create('vector geometry record unavailable');
  Pts[0]:=Cache.Project(-1,0);Pts[1]:=Cache.Project(11,0);Pts[2]:=Cache.Project(11,6);
  Cache.Polygon(Pts,clNavy);
  Pts[1]:=Cache.Project(-1,6);Cache.Polygon(Pts,clNavy);
  if not Cache.EndRecord then raise Exception.Create('vector geometry record failed');
  ResultImage.SetSize(1100,600);Errors:=0;
  for I:=0 to 1 do begin
   ResultImage.Canvas.Brush.Color:=clWhite;ResultImage.Canvas.FillRect(0,0,1100,600);
   if I=0 then R:=Rect(20,30,1020,530) else R:=Rect(-130,-55,1070,545);
   if not Cache.Draw(ResultImage.Canvas,R) then raise Exception.Create('vector geometry replay failed');
   for Y:=0 to 27 do for X:=0 to 46 do begin
    // A rectangle covering the central 60 percent in both model axes must
    // stay there for any destination size or translation. Skip edge pixels.
    Expected:=(X*23>=R.Left+(R.Right-R.Left)*0.2+4) and (X*23<=R.Left+(R.Right-R.Left)*0.8-4) and
      (Y*21>=R.Top+(R.Bottom-R.Top)*0.2+4) and (Y*21<=R.Top+(R.Bottom-R.Top)*0.8-4);
    if ((Abs(X*23-(R.Left+(R.Right-R.Left)*0.2))>4) and (Abs(X*23-(R.Left+(R.Right-R.Left)*0.8))>4) and
       (Abs(Y*21-(R.Top+(R.Bottom-R.Top)*0.2))>4) and (Abs(Y*21-(R.Top+(R.Bottom-R.Top)*0.8))>4)) and
       (X*23<ResultImage.Width) and (Y*21<ResultImage.Height) then begin
     Painted:=ColorToRGB(ResultImage.Canvas.Pixels[X*23,Y*21])<>ColorToRGB(clWhite);
     if Painted<>Expected then Inc(Errors);
    end;
   end;
  end;
  WriteLn('VECTORGEOMETRY physical_dpi ',GetDeviceCaps(Reference.Canvas.Handle,LOGPIXELSX),' errors ',Errors);
  WriteLn('STARTUP vector_geometry_aligned ',Ord(Errors=0));
  ResultImage.SaveToFile(ParamStr(2)+'/vector-geometry.bmp');
 finally Cache.Free;Reference.Free;ResultImage.Free end;
end;

procedure CheckTranslationRules;
var Layout:TFreeLabelLayout; Background:TBackgroundProbe; Before,After:TLabelPlacement; R:TRect;
 procedure Require(Value:Boolean; const Name:string);
 begin
  if not Value then raise Exception.Create('translation rule: '+Name);
  WriteLn('BEHAVIOR ',Name,' pass');
 end;
 procedure Prime(X,Y:Integer);
 begin
  Layout.Clear;Layout.TranslationOnly:=False;Layout.BeginFrame(Rect(0,0,1000,700),'rules');
  Before:=Layout.Place('label','alpha',Point(X,Y),Point(X,Y),50,15,lkNamedLine);
  Layout.EndFrame;Background.Calls:=0;Layout.TranslateHistory(8,3);
  Layout.TranslationOnly:=True;Layout.BeginFrame(Rect(0,0,1000,700),'rules');
 end;
begin
 Layout:=TFreeLabelLayout.Create;Background:=TBackgroundProbe.Create;
 try
  Layout.AnimationEnabled:=False;Layout.ExtraCost:=@Background.Cost;
  Prime(200,200);
  After:=Layout.Place('label','alpha',Point(208,203),Point(208,203),50,15,lkNamedLine);
  Require(After.Visible and (After.Position.X=Before.Position.X+8) and (After.Position.Y=Before.Position.Y+3) and
    (Background.Calls=0) and (Layout.ReusedPlacementCount=1),'translation.cached-interior');Layout.EndFrame;
  Prime(200,200);R:=Rect(208,203,258,218);Layout.Reserve(R);
  After:=Layout.Place('label','alpha',Point(208,203),Point(208,203),50,15,lkNamedLine);
  Require((Layout.ReusedPlacementCount=0) and (not After.Visible or not IntersectRect(R,After.Rect,R)),
    'translation.new-obstacle');Layout.EndFrame;
  Prime(200,200);
  After:=Layout.Place('label','beta',Point(208,203),Point(208,203),50,15,lkNamedLine);
  Require((Layout.ReusedPlacementCount=0) and (Background.Calls>0),'translation.changed-text');Layout.EndFrame;
  Prime(200,200);
  After:=Layout.Place('label','alpha',Point(208,203),Point(208,203),60,16,lkNamedLine);
  Require((Layout.ReusedPlacementCount=0) and (Background.Calls>0),'translation.changed-metrics');Layout.EndFrame;
  Prime(20,200);
  After:=Layout.Place('label','alpha',Point(28,203),Point(28,203),50,15,lkNamedLine);
  Require((Layout.ReusedPlacementCount=0) and (Background.Calls>0),'translation.edge-solver');Layout.EndFrame;
  Prime(200,200);Layout.TranslationOnly:=False;
  After:=Layout.Place('label','alpha',Point(208,203),Point(208,203),50,15,lkNamedLine);
  Require((Layout.ReusedPlacementCount=0) and (Background.Calls>0),'translation.default-solver');Layout.EndFrame;
 finally Layout.Free;Background.Free end;
end;
procedure SaveExternalCanvas(B:TBitmap;const Name:string);
var Capture:TBitmap;
begin
 GdiFlush;Capture:=TBitmap.Create;
 try
  Capture.SetSize(B.Width,B.Height);
  Capture.Canvas.CopyRect(Rect(0,0,B.Width,B.Height),B.Canvas,Rect(0,0,B.Width,B.Height));
  Capture.SaveToFile(ParamStr(2)+'/'+Name+'.bmp');
 finally Capture.Free end;
end;
procedure CheckEraseBackground(V:TFreeViewport;Expected:TColor;const Key:string);
var B:TBitmap;R:PtrInt;OK:Boolean;
begin
 B:=TBitmap.Create;
 try
  B.SetSize(V.ClientWidth,V.ClientHeight);B.Canvas.Brush.Color:=clFuchsia;
  B.Canvas.FillRect(0,0,B.Width,B.Height);
  R:=V.Perform(LM_ERASEBKGND,WPARAM(B.Canvas.Handle),0);
  OK:=(R<>0) and (ColorToRGB(B.Canvas.Pixels[0,0])=ColorToRGB(Expected)) and
      (ColorToRGB(B.Canvas.Pixels[B.Width-1,0])=ColorToRGB(Expected)) and
      (ColorToRGB(B.Canvas.Pixels[0,B.Height-1])=ColorToRGB(Expected)) and
      (ColorToRGB(B.Canvas.Pixels[B.Width-1,B.Height-1])=ColorToRGB(Expected));
  WriteLn('STARTUP ',Key,' ',Ord(OK));
 finally B.Free end;
end;

procedure CheckPresentationSequence(V:TLinesplanViewport);
var C:TPresentationCanvas;B:TBitmap;
begin
 B:=TBitmap.Create;C:=TPresentationCanvas.Create;
 try
  B.SetSize(V.ClientWidth,V.ClientHeight);C.Handle:=B.Canvas.Handle;
  V.PrepareFrame;
  V.PresentPreparedFrame(C);
  WriteLn('STARTUP cached_present_without_white_flash ',Ord((C.Clears=0) and (C.Blits=1)));
  WriteLn('STARTUP cached_present_without_text_scaling ',Ord(C.ScaledBlits=0));
 finally C.Handle:=0;C.Free;B.Free end;
end;

procedure CheckNativeCommitBarrier(V:TLinesplanViewport);
var C:TPresentationCanvas;W,H,BW,BH:Integer;R:TRect;Ready,WasDeferred:Boolean;ReadyHost:TForm;ReadyFrame:TFrame;ReadyPlot:TLinesplanViewport;
begin
  // LCL's autosize lock reproduces the real recorded intermediate state:
  // logical bounds change before the native HWND is resized. No visible input.
  ReadyHost:=TForm.Create(nil);
  try
    ReadyHost.Position:=poDesigned;ReadyHost.SetBounds(0,0,800,600);ReadyHost.HandleNeeded;
    WriteLn('STARTUP native_committed_layout_not_blocked ',Ord(LinesplanNativeLayoutReady(ReadyHost,ReadyHost.Canvas.Handle)));
    ReadyFrame:=TFrame.Create(ReadyHost);ReadyFrame.Parent:=ReadyHost;
    ReadyFrame.SetBounds(0,0,ReadyHost.ClientWidth,ReadyHost.ClientHeight);ReadyFrame.HandleNeeded;
    ReadyPlot:=TLinesplanViewport.Create(ReadyFrame);ReadyPlot.Parent:=ReadyFrame;
    ReadyPlot.SetBounds(0,0,ReadyFrame.ClientWidth,ReadyFrame.ClientHeight);ReadyPlot.HandleNeeded;
    ReadyPlot.InvalidateClientRectCache(False);R:=ReadyPlot.ClientRect;
    ReadyFrame.InvalidateClientRectCache(False);R:=ReadyFrame.ClientRect;
    WriteLn('STARTUP native_committed_control_tree_not_blocked ',Ord(LinesplanNativeLayoutReady(ReadyPlot,ReadyPlot.Canvas.Handle)));
  finally ReadyHost.Free end;
  C:=TPresentationCanvas.Create;W:=V.Width;H:=V.Height;
  V.HandleNeeded;C.Handle:=V.Canvas.Handle;
  BW:=V.GetDrawingBuffer.Width;BH:=V.GetDrawingBuffer.Height;
  V.DisableAutoSizing;
  try
    V.SetBounds(V.Left,V.Top,W+283,H+167);
    Windows.GetWindowRect(V.Handle,R);
    Ready:=((R.Right-R.Left)<>V.Width) or ((R.Bottom-R.Top)<>V.Height);
    WriteLn('STARTUP native_pending_size_reproduced ',Ord(Ready));
    LineplanTextMetrics.Reset;V.PresentPreparedFrame(C);
    // Future pixels may be prepared in memory; the user's old WINDOW image
    // must remain untouched. Do not confuse bitmap work with native output.
    WasDeferred:=(C.Blits=0) and (C.Clears=0);
    WriteLn('PENDINGBUFFER ',BW,' ',BH,' ',V.GetDrawingBuffer.Width,' ',V.GetDrawingBuffer.Height,' glyphs ',LineplanTextMetrics.Calls);
    WriteLn('STARTUP native_pending_size_no_window_output ',Ord(WasDeferred));
    WriteLn('STARTUP native_pending_size_no_render_work ',Ord((LineplanTextMetrics.Calls=0) and (V.GetDrawingBuffer.Width=BW) and (V.GetDrawingBuffer.Height=BH)));
    LineplanTextMetrics.Reset;C.Blits:=0;C.Clears:=0;
    V.PresentPreparedFrame(C);
    WriteLn('STARTUP native_pending_repeat_no_window_output ',Ord((C.Blits=0) and (C.Clears=0) and (LineplanTextMetrics.Calls=0)));
  finally V.SetBounds(V.Left,V.Top,W,H);V.EnableAutoSizing;C.Handle:=0;C.Free end;
end;

procedure CheckNativePreparation(V:TLinesplanViewport);
var SavedBounds,BeforeRect,AfterRect:TRect;BeforeOrigin,AfterOrigin:TPoint;
 SavedW,SavedH:Integer;Image:TBitmap;C:TPresentationCanvas;
begin
 SavedBounds:=V.BoundsRect;SavedW:=V.GetDrawingBuffer.Width;SavedH:=V.GetDrawingBuffer.Height;
 BeforeOrigin:=V.Project(ZERO);Windows.GetWindowRect(V.Handle,BeforeRect);
 V.PrepareNativeResize(V.Width+283,V.Height+167);
 Windows.GetWindowRect(V.Handle,AfterRect);AfterOrigin:=V.Project(ZERO);
 WriteLn('STARTUP native_preparation_keeps_visible_geometry ',Ord(
   EqualRect(BeforeRect,AfterRect) and EqualRect(SavedBounds,V.BoundsRect) and
   (BeforeOrigin.X=AfterOrigin.X) and (BeforeOrigin.Y=AfterOrigin.Y) and
   (V.GetDrawingBuffer.Width=SavedW) and (V.GetDrawingBuffer.Height=SavedH)));
 V.SetBounds(V.Left,V.Top,V.Width+283,V.Height+167);
 Image:=TBitmap.Create;C:=TPresentationCanvas.Create;
 try
  Image.SetSize(V.ClientWidth,V.ClientHeight);C.Handle:=Image.Canvas.Handle;
  LineplanTextMetrics.Reset;V.PresentPreparedFrame(C);
  WriteLn('STARTUP native_prepared_first_present_has_no_render_work ',Ord(
    (LineplanTextMetrics.Calls=0) and (C.Blits=1) and (C.Clears=0) and
    (V.GetDrawingBuffer.Width=V.ClientWidth) and (V.GetDrawingBuffer.Height=V.ClientHeight)));
 finally C.Handle:=0;C.Free;Image.Free;
  V.SetBounds(SavedBounds.Left,SavedBounds.Top,SavedBounds.Right-SavedBounds.Left,SavedBounds.Bottom-SavedBounds.Top);
  V.PrepareFrame;
 end;
end;

procedure CheckResizeWorkload(V:TLinesplanViewport);
var Counter:TResizeDrawCounter;I,W,H,NewW,NewH:Integer;
 C,Sink:TPresentationCanvas;B,Capture,SinkImage:TBitmap;SavedCanvas:TCanvas;Started:QWord;FirstScale:Double;FirstOrigin,FreshOrigin:TPoint;ProjectionCurrent:Boolean;
begin
 Counter:=TResizeDrawCounter.Create;Counter.Original:=V.OnRedraw;
 B:=TBitmap.Create;C:=TPresentationCanvas.Create;
 Sink:=TPresentationCanvas.Create;SinkImage:=TBitmap.Create;SinkImage.SetSize(1,1);
 Sink.Handle:=SinkImage.Canvas.Handle;SavedCanvas:=V.DrawingCanvas;V.DrawingCanvas:=Sink;
 W:=V.Width;H:=V.Height;V.Align:=alNone;V.OnRedraw:=@Counter.Draw;
 try
  V.PrepareFrame;
  // A maximize jump, restore, another maximize, then consecutive drag sizes.
  // Observe immediately, without PrepareFrame or waiting for an idle timer.
  for I:=1 to 15 do begin
   case I of
    1,3:begin NewW:=1920;NewH:=1040 end;
    2:begin NewW:=W;NewH:=H end;
   else begin NewW:=W+I*13;NewH:=H+I*7 end end;
   LineplanTextMetrics.Reset;
   QueryPerformanceCounter(QStart);
   V.SetBounds(0,0,NewW,NewH);
   B.SetSize(V.ClientWidth,V.ClientHeight);C.Handle:=B.Canvas.Handle;
   C.Clears:=0;C.Blits:=0;C.ScaledBlits:=0;Sink.Blits:=0;Sink.Clears:=0;
   LineplanTextMetrics.CaptureFill:=I=1;
   V.PresentPreparedFrame(C); // Same presentation route as LM_ERASEBKGND.
   LineplanTextMetrics.CaptureFill:=False;
   WriteLn('RESIZEOUTPUT ',I,' ',Sink.Blits,' ',Sink.Clears,' ',C.Blits,' ',C.Clears);
   QueryPerformanceCounter(QEnd);
   FirstScale:=V.Scale;FirstOrigin:=V.Project(ZERO);
   TProbe(V).SyncProjection;FreshOrigin:=V.Project(ZERO);
   ProjectionCurrent:=(Abs(FirstScale-V.Scale)<1E-9) and
    (FirstOrigin.X=FreshOrigin.X) and (FirstOrigin.Y=FreshOrigin.Y);
   WriteLn('RESIZEPROJECTION ',I,' ',Ord(ProjectionCurrent));
   WriteLn('RESIZEFRAME ',I,' ',LineplanTextMetrics.Calls,' ',LineplanTextMetrics.MinSize,' ',LineplanTextMetrics.MaxSize,
    ' ',V.GetDrawingBuffer.Width,' ',V.GetDrawingBuffer.Height,' ',V.ClientWidth,' ',V.ClientHeight,' ',C.ScaledBlits,
    ' ',FormatFloat('0.000',(QEnd-QStart)*1000.0/QFrequency));
   if I<=2 then begin
    // The recording canvas borrows B's DC; copy through an owning canvas
    // before saving so TBitmap cannot reuse its unmodified raw-image cache.
    GdiFlush;Capture:=TBitmap.Create;
    try
     Capture.SetSize(B.Width,B.Height);
     Capture.Canvas.CopyRect(Rect(0,0,B.Width,B.Height),C,Rect(0,0,B.Width,B.Height));
     if I=1 then Capture.SaveToFile(ParamStr(2)+'/resize-maximize-first.bmp')
     else Capture.SaveToFile(ParamStr(2)+'/resize-restore-first.bmp');
    finally Capture.Free end;
   end;
   if I=1 then begin
    if LineplanTextMetrics.FillImage<>nil then
     LineplanTextMetrics.FillImage.SaveToFile(ParamStr(2)+'/reveal-resize-preview.bmp');
    LineplanTextMetrics.CaptureFill:=True;V.PrepareFrame;
    LineplanTextMetrics.CaptureFill:=False;
    if LineplanTextMetrics.FillImage<>nil then
     LineplanTextMetrics.FillImage.SaveToFile(ParamStr(2)+'/reveal-resize-fresh.bmp');
   end;
   C.Handle:=0;
  end;
  Counter.Calls:=0;LineplanTextMetrics.Reset;
  TProbe(V).Render;
  WriteLn('STARTUP resize_paint_native_text ',Ord((Counter.Calls=1) and (LineplanTextMetrics.Calls>0) and
   (V.GetDrawingBuffer.Width=V.ClientWidth) and (V.GetDrawingBuffer.Height=V.ClientHeight)));
  Started:=GetTickCount64;
  repeat Application.ProcessMessages;Sleep(5) until GetTickCount64-Started>=600;
  V.PrepareFrame;
  WriteLn('STARTUP resize_settled_native_buffer ',Ord((V.GetDrawingBuffer.Width=V.ClientWidth) and
   (V.GetDrawingBuffer.Height=V.ClientHeight)));
  V.SetBounds(0,0,W,H);V.PrepareFrame;
 finally V.DrawingCanvas:=SavedCanvas;Sink.Handle:=0;Sink.Free;SinkImage.Free;
 C.Handle:=0;C.Free;B.Free;V.OnRedraw:=Counter.Original;Counter.Free end;
end;

procedure PumpGap(const Kind:string);
var A,B:Int64;
begin
  if GapMS<=0 then Exit;
  QueryPerformanceCounter(A); Sleep(GapMS); Application.ProcessMessages;
  TProbe(Frame.Viewport).Render; GdiFlush; QueryPerformanceCounter(B);
  WriteLn('PUMP ',Kind,' ',FormatFloat('0.000',(B-A)*1000.0/QFrequency-GapMS));
end;
procedure Snapshot(const Name:string);
var Cached, Fresh:TMemoryStream;
  procedure Capture(Stream:TMemoryStream);
  var B:TBitmap;
  begin
    Frame.Viewport.LabelLayout.Clear;
    TProbe(Frame.Viewport).Render;
    B:=TBitmap.Create;
    try
      B.SetSize(Frame.Viewport.ClientWidth,Frame.Viewport.ClientHeight);
      B.Canvas.CopyRect(Rect(0,0,B.Width,B.Height),Frame.Viewport.GetDrawingBuffer.Canvas,Rect(0,0,B.Width,B.Height));
      B.SaveToStream(Stream);
    finally B.Free end;
  end;
begin
  Cached:=TMemoryStream.Create; Fresh:=TMemoryStream.Create;
  try
    Capture(Cached);
    Cached.SaveToFile(ParamStr(2)+'/'+Name+'.bmp');
    Frame.InvalidateRenderCache;
    Capture(Fresh);
    if (Cached.Size<>Fresh.Size) or not CompareMem(Cached.Memory,Fresh.Memory,Cached.Size) then
    begin
      Fresh.SaveToFile(ParamStr(2)+'/'+Name+'-fresh.bmp');
      raise Exception.Create('cached/fresh mismatch: '+Name);
    end;
    WriteLn('CHECK ',Name,' pass');
  finally Cached.Free; Fresh.Free end;
end;
procedure CheckFillReveal;
var SavedZoom:Double; SavedPan:TPoint; J:Integer;
 procedure Capture(const Name:string);
 begin
  LineplanTextMetrics.CaptureFill:=True;
  TProbe(Frame.Viewport).Render;
  LineplanTextMetrics.CaptureFill:=False;
  LineplanTextMetrics.FillImage.SaveToFile(ParamStr(2)+'/'+Name+'.bmp');
 end;
begin
 SavedZoom:=Frame.Viewport.Zoom; SavedPan:=Frame.Viewport.Pan;
 Frame.Viewport.Zoom:=5;Frame.Viewport.Pan:=Point(0,0);
 Frame.InvalidateRenderCache;TProbe(Frame.Viewport).Render;
 for J:=1 to 12 do begin TProbe(Frame.Viewport).WheelOut;TProbe(Frame.Viewport).Render end;
 Capture('reveal-zoom-preview');Frame.InvalidateRenderCache;Capture('reveal-zoom-fresh');
 Frame.Viewport.Zoom:=1;Frame.Viewport.Pan:=Point(TestWidth,0);
 Frame.InvalidateRenderCache;TProbe(Frame.Viewport).Render;
 TProbe(Frame.Viewport).PanDown;TProbe(Frame.Viewport).PanMove(100-TestWidth,100);
 Capture('reveal-pan-preview');TProbe(Frame.Viewport).PanUp;
 Frame.InvalidateRenderCache;Capture('reveal-pan-fresh');
 Frame.Viewport.Zoom:=SavedZoom;Frame.Viewport.Pan:=SavedPan;
 Frame.InvalidateRenderCache;TProbe(Frame.Viewport).Render;
end;
begin
 try
 GapMS:=StrToIntDef(ParamStr(3),0);
 TestWidth:=StrToIntDef(ParamStr(4),1200); TestHeight:=StrToIntDef(ParamStr(5),760);
 PanSteps:=StrToIntDef(ParamStr(6),12);
 QueryPerformanceFrequency(QFrequency);
 Application.Initialize; Host:=TForm.Create(nil); Host.SetBounds(0,0,1200,800);
 CheckTranslationRules;
 CheckVectorResources;
 CheckVectorGeometry;
 Ship:=TFreeShip.Create(Host); GlobalFreeShip:=Ship;
 Buffer:=TFreeFileBuffer.Create; Buffer.LoadFromFile(ParamStr(1));
 T:=GetTickCount64; Ship.LoadProject(Buffer); Buffer.Free; Ship.RebuildModel;
 WriteLn('LOAD ',GetTickCount64-T);
 Startup:=TFreeLinesplanForm.Create(Host);
 StartupHidden:=not Startup.Visible;
 Startup.HandleNeeded;
 WriteLn('STARTUP independent_native_owner ',Ord(GetWindow(Startup.Handle,GW_OWNER)=0));
 WriteLn('STARTUP independent_task_switch_window ',Ord(
  (Startup.ShowInTaskBar=stAlways) and
  ((GetWindowLongPtr(Startup.Handle,GWL_EXSTYLE) and WS_EX_APPWINDOW)<>0) and
  ((GetWindowLongPtr(Startup.Handle,GWL_EXSTYLE) and WS_EX_TOOLWINDOW)=0)));
 WriteLn('NATIVESTYLE ',GetWindowLongPtr(Startup.Handle,GWL_EXSTYLE),' ',GetClassLongPtr(Startup.Handle,GCL_STYLE));
 Startup.LinesplanFrame.FreeShip:=Ship;
 Startup.PrepareForDisplay;
 WriteLn('STARTUP hidden_before_ready ',Ord(StartupHidden));
 WriteLn('STARTUP hidden_after_prepare ',Ord(not Startup.Visible));
 WriteLn('STARTUP buffer_sized ',Ord((Startup.Viewport.GetDrawingBuffer.Width=Startup.Viewport.ClientWidth) and (Startup.Viewport.GetDrawingBuffer.Height=Startup.Viewport.ClientHeight)));
 WriteLn('STARTUP background_white ',Ord(ColorToRGB(Startup.Color)=ColorToRGB(clWhite)));
 // Late native layout/DPI size change after preparation: first presentation
 // must regenerate at native resolution, never stretch the smaller text buffer.
 Startup.SetBounds(0,0,1420,860);
 Startup.PrepareForDisplay;
 Startup.Viewport.GetDrawingBuffer.SetSize(710,400);
 Image:=TBitmap.Create;
 try
  Image.SetSize(Startup.Viewport.ClientWidth,Startup.Viewport.ClientHeight);
  TLinesplanViewport(Startup.Viewport).PresentPreparedFrame(Image.Canvas);
  WriteLn('STARTUP late_layout_native_resolution ',Ord(
   (Startup.Viewport.GetDrawingBuffer.Width=Startup.Viewport.ClientWidth) and
   (Startup.Viewport.GetDrawingBuffer.Height=Startup.Viewport.ClientHeight)));
 finally Image.Free end;
 TProbe(Startup.Viewport).Render;
 // Background-only fallback is tested without a drawing source, separately
 // from the model-backed resize route which must generate current-size text.
 Startup.Viewport.GetDrawingBuffer.SetSize(Startup.Viewport.ClientWidth,Startup.Viewport.ClientHeight);
 Startup.Viewport.GetDrawingBuffer.Canvas.Brush.Color:=clWhite;
 Startup.Viewport.GetDrawingBuffer.Canvas.FillRect(Startup.Viewport.ClientRect);
 CheckEraseBackground(Startup.Viewport,clWhite,'blank_client_covered');
 Startup.Viewport.GetDrawingBuffer.SetSize(Startup.Viewport.ClientWidth,Startup.Viewport.ClientHeight);
 Startup.Viewport.GetDrawingBuffer.Canvas.Brush.Color:=clYellow;
 Startup.Viewport.GetDrawingBuffer.Canvas.FillRect(Startup.Viewport.ClientRect);
 CheckEraseBackground(Startup.Viewport,clYellow,'cached_frame_covers_client');
 Startup.SetBounds(0,0,1400,900);Startup.PrepareForDisplay;
 WriteLn('STARTUP enlarged_buffer_sized ',Ord((Startup.Viewport.GetDrawingBuffer.Width=Startup.Viewport.ClientWidth) and (Startup.Viewport.GetDrawingBuffer.Height=Startup.Viewport.ClientHeight)));
 Startup.SetBounds(0,0,828,621);Startup.PrepareForDisplay;
 WriteLn('STARTUP restored_buffer_sized ',Ord((Startup.Viewport.GetDrawingBuffer.Width=Startup.Viewport.ClientWidth) and (Startup.Viewport.GetDrawingBuffer.Height=Startup.Viewport.ClientHeight)));
 TProbe(Startup.Viewport).Render; // First presentation ends startup-only policy.
 CheckPresentationSequence(TLinesplanViewport(Startup.Viewport));
 CheckNativeCommitBarrier(TLinesplanViewport(Startup.Viewport));
 CheckNativePreparation(TLinesplanViewport(Startup.Viewport));
 CheckResizeWorkload(TLinesplanViewport(Startup.Viewport));
 // Exercise parent/background output at growing sizes, not only the viewport.
 Startup.SetBounds(0,0,1920,1080);Startup.Realign;Startup.LinesplanFrame.Realign;
 Image:=TBitmap.Create;
 try
  Image.SetSize(Startup.ClientWidth,Startup.ClientHeight);
  TFormBackgroundProbe(Startup).RenderBackground(Image.Canvas.Handle);
  SaveExternalCanvas(Image,'growth-form-background');
  Image.Canvas.Brush.Color:=clFuchsia;Image.Canvas.FillRect(0,0,Image.Width,Image.Height);
  TFrameBackgroundProbe(Startup.LinesplanFrame).RenderBackground(Image.Canvas.Handle);
  SaveExternalCanvas(Image,'growth-frame-background');
 finally Image.Free end;
 Startup.Free;

 T:=GetTickCount64; Frame:=TFreeLinesplanFrame.Create(Host); Frame.Parent:=Host;
 Frame.SetBounds(0,0,TestWidth,TestHeight+40); Frame.FontSize:=7; Frame.FreeShip:=Ship;
 Frame.Viewport.SetBounds(0,0,TestWidth,TestHeight); Frame.Viewport.ZoomExtents;
 Frame.Viewport.Zoom:=StrToFloatDef(ParamStr(7),1.0);
 // Warm native handles before measuring interaction, as in an already-open window.
 Frame.HandleNeeded;Frame.Viewport.HandleNeeded;Frame.Realign;
 if Frame.Viewport is TLinesplanViewport then TLinesplanViewport(Frame.Viewport).PrepareFrame;
 WriteLn('CREATE_EXTENTS ',GetTickCount64-T);
 for I:=0 to 4 do begin
  T:=GetTickCount64; TProbe(Frame.Viewport).Render;
  WriteLn('RENDER ',I,' ',GetTickCount64-T);
 end;
 LineplanTextMetrics.Reset; TProbe(Frame.Viewport).Render; LineplanTextMetrics.SavePlacements;
 StartPan:=Frame.Viewport.Pan;
 TProbe(Frame.Viewport).PanDown;
 for I:=1 to PanSteps do begin
   LineplanTextMetrics.Reset;
   QueryPerformanceCounter(QStart);
   PreviousPan:=Frame.Viewport.Pan;
   TProbe(Frame.Viewport).PanMove(100+I*8,100+I*3);
   TProbe(Frame.Viewport).Render;
   GdiFlush; QueryPerformanceCounter(QEnd);
   WriteLn('GLYPHS pan ',I,' ',LineplanTextMetrics.Calls,' ',LineplanTextMetrics.MinSize,' ',LineplanTextMetrics.MaxSize,' ',LineplanTextMetrics.BusyCalls);
   WriteLn('INTERACT pan ',I,' ',FormatFloat('0.000',(QEnd-QStart)*1000.0/QFrequency));
   WriteLn('ANIMATING pan ',I,' ',Ord(Frame.Viewport.LabelLayout.Animating));
   WriteLn('LABELREUSE pan ',I,' ',Frame.Viewport.LabelLayout.ReusedPlacementCount);
   LineplanTextMetrics.ObservePan(Frame.Viewport.Pan.X-PreviousPan.X,
     Frame.Viewport.Pan.Y-PreviousPan.Y,Frame.Viewport.ClientWidth,Frame.Viewport.ClientHeight,Checked,Errors);
   WriteLn('PANLABEL ',I,' ',Checked,' ',Errors);
   PumpGap('pan');
 end;
 for I:=1 to 8 do begin
   LineplanTextMetrics.Reset; PreviousPan:=Frame.Viewport.Pan;
   TProbe(Frame.Viewport).PanMove(100+PanSteps*8+I,100+PanSteps*3); TProbe(Frame.Viewport).Render;
   WriteLn('MICROPAN ',I,' ',Frame.Viewport.Pan.X-PreviousPan.X);
 end;
 Frame.FontSize:=8;TProbe(Frame.Viewport).Render;
 if Frame.Viewport.LabelLayout.ReusedPlacementCount<>0 then raise Exception.Create('font change reused layout');
 WriteLn('BEHAVIOR translation.font-change-while-held pass');
 Frame.FontSize:=7;TProbe(Frame.Viewport).Render;
 GuardZoom:=Frame.Viewport.Zoom;Frame.Viewport.Zoom:=GuardZoom*1.01;TProbe(Frame.Viewport).Render;
 if Frame.Viewport.LabelLayout.ReusedPlacementCount<>0 then raise Exception.Create('zoom change reused layout');
 WriteLn('BEHAVIOR translation.zoom-change-while-held pass');
 Frame.Viewport.Zoom:=GuardZoom;TProbe(Frame.Viewport).Render;
 Frame.UseLights.Checked:=not Frame.UseLights.Checked;TProbe(Frame.Viewport).Render;
 if Frame.Viewport.LabelLayout.ReusedPlacementCount<>0 then raise Exception.Create('style change reused layout');
 WriteLn('BEHAVIOR translation.style-change-while-held pass');
 Frame.UseLights.Checked:=not Frame.UseLights.Checked;TProbe(Frame.Viewport).Render;
 TProbe(Frame.Viewport).PanUp;
 // Compare rendered output at the same camera pose in old and new sources.
 Frame.Viewport.Pan:=Point(StartPan.X+PanSteps*8,StartPan.Y+PanSteps*3);
 Sleep(600); Application.ProcessMessages;
 TProbe(Frame.Viewport).Render;
 Snapshot('pan-final');
 for I:=1 to 12 do begin
   LineplanTextMetrics.Reset;
   QueryPerformanceCounter(QStart);
   TProbe(Frame.Viewport).Wheel;
   TProbe(Frame.Viewport).Render;
   GdiFlush; QueryPerformanceCounter(QEnd);
   WriteLn('GLYPHS wheel ',I,' ',LineplanTextMetrics.Calls,' ',LineplanTextMetrics.MinSize,' ',LineplanTextMetrics.MaxSize,' ',LineplanTextMetrics.BusyCalls);
   WriteLn('INTERACT wheel ',I,' ',FormatFloat('0.000',(QEnd-QStart)*1000.0/QFrequency));
   WriteLn('ANIMATING wheel ',I,' ',Ord(Frame.Viewport.LabelLayout.Animating));
   WriteLn('LABELREUSE wheel ',I,' ',Frame.Viewport.LabelLayout.ReusedPlacementCount);
   PumpGap('wheel');
 end;
 Frame.Viewport.LabelLayout.Clear;
 Frame.Viewport.SaveAsBitmap(ParamStr(2)+'/during-wheel.bmp',False);
 if Frame.Viewport.ExportingBitmap then raise Exception.Create('bitmap export state not restored');
 WriteLn('CHECK export-state-restored pass');
 Sleep(600); Application.ProcessMessages;
 Snapshot('wheel-idle-final');
 Frame.ViewportMouseUp(nil,mbLeft,[],0,0);
 TProbe(Frame.Viewport).Render;
 Snapshot('wheel-final');
 Frame.Viewport.LabelLayout.Clear; Sleep(300);
 TProbe(Frame.Viewport).Render;
 Image:=TBitmap.Create; Image.SetSize(1200,760);
 Image.Canvas.CopyRect(Rect(0,0,1200,760),Frame.Viewport.GetDrawingBuffer.Canvas,Rect(0,0,1200,760));
 Image.SaveToFile(ParamStr(2)+'/screen.bmp'); Image.Free;
 Snapshot('unchanged');
 Frame.Viewport.Zoom:=1.25; Snapshot('zoom');
 Frame.Viewport.Pan:=Point(35,-17); Snapshot('pan');
 Frame.UseLights.Checked:=False; Snapshot('lights-off');
 Ship.Layer[0].Color:=clLime; Snapshot('layer-color');
 Frame.MirrorPlanView.Checked:=not Frame.MirrorPlanView.Checked; Snapshot('mirror');
 Ship.ProjectSettings.ProjectDraft:=Ship.ProjectSettings.ProjectDraft*0.9; Snapshot('draft');
 Ship.Layer[0].ShowInLinesplan:=False; Snapshot('layer-hidden');
 Ship.Layer[0].ShowInLinesplan:=True;
 Ship.Surface.ControlPoint[0].Locked:=False;
 Coord:=Ship.Surface.ControlPoint[0].Coordinate; Coord.Z:=Coord.Z+0.01;
 Ship.Surface.ControlPoint[0].Coordinate:=Coord; Ship.RebuildModel;
 Frame.Viewport.ZoomExtents; Snapshot('geometry-edit');
 Frame.Viewport.SetBounds(0,0,1000,700); Frame.Viewport.ZoomExtents;
 Sleep(600); Application.ProcessMessages; Snapshot('resize');
 Frame.FontSize:=10; Snapshot('font');
 Frame.ShowMonochrome.Checked:=True; Snapshot('monochrome');
 Frame.ShowMonochrome.Checked:=False; Frame.ShowFillColor.Checked:=False; Snapshot('lines-only');
 Frame.ShowFillColor.Checked:=True; Snapshot('fill-restored');
 CheckFillReveal;
 Frame.Viewport.LabelLayout.Clear; // Export a defined, fully revealed text state.
 Frame.Viewport.SaveAsBitmap(ParamStr(2)+'/bitmap.bmp',False);
 Frame.Viewport.Printing:=True; Snapshot('print-path'); Frame.Viewport.Printing:=False;
 Frame.ExportDXFExecute(nil);
 Frame.FreeShip:=nil; Host.Free;
 except on E:Exception do begin WriteLn('FAIL ',E.ClassName,': ',E.Message); DumpExceptionBackTrace(Output); Halt(1) end end;
end.


