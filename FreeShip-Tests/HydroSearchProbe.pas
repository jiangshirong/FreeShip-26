unit HydroSearchProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure RunHydroSearchProbe(Form:TMainForm);
implementation
uses SysUtils, Classes, Math, fpjson, FreeShipUnit, FreeGeometry, FreeTypes, FreeFileBuffer;

type
  TScan = record
    X, Area, NearMin, NearMax, Step: Double;
    Calls: Integer;
    Milliseconds: QWord;
  end;
  TDoubles = array of Double;

var Root:TJSONObject; Cases,Observations:TJSONArray;

procedure SaveProgress;
var Text:TStringList;
begin
  Text:=TStringList.Create;
  try Text.Text:=Root.FormatJSON; Text.SaveToFile(SysUtils.GetEnvironmentVariable('FS_HSEARCH_REPORT'))
  finally Text.Free end;
end;

procedure Check(const ID,Expected:string; Passed:Boolean; const Actual:string='');
var O:TJSONObject;
begin
  O:=TJSONObject.Create; O.Add('id',ID); O.Add('expected',Expected); O.Add('actual',Actual);
  if Passed then O.Add('status','pass') else O.Add('status','fail'); Cases.Add(O);
end;

procedure Uncertain(const ID,Status,Reason:string);
var O:TJSONObject;
begin
  O:=TJSONObject.Create; O.Add('id',ID); O.Add('status',Status); O.Add('actual',Reason); Cases.Add(O);
end;

procedure Sort(var Values:TDoubles; L,R:Integer);
var I,J:Integer; Pivot,Temp:Double;
begin
  I:=L; J:=R; Pivot:=Values[(L+R) div 2];
  repeat
    while Values[I]<Pivot do Inc(I);
    while Values[J]>Pivot do Dec(J);
    if I<=J then begin Temp:=Values[I]; Values[I]:=Values[J]; Values[J]:=Temp; Inc(I); Dec(J) end;
  until I>J;
  if L<J then Sort(Values,L,J);
  if I<R then Sort(Values,I,R);
end;

procedure ModelSize(S:TFreeShip; O:TJSONObject);
var I,J,K,N,Smooth,Children,Unique:Integer; Xs:TDoubles;
  Face:TFreeSubdivisionFace;
begin
  N:=0; Children:=0; Smooth:=0;
  for I:=0 to S.Surface.NumberOfControlEdges-1 do
    if not S.Surface.ControlEdge[I].Crease then Inc(Smooth);
  for I:=0 to S.Surface.NumberOfControlFaces-1 do
  begin
    Inc(Children,S.Surface.ControlFace[I].ChildCount);
    if not S.Surface.ControlFace[I].Layer.UseInHydrostatics then Continue;
    for J:=0 to S.Surface.ControlFace[I].ChildCount-1 do
      Inc(N,S.Surface.ControlFace[I].Child[J].NumberOfPoints);
  end;
  SetLength(Xs,N); N:=0;
  for I:=0 to S.Surface.NumberOfControlFaces-1 do
  begin
    if not S.Surface.ControlFace[I].Layer.UseInHydrostatics then Continue;
    for J:=0 to S.Surface.ControlFace[I].ChildCount-1 do
    begin
      Face:=S.Surface.ControlFace[I].Child[J];
      for K:=0 to Face.NumberOfPoints-1 do begin Xs[N]:=Face.Point[K].Coordinate.X; Inc(N) end;
    end;
  end;
  Unique:=0;
  if N>0 then
  begin
    Sort(Xs,0,N-1); Unique:=1;
    for I:=1 to N-1 do if Xs[I]<>Xs[I-1] then Inc(Unique);
  end;
  O.Add('control_points',S.Surface.NumberOfControlPoints);
  O.Add('control_faces',S.Surface.NumberOfControlFaces);
  O.Add('child_faces',Children); O.Add('smooth_control_edges',Smooth);
  O.Add('hydro_mesh_unique_x',Unique);
end;

function BuildHull(Narrow:Boolean):TFreeShip;
var XS,YS:TDoubles; S:TFreeShip; I,N:Integer;
  Points:array of array[0..3] of T3DCoordinate;
  A,B:T3DCoordinate; Edge:TFreeSubdivisionControlEdge;
begin
  if Narrow then
  begin
    XS:=TDoubles.Create(0,3,4,5,11.97,11.98,11.99,12,12.01,12.02,12.03,16,20);
    YS:=TDoubles.Create(1,2,3.3,1,1,1,1.2,7,1.2,1,1,2.5,1);
  end
  else
  begin
    XS:=TDoubles.Create(0,2,4,6,9,11,12,14,17,20);
    YS:=TDoubles.Create(1,2,4,1,1.5,4.8,4.5,1,3,1);
  end;
  N:=Length(XS); SetLength(Points,N); S:=TFreeShip.Create(nil);
  try
    S.IsRebuildAsyncDisabled:=True; S.ProjectSettings.DisableModelCheck:=True;
    S.ProjectSettings.EnableModelAutoMove:=False;
    S.ProjectSettings.ProjectLength:=20; S.ProjectSettings.ProjectBeam:=14;
    S.ProjectSettings.ProjectDraft:=3; S.ProjectSettings.ProjectCoefficients:=fcActualData;
    for I:=0 to N-1 do
    begin
      Points[I,0]:=SetPoint(XS[I],0,0); Points[I,1]:=SetPoint(XS[I],YS[I],0);
      Points[I,2]:=SetPoint(XS[I],YS[I],8); Points[I,3]:=SetPoint(XS[I],0,8);
    end;
    for I:=0 to N-2 do
    begin
      S.Surface.AddControlFace([Points[I,0],Points[I,1],Points[I+1,1],Points[I+1,0]],4).Layer:=S.Layer[0];
      S.Surface.AddControlFace([Points[I,1],Points[I,2],Points[I+1,2],Points[I+1,1]],4).Layer:=S.Layer[0];
    end;
    S.Surface.AddControlFace([Points[0,0],Points[0,3],Points[0,2],Points[0,1]],4).Layer:=S.Layer[0];
    S.Surface.AddControlFace([Points[N-1,0],Points[N-1,1],Points[N-1,2],Points[N-1,3]],4).Layer:=S.Layer[0];
    for I:=0 to S.Surface.NumberOfControlEdges-1 do
    begin
      Edge:=S.Surface.ControlEdge[I]; A:=Edge.StartPoint.Coordinate; B:=Edge.EndPoint.Coordinate;
      // Smooth the interior transverse rings, retaining the bottom chine and end caps.
      Edge.Crease:=not ((Abs(A.X-B.X)<1e-6) and (A.X>0) and (A.X<20));
    end;
    S.Layer[0].Symmetric:=True; S.Layer[0].UseInHydrostatics:=True;
    S.Precision:=fpMedium; S.ModelIsLoaded:=True; S.Surface.Rebuild;
    Result:=S;
  except S.Free; raise end;
end;

function SectionArea(Frame:TFreeIntersection; X:Double; Waterline:T3DPlane):Double;
var Plane:T3DPlane; Area:TFloatType; COG:T3DCoordinate; MOI:T2DCoordinate;
begin
  Plane.a:=1; Plane.b:=0; Plane.c:=0; Plane.d:=-X;
  Frame.Plane:=Plane; Frame.Built:=False;
  Frame.CalculateArea(Waterline,Area,COG,MOI); Result:=Area;
end;

function Scan(Frame:TFreeIntersection; Waterline:T3DPlane; Left,Right:Double; Steps:Integer):TScan;
var I:Integer; X,Tolerance:Double; Areas:TDoubles; Started:QWord;
begin
  FillChar(Result,SizeOf(Result),0); Result.Area:=-1; Result.Step:=(Right-Left)/Steps;
  SetLength(Areas,Steps+1); Started:=GetTickCount64;
  for I:=0 to Steps do
  begin
    X:=Left+Result.Step*I; Areas[I]:=SectionArea(Frame,X,Waterline);
    if Areas[I]>Result.Area then begin Result.Area:=Areas[I]; Result.X:=X end;
  end;
  Result.Calls:=Steps+1; Result.Milliseconds:=GetTickCount64-Started;
  // A narrow near-maximum band allows a flat top without inventing an exact X.
  Tolerance:=Max(1e-4,Abs(Result.Area)*5e-5); Result.NearMin:=Result.X; Result.NearMax:=Result.X;
  for I:=0 to Steps do if Areas[I]>=Result.Area-Tolerance then
  begin X:=Left+Result.Step*I; Result.NearMin:=Min(Result.NearMin,X); Result.NearMax:=Max(Result.NearMax,X) end;
end;

function ScanJSON(const S:TScan):TJSONObject;
begin
  Result:=TJSONObject.Create; Result.Add('x',S.X); Result.Add('area',S.Area);
  Result.Add('near_min_x',S.NearMin); Result.Add('near_max_x',S.NearMax);
  Result.Add('step',S.Step); Result.Add('calls',S.Calls); Result.Add('milliseconds',S.Milliseconds);
end;

procedure RunOne(S:TFreeShip; const Name:string; Draft,WindowLeft,WindowRight:Double; Synthetic:Boolean; Narrow:Boolean=False);
var H:TFreeHydrostaticCalc; Frame:TFreeIntersection; O:TJSONObject;
  Started,Elapsed,Calls:QWord; GlobalScan,Coarse,Fine:TScan;
  Left,Right,LengthX,Tol,AtReported,FirstPeak,SecondPeak,Valley:Double;
  InitialX,InitialArea,InitialVolume:TFloatType;
  Smooth,I:Integer; Converged,RepeatStable:Boolean; Timings:TJSONArray;
begin
  O:=TJSONObject.Create; O.Add('name',Name); O.Add('stage','calculate'); Observations.Add(O);
  ModelSize(S,O); SaveProgress;
  H:=TFreeHydrostaticCalc.Create(S); Frame:=TFreeIntersection.Create(S);
  try
    H.Draft:=Draft; HydroSearchAreaCalls:=0; Started:=GetTickCount64; H.Calculate;
    Elapsed:=GetTickCount64-Started; Calls:=HydroSearchAreaCalls;
    O.Add('hydro_ms',Elapsed); O.Add('midship_search_area_calls',Calls);
    O.Add('midship_x',H.MidshipLocation); O.Add('midship_area',H.Data.MidshipArea);
    O.Add('volume',H.Data.Volume); O.Add('errors',H.ErrorString); O.Strings['stage']:='reference scan'; SaveProgress;
    Check('07.search.'+Name+'.valid-hydro','calculation completes with positive volume and no hydrostatic error',
      H.Calculated and (H.Errors=[]) and (H.Data.Volume>0),H.ErrorString);
    if not H.Calculated or (H.Errors<>[]) or (H.Data.Volume<=0) then Exit;
    if not Synthetic then
    begin
      InitialX:=H.MidshipLocation; InitialArea:=H.Data.MidshipArea; InitialVolume:=H.Data.Volume;
      Timings:=TJSONArray.Create; O.Add('repeat_hydro_ms',Timings); RepeatStable:=True;
      for I:=1 to 5 do
      begin
        HydroSearchAreaCalls:=0; Started:=GetTickCount64; H.Calculate;
        Timings.Add(GetTickCount64-Started);
        RepeatStable:=RepeatStable and H.Calculated and (H.Errors=[]) and
          (H.MidshipLocation=InitialX) and (H.Data.MidshipArea=InitialArea) and
          (H.Data.Volume=InitialVolume) and (HydroSearchAreaCalls=Calls);
      end;
      Check('07.search.'+Name+'.repeat-stability','five repeated calculations preserve location, area, volume and call count exactly',RepeatStable);
      SaveProgress;
    end;
    if Synthetic then
    begin
      Smooth:=0;
      for I:=0 to S.Surface.NumberOfControlEdges-1 do if not S.Surface.ControlEdge[I].Crease then Inc(Smooth);
      Check('07.search.'+Name+'.real-subdivision','fixture contains non-crease edges and rebuilt child faces',
        (Smooth>0) and (S.Surface.ControlFace[0].ChildCount>1),IntToStr(Smooth));
    end;
    Frame.IntersectionType:=fiStation; Frame.UseHydrostaticsSurfacesOnly:=True;
    Left:=H.Data.ModelMin.X; Right:=H.Data.ModelMax.X; LengthX:=Right-Left;
    Check('07.search.'+Name+'.bounds','finite nonempty longitudinal interval',
      not IsNan(Left) and not IsNan(Right) and (LengthX>0),FloatToStr(Left)+'..'+FloatToStr(Right));
    if LengthX<=0 then Exit;
    GlobalScan:=Scan(Frame,H.WaterlinePlane,Left,Right,512); O.Add('global_512',ScanJSON(GlobalScan)); SaveProgress;
    if not Synthetic then
    begin
      // On an arbitrary real model this is a bounded diagnostic, not a global proof.
      WindowLeft:=Max(Left,Min(H.MidshipLocation,GlobalScan.X)-LengthX/64);
      WindowRight:=Min(Right,Max(H.MidshipLocation,GlobalScan.X)+LengthX/64);
    end;
    Coarse:=Scan(Frame,H.WaterlinePlane,WindowLeft,WindowRight,512);
    Fine:=Scan(Frame,H.WaterlinePlane,WindowLeft,WindowRight,1024);
    O.Add('local_512',ScanJSON(Coarse)); O.Add('local_1024',ScanJSON(Fine));
    Tol:=Max(0.001,Abs(Fine.Area)*0.0005);
    Converged:=(Abs(Fine.Area-Coarse.Area)<=Tol/4) and (Fine.Area>0);
    if Converged then Check('07.search.'+Name+'.reference-convergence','doubling local sample density changes peak area by at most one quarter of comparison tolerance',True,
      FloatToStr(Abs(Fine.Area-Coarse.Area)))
    else Uncertain('07.search.'+Name+'.reference-convergence','review','reference scans did not converge; no precise location conclusion');
    AtReported:=SectionArea(Frame,H.MidshipLocation,H.WaterlinePlane);
    O.Add('independent_area_at_reported_x',AtReported);
    Check('07.search.'+Name+'.reported-area','reported area equals an independently rebuilt section at the reported location',
      Abs(AtReported-H.Data.MidshipArea)<=Tol,FloatToStr(AtReported));
    Check('07.search.'+Name+'.sampled-maximum','reported area is not below either independently sampled maximum beyond tolerance',
      H.Data.MidshipArea+Tol>=Max(GlobalScan.Area,Fine.Area),FloatToStr(Max(GlobalScan.Area,Fine.Area)));
    if Synthetic then
    begin
      Check('07.search.'+Name+'.window-covers-global-peak','known geometric feature window reaches the global coarse-scan peak',
        Fine.Area+Tol>=GlobalScan.Area);
      if Converged then
        Check('07.search.'+Name+'.location','reported location is within the independently sampled near-maximum band',
          (H.MidshipLocation>=Fine.NearMin-Fine.Step*2) and (H.MidshipLocation<=Fine.NearMax+Fine.Step*2),
          FloatToStr(H.MidshipLocation)+' in '+FloatToStr(Fine.NearMin)+'..'+FloatToStr(Fine.NearMax));
      if Narrow then
      begin
        FirstPeak:=SectionArea(Frame,11.95,H.WaterlinePlane); SecondPeak:=SectionArea(Frame,12.05,H.WaterlinePlane);
        Check('07.search.'+Name+'.narrow-feature','local peak area exceeds both sections 0.05 away by ten percent',
          Fine.Area>1.1*Max(FirstPeak,SecondPeak),FloatToStr(FirstPeak)+' / '+FloatToStr(SecondPeak));
      end
      else
      begin
        FirstPeak:=SectionArea(Frame,4,H.WaterlinePlane); SecondPeak:=SectionArea(Frame,11.5,H.WaterlinePlane);
        Valley:=SectionArea(Frame,7,H.WaterlinePlane);
        Check('07.search.'+Name+'.multiple-peaks','two separated high-area regions are divided by a lower-area valley',
          (FirstPeak>Valley*1.1) and (SecondPeak>Valley*1.1),FloatToStr(FirstPeak)+' / '+FloatToStr(Valley)+' / '+FloatToStr(SecondPeak));
      end;
    end
    else Uncertain('07.search.'+Name+'.global-guarantee','review','global 512 samples plus two local grids are evidence, not proof of the true global maximum');
    O.Strings['stage']:='complete'; SaveProgress;
  finally Frame.Free; H.Free end;
end;

procedure ExcludedLayerCase;
var S:TFreeShip; H:TFreeHydrostaticCalc; Layer:TFreeSubdivisionLayer;
  I:Integer; X,BeforeX,BeforeArea,BeforeVolume:TFloatType;
  BeforeCalls,AfterCalls:QWord; ValidBefore:Boolean; O:TJSONObject;
begin
  S:=BuildHull(False); H:=TFreeHydrostaticCalc.Create(S);
  O:=TJSONObject.Create; O.Add('name','excluded-layer'); Observations.Add(O);
  try
    H.Draft:=3; HydroSearchAreaCalls:=0; H.Calculate;
    BeforeCalls:=HydroSearchAreaCalls; BeforeX:=H.MidshipLocation;
    BeforeArea:=H.Data.MidshipArea; BeforeVolume:=H.Data.Volume;
    ValidBefore:=H.Calculated and (H.Errors=[]) and (BeforeVolume>0);
    O.Add('before_x',BeforeX); O.Add('before_area',BeforeArea);
    O.Add('before_volume',BeforeVolume); O.Add('before_search_calls',BeforeCalls);
    Layer:=S.Surface.AddNewLayer;
    Layer.UseInHydrostatics:=False; Layer.Symmetric:=False;
    for I:=0 to 99 do
    begin
      // Interior abscissae expose accidental sampling of excluded control points.
      // Every face is disconnected from, and far above/beside, the hull.
      X:=0.123+I*0.187;
      S.Surface.AddControlFace([SetPoint(X,-30,20),SetPoint(X+0.003,-30,20),
        SetPoint(X+0.003,-29.5,20),SetPoint(X,-29.5,20)],4).Layer:=Layer;
    end;
    S.Surface.Rebuild;
    H.Free; H:=nil; H:=TFreeHydrostaticCalc.Create(S);
    H.Draft:=3; HydroSearchAreaCalls:=0; H.Calculate; AfterCalls:=HydroSearchAreaCalls;
    O.Add('after_x',H.MidshipLocation); O.Add('after_area',H.Data.MidshipArea);
    O.Add('after_volume',H.Data.Volume); O.Add('after_search_calls',AfterCalls);
    ModelSize(S,O);
    Check('07.search.excluded-layer.fixture','100 rebuilt faces excluded from hydrostatics; both calculations remain valid',
      ValidBefore and (Layer.Count=100) and not Layer.UseInHydrostatics and
      (Layer.Items[0].ChildCount>0) and H.Calculated and (H.Errors=[]),H.ErrorString);
    Check('07.search.excluded-layer.location','excluded geometry leaves the maximum-section abscissa exactly unchanged',
      H.MidshipLocation=BeforeX,FloatToStr(BeforeX)+' -> '+FloatToStr(H.MidshipLocation));
    Check('07.search.excluded-layer.area-volume','excluded geometry leaves maximum-section area and displaced volume exactly unchanged',
      (H.Data.MidshipArea=BeforeArea) and (H.Data.Volume=BeforeVolume),
      FloatToStr(BeforeArea)+' -> '+FloatToStr(H.Data.MidshipArea)+'; '+FloatToStr(BeforeVolume)+' -> '+FloatToStr(H.Data.Volume));
    Check('07.search.excluded-layer.calls','excluded control-point abscissae do not add maximum-section area evaluations',
      AfterCalls=BeforeCalls,IntToStr(BeforeCalls)+' -> '+IntToStr(AfterCalls));
    SaveProgress;
  finally H.Free; S.Free end;
end;

procedure RunHydroSearchProbe(Form:TMainForm);
var S:TFreeShip; Source:TFreeFileBuffer; ModelName:string;
begin
  Root:=TJSONObject.Create; Cases:=TJSONArray.Create; Observations:=TJSONArray.Create;
  Root.Add('cases',Cases); Root.Add('observations',Observations);
  try
    try
      S:=BuildHull(False);
      try RunOne(S,'smooth-multiple-peaks',3,9,14,True) finally S.Free end;
      S:=BuildHull(True);
      try RunOne(S,'smooth-narrow-peak',3,11.95,12.05,True,True) finally S.Free end;
      ExcludedLayerCase;
      ModelName:=SysUtils.GetEnvironmentVariable('FS_HSEARCH_MODEL');
      if ModelName<>'' then
      begin
        S:=TFreeShip.Create(nil); Source:=nil;
        try
          S.IsRebuildAsyncDisabled:=True;
          if LowerCase(ExtractFileExt(ModelName))='.ftm' then Source:=TFreeTextBuffer.Create else Source:=TFreeFileBuffer.Create;
          Source.LoadFromFile(ModelName); S.LoadProject(Source);
          S.ProjectSettings.EnableModelAutoMove:=False; S.Surface.Rebuild;
          if S.ProjectSettings.ProjectDraft>0 then
            RunOne(S,'real-model',S.ProjectSettings.ProjectDraft,0,0,False)
          else Uncertain('07.search.real-model.draft','not_run','the supplied model has no positive project draft');
        finally Source.Free; S.Free end;
      end;
    except on E:Exception do Root.Add('error',E.ClassName+': '+E.Message) end;
    SaveProgress;
  finally Root.Free end;
end;
end.
