{ FreeShip_26 native input adapter. GPL-2.0-or-later; see COPYING.
  Control-net serialization adapted from the validated subdivision probe. }
unit NativeLinesplanInput;
{$mode objfpc}{$H+}
interface
uses FreeShipUnit,fpjson;
procedure GenerateNativeLinesplan(Ship:TFreeShip;const Directory,DocumentId:string;Revision:Int64;Display:TJSONObject=nil);
implementation
uses SysUtils,Classes,Math,Graphics,Process,jsonparser,Windows,FreeGeometry,FreeTypes,FreeStringsUnit;
function PointJSON(const P:T3DCoordinate):TJSONArray;
begin Result:=TJSONArray.Create;Result.Add(Double(P.X));Result.Add(Double(P.Y));Result.Add(Double(P.Z)) end;
function PointObject(P:TFreeSubdivisionPoint):TJSONObject;
begin Result:=TJSONObject.Create;Result.Add('position',PointJSON(P.Coordinate));Result.Add('type',Ord(P.VertexType)) end;

function PairJSON(A,B:Integer):TJSONArray;
begin Result:=TJSONArray.Create;Result.Add(A);Result.Add(B) end;
function LayerJSON(L:TFreeSubdivisionLayer):TJSONObject;
begin
 Result:=TJSONObject.Create;Result.Add('name',L.Name);Result.Add('layer_id',L.LayerID);
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
  Item.Add('visible',C.Visible);
 end;
end;


procedure GenerateNativeLinesplan(Ship:TFreeShip;const Directory,DocumentId:string;Revision:Int64;Display:TJSONObject);
var Root,Settings,Colors,Texts,Obj:TJSONObject;Sections:TJSONArray;I,J:Integer;
    Int:TFreeIntersection;Plane:T3DPlane;FileStream:TFileStream;Text,Exe:string;Worker:TProcess;Started:QWord;
    Failure:TJSONData;
 procedure ColorValue(const Key:string;Value:TColor);
 var C:TJSONObject;
 begin C:=TJSONObject.Create;C.Add('value',ColorToRGB(Value));C.Add('raw',LongInt(Value));C.Add('dxf',FindDXFColorIndex(Value));Colors.Add(Key,C) end;
begin
 if not Ship.Surface.Built then Ship.Surface.Rebuild;
 Root:=TJSONObject.Create;
 try
  Root.Add('format','FreeShipLinesplanInput');Root.Add('version',1);
  Root.Add('document_id',DocumentId);Root.Add('revision',Revision);Root.Add('geometry',GeometryJSON(Ship));
  if Display<>nil then Root.Add('surface_display',Display.Clone);
  Settings:=TJSONObject.Create;Root.Add('settings',Settings);
  Settings.Add('draft',Double(Ship.ProjectSettings.ProjectDraft));
  Settings.Add('split',Double(Ship.ProjectSettings.ProjectSplitSectionLocation));
  // The existing display uses the maximum-area hydrostatic section for crease
  // placement. Keep that specialized calculation authoritative until its own
  // migration; the C++ worker does all lines-plan geometry and drawing assembly.
  if not Ship.HydrostaticCalculation[0].Calculated then Ship.HydrostaticCalculation[0].Calculate;
  Settings.Add('crease_split',Double(Ship.HydrostaticCalculation[0].MidshipLocation));
  Settings.Add('imperial',Ship.ProjectSettings.ProjectUnits=fuImperial);
  Settings.Add('simplify',Ship.ProjectSettings.ProjectSimplifyIntersections);
  Settings.Add('shade_underwater',Ship.ProjectSettings.ProjectShadeUnderwaterShip);
  Colors:=TJSONObject.Create;Root.Add('colors',Colors);
  ColorValue('underwater',Ship.ProjectSettings.ProjectUnderWaterColor);
  ColorValue('station',Ship.Preferences.StationColor);ColorValue('buttock',Ship.Preferences.ButtockColor);
  ColorValue('waterline',Ship.Preferences.WaterlineColor);ColorValue('diagonal',Ship.Preferences.DiagonalColor);
  ColorValue('crease',Ship.Preferences.CreaseColor);ColorValue('grid',Ship.Preferences.GridColor);ColorValue('red',clRed);
  Texts:=TJSONObject.Create;Root.Add('texts',Texts);
  Texts.Add('base',rs_Base);Texts.Add('dwl',rs_DWL);Texts.Add('center',rs_Center);
  Sections:=TJSONArray.Create;Root.Add('sections',Sections);
  for J:=1 to 4 do begin
   case J of
    1:I:=Ship.NumberOfStations;2:I:=Ship.NumberOfButtocks;3:I:=Ship.NumberOfWaterlines;4:I:=Ship.NumberOfDiagonals;
   end;
   while I>0 do begin
    Dec(I);case J of
     1:Int:=Ship.Station[I];2:Int:=Ship.Buttock[I];3:Int:=Ship.Waterline[I];4:Int:=Ship.Diagonal[I];
    end;
    Obj:=TJSONObject.Create;Sections.Insert(0,Obj);Obj.Add('kind',J);Obj.Add('order',I);
    Plane:=Int.Plane;Obj.Add('plane',TJSONArray.Create([Double(Plane.a),Double(Plane.b),Double(Plane.c),Double(Plane.d)]));
   end;
  end;
  Text:=Root.AsJSON;
  FileStream:=TFileStream.Create(Directory+'native-input.json',fmCreate);
  try if Text<>'' then FileStream.WriteBuffer(Text[1],Length(Text)) finally FileStream.Free end;
 finally Root.Free end;
 Exe:=ExpandFileName(ExtractFilePath(ParamStr(0))+'../FreeShipNativeLinesplan.exe');
 if not FileExists(Exe) then raise Exception.Create('Native lines-plan component is missing');
 Worker:=TProcess.Create(nil);
 try
  Worker.Executable:=Exe;Worker.Parameters.Add(Directory+'native-input.json');Worker.Parameters.Add(Directory);
  Worker.Parameters.Add(ExpandFileName(Directory+'../../native-lines-cache'));
  Worker.Parameters.Add(IntToStr(GetCurrentProcessId));
  Worker.Options:=[poNoConsole];Worker.ShowWindow:=swoHIDE;Worker.Execute;Started:=GetTickCount64;
  while Worker.Running do begin
   if GetTickCount64-Started>120000 then begin Worker.Terminate(1);raise Exception.Create('Native lines-plan generation timed out') end;
   Sleep(10);
  end;
  if Worker.ExitStatus<>0 then begin
   Text:='Native lines-plan generation failed';
   if FileExists(Directory+'native-report.json') then begin
    FileStream:=TFileStream.Create(Directory+'native-report.json',fmOpenRead or fmShareDenyNone);
    try
     if FileStream.Size<1048576 then begin
      Failure:=GetJSON(FileStream);
      try if Failure is TJSONObject then Text:=Text+': '+TJSONObject(Failure).Get('error','') finally Failure.Free end;
     end;
    finally FileStream.Free end;
   end;
   raise Exception.Create(Text);
  end;
 finally Worker.Free end;
end;
end.
