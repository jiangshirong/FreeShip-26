{ Model construction, grouping and explicit repairs. GPL-2.0-or-later. }
function TDocument.InspectModeling:TJSONObject;
var Engine:TLackenbyEngine;Prop:TBodyProp;Displ:Double;
begin
  Engine:=TLackenbyEngine.Create(Ship);
  try
    Prop:=Engine.Measure;
    with Ship.ProjectSettings do Displ:=VolumeToDisplacement(Prop.Displacement,ProjectWaterDensity,ProjectAppendageCoefficient,ProjectUnits);
    Result:=TJSONObject.Create(['volume',Double(Prop.Displacement),'displacement',Displ,
      'lcb',Double(Prop.LCB),'cp',Double(Prop.Cp),'length',Double(Prop.Length),'midship',Double(Engine.MainLoc)]);
  finally Engine.Free end;
end;

procedure TDocument.WriteModelingData(Root:TJSONObject);
var List,Ids,Coords,Flags:TJSONArray;Obj,SplineData:TJSONObject;
    I,J,K,H:Integer;Group:TFreeSubdivisionControlPointGroup;Marker:TFreeMarker;Locked:Boolean;
    Face:TFreeSubdivisionControlFace;Child:TFreeSubdivisionFace;Point:TFreeSubdivisionPoint;
    Seen:TFasterListTFreeSubdivisionPoint;
begin
  if NativeDisplay=nil then List:=TJSONArray.Create
  else List:=TJSONArray(Field(NativeDisplay,'surface_normals',jtArray).Clone);
  Root.Add('surface_normals',List);
  Root.Add('normal_color',Int64(ColorToRGB(Ship.Surface.NormalColor)));
  if (NativeDisplay=nil) and Ship.Visibility.ShowNormals then begin
    Seen:=TFasterListTFreeSubdivisionPoint.Create(False,True);
    try
      for I:=0 to Ship.Surface.NumberOfControlFaces-1 do begin
        Face:=Ship.Surface.ControlFace[I];if not Face.Layer.SurfaceVisible then Continue;
        Seen.Clear;
        for J:=0 to Face.ChildCount-1 do begin
          Child:=Face.Child[J];
          for K:=0 to Child.NumberOfPoints-1 do if Seen.SortedIndexOf(Child.Point[K])<0 then Seen.AddSorted(Child.Point[K]);
        end;
        for H:=0 to Seen.Count-1 do begin
          Point:=Seen[H];Obj:=TJSONObject.Create;List.Add(Obj);
          Obj.Add('face',EntityId(Face));Obj.Add('p',Coordinates(Point.Coordinate));Obj.Add('n',Coordinates(Point.Normal));
        end;
      end;
    finally Seen.Free end;
  end;
  List:=TJSONArray.Create;Root.Add('point_groups',List);
  for I:=0 to Ship.Surface.NumberOfControlPointGroups-1 do begin
    Group:=Ship.Surface.ControlPointGroup[I];Obj:=TJSONObject.Create;List.Add(Obj);
    Locked:=Group.ControlPoints.Count>0;
    for J:=0 to Group.ControlPoints.Count-1 do Locked:=Locked and Group.ControlPoints[J].Locked;
    Obj.Add('index',I);Obj.Add('name',Group.Name);Obj.Add('locked',Locked);
    Ids:=TJSONArray.Create;Obj.Add('points',Ids);
    for J:=0 to Group.ControlPoints.Count-1 do Ids.Add(EntityId(Group.ControlPoints[J]));
  end;
  List:=TJSONArray.Create;Root.Add('markers',List);
  for I:=0 to Ship.NumberOfMarkers-1 do begin
    Marker:=Ship.Marker[I];Obj:=TJSONObject.Create;List.Add(Obj);
    Obj.Add('index',I);Obj.Add('visible',Marker.Visible);
    Obj.Add('color',Int64(ColorToRGB(Ship.Preferences.MarkerColor)));
    SplineData:=TJSONObject.Create;Obj.Add('spline',SplineData);
    Coords:=TJSONArray.Create;Flags:=TJSONArray.Create;
    SplineData.Add('points',Coords);SplineData.Add('knuckles',Flags);
    for J:=0 to Marker.NumberOfPoints-1 do begin Coords.Add(Coordinates(Marker.Point[J]));Flags.Add(Marker.Knuckle[J]) end;
  end;
end;

function TDocument.ModelToolsEdit(Request:TJSONObject):Boolean;
var Op,Name:string;Ids:TIds;I,J,K,N,M,Rows,Cols,Axis,Count,Index,Head:Integer;
    Origin,Finish,P,V:T3DCoordinate;SizeA,SizeB,Radius,Angle:TFloatType;
    Grid:TFreeSubdivisionControlPointGrid;Layer,SourceLayer:TFreeSubdivisionLayer;
    List,Queue:TFPList;Chosen,Visited:array of Boolean;Orientation:array of Integer;
    F,G:TFreeSubdivisionControlFace;E:TFreeSubdivisionEdge;
    Group:TFreeSubdivisionControlPointGroup;Marker:TFreeMarker;
    Values,Path,Position:TJSONArray;Point:TFreeSubdivisionControlPoint;
    Matrix,Inv,OrgPts,NewPts:TFreeMatrix;ControlList:TFasterListTFreeSubdivisionControlPoint;
    LayerList:TFasterListTFreeSubdivisionLayer;Assembled:TFreeFaceArray;Patch:TFreeFaceGrid;
    ClosedStart,ClosedEnd,SameLayer,Flag:Boolean;
    Engine:TLackenbyEngine;Volume,LCB:Double;
  function Number(const Key:string):Double;
  begin Result:=Field(Request,Key,jtNumber).AsFloat;CheckCoordinate(Result) end;
  function Vector(const Key:string):T3DCoordinate;
  var A:TJSONArray;H:Integer;
  begin
    A:=TJSONArray(Field(Request,Key,jtArray));if A.Count<>3 then raise Exception.Create('Expected three coordinates');
    for H:=0 to 2 do begin if A[H].JSONType<>jtNumber then raise Exception.Create('Expected numeric coordinates');CheckCoordinate(A[H].AsFloat) end;
    Result:=SetPoint(A[0].AsFloat,A[1].AsFloat,A[2].AsFloat);
  end;
  function FaceDirection(Face:TFreeSubdivisionFace;Edge:TFreeSubdivisionEdge):Integer;
  var A,B:Integer;
  begin
    A:=Face.IndexOfPoint(Edge.StartPoint);B:=Face.IndexOfPoint(Edge.EndPoint);
    if (A<0) or (B<0) then raise Exception.Create('Invalid face-edge adjacency');
    if (A+1) mod Face.NumberOfPoints=B then Result:=1
    else if (B+1) mod Face.NumberOfPoints=A then Result:=-1
    else raise Exception.Create('Edge is not on the face boundary');
  end;
  procedure CleanLayers;
  var H:Integer;L:TFreeSubdivisionLayer;
  begin
    for H:=Ship.Surface.NumberOfLayers-1 downto 0 do begin
      L:=Ship.Surface.Layer[H];
      if (L.Count=0) and (Ship.Surface.NumberOfLayers>1) then L.Delete;
    end;
    Ship.Surface.ActiveLayer:=Ship.Surface.Layer[0];
  end;
  procedure CopyLayerProperties(Target,Source:TFreeSubdivisionLayer);
  begin
    Target.AssignProperties(Source);
    // AssignProperties omits several calculation flags and forces visibility.
    Target.SurfaceVisible:=Source.SurfaceVisible;Target.ControlNetVisible:=Source.ControlNetVisible;
    Target.UseInHydrostatics:=Source.UseInHydrostatics;Target.Symmetric:=Source.Symmetric;
    Target.ShowInLinesplan:=Source.ShowInLinesplan;Target.UseForIntersections:=Source.UseForIntersections;
  end;
  procedure ImportGrid(const G:TFreeSubdivisionControlPointGrid;Width,Height:Integer;Target:TFreeSubdivisionLayer);
  var Row,Col,H:Integer;FacePoints:TFasterListTFreeSubdivisionControlPoint;WasLoading:Boolean;
  begin
    // Same face order, crease and corner rules as ImportControlPointGrid, with
    // bounded scratch storage and guaranteed cleanup of the loading flag.
    WasLoading:=Ship.Surface.IsLoading;FacePoints:=TFasterListTFreeSubdivisionControlPoint.Create;
    Ship.Surface.IsLoading:=True;
    try
      for Row:=1 to Height-1 do for Col:=1 to Width-1 do begin
        FacePoints.Clear;FacePoints.Add(G[Row-1][Col-1]);FacePoints.Add(G[Row-1][Col]);
        FacePoints.Add(G[Row][Col]);FacePoints.Add(G[Row][Col-1]);
        Ship.Surface.AddControlFace(FacePoints,True,Target);
      end;
      for Row:=0 to Height-1 do for Col:=0 to Width-1 do begin
        for H:=0 to G[Row][Col].NumberOfEdges-1 do G[Row][Col].Edge[H].Crease:=False;
        if G[Row][Col].NumberOfFaces<2 then G[Row][Col].VertexType:=svCorner;
      end;
    finally Ship.Surface.IsLoading:=WasLoading;FacePoints.Free end;
  end;
begin
  Op:=StringField(Request,'operation');Result:=False;
  if (Op='create_group') or (Op='update_group') or (Op='delete_group') then begin
    Group:=nil;
    if Op<>'create_group' then begin
      Index:=BoundedInteger(Request,'index',0,Ship.Surface.NumberOfControlPointGroups-1);
      if Index>=Ship.Surface.NumberOfControlPointGroups then raise Exception.Create('Unknown point group in this revision');
      Group:=Ship.Surface.ControlPointGroup[Index];
    end;
    if Op='delete_group' then begin Group.Delete;Exit(True) end;
    Ids:=ReadEntityIds(Request,Points);if Length(Ids)<2 then raise Exception.Create('A group needs at least two points');
    Name:=TextValue(Ship,Request,'name');if (Name='') or (Length(Name)>4096) then raise Exception.Create('Invalid group name');
    Flag:=Field(Request,'locked',jtBoolean).AsBoolean;
    if Group=nil then begin Group:=TFreeSubdivisionControlPointGroup.Create(Ship.Surface);Ship.Surface.ControlPointGroups.Add(Group) end
    else begin
      ClosedStart:=Group.ControlPoints.Count>0;
      for I:=0 to Group.ControlPoints.Count-1 do ClosedStart:=ClosedStart and Group.ControlPoints[I].Locked;
      SameLayer:=(Group.Name=Name) and (ClosedStart=Flag) and (Group.ControlPoints.Count=Length(Ids));
      if SameLayer then for I:=0 to High(Ids) do if Group.ControlPoints.IndexOf(TFreeSubdivisionControlPoint(ResolveEntity(Points,Ids[I])))<0 then SameLayer:=False;
      if SameLayer then Exit;
      Group.BreakGroup;
    end;
    Group.Name:=Name;
    // FBM does not store Group.Locked. Use its persisted point locks, including
    // for history, rather than exposing a checkbox that silently resets on save.
    Group.Locked:=False;
    for I:=0 to High(Ids) do begin
      Point:=TFreeSubdivisionControlPoint(ResolveEntity(Points,Ids[I]));Group.AddControlPoint(Point);
      if Flag or (Op='update_group') then Point.Locked:=Flag;
    end;
    RequireVersion(Ship,fv430);Result:=True;
  end else if Op='grid_panel' then begin
    Origin:=Vector('origin');SizeA:=Number('size_a');SizeB:=Number('size_b');
    if (SizeA<=0) or (SizeB<=0) then raise Exception.Create('Panel dimensions must be positive');
    Cols:=BoundedInteger(Request,'columns',2,100);Rows:=BoundedInteger(Request,'rows',2,100);
    if (Cols>100) or (Rows>100) then raise Exception.Create('Panel grid must have 2..100 points in each direction');
    Axis:=BoundedInteger(Request,'plane',0,2);if Axis>2 then raise Exception.Create('Invalid panel plane');
    Layer:=TFreeSubdivisionLayer(ResolveEntity(Layers,IntegerField(Request,'layer_id',1)));
    SetLength(Grid,Rows,Cols);
    for I:=0 to Cols-1 do for J:=0 to Rows-1 do begin
      P:=Origin;
      case Axis of
        0:begin P.X:=Origin.X+I*SizeA/(Cols-1);P.Y:=Origin.Y+J*SizeB/(Rows-1) end;
        1:begin P.X:=Origin.X+I*SizeA/(Cols-1);P.Z:=Origin.Z+J*SizeB/(Rows-1) end;
        2:begin P.Y:=Origin.Y+I*SizeA/(Cols-1);P.Z:=Origin.Z+J*SizeB/(Rows-1) end;
      end;
      CheckCoordinate(P.X);CheckCoordinate(P.Y);CheckCoordinate(P.Z);
      // A new panel does not silently weld unrelated existing vertices.
      Grid[J][I]:=Ship.Surface.AddControlPoint;Grid[J][I].Coordinate:=P;
    end;
    Ship.Surface.ControlPoints.IsSorted:=False;Ship.Surface.ControlEdges.IsSorted:=False;
    ImportGrid(Grid,Cols,Rows,Layer);Result:=True;
  end else if Op='cylinder' then begin
    Origin:=Vector('origin');Finish:=Vector('end');V:=Subtract(Finish,Origin);
    if Sqr(Double(V.X))+Sqr(Double(V.Y))+Sqr(Double(V.Z))<1e-12 then raise Exception.Create('Cylinder axis must have nonzero length');
    Radius:=Number('radius');if Radius<=0 then raise Exception.Create('Cylinder radius must be positive');
    N:=BoundedInteger(Request,'segments',4,128);if N>128 then raise Exception.Create('Cylinder needs 4..128 circumferential points');
    ClosedStart:=Field(Request,'close_start',jtBoolean).AsBoolean;ClosedEnd:=Field(Request,'close_end',jtBoolean).AsBoolean;
    Layer:=TFreeSubdivisionLayer(ResolveEntity(Layers,IntegerField(Request,'layer_id',1)));
    Matrix:=TFreeMatrix.Create;Inv:=nil;OrgPts:=nil;NewPts:=nil;
    try
      // Same interpolating control-ring construction as Plus Geometry_AddCylinder.
      Matrix.SetSize(N+4,N+4);Matrix.Fill(0);Matrix.Value[0,0]:=1;
      for I:=2 to N+3 do begin Matrix.Value[I-1,I-2]:=1/6;Matrix.Value[I-1,I-1]:=2/3;Matrix.Value[I-1,I]:=1/6 end;
      Matrix.Value[N+3,N+3]:=1;Inv:=Matrix.Invert;
      OrgPts:=TFreeMatrix.Create;OrgPts.SetSize(3,N+4);
      for I:=1 to N do begin
        Angle:=-((I-1)/N)*2*Pi;P:=SetPoint(Origin.X+Sin(Angle)*Radius,Origin.Y+Cos(Angle)*Radius,Origin.Z);
        P:=RotatePointAroundVector(P,Origin,V);
        OrgPts.Value[I+1,0]:=P.X;OrgPts.Value[I+1,1]:=P.Y;OrgPts.Value[I+1,2]:=P.Z;
      end;
      for I:=0 to 2 do begin OrgPts.Value[1,I]:=OrgPts.Value[N+1,I];OrgPts.Value[0,I]:=OrgPts.Value[N,I];OrgPts.Value[N+2,I]:=OrgPts.Value[2,I];OrgPts.Value[N+3,I]:=OrgPts.Value[3,I] end;
      NewPts:=Inv.Multiply(OrgPts);SetLength(Grid,2,N+1);
      for I:=0 to N-1 do for J:=0 to 1 do begin
        P:=SetPoint(NewPts.Value[I+2,0]+J*V.X,NewPts.Value[I+2,1]+J*V.Y,NewPts.Value[I+2,2]+J*V.Z);
        CheckCoordinate(P.X);CheckCoordinate(P.Y);CheckCoordinate(P.Z);
        Grid[J][I]:=Ship.Surface.AddControlPoint;Grid[J][I].Coordinate:=P;
      end;
      Grid[0][N]:=Grid[0][0];Grid[1][N]:=Grid[1][0];
      Ship.Surface.ControlPoints.IsSorted:=False;Ship.Surface.ControlEdges.IsSorted:=False;
      Count:=Ship.Surface.NumberOfControlEdges;
      ImportGrid(Grid,N+1,2,Layer);
      for I:=Count to Ship.Surface.NumberOfControlEdges-1 do Ship.Surface.ControlEdge[I].Crease:=False;
      ControlList:=TFasterListTFreeSubdivisionControlPoint.Create;
      try
        if ClosedStart then begin for I:=N-1 downto 0 do ControlList.Add(Grid[0][I]);Ship.Surface.AddControlFaceN(ControlList,True,Layer) end;
        ControlList.Clear;
        if ClosedEnd then begin for I:=0 to N-1 do ControlList.Add(Grid[1][I]);Ship.Surface.AddControlFaceN(ControlList,True,Layer) end;
      finally ControlList.Free end;
      Result:=True;
    finally NewPts.Free;OrgPts.Free;Inv.Free;Matrix.Free end;
  end else if Op='auto_group_layers' then begin
    SetLength(Chosen,Faces.Count);SetLength(Visited,Faces.Count);
    if Request.Find('ids')<>nil then begin Ids:=ReadEntityIds(Request,Faces);for I:=0 to High(Ids) do Chosen[Ids[I]-1]:=True end
    else for I:=0 to Faces.Count-1 do if Faces[I]<>nil then Chosen[I]:=TFreeSubdivisionControlFace(Faces[I]).Layer.SurfaceVisible;
    Queue:=TFPList.Create;
    try
      for I:=0 to Faces.Count-1 do if Chosen[I] and not Visited[I] then begin
        Queue.Clear;Queue.Add(Faces[I]);Visited[I]:=True;Head:=0;
        while Head<Queue.Count do begin
          F:=TFreeSubdivisionControlFace(Queue[Head]);Inc(Head);
          for J:=0 to F.NumberOfPoints-1 do begin
            E:=Ship.Surface.EdgeExists(F.Point[J],F.Point[(J+1) mod F.NumberOfPoints]);
            if (E=nil) or E.Crease then Continue;
            for K:=0 to E.NumberOfFaces-1 do begin
              Index:=EntityId(E.Face[K])-1;
              if Chosen[Index] and not Visited[Index] and (TFreeSubdivisionControlFace(E.Face[K]).Layer=F.Layer) then begin Visited[Index]:=True;Queue.Add(E.Face[K]) end;
            end;
          end;
        end;
        F:=TFreeSubdivisionControlFace(Queue[0]);SourceLayer:=F.Layer;SameLayer:=True;
        for J:=0 to Queue.Count-1 do if TFreeSubdivisionControlFace(Queue[J]).Layer<>SourceLayer then SameLayer:=False;
        if SameLayer and (Queue.Count=SourceLayer.Count) then Continue;
        Layer:=Ship.Surface.AddNewLayer;CopyLayerProperties(Layer,SourceLayer);
        Name:=SourceLayer.ServiceStoredName;if Name='' then Name:='Layer';
        Layer.Name:=Name+' '+IntToStr(I+1);
        for J:=0 to Queue.Count-1 do TFreeSubdivisionControlFace(Queue[J]).Layer:=Layer;
        Result:=True;
      end;
      if Result then CleanLayers;
    finally Queue.Free end;
  end else if Op='assemble_faces' then begin
    LayerList:=TFasterListTFreeSubdivisionLayer.Create;
    try
      // Assemble each source layer separately: joining patches must not silently
      // replace another layer's thickness, density, symmetry or hydro flags.
      Count:=Ship.Surface.NumberOfLayers;
      for M:=0 to Count-1 do begin
      LayerList.Clear;LayerList.Add(Ship.Surface.Layer[M]);
      Ship.Surface.AssembleFacesToPatches(LayerList,amNurbs,Assembled,N);
      for I:=0 to N-1 do begin
        Patch:=Assembled[I];Layer:=nil;
        for J:=0 to Patch.NRows-1 do for K:=0 to Patch.NCols-1 do begin
          F:=Patch.Faces[J][K];if F=nil then Continue;
          if Layer=nil then begin Layer:=Ship.Surface.AddNewLayer;CopyLayerProperties(Layer,F.Layer);Layer.Name:='Patch '+IntToStr(M+1)+'-'+IntToStr(I+1) end;
          F.Layer:=Layer;Result:=True;
        end;
      end;
      end;
      if Result then CleanLayers;
    finally LayerList.Free end;
  end else if Op='repair_normals' then begin
    SetLength(Orientation,Faces.Count);Queue:=TFPList.Create;
    try
      for I:=0 to Faces.Count-1 do if (Faces[I]<>nil) and (Orientation[I]=0) then begin
        Orientation[I]:=1;Queue.Clear;Queue.Add(Faces[I]);Head:=0;
        while Head<Queue.Count do begin
          F:=TFreeSubdivisionControlFace(Queue[Head]);Inc(Head);
          for J:=0 to F.NumberOfPoints-1 do begin
            E:=Ship.Surface.EdgeExists(F.Point[J],F.Point[(J+1) mod F.NumberOfPoints]);
            if E=nil then raise Exception.Create('Missing face edge');
            if E.NumberOfFaces>2 then raise Exception.Create('Repair nonmanifold topology before orienting faces');
            if E.NumberOfFaces<>2 then Continue;
            G:=TFreeSubdivisionControlFace(E.Face[0]);if G=F then G:=TFreeSubdivisionControlFace(E.Face[1]);
            Index:=EntityId(G)-1;N:=-Orientation[EntityId(F)-1]*FaceDirection(F,E)*FaceDirection(G,E);
            if (Orientation[Index]<>0) and (Orientation[Index]<>N) then raise Exception.Create('Surface cannot be consistently oriented');
            if Orientation[Index]=0 then begin Orientation[Index]:=N;Queue.Add(G) end;
          end;
        end;
      end;
      for I:=0 to Faces.Count-1 do if Orientation[I]=-1 then RequireEditableFace(TFreeSubdivisionControlFace(Faces[I]));
      for I:=0 to Faces.Count-1 do if Orientation[I]=-1 then begin TFreeSubdivisionControlFace(Faces[I]).FlipNormal;Result:=True end;
    finally Queue.Free end;
  end else if Op='add_markers' then begin
    Values:=TJSONArray(Field(Request,'paths',jtArray));
    if (Values.Count<1) or (Values.Count>4096) or (Values.Count+Ship.NumberOfMarkers>4096) then raise Exception.Create('Invalid marker count');
    Count:=0;
    for I:=0 to Values.Count-1 do begin
      if Values[I].JSONType<>jtArray then raise Exception.Create('Invalid marker path');Path:=TJSONArray(Values[I]);
      Inc(Count,Path.Count);if (Path.Count<2) or (Count>100000) then raise Exception.Create('Invalid marker point count');
      Marker:=TFreeMarker.Create(Ship.Surface);Ship.Edit.Marker_Add(Marker);Marker.Visible:=True;
      for J:=0 to Path.Count-1 do begin
        if Path[J].JSONType<>jtArray then raise Exception.Create('Invalid marker point');Position:=TJSONArray(Path[J]);
        if Position.Count<>3 then raise Exception.Create('Invalid marker coordinate');
        for K:=0 to 2 do begin if Position[K].JSONType<>jtNumber then raise Exception.Create('Invalid marker coordinate');CheckCoordinate(Position[K].AsFloat) end;
        Marker.Add(SetPoint(Position[0].AsFloat,Position[1].AsFloat,Position[2].AsFloat));
      end;
    end;
    Ship.Visibility.ShowMarkers:=True;Result:=True;
  end else if Op='lackenby' then begin
    Ids:=ReadEntityIds(Request,Layers);Volume:=Number('displacement');LCB:=Number('lcb');
    N:=BoundedInteger(Request,'iterations',1,100);if N>100 then raise Exception.Create('Iteration limit is 100');
    if Volume<=0 then raise Exception.Create('Target displacement must be positive');
    with Ship.ProjectSettings do Volume:=DisplacementToVolume(Volume,ProjectWaterDensity,ProjectAppendageCoefficient,ProjectUnits);
    ControlList:=TFasterListTFreeSubdivisionControlPoint.Create;SetLength(Chosen,Points.Count);
    try
      for I:=0 to High(Ids) do begin
        Layer:=TFreeSubdivisionLayer(ResolveEntity(Layers,Ids[I]));
        for J:=0 to Layer.Count-1 do begin
          F:=Layer.Items[J];RequireEditableFace(F);
          for K:=0 to F.NumberOfPoints-1 do begin
            Index:=EntityId(F.Point[K])-1;
            if not Chosen[Index] then begin Chosen[Index]:=True;ControlList.Add(TFreeSubdivisionControlPoint(F.Point[K])) end;
          end;
        end;
      end;
      Engine:=TLackenbyEngine.Create(Ship);
      try Result:=Engine.Transform(ControlList,Volume,LCB,N) finally Engine.Free end;
    finally ControlList.Free end;
  end else if Op='delete_markers' then begin
    Result:=Ship.NumberOfMarkers>0;
    for I:=Ship.NumberOfMarkers-1 downto 0 do Ship.Marker[I].Delete;
  end else raise Exception.Create('Unsupported modeling operation');
end;
