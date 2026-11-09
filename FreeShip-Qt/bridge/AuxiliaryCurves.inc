{ Control-curve definitions and original surface flow traces. GPL-2.0-or-later.
  No legacy window, editor selection or nested undo object is used. }
procedure TDocument.WriteAuxiliary(Root:TJSONObject);
var List,Controls,Flags,Ids:TJSONArray;Item,SplineData:TJSONObject;I,J,Count:Integer;
    C:TFreeSubdivisionControlCurve;F:TFreeFlowline;S:TFreeSpline;P:T2DCoordinate;
  procedure WriteSpline;
  var K:Integer;
  begin
    Inc(Count,S.NumberOfPoints);if Count>1000000 then raise Exception.Create('Auxiliary spline point limit exceeded');
    SplineData:=TJSONObject.Create;Item.Add('spline',SplineData);
    Controls:=TJSONArray.Create;Flags:=TJSONArray.Create;SplineData.Add('points',Controls);SplineData.Add('knuckles',Flags);
    for K:=0 to S.NumberOfPoints-1 do begin Controls.Add(Coordinates(S.Point[K]));Flags.Add(S.Knuckle[K]) end;
  end;
begin
  if (Ship.Surface.NumberOfControlCurves>4096) or (Ship.NumberOfFlowLines>4096) then raise Exception.Create('Auxiliary curve count limit exceeded');
  Count:=0;List:=TJSONArray.Create;Root.Add('control_curves',List);
  for I:=0 to Ship.Surface.NumberOfControlCurves-1 do begin
    C:=Ship.Surface.ControlCurve[I];if (NativeDisplay=nil) and not C.Built then C.Rebuild;
    Item:=TJSONObject.Create;List.Add(Item);Item.Add('id',EntityId(C));Item.Add('name',C.Name);
    Item.Add('color',Int64(ColorToRGB(Ship.Preferences.ControlCurveColor)));Ids:=TJSONArray.Create;Item.Add('controls',Ids);
    for J:=0 to C.NumberOfControlPoints-1 do Ids.Add(EntityId(C.ControlPoint[J]));
    if NativeDisplay=nil then begin S:=C.Spline;WriteSpline end
    else begin
      if NativeDisplay.Arrays['control_curves'].Count<>Ship.Surface.NumberOfControlCurves then
        raise Exception.Create('Native control curve count mismatch');
      SplineData:=NativeDisplay.Arrays['control_curves'].Objects[I];
      if IntegerField(SplineData,'id')<>EntityId(C) then raise Exception.Create('Native control curve identity mismatch');
      SplineData:=TJSONObject(Field(SplineData,'spline',jtObject));
      Inc(Count,TJSONArray(Field(SplineData,'points',jtArray)).Count);
      if Count>1000000 then raise Exception.Create('Auxiliary spline point limit exceeded');
      Item.Add('spline',SplineData.Clone);
    end;
  end;
  List:=TJSONArray.Create;Root.Add('flowlines',List);
  for I:=0 to Ship.NumberOfFlowLines-1 do begin
    F:=Ship.Flowline[I];if (NativeDisplay=nil) and not F.Build then F.Rebuild;
    Item:=TJSONObject.Create;List.Add(Item);Item.Add('id',EntityId(F));Item.Add('name',F.Name);
    // Selection is a Qt view state, not an FBM mutation or a stored highlight.
    Item.Add('color',Int64(ColorToRGB(clBlue)));Item.Add('view',Ord(F.ServiceView));
    P:=F.ServiceSource;CheckCoordinate(P.X);CheckCoordinate(P.Y);Item.Add('source',TJSONArray.Create([Double(P.X),Double(P.Y)]));
    if NativeDisplay=nil then begin S:=F.ServiceSpline;WriteSpline end
    else begin
      if NativeDisplay.Arrays['flowlines'].Count<>Ship.NumberOfFlowLines then raise Exception.Create('Native flowline count mismatch');
      SplineData:=NativeDisplay.Arrays['flowlines'].Objects[I];
      if IntegerField(SplineData,'id')<>EntityId(F) then raise Exception.Create('Native flowline identity mismatch');
      SplineData:=TJSONObject(Field(SplineData,'spline',jtObject));
      Inc(Count,TJSONArray(Field(SplineData,'points',jtArray)).Count);
      if Count>1000000 then raise Exception.Create('Auxiliary spline point limit exceeded');
      Item.Add('spline',SplineData.Clone);
    end;
  end;
end;

function TDocument.EditAuxiliary(Request:TJSONObject):Boolean;
var Operation,Kind:string;Values:TJSONArray;Slots:TFPList;Ids:TIds;
    Seen,Used:array of Boolean;Adjacency:array of array of Integer;Selected:array of TFreeSubdivisionControlEdge;
    I,J,N,Index:Integer;Id,PointId:Int64;E:TFreeSubdivisionControlEdge;
    C:TFreeSubdivisionControlCurve;F:TFreeFlowline;P:T2DCoordinate;Wide:Int64;
  function ReadSelection(List:TFPList):TIds;
  var K:Integer;Value:Double;Flags:array of Boolean;
  begin
    Values:=TJSONArray(Field(Request,'ids',jtArray));
    if (Values.Count<1) or (Values.Count>100000) then raise Exception.Create('Invalid auxiliary selection size');
    SetLength(Result,Values.Count);SetLength(Flags,List.Count);
    for K:=0 to Values.Count-1 do begin
      if Values[K].JSONType<>jtNumber then raise Exception.Create('Invalid auxiliary entity ID');
      Value:=Values[K].AsFloat;CheckCoordinate(Value);
      if (Value<1) or (Value>List.Count) or (Frac(Value)<>0) then raise Exception.Create('Unknown auxiliary entity ID');
      Result[K]:=Trunc(Value);
      if (List[Result[K]-1]=nil) or Flags[Result[K]-1] then raise Exception.Create('Deleted or duplicate auxiliary entity ID');
      Flags[Result[K]-1]:=True;
    end;
  end;
  procedure Attach(Point:TFreeSubdivisionPoint;EdgeIndex:Integer);
  var K,N:Integer;
  begin
    K:=EntityId(Point)-1;N:=Length(Adjacency[K]);SetLength(Adjacency[K],N+1);Adjacency[K][N]:=EdgeIndex;
  end;
  procedure Trace(Start,FirstEdge:Integer);
  var K,Next,J:Integer;Point:TFreeSubdivisionControlPoint;Curve:TFreeSubdivisionControlCurve;Edge:TFreeSubdivisionControlEdge;
  begin
    if Used[FirstEdge] then Exit;
    if Ship.Surface.NumberOfControlCurves>=4096 then raise Exception.Create('Control curve count limit exceeded');
    Curve:=TFreeSubdivisionControlCurve.Create(Ship.Surface);Ship.Surface.AddControlCurve(Curve);
    Curve.ControlPoints.IsUnique:=False;Point:=TFreeSubdivisionControlPoint(Points[Start]);Curve.AddPoint(Point);Next:=FirstEdge;
    repeat
      Used[Next]:=True;Edge:=Selected[Next];Edge.Curve:=Curve;
      if Edge.StartPoint=Point then Point:=TFreeSubdivisionControlPoint(Edge.EndPoint) else Point:=TFreeSubdivisionControlPoint(Edge.StartPoint);
      Curve.AddPoint(Point);K:=EntityId(Point)-1;
      if (K=Start) or (Length(Adjacency[K])<>2) then Break;
      Next:=-1;for J:=0 to High(Adjacency[K]) do if not Used[Adjacency[K][J]] then Next:=Adjacency[K][J];
    until Next<0;
  end;

begin
  Operation:=StringField(Request,'operation');Result:=False;
  if Operation='create_control' then begin
    Ids:=ReadSelection(Edges);SetLength(Selected,Length(Ids));N:=0;
    for I:=0 to High(Ids) do begin E:=TFreeSubdivisionControlEdge(Edges[Ids[I]-1]);if E.Curve=nil then begin Selected[N]:=E;Inc(N) end end;
    if N=0 then Exit;SetLength(Selected,N);SetLength(Used,N);SetLength(Adjacency,Points.Count);
    for I:=0 to N-1 do begin Attach(Selected[I].StartPoint,I);Attach(Selected[I].EndPoint,I) end;
    RequireVersion(Ship,fv195);
    for I:=0 to Points.Count-1 do if Length(Adjacency[I])<>2 then
      for J:=0 to High(Adjacency[I]) do if not Used[Adjacency[I][J]] then Trace(I,Adjacency[I][J]);
    for I:=0 to N-1 do if not Used[I] then Trace(EntityId(Selected[I].StartPoint)-1,I);
    for I:=0 to N-1 do if not Used[I] then raise Exception.Create('Selected edge was not assigned to a curve');
    Ship.Visibility.ShowControlCurves:=True;Ship.Built:=False;Ship.RebuildModel;Result:=True;
  end else if Operation='rename_control' then begin
    Ids:=ReadSelection(Curves);if Length(Ids)<>1 then raise Exception.Create('Select one control curve');
    Kind:=TextValue(Ship,Request,'name');
    if (Length(Kind)>4096) or (Pos(#0,Kind)>0) then raise Exception.Create('Invalid curve name');
    C:=TFreeSubdivisionControlCurve(Curves[Ids[0]-1]);
    if C.Name<>Kind then begin C.Name:=Kind;RequireVersion(Ship,fv421);Result:=True end;
  end else if Operation='create_flow' then begin
    if Ship.NumberOfFlowLines>=4096 then raise Exception.Create('Flowline count limit exceeded');
    Wide:=IntegerField(Request,'view',0);if Wide>2 then raise Exception.Create('Flowlines require an orthographic view');
    Values:=TJSONArray(Field(Request,'source',jtArray));if Values.Count<>2 then raise Exception.Create('Flow source requires two coordinates');
    for I:=0 to 1 do begin if Values[I].JSONType<>jtNumber then raise Exception.Create('Invalid flow source');CheckCoordinate(Values[I].AsFloat) end;
    P.X:=Values[0].AsFloat;P.Y:=Values[1].AsFloat;
    F:=TFreeFlowline.Create(Ship);
    try
      F.ServiceSource:=P;F.ServiceView:=TFreeViewType(Wide);F.Rebuild;
      if F.ServiceSpline.NumberOfPoints<2 then raise Exception.Create('The selected position does not produce a flowline on the submerged hull');
      RequireVersion(Ship,fv250);Ship.ServiceFlowlines.Add(F);F:=nil;
    finally F.Free end;
    Ship.Visibility.ShowFlowlines:=True;Result:=True;
  end else if Operation='delete' then begin
    Kind:=StringField(Request,'kind');if Kind='curve' then Slots:=Curves else if Kind='flow' then Slots:=Flows else raise Exception.Create('Invalid auxiliary kind');
    Ids:=ReadSelection(Slots);
    for I:=0 to High(Ids) do if Kind='curve' then TFreeSubdivisionControlCurve(Slots[Ids[I]-1]).Delete else TFreeFlowline(Slots[Ids[I]-1]).Delete;
    // Definitions are removed without deleting their control edges or faces.
    if Kind='curve' then begin Ship.Built:=False;Ship.RebuildModel end;
    Result:=True;
  end else raise Exception.Create('Unsupported auxiliary operation');
  if Result then ReindexTopology;
end;
