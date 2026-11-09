unit GeometrySafetyProbe;
{$mode objfpc}{$H+}

interface
uses fpjson;
procedure RunGeometrySafetyProbe(C: TJSONArray);

implementation
uses Classes, SysUtils, FreeGeometry, FreeFileBuffer, FreeShipUnit, FreeTypes,
  FreeVersionUnit, FasterList;

type
  TObjectList = specialize TFasterList<TObject>;
  TRebuildFault = class
    Calls: Integer;
    FailAt: Integer;
    WasBuilding: Boolean;
    procedure Progress(Sender: TObject; Current, Total: Integer);
  end;

procedure Check(C: TJSONArray; const ID: string; OK: Boolean;
  const Expected, Actual: string);
var O: TJSONObject;
begin
  O := TJSONObject.Create;
  O.Add('id', ID);
  if OK then O.Add('status', 'pass') else O.Add('status', 'fail');
  O.Add('expected', Expected);
  O.Add('actual', Actual);
  C.Add(O);
end;

procedure TRebuildFault.Progress(Sender: TObject; Current, Total: Integer);
begin
  Inc(Calls);
  WasBuilding := WasBuilding or TFreeSubdivisionSurface(Sender).IsBuilding;
  if Calls = FailAt then raise Exception.Create('geometry probe rebuild callback fault');
end;

function NewShip: TFreeShip;
var I: Integer;
begin
  Result := TFreeShip.Create(nil);
  Result.IsRebuildAsyncDisabled := True;
  Result.FileVersion := fv462;
  Result.ProjectSettings.SavePreview := False;
  Result.Surface.AddControlPoint(SetPoint(0, 0, 0));
  Result.Surface.AddControlPoint(SetPoint(1, 0, 0));
  Result.Surface.AddControlPoint(SetPoint(0, 1, 0));
  for I := 0 to 2 do Result.Surface.ControlPoint[I].Name := 'ProbePoint' + IntToStr(I);
  Result.Surface.AddControlEdge(Result.Surface.ControlPoint[0], Result.Surface.ControlPoint[1]);
  Result.Surface.AddControlEdge(Result.Surface.ControlPoint[1], Result.Surface.ControlPoint[2]);
  Result.Surface.AddControlEdge(Result.Surface.ControlPoint[2], Result.Surface.ControlPoint[0]);
end;

procedure CheckIndex(C: TJSONArray; const Kind: string; Bad: Integer; TextMode: Boolean);
var
  Ship: TFreeShip;
  B: TFreeFileBuffer;
  F: TFreeSubdivisionControlFace;
  E: TFreeSubdivisionControlEdge;
  Curve: TFreeSubdivisionControlCurve;
  Group: TFreeSubdivisionControlPointGroup;
  Words: TStrings;
  NameData: TNameData;
  Anchor: TAnchorData;
  Constraint: TLinearConstraintData;
  ErrorText, ErrorClass, Suffix: string;
begin
  Ship := NewShip;
  if TextMode then B := TFreeTextBuffer.Create else B := TFreeFileBuffer.Create;
  F := nil; E := nil; Curve := nil; Group := nil;
  try
    B.Version := fv462;
    if Kind = 'face-point' then
    begin
      B.Add(3); B.Add(0); B.Add(1); B.Add(Bad); B.Add(0); B.Add(False);
      F := TFreeSubdivisionControlFace.Create(Ship.Surface);
      Ship.Surface.ControlFaces.Add(F);
    end
    else if Kind = 'face-count' then
    begin
      B.Add(Bad); B.Add(0); B.Add(1); B.Add(2); B.Add(0); B.Add(False);
      F := TFreeSubdivisionControlFace.Create(Ship.Surface);
      Ship.Surface.ControlFaces.Add(F);
    end
    else if Kind = 'face-layer' then
    begin
      B.Add(3); B.Add(0); B.Add(1); B.Add(2); B.Add(Bad); B.Add(False);
      F := TFreeSubdivisionControlFace.Create(Ship.Surface);
      Ship.Surface.ControlFaces.Add(F);
    end
    else if Kind = 'edge-start' then
    begin
      B.Add(Bad); B.Add(1); B.Add(False); B.Add(False);
      E := TFreeSubdivisionControlEdge.Create(Ship.Surface);
      Ship.Surface.ControlEdges.Add(E);
    end
    else if Kind = 'edge-end' then
    begin
      B.Add(0); B.Add(Bad); B.Add(False); B.Add(False);
      E := TFreeSubdivisionControlEdge.Create(Ship.Surface);
      Ship.Surface.ControlEdges.Add(E);
    end
    else if Kind = 'curve-point' then
    begin
      B.Add(3); B.Add(0); B.Add(1); B.Add(Bad); B.Add(False);
      Curve := TFreeSubdivisionControlCurve.Create(Ship.Surface);
      Ship.Surface.ControlCurves.Add(Curve);
    end
    else if Kind = 'curve-count' then
    begin
      B.Add(Bad); B.Add(0); B.Add(1); B.Add(False);
      Curve := TFreeSubdivisionControlCurve.Create(Ship.Surface);
      Ship.Surface.ControlCurves.Add(Curve);
    end
    else if Kind = 'group-point' then
    begin
      B.Add('group');
      Words := TStringList.Create;
      try Words.Add('0'); Words.Add(IntToStr(Bad)); B.Add(Words)
      finally Words.Free end;
      Group := TFreeSubdivisionControlPointGroup.Create(Ship.Surface);
      Ship.Surface.ControlPointGroups.Add(Group);
    end
    else if Pos('name-', Kind) = 1 then
    begin
      B.Add(1); NameData.N := Bad; NameData.Name := 'bad'; B.Add(NameData);
    end
    else if Kind = 'anchor' then
    begin
      B.Add(1); Anchor.N := 0; Anchor.AnchorPoint := Bad; Anchor.IsAnchorHard := True;
      B.Add(Anchor);
    end
    else if Kind = 'anchor-owner' then
    begin
      B.Add(1); Anchor.N := Bad; Anchor.AnchorPoint := 1; Anchor.IsAnchorHard := True;
      B.Add(Anchor);
    end
    else if Kind = 'constraint' then
    begin
      B.Add(1); Constraint.N := 0; Constraint.LinearConstraintPointA := 1;
      Constraint.LinearConstraintPointB := Bad; B.Add(Constraint);
    end
    else if Kind = 'constraint-owner' then
    begin
      B.Add(1); Constraint.N := Bad; Constraint.LinearConstraintPointA := 1;
      Constraint.LinearConstraintPointB := 2; B.Add(Constraint);
    end;
    B.Reset;
    ErrorText := ''; ErrorClass := '';
    try
      if F <> nil then F.LoadBinary(B)
      else if E <> nil then E.LoadBinary(B)
      else if Curve <> nil then Curve.LoadBinary(B)
      else if Group <> nil then Group.LoadBinary(B)
      else if Kind = 'name-point' then Ship.LoadControlPointNames(B)
      else if Kind = 'name-edge' then Ship.LoadControlEdgeNames(B)
      else if Kind = 'name-face' then Ship.LoadControlFaceNames(B)
      else if Kind = 'name-curve' then Ship.LoadControlCurveNames(B)
      else if Pos('anchor', Kind) = 1 then Ship.LoadControlPointAnchors(B)
      else if Pos('constraint', Kind) = 1 then Ship.LoadControlPointLinearConstraints(B);
    except
      on Ex: Exception do begin ErrorText := Ex.Message; ErrorClass := Ex.ClassName end;
    end;
    Suffix := '.binary'; if TextMode then Suffix := '.text';
    Check(C, '04.' + Kind + '.' + IntToStr(Bad) + Suffix,
      ErrorClass = 'EReadError', 'reject the malformed reference with a field-specific EReadError',
      ErrorClass + ': ' + ErrorText);
  finally
    B.Free;
    Ship.Free;
  end;
end;

procedure CheckLegacyLayer(C: TJSONArray);
var Ship: TFreeShip; F: TFreeSubdivisionControlFace; B: TFreeFileBuffer;
  ErrorText: string;
begin
  Ship := NewShip; B := TFreeFileBuffer.Create;
  try
    F := TFreeSubdivisionControlFace.Create(Ship.Surface);
    Ship.Surface.ControlFaces.Add(F);
    B.Add(3); B.Add(0); B.Add(1); B.Add(2); B.Add(-1); B.Add(False); B.Reset;
    ErrorText := '';
    try F.LoadBinary(B) except on E: Exception do ErrorText := E.ClassName + ': ' + E.Message end;
    Check(C, '04.unassigned-layer-sentinel', (ErrorText = '') and
      (F.Layer = Ship.Surface.Layer[0]) and (F.NumberOfPoints = 3),
      'writer-supported layer -1 selects the default layer without changing point indices', ErrorText);
  finally B.Free; Ship.Free end;
end;

procedure CheckList(C: TJSONArray);
const Indices: array[0..2] of Integer = (-1, 1, High(Integer));
var L: TObjectList; Item, ReadItem: TObject; Index: Integer; Err: string;
begin
  L := TObjectList.Create; Item := TObject.Create;
  try
    L.Add(Item);
    Check(C, '04.list.valid', L[0] = Item, 'valid access returns the stored object', '');
    for Index in Indices do
    begin
      Err := '';
      try ReadItem := L[Index] except on E: Exception do Err := E.ClassName end;
      Check(C, '04.list.' + IntToStr(Index), Err = 'EListError',
        'out-of-range access raises EListError', Err);
    end;
  finally Item.Free; L.Free end;
end;

procedure CheckGroupRoundTrip(C: TJSONArray; TextMode: Boolean);
var A, B: TFreeShip; G: TFreeSubdivisionControlPointGroup; Buffer: TFreeFileBuffer;
  ErrorText, Suffix: string; I: Integer; P, Anchor: TFreeSubdivisionControlPoint;
  Named, Unnamed, Linked: Boolean;
begin
  A := NewShip; B := TFreeShip.Create(nil); B.IsRebuildAsyncDisabled := True;
  if TextMode then Buffer := TFreeTextBuffer.Create else Buffer := TFreeFileBuffer.Create;
  try
    G := TFreeSubdivisionControlPointGroup.Create(A.Surface);
    G.AddControlPoint(A.Surface.ControlPoint[0]); G.AddControlPoint(A.Surface.ControlPoint[1]);
    A.Surface.ControlPointGroups.Add(G);
    G := TFreeSubdivisionControlPointGroup.Create(A.Surface); G.Name := 'Named group';
    G.AddControlPoint(A.Surface.ControlPoint[1]); G.AddControlPoint(A.Surface.ControlPoint[2]);
    A.Surface.ControlPointGroups.Add(G);
    A.Surface.ControlPoint[2].SetAnchorPoint(A.Surface.ControlPoint[0]);
    A.Surface.ControlPoint[2].IsAnchorHard := True;
    ErrorText := '';
    try A.SaveProject(Buffer); B.LoadProject(Buffer, False)
    except on E: Exception do ErrorText := E.ClassName + ': ' + E.Message end;
    Named := False; Unnamed := False; Linked := False;
    for I := 0 to B.Surface.NumberOfControlPointGroups - 1 do
    begin
      G := B.Surface.ControlPointGroup[I];
      if G.Name = '' then Unnamed := (G.ControlPoints.Count = 2) and
        (G.ControlPoints[0].Name = 'ProbePoint0') and (G.ControlPoints[1].Name = 'ProbePoint1');
      if G.Name = 'Named group' then Named := (G.ControlPoints.Count = 2) and
        (G.ControlPoints[0].Name = 'ProbePoint1') and (G.ControlPoints[1].Name = 'ProbePoint2');
    end;
    for I := 0 to B.Surface.NumberOfControlPoints - 1 do
    begin
      P := B.Surface.ControlPoint[I]; Anchor := P.AnchorPoint;
      if P.Name = 'ProbePoint2' then
        Linked := (Anchor <> nil) and (Anchor.Name = 'ProbePoint0') and P.IsAnchorHard;
    end;
    Suffix := '.binary'; if TextMode then Suffix := '.text';
    Check(C, '04.groups.roundtrip' + Suffix, (ErrorText = '') and Named and Unnamed and Linked and
      (B.Surface.NumberOfControlPointGroups = 2),
      'named and unnamed groups plus the following anchor retain their data across a full project round trip',
      ErrorText + Format('; groups=%d named=%s unnamed=%s linked=%s',
        [B.Surface.NumberOfControlPointGroups, SysUtils.BoolToStr(Named, True),
        SysUtils.BoolToStr(Unnamed, True), SysUtils.BoolToStr(Linked, True)]));
  finally Buffer.Free; B.Free; A.Free end;
end;

procedure CheckRebuild(C: TJSONArray);
var Ship: TFreeShip; Points: TFasterListTFreeSubdivisionControlPoint;
  Fault: TRebuildFault; ErrorText: string; I: Integer;
begin
  Ship := NewShip; Points := TFasterListTFreeSubdivisionControlPoint.Create;
  Fault := TRebuildFault.Create;
  try
    for I := 0 to 2 do Points.Add(Ship.Surface.ControlPoint[I]);
    Ship.Surface.AddControlFace(Points, True);
    Ship.Surface.DesiredSubdivisionLevel := 2;
    Ship.Surface.Rebuild;
    Fault.FailAt := 1;
    Ship.Surface.OnFaceRebuilt := @Fault.Progress;
    Ship.Surface.RebuildRequested := True;
    ErrorText := '';
    try Ship.Surface.Rebuild except on E: Exception do ErrorText := E.Message end;
    Check(C, '08.rebuild.failure', (Pos('geometry probe rebuild callback fault', ErrorText) > 0)
      and Fault.WasBuilding and (Fault.Calls = 1),
      'an actual subdivision callback fails after entering the busy state', ErrorText);
    Check(C, '08.rebuild.flags', not Ship.Surface.IsBuilding and not Ship.Surface.Built and
      not Ship.Surface.RebuildRequested, 'failed rebuild clears busy and queued flags and remains unbuilt',
      Format('building=%s built=%s queued=%s', [SysUtils.BoolToStr(Ship.Surface.IsBuilding, True),
        SysUtils.BoolToStr(Ship.Surface.Built, True), SysUtils.BoolToStr(Ship.Surface.RebuildRequested, True)]));
    Ship.Surface.OnFaceRebuilt := nil;
    ErrorText := '';
    try Ship.Surface.Rebuild except on E: Exception do ErrorText := E.ClassName + ': ' + E.Message end;
    Check(C, '08.rebuild.retry', (ErrorText = '') and Ship.Surface.Built and
      not Ship.Surface.IsBuilding and (Ship.Surface.NumberOfControlPoints = 3) and
      (Ship.Surface.NumberOfControlFaces = 1),
      'the same surface rebuilds successfully after the fault is removed', ErrorText);
  finally Ship.Surface.OnFaceRebuilt := nil; Fault.Free; Points.Free; Ship.Free end;
end;

procedure RunGeometrySafetyProbe(C: TJSONArray);

  procedure RunCases(const Kinds: array of string; const Values: array of Integer;
    TextMode: Boolean);
  var Kind: string; Bad: Integer;
  begin
    for Kind in Kinds do
      for Bad in Values do CheckIndex(C, Kind, Bad, TextMode);
  end;

begin
  CheckList(C);
  RunCases(['face-point', 'edge-start', 'edge-end', 'curve-point', 'group-point',
    'name-point', 'name-edge', 'name-face', 'name-curve', 'anchor-owner', 'constraint-owner'],
    [-1, 3, High(Integer)], False);
  RunCases(['anchor', 'constraint', 'face-layer'], [-2, 3, High(Integer)], False);
  RunCases(['face-count', 'curve-count'], [-1, High(Integer)], False);
  RunCases(['face-point', 'edge-start', 'edge-end', 'curve-point', 'group-point', 'name-point'],
    [High(Integer)], True);
  CheckLegacyLayer(C);
  CheckGroupRoundTrip(C, False);
  CheckGroupRoundTrip(C, True);
  CheckRebuild(C);
end;

end.
