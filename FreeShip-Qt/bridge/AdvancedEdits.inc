{ Selected geometry transforms, boundary extrusion and plane reflection.
  Adapted from current Plus FreeEdit and FreeSubdivisionSurface, GPL-2.0-or-later. }
function TDocument.AdvancedEdit(Request:TJSONObject):Boolean;
var Operation,Kind:string;Slots:TFPList;Ids:TIds;Wanted,Seen:array of Boolean;
    SourcePoints,TargetPoints:array of TFreeSubdivisionControlPoint;
    Positions:array of T3DCoordinate;SourceFaces:array of TFreeSubdivisionControlFace;
    Adjacent:array of TFreeSubdivisionControlFace;Values:TJSONArray;
    I,J,K,N,Axis,Count:Integer;Wide:Int64;Number,Distance:Double;
    Vector,Position,A,B,C,D,Pivot:T3DCoordinate;Plane:T3DPlane;
    SX,CX,SY,CY,SZ,CZ:TFloatType;All,Markers,Weld,InPlane:Boolean;
    P,Q:TFreeSubdivisionControlPoint;Edge,Other:TFreeSubdivisionControlEdge;
    Face,NewFace:TFreeSubdivisionControlFace;Layer,OldLayer:TFreeSubdivisionLayer;
    EdgeList:TFasterListTFreeSubdivisionEdge;PointList:TFasterListTFreeSubdivisionControlPoint;
    Marker:TFreeMarker;
  function Resolve(List:TFPList;Id:Int64):Pointer;
  begin
    if (Id<1) or (Id>List.Count) or (List[Id-1]=nil) then raise Exception.Create('Unknown or deleted entity ID');
    Result:=List[Id-1];
  end;
  procedure CheckPoint(const P:T3DCoordinate);
  begin CheckCoordinate(P.X);CheckCoordinate(P.Y);CheckCoordinate(P.Z) end;
  procedure Editable(P:TFreeSubdivisionControlPoint);
  begin
    if not BatchAllowed[EntityId(P)-1] then raise Exception.Create('所选对象包含锁定点或约束点，请先解除锁定或约束。');
  end;
  procedure IncludePoint(P:TFreeSubdivisionControlPoint);
  begin Editable(P);Wanted[EntityId(P)-1]:=True end;
  function Changed(A,B:T3DCoordinate):Boolean;
  begin Result:=(A.X<>B.X) or (A.Y<>B.Y) or (A.Z<>B.Z) end;
  function Transformed(P:T3DCoordinate):T3DCoordinate;
  begin
    if Operation='translate' then begin Result.X:=P.X+Vector.X;Result.Y:=P.Y+Vector.Y;Result.Z:=P.Z+Vector.Z end
    else if Operation='scale' then begin Result.X:=P.X*Vector.X;Result.Y:=P.Y*Vector.Y;Result.Z:=P.Z*Vector.Z end
    else begin
      P:=Subtract(P,Pivot);Result:=RotateVector(P,SX,CX,SY,CY,SZ,CZ);
      // FreeGeometry.AddPoint clamps negative Y for half-hull editing. Rotation
      // around a pivot is signed vector arithmetic, for both hull and markers.
      Result.X:=Result.X+Pivot.X;Result.Y:=Result.Y+Pivot.Y;Result.Z:=Result.Z+Pivot.Z;
    end;
    CheckPoint(Result);
  end;
  function Direction(F:TFreeSubdivisionControlFace;P,Q:TFreeSubdivisionPoint):Integer;
  var At:Integer;
  begin
    for At:=0 to F.NumberOfPoints-1 do begin
      if (F.Point[At]=P) and (F.Point[(At+1) mod F.NumberOfPoints]=Q) then Exit(1);
      if (F.Point[At]=Q) and (F.Point[(At+1) mod F.NumberOfPoints]=P) then Exit(-1);
    end;
    Result:=0;
  end;
  function Target(P:TFreeSubdivisionPoint):TFreeSubdivisionControlPoint;
  var At:Integer;
  begin
    At:=EntityId(P)-1;Result:=TargetPoints[At];
    if Result=nil then raise Exception.Create('Missing reflected control point');
  end;
begin
  Pivot:=ZERO;
  if Request.Find('pivot')<>nil then begin
    Values:=TJSONArray(Field(Request,'pivot',jtArray));
    if Values.Count<>3 then raise Exception.Create('Rotation pivot requires three coordinates');
    for I:=0 to 2 do begin if Values[I].JSONType<>jtNumber then raise Exception.Create('Invalid pivot');CheckCoordinate(Values[I].AsFloat) end;
    Pivot:=SetPoint(Values[0].AsFloat,Values[1].AsFloat,Values[2].AsFloat);
  end;
  Operation:=StringField(Request,'operation');Kind:=StringField(Request,'kind');Result:=False;
  if Kind='point' then Slots:=Points else if Kind='edge' then Slots:=Edges
  else if Kind='face' then Slots:=Faces else if Kind='curve' then Slots:=Curves
  else raise Exception.Create('Unsupported transform selection');
  Values:=TJSONArray(Field(Request,'ids',jtArray));
  if (Values.Count<1) or (Values.Count>100000) then raise Exception.Create('Invalid selection size');
  SetLength(Ids,Values.Count);SetLength(Seen,Slots.Count);SetLength(Wanted,Points.Count);
  for I:=0 to Values.Count-1 do begin
    if Values[I].JSONType<>jtNumber then raise Exception.Create('Invalid entity ID');
    Number:=Values[I].AsFloat;CheckCoordinate(Number);
    if (Number<1) or (Frac(Number)<>0) or (Number>Slots.Count) then raise Exception.Create('Invalid entity ID');
    Ids[I]:=Trunc(Number);Resolve(Slots,Ids[I]);if Seen[Ids[I]-1] then raise Exception.Create('Duplicate entity ID');Seen[Ids[I]-1]:=True;
    if Kind='point' then IncludePoint(TFreeSubdivisionControlPoint(Slots[Ids[I]-1]))
    else if Kind='edge' then begin Edge:=TFreeSubdivisionControlEdge(Slots[Ids[I]-1]);IncludePoint(TFreeSubdivisionControlPoint(Edge.StartPoint));IncludePoint(TFreeSubdivisionControlPoint(Edge.EndPoint)) end
    else if Kind='face' then begin Face:=TFreeSubdivisionControlFace(Slots[Ids[I]-1]);for J:=0 to Face.NumberOfPoints-1 do IncludePoint(TFreeSubdivisionControlPoint(Face.Point[J])) end
    else for J:=0 to TFreeSubdivisionControlCurve(Slots[Ids[I]-1]).NumberOfControlPoints-1 do IncludePoint(TFreeSubdivisionControlCurve(Slots[Ids[I]-1]).ControlPoint[J]);
  end;
  SetLength(SourcePoints,Points.Count);Count:=0;
  for I:=0 to Points.Count-1 do if Wanted[I] then begin SourcePoints[Count]:=TFreeSubdivisionControlPoint(Points[I]);Inc(Count) end;
  SetLength(SourcePoints,Count);SetLength(Positions,Count);
  if Operation='mirror' then begin
    if Kind<>'face' then raise Exception.Create('镜像需要先选择面。');
    Wide:=IntegerField(Request,'axis',0);if Wide>2 then raise Exception.Create('Invalid mirror axis');Axis:=Wide;
    Number:=Field(Request,'distance',jtNumber).AsFloat;CheckCoordinate(Number);Distance:=TFloatType(Number);
    Weld:=Field(Request,'weld',jtBoolean).AsBoolean;
    Plane:=SetPlane(0,0,0,-Distance);case Axis of 0:Plane.a:=1;1:Plane.b:=1;2:Plane.c:=1 end;
    SetLength(SourceFaces,Length(Ids));N:=0;
    for I:=0 to High(Ids) do begin
      Face:=TFreeSubdivisionControlFace(Faces[Ids[I]-1]);InPlane:=True;
      for J:=0 to Face.NumberOfPoints-1 do begin A:=Face.Point[J].Coordinate;InPlane:=InPlane and (Plane.a*A.X+Plane.b*A.Y+Plane.c*A.Z+Plane.d=0) end;
      if not InPlane then begin SourceFaces[N]:=Face;Inc(N) end;
    end;
    if N=0 then Exit;SetLength(SourceFaces,N);
    // Fully coplanar faces are unchanged; do not leave unused copied points.
    for I:=0 to High(Wanted) do Wanted[I]:=False;
    for I:=0 to High(SourceFaces) do for J:=0 to SourceFaces[I].NumberOfPoints-1 do Wanted[EntityId(SourceFaces[I].Point[J])-1]:=True;
    for I:=0 to High(SourcePoints) do begin Positions[I]:=FreeGeometry.MirrorPlane(SourcePoints[I].Coordinate,Plane);CheckPoint(Positions[I]) end;
    SetLength(TargetPoints,Points.Count);
    for I:=0 to High(SourcePoints) do if Wanted[EntityId(SourcePoints[I])-1] then begin
      if Weld then Q:=Ship.Surface.AddControlPoint(Positions[I]) else begin Q:=Ship.Surface.AddControlPoint;Q.Coordinate:=Positions[I] end;
      if EntityIds.IndexOf(PtrUInt(Q))>=0 then Editable(Q);
      TargetPoints[EntityId(SourcePoints[I])-1]:=Q;
    end;
    PointList:=TFasterListTFreeSubdivisionControlPoint.Create;
    try
      for I:=0 to High(SourceFaces) do begin
        Face:=SourceFaces[I];PointList.Clear;
        for J:=Face.NumberOfPoints-1 downto 0 do begin Q:=Target(Face.Point[J]);if PointList.IndexOf(Q)>=0 then raise Exception.Create('合并后镜像面退化，操作已取消。');PointList.Add(Q) end;
        for J:=0 to PointList.Count-1 do begin
          Edge:=TFreeSubdivisionControlEdge(Ship.Surface.EdgeExists(PointList[J],PointList[(J+1) mod PointList.Count]));
          if (Edge<>nil) and (Edge.NumberOfFaces>=2) then raise Exception.Create('镜像会使一条边连接超过两个面，操作已取消。');
        end;
        NewFace:=Ship.Surface.AddControlFace(PointList,False);
        if NewFace=nil then raise Exception.Create('镜像目标已有相同的面，操作已取消。');
        NewFace.Layer:=Face.Layer;
      end;
      // Preserve source edge properties, including seams joined to existing points.
      for I:=0 to High(SourceFaces) do begin Face:=SourceFaces[I];
        for J:=0 to Face.NumberOfPoints-1 do begin
          P:=TFreeSubdivisionControlPoint(Face.Point[J]);Q:=TFreeSubdivisionControlPoint(Face.Point[(J+1) mod Face.NumberOfPoints]);
          Edge:=TFreeSubdivisionControlEdge(Ship.Surface.EdgeExists(P,Q));Other:=TFreeSubdivisionControlEdge(Ship.Surface.EdgeExists(Target(P),Target(Q)));
          if (Edge<>nil) and (Other<>nil) and (Edge<>Other) then Other.Crease:=Edge.Crease;
        end;
      end;
      for I:=0 to High(SourcePoints) do if Wanted[EntityId(SourcePoints[I])-1] then begin
        P:=SourcePoints[I];Q:=Target(P);if Q<>P then begin if P.VertexType=svCorner then Q.VertexType:=svCorner;Q.Locked:=P.Locked end;
      end;
    finally PointList.Free end;
    Result:=True;ReindexTopology;Exit;
  end;
  if (Operation<>'extrude') and (Operation<>'translate') and (Operation<>'scale') and (Operation<>'rotate') then raise Exception.Create('Unsupported advanced operation');
  Values:=TJSONArray(Field(Request,'vector',jtArray));if Values.Count<>3 then raise Exception.Create('Vector requires three numbers');
  for I:=0 to 2 do begin if Values[I].JSONType<>jtNumber then raise Exception.Create('Invalid vector');CheckCoordinate(Values[I].AsFloat) end;
  Vector:=Point3D(Values[0].AsFloat,Values[1].AsFloat,Values[2].AsFloat);
  if Operation='extrude' then begin
    if Kind<>'edge' then raise Exception.Create('挤出需要先选择边。');
    Layer:=TFreeSubdivisionLayer(Resolve(Layers,IntegerField(Request,'layer_id',1)));
    if (Vector.X=0) and (Vector.Y=0) and (Vector.Z=0) then Exit;
    SetLength(Adjacent,Length(Ids));
    for I:=0 to High(Ids) do begin
      Edge:=TFreeSubdivisionControlEdge(Edges[Ids[I]-1]);
      if Edge.NumberOfFaces>1 then raise Exception.Create('只能挤出边界边或独立边，请调整选择。');
      if Edge.NumberOfFaces=1 then Adjacent[I]:=TFreeSubdivisionControlFace(Edge.Face[0]);
      A:=Edge.StartPoint.Coordinate;B:=Edge.EndPoint.Coordinate;
      if (Double(A.Y)+Vector.Y<0) or (Double(B.Y)+Vector.Y<0) then raise Exception.Create('挤出后的 Y 坐标不能越过中心线。');
      C:=AddPointSymm(A,Vector);D:=AddPointSymm(B,Vector);CheckPoint(C);CheckPoint(D);
      if not Changed(A,C) or not Changed(B,D) or
         (((Double(B.Y)-A.Y)*(C.Z-A.Z)-(Double(B.Z)-A.Z)*(C.Y-A.Y)=0) and
          ((Double(B.Z)-A.Z)*(C.X-A.X)-(Double(B.X)-A.X)*(C.Z-A.Z)=0) and
          ((Double(B.X)-A.X)*(C.Y-A.Y)-(Double(B.Y)-A.Y)*(C.X-A.X)=0)) then raise Exception.Create('挤出方向与边重合或距离太小，会产生退化面。');
    end;
    OldLayer:=Ship.Surface.ActiveLayer;EdgeList:=TFasterListTFreeSubdivisionEdge.Create;
    try
      for I:=0 to High(Ids) do EdgeList.Add(TFreeSubdivisionControlEdge(Edges[Ids[I]-1]));
      // SaveProject sorts control points by address. ExtrudeEdges initializes
      // the newly appended range; sorted insertion would put fresh points among
      // old ones, randomly skipping corners or changing unrelated old points.
      // Restore append semantics before creating anything. Capture may sort again
      // afterwards; stable IDs are bound to objects, never array positions.
      Ship.Surface.ControlPoints.IsSorted:=False;Ship.Surface.ControlEdges.IsSorted:=False;
      Ship.Surface.ActiveLayer:=Layer;Ship.Surface.ExtrudeEdges(EdgeList,Vector);
      for I:=0 to High(Ids) do if Adjacent[I]<>nil then begin
        Edge:=TFreeSubdivisionControlEdge(Edges[Ids[I]-1]);
        for J:=0 to Edge.NumberOfFaces-1 do if Edge.Face[J]<>Adjacent[I] then begin
          Face:=TFreeSubdivisionControlFace(Edge.Face[J]);K:=Direction(Face,Edge.StartPoint,Edge.EndPoint);
          if (K<>0) and (K=Direction(Adjacent[I],Edge.StartPoint,Edge.EndPoint)) then Face.FlipNormal;
        end;
      end;
    finally EdgeList.Free;Ship.Surface.ActiveLayer:=OldLayer end;
    Result:=True;ReindexTopology;Exit;
  end;
  if (Operation='scale') and ((Vector.X<=0) or (Vector.Y<=0) or (Vector.Z<=0)) then raise Exception.Create('缩放倍率必须大于零；反射请使用镜像。');
  if Operation='rotate' then begin
    if (Abs(Vector.X)>360000) or (Abs(Vector.Y)>360000) or (Abs(Vector.Z)>360000) then raise Exception.Create('Rotation angle limit exceeded');
    CX:=Cos(DegToRad(Vector.X));SX:=Sin(DegToRad(Vector.X));CY:=Cos(DegToRad(Vector.Y));SY:=Sin(DegToRad(Vector.Y));CZ:=Cos(DegToRad(Vector.Z));SZ:=Sin(DegToRad(Vector.Z));
  end;
  All:=Count=Ship.Surface.NumberOfControlPoints;Markers:=False;
  if Request.Find('adjust_markers')<>nil then Markers:=Field(Request,'adjust_markers',jtBoolean).AsBoolean;
  if Markers and not All then raise Exception.Create('标记只能随整个模型一起变换。');
  for I:=0 to High(SourcePoints) do begin Positions[I]:=Transformed(SourcePoints[I].Coordinate);Result:=Result or Changed(Positions[I],SourcePoints[I].Coordinate) end;
  if All and (Operation='scale') then Result:=Result or (Vector.X<>1) or (Vector.Y<>1) or (Vector.Z<>1);
  if not Result then Exit;
  if Markers then for I:=0 to Ship.NumberOfMarkers-1 do for J:=0 to Ship.Marker[I].NumberOfPoints-1 do Position:=Transformed(Ship.Marker[I].Point[J]);
  if All and (Operation='scale') then Ship.Edit.Model_Scale(Vector,False,Markers)
  else begin
    for I:=0 to High(SourcePoints) do SourcePoints[I].Coordinate:=Positions[I];
    if All and (Operation='translate') then begin
      if not Ship.ProjectSettings.UseDefaultSplitSectionLocation then Ship.ProjectSettings.ProjectSplitSectionLocation:=Ship.ProjectSettings.ProjectSplitSectionLocation+Vector.X;
      // n.p+d=0: every translated section, including diagonals, follows the hull.
      for I:=1 to 4 do for J:=0 to SectionCount(I)-1 do begin Plane:=SectionAt(I,J).Plane;
        Plane.d:=Plane.d-(Plane.a*Vector.X+Plane.b*Vector.Y+Plane.c*Vector.Z);CheckCoordinate(Plane.d);SectionAt(I,J).Plane:=Plane;SectionAt(I,J).Built:=False;
      end;
    end;
    if Markers then for I:=0 to Ship.NumberOfMarkers-1 do begin Marker:=Ship.Marker[I];for J:=0 to Marker.NumberOfPoints-1 do Marker.Point[J]:=Transformed(Marker.Point[J]) end;
  end;
end;
