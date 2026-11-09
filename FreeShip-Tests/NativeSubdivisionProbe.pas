program NativeSubdivisionProbe;
{$mode objfpc}{$H+}{$codepage utf8}
// Observe the original control net and subdivision. No replacement geometry formulas.
uses Interfaces,Forms,SysUtils,Classes,Math,Contnrs,fpjson,jsonparser,
 FreeGeometry,FreeShipUnit,FreeFileBuffer,FreeTypes,FreeVersionUnit,FreeSafeLanguage;

function ReadJSON(const Filename:string):TJSONData;
var Stream:TFileStream;
begin Stream:=TFileStream.Create(Filename,fmOpenRead or fmShareDenyWrite);
 try Result:=GetJSON(Stream) finally Stream.Free end end;

procedure WriteJSON(const Filename:string;Data:TJSONData);
var Stream:TFileStream;Value:UTF8String;
begin Value:=Data.AsJSON;Stream:=TFileStream.Create(Filename,fmCreate);
 try if Length(Value)>0 then Stream.WriteBuffer(Value[1],Length(Value)) finally Stream.Free end end;

function Obj(Data:TJSONData):TJSONObject;
begin if (Data=nil) or (Data.JSONType<>jtObject) then raise Exception.Create('Expected object');Result:=TJSONObject(Data) end;
function Arr(Data:TJSONData;Limit:Integer=1000000):TJSONArray;
begin if (Data=nil) or (Data.JSONType<>jtArray) then raise Exception.Create('Expected array');
 Result:=TJSONArray(Data);if Result.Count>Limit then raise Exception.Create('Array limit exceeded') end;
function Num(Data:TJSONData):Double;
begin if (Data=nil) or (Data.JSONType<>jtNumber) then raise Exception.Create('Expected number');Result:=Data.AsFloat;
 if IsNan(Result) or IsInfinite(Result) then raise Exception.Create('Expected finite number') end;
function IntVal(Data:TJSONData;Lo,Hi:Integer):Integer;
var N:Double;
begin N:=Num(Data);if (N<Lo) or (N>Hi) or (Frac(N)<>0) then raise Exception.Create('Integer range error');Result:=Trunc(N) end;
function IntField(Data:TJSONObject;const Key:string;Default,Lo,Hi:Integer):Integer;
begin if Data.Find(Key)=nil then Result:=Default else Result:=IntVal(Data.Find(Key),Lo,Hi) end;
function Flag(Data:TJSONObject;const Key:string;Default:Boolean):Boolean;
begin if Data.Find(Key)=nil then Exit(Default);if Data.Find(Key).JSONType<>jtBoolean then raise Exception.Create('Expected Boolean '+Key);
 Result:=Data.Booleans[Key] end;
function FloatField(Data:TJSONObject;const Key:string;Default:Single):Single;
var N:Double;
begin if Data.Find(Key)=nil then Exit(Default);N:=Num(Data.Find(Key));
 if Abs(N)>MaxSingle then raise Exception.Create('Single range error');Result:=N end;
function PointValue(Data:TJSONData):T3DCoordinate;
var A:TJSONArray;I:Integer;
begin A:=Arr(Data,3);if A.Count<>3 then raise Exception.Create('Expected xyz');
 for I:=0 to 2 do if Abs(Num(A[I]))>MaxSingle then raise Exception.Create('Single range error');
 Result:=Point3D(Num(A[0]),Num(A[1]),Num(A[2])) end;
function PointJSON(const P:T3DCoordinate):TJSONArray;
begin Result:=TJSONArray.Create;Result.Add(Double(P.X));Result.Add(Double(P.Y));Result.Add(Double(P.Z)) end;
function PointObject(P:TFreeSubdivisionPoint):TJSONObject;
begin Result:=TJSONObject.Create;Result.Add('position',PointJSON(P.Coordinate));Result.Add('type',Ord(P.VertexType)) end;
function PairJSON(A,B:Integer):TJSONArray;
begin Result:=TJSONArray.Create;Result.Add(A);Result.Add(B) end;
function Key(P:Pointer):string;
begin Result:=IntToHex(PtrUInt(P),SizeOf(Pointer)*2) end;
function Lookup(Map:TFPHashList;P:Pointer):Integer;
begin Result:=PtrUInt(Map.Find(Key(P)))-1 end;
procedure RecordIndex(Map:TFPHashList;P:Pointer;Index:Integer);
begin Map.Add(Key(P),Pointer(PtrUInt(Index+1))) end;

function Metadata:TJSONObject;
begin
 Result:=TJSONObject.Create;
 Result.Add('compiler','FPC '+{$I %FPCVERSION%});Result.Add('cpu',{$I %FPCTARGETCPU%});Result.Add('os',{$I %FPCTARGETOS%});
 Result.Add('single_bytes',SizeOf(Single));Result.Add('double_bytes',SizeOf(Double));Result.Add('extended_bytes',SizeOf(Extended));
 Result.Add('rounding_mode',Ord(GetRoundMode));
 Result.Add('phase','Fresh surface per requested level; original Rebuild performs Subdivide then simultaneous Average; LastAveraging is disabled in this source.');
 Result.Add('identity','Export-local input order. Output points first encountered in control-face/child/oriented-corner order, then ordered curve chains; never coordinate/address sorting.');
 Result.Add('scope','Complete static calculation control net after validated project load. Not full FBM editing/file state; excludes selections, locks, anchors, groups, names of editable objects, project calculations, images and undo.');
end;

function LayerJSON(L:TFreeSubdivisionLayer):TJSONObject;
begin
 Result:=TJSONObject.Create;Result.Add('name',L.ProbeRawName);Result.Add('layer_id',L.LayerID);
 Result.Add('color',LongInt(L.Color));Result.Add('alpha_blend',L.AlphaBlend);
 Result.Add('symmetric',L.Symmetric);Result.Add('surface_visible',L.SurfaceVisible);Result.Add('control_net_visible',L.ControlNetVisible);
 Result.Add('use_for_intersections',L.UseForIntersections);Result.Add('use_in_hydrostatics',L.UseInHydrostatics);
 Result.Add('show_in_linesplan',L.ShowInLinesplan);Result.Add('developable',L.Developable);
 Result.Add('material_density',Double(L.MaterialDensity));Result.Add('thickness',Double(L.Thickness));
end;

function GeometryJSON(Ship:TFreeShip):TJSONObject;
var S:TFreeSubdivisionSurface;Points,Edges,Faces,Layers,Curves,Indices:TJSONArray;Item:TJSONObject;
    I,J:Integer;E:TFreeSubdivisionEdge;F:TFreeSubdivisionControlFace;C:TFreeSubdivisionControlCurve;
begin
 S:=Ship.Surface;Result:=TJSONObject.Create;
 Result.Add('format','FreeShipGeometryInput');Result.Add('version',1);
 Result.Add('subdivision_mode',Ord(S.SubdivisionMode));Result.Add('precision',Ord(Ship.Precision));Result.Add('desired_level',S.DesiredSubdivisionLevel);
 Points:=TJSONArray.Create;Edges:=TJSONArray.Create;Faces:=TJSONArray.Create;Layers:=TJSONArray.Create;Curves:=TJSONArray.Create;
 Result.Add('points',Points);Result.Add('edges',Edges);Result.Add('faces',Faces);Result.Add('layers',Layers);Result.Add('curves',Curves);
 for I:=0 to S.NumberOfControlPoints-1 do Points.Add(PointObject(S.ControlPoint[I]));
 for I:=0 to S.NumberOfLayers-1 do Layers.Add(LayerJSON(S.Layer[I]));
 for I:=0 to S.NumberOfControlEdges-1 do begin
  E:=S.ControlEdge[I];Item:=TJSONObject.Create;Edges.Add(Item);
  Item.Add('points',PairJSON(S.ControlPoints.IndexOf(TFreeSubdivisionControlPoint(E.StartPoint)),S.ControlPoints.IndexOf(TFreeSubdivisionControlPoint(E.EndPoint))));
  Item.Add('crease',E.Crease);Item.Add('curve',S.ControlCurves.IndexOf(E.Curve));
 end;
 for I:=0 to S.NumberOfControlFaces-1 do begin
  F:=S.ControlFace[I];Item:=TJSONObject.Create;Faces.Add(Item);Indices:=TJSONArray.Create;Item.Add('points',Indices);
  for J:=0 to F.NumberOfPoints-1 do Indices.Add(S.ControlPoints.IndexOf(F.Point[J]));
  Item.Add('layer',F.Layer.LayerIndex);
 end;
 for I:=0 to S.NumberOfControlCurves-1 do begin
  C:=S.ControlCurve[I];Item:=TJSONObject.Create;Curves.Add(Item);Indices:=TJSONArray.Create;Item.Add('points',Indices);
  for J:=0 to C.NumberOfControlPoints-1 do Indices.Add(S.ControlPoints.IndexOf(C.ControlPoint[J]));
  Item.Add('visible',C.ProbeRawVisible);
 end;
end;

function NewShip:TFreeShip;
begin Result:=TFreeShip.Create(nil);GlobalFreeShip:=Result;Result.IsRebuildAsyncDisabled:=True;Result.Preferences.Load end;
function LoadModel(const Filename:string):TFreeShip;
var Buffer:TFreeFileBuffer;
begin
 Result:=NewShip;Buffer:=TFreeFileBuffer.Create;
 try
  try Buffer.LoadFromFile(Filename);Result.LoadProject(Buffer,False)
  except Result.Free;raise end;
 finally Buffer.Free end;
end;

function LoadGeometry(G:TJSONObject):TFreeShip;
var Buffer:TFreeFileBuffer;Points,Edges,Faces,Layers,Curves,Indices:TJSONArray;Item:TJSONObject;
    I,J,K:Integer;P:T3DCoordinate;N:Single;E:TFreeSubdivisionEdge;
begin
 if (G.Get('format','')<>'FreeShipGeometryInput') or (G.Get('version',0)<>1) then raise Exception.Create('Unsupported geometry format');
 Points:=Arr(G.Find('points'));Edges:=Arr(G.Find('edges'));Faces:=Arr(G.Find('faces'));Layers:=Arr(G.Find('layers'),10000);Curves:=Arr(G.Find('curves'));
 if Layers.Count=0 then raise Exception.Create('At least one explicit layer is required');
 Result:=NewShip;Buffer:=TFreeFileBuffer.Create;
 try
  try
   Buffer.Version:=CurrentVersion;Buffer.Encoding:='utf8';
   // Feed the real loader, which preserves raw vertex type and crease fields.
   // Editing setters and Initialize would silently normalize this input.
   Buffer.Add(Layers.Count);
   for I:=0 to Layers.Count-1 do begin
    Item:=Obj(Layers[I]);Buffer.Add(Item.Get('name',''));Buffer.Add(IntField(Item,'layer_id',I,Low(Integer),High(Integer)));
    Buffer.Add(IntField(Item,'color',0,Low(Integer),High(Integer)));Buffer.Add(Flag(Item,'surface_visible',True));
    Buffer.Add(Flag(Item,'symmetric',True));Buffer.Add(Flag(Item,'developable',False));
    Buffer.Add(Flag(Item,'use_for_intersections',True));Buffer.Add(Flag(Item,'use_in_hydrostatics',True));
    N:=FloatField(Item,'material_density',0);Buffer.Add(N);N:=FloatField(Item,'thickness',0);Buffer.Add(N);
    Buffer.Add(Flag(Item,'show_in_linesplan',True));Buffer.Add(IntField(Item,'alpha_blend',255,0,255));
   end;
   Buffer.Add(0);Buffer.Add(Points.Count);
   for I:=0 to Points.Count-1 do begin
    Item:=Obj(Points[I]);P:=PointValue(Item.Find('position'));Buffer.Add(P);Buffer.Add(IntField(Item,'type',0,0,3));
    Buffer.Add(False);Buffer.Add(False);
   end;
   Buffer.Add(Edges.Count);
   for I:=0 to Edges.Count-1 do begin
    Item:=Obj(Edges[I]);Indices:=Arr(Item.Find('points'),2);if Indices.Count<>2 then raise Exception.Create('Edge requires two points');
    for J:=0 to 1 do Buffer.Add(IntVal(Indices[J],0,Points.Count-1));Buffer.Add(Flag(Item,'crease',False));Buffer.Add(False);
   end;
   Buffer.Add(Curves.Count);
   for I:=0 to Curves.Count-1 do begin
    Item:=Obj(Curves[I]);Indices:=Arr(Item.Find('points'));Buffer.Add(Indices.Count);
    for J:=0 to Indices.Count-1 do Buffer.Add(IntVal(Indices[J],0,Points.Count-1));Buffer.Add(False);
   end;
   Buffer.Add(Faces.Count);
   for I:=0 to Faces.Count-1 do begin
    Item:=Obj(Faces[I]);Indices:=Arr(Item.Find('points'));Buffer.Add(Indices.Count);
    for J:=0 to Indices.Count-1 do Buffer.Add(IntVal(Indices[J],0,Points.Count-1));
    Buffer.Add(IntField(Item,'layer',0,0,Layers.Count-1));Buffer.Add(False);
   end;
   Buffer.Reset;Result.Surface.LoadBinary(Buffer);
   if Result.Surface.NumberOfControlEdges<>Edges.Count then raise Exception.Create('Input omitted face edges; real loader added edges');
   // Properties absent from the surface binary block are ordinary public fields.
   for I:=0 to Layers.Count-1 do Result.Surface.Layer[I].ControlNetVisible:=Flag(Obj(Layers[I]),'control_net_visible',True);
   for I:=0 to Curves.Count-1 do Result.Surface.ControlCurve[I].Visible:=Flag(Obj(Curves[I]),'visible',True);
   for I:=0 to Edges.Count-1 do begin
    Item:=Obj(Edges[I]);K:=IntField(Item,'curve',-1,-1,Curves.Count-1);E:=Result.Surface.ControlEdge[I];
    if Result.Surface.ControlCurves.IndexOf(E.Curve)<>K then raise Exception.Create('Edge curve membership disagrees with directed curve chain');
   end;
   Result.Precision:=TFreePrecisionType(IntField(G,'precision',0,0,3));
   Result.Surface.SubdivisionMode:=TFreeSubdivisionMode(IntField(G,'subdivision_mode',0,0,1));
   Result.Surface.DesiredSubdivisionLevel:=IntField(G,'desired_level',0,0,4);
  except Result.Free;raise end;
 finally Buffer.Free end;
end;

function Snapshot(Ship:TFreeShip;Level:Integer):TJSONObject;
var S:TFreeSubdivisionSurface;Points,Faces,Edges,Curves,Layers,LoosePoints,LooseEdges,RawFaceLevels:TJSONArray;
    PointMap,FaceMap,EdgeMap,Referenced,ControlUsed:TFPHashList;PointObjects,FaceObjects,EdgeObjects:TList;
    I,J,K,A,B,Idx,Unreferenced:Integer;F:TFreeSubdivisionFace;CF:TFreeSubdivisionControlFace;E:TFreeSubdivisionEdge;
    C:TFreeSubdivisionControlCurve;P,Q:TFreeSubdivisionPoint;Item,Diag,Bounds:TJSONObject;Indices,Neighbors,Knuckles:TJSONArray;
    MinP,MaxP:T3DCoordinate;HasBounds:Boolean;Adjacency:array of Integer;
 function AddPoint(Point:TFreeSubdivisionPoint;UseForBounds:Boolean):Integer;
 begin
  Result:=Lookup(PointMap,Point);
  if Result<0 then begin Result:=PointObjects.Count;PointObjects.Add(Point);RecordIndex(PointMap,Point,Result);Points.Add(PointObject(Point)) end;
  if UseForBounds and (Lookup(Referenced,Point)<0) then begin
   RecordIndex(Referenced,Point,0);if not HasBounds then begin MinP:=Point.Coordinate;MaxP:=MinP;HasBounds:=True end
   else MinMax(Point.Coordinate,MinP,MaxP);
  end;
 end;
 procedure AddEdge(Edge:TFreeSubdivisionEdge);
 begin
  if Edge=nil then raise Exception.Create('Referenced topology edge is missing');
  if Lookup(EdgeMap,Edge)<0 then begin RecordIndex(EdgeMap,Edge,EdgeObjects.Count);EdgeObjects.Add(Edge) end;
 end;
 procedure AddFace(Face:TFreeSubdivisionFace;Root,Child,Layer:Integer);
 var N:Integer;FaceData:TJSONObject;Cycle:TJSONArray;
 begin
  RecordIndex(FaceMap,Face,FaceObjects.Count);FaceObjects.Add(Face);FaceData:=TJSONObject.Create;Faces.Add(FaceData);
  FaceData.Add('root_control_face',Root);FaceData.Add('child_index',Child);FaceData.Add('layer',Layer);
  Cycle:=TJSONArray.Create;FaceData.Add('points',Cycle);
  for N:=0 to Face.NumberOfPoints-1 do Cycle.Add(AddPoint(Face.Point[N],True));
  for N:=0 to Face.NumberOfPoints-1 do AddEdge(S.EdgeExists(Face.Point[N],Face.Point[(N+1) mod Face.NumberOfPoints]));
  RawFaceLevels.Add(Face.ProbeSubdivisionLevel);
 end;
begin
 S:=Ship.Surface;Result:=TJSONObject.Create;
 Points:=TJSONArray.Create;Faces:=TJSONArray.Create;Edges:=TJSONArray.Create;Curves:=TJSONArray.Create;Layers:=TJSONArray.Create;
 LoosePoints:=TJSONArray.Create;LooseEdges:=TJSONArray.Create;RawFaceLevels:=TJSONArray.Create;
 Result.Add('level',Level);Result.Add('points',Points);Result.Add('faces',Faces);Result.Add('edges',Edges);Result.Add('curves',Curves);
 Result.Add('layers',Layers);Result.Add('loose_control_points',LoosePoints);Result.Add('loose_control_edges',LooseEdges);
 Diag:=TJSONObject.Create;Result.Add('oracle_diagnostics',Diag);Diag.Add('raw_face_subdivision_levels',RawFaceLevels);
 PointMap:=TFPHashList.Create;FaceMap:=TFPHashList.Create;EdgeMap:=TFPHashList.Create;Referenced:=TFPHashList.Create;ControlUsed:=TFPHashList.Create;
 PointObjects:=TList.Create;FaceObjects:=TList.Create;EdgeObjects:=TList.Create;HasBounds:=False;MinP:=ZERO;MaxP:=ZERO;
 try
  try
   for I:=0 to S.NumberOfLayers-1 do Layers.Add(LayerJSON(S.Layer[I]));
   if Level=0 then for I:=0 to S.NumberOfControlPoints-1 do AddPoint(S.ControlPoint[I],False);
   for I:=0 to S.NumberOfControlFaces-1 do begin
    CF:=S.ControlFace[I];for J:=0 to CF.NumberOfPoints-1 do if Lookup(ControlUsed,CF.Point[J])<0 then RecordIndex(ControlUsed,CF.Point[J],0);
    if Level=0 then AddFace(CF,I,-1,CF.Layer.LayerIndex)
    else for J:=0 to CF.ChildCount-1 do AddFace(CF.Child[J],I,J,CF.Layer.LayerIndex);
   end;
   for I:=0 to S.NumberOfControlCurves-1 do begin
    C:=S.ControlCurve[I];Item:=TJSONObject.Create;Curves.Add(Item);Indices:=TJSONArray.Create;Knuckles:=TJSONArray.Create;
    Item.Add('points',Indices);Item.Add('visible',C.ProbeRawVisible);Item.Add('free_standing',C.IsFreeStanding);Item.Add('knuckles',Knuckles);
    for J:=0 to C.NumberOfControlPoints-1 do if Lookup(ControlUsed,C.ControlPoint[J])<0 then RecordIndex(ControlUsed,C.ControlPoint[J],0);
    for J:=0 to C.SubdividedPoints.Count-1 do begin
     P:=C.SubdividedPoints[J];Indices.Add(AddPoint(P,True));Knuckles.Add(C.Spline.Knuckle[J]);
     if J>0 then AddEdge(S.EdgeExists(C.SubdividedPoints[J-1],P));
    end;
   end;
   // All edge references are real core objects. Canonicalize endpoint IDs only;
   // retain oriented face cycles above and sorted adjacent face IDs below.
   for I:=0 to EdgeObjects.Count-1 do begin
    E:=TFreeSubdivisionEdge(EdgeObjects[I]);Item:=TJSONObject.Create;Edges.Add(Item);
    A:=Lookup(PointMap,E.StartPoint);B:=Lookup(PointMap,E.EndPoint);
    if (A<0) or (B<0) then raise Exception.Create('Unmapped edge endpoint');
    if A>B then begin K:=A;A:=B;B:=K end;
    Item.Add('points',PairJSON(A,B));Item.Add('crease',E.Crease);Item.Add('curve',S.ControlCurves.IndexOf(E.Curve));
    Item.Add('control_descendant',E.ProbeControlDescendant);Neighbors:=TJSONArray.Create;Item.Add('faces',Neighbors);
    SetLength(Adjacency,E.NumberOfFaces);A:=0;
    for J:=0 to E.NumberOfFaces-1 do begin
     Idx:=Lookup(FaceMap,E.Face[J]);if Idx<0 then Continue;
     K:=A;while (K>0) and (Adjacency[K-1]>Idx) do begin Adjacency[K]:=Adjacency[K-1];Dec(K) end;
     Adjacency[K]:=Idx;Inc(A);
    end;
    for J:=0 to A-1 do Neighbors.Add(Adjacency[J]);
   end;
   for I:=0 to S.NumberOfControlPoints-1 do if Lookup(ControlUsed,S.ControlPoint[I])<0 then begin
    Item:=PointObject(S.ControlPoint[I]);Item.Add('control_point',I);LoosePoints.Add(Item);
   end;
   for I:=0 to S.NumberOfControlEdges-1 do begin
    E:=S.ControlEdge[I];if (E.NumberOfFaces=0) and (E.Curve=nil) then begin
     Item:=TJSONObject.Create;LooseEdges.Add(Item);Item.Add('control_edge',I);
     Item.Add('points',PairJSON(S.ControlPoints.IndexOf(TFreeSubdivisionControlPoint(E.StartPoint)),S.ControlPoints.IndexOf(TFreeSubdivisionControlPoint(E.EndPoint))));
     Item.Add('crease',E.Crease);Item.Add('curve',-1);
    end;
   end;
   Bounds:=TJSONObject.Create;Bounds.Add('min',PointJSON(MinP));Bounds.Add('max',PointJSON(MaxP));Result.Add('bounds',Bounds);Result.Add('empty',not HasBounds);
   Diag.Add('raw_subdiv_point_count',S.NumberOfSubDivPoints);Diag.Add('raw_subdiv_edge_count',S.NumberOfSubDivEdges);
   Unreferenced:=0;for I:=0 to S.NumberOfSubDivPoints-1 do if Lookup(Referenced,S.SubDivPoint[I])<0 then Inc(Unreferenced);
   Diag.Add('unreferenced_subdiv_point_count',Unreferenced);Diag.Add('surface_min',PointJSON(S.Min));Diag.Add('surface_max',PointJSON(S.Max));
   Diag.Add('effective_geometry',GeometryJSON(Ship));
  except Result.Free;raise end;
 finally PointObjects.Free;FaceObjects.Free;EdgeObjects.Free;PointMap.Free;FaceMap.Free;EdgeMap.Free;Referenced.Free;ControlUsed.Free end;
end;

function EvaluateLevel(G:TJSONObject;Level:Integer;const ModelFile:string=''):TJSONObject;
var Ship:TFreeShip;
begin
 if ModelFile='' then Ship:=LoadGeometry(G) else Ship:=LoadModel(ModelFile);
 try Ship.Surface.DesiredSubdivisionLevel:=Level;Ship.Surface.Built:=False;Ship.Surface.Rebuild;Result:=Snapshot(Ship,Level)
 finally Ship.Free end;
end;

function RunCase(Data:TJSONObject):TJSONObject;
var Levels,Results:TJSONArray;I,Level:Integer;G:TJSONObject;Loaded:TFreeShip;
begin
 Result:=TJSONObject.Create;Result.Add('id',Data.Get('id',''));
 try
  G:=Obj(Data.Find('geometry'));Levels:=Arr(Data.Find('levels'),32);Results:=TJSONArray.Create;Result.Add('levels',Results);
  Loaded:=LoadGeometry(G);try Result.Add('loaded_geometry',GeometryJSON(Loaded)) finally Loaded.Free end;
  for I:=0 to Levels.Count-1 do begin Level:=IntVal(Levels[I],0,4);Results.Add(EvaluateLevel(G,Level)) end;
  Result.Add('status','ok');
 except on E:Exception do begin Result.Add('status','error');Result.Add('error',E.ClassName+': '+E.Message) end end;
end;

procedure RunCases(const InputFile,OutputFolder:string);
var Input:TJSONData;Output:TJSONObject;Cases,Results:TJSONArray;I:Integer;
begin
 Input:=ReadJSON(InputFile);Output:=TJSONObject.Create;
 try
  if (Obj(Input).Get('format','')<>'FreeShipNativeSubdivisionCases') or (Obj(Input).Get('version',0)<>1) then raise Exception.Create('Unsupported cases format');
  Cases:=Arr(Obj(Input).Find('cases'),10000);Results:=TJSONArray.Create;
  Output.Add('format','FreeShipNativeSubdivisionResults');Output.Add('version',1);Output.Add('metadata',Metadata);Output.Add('cases',Results);
  for I:=0 to Cases.Count-1 do Results.Add(RunCase(Obj(Cases[I])));
  WriteJSON(OutputFolder+'/results.json',Output);
 finally Input.Free;Output.Free end;
end;

procedure ExportModel(const Filename,OutputFolder:string);
var Ship:TFreeShip;G,Input,Output,CaseInput,CaseResult,Replay,CaseReplay,Coverage:TJSONObject;
    Cases,Results,Levels,Replays,Differences:TJSONArray;I:Integer;Direct,Reconstructed,DirectCompare,ReplayCompare,Diff,RoundTrip:TJSONObject;
    Same,GeometrySame,InputSame:Boolean;
begin
 Ship:=LoadModel(Filename);try G:=GeometryJSON(Ship) finally Ship.Free end;
 Input:=TJSONObject.Create;Output:=TJSONObject.Create;Replay:=TJSONObject.Create;Coverage:=TJSONObject.Create;
 try
  Input.Add('format','FreeShipNativeSubdivisionCases');Input.Add('version',1);Cases:=TJSONArray.Create;Input.Add('cases',Cases);
  CaseInput:=TJSONObject.Create;Cases.Add(CaseInput);CaseInput.Add('id','model');CaseInput.Add('geometry',G);
  Levels:=TJSONArray.Create;CaseInput.Add('levels',Levels);for I:=0 to 3 do Levels.Add(I);
  Output.Add('format','FreeShipNativeSubdivisionResults');Output.Add('version',1);Output.Add('metadata',Metadata);Results:=TJSONArray.Create;Output.Add('cases',Results);
  CaseResult:=TJSONObject.Create;Results.Add(CaseResult);CaseResult.Add('id','model');CaseResult.Add('status','ok');Results:=TJSONArray.Create;CaseResult.Add('levels',Results);
  Replay.Add('format','FreeShipNativeSubdivisionResults');Replay.Add('version',1);Cases:=TJSONArray.Create;Replay.Add('cases',Cases);
  CaseReplay:=TJSONObject.Create;Cases.Add(CaseReplay);CaseReplay.Add('id','model');CaseReplay.Add('status','ok');Replays:=TJSONArray.Create;CaseReplay.Add('levels',Replays);
  Ship:=LoadGeometry(G);try RoundTrip:=GeometryJSON(Ship) finally Ship.Free end;
  try InputSame:=G.AsJSON=RoundTrip.AsJSON finally RoundTrip.Free end;
  Same:=True;GeometrySame:=True;Differences:=TJSONArray.Create;Coverage.Add('replay_diagnostic_differences',Differences);
  for I:=0 to 3 do begin
   Direct:=EvaluateLevel(G,I,Filename);Results.Add(Direct);Reconstructed:=EvaluateLevel(G,I);Replays.Add(Reconstructed);
   Same:=Same and (Direct.AsJSON=Reconstructed.AsJSON);
   // This private debug field is not reset by level-zero Rebuild after a model
   // has already been loaded at another level. Keep the original evidence;
   // compare all mathematical/topological and effective-input fields separately.
   if Obj(Direct.Find('oracle_diagnostics')).Find('raw_face_subdivision_levels').AsJSON <>
      Obj(Reconstructed.Find('oracle_diagnostics')).Find('raw_face_subdivision_levels').AsJSON then begin
    Diff:=TJSONObject.Create;Differences.Add(Diff);Diff.Add('level',I);Diff.Add('field','oracle_diagnostics.raw_face_subdivision_levels');
    Diff.Add('direct',GetJSON(Obj(Direct.Find('oracle_diagnostics')).Find('raw_face_subdivision_levels').AsJSON));
    Diff.Add('reconstructed',GetJSON(Obj(Reconstructed.Find('oracle_diagnostics')).Find('raw_face_subdivision_levels').AsJSON));
   end;
   DirectCompare:=Obj(GetJSON(Direct.AsJSON));ReplayCompare:=Obj(GetJSON(Reconstructed.AsJSON));
   try
    Obj(DirectCompare.Find('oracle_diagnostics')).Delete('raw_face_subdivision_levels');
    Obj(ReplayCompare.Find('oracle_diagnostics')).Delete('raw_face_subdivision_levels');
    GeometrySame:=GeometrySame and (DirectCompare.AsJSON=ReplayCompare.AsJSON);
   finally DirectCompare.Free;ReplayCompare.Free end;
  end;
  Coverage.Add('source_state','Complete static calculation control net after validated project load; all source control points/edges/faces/layers/curves, no screen mesh inference.');
  Coverage.Add('control_points',Arr(G.Find('points')).Count);Coverage.Add('control_edges',Arr(G.Find('edges')).Count);
  Coverage.Add('control_faces',Arr(G.Find('faces')).Count);Coverage.Add('layers',Arr(G.Find('layers')).Count);Coverage.Add('control_curves',Arr(G.Find('curves')).Count);
  Coverage.Add('levels',TJSONArray.Create([0,1,2,3]));Coverage.Add('direct_vs_reconstructed_exact',Same);
  Coverage.Add('control_net_roundtrip_exact',InputSame);Coverage.Add('direct_vs_reconstructed_geometry_exact',GeometrySame);
  WriteJSON(OutputFolder+'/input.json',Input);WriteJSON(OutputFolder+'/results.json',Output);
  WriteJSON(OutputFolder+'/replay-results.json',Replay);WriteJSON(OutputFolder+'/coverage.json',Coverage);
 finally Input.Free;Output.Free;Replay.Free;Coverage.Free end;
end;

begin
 try
  DefaultFormatSettings.DecimalSeparator:='.';Application.Initialize;ApplyLanguageSafely('cn',ExtractFilePath(ParamStr(0))+'locale');
  if ParamStr(3)='export-model' then ExportModel(ParamStr(1),ParamStr(2)) else RunCases(ParamStr(1),ParamStr(2));
  Halt(0);
 except on E:Exception do begin WriteLn(E.ClassName+': '+E.Message);Halt(1) end end;
end.
