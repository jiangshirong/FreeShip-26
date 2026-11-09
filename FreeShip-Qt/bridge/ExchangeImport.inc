{ GPL-2.0-or-later. Construct a fresh document from the C++ validated exchange.
  No active document is cleared. Adoption happens only after full snapshot and
  lines-plan generation. Validate again at the service boundary. }
procedure TDocument.ImportExchange(const Filename:string);
var Stream:TFileStream;Data:TJSONData;Root,Obj,Project:TJSONObject;
    Pts,Eds,Fcs,Lys,Arr:TJSONArray;I,J,A,B,L,N:Integer;C:T3DCoordinate;
    ImportedPoints:array of TFreeSubdivisionControlPoint;
    FacePoints:TFasterListTFreeSubdivisionControlPoint;
    Edge:TFreeSubdivisionControlEdge;Layer:TFreeSubdivisionLayer;
    Keys:TStringList;Key:string;Corners:Int64;
  function Index(Item:TJSONData;Maximum:Integer):Integer;
  var V:Double;
  begin
    if Item.JSONType<>jtNumber then raise Exception.Create('Exchange index must be numeric');
    V:=Item.AsFloat;
    if IsNan(V) or IsInfinite(V) or (V<0) or (V>Maximum) or (Frac(V)<>0) then raise Exception.Create('Exchange index out of range');
    Result:=Trunc(V);
  end;
  function ArrayField(const Name:string;MaxCount:Integer):TJSONArray;
  begin
    Result:=TJSONArray(Field(Root,Name,jtArray));
    if (Result.Count=0) or (Result.Count>MaxCount) then raise Exception.Create('Invalid exchange '+Name+' count');
  end;
  function EdgeKey(X,Y:Integer):string;
  var T:Integer;
  begin if X>Y then begin T:=X;X:=Y;Y:=T end;Result:=IntToStr(X)+':'+IntToStr(Y) end;
begin
  Data:=nil;FacePoints:=nil;Keys:=nil;
  Stream:=TFileStream.Create(Filename,fmOpenRead or fmShareDenyWrite);
  try
    if (Stream.Size<2) or (Stream.Size>128*1024*1024) then raise Exception.Create('Exchange input exceeds 128 MiB');
    Data:=GetJSON(Stream);
  finally Stream.Free end;
  try
    if Data.JSONType<>jtObject then raise Exception.Create('Invalid exchange root');Root:=TJSONObject(Data);
    if (StringField(Root,'format')<>'FreeShipExchange') or (IntegerField(Root,'version')<>1) then raise Exception.Create('Unsupported exchange version');
    Pts:=ArrayField('points',100000);Eds:=ArrayField('edges',300000);Fcs:=ArrayField('faces',100000);Lys:=ArrayField('layers',1000);
    Ship.ProjectSettings.SavePreview:=False;
    Project:=TJSONObject(Field(Root,'project',jtObject));ApplyProjectChanges(Ship,Project);
    Corners:=0;
    for I:=0 to Fcs.Count-1 do begin
      if Fcs[I].JSONType<>jtObject then raise Exception.Create('Invalid exchange face');
      Arr:=TJSONArray(Field(TJSONObject(Fcs[I]),'points',jtArray));Inc(Corners,Arr.Count);
      if Corners*2*(Int64(1) shl (2*Ord(Ship.Precision)))>1000000 then raise Exception.Create('Exchange subdivision exceeds one million triangles');
    end;
    // The existing FBM text encoding is respected. Unsupported characters are
    // rejected by TextValue, rather than silently becoming question marks.
    Ship.ProjectSettings.ProjectFileCreatedBy:=TextValue(Ship,Root,'creator');
    Ship.ProjectSettings.ServiceParticularsSet:=Field(Root,'particulars_set',jtBoolean).AsBoolean;
    for I:=0 to Lys.Count-1 do begin
      if Lys[I].JSONType<>jtObject then raise Exception.Create('Invalid exchange layer');
      if I=0 then Layer:=Ship.Surface.Layer[0] else Layer:=Ship.Surface.AddNewLayer;
      ApplyLayerChanges(Ship,Layer,TJSONObject(Lys[I]));
    end;
    SetLength(ImportedPoints,Pts.Count);
    for I:=0 to Pts.Count-1 do begin
      if Pts[I].JSONType<>jtObject then raise Exception.Create('Invalid exchange point');Obj:=TJSONObject(Pts[I]);
      Arr:=TJSONArray(Field(Obj,'p',jtArray));if Arr.Count<>3 then raise Exception.Create('Invalid exchange coordinate');
      for J:=0 to 2 do begin if Arr[J].JSONType<>jtNumber then raise Exception.Create('Invalid exchange coordinate');CheckCoordinate(Arr.Floats[J]) end;
      C.X:=Arr.Floats[0];C.Y:=Arr.Floats[1];C.Z:=Arr.Floats[2];
      // AddControlPoint(Coordinate) can merge close vertices. Preserve the
      // already validated vertex identity, including nearby separate shells.
      ImportedPoints[I]:=TFreeSubdivisionControlPoint.Create(Ship.Surface);Ship.Surface.AddControlPoint(ImportedPoints[I]);ImportedPoints[I].Coordinate:=C;
      BoundedInteger(Obj,'vertex_type',0,3);
    end;
    Keys:=TStringList.Create;Keys.Sorted:=True;Keys.Duplicates:=dupError;
    for I:=0 to Eds.Count-1 do begin
      if Eds[I].JSONType<>jtArray then raise Exception.Create('Invalid exchange edge');Arr:=TJSONArray(Eds[I]);
      if Arr.Count<>3 then raise Exception.Create('Invalid exchange edge');A:=Index(Arr[0],High(ImportedPoints));B:=Index(Arr[1],High(ImportedPoints));
      if (A=B) or (Arr[2].JSONType<>jtBoolean) then raise Exception.Create('Invalid exchange edge');
      Key:=EdgeKey(A,B);if Keys.IndexOf(Key)>=0 then raise Exception.Create('Duplicate exchange edge');Keys.Add(Key);
      Edge:=Ship.Surface.AddControlEdge(ImportedPoints[A],ImportedPoints[B]);if Edge=nil then raise Exception.Create('Cannot create exchange edge');
    end;
    FacePoints:=TFasterListTFreeSubdivisionControlPoint.Create;
    for I:=0 to Fcs.Count-1 do begin
      if Fcs[I].JSONType<>jtObject then raise Exception.Create('Invalid exchange face');Obj:=TJSONObject(Fcs[I]);Arr:=TJSONArray(Field(Obj,'points',jtArray));
      if (Arr.Count<3) or (Arr.Count>4096) then raise Exception.Create('Invalid exchange face size');L:=BoundedInteger(Obj,'layer',0,Lys.Count-1);
      FacePoints.Clear;
      for J:=0 to Arr.Count-1 do begin A:=Index(Arr[J],High(ImportedPoints));if FacePoints.IndexOf(ImportedPoints[A])>=0 then raise Exception.Create('Repeated exchange face vertex');FacePoints.Add(ImportedPoints[A]);
        B:=Index(Arr[(J+1) mod Arr.Count],High(ImportedPoints));N:=Keys.IndexOf(EdgeKey(A,B));if N<0 then raise Exception.Create('Missing exchange face edge');
        if PtrInt(Keys.Objects[N])>=2 then raise Exception.Create('Non-manifold exchange edge');Keys.Objects[N]:=TObject(PtrInt(Keys.Objects[N])+1);
      end;
      if Ship.Surface.AddControlFace(FacePoints,False,Ship.Surface.Layer[L])=nil then raise Exception.Create('Cannot create exchange face');
    end;
    // Initialize derives boundary defaults, including corner types. Restore
    // the file's explicit crease/vertex attributes afterwards; otherwise valid
    // crease vertices in real hulls become fixed corners and change the surface.
    Ship.Surface.Initialize(1,1,1);
    for I:=0 to Eds.Count-1 do begin Arr:=TJSONArray(Eds[I]);Edge:=Ship.Surface.EdgeExists(ImportedPoints[Index(Arr[0],High(ImportedPoints))],ImportedPoints[Index(Arr[1],High(ImportedPoints))]) as TFreeSubdivisionControlEdge;Edge.Crease:=Arr.Booleans[2] end;
    for I:=0 to Pts.Count-1 do ImportedPoints[I].VertexType:=TFreeVertexType(BoundedInteger(TJSONObject(Pts[I]),'vertex_type',0,3));
    Ship.Built:=False;Ship.ModelIsLoaded:=True;Ship.FileChanged:=True;Ship.RebuildModel;
  finally Keys.Free;FacePoints.Free;Data.Free end;
end;
