{ FreeShip Qt migration adapter. GPL-2.0-or-later; see COPYING. }
unit ModelService;
{$mode objfpc}{$H+}
interface
procedure RunModelService;
implementation
uses Classes,SysUtils,Math,Forms,Graphics,Windows,fpjson,jsonparser,FGL,TypInfo,base64,
  FreeShipUnit,FreeFileBuffer,FreeGeometry,FreeTypes,FreeLinesplanScene,
  FreeSafeLanguage,ModelEdits,NativeLinesplanInput,FreeVersionUnit,FreeStringsUnit,
  FreeMatrices,LackenbyEngine,HydrostaticsEngine,PerformanceEngine,PlateEngine,CadExportEngine;

var NativeLinesplanEnabled:Boolean=False;
    NativeSurfaceEnabled:Boolean=False;

const MaxJSONInteger=9007199254740991;
      MaxCoordinate=1.0e9;
      MaxRequestBytes=1048576;
      MoveFileWriteThrough=$8;
type
  TIds=array of Int64;
  TEntityIdMap=specialize TFPGMap<PtrUInt,Int64>;
  TModelState=class
    Data:TFreeFileBuffer;
    PointIds,EdgeIds,FaceIds,LayerIds,CurveIds,FlowIds:TIds;
    constructor Create;
    destructor Destroy;override;
    function MemoryBytes:Int64;
  end;
  TDocument=class
    Ship:TFreeShip;
    OverlayCalc:TFreeHydrostaticCalc;
    Points,Edges,Faces,Layers,Curves,Flows:TFPList;
    EntityIds:TEntityIdMap;
    EntityBirths:TEntityIdMap;
    BatchAllowed:array of Boolean;
    NativeDisplay:TJSONObject; // Owned only during staged Export.
    constructor Create(Template:TDocument=nil);
    destructor Destroy;override;
    procedure BindIds(State:TModelState);
    procedure ReindexTopology;
    function TopologyEdit(Request:TJSONObject):Boolean;
    function AdvancedEdit(Request:TJSONObject):Boolean;
    function ModelingEdit(Request:TJSONObject):Boolean;
    function ModelToolsEdit(Request:TJSONObject):Boolean;
    procedure WriteModelingData(Root:TJSONObject);
    procedure NewHull(Request:TJSONObject);
    procedure ImportExchange(const Filename:string);
    function InspectModeling:TJSONObject;
    function HydroDefaults:TJSONObject;
    function HydroSettings(Request:TJSONObject):Boolean;
    function ReadEntityIds(Request:TJSONObject;Slots:TFPList;const Key:string='ids'):TIds;
    function ResolveEntity(Slots:TFPList;Id:Int64):Pointer;
    procedure RequireEditable(Point:TFreeSubdivisionPoint);
    procedure RequireEditableFace(Face:TFreeSubdivisionFace);
    procedure DeletePointWithCurves(Point:TFreeSubdivisionControlPoint);
    function SectionCount(Kind:Integer):Integer;
    function SectionAt(Kind,Index:Integer):TFreeIntersection;
    function EditSections(Request:TJSONObject):Boolean;
    function EditDisplay(Request:TJSONObject):Boolean;
    function EditAuxiliary(Request:TJSONObject):Boolean;
    procedure WriteAuxiliary(Root:TJSONObject);
    function ManageLayers(Request:TJSONObject):Boolean;
    procedure WriteSections(Root:TJSONObject);
    procedure WriteOverlay(Root:TJSONObject);
    procedure WriteBackgrounds(Root:TJSONObject);
    function EditBackground(Request:TJSONObject):Boolean;
    procedure InvalidateAnalysis;
    function EntityId(Item:Pointer):Int64;
    function BatchMove(Request:TJSONObject):Boolean;
    procedure ApplyNativeCoordinates(const Filename:string);
    function Capture:TModelState;
    procedure LoadState(State:TModelState);
    procedure WriteModel(const Filename,DocumentId:string;Revision:Int64);
    procedure Export(const Output,DocumentId:string;Revision:Int64);
  end;
  TService=class
    Doc:TDocument;
    History:TFPList;
    HistoryIndex,CleanIndex:Integer;
    Revision:Int64;
    DocumentId:string;
    constructor Create;
    destructor Destroy;override;
    procedure ClearHistory;
    function HistoryBytes:Int64;
    function HistoryLimit:Int64;
    procedure TrimHistory;
    procedure RequireDocument(Request:TJSONObject);
    procedure AddState(Response:TJSONObject);
    procedure AddArtifacts(Response:TJSONObject;const Output:string);
    procedure ExportDocumentState(Response:TJSONObject;const Output:string);
    procedure ReleaseExternalHistory;
    function Execute(Request:TJSONObject;out Quit:Boolean):TJSONObject;
  end;

function Field(Obj:TJSONObject;const Name:string;Kind:TJSONType):TJSONData;
begin
  Result:=Obj.Find(Name);
  if (Result=nil) or (Result.JSONType<>Kind) then
    raise Exception.Create('Invalid or missing '+Name);
end;
function IntegerField(Obj:TJSONObject;const Name:string;Minimum:Int64=0):Int64;
var D:Double;
begin
  D:=Field(Obj,Name,jtNumber).AsFloat;
  if IsNan(D) or IsInfinite(D) or (D<Minimum) or (D>MaxJSONInteger) or (Frac(D)<>0) then
    raise Exception.Create('Invalid integer '+Name);
  Result:=Trunc(D);
end;
function StringField(Obj:TJSONObject;const Name:string):string;
begin
  Result:=Field(Obj,Name,jtString).AsString;
  if (Result='') or (Pos(#0,Result)>0) then raise Exception.Create('Invalid '+Name);
end;
function BoundedInteger(Obj:TJSONObject;const Name:string;Minimum,Maximum:Integer):Integer;
var N:Int64;
begin
  N:=IntegerField(Obj,Name,Minimum);
  if N>Maximum then raise Exception.Create('Integer out of range: '+Name);
  Result:=N;
end;
function NormalizeUnicodeEscapes(const Line:UTF8String):UTF8String;
// FPC 3.2.2 jsonscanner combines adjacent \u escapes in String[4], which
// truncates two non-ASCII BMP characters (up to six UTF-8 bytes). Decode only
// these escapes before parsing. Escaped backslashes and JSON syntax stay intact.
var I,J,N,U,V,K:Integer;InString:Boolean;
  function Hex4(Start:Integer):Integer;
  var H,T:Integer;
  begin
    if Start+3>Length(Line) then raise Exception.Create('Incomplete Unicode escape');
    Result:=0;
    for H:=Start to Start+3 do begin
      case Line[H] of
        '0'..'9':T:=Ord(Line[H])-Ord('0');
        'a'..'f':T:=Ord(Line[H])-Ord('a')+10;
        'A'..'F':T:=Ord(Line[H])-Ord('A')+10;
        else raise Exception.Create('Invalid Unicode escape');
      end;
      Result:=Result*16+T;
    end;
  end;
  procedure ByteOut(B:Integer);
  begin Inc(N);Result[N]:=Chr(B) end;
begin
  SetLength(Result,Length(Line));I:=1;N:=0;InString:=False;
  while I<=Length(Line) do begin
    if (Line[I]='\') and InString then begin
      if (I<Length(Line)) and (Line[I+1]='u') then begin
        U:=Hex4(I+2);J:=I+6;
        if (U>=$D800) and (U<=$DBFF) then begin
          if (J+5>Length(Line)) or (Line[J]<>'\') or (Line[J+1]<>'u') then
            raise Exception.Create('Missing Unicode low surrogate');
          V:=Hex4(J+2);
          if (V<$DC00) or (V>$DFFF) then raise Exception.Create('Invalid Unicode low surrogate');
          U:=$10000+(U-$D800)*$400+V-$DC00;Inc(J,6);
        end else if (U>=$DC00) and (U<=$DFFF) then raise Exception.Create('Unpaired Unicode low surrogate');
        if (U<32) or (U=Ord('"')) or (U=Ord('\')) then
          for K:=I to J-1 do ByteOut(Ord(Line[K]))
        else if U<$80 then ByteOut(U)
        else if U<$800 then begin ByteOut($C0 or (U shr 6));ByteOut($80 or (U and $3F)) end
        else if U<$10000 then begin ByteOut($E0 or (U shr 12));ByteOut($80 or ((U shr 6) and $3F));ByteOut($80 or (U and $3F)) end
        else begin ByteOut($F0 or (U shr 18));ByteOut($80 or ((U shr 12) and $3F));ByteOut($80 or ((U shr 6) and $3F));ByteOut($80 or (U and $3F)) end;
        I:=J;Continue;
      end;
      ByteOut(Ord(Line[I]));Inc(I);
      if I<=Length(Line) then begin ByteOut(Ord(Line[I]));Inc(I) end;
      Continue;
    end;
    if Line[I]='"' then InString:=not InString;
    ByteOut(Ord(Line[I]));Inc(I);
  end;
  SetLength(Result,N);
end;
function OutputDirectory(Request:TJSONObject):string;
begin
  Result:=ExpandFileName(StringField(Request,'output'));
  if not DirectoryExists(Result) and not ForceDirectories(Result) then
    raise Exception.Create('Cannot create output directory');
end;
procedure CheckCoordinate(Value:Double);
begin
  if IsNan(Value) or IsInfinite(Value) or (Abs(Value)>MaxCoordinate) then
    raise Exception.Create('Coordinate is not finite or exceeds 1e9 document units');
end;
function Coordinates(const P:T3DCoordinate):TJSONArray;
begin
  CheckCoordinate(P.X);CheckCoordinate(P.Y);CheckCoordinate(P.Z);
  Result:=TJSONArray.Create;
  Result.Add(Double(P.X));Result.Add(Double(P.Y));Result.Add(Double(P.Z));
end;
function ReadPosition(Obj:TJSONObject):T3DCoordinate;
var Position:TJSONArray;I:Integer;
begin
  Position:=TJSONArray(Field(Obj,'position',jtArray));
  if Position.Count<>3 then raise Exception.Create('position must have three coordinates');
  for I:=0 to 2 do begin
    if Position.Items[I].JSONType<>jtNumber then raise Exception.Create('position must be numeric');
    CheckCoordinate(Position.Floats[I]);
  end;
  Result.X:=Position.Floats[0];Result.Y:=Position.Floats[1];Result.Z:=Position.Floats[2];
end;
procedure SaveJSON(const Filename:string;Data:TJSONData);
var Text:UTF8String;Stream:TFileStream;
begin
  Text:=Data.AsJSON;
  if Length(Text)>256*1024*1024 then raise Exception.Create('Model snapshot exceeds 256 MiB');
  Stream:=TFileStream.Create(Filename,fmCreate);
  try if Length(Text)>0 then Stream.WriteBuffer(Text[1],Length(Text)) finally Stream.Free end;
end;
procedure PublishFile(const Source,Destination:string);
begin
  if not MoveFileExW(PWideChar(UTF8Decode(Source)),PWideChar(UTF8Decode(Destination)),
       MOVEFILE_REPLACE_EXISTING or MoveFileWriteThrough) then
    RaiseLastOSError;
end;
constructor TModelState.Create;
begin inherited Create;Data:=TFreeFileBuffer.Create end;
destructor TModelState.Destroy;
begin Data.Free;inherited Destroy end;
function TModelState.MemoryBytes:Int64;
begin
  Result:=Int64(Data.Capacity)+Int64(SizeOf(Int64))*(Length(PointIds)+Length(EdgeIds)+Length(FaceIds)+Length(LayerIds)+Length(CurveIds)+Length(FlowIds));
end;
constructor TDocument.Create(Template:TDocument);
var Properties:PPropList;Count,I:Integer;
begin
  inherited Create;
  Points:=TFPList.Create;Edges:=TFPList.Create;Faces:=TFPList.Create;Layers:=TFPList.Create;Curves:=TFPList.Create;Flows:=TFPList.Create;
  EntityIds:=TEntityIdMap.Create;EntityBirths:=TEntityIdMap.Create;
  Ship:=TFreeShip.Create(nil);Ship.IsRebuildAsyncDisabled:=True;
  if Template=nil then Ship.Preferences.Load else begin
    // Keep the allocation high-water marks across undo and rollback. Deleted
    // identifiers are never reassigned to a different object in this document.
    Points.Count:=Template.Points.Count;Edges.Count:=Template.Edges.Count;
    Faces.Count:=Template.Faces.Count;Layers.Count:=Template.Layers.Count;
    Curves.Count:=Template.Curves.Count;Flows.Count:=Template.Flows.Count;
    // Undo and rollback use the live document's settings. Re-reading external
    // INI/theme files here could itself fail after an edit has already failed.
    Properties:=nil;Count:=GetPropList(Template.Ship.Preferences,Properties);
    try
      for I:=0 to Count-1 do if Properties^[I]^.SetProc<>nil then
        SetPropValue(Ship.Preferences,Properties^[I]^.Name,
          GetPropValue(Template.Ship.Preferences,Properties^[I]^.Name,False));
    finally FreeMem(Properties) end;
  end;
end;
destructor TDocument.Destroy;
begin
  if GlobalFreeShip=Ship then GlobalFreeShip:=nil;
  OverlayCalc.Free;
  Ship.Free;Points.Free;Edges.Free;Faces.Free;Layers.Free;Curves.Free;Flows.Free;EntityIds.Free;EntityBirths.Free;inherited Destroy;
end;
procedure TDocument.BindIds(State:TModelState);
var I,J:Integer;P:TFreeSubdivisionControlPoint;
  procedure Prepare(List:TFPList;Count:Integer;const Ids:TIds);
  var N,J:Integer;
  begin
    if State=nil then N:=Count else begin
      if Length(Ids)<>Count then raise Exception.Create('History entity count mismatch');
      N:=List.Count;
      for J:=0 to High(Ids) do begin
        if (Ids[J]<1) or (Ids[J]>2000000) then raise Exception.Create('Invalid history entity ID');
        if Ids[J]>N then N:=Ids[J];
      end;
    end;
    List.Clear;List.Count:=N;
  end;
  procedure Bind(List:TFPList;Item:Pointer;Index:Integer;const Ids:TIds);
  var Id:Int64;
  begin
    if State=nil then Id:=Index+1 else begin
      if Index>=Length(Ids) then raise Exception.Create('History entity count mismatch');
      Id:=Ids[Index];
    end;
    if (Id<1) or (Id>List.Count) or (List[Id-1]<>nil) then
      raise Exception.Create('History entity ID mismatch');
    List[Id-1]:=Item;
  end;
  procedure Restrict(Point:TFreeSubdivisionControlPoint);
  begin
    if Point<>nil then BatchAllowed[EntityId(Point)-1]:=False;
  end;
var Empty:TIds;
begin
  SetLength(Empty,0);
  if State=nil then begin
    Prepare(Points,Ship.Surface.NumberOfControlPoints,Empty);Prepare(Edges,Ship.Surface.NumberOfControlEdges,Empty);
    Prepare(Faces,Ship.Surface.NumberOfControlFaces,Empty);Prepare(Layers,Ship.Surface.NumberOfLayers,Empty);
    Prepare(Curves,Ship.Surface.NumberOfControlCurves,Empty);Prepare(Flows,Ship.NumberOfFlowLines,Empty);
  end else begin
    Prepare(Points,Ship.Surface.NumberOfControlPoints,State.PointIds);Prepare(Edges,Ship.Surface.NumberOfControlEdges,State.EdgeIds);
    Prepare(Faces,Ship.Surface.NumberOfControlFaces,State.FaceIds);Prepare(Layers,Ship.Surface.NumberOfLayers,State.LayerIds);
    Prepare(Curves,Ship.Surface.NumberOfControlCurves,State.CurveIds);Prepare(Flows,Ship.NumberOfFlowLines,State.FlowIds);
  end;
  for I:=0 to Ship.Surface.NumberOfControlPoints-1 do
    if State=nil then Bind(Points,Ship.Surface.ControlPoint[I],I,Empty)
    else Bind(Points,Ship.Surface.ControlPoint[I],I,State.PointIds);
  for I:=0 to Ship.Surface.NumberOfControlEdges-1 do
    if State=nil then Bind(Edges,Ship.Surface.ControlEdge[I],I,Empty)
    else Bind(Edges,Ship.Surface.ControlEdge[I],I,State.EdgeIds);
  for I:=0 to Ship.Surface.NumberOfControlFaces-1 do
    if State=nil then Bind(Faces,Ship.Surface.ControlFace[I],I,Empty)
    else Bind(Faces,Ship.Surface.ControlFace[I],I,State.FaceIds);
  for I:=0 to Ship.Surface.NumberOfLayers-1 do
    if State=nil then Bind(Layers,Ship.Surface.Layer[I],I,Empty)
    else Bind(Layers,Ship.Surface.Layer[I],I,State.LayerIds);
  for I:=0 to Ship.Surface.NumberOfControlCurves-1 do
    if State=nil then Bind(Curves,Ship.Surface.ControlCurve[I],I,Empty) else Bind(Curves,Ship.Surface.ControlCurve[I],I,State.CurveIds);
  for I:=0 to Ship.NumberOfFlowLines-1 do
    if State=nil then Bind(Flows,Ship.Flowline[I],I,Empty) else Bind(Flows,Ship.Flowline[I],I,State.FlowIds);
  EntityIds.Clear;EntityIds.Sorted:=False;
  for I:=0 to Points.Count-1 do if Points[I]<>nil then EntityIds.Add(PtrUInt(Points[I]),I+1);
  for I:=0 to Edges.Count-1 do if Edges[I]<>nil then EntityIds.Add(PtrUInt(Edges[I]),I+1);
  for I:=0 to Faces.Count-1 do if Faces[I]<>nil then EntityIds.Add(PtrUInt(Faces[I]),I+1);
  for I:=0 to Layers.Count-1 do if Layers[I]<>nil then EntityIds.Add(PtrUInt(Layers[I]),I+1);
  for I:=0 to Curves.Count-1 do if Curves[I]<>nil then EntityIds.Add(PtrUInt(Curves[I]),I+1);
  for I:=0 to Flows.Count-1 do if Flows[I]<>nil then EntityIds.Add(PtrUInt(Flows[I]),I+1);
  EntityIds.Sorted:=True;
  // Pointer addresses can be reused inside one command (split/merge/insert).
  // The core's creation sequence distinguishes a new object from a survivor.
  EntityBirths.Clear;EntityBirths.Sorted:=False;
  for I:=0 to Points.Count-1 do if Points[I]<>nil then EntityBirths.Add(PtrUInt(Points[I]),TFreeNamedObject(Points[I]).Id);
  for I:=0 to Edges.Count-1 do if Edges[I]<>nil then EntityBirths.Add(PtrUInt(Edges[I]),TFreeNamedObject(Edges[I]).Id);
  for I:=0 to Faces.Count-1 do if Faces[I]<>nil then EntityBirths.Add(PtrUInt(Faces[I]),TFreeNamedObject(Faces[I]).Id);
  for I:=0 to Curves.Count-1 do if Curves[I]<>nil then EntityBirths.Add(PtrUInt(Curves[I]),TFreeNamedObject(Curves[I]).Id);
  for I:=0 to Flows.Count-1 do if Flows[I]<>nil then EntityBirths.Add(PtrUInt(Flows[I]),TFreeNamedObject(Flows[I]).Id);
  EntityBirths.Sorted:=True;
  SetLength(BatchAllowed,Points.Count);
  for I:=0 to Points.Count-1 do BatchAllowed[I]:=Points[I]<>nil;
  // Mark both ends of every dependency once. This covers the reverse links
  // without rescanning the whole hull for each selected point or snapshot row.
  for I:=0 to Points.Count-1 do begin
    if Points[I]=nil then Continue;
    P:=TFreeSubdivisionControlPoint(Points[I]);
    if P.Locked or P.IsAnchorHard or (P.AnchorPoint<>nil) or
       (P.LinearConstraintPointA<>nil) or (P.LinearConstraintPointB<>nil) then Restrict(P);
    Restrict(P.AnchorPoint);Restrict(P.LinearConstraintPointA);Restrict(P.LinearConstraintPointB);
  end;
  for I:=0 to Ship.Surface.NumberOfControlPointGroups-1 do
    if Ship.Surface.ControlPointGroup[I].Locked then
      for J:=0 to Ship.Surface.ControlPointGroup[I].ControlPoints.Count-1 do Restrict(Ship.Surface.ControlPointGroup[I].ControlPoints[J]);
end;
function TDocument.EntityId(Item:Pointer):Int64;
var Index:Integer;
begin
  Index:=EntityIds.IndexOf(PtrUInt(Item));
  if Index<0 then raise Exception.Create('Model contains an unknown entity reference');
  Result:=EntityIds.Data[Index];
end;
function TDocument.BatchMove(Request:TJSONObject):Boolean;
var Moves:TJSONArray;Move:TJSONObject;I:Integer;Id:Int64;P:TFreeSubdivisionControlPoint;
    Selected:array of Boolean;Positions:array of T3DCoordinate;Ids:TIds;Before:T3DCoordinate;
begin
  if Request.Get('external_document',False) then
    raise Exception.Create('Externally owned point edits must execute in C++');
  Moves:=TJSONArray(Field(Request,'moves',jtArray));
  if (Moves.Count=0) or (Moves.Count>Points.Count) then raise Exception.Create('Invalid batch point count');
  SetLength(Selected,Points.Count);SetLength(Positions,Moves.Count);SetLength(Ids,Moves.Count);
  Result:=False;
  for I:=0 to Moves.Count-1 do begin
    if Moves[I].JSONType<>jtObject then raise Exception.Create('Invalid batch move');
    Move:=TJSONObject(Moves[I]);Id:=IntegerField(Move,'point_id',1);
    if (Id>Points.Count) or (Points[Id-1]=nil) then raise Exception.Create('Unknown control point ID');
    if Selected[Id-1] then raise Exception.Create('Duplicate control point ID');
    Selected[Id-1]:=True;
    if not BatchAllowed[Id-1] then raise Exception.Create('Batch point is locked or participates in an anchor/linear constraint');
    Ids[I]:=Id;Positions[I]:=ReadPosition(Move);
    P:=TFreeSubdivisionControlPoint(Points[Id-1]);Before:=P.Coordinate;
    if (Positions[I].X<>Before.X) or (Positions[I].Y<>Before.Y) or (Positions[I].Z<>Before.Z) then Result:=True;
  end;
  // No setter runs until the complete selection and every coordinate is valid.
  // Independent absolute setters cannot apply a linked translation twice.
  if Result then for I:=0 to High(Ids) do begin
    P:=TFreeSubdivisionControlPoint(Points[Ids[I]-1]);Before:=P.Coordinate;
    if (Positions[I].X<>Before.X) or (Positions[I].Y<>Before.Y) or (Positions[I].Z<>Before.Z) then
      P.Coordinate:=Positions[I];
  end;
end;
{$I TopologyEdits.inc}
{$I SectionEdits.inc}
{$I AdvancedEdits.inc}
{$I ModelingEdits.inc}
{$I ModelTools.inc}
{$I HydrostaticsEdits.inc}
{$I NewHull.inc}
{$I ExchangeImport.inc}
{$I DisplayEdits.inc}
{$I AuxiliaryCurves.inc}
{$I OverlaySnapshot.inc}
{$I BackgroundEdits.inc}

procedure TDocument.InvalidateAnalysis;
var I:Integer;
begin
  // Point setters already clear Surface.Built. Ship.Built := False can then
  // return early without invalidating calculations, leaving stale midship and
  // design-hydrostatics results until undo/reload constructs a fresh document.
  for I:=0 to Ship.NumberOfHydrostaticCalculations-1 do
    Ship.HydrostaticCalculation[I].Calculated:=False;
  // Surface/precision setters can also make FSetBuilt return before clearing
  // flow traces. They belong to the new geometry, not to the last paint.
  for I:=0 to Ship.NumberOfFlowLines-1 do Ship.Flowline[I].Build:=False;
end;

function TDocument.Capture:TModelState;
var I:Integer;
begin
  Result:=TModelState.Create;
  try
    // SaveBinary sorts points by address. Record the resulting serialization
    // order separately from IDs; an undo reload creates different addresses.
    Ship.SaveProject(Result.Data);
    SetLength(Result.PointIds,Ship.Surface.NumberOfControlPoints);SetLength(Result.EdgeIds,Ship.Surface.NumberOfControlEdges);
    SetLength(Result.FaceIds,Ship.Surface.NumberOfControlFaces);SetLength(Result.LayerIds,Ship.Surface.NumberOfLayers);
    SetLength(Result.CurveIds,Ship.Surface.NumberOfControlCurves);SetLength(Result.FlowIds,Ship.NumberOfFlowLines);
    for I:=0 to High(Result.PointIds) do Result.PointIds[I]:=EntityId(Ship.Surface.ControlPoint[I]);
    for I:=0 to High(Result.EdgeIds) do Result.EdgeIds[I]:=EntityId(Ship.Surface.ControlEdge[I]);
    for I:=0 to High(Result.FaceIds) do Result.FaceIds[I]:=EntityId(Ship.Surface.ControlFace[I]);
    for I:=0 to High(Result.LayerIds) do Result.LayerIds[I]:=EntityId(Ship.Surface.Layer[I]);
    for I:=0 to High(Result.CurveIds) do Result.CurveIds[I]:=EntityId(Ship.Surface.ControlCurve[I]);
    for I:=0 to High(Result.FlowIds) do Result.FlowIds[I]:=EntityId(Ship.Flowline[I]);
  except Result.Free;raise end;
end;
procedure TDocument.LoadState(State:TModelState);
begin
  GlobalFreeShip:=Ship;State.Data.Reset;Ship.LoadProject(State.Data,False);
  BindIds(State);
end;
procedure TDocument.WriteModel(const Filename,DocumentId:string;Revision:Int64);
var Root,Bounds,Settings,Obj,Diagnostics:TJSONObject;
    PointArray,EdgeArray,FaceArray,LayerArray,TriangleArray,Ids,Vertices:TJSONArray;
    I,J,K,ChildFaces,TriangleCount:Integer;LayerId,PointId:Int64;
    FaceVisited:array of Integer;D:Double;Minimum,Maximum:T3DCoordinate;
    P:TFreeSubdivisionControlPoint;E:TFreeSubdivisionControlEdge;
    F:TFreeSubdivisionControlFace;Child:TFreeSubdivisionFace;L:TFreeSubdivisionLayer;
begin
  // Validate before publishing or adopting a candidate. The C++ reader uses
  // these same domains; rejection must leave the previous document editable.
  if (Points.Count>1000000) or (Edges.Count>2000000) or (Faces.Count>1000000) or (Layers.Count>100000) then
    raise Exception.Create('Model exceeds supported collection limits');
  if Pos(#0,Ship.ProjectSettings.ProjectName)>0 then raise Exception.Create('Project name contains NUL');
  if Pos(#0,Ship.ProjectSettings.ProjectDesigner)>0 then raise Exception.Create('Designer contains NUL');
  if Pos(#0,Ship.ProjectSettings.ProjectComment)>0 then raise Exception.Create('Comment contains NUL');
  if (Ship.Preferences.FbmEncoding='') or (Pos(#0,Ship.Preferences.FbmEncoding)>0) then raise Exception.Create('Invalid text encoding');
  if not (Ship.ProjectSettings.ProjectUnits in [fuMetric,fuImperial]) then raise Exception.Create('Invalid project units');
  D:=Ship.ProjectSettings.ProjectLength;CheckCoordinate(D);if D<0 then raise Exception.Create('Negative project length');
  D:=Ship.ProjectSettings.ProjectBeam;CheckCoordinate(D);if D<0 then raise Exception.Create('Negative project beam');
  D:=Ship.ProjectSettings.ProjectDraft;CheckCoordinate(D);if D<0 then raise Exception.Create('Negative project draft');
  D:=Ship.ProjectSettings.ProjectWaterDensity;CheckCoordinate(D);if D<0 then raise Exception.Create('Negative water density');
  D:=Ship.ProjectSettings.ProjectAppendageCoefficient;CheckCoordinate(D);if D<0 then raise Exception.Create('Negative appendage coefficient');
  CheckCoordinate(Ship.ProjectSettings.ProjectSplitSectionLocation);
  if not (Ship.Precision in [fpLow,fpMedium,fpHigh,fpVeryHigh]) then raise Exception.Create('Invalid precision');
  Minimum:=Ship.Surface.Min;Maximum:=Ship.Surface.Max;
  if (Minimum.X>Maximum.X) or (Minimum.Y>Maximum.Y) or (Minimum.Z>Maximum.Z) then raise Exception.Create('Reversed model bounds');
  CheckCoordinate(Minimum.Z+Ship.ProjectSettings.ProjectDraft);
  if (ColorToRGB(Ship.ProjectSettings.ProjectUnderWaterColor)<0) or (ColorToRGB(Ship.ProjectSettings.ProjectUnderWaterColor)>$FFFFFF) then
    raise Exception.Create('Invalid underwater color');
  SetLength(FaceVisited,Points.Count);
  Root:=TJSONObject.Create;
  try
    Root.Add('format','FreeShipModelSnapshot');Root.Add('version',1);
    Root.Add('document_id',DocumentId);Root.Add('revision',Revision);
    Root.Add('name',Ship.ProjectSettings.ProjectName);
    if Ship.ProjectSettings.ProjectUnits=fuMetric then Root.Add('units','metric')
    else Root.Add('units','imperial');
    Bounds:=TJSONObject.Create;Root.Add('bounds',Bounds);
    Bounds.Add('min',Coordinates(Ship.Surface.Min));Bounds.Add('max',Coordinates(Ship.Surface.Max));
    Settings:=ProjectValues(Ship);Root.Add('settings',Settings);
    Settings.Add('created_by',Ship.ProjectSettings.ProjectFileCreatedBy);
    Settings.Add('particulars_set',Ship.ProjectSettings.MainparticularsHasBeenset);
    Settings.Add('text_encoding',Ship.Preferences.FbmEncoding);
    // Read-only persisted options: keep the diagnostic snapshot schema aligned
    // with the native publisher; edits remain owned by the C++ document service.
    Settings.Add('save_preview',(Ship.FileVersion>=fv210) and Ship.ProjectSettings.SavePreview);
    Settings.Add('coefficient_basis',Ord(Ship.ProjectSettings.ProjectCoefficients));
    Settings.Add('underwater_color_alpha',Ship.ProjectSettings.ProjectUnderWaterColorAlpha);
    Settings.Add('generate_auxiliary_reports',Ship.ProjectSettings.EnableBonjeanSAC);
    Settings.Add('hydro_displacement',Ship.Visibility.ServiceDisplacement);
    Settings.Add('hydro_lateral_area',Ship.Visibility.ServiceLateralArea);
    Settings.Add('hydro_sectional_areas',Ship.Visibility.ServiceSectionalAreas);
    Settings.Add('hydro_metacentric_height',Ship.Visibility.ServiceMetacentricHeight);
    Settings.Add('hydro_lcf',Ship.Visibility.ServiceLCF);
    Settings.Add('waterline_z',Double(Ship.Surface.Min.Z+Ship.ProjectSettings.ProjectDraft));
    WriteSections(Root);WriteAuxiliary(Root);WriteModelingData(Root);
    WriteOverlay(Root);
    WriteBackgrounds(Root);
    PointArray:=TJSONArray.Create;Root.Add('control_points',PointArray);
    for I:=0 to Points.Count-1 do begin
      if Points[I]=nil then Continue;
      P:=TFreeSubdivisionControlPoint(Points[I]);Obj:=TJSONObject.Create;PointArray.Add(Obj);
      Obj.Add('id',I+1);Obj.Add('p',Coordinates(P.Coordinate));Obj.Add('locked',P.Locked);
      Obj.Add('batch_movable',BatchAllowed[I]);
      Obj.Add('name',P.Name);Obj.Add('vertex_type',Ord(P.VertexType));Obj.Add('leak',P.IsLeak);
      if P.AnchorPoint=nil then Obj.Add('anchor',0) else Obj.Add('anchor',EntityId(P.AnchorPoint));
      if P.LinearConstraintPointA=nil then Obj.Add('line_a',0) else Obj.Add('line_a',EntityId(P.LinearConstraintPointA));
      if P.LinearConstraintPointB=nil then Obj.Add('line_b',0) else Obj.Add('line_b',EntityId(P.LinearConstraintPointB));
      Obj.Add('hard_anchor',P.IsAnchorHard);
    end;
    EdgeArray:=TJSONArray.Create;Root.Add('control_edges',EdgeArray);
    for I:=0 to Edges.Count-1 do begin
      if Edges[I]=nil then Continue;
      E:=TFreeSubdivisionControlEdge(Edges[I]);Obj:=TJSONObject.Create;EdgeArray.Add(Obj);
      if E.StartPoint=E.EndPoint then raise Exception.Create('Edge has repeated endpoints');
      Obj.Add('id',I+1);Obj.Add('a',EntityId(E.StartPoint));Obj.Add('b',EntityId(E.EndPoint));Obj.Add('crease',E.Crease);
    end;
    FaceArray:=TJSONArray.Create;Root.Add('control_faces',FaceArray);
    for I:=0 to Faces.Count-1 do begin
      if Faces[I]=nil then Continue;
      F:=TFreeSubdivisionControlFace(Faces[I]);Obj:=TJSONObject.Create;FaceArray.Add(Obj);
      if (F.NumberOfPoints<3) or (F.NumberOfPoints>100000) then raise Exception.Create('Invalid control face point count');
      Obj.Add('id',I+1);Obj.Add('layer',EntityId(F.Layer));Ids:=TJSONArray.Create;Obj.Add('points',Ids);
      for J:=0 to F.NumberOfPoints-1 do begin
        PointId:=EntityId(F.Point[J]);
        if FaceVisited[PointId-1]=I+1 then raise Exception.Create('Control face has repeated points');
        FaceVisited[PointId-1]:=I+1;Ids.Add(PointId);
      end;
    end;
    LayerArray:=TJSONArray.Create;Root.Add('layers',LayerArray);
    for I:=0 to Layers.Count-1 do begin
      if Layers[I]=nil then Continue;
      L:=TFreeSubdivisionLayer(Layers[I]);Obj:=LayerValues(L);LayerArray.Add(Obj);
      if Pos(#0,L.Name)>0 then raise Exception.Create('Layer name contains NUL');
      if (ColorToRGB(L.Color)<0) or (ColorToRGB(L.Color)>$FFFFFF) then raise Exception.Create('Invalid layer color');
      D:=L.Thickness;CheckCoordinate(D);if D<0 then raise Exception.Create('Negative layer thickness');
      D:=L.MaterialDensity;CheckCoordinate(D);if D<0 then raise Exception.Create('Negative layer density');
      Obj.Add('id',I+1);
    end;
    if NativeDisplay=nil then TriangleArray:=TJSONArray.Create
    else TriangleArray:=TJSONArray(Field(NativeDisplay,'triangles',jtArray).Clone);
    Root.Add('triangles',TriangleArray);
    ChildFaces:=0;TriangleCount:=0;
    // Kept as an independent oracle; production reuses the native subdivision.
    if NativeDisplay=nil then for I:=0 to Faces.Count-1 do begin
      if Faces[I]=nil then Continue;
      F:=TFreeSubdivisionControlFace(Faces[I]);Inc(ChildFaces,F.ChildCount);LayerId:=EntityId(F.Layer);
      for J:=0 to F.ChildCount-1 do begin
        Child:=F.Child[J];
        if Child.NumberOfPoints<3 then raise Exception.Create('Invalid derived face');
        Inc(TriangleCount,Child.NumberOfPoints-2);
        if TriangleCount>5000000 then raise Exception.Create('Too many derived triangles');
        for K:=2 to Child.NumberOfPoints-1 do begin
          Obj:=TJSONObject.Create;TriangleArray.Add(Obj);Obj.Add('layer',LayerId);
          Vertices:=TJSONArray.Create;Obj.Add('p',Vertices);
          Vertices.Add(Coordinates(Child.Point[0].Coordinate));
          Vertices.Add(Coordinates(Child.Point[K-1].Coordinate));
          Vertices.Add(Coordinates(Child.Point[K].Coordinate));
        end;
      end;
    end;
    Diagnostics:=TJSONObject.Create;Root.Add('diagnostics',Diagnostics);
    Diagnostics.Add('legacy_triangle_count',TriangleCount);
    if NativeDisplay<>nil then begin
      ChildFaces:=IntegerField(NativeDisplay,'child_faces');TriangleCount:=TriangleArray.Count;
      Diagnostics.Add('surface_engine','cpp-native');Diagnostics.Add('flow_engine','cpp-native');
    end else begin Diagnostics.Add('surface_engine','pascal-oracle');Diagnostics.Add('flow_engine','pascal-initialized-edge-parameter') end;
    Diagnostics.Add('source_child_faces',ChildFaces);Diagnostics.Add('source_triangle_count',TriangleCount);
    Diagnostics.Add('forms_created',Screen.FormCount);Diagnostics.Add('viewports_created',Ship.NumberOfViewports);
    SaveJSON(Filename,Root);
  finally Root.Free end;
end;
procedure TDocument.Export(const Output,DocumentId:string;Revision:Int64);
var Scene:TLinesplanScene;Prefix,Stage:string;G:TGUID;Display:TJSONObject;
    FaceIds,LayerIds,CurveIds,FlowDefs,LayerFaces,FaceOrder:TJSONArray;I,J:Integer;Stream:TFileStream;Data:TJSONData;
    Flow:TFreeFlowline;Seed:T2DCoordinate;Def:TJSONObject;
begin
  GlobalFreeShip:=Ship;
  if (Screen.FormCount<>0) or (Ship.NumberOfViewports<>0) then
    raise Exception.Create('Model service created a legacy window');
  CreateGUID(G);Stage:=IncludeTrailingPathDelimiter(Output)+'.pending-'+GUIDToString(G);
  if not CreateDir(Stage) then raise Exception.Create('Cannot create artifact staging directory');
  Prefix:=IncludeTrailingPathDelimiter(Stage);
  Display:=nil;NativeDisplay:=nil;
  try
    // DXF adjusts spline fragments. Reset intersections for every standalone
    // generation so snapshot-after-save has the same sampling as initial load.
    if NativeLinesplanEnabled then begin
      if NativeSurfaceEnabled then begin
        Display:=TJSONObject.Create;Display.Add('show_normals',Ship.Visibility.ShowNormals);
        FaceIds:=TJSONArray.Create;Display.Add('faces',FaceIds);
        LayerIds:=TJSONArray.Create;Display.Add('layers',LayerIds);
        CurveIds:=TJSONArray.Create;Display.Add('curves',CurveIds);
        for I:=0 to Ship.Surface.NumberOfControlFaces-1 do FaceIds.Add(EntityId(Ship.Surface.ControlFace[I]));
        for I:=0 to Ship.Surface.NumberOfLayers-1 do LayerIds.Add(EntityId(Ship.Surface.Layer[I]));
        for I:=0 to Ship.Surface.NumberOfControlCurves-1 do CurveIds.Add(EntityId(Ship.Surface.ControlCurve[I]));
        FlowDefs:=TJSONArray.Create;Display.Add('flows',FlowDefs);
        for I:=0 to Ship.NumberOfFlowLines-1 do begin
          Flow:=Ship.Flowline[I];Seed:=Flow.ServiceSource;Def:=TJSONObject.Create;FlowDefs.Add(Def);
          Def.Add('id',EntityId(Flow));Def.Add('view',Ord(Flow.ServiceView));
          Def.Add('source',TJSONArray.Create([Double(Seed.X),Double(Seed.Y)]));
        end;
        LayerFaces:=TJSONArray.Create;Display.Add('layer_faces',LayerFaces);
        for I:=0 to Ship.Surface.NumberOfLayers-1 do begin
          FaceOrder:=TJSONArray.Create;LayerFaces.Add(FaceOrder);
          for J:=0 to Ship.Surface.Layer[I].Count-1 do
            FaceOrder.Add(EntityId(Ship.Surface.Layer[I].Items[J]));
        end;
      end;
      GenerateNativeLinesplan(Ship,Prefix,DocumentId,Revision,Display);
      if NativeSurfaceEnabled then begin
        Stream:=TFileStream.Create(Prefix+'surface.json',fmOpenRead or fmShareDenyNone);
        try
          if Stream.Size>256*1024*1024 then raise Exception.Create('Surface display too large');
          Data:=GetJSON(Stream);
          if not (Data is TJSONObject) then begin Data.Free;raise Exception.Create('Invalid surface display') end;
          NativeDisplay:=TJSONObject(Data);
          if (StringField(NativeDisplay,'format')<>'FreeShipSurfaceDisplay') or
             (IntegerField(NativeDisplay,'version')<>1) then raise Exception.Create('Unsupported surface display');
        finally Stream.Free end;
      end;
    end
    else begin
    Ship.InvalidateIntersections;
    Scene:=TLinesplanScene.Create(Ship);
    try
      Scene.Generate(Prefix+'scene.fslp');Scene.ExportDXF(Prefix+'reference.dxf');
    finally Scene.Free end;
    end;
    WriteModel(Prefix+'model.json',DocumentId,Revision);
    PublishFile(Prefix+'scene.fslp',IncludeTrailingPathDelimiter(Output)+'scene.fslp');
    PublishFile(Prefix+'reference.dxf',IncludeTrailingPathDelimiter(Output)+'reference.dxf');
    PublishFile(Prefix+'model.json',IncludeTrailingPathDelimiter(Output)+'model.json');
    if NativeLinesplanEnabled then begin
      PublishFile(Prefix+'native-input.json',IncludeTrailingPathDelimiter(Output)+'native-input.json');
      PublishFile(Prefix+'native-report.json',IncludeTrailingPathDelimiter(Output)+'native-report.json');
      PublishFile(Prefix+'sections.json',IncludeTrailingPathDelimiter(Output)+'sections.json');
      if NativeSurfaceEnabled then PublishFile(Prefix+'surface.json',IncludeTrailingPathDelimiter(Output)+'surface.json');
    end;
  finally
    SysUtils.DeleteFile(Prefix+'scene.fslp');SysUtils.DeleteFile(Prefix+'reference.dxf');SysUtils.DeleteFile(Prefix+'model.json');
    SysUtils.DeleteFile(Prefix+'native-input.json');SysUtils.DeleteFile(Prefix+'native-report.json');
    FreeAndNil(NativeDisplay);Display.Free;
    SysUtils.DeleteFile(Prefix+'sections.json');SysUtils.DeleteFile(Prefix+'surface.json');RemoveDir(Stage);
  end;
end;
constructor TService.Create;
begin inherited Create;History:=TFPList.Create;HistoryIndex:=-1;CleanIndex:=-1 end;
procedure TService.ClearHistory;
var I:Integer;
begin for I:=0 to History.Count-1 do TObject(History[I]).Free;History.Clear end;
function TService.HistoryBytes:Int64;
var I:Integer;
begin
  Result:=0;for I:=0 to History.Count-1 do Inc(Result,TModelState(History[I]).MemoryBytes);
end;
function TService.HistoryLimit:Int64;
begin
  Result:=Max(Int64(0),Int64(Doc.Ship.Preferences.MaxUndoMemory))*1024*1024;
end;
procedure TService.TrimHistory;
var Bytes,Limit:Int64;
begin
  Bytes:=HistoryBytes;Limit:=HistoryLimit;
  // Match the legacy budget's two-state floor: one undo remains available even
  // when a single large document exceeds the configured memory budget.
  while (History.Count>2) and (Bytes>Limit) do begin
    Dec(Bytes,TModelState(History[0]).MemoryBytes);
    TObject(History[0]).Free;History.Delete(0);Dec(HistoryIndex);
    if CleanIndex>=0 then Dec(CleanIndex);
  end;
end;
destructor TService.Destroy;
begin Doc.Free;ClearHistory;History.Free;inherited Destroy end;
procedure TService.RequireDocument(Request:TJSONObject);
begin
  if Doc=nil then raise Exception.Create('No document loaded');
  if StringField(Request,'expected_document_id')<>DocumentId then raise Exception.Create('Stale document ID');
  if IntegerField(Request,'expected_revision',1)<>Revision then raise Exception.Create('Stale document revision');
end;
procedure TService.AddState(Response:TJSONObject);
begin
  if Doc=nil then Exit;
  Response.Add('document_id',DocumentId);Response.Add('revision',Revision);
  Response.Add('can_undo',HistoryIndex>0);Response.Add('can_redo',HistoryIndex<History.Count-1);
  Response.Add('modified',HistoryIndex<>CleanIndex);
  Response.Add('history_states',History.Count);Response.Add('history_bytes',HistoryBytes);
  Response.Add('history_limit_bytes',HistoryLimit);
end;
procedure TService.AddArtifacts(Response:TJSONObject;const Output:string);
begin
  Response.Add('model',IncludeTrailingPathDelimiter(Output)+'model.json');
  Response.Add('scene',IncludeTrailingPathDelimiter(Output)+'scene.fslp');
  Response.Add('dxf',IncludeTrailingPathDelimiter(Output)+'reference.dxf');
  if NativeLinesplanEnabled then Response.Add('native_report',IncludeTrailingPathDelimiter(Output)+'native-report.json');
  if NativeLinesplanEnabled then Response.Add('sections',IncludeTrailingPathDelimiter(Output)+'sections.json');
  if NativeSurfaceEnabled then Response.Add('surface',IncludeTrailingPathDelimiter(Output)+'surface.json');
end;
{$I DocumentAdapter.inc}
function PresentationContext(Ship:TFreeShip):TJSONObject;
var Colors,Texts:TJSONObject;
  procedure ColorValue(const Key:string;Value:TColor);
  begin
    Colors.Add(Key,TJSONObject.Create(['value',LongInt(ColorToRGB(Value)),
      'raw',LongInt(Value),'dxf',FindDXFColorIndex(Value)]));
  end;
begin
  { Preferences and translations only. No user model is loaded or rebuilt. }
  Result:=TJSONObject.Create;Colors:=TJSONObject.Create;Result.Add('colors',Colors);
  ColorValue('station',Ship.Preferences.StationColor);ColorValue('buttock',Ship.Preferences.ButtockColor);
  ColorValue('waterline',Ship.Preferences.WaterlineColor);ColorValue('diagonal',Ship.Preferences.DiagonalColor);
  ColorValue('crease',Ship.Preferences.CreaseColor);ColorValue('grid',Ship.Preferences.GridColor);ColorValue('red',clRed);
  Texts:=TJSONObject.Create;Result.Add('texts',Texts);
  Texts.Add('base',rs_Base);Texts.Add('dwl',rs_DWL);Texts.Add('center',rs_Center);
  Result.Add('lateral',rs_Lateral_area);Result.Add('wind',rs_Lateral_wind_area);
  Result.Add('decimal_separator',String(DefaultFormatSettings.DecimalSeparator));
  Result.Add('curvature_color',LongInt(ColorToRGB(Ship.Preferences.CurvaturePlotColor)));
  Result.Add('control_curve_color',LongInt(ColorToRGB(Ship.Preferences.ControlCurveColor)));
  Result.Add('marker_color',LongInt(ColorToRGB(Ship.Preferences.MarkerColor)));
  Result.Add('normal_color',LongInt(ColorToRGB(Ship.Surface.NormalColor)));
  Result.Add('grid_font_color',LongInt(ColorToRGB(Ship.Preferences.GridFontColor)));
  Result.Add('hydro_color',LongInt(ColorToRGB(Ship.Preferences.HydrostaticsFontColor)));
  Result.Add('point_size',Ship.Preferences.PointSize);Result.Add('font_size',Ship.Preferences.FontSize);
end;
function TService.Execute(Request:TJSONObject;out Quit:Boolean):TJSONObject;
var Method,Output,NewId,Filename:string;Id,PointId,NewRevision:Int64;
    Candidate,OldDoc:TDocument;Buffer:TFreeFileBuffer;State:TModelState;
    P:TFreeSubdivisionControlPoint;Position:TJSONArray;C,Before:T3DCoordinate;
    Target,I:Integer;G:TGUID;Changed:Boolean;Hydro,Colors:TJSONObject;Values:TJSONArray;Color:TColor;Key:string;
begin
  Quit:=False;Id:=IntegerField(Request,'id');Method:=StringField(Request,'method');
  NativeLinesplanEnabled:=Request.Get('native_linesplan',False);
  NativeSurfaceEnabled:=NativeLinesplanEnabled and Request.Get('native_surface',False);
  Result:=TJSONObject.Create(['id',Id,'ok',True]);Candidate:=nil;State:=nil;
  try
   try
    if Method='quit' then begin AddState(Result);Quit:=True;Exit end;
    if Method='document_context' then begin
      if not Request.Get('external_document',False) then raise Exception.Create('Document context requires native ownership');
      Candidate:=TDocument.Create;
      Result.Add('fbm_encoding',Candidate.Ship.Preferences.FbmEncoding);Result.Add('layer_name_prefix',rs_Layer);
      Result.Add('default_layer_color',Int64(Candidate.Ship.Preferences.LayerColor));
      Result.Add('default_underwater_color',Int64(Candidate.Ship.Preferences.UnderWaterColor));
      Result.Add('default_underwater_alpha',Candidate.Ship.Preferences.UnderWaterColorAlpha);
      Result.Add('initial_layer_color',Int64(Candidate.Ship.Surface.Layer[0].Color));
      Result.Add('initial_layer_id',Candidate.Ship.Surface.Layer[0].LayerID);
      Result.Add('initial_underwater_color',Int64(Candidate.Ship.ProjectSettings.ProjectUnderWaterColor));
      Result.Add('initial_underwater_alpha',Candidate.Ship.ProjectSettings.ProjectUnderWaterColorAlpha);
      Result.Add('history_limit_bytes',Max(Int64(0),Int64(Candidate.Ship.Preferences.MaxUndoMemory))*1024*1024);
      Result.Add('presentation_context',PresentationContext(Candidate.Ship));
      { Presentation context only; the native document owns plate geometry. }
      Hydro:=TJSONObject.Create;Result.Add('plate_context',Hydro);Hydro.Add('part',rs_Part);
      Values:=TJSONArray.Create;Hydro.Add('palette',Values);
      for I:=1 to 255 do Values.Add(LongInt(DXFLayerColors[I]));
      Values:=TJSONArray.Create;Hydro.Add('sections',Values);
      for I:=0 to 3 do begin
        case I of
          0:Color:=Candidate.Ship.Preferences.StationColor;
          1:Color:=Candidate.Ship.Preferences.ButtockColor;
          2:Color:=Candidate.Ship.Preferences.WaterlineColor;
          3:Color:=Candidate.Ship.Preferences.DiagonalColor;
        end;
        Values.Add(TJSONObject.Create(['color',LongInt(ColorToRGB(Color)),'dxf',FindDXFColorIndex(Color)]));
      end;
      Colors:=TJSONObject.Create;Result.Add('resolved_colors',Colors);
      Values:=TJSONArray(Field(Request,'colors',jtArray));
      if Values.Count>100002 then raise Exception.Create('Too many palette colors');
      for I:=-4 to Values.Count-1 do begin
        case I of
          -4:Color:=Candidate.Ship.Preferences.LayerColor;
          -3:Color:=Candidate.Ship.Preferences.UnderWaterColor;
          -2:Color:=Candidate.Ship.Surface.Layer[0].Color;
          -1:Color:=Candidate.Ship.ProjectSettings.ProjectUnderWaterColor;
          else begin
            if (Values[I].JSONType<>jtNumber) or (Frac(Values[I].AsFloat)<>0) or
              (Values[I].AsFloat<Low(LongInt)) or (Values[I].AsFloat>High(LongInt)) then raise Exception.Create('Invalid palette color');
            Color:=Values[I].AsInt64;
          end;
        end;
        Key:=IntToStr(LongInt(Color));if Colors.Find(Key)=nil then Colors.Add(Key,Int64(ColorToRGB(Color)));
      end;
      Exit;
    end;
    if Request.Get('external_document',False) and ((Method='load') or (Method='new') or (Method='import_model') or (Method='promote_document')) then
      raise Exception.Create('Document creation and version conversion must execute in C++');
    if (Method='load') or (Method='new') or (Method='import_model') or (Method='restore_document') then begin
      Output:=OutputDirectory(Request);
      if (Method='load') or (Method='import_model') or (Method='restore_document') then Filename:=StringField(Request,'path');
      if ((Method='new') or (Method='import_model')) and (Doc<>nil) then RequireDocument(Request);
      NewId:=StringField(Request,'document_id');
      try
        if NewId[1]='{' then G:=StringToGUID(NewId) else G:=StringToGUID('{'+NewId+'}');
      except raise Exception.Create('Invalid document UUID') end;
      if GUIDToString(G)='{00000000-0000-0000-0000-000000000000}' then raise Exception.Create('Invalid document UUID');
      if (Method<>'restore_document') and (Doc<>nil) and (NewId=DocumentId) then raise Exception.Create('load requires a new document UUID');
      if Method='restore_document' then Candidate:=TDocument.Create(Doc) else Candidate:=TDocument.Create;
      GlobalFreeShip:=Candidate.Ship;
      if Method='new' then Candidate.NewHull(Request)
      else if Method='import_model' then Candidate.ImportExchange(Filename)
      else if Method='restore_document' then begin
        if not Request.Get('external_document',False) then raise Exception.Create('Document restore requires external ownership');
        State:=ReadDocumentState(Filename,StringField(Request,'bindings'),Candidate);
        Candidate.LoadState(State);
        ApplyNativeEdgeOrder(Candidate,StringField(Request,'bindings'));
        ApplyNativeDependencyOrder(Candidate,StringField(Request,'bindings'));
        ApplyNativeLayerOrder(Candidate,StringField(Request,'bindings'));
      end
      else begin Buffer:=TFreeFileBuffer.Create;
        try Buffer.LoadFromFile(Filename);Candidate.Ship.LoadProject(Buffer,False) finally Buffer.Free end;
      end;
      if Method<>'restore_document' then Candidate.BindIds(nil);
      // Native coordinate edits already carry a complete FBM and matching IDs.
      // Reuse that immutable checkpoint; do not serialize the same state again.
      if Method<>'restore_document' then State:=Candidate.Capture;
      NewRevision:=1;
      if Method='restore_document' then NewRevision:=IntegerField(Request,'revision',1);
      if Request.Get('publish',True) then Candidate.Export(Output,NewId,NewRevision);
      OldDoc:=Doc;Doc:=Candidate;Candidate:=nil;OldDoc.Free;
      ClearHistory;History.Add(State);State:=nil;HistoryIndex:=0;CleanIndex:=0;
      if (Method='new') or (Method='import_model') then CleanIndex:=-1;
      DocumentId:=NewId;Revision:=NewRevision;
      if Request.Get('publish',True) then AddArtifacts(Result,Output);
    end else begin
      RequireDocument(Request);
      if Method='promote_document' then begin
        if not Request.Get('external_document',False) then raise Exception.Create('Document conversion requires external ownership');
        I:=Request.Get('minimum_version',Ord(fv198));
        if not (I in [Ord(fv120),Ord(fv160),Ord(fv180),Ord(fv191),Ord(fv195),
          Ord(fv198),Ord(fv201),Ord(fv230),Ord(fv250),41,42,Ord(fv462)]) then
          raise Exception.Create('Unsupported document conversion');
        RequireVersion(Doc.Ship,TFreeFileVersion(I));
        State:=Doc.Capture;
        ClearHistory;History.Add(State);State:=nil;HistoryIndex:=0;CleanIndex:=0;
      end
      else if Method='apply_document_points' then begin
        if not Request.Get('external_document',False) then raise Exception.Create('Native coordinate publication requires external ownership');
        NewRevision:=IntegerField(Request,'revision',1);
        if (Revision=MaxJSONInteger) or (NewRevision<>Revision+1) then raise Exception.Create('Invalid native coordinate revision');
        Output:=OutputDirectory(Request);
        State:=ReadDocumentState(StringField(Request,'path'),StringField(Request,'bindings'),Doc);
        Doc.ApplyNativeCoordinates(StringField(Request,'coordinates'));
        Doc.InvalidateAnalysis;Doc.Ship.Built:=False;Doc.Ship.RebuildModel;
        Doc.Export(Output,DocumentId,NewRevision);
        ClearHistory;History.Add(State);State:=nil;HistoryIndex:=0;CleanIndex:=-1;Revision:=NewRevision;
        AddArtifacts(Result,Output);
      end
      else if Method='inspect_modeling' then begin
        if Request.Get('external_document',False) then raise Exception.Create('Externally owned modeling measurements must execute in C++');
        Result.Add('analysis',Doc.InspectModeling)
      end
      else if Method='hydro_defaults' then begin
        if Request.Get('external_document',False) then raise Exception.Create('Externally owned hydrostatics defaults must execute in C++');
        Result.Add('analysis',Doc.HydroDefaults)
      end
      else if Method='cad_iges' then begin
        if Request.Get('external_document',False) then raise Exception.Create('Externally owned IGES preparation must execute in C++');
        Output:=OutputDirectory(Request);
        Candidate:=TDocument.Create;Candidate.LoadState(TModelState(History[HistoryIndex]));GlobalFreeShip:=Candidate.Ship;
        Hydro:=BuildCadPatches(Candidate.Ship,Candidate.Layers,IncludeTrailingPathDelimiter(Output)+'cancel.flag');
        try
          Hydro.Add('document_id',DocumentId);Hydro.Add('revision',Revision);
          SaveJSON(IncludeTrailingPathDelimiter(Output)+'cad.json',Hydro);
        finally Hydro.Free end;
        Result.Add('cad',IncludeTrailingPathDelimiter(Output)+'cad.json');
      end
      else if Method='develop_plates' then begin
        if Request.Get('external_document',False) then raise Exception.Create('Externally owned plate development must execute in C++');
        Output:=OutputDirectory(Request);
        Candidate:=TDocument.Create;Candidate.LoadState(TModelState(History[HistoryIndex]));GlobalFreeShip:=Candidate.Ship;
        Hydro:=DevelopPlates(Candidate.Ship,IncludeTrailingPathDelimiter(Output)+'cancel.flag');
        try
          Hydro.Add('document_id',DocumentId);Hydro.Add('revision',Revision);
          SaveJSON(IncludeTrailingPathDelimiter(Output)+'plates.json',Hydro);
        finally Hydro.Free end;
        Result.Add('plates',IncludeTrailingPathDelimiter(Output)+'plates.json');
      end
      else if (Method='performance_defaults') or (Method='performance') then begin
        if Request.Get('external_document',False) and
           (Copy(Request.Get('calculator',''),1,9)<>'propeller') then
          raise Exception.Create('Externally owned resistance calculations must execute in C++');
        if Request.Get('external_document',False) and
           ((Request.Get('calculator','')='propeller1') or (Request.Get('calculator','')='propeller2') or
            (Request.Get('calculator','')='propeller3') or (Request.Get('calculator','')='propeller4') or
            (Request.Get('calculator','')='propeller5')) then
          raise Exception.Create('Externally owned propulsion calculations must execute in C++');
        Output:=OutputDirectory(Request);
        Candidate:=TDocument.Create;Candidate.LoadState(TModelState(History[HistoryIndex]));GlobalFreeShip:=Candidate.Ship;
        Hydro:=Performance(Candidate.Ship,Request,Output,Method='performance');
        try
          Hydro.Add('document_id',DocumentId);Hydro.Add('revision',Revision);
          SaveJSON(IncludeTrailingPathDelimiter(Output)+'performance.json',Hydro);
        finally Hydro.Free end;
        Result.Add('performance',IncludeTrailingPathDelimiter(Output)+'performance.json');
      end
      else if Method='hydrostatics' then begin
        if Request.Get('external_document',False) then raise Exception.Create('Externally owned hydrostatics must execute in C++');
        Output:=OutputDirectory(Request);
        // Calculations get a disposable model, never the editing document or its
        // cached design calculation. Failure/cancellation cannot alter history.
        Candidate:=TDocument.Create;Candidate.LoadState(TModelState(History[HistoryIndex]));GlobalFreeShip:=Candidate.Ship;
        Hydro:=CalculateHydrostatics(Candidate.Ship,Request,IncludeTrailingPathDelimiter(Output)+'cancel.flag');
        try
          Hydro.Add('document_id',DocumentId);Hydro.Add('revision',Revision);
          SaveJSON(IncludeTrailingPathDelimiter(Output)+'hydro.json',Hydro);
        finally Hydro.Free end;
        Result.Add('hydro',IncludeTrailingPathDelimiter(Output)+'hydro.json');
      end
      else if (Method='performance_settings') or (Method='hydro_settings') or (Method='update_project') or (Method='update_layer') or (Method='move_points') or (Method='topology') or (Method='advanced') or (Method='modeling') or (Method='sections') or (Method='manage_layers') or (Method='background') or (Method='display') or (Method='auxiliary') then begin
        if Request.Get('external_document',False) then begin
          if (Method='hydro_settings') or (Method='performance_settings') then
            raise Exception.Create('Externally owned analysis settings must execute in C++');
          if Method='topology' then raise Exception.Create('Externally owned topology edits must execute in C++');
          if Method='advanced' then raise Exception.Create('Externally owned advanced edits must execute in C++');
          if Method='modeling' then raise Exception.Create('Externally owned modeling edits must execute in C++');
          if (Method='update_project') or (Method='update_layer') or (Method='manage_layers') or (Method='sections') then
            raise Exception.Create('Externally owned project, layer and section edits must execute in C++');
          if (Method='background') or (Method='display') or (Method='auxiliary') then
            raise Exception.Create('Externally owned background, display and auxiliary edits must execute in C++');
        end;
        if Revision=MaxJSONInteger then raise Exception.Create('Revision limit reached');
        NewRevision:=Revision+1;Output:=OutputDirectory(Request);
        try
          if Method='performance_settings' then begin SavePerformanceSettings(Doc.Ship,Request,Output);Changed:=True end
          else if Method='hydro_settings' then Changed:=Doc.HydroSettings(Request)
          else if Method='topology' then Changed:=Doc.TopologyEdit(Request)
          else if Method='advanced' then Changed:=Doc.AdvancedEdit(Request)
          else if Method='modeling' then Changed:=Doc.ModelingEdit(Request)
          else if Method='sections' then Changed:=Doc.EditSections(Request)
          else if Method='manage_layers' then Changed:=Doc.ManageLayers(Request)
          else if Method='background' then Changed:=Doc.EditBackground(Request)
          else if Method='display' then Changed:=Doc.EditDisplay(Request)
          else if Method='auxiliary' then Changed:=Doc.EditAuxiliary(Request)
          else if Method='update_project' then
            Changed:=ApplyProjectChanges(Doc.Ship,TJSONObject(Field(Request,'changes',jtObject)))
          else if Method='update_layer' then begin
            PointId:=IntegerField(Request,'layer_id',1);
            if (PointId>Doc.Layers.Count) or (Doc.Layers[PointId-1]=nil) then raise Exception.Create('Unknown layer ID');
            Changed:=ApplyLayerChanges(Doc.Ship,TFreeSubdivisionLayer(Doc.Layers[PointId-1]),TJSONObject(Field(Request,'changes',jtObject)));
          end else Changed:=Doc.BatchMove(Request);
          if Changed then begin
            if (Method<>'background') and (Method<>'display') and (Method<>'auxiliary') and (Method<>'hydro_settings') and (Method<>'performance_settings') then Doc.InvalidateAnalysis;
            Doc.Ship.FileChanged:=True;
            // Section definitions do not change the surface or control net.
            if Method='sections' then Doc.Ship.InvalidateIntersections
            else if (Method<>'background') and (Method<>'display') and (Method<>'auxiliary') and (Method<>'hydro_settings') and (Method<>'performance_settings') then begin Doc.Ship.Built:=False;Doc.Ship.RebuildModel end;
            State:=Doc.Capture;Doc.Export(Output,DocumentId,NewRevision);
          end else Doc.Export(Output,DocumentId,Revision);
        except
          Candidate:=TDocument.Create(Doc);Candidate.LoadState(TModelState(History[HistoryIndex]));
          OldDoc:=Doc;Doc:=Candidate;Candidate:=nil;OldDoc.Free;raise;
        end;
        if Changed then begin
          while History.Count>HistoryIndex+1 do begin TObject(History[History.Count-1]).Free;History.Delete(History.Count-1) end;
          if CleanIndex>HistoryIndex then CleanIndex:=-1;
          History.Add(State);State:=nil;Inc(HistoryIndex);Revision:=NewRevision;TrimHistory;
        end;
        AddArtifacts(Result,Output);
      end else if (Method='move_point') or (Method='undo') or (Method='redo') then begin
        if (Method='move_point') and Request.Get('external_document',False) then
          raise Exception.Create('Externally owned point edits must execute in C++');
        if Revision=MaxJSONInteger then raise Exception.Create('Revision limit reached');
        NewRevision:=Revision+1;Output:=OutputDirectory(Request);
        if Method='move_point' then begin
          PointId:=IntegerField(Request,'point_id',1);
          if (PointId>Doc.Points.Count) or (Doc.Points[PointId-1]=nil) then raise Exception.Create('Unknown control point ID');
          P:=TFreeSubdivisionControlPoint(Doc.Points[PointId-1]);
          if P.Locked then raise Exception.Create('Control point is locked');
          if P.IsAnchorHard then raise Exception.Create('Hard-anchored point cannot be moved directly');
          Position:=TJSONArray(Field(Request,'position',jtArray));
          if Position.Count<>3 then raise Exception.Create('position must have three coordinates');
          for I:=0 to 2 do begin
            if Position.Items[I].JSONType<>jtNumber then raise Exception.Create('position must be numeric');
            CheckCoordinate(Position.Floats[I]);
          end;
          // Compare in the original float domain, not JSON's double domain.
          C.X:=Position.Floats[0];C.Y:=Position.Floats[1];C.Z:=Position.Floats[2];Before:=P.Coordinate;
          Changed:=(C.X<>Before.X) or (C.Y<>Before.Y) or (C.Z<>Before.Z);
          if Changed then begin
            try
              P.SetCoordinate(nil,C,nil);C:=P.Coordinate;
              Changed:=(C.X<>Before.X) or (C.Y<>Before.Y) or (C.Z<>Before.Z);
              if Changed then Doc.InvalidateAnalysis;
              Doc.Ship.Built:=False;Doc.Ship.RebuildModel;
              if Changed then begin
                State:=Doc.Capture;Doc.Export(Output,DocumentId,NewRevision);
              end else Doc.Export(Output,DocumentId,Revision);
            except
              Candidate:=TDocument.Create(Doc);Candidate.LoadState(TModelState(History[HistoryIndex]));
              OldDoc:=Doc;Doc:=Candidate;Candidate:=nil;OldDoc.Free;raise;
            end;
            if Changed then begin
              while History.Count>HistoryIndex+1 do begin TObject(History[History.Count-1]).Free;History.Delete(History.Count-1) end;
              if CleanIndex>HistoryIndex then CleanIndex:=-1;
              History.Add(State);State:=nil;Inc(HistoryIndex);Revision:=NewRevision;TrimHistory;
            end;
          end else Doc.Export(Output,DocumentId,Revision);
        end else begin
          if Method='undo' then Target:=HistoryIndex-1 else Target:=HistoryIndex+1;
          if (Target<0) or (Target>=History.Count) then raise Exception.Create('No '+Method+' state available');
          Candidate:=TDocument.Create(Doc);Candidate.LoadState(TModelState(History[Target]));
          Candidate.Export(Output,DocumentId,NewRevision);
          OldDoc:=Doc;Doc:=Candidate;Candidate:=nil;OldDoc.Free;HistoryIndex:=Target;Revision:=NewRevision;
        end;
        AddArtifacts(Result,Output);
      end else if Method='snapshot' then begin
        Output:=OutputDirectory(Request);Doc.Export(Output,DocumentId,Revision);AddArtifacts(Result,Output);
      end else if Method='save' then begin
        Output:=OutputDirectory(Request);Filename:=IncludeTrailingPathDelimiter(Output)+'saved.fbm';
        State:=Doc.Capture;
        try
          if not State.Data.SaveToFile(Filename+'.pending') then raise Exception.Create('Failed writing model');
          PublishFile(Filename+'.pending',Filename);
        finally SysUtils.DeleteFile(Filename+'.pending') end;
        Result.Add('saved_file',Filename);
      end else if Method='mark_saved' then CleanIndex:=HistoryIndex
      else raise Exception.Create('Unknown method '+Method);
    end;
    AddState(Result);
   except Result.Free;raise end;
  finally
    State.Free;Candidate.Free;
    if Doc<>nil then GlobalFreeShip:=Doc.Ship;
  end;
end;

procedure RunModelService;
var Service:TService;Line:string;Data:TJSONData;Response:TJSONObject;Quit:Boolean;Id:Int64;
begin
  DefaultFormatSettings.DecimalSeparator:='.';
  SetMultiByteConversionCodePage(CP_UTF8);SetMultiByteRTLFileSystemCodePage(CP_UTF8);
  SetTextCodePage(Input,CP_UTF8);SetTextCodePage(Output,CP_UTF8);
  Application.Initialize;ApplyLanguageSafely('cn',ExtractFilePath(ParamStr(0))+'locale');
  Service:=TService.Create;Quit:=False;
  try
    while not Quit and not EOF(Input) do begin
      ReadLn(Input,Line);Data:=nil;Response:=nil;Id:=0;
      try
        if Length(Line)>MaxRequestBytes then raise Exception.Create('Request exceeds 1 MiB');
        Data:=GetJSON(NormalizeUnicodeEscapes(Line));
        if Data.JSONType<>jtObject then raise Exception.Create('Request must be a JSON object');
        Id:=IntegerField(TJSONObject(Data),'id');
        Response:=Service.Execute(TJSONObject(Data),Quit);
        if not Quit and TJSONObject(Data).Get('external_document',False) then begin
          if TJSONObject(Data).Get('checkpoint',False) then
            Service.ExportDocumentState(Response,OutputDirectory(TJSONObject(Data)));
          Service.ReleaseExternalHistory;
        end;
      except on E:Exception do begin
        FreeAndNil(Response);Response:=TJSONObject.Create(['id',Id,'ok',False,'error',E.Message]);
        Service.AddState(Response);
      end end;
      try WriteLn(Output,Response.AsJSON);Flush(Output) finally Response.Free;Data.Free end;
    end;
  finally Service.Free end;
end;
end.
