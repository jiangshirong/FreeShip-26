{ FreeShip_26 control-net commands. GPL-2.0-or-later.
  Uses the original model operations, without old form/controller state. }
procedure TDocument.ReindexTopology;
var State:TModelState;I:Integer;
  function KeepOrAllocate(Item:Pointer;Slots:TFPList):Int64;
  var Index,Birth:Integer;
  begin
    Index:=EntityIds.IndexOf(PtrUInt(Item));
    if Index>=0 then begin
      Birth:=EntityBirths.IndexOf(PtrUInt(Item));
      if (Slots=Layers) or ((Birth>=0) and (EntityBirths.Data[Birth]=TFreeNamedObject(Item).Id)) then
        Exit(EntityIds.Data[Index]);
    end;
    if Slots.Count>=2000000 then raise Exception.Create('Document entity ID budget exhausted');
    Slots.Add(nil);Result:=Slots.Count;
  end;
begin
  State:=TModelState.Create;
  try
    SetLength(State.PointIds,Ship.Surface.NumberOfControlPoints);
    SetLength(State.EdgeIds,Ship.Surface.NumberOfControlEdges);
    SetLength(State.FaceIds,Ship.Surface.NumberOfControlFaces);
    SetLength(State.LayerIds,Ship.Surface.NumberOfLayers);
    for I:=0 to High(State.PointIds) do State.PointIds[I]:=KeepOrAllocate(Ship.Surface.ControlPoint[I],Points);
    for I:=0 to High(State.EdgeIds) do State.EdgeIds[I]:=KeepOrAllocate(Ship.Surface.ControlEdge[I],Edges);
    for I:=0 to High(State.FaceIds) do State.FaceIds[I]:=KeepOrAllocate(Ship.Surface.ControlFace[I],Faces);
    for I:=0 to High(State.LayerIds) do State.LayerIds[I]:=KeepOrAllocate(Ship.Surface.Layer[I],Layers);
    SetLength(State.CurveIds,Ship.Surface.NumberOfControlCurves);SetLength(State.FlowIds,Ship.NumberOfFlowLines);
    for I:=0 to High(State.CurveIds) do State.CurveIds[I]:=KeepOrAllocate(Ship.Surface.ControlCurve[I],Curves);
    for I:=0 to High(State.FlowIds) do State.FlowIds[I]:=KeepOrAllocate(Ship.Flowline[I],Flows);
    BindIds(State);
  finally State.Free end;
end;

function TDocument.TopologyEdit(Request:TJSONObject):Boolean;
var Operation,Kind:string;Ids:TIds;I,J,K:Integer;Id:Int64;Items:TFPList;
    P,Q:TFreeSubdivisionControlPoint;E:TFreeSubdivisionControlEdge;
    F:TFreeSubdivisionControlFace;L:TFreeSubdivisionLayer;List:TList;
    C,A,B:T3DCoordinate;Flag:Boolean;N:array[0..2] of Double;
    Axis:Integer;
  function Resolve(Slots:TFPList;Value:Int64):Pointer;
  begin
    if (Value<1) or (Value>Slots.Count) or (Slots[Value-1]=nil) then
      raise Exception.Create('Unknown or deleted entity ID');
    Result:=Slots[Value-1];
  end;
  function ReadIds(Slots:TFPList):TIds;
  var Values:TJSONArray;Index:Integer;Number:Double;Seen:array of Boolean;
  begin
    Values:=TJSONArray(Field(Request,'ids',jtArray));
    if (Values.Count=0) or (Values.Count>100000) then raise Exception.Create('Invalid selection size');
    SetLength(Result,Values.Count);SetLength(Seen,Slots.Count);
    for Index:=0 to Values.Count-1 do begin
      if Values[Index].JSONType<>jtNumber then raise Exception.Create('Invalid entity ID');
      Number:=Values[Index].AsFloat;
      if IsNan(Number) or IsInfinite(Number) or (Number<1) or (Number>Slots.Count) or (Frac(Number)<>0) then
        raise Exception.Create('Unknown or deleted entity ID');
      Result[Index]:=Trunc(Number);Resolve(Slots,Result[Index]);
      if Seen[Result[Index]-1] then raise Exception.Create('Duplicate entity ID');
      Seen[Result[Index]-1]:=True;
    end;
  end;
  procedure Editable(Point:TFreeSubdivisionPoint);
  begin
    if not BatchAllowed[EntityId(Point)-1] then
      raise Exception.Create('Topology touches a locked or constrained point');
  end;
  procedure EditableFace(Face:TFreeSubdivisionFace);
  var Index:Integer;
  begin for Index:=0 to Face.NumberOfPoints-1 do Editable(Face.Point[Index]) end;
  function U(const Value:T3DCoordinate):Double;
  begin if Axis=0 then Result:=Value.Y else Result:=Value.X end;
  function V(const Value:T3DCoordinate):Double;
  begin if Axis=2 then Result:=Value.Y else Result:=Value.Z end;
  function Turn(const P1,P2,P3:T3DCoordinate):Double;
  begin Result:=(U(P2)-U(P1))*(V(P3)-V(P1))-(V(P2)-V(P1))*(U(P3)-U(P1)) end;
  function Intersects(const P1,P2,P3,P4:T3DCoordinate):Boolean;
  var T1,T2,T3,T4:Double;
  begin
    Result:=False;
    if (Math.Max(U(P1),U(P2))<Math.Min(U(P3),U(P4))) or (Math.Max(U(P3),U(P4))<Math.Min(U(P1),U(P2))) or
       (Math.Max(V(P1),V(P2))<Math.Min(V(P3),V(P4))) or (Math.Max(V(P3),V(P4))<Math.Min(V(P1),V(P2))) then Exit;
    T1:=Turn(P1,P2,P3);T2:=Turn(P1,P2,P4);T3:=Turn(P3,P4,P1);T4:=Turn(P3,P4,P2);
    Result:=((T1<=0) and (T2>=0) or (T1>=0) and (T2<=0)) and
            ((T3<=0) and (T4>=0) or (T3>=0) and (T4<=0));
  end;
  function Position(Index:Integer):T3DCoordinate;
  begin Result:=TFreeSubdivisionControlPoint(Resolve(Points,Ids[Index])).Coordinate end;
begin
  Operation:=StringField(Request,'operation');Result:=False;
  if Operation='add_point' then begin
    C:=ReadPosition(Request);P:=Ship.Surface.AddControlPoint;
    P.Coordinate:=C;Result:=True;
  end else if Operation='split_edge' then begin
    E:=TFreeSubdivisionControlEdge(Resolve(Edges,IntegerField(Request,'edge_id',1)));
    Editable(E.StartPoint);Editable(E.EndPoint);
    A:=E.StartPoint.Coordinate;B:=E.EndPoint.Coordinate;
    C.X:=(Double(A.X)+B.X)/2;C.Y:=(Double(A.Y)+B.Y)/2;C.Z:=(Double(A.Z)+B.Z)/2;
    if E.InsertControlPoint(C)=nil then raise Exception.Create('Edge is too short to split');
    Result:=True;
  end else if Operation='set_crease' then begin
    Ids:=ReadIds(Edges);Flag:=Field(Request,'crease',jtBoolean).AsBoolean;
    for I:=0 to High(Ids) do begin
      E:=TFreeSubdivisionControlEdge(Resolve(Edges,Ids[I]));Editable(E.StartPoint);Editable(E.EndPoint);
      if not Flag and (E.NumberOfFaces<2) then raise Exception.Create('Boundary edges must remain creases');
    end;
    for I:=0 to High(Ids) do begin
      E:=TFreeSubdivisionControlEdge(Resolve(Edges,Ids[I]));
      if E.Crease<>Flag then begin E.Crease:=Flag;Result:=True end;
    end;
  end else if Operation='set_locked' then begin
    Ids:=ReadIds(Points);Flag:=Field(Request,'locked',jtBoolean).AsBoolean;
    for I:=0 to High(Ids) do begin
      P:=TFreeSubdivisionControlPoint(Resolve(Points,Ids[I]));
      if P.Locked<>Flag then begin P.Locked:=Flag;Result:=True end;
    end;
    // Earlier FBM formats do not serialize the locked flag, including history.
    if Result and Flag then RequireVersion(Ship,fv198);
  end else if Operation='add_face' then begin
    Ids:=ReadIds(Points);
    if (Length(Ids)<3) or (Length(Ids)>4096) then raise Exception.Create('A face requires 3..4096 ordered points');
    L:=TFreeSubdivisionLayer(Resolve(Layers,IntegerField(Request,'layer_id',1)));
    N[0]:=0;N[1]:=0;N[2]:=0;
    for I:=0 to High(Ids) do begin
      P:=TFreeSubdivisionControlPoint(Resolve(Points,Ids[I]));Editable(P);
      Q:=TFreeSubdivisionControlPoint(Resolve(Points,Ids[(I+1) mod Length(Ids)]));
      A:=P.Coordinate;B:=Q.Coordinate;
      if (A.X=B.X) and (A.Y=B.Y) and (A.Z=B.Z) then raise Exception.Create('Face has coincident adjacent points');
      N[0]:=N[0]+(Double(A.Y)-B.Y)*(Double(A.Z)+B.Z);
      N[1]:=N[1]+(Double(A.Z)-B.Z)*(Double(A.X)+B.X);
      N[2]:=N[2]+(Double(A.X)-B.X)*(Double(A.Y)+B.Y);
      E:=TFreeSubdivisionControlEdge(Ship.Surface.EdgeExists(P,Q));
      if (E<>nil) and (E.NumberOfFaces>=2) then raise Exception.Create('Face would create a nonmanifold edge');
    end;
    Axis:=0;if Abs(N[1])>Abs(N[Axis]) then Axis:=1;if Abs(N[2])>Abs(N[Axis]) then Axis:=2;
    if Abs(N[Axis])=0 then raise Exception.Create('Face is degenerate or has crossing edges');
    for I:=0 to High(Ids) do for J:=I+1 to High(Ids) do
      if (J<>I+1) and not ((I=0) and (J=High(Ids))) then
        if Intersects(Position(I),Position((I+1) mod Length(Ids)),Position(J),Position((J+1) mod Length(Ids))) then
          raise Exception.Create('Face has crossing edges');
    List:=TList.Create;
    try
      for I:=0 to High(Ids) do List.Add(Resolve(Points,Ids[I]));
      F:=Ship.Surface.AddControlFace(List,True);
      if F=nil then raise Exception.Create('Face already exists');
      F.Layer:=L;Result:=True;
    finally List.Free end;
  end else if Operation='assign_layer' then begin
    Ids:=ReadIds(Faces);L:=TFreeSubdivisionLayer(Resolve(Layers,IntegerField(Request,'layer_id',1)));
    for I:=0 to High(Ids) do EditableFace(TFreeSubdivisionControlFace(Resolve(Faces,Ids[I])));
    for I:=0 to High(Ids) do begin
      F:=TFreeSubdivisionControlFace(Resolve(Faces,Ids[I]));
      if F.Layer<>L then begin F.Layer:=L;Result:=True end;
    end;
  end else if Operation='delete' then begin
    Kind:=StringField(Request,'kind');
    if Kind='point' then Items:=Points else if Kind='edge' then Items:=Edges
    else if Kind='face' then Items:=Faces else raise Exception.Create('Invalid selection kind');
    Ids:=ReadIds(Items);
    for I:=0 to High(Ids) do begin
      if Kind='point' then begin
        P:=TFreeSubdivisionControlPoint(Resolve(Points,Ids[I]));Editable(P);
        for J:=0 to P.NumberOfFaces-1 do EditableFace(P.Face[J]);
        for J:=0 to P.NumberOfEdges-1 do begin Editable(P.Edge[J].StartPoint);Editable(P.Edge[J].EndPoint) end;
      end else if Kind='edge' then begin
        E:=TFreeSubdivisionControlEdge(Resolve(Edges,Ids[I]));Editable(E.StartPoint);Editable(E.EndPoint);
        for J:=0 to E.NumberOfFaces-1 do EditableFace(E.Face[J]);
      end else EditableFace(TFreeSubdivisionControlFace(Resolve(Faces,Ids[I])));
    end;
    // Earlier deletions can remove incident entities. Membership is checked
    // before dereferencing each saved pointer; no new entities are allocated.
    for I:=0 to High(Ids) do begin
      if Kind='point' then begin
        P:=TFreeSubdivisionControlPoint(Points[Ids[I]-1]);
        if Ship.Surface.ControlPoints.IndexOf(P)>=0 then DeletePointWithCurves(P);
      end else if Kind='edge' then begin
        E:=TFreeSubdivisionControlEdge(Edges[Ids[I]-1]);
        if Ship.Surface.ControlEdges.IndexOf(E)>=0 then E.Delete;
      end else begin
        F:=TFreeSubdivisionControlFace(Faces[Ids[I]-1]);
        if Ship.Surface.ControlFaces.IndexOf(F)>=0 then F.Delete;
      end;
    end;
    Result:=True;
  end else raise Exception.Create('Unsupported topology operation');
  if Result then ReindexTopology;
end;
