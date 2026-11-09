{ Read-only access to the original unfolding coordinates. Included only in the
  isolated FreeGeometry build. Never round coordinates to screen/export digits. }
function PlateNumber(V:Extended):Double;
begin Result:=V end;
function TFreeDevelopedPatch.ServicePlateData:TJSONObject;
var Points,Original,Edges,Faces,Curves,Corners:TJSONArray; Row:TJSONArray;
    I,J,K,N,Side,Offset:Integer; P:T3DCoordinate; P2:T2DCoordinate;
    E:TFreeSubdivisionEdge; Face:TFreeSubdivisionFace; C:TJSONObject;
    Lists:array[0..3] of TFasterListTFreeSpline; S:TFreeSpline;
begin
  Result:=TJSONObject.Create;
  try
    Result.Add('name',Name);Result.Add('color',LongInt(ColorToRGB(Owner.Color)));
    Result.Add('dxf_color',FindDXFColorIndex(Owner.Color));Result.Add('joined_mirror',FMirror);
    Result.Add('reference_area',Double(ReferenceArea));Result.Add('signed_area_error',Double(TotalAreaError));
    Result.Add('absolute_area_error',Double(AbsoluteAreaError));Result.Add('max_face_area_error',Double(MaxAreaError));
    Result.Add('min_edge_error',Double(MinError));Result.Add('max_edge_error',Double(MaxError));
    Result.Add('normalized_error',Double(NormalizedError));Result.Add('iterations',NumberOfIterations);
    Points:=TJSONArray.Create;Result.Add('points',Points);
    Original:=TJSONArray.Create;Result.Add('original',Original);
    Edges:=TJSONArray.Create;Result.Add('edges',Edges);
    Faces:=TJSONArray.Create;Result.Add('faces',Faces);
    Corners:=TJSONArray.Create;Result.Add('corners',Corners);
    N:=FPoints.Count;
    for Side:=0 to Ord(FMirror) do begin
      Offset:=Side*N;
      for I:=0 to N-1 do begin
        if Side=0 then P:=Point[I] else P:=MirrorPoint[I];
        Points.Add(TJSONArray.Create([PlateNumber(P.X),PlateNumber(P.Y)]));
        P:=FPoints[I].Coordinate;if Side=1 then P.Y:=-P.Y;
        Original.Add(TJSONArray.Create([PlateNumber(P.X),PlateNumber(P.Y),PlateNumber(P.Z)]));
      end;
      for I:=0 to FEdges.Count-1 do begin
        E:=FEdges[I];J:=FPoints.SortedIndexOf(E.StartPoint);K:=FPoints.SortedIndexOf(E.EndPoint);
        if (J<0) or (K<0) then raise Exception.Create('展开边缺少端点。');
        Edges.Add(TJSONArray.Create([J+Offset,K+Offset,Double(FEdgeErrors[I]),FBoundaryEdges.IndexOf(E)>=0]));
      end;
      for I:=0 to FDoneList.Count-1 do begin
        Face:=FDoneList[I];Row:=TJSONArray.Create;Faces.Add(Row);
        for J:=0 to Face.NumberOfPoints-1 do begin
          K:=FPoints.SortedIndexOf(Face.Point[J]);
          if K<0 then raise Exception.Create('展开面缺少顶点。');
          Row.Add(K+Offset);
        end;
      end;
      for I:=0 to FCorners.Count-1 do Corners.Add(FPoints.SortedIndexOf(FCorners[I])+Offset);
    end;
    Curves:=TJSONArray.Create;Result.Add('curves',Curves);
    Lists[0]:=FStations;Lists[1]:=FButtocks;Lists[2]:=FWaterlines;Lists[3]:=FDiagonals;
    for K:=0 to 3 do for I:=0 to Lists[K].Count-1 do begin
      PlateCheckpoint;S:=Lists[K][I];C:=TJSONObject.Create;Curves.Add(C);
      C.Add('kind',K+1);C.Add('color',LongInt(ColorToRGB(S.Color)));C.Add('dxf_color',FindDXFColorIndex(S.Color));
      Row:=TJSONArray.Create;C.Add('points',Row);
      for J:=0 to S.Fragments do begin
        P:=S.Value(J/Max(1,S.Fragments));P2.X:=P.X;P2.Y:=P.Y;
        if FMirrorOnScreen and not FMirror then P2.Y:=-P2.Y;
        P:=ConvertTo3D(P2);Row.Add(TJSONArray.Create([PlateNumber(P.X),PlateNumber(P.Y)]));
      end;
    end;
  except Result.Free;raise end;
end;
