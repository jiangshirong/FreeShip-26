{ FreeShip_26 section and layer management. GPL-2.0-or-later. }
function TDocument.SectionCount(Kind:Integer):Integer;
begin
  case Kind of
    1:Result:=Ship.NumberOfStations;2:Result:=Ship.NumberOfButtocks;
    3:Result:=Ship.NumberOfWaterlines;4:Result:=Ship.NumberOfDiagonals;
    else raise Exception.Create('Invalid section kind');
  end;
end;
function TDocument.SectionAt(Kind,Index:Integer):TFreeIntersection;
begin
  if (Index<0) or (Index>=SectionCount(Kind)) then raise Exception.Create('Unknown section in this revision');
  case Kind of
    1:Result:=Ship.Station[Index];2:Result:=Ship.Buttock[Index];
    3:Result:=Ship.Waterline[Index];4:Result:=Ship.Diagonal[Index];
  end;
end;
procedure TDocument.WriteSections(Root:TJSONObject);
var List:TJSONArray;Obj:TJSONObject;Kind,I:Integer;Int:TFreeIntersection;Plane:T3DPlane;
begin
  List:=TJSONArray.Create;Root.Add('sections',List);
  for Kind:=1 to 4 do for I:=0 to SectionCount(Kind)-1 do begin
    Int:=SectionAt(Kind,I);Plane:=Int.Plane;
    CheckCoordinate(Plane.a);CheckCoordinate(Plane.b);CheckCoordinate(Plane.c);CheckCoordinate(Plane.d);
    CheckCoordinate(Int.Distance);
    Obj:=TJSONObject.Create;List.Add(Obj);Obj.Add('kind',Kind);Obj.Add('index',I);
    Obj.Add('distance',Double(Int.Distance));Obj.Add('show_curvature',Int.ShowCurvature);
    Obj.Add('color',Int64(ColorToRGB(Int.Color)));
    Obj.Add('plane',TJSONArray.Create([Double(Plane.a),Double(Plane.b),Double(Plane.c),Double(Plane.d)]));
  end;
end;
function TDocument.EditSections(Request:TJSONObject):Boolean;
var Operation:string;Kind,I,J,Index:Integer;Wide:Int64;Values:TJSONArray;Value:Double;
    Positions:array of TFloatType;Selected:array of Boolean;Int:TFreeIntersection;
    Plane:T3DPlane;Distance:TFloatType;Flag:Boolean;List:TFasterListTFreeIntersection;
  procedure SortSections(Left,Right:Integer);
  var A,B:Integer;Pivot:TFloatType;
  begin
    A:=Left;B:=Right;Pivot:=-List[(Left+Right) div 2].Plane.d;
    repeat
      while -List[A].Plane.d<Pivot do Inc(A);
      while -List[B].Plane.d>Pivot do Dec(B);
      if A<=B then begin List.Exchange(A,B);Inc(A);Dec(B) end;
    until A>B;
    if Left<B then SortSections(Left,B);
    if A<Right then SortSections(A,Right);
  end;
  function ReadDistance(Data:TJSONData):TFloatType;
  begin
    if (Data=nil) or (Data.JSONType<>jtNumber) then raise Exception.Create('Invalid section distance');
    Value:=Data.AsFloat;CheckCoordinate(Value);Result:=Value;
  end;
  function Exists(Position:TFloatType;ExceptIndex:Integer=-1):Boolean;
  var At:Integer;
  begin
    Result:=False;
    for At:=0 to SectionCount(Kind)-1 do
      if (At<>ExceptIndex) and (Abs(Double(SectionAt(Kind,At).Distance)-Position)<1e-7) then Exit(True);
  end;
begin
  Operation:=StringField(Request,'operation');Wide:=IntegerField(Request,'kind',1);
  if Wide>4 then raise Exception.Create('Invalid section kind');Kind:=Wide;
  List:=Ship.ServiceSectionList(Kind);Result:=False;
  if Operation='add' then begin
    Values:=TJSONArray(Field(Request,'positions',jtArray));
    if (Values.Count=0) or (Values.Count>512) then raise Exception.Create('Add between 1 and 512 sections at a time');
    if SectionCount(1)+SectionCount(2)+SectionCount(3)+SectionCount(4)+Values.Count>4096 then
      raise Exception.Create('Section count limit exceeded');
    SetLength(Positions,Values.Count);
    for I:=0 to Values.Count-1 do Positions[I]:=ReadDistance(Values[I]);
    // Definitions outside the current hull remain useful after future edits.
    // The curve can be empty; the definition must still save and undo exactly.
    for I:=0 to High(Positions) do if not Exists(Positions[I]) then begin
      Int:=TFreeIntersection.Create(Ship);
      try
        Int.IntersectionType:=TFreeIntersectionType(Kind);
        FillChar(Plane,SizeOf(Plane),0);
        case Kind of
          1:Plane.a:=1;2:Plane.b:=1;3:Plane.c:=1;
          4:begin Plane.b:=1/Sqrt(2);Plane.c:=Plane.b end;
        end;
        Int.Plane:=Plane;Int.Distance:=Positions[I];
        // No legacy form, redraw or separate legacy undo object is invoked.
        List.Add(Int);Int:=nil;
        Result:=True;
      finally Int.Free end;
    end;
    if Result and (Kind=4) then RequireVersion(Ship,fv180);
  end else if Operation='update' then begin
    Wide:=IntegerField(Request,'index',0);
    if Wide>=SectionCount(Kind) then raise Exception.Create('Unknown section in this revision');
    Index:=Wide;Int:=SectionAt(Kind,Index);
    if Request.Find('distance')<>nil then begin
      Distance:=ReadDistance(Request.Find('distance'));
      if Exists(Distance,Index) then raise Exception.Create('A section already exists at that distance');
      if Int.Distance<>Distance then begin Int.Distance:=Distance;Int.Built:=False;Result:=True end;
    end;
    if Request.Find('show_curvature')<>nil then begin
      Flag:=Field(Request,'show_curvature',jtBoolean).AsBoolean;
      if Flag<>Int.ShowCurvature then begin
        Int.ShowCurvature:=Flag;Result:=True;
        if Flag then RequireVersion(Ship,fv191);
      end;
    end;
  end else if (Operation='delete') or (Operation='clear') then begin
    SetLength(Selected,SectionCount(Kind));
    if Operation='clear' then for I:=0 to High(Selected) do Selected[I]:=True
    else begin
      Values:=TJSONArray(Field(Request,'indices',jtArray));
      if (Values.Count=0) or (Values.Count>Length(Selected)) then raise Exception.Create('Invalid section selection');
      for I:=0 to Values.Count-1 do begin
        if Values[I].JSONType<>jtNumber then raise Exception.Create('Invalid section index');
        Value:=Values[I].AsFloat;CheckCoordinate(Value);
        if (Value<0) or (Value>=Length(Selected)) or (Frac(Value)<>0) then raise Exception.Create('Unknown section index');
        J:=Trunc(Value);if Selected[J] then raise Exception.Create('Duplicate section index');Selected[J]:=True;
      end;
    end;
    for I:=High(Selected) downto 0 do if Selected[I] then begin SectionAt(Kind,I).Delete(False);Result:=True end;
  end else raise Exception.Create('Unsupported section operation');
  if Result and (List.Count>1) then SortSections(0,List.Count-1);
end;
function TDocument.ManageLayers(Request:TJSONObject):Boolean;
var Operation:string;Id:Int64;I:Integer;L:TFreeSubdivisionLayer;Patch:TJSONObject;
begin
  Operation:=StringField(Request,'operation');Result:=False;
  if Operation='create' then begin
    Patch:=TJSONObject(Field(Request,'changes',jtObject));
    L:=Ship.Surface.AddNewLayer;L.Color:=Ship.Preferences.LayerColor;
    ApplyLayerChanges(Ship,L,Patch);Result:=True;
  end else if Operation='delete' then begin
    Id:=IntegerField(Request,'layer_id',1);
    if (Id>Layers.Count) or (Layers[Id-1]=nil) then raise Exception.Create('Unknown layer ID');
    L:=TFreeSubdivisionLayer(Layers[Id-1]);
    if L.Count<>0 then raise Exception.Create('Only empty layers can be deleted; move their faces first');
    if Ship.Surface.NumberOfLayers<=1 then raise Exception.Create('Keep at least one layer');
    L.Delete;Result:=True;
  end else if Operation='delete_empty' then begin
    for I:=Ship.Surface.NumberOfLayers-1 downto 0 do
      if (Ship.Surface.Layer[I].Count=0) and (Ship.Surface.NumberOfLayers>1) then begin
        Ship.Surface.Layer[I].Delete;Result:=True;
      end;
  end else raise Exception.Create('Unsupported layer management operation');
  if Result then begin
    if Ship.Surface.ActiveLayer=nil then Ship.Surface.ActiveLayer:=Ship.Surface.Layer[0];
    ReindexTopology;
  end;
end;
