{ GPL-2.0-or-later. The C++ service owns committed document states and history.
  This adapter retains one working state for legacy operations / rollback only. }
procedure TDocument.ApplyNativeCoordinates(const Filename:string);
var Stream:TFileStream;Text:string;Data:TJSONData;Moves:TJSONArray;Move:TJSONObject;
    Ids:TIds;Positions:array of T3DCoordinate;Seen:array of Boolean;
    I:Integer;Id:Int64;Point:TFreeSubdivisionControlPoint;
begin
  Data:=nil;Stream:=TFileStream.Create(Filename,fmOpenRead or fmShareDenyWrite);
  try
    if (Stream.Size<2) or (Stream.Size>256*1024*1024) then raise Exception.Create('Invalid native coordinate file size');
    SetLength(Text,Stream.Size);Stream.ReadBuffer(Text[1],Length(Text));
  finally Stream.Free end;
  try
    Data:=GetJSON(Text);if Data.JSONType<>jtArray then raise Exception.Create('Invalid native coordinate array');
    Moves:=TJSONArray(Data);
    if (Moves.Count<1) or (Moves.Count>Points.Count) then raise Exception.Create('Invalid native coordinate count');
    SetLength(Ids,Moves.Count);SetLength(Positions,Moves.Count);SetLength(Seen,Points.Count);
    for I:=0 to Moves.Count-1 do begin
      if Moves[I].JSONType<>jtObject then raise Exception.Create('Invalid native coordinate item');
      Move:=TJSONObject(Moves[I]);Id:=IntegerField(Move,'point_id',1);
      if (Id>Points.Count) or (Points[Id-1]=nil) then raise Exception.Create('Unknown native point ID');
      if Seen[Id-1] then raise Exception.Create('Duplicate native point ID');Seen[Id-1]:=True;
      Point:=TFreeSubdivisionControlPoint(Points[Id-1]);
      if Point.Locked then raise Exception.Create('Native candidate changes a locked point');
      Ids[I]:=Id;Positions[I]:=ReadPosition(Move);
    end;
    // Absolute coordinate property only: C++ already propagated all anchors and
    // constraints. Calling SetCoordinate would apply those dependencies twice.
    for I:=0 to High(Ids) do TFreeSubdivisionControlPoint(Points[Ids[I]-1]).Coordinate:=Positions[I];
  finally Data.Free end;
end;

function ReadDocumentState(const Filename,BindingFile:string;Candidate:TDocument):TModelState;
var Stream:TFileStream;Text:string;Data:TJSONData;Obj,High:TJSONObject;
  procedure ReadIds(const Key:string;out Values:TIds;Slots:TFPList);
  var Items:TJSONArray;I,N:Integer;Seen:array of Boolean;
  begin
    Items:=TJSONArray(Field(Obj,Key,jtArray));
    if Items.Count>2000000 then raise Exception.Create('Too many document IDs');
    N:=BoundedInteger(High,Key,0,2000000);if Slots.Count>N then N:=Slots.Count;
    SetLength(Seen,N);SetLength(Values,Items.Count);
    for I:=0 to Items.Count-1 do begin
      if (Items[I].JSONType<>jtNumber) or (Items.Floats[I]<1) or
        (Items.Floats[I]>N) or (Frac(Items.Floats[I])<>0) then raise Exception.Create('Invalid document ID');
      Values[I]:=Items.Int64s[I];
      if Seen[Values[I]-1] then raise Exception.Create('Duplicate document ID');
      Seen[Values[I]-1]:=True;
    end;
    Slots.Count:=N;
  end;
begin
  Result:=nil;Data:=nil;Stream:=TFileStream.Create(BindingFile,fmOpenRead or fmShareDenyWrite);
  try
    if (Stream.Size<2) or (Stream.Size>128*1024*1024) then raise Exception.Create('Invalid document binding size');
    SetLength(Text,Stream.Size);Stream.ReadBuffer(Text[1],Length(Text));
  finally Stream.Free end;
  try
    try
    Data:=GetJSON(NormalizeUnicodeEscapes(Text));
    if Data.JSONType<>jtObject then raise Exception.Create('Invalid document bindings');
    Obj:=TJSONObject(Data);
    if (StringField(Obj,'format')<>'FreeShipDocumentBindings') or (IntegerField(Obj,'version')<>1) then raise Exception.Create('Unsupported document bindings');
    High:=TJSONObject(Field(Obj,'high_water',jtObject));
    Result:=TModelState.Create;
    ReadIds('points',Result.PointIds,Candidate.Points);ReadIds('edges',Result.EdgeIds,Candidate.Edges);
    ReadIds('faces',Result.FaceIds,Candidate.Faces);ReadIds('layers',Result.LayerIds,Candidate.Layers);
    ReadIds('curves',Result.CurveIds,Candidate.Curves);ReadIds('flows',Result.FlowIds,Candidate.Flows);
    Result.Data.LoadFromFile(Filename);
    except Result.Free;Result:=nil;raise end;
  finally Data.Free end;
end;

procedure ApplyNativeEdgeOrder(Doc:TDocument;const BindingFile:string);
var Stream:TFileStream;Text:string;Data,Orders:TJSONData;Obj:TJSONObject;
    Points,Values:TJSONArray;I,J,K:Integer;Point:TFreeSubdivisionControlPoint;
    Edge:TFreeSubdivisionEdge;Seen:TFPList;
begin
  Data:=nil;Seen:=TFPList.Create;
  try
    Stream:=TFileStream.Create(BindingFile,fmOpenRead or fmShareDenyWrite);
    try
      if (Stream.Size<2) or (Stream.Size>128*1024*1024) then raise Exception.Create('Invalid native binding file');
      SetLength(Text,Stream.Size);Stream.ReadBuffer(Text[1],Length(Text));
    finally Stream.Free end;
    Data:=GetJSON(Text);Obj:=TJSONObject(Data);Orders:=Obj.Find('point_edges');
    if Orders=nil then Exit;
    Points:=TJSONArray(Field(Obj,'points',jtArray));
    if (Orders.JSONType<>jtArray) or (Orders.Count<>Points.Count) then raise Exception.Create('Invalid native edge order');
    for I:=0 to Points.Count-1 do begin
      Point:=TFreeSubdivisionControlPoint(Doc.Points[Points.Int64s[I]-1]);
      if Orders.Items[I].JSONType<>jtArray then raise Exception.Create('Invalid ordered incident edges');
      Values:=TJSONArray(Orders.Items[I]);Seen.Clear;
      if Values.Count<>Point.NumberOfEdges then raise Exception.Create('Incomplete ordered incident edges');
      for J:=0 to Values.Count-1 do begin
        if (Values.Items[J].JSONType<>jtNumber) or (Values.Floats[J]<1) or
          (Values.Floats[J]>Doc.Edges.Count) or (Frac(Values.Floats[J])<>0) then raise Exception.Create('Invalid ordered edge ID');
        Edge:=TFreeSubdivisionEdge(Doc.Edges[Values.Int64s[J]-1]);
        K:=Point.Edges.IndexOf(Edge);
        if (Edge=nil) or (K<0) or (Seen.IndexOf(Edge)>=0) then raise Exception.Create('Invalid ordered incident edge');
        Seen.Add(Edge);Point.Edges.Exchange(J,K);
      end;
    end;
  finally Seen.Free;Data.Free end;
end;

procedure ApplyNativeDependencyOrder(Doc:TDocument;const BindingFile:string);
var Stream:TFileStream;Text:string;Data:TJSONData;Obj:TJSONObject;Points:TJSONArray;
  procedure Restore(const Key:string;Anchors:Boolean);
  var Orders:TJSONData;Values:TJSONArray;I,J,K:Integer;
      Point,Child:TFreeSubdivisionControlPoint;Items:TFasterListTFreeSubdivisionControlPoint;Seen:TFPList;
  begin
    Orders:=Obj.Find(Key);if Orders=nil then Exit;
    if (Orders.JSONType<>jtArray) or (Orders.Count<>Points.Count) then raise Exception.Create('Invalid dependency order');
    Seen:=TFPList.Create;
    try
      for I:=0 to Points.Count-1 do begin
        Point:=TFreeSubdivisionControlPoint(Doc.Points[Points.Int64s[I]-1]);
        if Anchors then Items:=Point.ServiceAnchoredPoints else Items:=Point.ServiceLinearPoints;
        if Orders.Items[I].JSONType<>jtArray then raise Exception.Create('Invalid dependency row');
        Values:=TJSONArray(Orders.Items[I]);Seen.Clear;
        if Values.Count<>Items.Count then raise Exception.Create('Incomplete dependency order');
        for J:=0 to Values.Count-1 do begin
          if (Values.Items[J].JSONType<>jtNumber) or (Values.Floats[J]<1) or
            (Values.Floats[J]>Doc.Points.Count) or (Frac(Values.Floats[J])<>0) then raise Exception.Create('Invalid dependency point ID');
          Child:=TFreeSubdivisionControlPoint(Doc.Points[Values.Int64s[J]-1]);K:=Items.IndexOf(Child);
          if (Child=nil) or (K<0) or (Seen.IndexOf(Child)>=0) then raise Exception.Create('Invalid ordered dependency');
          Seen.Add(Child);Items.Exchange(J,K);
        end;
      end;
    finally Seen.Free end;
  end;
begin
  Data:=nil;Stream:=TFileStream.Create(BindingFile,fmOpenRead or fmShareDenyWrite);
  try
    if (Stream.Size<2) or (Stream.Size>128*1024*1024) then raise Exception.Create('Invalid dependency binding file');
    SetLength(Text,Stream.Size);Stream.ReadBuffer(Text[1],Length(Text));
  finally Stream.Free end;
  try
    Data:=GetJSON(Text);Obj:=TJSONObject(Data);Points:=TJSONArray(Field(Obj,'points',jtArray));
    Restore('point_anchors',True);Restore('point_lines',False);
  finally Data.Free end;
end;

procedure ApplyNativeLayerOrder(Doc:TDocument;const BindingFile:string);
var Stream:TFileStream;Text:string;Data,Orders:TJSONData;Obj:TJSONObject;
    Layers,Values:TJSONArray;I,J,K,LastId:Integer;Layer:TFreeSubdivisionLayer;
    Face:TFreeSubdivisionControlFace;Seen:TFPList;
begin
  Data:=nil;Seen:=TFPList.Create;
  try
    Stream:=TFileStream.Create(BindingFile,fmOpenRead or fmShareDenyWrite);
    try
      if (Stream.Size<2) or (Stream.Size>128*1024*1024) then raise Exception.Create('Invalid native binding file');
      SetLength(Text,Stream.Size);Stream.ReadBuffer(Text[1],Length(Text));
    finally Stream.Free end;
    Data:=GetJSON(Text);Obj:=TJSONObject(Data);
    if Obj.Find('fbm_encoding')<>nil then
      if StringField(Obj,'fbm_encoding')<>Doc.Ship.Preferences.FbmEncoding then raise Exception.Create('Native FBM encoding mismatch');
    if Obj.Find('last_used_layer_id')<>nil then begin
      LastId:=BoundedInteger(Obj,'last_used_layer_id',-1,High(Integer));
      // LoadProject allocates temporary layers before restoring their saved
      // IDs. That temporary allocation counter is not the document's counter.
      for I:=0 to Doc.Ship.Surface.NumberOfLayers-1 do
        if LastId<Doc.Ship.Surface.Layer[I].LayerID then raise Exception.Create('Invalid native layer counter');
      Doc.Ship.Surface.ServiceLastUsedLayerID:=LastId;
    end;
    Orders:=Obj.Find('layer_faces');if Orders=nil then Exit;
    Layers:=TJSONArray(Field(Obj,'layers',jtArray));
    if (Orders.JSONType<>jtArray) or (Orders.Count<>Layers.Count) then raise Exception.Create('Invalid layer face order');
    for I:=0 to Layers.Count-1 do begin
      Layer:=TFreeSubdivisionLayer(Doc.Layers[Layers.Int64s[I]-1]);
      if Orders.Items[I].JSONType<>jtArray then raise Exception.Create('Invalid ordered layer faces');
      Values:=TJSONArray(Orders.Items[I]);Seen.Clear;
      if Values.Count<>Layer.Count then raise Exception.Create('Incomplete layer face order');
      for J:=0 to Values.Count-1 do begin
        if (Values.Items[J].JSONType<>jtNumber) or (Values.Floats[J]<1) or
          (Values.Floats[J]>Doc.Faces.Count) or (Frac(Values.Floats[J])<>0) then raise Exception.Create('Invalid ordered face ID');
        Face:=TFreeSubdivisionControlFace(Doc.Faces[Values.Int64s[J]-1]);K:=Layer.ServiceFaces.IndexOf(Face);
        if (Face=nil) or (K<0) or (Seen.IndexOf(Face)>=0) then raise Exception.Create('Invalid ordered layer face');
        Seen.Add(Face);Layer.ServiceFaces.Exchange(J,K);
      end;
    end;
  finally Seen.Free;Data.Free end;
end;

procedure TService.ExportDocumentState(Response:TJSONObject;const Output:string);
var State:TModelState;Obj,High,Colors:TJSONObject;Filename,BindingFile:string;
    Order,Incident:TJSONArray;I,J:Integer;Point:TFreeSubdivisionControlPoint;Layer:TFreeSubdivisionLayer;
  procedure Ids(const Key:string;const Values:TIds;Slots:TFPList);
  var A:TJSONArray;Id:Int64;
  begin
    A:=TJSONArray.Create;for Id in Values do A.Add(Id);Obj.Add(Key,A);High.Add(Key,Slots.Count);
  end;
  procedure Color(Value:TColor);
  var Key:string;
  begin
    Key:=IntToStr(Int64(Value));
    if Colors.Find(Key)=nil then Colors.Add(Key,Int64(ColorToRGB(Value)));
  end;
  procedure Dependencies(const Key:string;Anchors:Boolean);
  var Rows,Row:TJSONArray;I,J:Integer;Point:TFreeSubdivisionControlPoint;
      Items:TFasterListTFreeSubdivisionControlPoint;
  begin
    Rows:=TJSONArray.Create;Obj.Add(Key,Rows);
    for I:=0 to Length(State.PointIds)-1 do begin
      Point:=TFreeSubdivisionControlPoint(Doc.Points[State.PointIds[I]-1]);
      if Anchors then Items:=Point.ServiceAnchoredPoints else Items:=Point.ServiceLinearPoints;
      Row:=TJSONArray.Create;Rows.Add(Row);
      for J:=0 to Items.Count-1 do Row.Add(Doc.EntityId(Items[J]));
    end;
  end;
begin
  if Doc=nil then raise Exception.Create('No document to checkpoint');
  State:=TModelState(History[HistoryIndex]);
  Filename:=IncludeTrailingPathDelimiter(Output)+'document.fbm';BindingFile:=IncludeTrailingPathDelimiter(Output)+'document-bindings.json';
  Obj:=TJSONObject.Create(['format','FreeShipDocumentBindings','version',1,'document_id',DocumentId,'revision',Revision]);
  try
    Obj.Add('fbm_encoding',Doc.Ship.Preferences.FbmEncoding);
    Obj.Add('layer_name_prefix',rs_Layer);
    Obj.Add('default_layer_color',Int64(Doc.Ship.Preferences.LayerColor));
    Obj.Add('default_underwater_color',Int64(Doc.Ship.Preferences.UnderWaterColor));
    Colors:=TJSONObject.Create;Obj.Add('resolved_colors',Colors);
    Color(Doc.Ship.Preferences.LayerColor);Color(Doc.Ship.Preferences.UnderWaterColor);
    Color(Doc.Ship.ProjectSettings.ProjectUnderWaterColor);
    Obj.Add('last_used_layer_id',Doc.Ship.Surface.ServiceLastUsedLayerID);
    High:=TJSONObject.Create;Obj.Add('high_water',High);
    Ids('points',State.PointIds,Doc.Points);Ids('edges',State.EdgeIds,Doc.Edges);
    Ids('faces',State.FaceIds,Doc.Faces);Ids('layers',State.LayerIds,Doc.Layers);
    Ids('curves',State.CurveIds,Doc.Curves);Ids('flows',State.FlowIds,Doc.Flows);
    Order:=TJSONArray.Create;Obj.Add('layer_faces',Order);
    for I:=0 to Length(State.LayerIds)-1 do begin
      Layer:=TFreeSubdivisionLayer(Doc.Layers[State.LayerIds[I]-1]);
      Color(Layer.Color);
      Incident:=TJSONArray.Create;Order.Add(Incident);
      for J:=0 to Layer.Count-1 do Incident.Add(Doc.EntityId(Layer.Items[J]));
    end;
    // Live point-edge order affects which stable edge survives a point collapse.
    // FBM itself reconstructs this order on load, so keep it in session bindings.
    Order:=TJSONArray.Create;Obj.Add('point_edges',Order);
    for I:=0 to Length(State.PointIds)-1 do begin
      Point:=TFreeSubdivisionControlPoint(Doc.Points[State.PointIds[I]-1]);
      Incident:=TJSONArray.Create;Order.Add(Incident);
      for J:=0 to Point.NumberOfEdges-1 do Incident.Add(Doc.EntityId(Point.Edge[J]));
    end;
    Dependencies('point_anchors',True);Dependencies('point_lines',False);
    if not State.Data.SaveToFile(Filename) then raise Exception.Create('Cannot write document checkpoint');
    SaveJSON(BindingFile,Obj);Response.Add('state_file',Filename);Response.Add('binding_file',BindingFile);
  finally Obj.Free end;
end;

procedure TService.ReleaseExternalHistory;
var Current:TModelState;WasClean:Boolean;I:Integer;
begin
  if Doc=nil then Exit;
  Current:=TModelState(History[HistoryIndex]);WasClean:=CleanIndex=HistoryIndex;
  for I:=0 to History.Count-1 do if I<>HistoryIndex then TObject(History[I]).Free;
  History.Clear;History.Add(Current);HistoryIndex:=0;
  if WasClean then CleanIndex:=0 else CleanIndex:=-1;
end;
