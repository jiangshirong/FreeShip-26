program QtEditingFixture;
{$mode objfpc}{$H+}
// Test-only direct core observer. ModelService/ModelEdits are not used here.
uses Interfaces,Forms,Graphics,SysUtils,Classes,Math,fpjson,jsonparser,FreeShipUnit,
 FreeFileBuffer,FreeGeometry,FreeTypes,FreeVersionUnit,FreeSafeLanguage,FreeLinesplanScene;

var Ship:TFreeShip;OutputFolder,Mode,InputFile:string;

function PointJSON(const P:T3DCoordinate):TJSONArray;
begin Result:=TJSONArray.Create;Result.Add(Double(P.X));Result.Add(Double(P.Y));Result.Add(Double(P.Z)) end;

procedure WriteJSON(const Filename:string;Data:TJSONData);
var Stream:TFileStream;Value:UTF8String;
begin
 Value:=Data.AsJSON;Stream:=TFileStream.Create(Filename,fmCreate);
 try if Length(Value)>0 then Stream.WriteBuffer(Value[1],Length(Value)) finally Stream.Free end;
end;

procedure LoadModel(const Filename:string);
var Buffer:TFreeFileBuffer;
begin
 FreeAndNil(Ship);Ship:=TFreeShip.Create(nil);GlobalFreeShip:=Ship;
 Ship.IsRebuildAsyncDisabled:=True;Ship.Preferences.Load;
 Buffer:=TFreeFileBuffer.Create;
 try Buffer.LoadFromFile(Filename);Ship.LoadProject(Buffer,False) finally Buffer.Free end;
 Ship.RebuildModel;
end;

procedure SaveModel(const Filename:string);
var Buffer:TFreeFileBuffer;
begin
 Buffer:=TFreeFileBuffer.Create;
 try Ship.SaveProject(Buffer);Buffer.SaveToFile(Filename) finally Buffer.Free end;
end;

function PointId(Point:TFreeSubdivisionControlPoint):Integer;
var I:Integer;
begin
 if Point=nil then Exit(0);
 for I:=0 to Ship.Surface.NumberOfControlPoints-1 do
  if Ship.Surface.ControlPoint[I]=Point then Exit(I+1);
 raise Exception.Create('Unknown control point relation');
end;

function BatchMovable(Point:TFreeSubdivisionControlPoint):Boolean;
var I:Integer;Other:TFreeSubdivisionControlPoint;
begin
 Result:=False;
 if Point.Locked or (Point.AnchorPoint<>nil) or (Point.LinearConstraintPointA<>nil) or
    (Point.LinearConstraintPointB<>nil) then Exit;
 for I:=0 to Ship.Surface.NumberOfControlPoints-1 do begin
  Other:=Ship.Surface.ControlPoint[I];
  if (Other.AnchorPoint=Point) or (Other.LinearConstraintPointA=Point) or
     (Other.LinearConstraintPointB=Point) then Exit;
 end;
 Result:=True;
end;

function SectionJSON(Section:TFreeIntersection):TJSONObject;
var I,J,Count:Integer;P,Minimum,Maximum:T3DCoordinate;Plane:T3DPlane;
    Coordinates:TJSONArray;Bounds:TJSONObject;
begin
 Result:=TJSONObject.Create;Section.Rebuild;Plane:=Section.Plane;
 Coordinates:=TJSONArray.Create;Coordinates.Add(Double(Plane.a));Coordinates.Add(Double(Plane.b));
 Coordinates.Add(Double(Plane.c));Coordinates.Add(Double(Plane.d));Result.Add('plane',Coordinates);
 Result.Add('distance',Double(Section.Distance));Count:=0;
 Minimum:=Point3D(MaxSingle,MaxSingle,MaxSingle);Maximum:=Point3D(-MaxSingle,-MaxSingle,-MaxSingle);
 for I:=0 to Section.Count-1 do for J:=0 to Section.Items[I].NumberOfPoints-1 do begin
  P:=Section.Items[I].Point[J];Inc(Count);
  Minimum.X:=Min(Minimum.X,P.X);Minimum.Y:=Min(Minimum.Y,P.Y);Minimum.Z:=Min(Minimum.Z,P.Z);
  Maximum.X:=Max(Maximum.X,P.X);Maximum.Y:=Max(Maximum.Y,P.Y);Maximum.Z:=Max(Maximum.Z,P.Z);
 end;
 Result.Add('point_count',Count);
 if Count>0 then begin Bounds:=TJSONObject.Create;Bounds.Add('min',PointJSON(Minimum));
  Bounds.Add('max',PointJSON(Maximum));Result.Add('bounds',Bounds) end
 else Result.Add('bounds',TJSONNull.Create);
end;

procedure DumpState(const Filename:string);
var Root,Settings,ObjectValue,Sections,Bounds:TJSONObject;
    Points,Layers,Items,Aux:TJSONArray;I,J,K:Integer;P:TFreeSubdivisionControlPoint;
    Layer:TFreeSubdivisionLayer;Point2:T2DCoordinate;ImageFile:string;
    Curve:TFreeSubdivisionControlCurve;Edge:TFreeSubdivisionControlEdge;Face:TFreeSubdivisionControlFace;
    Group:TFreeSubdivisionControlPointGroup;Locked:Boolean;
 function SplineJSON(S:TFreeSpline):TJSONObject;
 var K:Integer;Coordinates,Flags:TJSONArray;
 begin
  Result:=TJSONObject.Create;Coordinates:=TJSONArray.Create;Flags:=TJSONArray.Create;
  Result.Add('points',Coordinates);Result.Add('knuckles',Flags);
  for K:=0 to S.NumberOfPoints-1 do begin Coordinates.Add(PointJSON(S.Point[K]));Flags.Add(S.Knuckle[K]) end;
 end;
begin
 Root:=TJSONObject.Create;
 try
  Root.Add('format','FreeShipEditingCoreObservation');Root.Add('version',1);
  Settings:=TJSONObject.Create;Root.Add('display',Settings);
  Settings.Add('both_sides',Ship.Visibility.ModelView=mvBoth);Settings.Add('show_curvature',Ship.Visibility.ShowCurvature);
  Settings.Add('curvature_scale',Double(Ship.Visibility.CurvatureScale));
  Settings.Add('show_control_curves',Ship.Visibility.ShowControlCurves);Settings.Add('show_flowlines',Ship.Visibility.ShowFlowlines);
  if Ship.ProjectSettings.ProjectUnits=fuMetric then Root.Add('units','metric') else Root.Add('units','imperial');
  Root.Add('file_version',Ord(Ship.FileVersion));Root.Add('text_encoding',Ship.Preferences.FbmEncoding);
  Root.Add('name',Ship.ProjectSettings.ProjectName);Root.Add('designer',Ship.ProjectSettings.ProjectDesigner);
  Root.Add('comment',Ship.ProjectSettings.ProjectComment);
  Root.Add('control_edge_count',Ship.Surface.NumberOfControlEdges);Root.Add('control_face_count',Ship.Surface.NumberOfControlFaces);
  Bounds:=TJSONObject.Create;Bounds.Add('min',PointJSON(Ship.Surface.Min));Bounds.Add('max',PointJSON(Ship.Surface.Max));Root.Add('bounds',Bounds);
  Settings:=TJSONObject.Create;Root.Add('settings',Settings);
  Settings.Add('length',Double(Ship.ProjectSettings.ProjectLength));Settings.Add('beam',Double(Ship.ProjectSettings.ProjectBeam));
  Settings.Add('draft',Double(Ship.ProjectSettings.ProjectDraft));Settings.Add('water_density',Double(Ship.ProjectSettings.ProjectWaterDensity));
  Settings.Add('appendage_coefficient',Double(Ship.ProjectSettings.ProjectAppendageCoefficient));
  Settings.Add('split_section_location',Double(Ship.ProjectSettings.ProjectSplitSectionLocation));
  Settings.Add('stored_split_location',Double(Ship.ProjectSettings.ServiceStoredSplitSectionLocation));
  Settings.Add('use_default_split_section_location',Ship.ProjectSettings.UseDefaultSplitSectionLocation);
  Settings.Add('simplify_intersections',Ship.ProjectSettings.ProjectSimplifyIntersections);
  Settings.Add('precision',Ord(Ship.Precision));Settings.Add('shade_underwater',Ship.ProjectSettings.ProjectShadeUnderwaterShip);
  Settings.Add('underwater_color',Int64(ColorToRGB(Ship.ProjectSettings.ProjectUnderWaterColor)));
  Settings.Add('raw_underwater_color',Int64(Ship.ProjectSettings.ProjectUnderWaterColor));
  Settings.Add('hydro_start_draft',Double(Ship.ProjectSettings.Hydrostatics_StartDraft));
  Settings.Add('hydro_end_draft',Double(Ship.ProjectSettings.Hydrostatics_EndDraft));
  Settings.Add('hydro_draft_step',Double(Ship.ProjectSettings.Hydrostatics_DraftStep));
  Settings.Add('hydro_trim',Double(Ship.ProjectSettings.Hydrostatics_Trim));
  Points:=TJSONArray.Create;Root.Add('control_points',Points);
  for I:=0 to Ship.Surface.NumberOfControlPoints-1 do begin
   P:=Ship.Surface.ControlPoint[I];ObjectValue:=TJSONObject.Create;Points.Add(ObjectValue);
   ObjectValue.Add('id',I+1);ObjectValue.Add('p',PointJSON(P.Coordinate));ObjectValue.Add('locked',P.Locked);
   ObjectValue.Add('batch_movable',BatchMovable(P));ObjectValue.Add('anchor',PointId(P.AnchorPoint));
   ObjectValue.Add('linear_a',PointId(P.LinearConstraintPointA));ObjectValue.Add('linear_b',PointId(P.LinearConstraintPointB));
   ObjectValue.Add('name',P.Name);ObjectValue.Add('vertex_type',Ord(P.VertexType));ObjectValue.Add('hard_anchor',P.IsAnchorHard);
  end;
  Items:=TJSONArray.Create;Root.Add('point_groups',Items);
  for I:=0 to Ship.Surface.NumberOfControlPointGroups-1 do begin
    Group:=Ship.Surface.ControlPointGroup[I];ObjectValue:=TJSONObject.Create;Items.Add(ObjectValue);
    ObjectValue.Add('name',Group.Name);Aux:=TJSONArray.Create;ObjectValue.Add('points',Aux);Locked:=Group.ControlPoints.Count>0;
    for J:=0 to Group.ControlPoints.Count-1 do begin Aux.Add(PointId(Group.ControlPoints[J]));Locked:=Locked and Group.ControlPoints[J].Locked end;
    ObjectValue.Add('locked',Locked);
  end;
  Items:=TJSONArray.Create;Root.Add('control_edges',Items);
  for I:=0 to Ship.Surface.NumberOfControlEdges-1 do begin
   Edge:=Ship.Surface.ControlEdge[I];ObjectValue:=TJSONObject.Create;Items.Add(ObjectValue);
   ObjectValue.Add('a',PointId(TFreeSubdivisionControlPoint(Edge.StartPoint)));ObjectValue.Add('b',PointId(TFreeSubdivisionControlPoint(Edge.EndPoint)));
   ObjectValue.Add('crease',Edge.Crease);
  end;
  Items:=TJSONArray.Create;Root.Add('control_faces',Items);
  for I:=0 to Ship.Surface.NumberOfControlFaces-1 do begin
   Face:=Ship.Surface.ControlFace[I];ObjectValue:=TJSONObject.Create;Items.Add(ObjectValue);Aux:=TJSONArray.Create;ObjectValue.Add('points',Aux);
   for J:=0 to Face.NumberOfPoints-1 do Aux.Add(PointId(TFreeSubdivisionControlPoint(Face.Point[J])));
   for K:=0 to Ship.Surface.NumberOfLayers-1 do if Ship.Surface.Layer[K]=Face.Layer then ObjectValue.Add('layer_index',K);
  end;
  Items:=TJSONArray.Create;Root.Add('markers',Items);
  for I:=0 to Ship.NumberOfMarkers-1 do Items.Add(SplineJSON(Ship.Marker[I]));
  Layers:=TJSONArray.Create;Root.Add('layers',Layers);
  for I:=0 to Ship.Surface.NumberOfLayers-1 do begin
   Layer:=Ship.Surface.Layer[I];ObjectValue:=TJSONObject.Create;Layers.Add(ObjectValue);
   ObjectValue.Add('id',I+1);ObjectValue.Add('name',Layer.Name);ObjectValue.Add('color',Int64(ColorToRGB(Layer.Color)));
   ObjectValue.Add('raw_name',Layer.ServiceStoredName);ObjectValue.Add('raw_color',Int64(Layer.Color));
   ObjectValue.Add('visible',Layer.SurfaceVisible);ObjectValue.Add('symmetric',Layer.Symmetric);
   ObjectValue.Add('developable',Layer.Developable);ObjectValue.Add('use_in_hydrostatics',Layer.UseInHydrostatics);
   ObjectValue.Add('show_in_linesplan',Layer.ShowInLinesplan);ObjectValue.Add('use_for_intersections',Layer.UseForIntersections);
   ObjectValue.Add('thickness',Double(Layer.Thickness));ObjectValue.Add('material_density',Double(Layer.MaterialDensity));
  end;
  Sections:=TJSONObject.Create;Root.Add('intersections',Sections);
  Items:=TJSONArray.Create;Sections.Add('stations',Items);
  for I:=0 to Ship.NumberOfStations-1 do Items.Add(SectionJSON(Ship.Station[I]));
  Items:=TJSONArray.Create;Sections.Add('buttocks',Items);
  for I:=0 to Ship.NumberOfButtocks-1 do Items.Add(SectionJSON(Ship.Buttock[I]));
  Items:=TJSONArray.Create;Sections.Add('waterlines',Items);
  for I:=0 to Ship.NumberOfWaterlines-1 do Items.Add(SectionJSON(Ship.Waterline[I]));
  Items:=TJSONArray.Create;Sections.Add('diagonals',Items);
  for I:=0 to Ship.NumberOfDiagonals-1 do Items.Add(SectionJSON(Ship.Diagonal[I]));
  Items:=TJSONArray.Create;Root.Add('backgrounds',Items);
  for I:=0 to Ship.NumberofBackgroundImages-1 do begin
   ObjectValue:=TJSONObject.Create;Items.Add(ObjectValue);
   ObjectValue.Add('view',Ord(Ship.BackgroundImage[I].AssignedView));ObjectValue.Add('scale',Double(Ship.BackgroundImage[I].Scale));
   Aux:=TJSONArray.Create;Aux.Add(Ship.BackgroundImage[I].Origin.X);Aux.Add(Ship.BackgroundImage[I].Origin.Y);ObjectValue.Add('origin',Aux);
   ObjectValue.Add('width',Ship.BackgroundImage[I].Image.Width);ObjectValue.Add('height',Ship.BackgroundImage[I].Image.Height);
   ObjectValue.Add('visible',Ship.BackgroundImage[I].ServiceVisible);ObjectValue.Add('alpha',Ship.BackgroundImage[I].BlendingValue);
   ObjectValue.Add('transparent',Ship.BackgroundImage[I].Transparent);ObjectValue.Add('transparent_color',Int64(Ship.BackgroundImage[I].TransparentColor));
   ObjectValue.Add('tolerance',Ship.BackgroundImage[I].Tolerance);ObjectValue.Add('quality',Ship.BackgroundImage[I].Quality);
   ImageFile:=ChangeFileExt(Filename,'')+'-background-'+IntToStr(I+1)+'.jpg';
   Ship.BackgroundImage[I].Image.SaveToFile(ImageFile);ObjectValue.Add('image_file',ImageFile);
  end;
  Items:=TJSONArray.Create;Root.Add('control_curves',Items);
  for I:=0 to Ship.Surface.NumberOfControlCurves-1 do begin
   // LoadModel already ran Surface.Rebuild, which calls RebuildSpline.
   // The unused Curve.Rebuild method assigns different endpoint knuckles;
   // calling it here would mutate the very state we intend to observe.
   Curve:=Ship.Surface.ControlCurve[I];
   ObjectValue:=TJSONObject.Create;Items.Add(ObjectValue);Aux:=TJSONArray.Create;ObjectValue.Add('control_positions',Aux);
   for J:=0 to Curve.NumberOfControlPoints-1 do Aux.Add(PointJSON(Curve.ControlPoint[J].Coordinate));
   ObjectValue.Add('spline',SplineJSON(Curve.Spline));
  end;
  Items:=TJSONArray.Create;Root.Add('flowlines',Items);
  for I:=0 to Ship.NumberOfFlowlines-1 do begin
   ObjectValue:=TJSONObject.Create;Items.Add(ObjectValue);Point2:=Ship.QtEditingFlowPoint(I);
   Aux:=TJSONArray.Create;Aux.Add(Double(Point2.X));Aux.Add(Double(Point2.Y));ObjectValue.Add('projection_point',Aux);
   ObjectValue.Add('view',Ord(Ship.QtEditingFlowView(I)));
   Ship.Flowline[I].Build:=False;Ship.Flowline[I].Rebuild;
   ObjectValue.Add('spline',SplineJSON(Ship.Flowline[I].ServiceSpline));
  end;
  WriteJSON(Filename,Root);
 finally Root.Free end;
end;

procedure ClearRelations;
var I:Integer;P:TFreeSubdivisionControlPoint;
begin
 for I:=0 to Ship.Surface.NumberOfControlPoints-1 do begin
  P:=Ship.Surface.ControlPoint[I];P.AnchorPoint:=nil;P.SetLinearConstraint(nil,nil);P.Locked:=False;
 end;
end;

procedure Seed;
var I:Integer;Layer:TFreeSubdivisionLayer;L,B,D:TFloatType;
begin
 Ship.ProjectSettings.DisableModelCheck:=True;Ship.ProjectSettings.EnableModelAutoMove:=False;
 Ship.ProjectSettings.EnableBonjeanSAC:=False;ClearRelations;
 if Ship.ProjectSettings.ProjectUnits<>fuMetric then Ship.ProjectSettings.ProjectUnits:=fuMetric;
 Ship.ProjectSettings.ProjectWaterDensity:=1.025;
 Ship.ProjectSettings.UseDefaultSplitSectionLocation:=False;Ship.ProjectSettings.ProjectSplitSectionLocation:=-2.125;
 Ship.ProjectSettings.Hydrostatics_StartDraft:=0.5;Ship.ProjectSettings.Hydrostatics_EndDraft:=2.5;
 Ship.ProjectSettings.Hydrostatics_DraftStep:=0.25;Ship.ProjectSettings.Hydrostatics_Trim:=-0.4;
 if (Ship.Surface.NumberOfLayers=1) and (Ship.Surface.NumberOfControlFaces>1) then begin
  Layer:=Ship.Surface.AddNewLayer;Layer.AssignProperties(Ship.Surface.Layer[0]);Layer.Name:='Editing secondary layer';
  Ship.Surface.ControlFace[Ship.Surface.NumberOfControlFaces-1].Layer:=Layer;
 end;
 for I:=0 to Ship.Surface.NumberOfLayers-1 do begin
  Ship.Surface.Layer[I].MaterialDensity:=7.85;Ship.Surface.Layer[I].Thickness:=12;
 end;
 if Ship.Surface.NumberOfControlPoints>0 then Ship.Surface.ControlPoint[Ship.Surface.NumberOfControlPoints-1].Locked:=True;
 Ship.Built:=False;Ship.RebuildModel;
 L:=Ship.Surface.Max.X-Ship.Surface.Min.X;B:=Ship.Surface.Max.Y-Ship.Surface.Min.Y;D:=Ship.Surface.Max.Z-Ship.Surface.Min.Z;
 Ship.Edit.Intersection_Add(fiStation,Ship.Surface.Min.X+L*0.3712345);
 Ship.Edit.Intersection_Add(fiStation,-Abs(Ship.Surface.Min.X+L*0.1234567)-0.333);
 Ship.Edit.Intersection_Add(fiButtock,Ship.Surface.Min.Y+B*0.3712345);
 Ship.Edit.Intersection_Add(fiWaterline,Ship.Surface.Min.Z+D*0.3712345);
 Ship.Edit.Intersection_Add(fiDiagonal,Abs(B*0.2712345+D*0.1123456)+0.125);
 Ship.QtEditingSeedAux;Ship.FileVersion:=CurrentVersion;Ship.Built:=False;Ship.RebuildModel;
 SaveModel(OutputFolder+'/seed.fbm');LoadModel(OutputFolder+'/seed.fbm');DumpState(OutputFolder+'/state.json');
 if Ship.Surface.NumberOfControlPoints>=8 then begin
  ClearRelations;
  Ship.Surface.ControlPoint[3].AnchorPoint:=Ship.Surface.ControlPoint[2];
  Ship.Surface.ControlPoint[6].SetLinearConstraint(Ship.Surface.ControlPoint[4],Ship.Surface.ControlPoint[5]);
  Ship.Surface.ControlPoint[Ship.Surface.NumberOfControlPoints-1].Locked:=True;
  Ship.FileVersion:=CurrentVersion;SaveModel(OutputFolder+'/constraints.fbm');
  LoadModel(OutputFolder+'/constraints.fbm');DumpState(OutputFolder+'/constraints-state.json');
 end;
 LoadModel(OutputFolder+'/seed.fbm');ClearRelations;Ship.QtEditingClearAux;
 for I:=0 to Ship.Surface.NumberOfLayers-1 do begin
  Ship.Surface.Layer[I].MaterialDensity:=0;Ship.Surface.Layer[I].Thickness:=0;Ship.Surface.Layer[I].ShowInLinesplan:=True;
 end;
 Ship.FileVersion:=fv180;SaveModel(OutputFolder+'/legacy.fbm');
 LoadModel(OutputFolder+'/legacy.fbm');DumpState(OutputFolder+'/legacy-state.json');
 LoadModel(OutputFolder+'/seed.fbm');
 while Ship.NumberOfStations>0 do Ship.Station[0].Delete(False);
 while Ship.NumberOfButtocks>0 do Ship.Buttock[0].Delete(False);
 while Ship.NumberOfWaterlines>0 do Ship.Waterline[0].Delete(False);
 while Ship.NumberOfDiagonals>0 do Ship.Diagonal[0].Delete(False);
 SaveModel(OutputFolder+'/no-intersections.fbm');LoadModel(OutputFolder+'/no-intersections.fbm');
 DumpState(OutputFolder+'/no-intersections-state.json');
 LoadModel(OutputFolder+'/seed.fbm');
 Ship.ProjectSettings.ProjectUnderWaterColor:=clBtnFace;
 Ship.ProjectSettings.UseDefaultSplitSectionLocation:=True;
 if Ship.Surface.NumberOfLayers>0 then begin
  Ship.Surface.Layer[0].Color:=clBtnFace;Ship.Surface.Layer[0].Name:='';
 end;
 SaveModel(OutputFolder+'/system-colors.fbm');LoadModel(OutputFolder+'/system-colors.fbm');
 DumpState(OutputFolder+'/system-colors-state.json');
end;

// Independent original arithmetic and model-wide scale; no ModelService commands.
procedure AdvancedCore;
var Data:TJSONData;Request,Root:TJSONObject;Stream:TFileStream;Rows,Row,Output:TJSONArray;
    Operation:string;I:Integer;V,P:T3DCoordinate;Plane:T3DPlane;SX,CX,SY,CY,SZ,CZ:TFloatType;
    Edges:TFasterListTFreeSubdivisionEdge;Marker:TFreeMarker;
begin
 if Mode='advanced_seed' then begin
  ClearRelations;Marker:=TFreeMarker.Create(Ship.Surface);Marker.Add(Point3D(1.25,2.5,3.75));Marker.Add(Point3D(2,3,4));Ship.Edit.Marker_Add(Marker);
  Ship.ProjectSettings.UseDefaultSplitSectionLocation:=False;Ship.ProjectSettings.ProjectSplitSectionLocation:=2.125;
  Ship.Edit.Intersection_Add(fiDiagonal,0.125);
  Ship.FileVersion:=CurrentVersion;SaveModel(OutputFolder+'/advanced.fbm');LoadModel(OutputFolder+'/advanced.fbm');DumpState(OutputFolder+'/state.json');Exit;
 end;
 Stream:=TFileStream.Create(OutputFolder+'/request.json',fmOpenRead);
 try Data:=GetJSON(Stream) finally Stream.Free end;
 try
  Request:=TJSONObject(Data);Operation:=Request.Strings['operation'];Row:=Request.Arrays['vector'];V:=Point3D(Row.Floats[0],Row.Floats[1],Row.Floats[2]);
  if Mode='advanced_core' then begin
   if Operation='scale' then Ship.Edit.Model_Scale(V,False,Request.Booleans['adjust_markers'])
   else if Operation='flip' then Ship.Surface.ControlFace[Request.Integers['face_index']].FlipNormal
   else if Operation='extrude' then begin
    Edges:=TFasterListTFreeSubdivisionEdge.Create;
    try
     Rows:=Request.Arrays['edge_indices'];for I:=0 to Rows.Count-1 do Edges.Add(Ship.Surface.ControlEdge[Rows.Integers[I]]);
     Ship.Surface.ActiveLayer:=Ship.Surface.Layer[Request.Integers['layer_index']];Ship.Surface.ExtrudeEdges(Edges,V);
    finally Edges.Free end;
   end else raise Exception.Create('Unknown original-core operation');
   Ship.Built:=False;Ship.RebuildModel;SaveModel(OutputFolder+'/advanced.fbm');LoadModel(OutputFolder+'/advanced.fbm');DumpState(OutputFolder+'/state.json');Exit;
  end;
  CX:=Cos(DegToRad(V.X));SX:=Sin(DegToRad(V.X));CY:=Cos(DegToRad(V.Y));SY:=Sin(DegToRad(V.Y));CZ:=Cos(DegToRad(V.Z));SZ:=Sin(DegToRad(V.Z));
  Plane:=SetPlane(0,0,0,0);
  if Operation='mirror' then begin
   Plane.d:=-TFloatType(Request.Floats['distance']);case Request.Integers['axis'] of 0:Plane.a:=1;1:Plane.b:=1;2:Plane.c:=1 end;
  end;
  Root:=TJSONObject.Create;
  try
   Output:=TJSONArray.Create;Root.Add('points',Output);Rows:=Request.Arrays['points'];
   for I:=0 to Rows.Count-1 do begin Row:=Rows.Arrays[I];P:=Point3D(Row.Floats[0],Row.Floats[1],Row.Floats[2]);
    if Operation='rotate' then P:=RotateVector(P,SX,CX,SY,CY,SZ,CZ)
    else if Operation='mirror' then P:=MirrorPlane(P,Plane)
    else if Operation='translate' then begin P.X:=P.X+V.X;P.Y:=P.Y+V.Y;P.Z:=P.Z+V.Z end
    else if Operation='scale' then begin P.X:=P.X*V.X;P.Y:=P.Y*V.Y;P.Z:=P.Z*V.Z end
    else raise Exception.Create('Unknown original arithmetic');
    Output.Add(PointJSON(P));
   end;
   WriteJSON(OutputFolder+'/state.json',Root);
  finally Root.Free end;
 finally Data.Free end;
end;

// Observe the original integration directly. Neither the service nor the new
// balance solver is linked into this oracle. Requests specify static conditions
// or an explicit waterplane for independent equilibrium re-evaluation.
procedure ObserveHydro;
var Data:TJSONData;Stream:TFileStream;Request,Root,Entry,Condition:TJSONObject;
    Conditions,Rows,A:TJSONArray;I,J:Integer;Calc:TFreeHydrostaticCalc;D:TFreeHydrostaticsData;Plane:T3DPlane;
begin
 Stream:=TFileStream.Create(OutputFolder+'/request.json',fmOpenRead);
 try Data:=GetJSON(Stream) finally Stream.Free end;
 Root:=TJSONObject.Create;Calc:=TFreeHydrostaticCalc.Create(Ship);
 try
  Request:=TJSONObject(Data);Conditions:=Request.Arrays['conditions'];Rows:=TJSONArray.Create;Root.Add('rows',Rows);
  Root.Add('keel_z',Double(Ship.FindLowestHydrostaticsPoint));
  for I:=0 to Conditions.Count-1 do begin
   Condition:=Conditions.Objects[I];Calc.Clear;
   Calc.Draft:=Condition.Get('draft',Double(Ship.ProjectSettings.ProjectDraft));
   Calc.Trim:=Condition.Get('trim',0.0);Calc.HeelingAngle:=Condition.Get('heel',0.0);
   if Condition.Find('plane')<>nil then begin
    A:=Condition.Arrays['plane'];Plane:=SetPlane(A.Floats[0],A.Floats[1],A.Floats[2],A.Floats[3]);Calc.CalculateVolume(Plane);
   end else begin Calc.Calculate;Calc.CalculateGravity end;
   D:=Calc.Data;Entry:=TJSONObject.Create;Rows.Add(Entry);
   Entry.Add('error',Calc.ErrorString);Entry.Add('valid',Calc.Errors=[]);
   Entry.Add('volume',Double(D.Volume));Entry.Add('displacement',Double(D.Displacement));Entry.Add('buoyancy',PointJSON(D.CenterOfBuoyancy));
   Entry.Add('absolute_draft',Double(D.AbsoluteDraft));Entry.Add('lwl',Double(D.LengthWaterline));Entry.Add('bwl',Double(D.BeamWaterline));
   Entry.Add('wetted_area',Double(D.WettedSurface));Entry.Add('cb',Double(D.BlockCoefficient));Entry.Add('cp',Double(D.PrismCoefficient));
   Entry.Add('cvp',Double(D.VertPrismCoefficient));Entry.Add('midship_x',Double(Calc.MidshipLocation));Entry.Add('midship_area',Double(D.MidshipArea));
   Entry.Add('cm',Double(D.MidshipCoeff));Entry.Add('waterplane_area',Double(D.WaterplaneArea));Entry.Add('cwp',Double(D.WaterplaneCoeff));
   Entry.Add('waterplane_cog',PointJSON(D.WaterplaneCOG));Entry.Add('waterplane_ix',Double(D.WaterplaneMomInertia.X));Entry.Add('waterplane_iy',Double(D.WaterplaneMomInertia.Y));
   Entry.Add('kmt',Double(D.KMtransverse));Entry.Add('kml',Double(D.KMlongitudinal));Entry.Add('lateral_area',Double(D.LateralArea));
   Entry.Add('lateral_cog',PointJSON(D.LateralCOG));Entry.Add('weight',Double(D.Weight_));Entry.Add('weight_cog',PointJSON(D.CenterOfGravity_));
   A:=TJSONArray.Create;Entry.Add('sectional_areas',A);
   for J:=0 to High(D.SAC) do A.Add(TJSONArray.Create([Double(D.SAC[J].X),Double(D.SAC[J].Y)]));
  end;
  WriteJSON(OutputFolder+'/state.json',Root);
 finally Calc.Free;Root.Free;Data.Free end;
end;

procedure LegacyExports;
var Scene:TLinesplanScene;Root,Entry:TJSONObject;Short:TJSONArray;I,J,Kind:Integer;Section:TFreeIntersection;
begin
 Root:=TJSONObject.Create;Ship.InvalidateIntersections;Scene:=TLinesplanScene.Create(Ship);
 try
  Short:=TJSONArray.Create;Root.Add('short_sampling',Short);
  for Kind:=1 to 4 do for I:=0 to Ship.ServiceSectionList(Kind).Count-1 do begin
   Section:=TFreeIntersection(Ship.ServiceSectionList(Kind)[I]);Section.Rebuild;
   for J:=0 to Section.Count-1 do if Section.Items[J].Fragments<2 then begin
    Entry:=TJSONObject.Create;Short.Add(Entry);Entry.Add('kind',Kind);Entry.Add('index',I);Entry.Add('curve',J);
    Entry.Add('fragments',Section.Items[J].Fragments);Entry.Add('points',Section.Items[J].NumberOfPoints);
   end;
  end;
  try Scene.Generate(OutputFolder+'/scene.fslp');Root.Add('scene','pass')
  except on E:Exception do begin Root.Add('scene_error',E.ClassName+': '+E.Message);DumpExceptionBackTrace(StdErr) end end;
  // Scene layout is prepared before emitting primitives; a shading failure must
  // not prevent this independent attempt to collect the original DXF exporter.
  try Scene.ExportDXF(OutputFolder+'/reference.dxf');Root.Add('dxf','pass')
  except on E:Exception do begin Root.Add('dxf_error',E.ClassName+': '+E.Message);DumpExceptionBackTrace(StdErr) end end;
  WriteJSON(OutputFolder+'/state.json',Root);
 finally Scene.Free;Root.Free end;
end;

procedure ObserveFlow;
var Stream:TFileStream;Data:TJSONData;Root,Entry,Request:TJSONObject;
    Requests,Entries,Coordinates,Points,Flags:TJSONArray;I,J:Integer;Flow:TFreeFlowline;P:T2DCoordinate;
begin
 Stream:=TFileStream.Create(OutputFolder+'/request.json',fmOpenRead);
 try Data:=GetJSON(Stream) finally Stream.Free end;
 Root:=TJSONObject.Create;
 try
  Requests:=TJSONArray(Data);Entries:=TJSONArray.Create;Root.Add('flowlines',Entries);
  for I:=0 to Requests.Count-1 do begin
   Request:=Requests.Objects[I];Coordinates:=Request.Arrays['source'];P.X:=Coordinates.Floats[0];P.Y:=Coordinates.Floats[1];
   Flow:=TFreeFlowline.Create(Ship);
   try
    Flow.ServiceSource:=P;Flow.ServiceView:=TFreeViewType(Request.Integers['view']);Flow.Rebuild;
    Entry:=TJSONObject.Create;Entries.Add(Entry);Entry.Add('name',Request.Strings['name']);
    Points:=TJSONArray.Create;Flags:=TJSONArray.Create;Entry.Add('points',Points);Entry.Add('knuckles',Flags);
    for J:=0 to Flow.ServiceSpline.NumberOfPoints-1 do begin Points.Add(PointJSON(Flow.ServiceSpline.Point[J]));Flags.Add(Flow.ServiceSpline.Knuckle[J]) end;
   finally Flow.Free end;
  end;
  WriteJSON(OutputFolder+'/state.json',Root);
 finally Root.Free;Data.Free end;
end;

procedure ObserveCurvature;
var Requests:TJSONArray;Data:TJSONData;Stream:TFileStream;Root,Entry,Row,Request:TJSONObject;
    Curves,Samples,Points,Knuckles,Coordinates:TJSONArray;Spline:TFreeSpline;
    I,J,K,N,Pn,Pn1,Pn2:Integer;C,Scale:TFloatType;P,Tip,Normal,KnucklePoint:T3DCoordinate;
  procedure AppendSample(const Base,Top:T3DCoordinate);
  var Index:Integer;
  begin
    Index:=Samples.Count;Row:=TJSONObject.Create;Samples.Add(Row);
    Row.Add('base',PointJSON(Base));Row.Add('tip',PointJSON(Top));Row.Add('rib',(Index mod 4=0) or (Index=1) or (Index=N));
  end;
begin
  Stream:=TFileStream.Create(OutputFolder+'/request.json',fmOpenRead);
  try Data:=GetJSON(Stream) finally Stream.Free end;
  Root:=TJSONObject.Create;
  try
    Requests:=TJSONArray(Data);Curves:=TJSONArray.Create;Root.Add('curves',Curves);
    for I:=0 to Requests.Count-1 do begin
      Request:=Requests.Objects[I];Points:=Request.Arrays['points'];Knuckles:=Request.Arrays['knuckles'];
      Spline:=TFreeSpline.Create(Ship.Surface);
      try
        for J:=0 to Points.Count-1 do begin Coordinates:=Points.Arrays[J];P:=Point3D(Coordinates.Floats[0],Coordinates.Floats[1],Coordinates.Floats[2]);Spline.Add(P);Spline.Knuckle[J]:=Knuckles.Booleans[J] end;
        N:=384 shl Request.Integers['precision'];Scale:=Request.Floats['scale'];Pn:=0;
        Entry:=TJSONObject.Create;Curves.Add(Entry);Entry.Add('name',Request.Strings['name']);Samples:=TJSONArray.Create;Entry.Add('samples',Samples);
        for K:=0 to N-1 do begin
          C:=Spline.Curvature(K/(N-1),P,Normal);KnucklePoint:=Spline.Value(K/(N-1),Pn1,Pn2);
          if (Pn<>Pn1) and Spline.Knuckle[Pn1] then begin Pn:=Pn1;KnucklePoint:=Spline.Point[Pn1];AppendSample(KnucklePoint,KnucklePoint) end;
          Tip.X:=P.X-C*2*Scale*Normal.X;Tip.Y:=P.Y-C*2*Scale*Normal.Y;Tip.Z:=P.Z-C*2*Scale*Normal.Z;
          AppendSample(P,Tip);
        end;
      finally Spline.Free end;
    end;
    WriteJSON(OutputFolder+'/state.json',Root);
  finally Root.Free;Data.Free end;
end;

begin
 try
  DefaultFormatSettings.DecimalSeparator:='.';Application.Initialize;
  ApplyLanguageSafely('cn',ExtractFilePath(ParamStr(0))+'locale');
  InputFile:=ParamStr(1);OutputFolder:=ParamStr(2);Mode:=ParamStr(3);
  LoadModel(InputFile);
  if Mode='hydro' then ObserveHydro
  else if Mode='legacy_exports' then LegacyExports
  else if (Mode='advanced_math') or (Mode='advanced_core') or (Mode='advanced_seed') then AdvancedCore
  else if Mode='curvature' then ObserveCurvature
  else if Mode='flow' then ObserveFlow
  else if Mode='seed' then Seed
  else if (Mode='imperial') or (Mode='metric') then begin
   if Mode='imperial' then Ship.ProjectSettings.ProjectUnits:=fuImperial
   else Ship.ProjectSettings.ProjectUnits:=fuMetric;
   Ship.RebuildModel;SaveModel(OutputFolder+'/converted.fbm');
   LoadModel(OutputFolder+'/converted.fbm');DumpState(OutputFolder+'/state.json');
  end else if Mode='inspect' then DumpState(OutputFolder+'/state.json')
  else raise Exception.Create('Unknown fixture mode');
  Ship.Free;Halt(0);
 except on E:Exception do begin WriteLn(E.ClassName+': '+E.Message);Halt(1) end end;
end.
