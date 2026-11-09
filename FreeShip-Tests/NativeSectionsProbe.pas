program NativeSectionsProbe;
{$mode objfpc}{$H+}{$codepage utf8}
// Original core calls plus read-only callbacks. Never infer topology from coordinates.
uses Interfaces,Forms,SysUtils,Classes,Math,Contnrs,fpjson,jsonparser,Graphics,Types,
 FreeGeometry,FreeShipUnit,FreeFileBuffer,FreeTypes,FreeVersionUnit,FreeSafeLanguage,FreeStringUtils;
{$I NativeSectionsSupport.inc}

var ObservedSurface:TFreeSubdivisionSurface;
    FaceMap,EdgeMap,RootFaceMap:TFPHashList;
    RawSegments,ConnectedCurves,JoinedCurves,SelectedFaces:TJSONArray;

function PlaneValue(Data:TJSONData):T3DPlane;
var A:TJSONArray;I:Integer;
begin A:=Arr(Data,4);if A.Count<>4 then raise Exception.Create('Expected four plane coefficients');
 for I:=0 to 3 do if Abs(Num(A[I]))>MaxSingle then raise Exception.Create('Plane exceeds Single');
 Result:=SetPlane(Num(A[0]),Num(A[1]),Num(A[2]),Num(A[3])) end;
function PlaneJSON(P:T3DPlane):TJSONArray;
begin Result:=TJSONArray.Create;Result.Add(Double(P.a));Result.Add(Double(P.b));Result.Add(Double(P.c));Result.Add(Double(P.d)) end;
function Parameters(Data:TJSONObject):TJSONArray;
var I:Integer;
begin
 if Data.Find('sample_parameters')<>nil then Exit(Arr(GetJSON(Data.Find('sample_parameters').AsJSON)));
 Result:=TJSONArray.Create;for I:=0 to 8 do Result.Add(I/8.0);
end;

function ChainJSON(S:TFreeSpline;SampleParameters:TJSONArray):TJSONObject;
var Points,Knuckles,Knots,Values,Firsts,Seconds:TJSONArray;Samples:TJSONObject;I:Integer;T:Double;ST:Single;
begin
 Result:=TJSONObject.Create;Points:=TJSONArray.Create;Knuckles:=TJSONArray.Create;
 Result.Add('points',Points);Result.Add('knuckles',Knuckles);
 // These getters read fields only; stage callbacks must not rebuild the spline.
 for I:=0 to S.NumberOfPoints-1 do begin Points.Add(PointJSON(S.Point[I]));Knuckles.Add(S.Knuckle[I]) end;
 Result.Add('fragments',S.ProbeRawFragments);
 Result.Add('closed',(S.NumberOfPoints>1) and (DistPP3D(S.Point[0],S.Point[S.NumberOfPoints-1])<1e-5));
 if SampleParameters<>nil then begin
  S.Rebuild;Samples:=TJSONObject.Create;Result.Add('samples',Samples);Knots:=TJSONArray.Create;Samples.Add('knots',Knots);
  if S.NumberOfPoints>1 then for I:=0 to S.NumberOfPoints-1 do Knots.Add(Double(S.Parameter[I]));
  if S.NumberOfPoints=1 then Samples.Add('unsupported',TJSONArray.Create(['knots']));
  Samples.Add('parameters',GetJSON(SampleParameters.AsJSON));Samples.Add('total_length',Double(S.TotalLength));Samples.Add('fragments',S.Fragments);
  Values:=TJSONArray.Create;Firsts:=TJSONArray.Create;Seconds:=TJSONArray.Create;
  Samples.Add('values',Values);Samples.Add('first',Firsts);Samples.Add('second',Seconds);
  for I:=0 to SampleParameters.Count-1 do begin
   T:=Num(SampleParameters[I]);ST:=T;Values.Add(PointJSON(S.Value(T)));Firsts.Add(PointJSON(S.FirstDerive(ST)));Seconds.Add(PointJSON(S.SecondDerive(ST)));
  end;
 end;
end;

function ChainsJSON(List:TFasterListTFreeSpline;SampleParameters:TJSONArray):TJSONArray;
var I:Integer;
begin Result:=TJSONArray.Create;for I:=0 to List.Count-1 do Result.Add(ChainJSON(List[I],SampleParameters)) end;

procedure ObserveFaces(Faces:TFasterListTFreeSubdivisionFace);
var I:Integer;
begin for I:=0 to Faces.Count-1 do SelectedFaces.Add(ObservedSurface.ControlFaces.IndexOf(TFreeSubdivisionControlFace(Faces[I]))) end;
procedure ObserveSegment(Face:TFreeSubdivisionFace;StartPoint,EndPoint:T3DCoordinate;
 StartKnuckle,EndKnuckle:Boolean;StartEdge,EndEdge:TFreeSubdivisionEdge);
var Item,EndpointData:TJSONObject;
begin
 Item:=TJSONObject.Create;RawSegments.Add(Item);Item.Add('face',Lookup(FaceMap,Face));Item.Add('root_control_face',Lookup(RootFaceMap,Face));
 EndpointData:=TJSONObject.Create;Item.Add('start',EndpointData);EndpointData.Add('position',PointJSON(StartPoint));EndpointData.Add('knuckle',StartKnuckle);EndpointData.Add('edge',Lookup(EdgeMap,StartEdge));
 EndpointData:=TJSONObject.Create;Item.Add('end',EndpointData);EndpointData.Add('position',PointJSON(EndPoint));EndpointData.Add('knuckle',EndKnuckle);EndpointData.Add('edge',Lookup(EdgeMap,EndEdge));
end;
procedure ObserveStage(Stage:Integer;List:TFasterListTFreeSpline);
begin
 if Stage=0 then begin ConnectedCurves.Free;ConnectedCurves:=ChainsJSON(List,nil) end
 else begin JoinedCurves.Free;JoinedCurves:=ChainsJSON(List,nil) end;
end;

procedure PrepareIdentity(S:TFreeSubdivisionSurface;Level:Integer);
var I,J,K,FaceIndex:Integer;F:TFreeSubdivisionFace;C:TFreeSubdivisionControlFace;E:TFreeSubdivisionEdge;
begin
 ObservedSurface:=S;FaceMap:=TFPHashList.Create;EdgeMap:=TFPHashList.Create;RootFaceMap:=TFPHashList.Create;FaceIndex:=0;
 for I:=0 to S.NumberOfControlFaces-1 do begin
  C:=S.ControlFace[I];
  if Level=0 then begin
   RecordIndex(FaceMap,C,FaceIndex);RecordIndex(RootFaceMap,C,I);Inc(FaceIndex);
   for K:=0 to C.NumberOfPoints-1 do begin
    E:=S.EdgeExists(C.Point[K],C.Point[(K+1) mod C.NumberOfPoints]);
    if (E<>nil) and (Lookup(EdgeMap,E)<0) then RecordIndex(EdgeMap,E,EdgeMap.Count);
   end;
  end else for J:=0 to C.ChildCount-1 do begin
   F:=C.Child[J];RecordIndex(FaceMap,F,FaceIndex);RecordIndex(RootFaceMap,F,I);Inc(FaceIndex);
   for K:=0 to F.NumberOfPoints-1 do begin
    E:=S.EdgeExists(F.Point[K],F.Point[(K+1) mod F.NumberOfPoints]);
    if (E<>nil) and (Lookup(EdgeMap,E)<0) then RecordIndex(EdgeMap,E,EdgeMap.Count);
   end;
  end;
 end;
end;

function RunPlane(G,Query:TJSONObject;Level:Integer;Samples:TJSONArray;const ModelFile:string=''):TJSONObject;
var Ship:TFreeShip;List:TFasterListTFreeSpline;Faces:TFasterListTFreeSubdivisionFace;Plane:T3DPlane;
    Ids:TJSONArray;I,Index:Integer;Found:Boolean;Route:string;
begin
 Result:=TJSONObject.Create;Result.Add('id',Query.Get('id',''));Ship:=nil;List:=nil;Faces:=nil;
 RawSegments:=nil;ConnectedCurves:=nil;JoinedCurves:=nil;SelectedFaces:=nil;FaceMap:=nil;EdgeMap:=nil;RootFaceMap:=nil;
 try
  try
   if ModelFile='' then Ship:=LoadGeometry(G) else Ship:=LoadModel(ModelFile);
   Ship.Surface.DesiredSubdivisionLevel:=Level;Ship.Surface.Built:=False;Ship.Surface.Rebuild;
   Plane:=PlaneValue(Query.Find('coefficients'));Route:=Query.Get('route','surface');
   PrepareIdentity(Ship.Surface,Level);RawSegments:=TJSONArray.Create;SelectedFaces:=TJSONArray.Create;
   List:=TFasterListTFreeSpline.Create(False,False);
   if Flag(Query,'observe',True) then begin
    NativeSectionsFacesObserver:=@ObserveFaces;NativeSectionsSegmentObserver:=@ObserveSegment;NativeSectionsStageObserver:=@ObserveStage;
   end;
   if Route='surface' then Found:=Ship.Surface.IntersectPlane(Plane,Flag(Query,'hydrostatics_layers_only',False),List)
   else if Route='faces' then begin
    Faces:=TFasterListTFreeSubdivisionFace.Create(False,False);Ids:=Arr(Query.Find('control_faces'));
    for I:=0 to Ids.Count-1 do begin Index:=IntVal(Ids[I],0,Ship.Surface.NumberOfControlFaces-1);Faces.Add(Ship.Surface.ControlFace[Index]) end;
    Ship.Surface.CalculateIntersections(Plane,Faces,List);Found:=List.Count>0;
   end else raise Exception.Create('Unknown section route');
   NativeSectionsFacesObserver:=nil;NativeSectionsSegmentObserver:=nil;NativeSectionsStageObserver:=nil;
   if ConnectedCurves=nil then ConnectedCurves:=TJSONArray.Create;
   Result.Add('join_invoked',JoinedCurves<>nil);
   if JoinedCurves=nil then JoinedCurves:=Arr(GetJSON(ConnectedCurves.AsJSON));
   Result.Add('segments',RawSegments);RawSegments:=nil;Result.Add('selected_control_faces',SelectedFaces);SelectedFaces:=nil;
   Result.Add('connected_curves',ConnectedCurves);ConnectedCurves:=nil;Result.Add('joined_curves',JoinedCurves);JoinedCurves:=nil;
   Result.Add('curves',ChainsJSON(List,Samples));Result.Add('found',Found);Result.Add('status','ok');Result.Add('observer_enabled',Flag(Query,'observe',True));
   Result.Add('unsupported',TJSONArray.Create(['endpoint_vertex_ids']));
  except on E:Exception do begin Result.Add('status','error');Result.Add('error',E.ClassName+': '+E.Message) end end;
 finally
  NativeSectionsFacesObserver:=nil;NativeSectionsSegmentObserver:=nil;NativeSectionsStageObserver:=nil;
  if List<>nil then begin for I:=0 to List.Count-1 do List[I].Free;List.Free end;
  Faces.Free;FaceMap.Free;EdgeMap.Free;RootFaceMap.Free;RawSegments.Free;ConnectedCurves.Free;JoinedCurves.Free;SelectedFaces.Free;Ship.Free;
 end;
end;

function RunJoin(Data:TJSONObject;Samples:TJSONArray):TJSONObject;
var List:TFasterListTFreeSpline;Chains,Points,Knuckles:TJSONArray;S:TFreeSpline;Item:TJSONObject;I,J:Integer;Tolerance:Single;
begin
 Result:=TJSONObject.Create;List:=TFasterListTFreeSpline.Create(False,False);
 try
  try
   Chains:=Arr(Data.Find('chains'),10000);
   for I:=0 to Chains.Count-1 do begin
    Item:=Obj(Chains[I]);Points:=Arr(Item.Find('points'));Knuckles:=Arr(Item.Find('knuckles'));
    if Points.Count<>Knuckles.Count then raise Exception.Create('Knuckle count mismatch');
    S:=TFreeSpline.Create(nil);List.Add(S);
    for J:=0 to Points.Count-1 do begin S.Add(PointValue(Points[J]));
     if Knuckles[J].JSONType<>jtBoolean then raise Exception.Create('Expected Boolean knuckle');S.Knuckle[J]:=Knuckles[J].AsBoolean end;
    if Item.Find('fragments')<>nil then S.Fragments:=IntVal(Item.Find('fragments'),0,1000000);
   end;
   Result.Add('input_chains',ChainsJSON(List,nil));Tolerance:=FloatField(Data,'join_error',0.01);
   JoinSplineSegments(Tolerance,Flag(Data,'force_to_one',False),List);
   Result.Add('curves',ChainsJSON(List,Samples));Result.Add('status','ok');
  except on E:Exception do begin Result.Add('status','error');Result.Add('error',E.ClassName+': '+E.Message) end end;
 finally for I:=0 to List.Count-1 do List[I].Free;List.Free end;
end;

function RunSimplify(Data:TJSONObject;Samples:TJSONArray):TJSONObject;
var S:TFreeSpline;Item:TJSONObject;Points,Knuckles,Curves:TJSONArray;I:Integer;Done:Boolean;
begin
 Result:=TJSONObject.Create;S:=TFreeSpline.Create(nil);
 try
  Item:=Obj(Data.Find('chain'));Points:=Arr(Item.Find('points'));Knuckles:=Arr(Item.Find('knuckles'));
  if Points.Count<>Knuckles.Count then raise Exception.Create('Knuckle count mismatch');
  for I:=0 to Points.Count-1 do begin S.Add(PointValue(Points[I]));S.Knuckle[I]:=Knuckles[I].AsBoolean end;
  S.Fragments:=IntVal(Item.Find('fragments'),0,1000000);
  if Flag(Data,'prebuild',True) then S.Rebuild;
  Done:=S.Simplify(FloatField(Data,'criterion',2));
  Curves:=TJSONArray.Create;Curves.Add(ChainJSON(S,Samples));Result.Add('curves',Curves);
  Result.Add('simplified',Done);Result.Add('status','ok');
 finally S.Free end;
end;

{$I NativeLinesplanProbe.inc}

function RunCaseUncaught(Data:TJSONObject;const ModelFile:string):TJSONObject;
var Samples,Planes,Results:TJSONArray;I,Level:Integer;Kind:string;
begin
 Samples:=nil;
 try
  Samples:=Parameters(Data);Kind:=Data.Get('kind','surface');
  if Kind='join' then Result:=RunJoin(Data,Samples)
  else if Kind='simplify' then Result:=RunSimplify(Data,Samples)
  else if Kind='linesplan' then Result:=RunLinesplan(Data)
  else begin
   Result:=TJSONObject.Create;Results:=TJSONArray.Create;Result.Add('planes',Results);
   try
    Level:=IntField(Data,'level',1,0,4);Planes:=Arr(Data.Find('planes'),10000);
    for I:=0 to Planes.Count-1 do Results.Add(RunPlane(Obj(Data.Find('geometry')),Obj(Planes[I]),Level,Samples,ModelFile));
    Result.Add('status','ok');
   except on E:Exception do begin Result.Add('status','error');Result.Add('error',E.ClassName+': '+E.Message) end end;
  end;
  Result.Add('id',Data.Get('id',''));Result.Add('kind',Kind);
 finally Samples.Free end;
end;

function RunCase(Data:TJSONObject;const ModelFile:string=''):TJSONObject;
begin
 try Result:=RunCaseUncaught(Data,ModelFile)
 except on E:Exception do begin
  Result:=TJSONObject.Create;Result.Add('id',Data.Get('id',''));Result.Add('kind',Data.Get('kind','surface'));
  Result.Add('status','error');Result.Add('error',E.ClassName+': '+E.Message);
 end end;
end;

function SectionsMetadata:TJSONObject;
begin
 Result:=Metadata;
 Result.Strings['phase']:='Fresh real Surface per query and fresh empty destination list. Surface.IntersectPlane/CalculateIntersections/JoinSplineSegments are unchanged except read-only callbacks.';
 Result.Strings['scope']:='Raw section candidates only; no mirroring, Intersection.Rebuild memoization, Simplify(2.0), hydrostatic integration or exports.';
 Result.Add('closed_rule','Original DistPP3D(first,last)<1e-5, with at least two points');
 Result.Add('join_error_units','JoinSplineSegments compares squared endpoint distance directly with join_error');
 Result.Add('callback_access','Only raw points/knuckles/fragments and existing object references; no rebuild-triggering getter in callbacks. Final result sampling is separate.');
end;

procedure RunCases(const InputFile,OutputFolder:string);
var Input:TJSONData;Output:TJSONObject;Cases,Results:TJSONArray;I:Integer;
begin Input:=ReadJSON(InputFile);Output:=TJSONObject.Create;
 try
  if (Obj(Input).Get('format','')<>'FreeShipNativeSectionsCases') or (Obj(Input).Get('version',0)<>1) then raise Exception.Create('Unsupported cases format');
  Cases:=Arr(Obj(Input).Find('cases'),10000);Results:=TJSONArray.Create;
  Output.Add('format','FreeShipNativeSectionsResults');Output.Add('version',1);Output.Add('metadata',SectionsMetadata);Output.Add('cases',Results);
  for I:=0 to Cases.Count-1 do Results.Add(RunCase(Obj(Cases[I])));
  WriteJSON(OutputFolder+'/results.json',Output);
 finally Input.Free;Output.Free end;
end;

procedure ExportModel(const Filename,OutputFolder:string);
var Ship:TFreeShip;Input,Output,Replay,CaseData,G,Query,Coverage,Direct,Reconstructed:TJSONObject;
    Cases,Planes,Results,Replays:TJSONArray;Kind,I,Count,Saved:Integer;Section:TFreeIntersection;Name:string;Mid,Minimum,Maximum:T3DCoordinate;
 procedure AddPlane(const Id,Origin:string;P:T3DPlane);
 begin Query:=TJSONObject.Create;Planes.Add(Query);Query.Add('id',Id);Query.Add('origin',Origin);Query.Add('coefficients',PlaneJSON(P));Query.Add('route','surface');Query.Add('hydrostatics_layers_only',False) end;
begin
 Ship:=LoadModel(Filename);Input:=TJSONObject.Create;Output:=TJSONObject.Create;Replay:=TJSONObject.Create;Coverage:=TJSONObject.Create;
 try
  G:=GeometryJSON(Ship);Input.Add('format','FreeShipNativeSectionsCases');Input.Add('version',1);Cases:=TJSONArray.Create;Input.Add('cases',Cases);
  CaseData:=TJSONObject.Create;Cases.Add(CaseData);CaseData.Add('id','model');CaseData.Add('kind','surface');CaseData.Add('geometry',G);
  CaseData.Add('level',Ship.Surface.DesiredSubdivisionLevel);Planes:=TJSONArray.Create;CaseData.Add('planes',Planes);Saved:=0;
  for Kind:=0 to 3 do begin
   case Kind of 0:begin Count:=Ship.NumberOfStations;Name:='station' end;1:begin Count:=Ship.NumberOfButtocks;Name:='buttock' end;
    2:begin Count:=Ship.NumberOfWaterlines;Name:='waterline' end;else begin Count:=Ship.NumberOfDiagonals;Name:='diagonal' end end;
   for I:=0 to Count-1 do begin
    case Kind of 0:Section:=Ship.Station[I];1:Section:=Ship.Buttock[I];2:Section:=Ship.Waterline[I];else Section:=Ship.Diagonal[I] end;
    AddPlane(Name+'-'+IntToStr(I),'saved-section',Section.Plane);Inc(Saved);
   end;
  end;
  Minimum:=Ship.Surface.Min;Maximum:=Ship.Surface.Max;Mid:=MidPoint(Minimum,Maximum);
  if Saved=0 then begin
   AddPlane('fixture-x','fixture-midpoint',PlanePointNormal(Mid,Point3D(1,0,0)));
   AddPlane('fixture-y','fixture-midpoint',PlanePointNormal(Mid,Point3D(0,1,0)));
   AddPlane('fixture-z','fixture-midpoint',PlanePointNormal(Mid,Point3D(0,0,1)));
  end;
  AddPlane('fixture-outside','fixture-outside',SetPlane(1,0,0,-Maximum.X-1));
  Coverage.Add('saved_section_count',Saved);Coverage.Add('query_count',Planes.Count);Coverage.Add('actual_level',Ship.Surface.DesiredSubdivisionLevel);
  Coverage.Add('source_simplify_intersections',Ship.ProjectSettings.ProjectSimplifyIntersections);Coverage.Add('simplification_applied',False);
  Coverage.Add('scope','All saved section planes are evaluated by Surface.IntersectPlane; no upper-layer Simplify or mirroring. Models with no saved sections receive explicit private midpoint fixtures.');
  Output.Add('format','FreeShipNativeSectionsResults');Output.Add('version',1);Output.Add('metadata',SectionsMetadata);Results:=TJSONArray.Create;Output.Add('cases',Results);
  Replay.Add('format','FreeShipNativeSectionsResults');Replay.Add('version',1);Replays:=TJSONArray.Create;Replay.Add('cases',Replays);
  Direct:=RunCase(CaseData,Filename);Results.Add(Direct);Reconstructed:=RunCase(CaseData);Replays.Add(Reconstructed);
  Coverage.Add('direct_vs_reconstructed_exact',Direct.AsJSON=Reconstructed.AsJSON);
  WriteJSON(OutputFolder+'/input.json',Input);WriteJSON(OutputFolder+'/results.json',Output);WriteJSON(OutputFolder+'/replay-results.json',Replay);WriteJSON(OutputFolder+'/coverage.json',Coverage);
 finally Ship.Free;Input.Free;Output.Free;Replay.Free;Coverage.Free end;
end;

begin
 try
  DefaultFormatSettings.DecimalSeparator:='.';Application.Initialize;ApplyLanguageSafely('cn',ExtractFilePath(ParamStr(0))+'locale');
  if ParamStr(3)='export-model' then ExportModel(ParamStr(1),ParamStr(2)) else RunCases(ParamStr(1),ParamStr(2));Halt(0);
 except on E:Exception do begin WriteLn(E.ClassName+': '+E.Message);Halt(1) end end;
end.
