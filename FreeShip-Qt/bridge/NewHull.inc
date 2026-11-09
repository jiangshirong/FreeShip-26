{ Adapted from Plus FreeEdit.inc::Model_New, GPL-2.0-or-later.
  Original template coordinates and spline sampling; no legacy dialog or undo. }
procedure TDocument.NewHull(Request:TJSONObject);
var L,B,D:TFloatType;I,J,Cols,Rows,Units:Integer;P:T3DCoordinate;
    Default:array[0..6,0..4] of T3DCoordinate;
    Spline1,Spline2:TFreeSpline;TrvSplines:TFasterListTFreeSpline;
    TrvPoints:TFasterListTFreeSubdivisionControlPoint;
    Pts:array of array of TFreeSubdivisionControlPoint;StemPoint:TFreeSubdivisionControlPoint;
begin
  // There is no source thumbnail for a newly constructed hull. The headless
  // service cannot ask an old viewport to fabricate one during SaveProject.
  Ship.ProjectSettings.SavePreview:=False;
  L:=Field(Request,'length',jtNumber).AsFloat;B:=Field(Request,'beam',jtNumber).AsFloat;D:=Field(Request,'draft',jtNumber).AsFloat;
  CheckCoordinate(L);CheckCoordinate(B);CheckCoordinate(D);
  if (L<=0) or (B<=0) or (D<=0) then raise Exception.Create('Hull dimensions must be positive');
  Cols:=BoundedInteger(Request,'columns',2,100)-1;Rows:=BoundedInteger(Request,'rows',2,100)-1;
  if (Cols>99) or (Rows>99) then raise Exception.Create('Hull grid must have 2..100 points in each direction');
  Units:=BoundedInteger(Request,'units',0,1);if Units>1 then raise Exception.Create('Invalid units');
  Ship.ProjectSettings.ProjectUnits:=TFreeUnitType(Units);
  Ship.ProjectSettings.ProjectLength:=L;Ship.ProjectSettings.ProjectBeam:=B;Ship.ProjectSettings.ProjectDraft:=D;
    // station 0, stern
    Default[0, 0] := Point3D(0.00000, 0.00000, 1.56754);
    Default[0, 1] := Point3D(0.00000, 0.05280, 1.59170);
    Default[0, 2] := Point3D(0.00000, 0.22171, 1.77284);
    Default[0, 3] := Point3D(0.00000, 0.28506, 2.64108);
    Default[0, 4] := Point3D(0.00000, 0.29135, 3.48932);
    // station 1
    Default[1, 0] := Point3D(0.20880, 0.00000, 0.49656);
    Default[1, 1] := Point3D(0.20881, 0.18796, 0.53622);
    Default[1, 2] := Point3D(0.20880, 0.33700, 0.97840);
    Default[1, 3] := Point3D(0.20880, 0.45607, 2.05422);
    Default[1, 4] := Point3D(0.20882, 0.47184, 3.44280);
    // station 2
    Default[2, 0] := Point3D(0.41765, 0.00000, 0.00000);
    Default[2, 1] := Point3D(0.41765, 0.23565, 0.07524);
    Default[2, 2] := Point3D(0.41765, 0.41555, 0.67735);
    Default[2, 3] := Point3D(0.41765, 0.49421, 1.91004);
    Default[2, 4] := Point3D(0.41737, 0.51468, 3.45474);
    // station 3
    Default[3, 0] := Point3D(0.58471, 0.00000, 0.00000);
    Default[3, 1] := Point3D(0.58472, 0.24072, 0.02507);
    Default[3, 2] := Point3D(0.58472, 0.39528, 0.71080);
    Default[3, 3] := Point3D(0.58488, 0.45356, 2.04881);
    Default[3, 4] := Point3D(0.58472, 0.46756, 3.54662);
    // station 4
    Default[4, 0] := Point3D(0.75179, 0.00000, 0.28284);
    Default[4, 1] := Point3D(0.75178, 0.13715, 0.44098);
    Default[4, 2] := Point3D(0.75179, 0.20950, 0.87760);
    Default[4, 3] := Point3D(0.75179, 0.30538, 2.38232);
    Default[4, 4] := Point3D(0.75177, 0.34473, 3.67786);
    // station 5
    Default[5, 0] := Point3D(0.90672, 0.00000, 0.81860);
    Default[5, 1] := Point3D(0.90681, 0.01887, 0.98650);
    Default[5, 2] := Point3D(0.90658, 0.04671, 1.29873);
    Default[5, 3] := Point3D(0.90637, 0.11195, 2.83107);
    Default[5, 4] := Point3D(0.90672, 0.14523, 3.81697);
    // station 6 , stem
    Default[6, 0] := Point3D(0.91580, 0.00000, 0.85643);
    Default[6, 1] := Point3D(0.92562, 0.00000, 1.17444);
    Default[6, 2] := Point3D(0.93387, 0.00000, 1.44618);
    Default[6, 3] := Point3D(0.97668, 0.00000, 3.03482);
    Default[6, 4] := Point3D(1.00000, 0.00000, 3.91366);
    TrvSplines := TFasterListTFreeSpline.Create;
    Spline2:=nil;
    try
    StemPoint := nil;
    // First create tmp. splines in transverse direction
    for I := 0 to 6 do
    begin
      Spline1 := TFreeSpline.Create(Ship.Surface);
      TrvSplines.Add(Spline1);
      for J := 0 to 4 do
      begin
        P := Default[I, J];
        P.X := P.X * L;
        P.Y := P.Y * B;
        P.Z := P.Z * D;
        Spline1.Add(P);
      end;
    end;
    // now create tmp. splines in longitudinal direction
    Setlength(Pts, Rows + 1);
    for I := 0 to rows do
    begin
      Setlength(Pts[I], Cols + 1);
      Spline2 := TFreeSpline.Create(Ship.Surface);
      for j := 0 to TrvSplines.Count - 1 do
      begin
        Spline1 := TrvSplines[J];
        P := Spline1.Value(I / Rows);
        Spline2.Add(P);
      end;
      // now calculate all points on the longitudinal spline and send it to the surface
      for J := 0 to Cols do
      begin
        P := Spline2.Value(J / Cols);
        Pts[I, J] := TFreeSubdivisionControlPoint.Create(Ship.Surface);
        Ship.Surface.AddControlPoint(Pts[I, J]);
        Pts[I, J].Coordinate := P;
        if (I = 0) and (J = Cols) then
          StemPoint := Pts[I, J];
      end;
      FreeAndNil(Spline2);
    end;
    finally
    Spline2.Free;
    for I := 1 to TrvSplines.Count do
    begin
      Spline1 := TrvSplines[I - 1];
      FreeAndNil(Spline1);
    end;

    TrvSplines.Free;
    end;
    TrvPoints := TFasterListTFreeSubdivisionControlPoint.Create;
    try
    // finally create the controlfaces over the newly calculated points
    for I := 1 to Rows do
      for J := 1 to cols do
      begin
        TrvPoints.Clear;
        TrvPoints.Add(Pts[I, J - 1]);
        TrvPoints.Add(Pts[I, J]);
        TrvPoints.Add(Pts[I - 1, J]);
        TrvPoints.Add(Pts[I - 1, J - 1]);
        Ship.Surface.AddControlFace(TrvPoints, True);
      end;
    finally TrvPoints.Free end;
    // recheck edges crease after all faces added.
    for i:=0 to Ship.Surface.ControlEdges.Count - 1 do
       Ship.Surface.ControlEdge[i].Crease := false;

    Ship.Precision := fpMedium;
    Ship.Surface.Initialize(1, 1, 1);
    // Collapse stempoint to mage the grid irregular in order to demonstrate subdivision-surface capabilities
    if StemPoint <> nil then
      if StemPoint.VertexType = svCorner then
        stempoint.VertexType := svCrease;
    Ship.Built := False;

    // Add 21 stations
    for I := 0 to 20 do
      Ship.Edit.Intersection_Add(fiStation, I / 20 * (Ship.Surface.Max.X - Ship.Surface.Min.X));
    // Add 7 buttocks
    for I := 1 to 6 do
      Ship.Edit.Intersection_Add(fiButtock, I / 7 * (Ship.Surface.Max.Y - Ship.Surface.Min.Y));
    // Add 11 waterlines
    for I := 0 to 10 do
      Ship.Edit.Intersection_Add(fiWaterline, I / 10 * (Ship.Surface.Max.Z -
        Ship.Surface.Min.Z));

  Ship.FileChanged:=True;
end;
