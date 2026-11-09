// Capture model/layout coordinates before rasterization. The included original
// routines are extracted and checked by native_linesplan_observer.py.
type
 TLinesplanView=(lvProfile,lvAftBody,lvFrontBody,lvPlan);
 TLinesplanViews=set of TLinesplanView;
 TProbeViewport=class
  Pending:TJSONArray;
  Paths:TJSONArray;
  constructor Create;
  destructor Destroy;override;
  function Project(P:T3DCoordinate):TPoint;
  procedure Polyline(const Points:array of TPoint);
 end;
constructor TProbeViewport.Create;
begin inherited Create;Pending:=TJSONArray.Create;Paths:=TJSONArray.Create end;
destructor TProbeViewport.Destroy;
begin Pending.Free;Paths.Free;inherited Destroy end;
function TProbeViewport.Project(P:T3DCoordinate):TPoint;
begin Pending.Add(PointJSON(P));Result:=Types.Point(0,0) end;
procedure TProbeViewport.Polyline(const Points:array of TPoint);
begin
 if Length(Points)<>Pending.Count then raise Exception.Create('Projection capture count differs');
 Paths.Add(Pending);Pending:=TJSONArray.Create;
end;

function RunLinesplan(Data:TJSONObject):TJSONObject;
var S:TFreeSpline;Item:TJSONObject;Points,Knuckles,Sampled,DXFPaths,DXFPending:TJSONArray;
 I:Integer;Values:T3DCoordinateArray;View:TLinesplanView;Viewport:TProbeViewport;Strings:TStringList;
 FProfileOrigin,FAftOrigin,FFrontOrigin,FPlanOrigin:T3DCoordinate;MidshipLocation:TFloatType;
 MirrorPlanview:record Checked:Boolean end;
 Freeship:record NumberofDiagonals:Integer end;
 procedure CaptureDXFStart;
 begin DXFPending:=TJSONArray.Create;DXFPaths.Add(DXFPending) end;
 procedure CaptureDXFPoint(P:T3DCoordinate);
 begin DXFPending.Add(PointJSON(P)) end;
 {$I LegacyLinesplanDraw.inc}
 {$I LegacyLinesplanDXF.inc}
begin
 Result:=TJSONObject.Create;S:=TFreeSpline.Create(nil);Viewport:=TProbeViewport.Create;Strings:=TStringList.Create;
 try
  Item:=Obj(Data.Find('chain'));Points:=Arr(Item.Find('points'));Knuckles:=Arr(Item.Find('knuckles'));
  if Points.Count<>Knuckles.Count then raise Exception.Create('Knuckle count mismatch');
  for I:=0 to Points.Count-1 do begin S.Add(PointValue(Points[I]));S.Knuckle[I]:=Knuckles[I].AsBoolean end;
  S.Fragments:=IntVal(Item.Find('fragments'),2,1000000);
  FProfileOrigin:=PointValue(Data.Find('origin'));FAftOrigin:=FProfileOrigin;
  FFrontOrigin:=FProfileOrigin;FPlanOrigin:=FProfileOrigin;
  MidshipLocation:=FloatField(Data,'split',0);MirrorPlanview.Checked:=Flag(Data,'mirror_plan',False);
  Freeship.NumberofDiagonals:=Ord(Flag(Data,'has_diagonals',False));View:=TLinesplanView(IntVal(Data.Find('view'),0,3));
  Values:=S.GetValues;Sampled:=TJSONArray.Create;for I:=0 to High(Values) do Sampled.Add(PointJSON(Values[I]));
  Result.Add('samples',Sampled);
  DrawSpline(S,[View],psSolid);Result.Add('display_paths',GetJSON(Viewport.Paths.AsJSON));
  DXFPaths:=TJSONArray.Create;Result.Add('dxf_paths',DXFPaths);
  AddSpline(S,[View],'test',clBlack);
  Result.Add('fragments_after_export',S.ProbeRawFragments);
  Viewport.Free;Viewport:=nil;Viewport:=TProbeViewport.Create;
  DrawSpline(S,[View],psSolid);Result.Add('display_after_export',GetJSON(Viewport.Paths.AsJSON));
  Result.Add('status','ok');
 finally Strings.Free;Viewport.Free;S.Free end;
end;
