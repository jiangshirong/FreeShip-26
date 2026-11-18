{ FreeShip_26 modeling commands. GPL-2.0-or-later.
  Geometry operations use the Plus core; validation and transactions belong here. }
function TDocument.ResolveEntity(Slots:TFPList;Id:Int64):Pointer;
begin
  if (Id<1) or (Id>Slots.Count) or (Slots[Id-1]=nil) then
    raise Exception.Create('Unknown or deleted entity ID');
  Result:=Slots[Id-1];
end;

function TDocument.ReadEntityIds(Request:TJSONObject;Slots:TFPList;const Key:string):TIds;
var Values:TJSONArray;I:Integer;D:Double;Seen:array of Boolean;
begin
  Values:=TJSONArray(Field(Request,Key,jtArray));
  if (Values.Count=0) or (Values.Count>100000) then raise Exception.Create('Invalid selection size');
  SetLength(Result,Values.Count);SetLength(Seen,Slots.Count);
  for I:=0 to Values.Count-1 do begin
    if Values[I].JSONType<>jtNumber then raise Exception.Create('Invalid entity ID');
    D:=Values[I].AsFloat;
    if IsNan(D) or IsInfinite(D) or (D<1) or (D>Slots.Count) or (Frac(D)<>0) then
      raise Exception.Create('Unknown or deleted entity ID');
    Result[I]:=Trunc(D);ResolveEntity(Slots,Result[I]);
    if Seen[Result[I]-1] then raise Exception.Create('Duplicate entity ID');
    Seen[Result[I]-1]:=True;
  end;
end;

procedure TDocument.RequireEditable(Point:TFreeSubdivisionPoint);
begin
  if not BatchAllowed[EntityId(Point)-1] then
    raise Exception.Create('Operation touches a locked or constrained point');
end;

procedure TDocument.RequireEditableFace(Face:TFreeSubdivisionFace);
var I:Integer;
begin for I:=0 to Face.NumberOfPoints-1 do RequireEditable(Face.Point[I]) end;

procedure TDocument.DeletePointWithCurves(Point:TFreeSubdivisionControlPoint);
var I:Integer;Edge:TFreeSubdivisionControlEdge;
begin
  // Split/trim curve definitions while both edge endpoints are still present.
  // Point.Delete unreferences the point first, too early for DeleteEdge to find
  // the matching pair; that leaves one-point or disconnected curve definitions.
  for I:=0 to Point.NumberOfEdges-1 do begin
    Edge:=TFreeSubdivisionControlEdge(Point.Edge[I]);
    if Edge.Curve<>nil then begin Edge.Curve.DeleteEdge(Edge);Edge.Curve:=nil end;
  end;
  Point.Delete;
end;

function TDocument.ModelingEdit(Request:TJSONObject):Boolean;
var Op:string;Ids:TIds;I,J,K,N,Index:Integer;P,Q,R:TFreeSubdivisionControlPoint;
    E,E2:TFreeSubdivisionControlEdge;F,F2:TFreeSubdivisionControlFace;
    Layer,OtherLayer:TFreeSubdivisionLayer;A,B,C,D:T3DCoordinate;Plane:T3DPlane;
    Flag,Deleted,Crease:Boolean;Name:string;Value:Integer;BeforePoints,BeforeEdges:Integer;
    Stack:TFPList;Dependencies:array of Boolean;ControlList,NewPoints:TFasterListTFreeSubdivisionControlPoint;
    ChangedPoints:array of TFreeSubdivisionControlPoint;
    ControlCurve:TFreeSubdivisionControlCurve;
  function PointAt(Index:Integer):TFreeSubdivisionControlPoint;
  begin Result:=TFreeSubdivisionControlPoint(ResolveEntity(Points,Ids[Index])) end;
  procedure MarkDependencies(Start:TFreeSubdivisionControlPoint);
  var Current:TFreeSubdivisionControlPoint;Index:Integer;
  begin
    Stack.Clear;Stack.Add(Start);
    while Stack.Count>0 do begin
      Current:=TFreeSubdivisionControlPoint(Stack[Stack.Count-1]);Stack.Delete(Stack.Count-1);
      if Current=nil then Continue;
      Index:=EntityId(Current)-1;if Dependencies[Index] then Continue;
      Dependencies[Index]:=True;
      if Current.AnchorPoint<>nil then Stack.Add(Current.AnchorPoint);
      if Current.LinearConstraintPointA<>nil then Stack.Add(Current.LinearConstraintPointA);
      if Current.LinearConstraintPointB<>nil then Stack.Add(Current.LinearConstraintPointB);
    end;
  end;
  procedure Movable(Point:TFreeSubdivisionControlPoint);
  begin
    if Point.Locked or Point.IsAnchorHard then raise Exception.Create('Point is locked or hard anchored');
  end;
  procedure ValidateLine;
  begin
    A:=P.Coordinate;B:=Q.Coordinate;
    if Sqr(Double(A.X)-B.X)+Sqr(Double(A.Y)-B.Y)+Sqr(Double(A.Z)-B.Z)<=1.0e-12 then
      raise Exception.Create('Line endpoints must be distinct');
  end;
  procedure ChangedCoordinate(Point:TFreeSubdivisionControlPoint;const Position:T3DCoordinate);
  var Old:T3DCoordinate;
  begin
    CheckCoordinate(Position.X);CheckCoordinate(Position.Y);CheckCoordinate(Position.Z);
    Old:=Point.Coordinate;
    if (Old.X<>Position.X) or (Old.Y<>Position.Y) or (Old.Z<>Position.Z) then begin
      Point.SetCoordinate(nil,Position,nil);Result:=True;
    end;
  end;
  function NegativeFace(Face:TFreeSubdivisionControlFace):Boolean;
  var V:Integer;
  begin
    Result:=True;
    for V:=0 to Face.NumberOfPoints-1 do if Face.Point[V].Coordinate.Y>1e-7 then Exit(False);
  end;
  procedure ValidateDiagonal(Face:TFreeSubdivisionControlFace;First,Last:TFreeSubdivisionControlPoint);
  var X,Y:array of Double;Normal:array[0..2] of Double;
      H,Next,Axis,AIndex,BIndex:Integer;U,V:T3DCoordinate;MX,MY,T1,T2,T3,T4:Double;Inside:Boolean;
    function Turn(A,B,C:Integer):Double;
    begin Result:=(X[B]-X[A])*(Y[C]-Y[A])-(Y[B]-Y[A])*(X[C]-X[A]) end;
  begin
    Normal[0]:=0;Normal[1]:=0;Normal[2]:=0;
    for H:=0 to Face.NumberOfPoints-1 do begin
      U:=Face.Point[H].Coordinate;V:=Face.Point[(H+1) mod Face.NumberOfPoints].Coordinate;
      Normal[0]:=Normal[0]+(Double(U.Y)-V.Y)*(Double(U.Z)+V.Z);
      Normal[1]:=Normal[1]+(Double(U.Z)-V.Z)*(Double(U.X)+V.X);
      Normal[2]:=Normal[2]+(Double(U.X)-V.X)*(Double(U.Y)+V.Y);
    end;
    Axis:=0;if Abs(Normal[1])>Abs(Normal[Axis]) then Axis:=1;if Abs(Normal[2])>Abs(Normal[Axis]) then Axis:=2;
    if Normal[Axis]=0 then raise Exception.Create('Cannot split a degenerate face');
    SetLength(X,Face.NumberOfPoints);SetLength(Y,Face.NumberOfPoints);
    for H:=0 to Face.NumberOfPoints-1 do begin
      U:=Face.Point[H].Coordinate;
      if Axis=0 then X[H]:=U.Y else X[H]:=U.X;
      if Axis=2 then Y[H]:=U.Y else Y[H]:=U.Z;
    end;
    AIndex:=Face.IndexOfPoint(First);BIndex:=Face.IndexOfPoint(Last);
    for H:=0 to Face.NumberOfPoints-1 do begin
      if (H<>AIndex) and (H<>BIndex) and (Turn(AIndex,BIndex,H)=0) and
        (X[H]>=Math.Min(X[AIndex],X[BIndex])) and (X[H]<=Math.Max(X[AIndex],X[BIndex])) and
        (Y[H]>=Math.Min(Y[AIndex],Y[BIndex])) and (Y[H]<=Math.Max(Y[AIndex],Y[BIndex])) then
        raise Exception.Create('Diagonal touches another face vertex');
      Next:=(H+1) mod Face.NumberOfPoints;
      if (H=AIndex) or (H=BIndex) or (Next=AIndex) or (Next=BIndex) then Continue;
      T1:=Turn(AIndex,BIndex,H);T2:=Turn(AIndex,BIndex,Next);T3:=Turn(H,Next,AIndex);T4:=Turn(H,Next,BIndex);
      if (((T1<0) and (T2>0)) or ((T1>0) and (T2<0))) and
         (((T3<0) and (T4>0)) or ((T3>0) and (T4<0))) then raise Exception.Create('Diagonal crosses a face boundary');
    end;
    MX:=(X[AIndex]+X[BIndex])/2;MY:=(Y[AIndex]+Y[BIndex])/2;Inside:=False;
    for H:=0 to Face.NumberOfPoints-1 do begin
      Next:=(H+1) mod Face.NumberOfPoints;
      if (Y[H]>MY)<>(Y[Next]>MY) then
        if MX<X[H]+(MY-Y[H])*(X[Next]-X[H])/(Y[Next]-Y[H]) then Inside:=not Inside;
    end;
    if not Inside then raise Exception.Create('Diagonal lies outside the face');
  end;
begin
  Op:=StringField(Request,'operation');Result:=False;
  if (Op='anchor') or (Op='align') or (Op='linear_constraint') or (Op='coincide') then begin
    Ids:=ReadEntityIds(Request,Points);
    if Length(Ids)<2 then raise Exception.Create('Select at least two ordered points');
    P:=PointAt(0);Q:=PointAt(High(Ids));
    if (Op='align') or (Op='linear_constraint') then begin
      if Length(Ids)<3 then raise Exception.Create('Select endpoints and at least one intermediate point');
      ValidateLine;N:=High(Ids)-1;
    end else N:=High(Ids);
    for I:=1 to N do begin
      R:=PointAt(I);Movable(R);
      if (Op='align') or (Op='coincide') then RequireEditable(R);
    end;
    Stack:=TFPList.Create;SetLength(Dependencies,Points.Count);
    try
      if (Op='anchor') or (Op='linear_constraint') then begin
        MarkDependencies(P);if Op='linear_constraint' then MarkDependencies(Q);
      end;
      for I:=1 to N do begin
        R:=PointAt(I);
        if (Op='anchor') or (Op='linear_constraint') then begin
          if Dependencies[Ids[I]-1] then
            raise Exception.Create('Constraint would create a dependency cycle');
          // Both constraints on one target would compete during propagation.
          if (Op='anchor') and (R.LinearConstraintPointA<>nil) then
            raise Exception.Create('Remove the linear constraint before anchoring');
          if (Op='linear_constraint') and (R.AnchorPoint<>nil) then
            raise Exception.Create('Remove the anchor before constraining to a line');
        end;
      end;
      for I:=1 to N do begin
        R:=PointAt(I);
        if Op='anchor' then begin
          Flag:=Field(Request,'hard',jtBoolean).AsBoolean;
          if (R.AnchorPoint<>P) or (R.IsAnchorHard<>Flag) then begin
            R.AnchorPoint:=P;R.IsAnchorHard:=Flag;Result:=True;
          end;
        end else if Op='linear_constraint' then begin
          if (R.LinearConstraintPointA<>P) or (R.LinearConstraintPointB<>Q) then begin
            R.SetLinearConstraint(P,Q);Result:=True;
          end;
          ChangedCoordinate(R,ProjectPointOnLine(R.Coordinate,A,B));
        end else if Op='align' then ChangedCoordinate(R,ProjectPointOnLine(R.Coordinate,A,B))
        else ChangedCoordinate(R,P.Coordinate);
      end;
      if Result then begin
        if Op='anchor' then RequireVersion(Ship,fv462)
        else if Op='linear_constraint' then RequireVersion(Ship,fv421);
      end;
    finally Stack.Free end;
  end else if Op='clear_constraints' then begin
    Ids:=ReadEntityIds(Request,Points);
    for I:=0 to High(Ids) do begin
      P:=PointAt(I);
      if P.Locked then raise Exception.Create('Unlock points before removing their constraints');
    end;
    for I:=0 to High(Ids) do begin
      P:=PointAt(I);
      if (P.AnchorPoint<>nil) or P.IsAnchorHard or (P.LinearConstraintPointA<>nil) or (P.LinearConstraintPointB<>nil) then begin
        P.AnchorPoint:=nil;P.IsAnchorHard:=False;P.SetLinearConstraint(nil,nil);Result:=True;
      end;
    end;
  end else if Op='point_properties' then begin
    Ids:=ReadEntityIds(Request,Points);
    if Length(Ids)<>1 then raise Exception.Create('Select one point to edit its properties');
    P:=PointAt(0);Name:=TextValue(Ship,Request,'name');
    if (Length(Name)>4096) or (Pos(#0,Name)>0) then raise Exception.Create('Invalid point name');
    Value:=BoundedInteger(Request,'vertex_type',0,3);
    if Value>3 then raise Exception.Create('Invalid vertex type');
    if P.VertexType<>TFreeVertexType(Value) then begin RequireEditable(P);P.VertexType:=TFreeVertexType(Value);Result:=True end;
    if P.Name<>Name then begin P.Name:=Name;RequireVersion(Ship,fv421);Result:=True end;
  end else if Op='unlock_all' then begin
    for I:=0 to Points.Count-1 do if Points[I]<>nil then begin
      P:=TFreeSubdivisionControlPoint(Points[I]);
      if P.Locked then begin P.Locked:=False;Result:=True end;
    end;
  end else if Op='split_edges' then begin
    Ids:=ReadEntityIds(Request,Edges);
    for I:=0 to High(Ids) do begin
      E:=TFreeSubdivisionControlEdge(ResolveEntity(Edges,Ids[I]));
      RequireEditable(E.StartPoint);RequireEditable(E.EndPoint);
    end;
    for I:=0 to High(Ids) do begin
      E:=TFreeSubdivisionControlEdge(ResolveEntity(Edges,Ids[I]));A:=E.StartPoint.Coordinate;B:=E.EndPoint.Coordinate;
      C.X:=(Double(A.X)+B.X)/2;C.Y:=(Double(A.Y)+B.Y)/2;C.Z:=(Double(A.Z)+B.Z)/2;
      if E.InsertControlPoint(C)=nil then raise Exception.Create('Edge is too short to split');
    end;
    Result:=True;
  end else if Op='collapse_points' then begin
    Ids:=ReadEntityIds(Request,Points);
    // Each collapse can change the next one's neighborhood. Check again before
    // applying; the surrounding service transaction rolls back the whole request.
    for I:=0 to High(Ids) do begin
      P:=PointAt(I);RequireEditable(P);
      if P.NumberOfEdges<>2 then raise Exception.Create('Point collapse requires exactly two incident edges');
      E:=TFreeSubdivisionControlEdge(P.Edge[0]);E2:=TFreeSubdivisionControlEdge(P.Edge[1]);
      RequireEditable(E.StartPoint);RequireEditable(E.EndPoint);RequireEditable(E2.StartPoint);RequireEditable(E2.EndPoint);
      Q:=TFreeSubdivisionControlPoint(E.StartPoint);if Q=P then Q:=TFreeSubdivisionControlPoint(E.EndPoint);
      R:=TFreeSubdivisionControlPoint(E2.StartPoint);if R=P then R:=TFreeSubdivisionControlPoint(E2.EndPoint);
      if (Q=R) or (Ship.Surface.EdgeExists(Q,R)<>nil) then raise Exception.Create('Collapse would create a duplicate edge');
      for J:=0 to P.NumberOfFaces-1 do begin
        RequireEditableFace(P.Face[J]);
        if P.Face[J].NumberOfPoints<4 then raise Exception.Create('Collapse would degenerate a face');
      end;
      if E.Curve<>E2.Curve then raise Exception.Create('Remove the separate control curves before collapsing their junction');
      ControlCurve:=E.Curve;
      if ControlCurve<>nil then begin
        // Endpoint setters otherwise replace the middle control point with a
        // duplicate endpoint, and DeleteEdge may destroy the curve mid-collapse.
        E.Curve:=nil;E2.Curve:=nil;ControlCurve.DeletePoint(P);
      end;
      Crease:=E.Crease or E2.Crease;P.Collapse;E.Crease:=Crease;
      if ControlCurve<>nil then begin E.Curve:=ControlCurve;ControlCurve.Built:=False end;
      ReindexTopology;
    end;
    Result:=True;
  end else if Op='collapse_edges' then begin
    Ids:=ReadEntityIds(Request,Edges);
    for I:=0 to High(Ids) do begin
      E:=TFreeSubdivisionControlEdge(ResolveEntity(Edges,Ids[I]));
      if (E.NumberOfFaces<>2) or (E.StartPoint.NumberOfEdges<=2) or (E.EndPoint.NumberOfEdges<=2) then
        raise Exception.Create('Edge merge requires two adjacent faces and nonterminal endpoints');
      F:=TFreeSubdivisionControlFace(E.Face[0]);F2:=TFreeSubdivisionControlFace(E.Face[1]);
      RequireEditableFace(F);RequireEditableFace(F2);
      if F.Layer<>F2.Layer then raise Exception.Create('Assign the faces to the same layer before merging');
      N:=0;for J:=0 to F.NumberOfPoints-1 do if F2.IndexOfPoint(F.Point[J])>=0 then Inc(N);
      if N<>2 then raise Exception.Create('Faces share more than the selected edge');
      BeforeEdges:=Ship.Surface.NumberOfControlEdges;E.Collapse;
      if Ship.Surface.NumberOfControlEdges=BeforeEdges then raise Exception.Create('Edge could not be merged');
      ReindexTopology;
    end;
    Result:=True;
  end else if Op='connect_points' then begin
    Ids:=ReadEntityIds(Request,Points);
    if Length(Ids)<2 then raise Exception.Create('Select at least two ordered points');
    for I:=0 to High(Ids) do RequireEditable(PointAt(I));
    Ship.Surface.ClearMesh;
    for I:=1 to High(Ids) do begin
      P:=PointAt(I-1);Q:=PointAt(I);
      if Ship.Surface.EdgeExists(P,Q)<>nil then Continue;
      A:=P.Coordinate;B:=Q.Coordinate;
      if (A.X=B.X) and (A.Y=B.Y) and (A.Z=B.Z) then raise Exception.Create('Cannot connect coincident points');
      if (P.NumberOfFaces=0) and (Q.NumberOfFaces=0) then begin
        E:=Ship.Surface.AddControlEdge(P,Q);E.Crease:=True;Result:=True;
      end else begin
        F:=nil;
        for J:=0 to P.NumberOfFaces-1 do if Q.IndexOfFace(P.Face[J])>=0 then begin
          if F<>nil then raise Exception.Create('More than one shared face; select an unambiguous pair');
          F:=TFreeSubdivisionControlFace(P.Face[J]);
        end;
        if F=nil then raise Exception.Create('Points must share a face or both be free points');
        RequireEditableFace(F);ValidateDiagonal(F,P,Q);E:=F.InsertEdge(P,Q,Deleted);
        if E=nil then raise Exception.Create('Could not split the shared face');
        Result:=True;
      end;
      ReindexTopology;
    end;
  end else if Op='extrude_points' then begin
    Ids:=ReadEntityIds(Request,Points);D:=ReadPosition(Request);
    if (D.X=0) and (D.Y=0) and (D.Z=0) then Exit;
    ControlList:=TFasterListTFreeSubdivisionControlPoint.Create;
    try
      for I:=0 to High(Ids) do begin
        P:=PointAt(I);RequireEditable(P);A:=P.Coordinate;
        CheckCoordinate(Double(A.X)+D.X);CheckCoordinate(Double(A.Y)+D.Y);CheckCoordinate(Double(A.Z)+D.Z);
        ControlList.Add(P);
      end;
      NewPoints:=Ship.Surface.ExtrudeControlPoints(ControlList,D);
      NewPoints.Free;Result:=True;
    finally ControlList.Free end;
  end else if Op='remove_unused' then begin
    SetLength(ChangedPoints,Ship.Surface.NumberOfControlPoints);N:=0;
    for I:=0 to Ship.Surface.NumberOfControlPoints-1 do begin
      P:=Ship.Surface.ControlPoint[I];
      if P.NumberOfFaces=0 then begin RequireEditable(P);ChangedPoints[N]:=P;Inc(N) end;
    end;
    for I:=0 to N-1 do if Ship.Surface.ControlPoints.IndexOf(ChangedPoints[I])>=0 then DeletePointWithCurves(ChangedPoints[I]);
    Result:=N>0;
  end else if Op='flip_faces' then begin
    Ids:=ReadEntityIds(Request,Faces);
    for I:=0 to High(Ids) do RequireEditableFace(TFreeSubdivisionControlFace(ResolveEntity(Faces,Ids[I])));
    for I:=0 to High(Ids) do TFreeSubdivisionControlFace(ResolveEntity(Faces,Ids[I])).FlipNormal;
    Result:=True;
  end else if Op='delete_negative' then begin
    for I:=0 to Ship.Surface.NumberOfControlFaces-1 do begin
      F:=Ship.Surface.ControlFace[I];if NegativeFace(F) then RequireEditableFace(F);
    end;
    for I:=Ship.Surface.NumberOfControlFaces-1 downto 0 do begin
      F:=Ship.Surface.ControlFace[I];if NegativeFace(F) then begin F.Delete;Result:=True end;
    end;
    for I:=Ship.Surface.NumberOfControlPoints-1 downto 0 do begin
      P:=Ship.Surface.ControlPoint[I];
      if (P.NumberOfFaces=0) and (P.Coordinate.Y < -1e-4) then begin RequireEditable(P);DeletePointWithCurves(P);Result:=True end;
    end;
  end else if Op='insert_plane' then begin
    Value:=BoundedInteger(Request,'axis',0,2);
    A:=ReadPosition(Request);Plane.a:=0;Plane.b:=0;Plane.c:=0;
    case Value of 0:begin Plane.a:=1;Plane.d:=-A.X end;1:begin Plane.b:=1;Plane.d:=-A.Y end;2:begin Plane.c:=1;Plane.d:=-A.Z end end;
    Flag:=Field(Request,'create_curve',jtBoolean).AsBoolean;
    for I:=0 to Ship.Surface.NumberOfControlEdges-1 do begin
      E:=Ship.Surface.ControlEdge[I];if not E.Visible then Continue;
      A:=E.StartPoint.Coordinate;B:=E.EndPoint.Coordinate;
      if (Plane.a*A.X+Plane.b*A.Y+Plane.c*A.Z+Plane.d)*(Plane.a*B.X+Plane.b*B.Y+Plane.c*B.Z+Plane.d)<0 then begin
        RequireEditable(E.StartPoint);RequireEditable(E.EndPoint);
        for J:=0 to E.NumberOfFaces-1 do RequireEditableFace(E.Face[J]);
      end;
    end;
    BeforePoints:=Ship.Surface.NumberOfControlPoints;
    Ship.Surface.ControlPoints.IsSorted:=False;Ship.Surface.ControlEdges.IsSorted:=False;
    Ship.Surface.InsertPlane(Plane,Flag);Result:=BeforePoints<>Ship.Surface.NumberOfControlPoints;
  end else if Op='intersect_layers' then begin
    Layer:=TFreeSubdivisionLayer(ResolveEntity(Layers,IntegerField(Request,'layer_id',1)));
    OtherLayer:=TFreeSubdivisionLayer(ResolveEntity(Layers,IntegerField(Request,'other_layer_id',1)));
    if (Layer=OtherLayer) or (Layer.Count=0) or (OtherLayer.Count=0) then raise Exception.Create('Select two distinct nonempty layers');
    for I:=0 to Layer.Count-1 do RequireEditableFace(Layer.Items[I]);
    BeforePoints:=Ship.Surface.NumberOfControlPoints;
    Result:=Layer.CalculateIntersectionPoints(OtherLayer);
    // The old helper reports only newly connecting edges, even when it has
    // inserted isolated intersection vertices. Those edits also need history.
    Result:=Result or (BeforePoints<>Ship.Surface.NumberOfControlPoints);
  end else Result:=ModelToolsEdit(Request);
  if Result then ReindexTopology;
end;
