program NativeGeometryProbe;
{$mode objfpc}{$H+}{$codepage utf8}
// Test-only observer of the original geometry routines. No replacement formulas.
uses Interfaces,Forms,SysUtils,Classes,Math,fpjson,jsonparser,FreeGeometry,
 FreeShipUnit,FreeFileBuffer,FreeTypes,FreeSafeLanguage;

var Ship:TFreeShip;

procedure WriteJSON(const Filename:string;Data:TJSONData);
var Stream:TFileStream;Value:UTF8String;
begin
 Value:=Data.AsJSON;Stream:=TFileStream.Create(Filename,fmCreate);
 try if Length(Value)>0 then Stream.WriteBuffer(Value[1],Length(Value)) finally Stream.Free end;
end;

function ReadJSON(const Filename:string):TJSONData;
var Stream:TFileStream;
begin
 Stream:=TFileStream.Create(Filename,fmOpenRead or fmShareDenyWrite);
 try Result:=GetJSON(Stream) finally Stream.Free end;
end;

function Number(Data:TJSONData):Double;
begin
 if (Data=nil) or (Data.JSONType<>jtNumber) then raise Exception.Create('Expected a number');
 Result:=Data.AsFloat;
 if IsNan(Result) or IsInfinite(Result) then raise Exception.Create('Expected a finite number');
end;

function IntegerNumber(Data:TJSONData;Minimum,Maximum:Integer):Integer;
var N:Double;
begin
 N:=Number(Data);
 if (Frac(N)<>0) or (N<Minimum) or (N>Maximum) then raise Exception.Create('Invalid integer range');
 Result:=Trunc(N);
end;

function ArrayValue(Data:TJSONData;Maximum:Integer=100000):TJSONArray;
begin
 if (Data=nil) or (Data.JSONType<>jtArray) then raise Exception.Create('Expected an array');
 Result:=TJSONArray(Data);
 if Result.Count>Maximum then raise Exception.Create('Array is too large');
end;

function PointValue(Data:TJSONData):T3DCoordinate;
var A:TJSONArray;I:Integer;
begin
 A:=ArrayValue(Data,3);if A.Count<>3 then raise Exception.Create('Expected three coordinates');
 for I:=0 to 2 do if Abs(Number(A[I]))>MaxSingle then raise Exception.Create('Coordinate exceeds Single');
 Result.X:=Number(A[0]);Result.Y:=Number(A[1]);Result.Z:=Number(A[2]);
end;

function PlaneValue(Data:TJSONData):T3DPlane;
var A:TJSONArray;I:Integer;
begin
 A:=ArrayValue(Data,4);if A.Count<>4 then raise Exception.Create('Expected four plane coefficients');
 for I:=0 to 3 do if Abs(Number(A[I]))>MaxSingle then raise Exception.Create('Plane exceeds Single');
 Result:=SetPlane(Number(A[0]),Number(A[1]),Number(A[2]),Number(A[3]));
end;

function PointJSON(const P:T3DCoordinate):TJSONArray;
begin Result:=TJSONArray.Create;Result.Add(Double(P.X));Result.Add(Double(P.Y));Result.Add(Double(P.Z)) end;

function PlaneJSON(const P:T3DPlane):TJSONArray;
begin Result:=TJSONArray.Create;Result.Add(Double(P.a));Result.Add(Double(P.b));Result.Add(Double(P.c));Result.Add(Double(P.d)) end;

function Metadata:TJSONObject;
begin
 Result:=TJSONObject.Create;
 Result.Add('compiler','FPC '+{$I %FPCVERSION%});
 Result.Add('cpu',{$I %FPCTARGETCPU%});Result.Add('os',{$I %FPCTARGETOS%});
 Result.Add('single_bytes',SizeOf(Single));Result.Add('double_bytes',SizeOf(Double));Result.Add('extended_bytes',SizeOf(Extended));
 Result.Add('rounding_mode',Ord(GetRoundMode));Result.Add('precision_mode',Ord(GetPrecisionMode));
 Result.Add('value_parameter','JSON Double promoted to Extended');
 Result.Add('derivative_parameter','JSON Double rounded to Single');
 Result.Add('scope','Original FreeGeometry and FreeSpline functions, no model/renderer snapshot oracle');
end;

function SampleSpline(Spline:TFreeSpline;Data:TJSONObject):TJSONObject;
var Knots,Values,Firsts,Seconds,Cuts,Ps,Planes,OutputPoints,OutputParameters:TJSONArray;
    I,J:Integer;T:Double;SingleT:TFloatType;Plane:T3DPlane;Cut:TJSONObject;
    Intersections:TFreeIntersectionData;Found:Boolean;
begin
 Result:=TJSONObject.Create;
 try
  Spline.Rebuild;
  Knots:=TJSONArray.Create;Result.Add('knots',Knots);
  if Spline.NumberOfPoints>1 then
   for I:=0 to Spline.NumberOfPoints-1 do Knots.Add(Double(Spline.Parameter[I]));
  if Spline.NumberOfPoints=1 then Result.Add('unsupported',TJSONArray.Create(['knots']));
  Result.Add('total_length',Double(Spline.TotalLength));Result.Add('fragments',Spline.Fragments);
  Values:=TJSONArray.Create;Firsts:=TJSONArray.Create;Seconds:=TJSONArray.Create;
  Result.Add('values',Values);Result.Add('first',Firsts);Result.Add('second',Seconds);
  if Data.Find('parameters')<>nil then begin
   Ps:=ArrayValue(Data.Find('parameters'));
   for I:=0 to Ps.Count-1 do begin
    T:=Number(Ps[I]);SingleT:=T;
    Values.Add(PointJSON(Spline.Value(T)));
    Firsts.Add(PointJSON(Spline.FirstDerive(SingleT)));
    Seconds.Add(PointJSON(Spline.SecondDerive(SingleT)));
   end;
  end;
  Cuts:=TJSONArray.Create;Result.Add('cuts',Cuts);
  // Reuse this output across planes; the original routine owns clearing it.
  Intersections.NumberOfIntersections:=0;Intersections.Points:=nil;Intersections.Parameters:=nil;
  if Data.Find('planes')<>nil then begin
   Planes:=ArrayValue(Data.Find('planes'),1024);
   for I:=0 to Planes.Count-1 do begin
    Plane:=PlaneValue(Planes[I]);Found:=Spline.IntersectPlane(Plane,Intersections);
    Cut:=TJSONObject.Create;Cuts.Add(Cut);Cut.Add('found',Found);
    Cut.Add('count',Intersections.NumberOfIntersections);
    OutputPoints:=TJSONArray.Create;OutputParameters:=TJSONArray.Create;
    Cut.Add('points',OutputPoints);Cut.Add('parameters',OutputParameters);
    for J:=0 to Intersections.NumberOfIntersections-1 do begin
     OutputPoints.Add(PointJSON(Intersections.Points[J]));OutputParameters.Add(Double(Intersections.Parameters[J]));
    end;
   end;
  end;
 except Result.Free;raise end;
end;

function SplineCase(Data:TJSONObject):TJSONObject;
var Spline:TFreeSpline;Points,Knuckles,Mutations,Steps:TJSONArray;I,Index:Integer;Change:TJSONObject;Op:string;
begin
 Spline:=TFreeSpline.Create(nil);
 try
  Points:=ArrayValue(Data.Find('points'),100000);
  Knuckles:=nil;
  if Data.Find('knuckles')<>nil then begin
   Knuckles:=ArrayValue(Data.Find('knuckles'));
   if Knuckles.Count<>Points.Count then raise Exception.Create('Knuckle count mismatch');
  end;
  for I:=0 to Points.Count-1 do begin
   Spline.Add(PointValue(Points[I]));
   if Knuckles<>nil then begin
    if Knuckles[I].JSONType<>jtBoolean then raise Exception.Create('Expected a Boolean knuckle');
    Spline.Knuckle[I]:=Knuckles.Booleans[I];
   end;
  end;
  if Data.Find('fragments')<>nil then Spline.Fragments:=IntegerNumber(Data.Find('fragments'),0,100000);
  Result:=SampleSpline(Spline,Data);
  try
   if Data.Find('mutations')<>nil then begin
    Mutations:=ArrayValue(Data.Find('mutations'),1024);Steps:=TJSONArray.Create;Result.Add('mutations',Steps);
    for I:=0 to Mutations.Count-1 do begin
     if Mutations[I].JSONType<>jtObject then raise Exception.Create('Invalid mutation');
     Change:=TJSONObject(Mutations[I]);Op:=Change.Get('op','');
     if (Op='set_point') or (Op='set_knuckle') then begin
      Index:=IntegerNumber(Change.Find('index'),0,Spline.NumberOfPoints-1);
      if Op='set_point' then Spline.Point[Index]:=PointValue(Change.Find('point'))
      else begin
       if (Change.Find('value')=nil) or (Change.Find('value').JSONType<>jtBoolean) then raise Exception.Create('Expected a Boolean knuckle');
       Spline.Knuckle[Index]:=Change.Booleans['value'];
      end;
     end else if Op='set_fragments' then Spline.Fragments:=IntegerNumber(Change.Find('value'),0,100000)
     else raise Exception.Create('Unsupported spline mutation '+Op);
     Steps.Add(SampleSpline(Spline,Data));
    end;
   end;
  except Result.Free;raise end;
 finally Spline.Free end;
end;

function PlaneCase(Data:TJSONObject):TJSONObject;
var Plane:T3DPlane;Queries,Eval,Proj,Mirrors,Boxes,Checks:TJSONArray;I:Integer;Item:TJSONObject;P:T3DCoordinate;
begin
 if Data.Find('coefficients')<>nil then Plane:=PlaneValue(Data.Find('coefficients'))
 else if Data.Find('points')<>nil then begin
  Queries:=ArrayValue(Data.Find('points'),3);if Queries.Count<>3 then raise Exception.Create('Expected three plane points');
  Plane:=PlanePPP(PointValue(Queries[0]),PointValue(Queries[1]),PointValue(Queries[2]));
 end else Plane:=PlanePointNormal(PointValue(Data.Find('point')),PointValue(Data.Find('normal')));
 Result:=TJSONObject.Create;
 try
  Result.Add('coefficients',PlaneJSON(Plane));Eval:=TJSONArray.Create;Proj:=TJSONArray.Create;Mirrors:=TJSONArray.Create;
  Result.Add('evaluations',Eval);Result.Add('projections',Proj);Result.Add('mirrors',Mirrors);
  if Data.Find('query_points')<>nil then begin
   Queries:=ArrayValue(Data.Find('query_points'));
   for I:=0 to Queries.Count-1 do begin
    P:=PointValue(Queries[I]);Eval.Add(Double(DistancePointToPlane(P,Plane)));
    Proj.Add(PointJSON(ProjectPointOnPlane(P,Plane)));Mirrors.Add(PointJSON(MirrorPlane(P,Plane)));
   end;
  end;
  Checks:=TJSONArray.Create;Result.Add('boxes',Checks);
  if Data.Find('boxes')<>nil then begin
   Boxes:=ArrayValue(Data.Find('boxes'));
   for I:=0 to Boxes.Count-1 do begin
    if Boxes[I].JSONType<>jtObject then raise Exception.Create('Invalid box');
    Item:=TJSONObject(Boxes[I]);Checks.Add(PlaneIntersectsBox(PointValue(Item.Find('min')),PointValue(Item.Find('max')),Plane));
   end;
  end;
  Result.Add('unsupported',TJSONArray.Create(['lines']));
 except Result.Free;raise end;
end;

function RunCase(Data:TJSONObject):TJSONObject;
var Kind,Id:string;
begin
 Kind:=Data.Get('kind','');Id:=Data.Get('id','');
 try
  if Kind='spline' then Result:=SplineCase(Data)
  else if Kind='plane' then Result:=PlaneCase(Data)
  else raise Exception.Create('Unsupported case kind');
  Result.Add('status','ok');
 except on E:Exception do begin
  Result:=TJSONObject.Create;Result.Add('status','error');Result.Add('error',E.ClassName+': '+E.Message);
 end end;
 Result.Add('id',Id);Result.Add('kind',Kind);
end;

procedure RunCases(const InputFile,OutputFolder:string);
var Root:TJSONData;InputCases,OutputCases:TJSONArray;Output:TJSONObject;I:Integer;
begin
 Root:=ReadJSON(InputFile);Output:=TJSONObject.Create;
 try
  if Root.JSONType<>jtObject then raise Exception.Create('Invalid input root');
  if (TJSONObject(Root).Get('format','')<>'FreeShipNativeGeometryCases') or (TJSONObject(Root).Get('version',0)<>1) then raise Exception.Create('Unsupported cases format');
  InputCases:=ArrayValue(TJSONObject(Root).Find('cases'),10000);OutputCases:=TJSONArray.Create;
  Output.Add('format','FreeShipNativeGeometryResults');Output.Add('version',1);Output.Add('metadata',Metadata);Output.Add('cases',OutputCases);
  for I:=0 to InputCases.Count-1 do begin
   if InputCases[I].JSONType<>jtObject then raise Exception.Create('Invalid case object');
   OutputCases.Add(RunCase(TJSONObject(InputCases[I])));
  end;
  WriteJSON(OutputFolder+'/results.json',Output);
 finally Root.Free;Output.Free end;
end;

procedure LoadModel(const Filename:string);
var Buffer:TFreeFileBuffer;
begin
 Ship:=TFreeShip.Create(nil);GlobalFreeShip:=Ship;Ship.IsRebuildAsyncDisabled:=True;Ship.Preferences.Load;
 Buffer:=TFreeFileBuffer.Create;
 try Buffer.LoadFromFile(Filename);Ship.LoadProject(Buffer,False) finally Buffer.Free end;
 Ship.RebuildModel;
end;

function SplineInput(Spline:TFreeSpline;const Id:string):TJSONObject;
var Points,Knuckles,Parameters,Planes:TJSONArray;I:Integer;MinP,MaxP,P:T3DCoordinate;
begin
 Result:=TJSONObject.Create;Result.Add('id',Id);Result.Add('kind','spline');
 Points:=TJSONArray.Create;Knuckles:=TJSONArray.Create;Parameters:=TJSONArray.Create;Planes:=TJSONArray.Create;
 Result.Add('points',Points);Result.Add('knuckles',Knuckles);Result.Add('parameters',Parameters);Result.Add('planes',Planes);
 Spline.Rebuild;Result.Add('fragments',Spline.Fragments);
 MinP:=Point3D(MaxSingle,MaxSingle,MaxSingle);MaxP:=Point3D(-MaxSingle,-MaxSingle,-MaxSingle);
 for I:=0 to Spline.NumberOfPoints-1 do begin
  P:=Spline.Point[I];Points.Add(PointJSON(P));Knuckles.Add(Spline.Knuckle[I]);MinMax(P,MinP,MaxP);
 end;
 for I:=0 to 8 do Parameters.Add(I/8.0);
 // Every source knot is included; duplicates intentionally remain observable.
 if Spline.NumberOfPoints>1 then for I:=0 to Spline.NumberOfPoints-1 do Parameters.Add(Double(Spline.Parameter[I]));
 if Spline.NumberOfPoints>0 then begin
  Planes.Add(PlaneJSON(SetPlane(1,0,0,-0.5*(MinP.X+MaxP.X))));
  Planes.Add(PlaneJSON(SetPlane(0,1,0,-0.5*(MinP.Y+MaxP.Y))));
  Planes.Add(PlaneJSON(SetPlane(0,0,1,-0.5*(MinP.Z+MaxP.Z))));
  Planes.Add(PlaneJSON(SetPlane(1,0,0,-MaxP.X-1)));
 end;
end;

procedure ExportModel(const Filename,OutputFolder:string);
var Input,Output,Coverage,Family,CaseData,CaseResult,Detail:TJSONObject;Inputs,Results,Families,Selected,Details:TJSONArray;
    Kind,Count,I,J,Index,LastIndex,Picked,CurveCount,CandidateCurves:Integer;Section:TFreeIntersection;Spline:TFreeSpline;
    Name,Id:string;Generated:Boolean;Mid:T3DCoordinate;
begin
 LoadModel(Filename);Input:=TJSONObject.Create;Output:=TJSONObject.Create;Coverage:=TJSONObject.Create;
 try
  Inputs:=TJSONArray.Create;Results:=TJSONArray.Create;Families:=TJSONArray.Create;
  Input.Add('format','FreeShipNativeGeometryCases');Input.Add('version',1);Input.Add('cases',Inputs);
  Output.Add('format','FreeShipNativeGeometryResults');Output.Add('version',1);Output.Add('metadata',Metadata);Output.Add('cases',Results);
  Coverage.Add('selection','Up to four equally spaced saved sections per family; first two original core splines per selected section. Families without saved sections get one private midpoint plane.');
  Coverage.Add('scope','Spline leaf validation only. Fixture planes use the original surface intersection code; this does not validate native hull cutting or exports.');
  Coverage.Add('families',Families);
  Mid:=MidPoint(Ship.Surface.Min,Ship.Surface.Max);
  for Kind:=0 to 3 do begin
   case Kind of
    0:begin Name:='stations';Count:=Ship.NumberOfStations end;
    1:begin Name:='buttocks';Count:=Ship.NumberOfButtocks end;
    2:begin Name:='waterlines';Count:=Ship.NumberOfWaterlines end;
    else begin Name:='diagonals';Count:=Ship.NumberOfDiagonals end;
   end;
   Family:=TJSONObject.Create;Families.Add(Family);Family.Add('kind',Name);Family.Add('available_sections',Count);
   Selected:=TJSONArray.Create;Family.Add('selected_sections',Selected);Details:=TJSONArray.Create;Family.Add('sections',Details);
   LastIndex:=-1;CurveCount:=0;CandidateCurves:=0;Picked:=Min(Count,4);Generated:=Count=0;
   if Generated then Picked:=1;
   Family.Add('generated_fixture_sections',Ord(Generated));Family.Add('candidate_sections',Count+Ord(Generated));
   Family.Add('sampled_sections',Picked);
   for I:=0 to Picked-1 do begin
    if Picked=1 then Index:=0 else Index:=(I*(Count-1)) div (Picked-1);
    if Index=LastIndex then Continue;LastIndex:=Index;
    if Generated then begin
     Section:=TFreeIntersection.Create(Ship);
     case Kind of
      0:begin Section.IntersectionType:=fiStation;Section.Plane:=PlanePointNormal(Mid,Point3D(1,0,0)) end;
      1:begin Section.IntersectionType:=fiButtock;Section.Plane:=PlanePointNormal(Mid,Point3D(0,1,0)) end;
      2:begin Section.IntersectionType:=fiWaterline;Section.Plane:=PlanePointNormal(Mid,Point3D(0,0,1)) end;
      else begin Section.IntersectionType:=fiDiagonal;Section.Plane:=PlanePointNormal(Mid,Point3D(0,1,1)) end;
     end;
    end else begin
     Selected.Add(Index);
     case Kind of
      0:Section:=Ship.Station[Index];1:Section:=Ship.Buttock[Index];2:Section:=Ship.Waterline[Index];else Section:=Ship.Diagonal[Index];
     end;
    end;
    try
     Section.Rebuild;Inc(CandidateCurves,Section.Count);Detail:=TJSONObject.Create;Details.Add(Detail);
     if Generated then Detail.Add('origin','fixture-generated-midpoint') else Detail.Add('origin','saved-section');
     Detail.Add('index',Index);Detail.Add('plane',PlaneJSON(Section.Plane));Detail.Add('candidate_splines',Section.Count);
     Detail.Add('sampled_splines',Min(Section.Count,2));Detail.Add('has_intersection',Section.Count>0);
     for J:=0 to Min(Section.Count,2)-1 do begin
      Spline:=Section.Items[J];Id:=Name+'-'+IntToStr(Index)+'-'+IntToStr(J);
      if Generated then Id:='fixture-'+Id;
      CaseData:=SplineInput(Spline,Id);Inputs.Add(CaseData);
      try CaseResult:=SampleSpline(Spline,CaseData);CaseResult.Add('status','ok')
      except on E:Exception do begin CaseResult:=TJSONObject.Create;CaseResult.Add('status','error');CaseResult.Add('error',E.ClassName+': '+E.Message) end end;
      CaseResult.Add('id',Id);CaseResult.Add('kind','spline');Results.Add(CaseResult);Inc(CurveCount);
     end;
    finally if Generated then Section.Free end;
   end;
   Family.Add('candidate_splines_in_sampled_sections',CandidateCurves);
   Family.Add('unselected_sections',Count+Ord(Generated)-Picked);
   Family.Add('exported_splines',CurveCount);
  end;
  Coverage.Add('exported_splines',Inputs.Count);WriteJSON(OutputFolder+'/input.json',Input);
  WriteJSON(OutputFolder+'/results.json',Output);WriteJSON(OutputFolder+'/coverage.json',Coverage);
 finally Input.Free;Output.Free;Coverage.Free;FreeAndNil(Ship) end;
end;

begin
 try
  DefaultFormatSettings.DecimalSeparator:='.';Application.Initialize;
  ApplyLanguageSafely('cn',ExtractFilePath(ParamStr(0))+'locale');
  if ParamStr(3)='export-model' then ExportModel(ParamStr(1),ParamStr(2))
  else RunCases(ParamStr(1),ParamStr(2));
  Halt(0);
 except on E:Exception do begin WriteLn(E.ClassName+': '+E.Message);Ship.Free;Halt(1) end end;
end.
