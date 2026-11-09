unit SurfaceLoadingProbe;
{$mode objfpc}{$H+}

interface

uses fpjson;

procedure RunSurfaceLoadingProbe(C: TJSONArray);

implementation

uses Classes, SysUtils, FreeGeometry, FreeFileBuffer, FreeTypes, FreeVersionUnit;

type
  TObservedBuffer = class(TFreeFileBuffer)
    Surface: TFreeSubdivisionSurface;
    Reads: Integer;
    AllReadsLoading: Boolean;
    procedure LoadInteger(var Output: Integer); override;
  end;

  TEventCounter = class
    Count: Integer;
    procedure Changed(Sender: TObject);
    procedure LayerChanged(Sender: TObject; Layer: TFreeSubdivisionLayer);
    procedure Progress(Sender: TObject; Current, Total: Integer);
  end;

procedure TObservedBuffer.LoadInteger(var Output: Integer);
begin
  Inc(Reads);
  AllReadsLoading := AllReadsLoading and Surface.IsLoading;
  inherited LoadInteger(Output);
end;

procedure TEventCounter.Changed(Sender: TObject);
begin
  Inc(Count);
end;

procedure TEventCounter.LayerChanged(Sender: TObject; Layer: TFreeSubdivisionLayer);
begin
  Inc(Count);
end;

procedure TEventCounter.Progress(Sender: TObject; Current, Total: Integer);
begin
  Inc(Count);
end;

procedure Add(C: TJSONArray; const ID: string; Passed: Boolean;
  const Expected, Actual: string);
var O: TJSONObject;
begin
  O := TJSONObject.Create;
  O.Add('id', ID);
  if Passed then O.Add('status', 'pass') else O.Add('status', 'fail');
  O.Add('expected', Expected);
  O.Add('actual', Actual);
  C.Add(O);
end;

procedure CheckLoad(C: TJSONArray; const ID: string; Mode: Integer;
  InitialLoading: Boolean);
var
  Surface: TFreeSubdivisionSurface;
  Buffer: TObservedBuffer;
  Counter: TEventCounter;
  ErrorText: string;
  ExpectedFailure: Boolean;
begin
  Surface := TFreeSubdivisionSurface.Create(nil);
  Buffer := TObservedBuffer.Create;
  Counter := TEventCounter.Create;
  try
    Surface.Clear; // Match TFreeShip.Create: initialise the surface's default layer.
    Buffer.Surface := Surface;
    Buffer.AllReadsLoading := True;
    Buffer.Version := fv198;
    Surface.AddOnSelectItemListener(@Counter.Changed);
    Surface.IsLoading := InitialLoading;
    case Mode of
      1: Buffer.Add(-1); // Negative layer count.
      2: begin Buffer.Add(0); Buffer.Add(1) end; // One default layer, index 1 is invalid.
      3: Buffer.Add(High(Integer)); // More layers than the buffer can contain.
      else
        begin
          Buffer.Add(0); // Keep the default layer.
          Buffer.Add(0); // Active layer.
          Buffer.Add(1); // One control point.
          Buffer.Add(ZERO);
          if Mode <> 4 then
          begin
            Buffer.Add(Ord(svRegular));
            Buffer.Add(True); // Selected, but callbacks must be suppressed while loading.
            Buffer.Add(False); // Not locked.
            Buffer.Add(0); // Edges.
            Buffer.Add(0); // Curves.
            Buffer.Add(0); // Faces.
          end;
        end;
    end;
    Buffer.Reset;
    ErrorText := '';
    try
      Surface.LoadBinary(Buffer);
    except
      on E: Exception do ErrorText := E.ClassName + ': ' + E.Message;
    end;
    ExpectedFailure := Mode <> 0;
    Add(C, ID + '.result', (ErrorText <> '') = ExpectedFailure,
      'malformed input raises; complete input loads', ErrorText);
    Add(C, ID + '.loading-during-read', Buffer.AllReadsLoading and (Buffer.Reads > 0),
      'the actual surface loading field is true during every integer read',
      Format('reads=%d all_loading=%s', [Buffer.Reads, SysUtils.BoolToStr(Buffer.AllReadsLoading, True)]));
    Add(C, ID + '.loading-restored', Surface.IsLoading = InitialLoading,
      SysUtils.BoolToStr(InitialLoading, True), SysUtils.BoolToStr(Surface.IsLoading, True));
    Add(C, ID + '.selection-events', Counter.Count = 0,
      'no partial-load selection notifications', IntToStr(Counter.Count));
    if Mode = 0 then
    begin
      Add(C, ID + '.geometry', (Surface.NumberOfControlPoints = 1) and
        (Surface.NumberOfSelectedControlPoints = 1),
        'one loaded point remains selected',
        Format('points=%d selected=%d', [Surface.NumberOfControlPoints,
          Surface.NumberOfSelectedControlPoints]));
      Surface.IsLoading := False;
      if Surface.NumberOfControlPoints > 0 then
      begin
        Surface.ControlPoint[0].Selected := False;
        Surface.ControlPoint[0].Selected := True;
      end;
      Add(C, ID + '.events-resume', Counter.Count > 0,
        'selection notifications resume after loading', IntToStr(Counter.Count));
    end;
  finally
    Surface.Free;
    Counter.Free;
    Buffer.Free;
  end;
end;

procedure Attach(Surface: TFreeSubdivisionSurface; Counter: TEventCounter);
begin
  Surface.OnChangeLayerData := @Counter.Changed;
  Surface.OnChangeActiveLayer := @Counter.LayerChanged;
  Surface.OnFaceRebuilt := @Counter.Progress;
  Surface.AddOnSelectItemListener(@Counter.Changed);
  Surface.AddOnChangeItemListener(@Counter.Changed);
  Surface.AddOnChangeActiveControlPointListener(@Counter.Changed);
  Surface.AddOnChangeActiveControlEdgeListener(@Counter.Changed);
  Surface.AddOnChangeActiveControlFaceListener(@Counter.Changed);
  Surface.AddOnChangeActiveControlCurveListener(@Counter.Changed);
end;

procedure FireAll(Surface: TFreeSubdivisionSurface);
begin
  Surface.OnChangeLayerData(Surface);
  Surface.OnChangeActiveLayer(Surface, Surface.ActiveLayer);
  Surface.OnFaceRebuilt(Surface, 1, 1);
  Surface.ExecuteOnSelectItem(nil);
  Surface.ExecuteOnChangeItem(nil);
  Surface.ExecuteOnChangeActiveControlPoint(nil);
  Surface.ExecuteOnChangeActiveControlEdge(nil);
  Surface.ExecuteOnChangeActiveControlFace(nil);
  Surface.ExecuteOnChangeActiveControlCurve(nil);
end;

procedure CheckExchange(C: TJSONArray);
var A, B: TFreeSubdivisionSurface; CA, CB: TEventCounter;
begin
  A := TFreeSubdivisionSurface.Create(nil);
  B := TFreeSubdivisionSurface.Create(nil);
  CA := TEventCounter.Create;
  CB := TEventCounter.Create;
  try
    A.Clear;
    B.Clear;
    Attach(A, CA);
    Attach(B, CB);
    A.ExchangeEventHandlers(B);
    FireAll(A);
    Add(C, '02.surface-events.swap-forward', (CA.Count = 0) and (CB.Count = 9),
      'only B subscriptions receive all nine A event dispatches',
      Format('A=%d B=%d', [CA.Count, CB.Count]));
    FireAll(B);
    Add(C, '02.surface-events.swap-backward', (CA.Count = 9) and (CB.Count = 9),
      'only A subscriptions receive all nine B event dispatches',
      Format('A=%d B=%d', [CA.Count, CB.Count]));
    A.ExchangeEventHandlers(B);
    FireAll(A);
    Add(C, '02.surface-events.reversible', (CA.Count = 18) and (CB.Count = 9),
      'exchanging again restores A subscriptions without duplicates',
      Format('A=%d B=%d', [CA.Count, CB.Count]));
  finally
    A.Free;
    B.Free;
    CA.Free;
    CB.Free;
  end;
end;

procedure RunSurfaceLoadingProbe(C: TJSONArray);
begin
  CheckLoad(C, '09.valid', 0, False);
  CheckLoad(C, '09.nested', 0, True);
  CheckLoad(C, '09.negative-count', 1, False);
  CheckLoad(C, '09.invalid-layer', 2, False);
  CheckLoad(C, '09.excessive-count', 3, False);
  CheckLoad(C, '09.truncated-point', 4, False);
  CheckLoad(C, '09.nested-failure', 4, True);
  CheckExchange(C);
end;

end.
