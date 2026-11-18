{ Qt hydrostatics adapter. Original integration: FREE!ship/Plus, GPL-2.0-or-later.
  Bounded waterplane and longitudinal equilibrium solvers are kept separate. }
unit HydrostaticsEngine;
{$mode objfpc}{$H+}{$codepage utf8}
interface
uses Classes,SysUtils,Math,fpjson,fgl,FreeShipUnit,FreeGeometry,FreeTypes;
function CalculateHydrostatics(Ship:TFreeShip;Request:TJSONObject;const CancelFile:string):TJSONObject;
implementation
type
  TSideMap=specialize TFPGMap<PtrUInt,Byte>;
  THydroVertex=record Point:T3DCoordinate;Leak:Boolean end;
  THydroJob=class
    Ship:TFreeShip;Calc:TFreeHydrostaticCalc;CancelFile:string;Started:QWord;
    Keel:Double;Evaluations:Integer;Prepared,HasAsymmetricLayer:Boolean;
    Vertices:array of THydroVertex;
    procedure Check;
    procedure PrepareBounds;
    procedure Bounds(const Normal:T3DCoordinate;out Low,High,Leak:Double);
    function Evaluate(const Normal:T3DCoordinate;Offset:Double):TFreeHydrostaticsData;
    function Balance(Mass,Heel,Trim:Double;FreeTrim:Boolean;CG:T3DCoordinate):TJSONObject;
  end;
function Number(O:TJSONObject;const Key:string;Low,High:Double):Double;
var V:TJSONData;
begin
  V:=O.Find(Key);if (V=nil) or (V.JSONType<>jtNumber) then raise Exception.Create('缺少数值：'+Key);
  Result:=V.AsFloat;
  if IsNan(Result) or IsInfinite(Result) or (Result<Low) or (Result>High) then raise Exception.Create('数值超出范围：'+Key);
end;
function Numbers(O:TJSONObject;const Key:string;Low,High:Double;Limit:Integer):TJSONArray;
var V:TJSONData;I:Integer;D,Prev:Double;
begin
  V:=O.Find(Key);if (V=nil) or (V.JSONType<>jtArray) then raise Exception.Create('缺少数值列表：'+Key);
  Result:=TJSONArray(V);if (Result.Count<1) or (Result.Count>Limit) then raise Exception.Create('列表长度超出范围：'+Key);
  Prev:=-1e300;
  for I:=0 to Result.Count-1 do begin
    if Result[I].JSONType<>jtNumber then raise Exception.Create('列表含非数值：'+Key);
    D:=Result[I].AsFloat;
    if IsNan(D) or IsInfinite(D) or (D<Low) or (D>High) or (D<=Prev) then raise Exception.Create('列表必须递增、无重复且在有效范围内：'+Key);
    Prev:=D;
  end;
end;
function VectorJSON(P:T3DCoordinate):TJSONArray;
begin
  if IsNan(P.X) or IsInfinite(P.X) or IsNan(P.Y) or IsInfinite(P.Y) or IsNan(P.Z) or IsInfinite(P.Z) then
    raise Exception.Create('计算产生无效坐标。');
  Result:=TJSONArray.Create([Double(P.X),Double(P.Y),Double(P.Z)]);
end;
procedure AddNumber(O:TJSONObject;const Key:string;D:Double);
begin
  if IsNan(D) or IsInfinite(D) then raise Exception.Create('计算产生无效数值：'+Key);
  O.Add(Key,D);
end;
function HydroData(C:TFreeHydrostaticCalc;Keel:Double;Asymmetric:Boolean):TJSONObject;
const SymmetricOnly:array[0..14] of string=('midship_area','cm','cp','cvp','waterplane_area','cwp',
  'waterplane_cog','waterplane_ix','waterplane_iy','kmt','kml','lateral_area','lateral_cog','sectional_areas','cb');
var D:TFreeHydrostaticsData;A:TJSONArray;I:Integer;
begin
  D:=C.Data;Result:=TJSONObject.Create;
  try
    Result.Add('status','ok');Result.Add('buoyancy',VectorJSON(D.CenterOfBuoyancy));
    Result.Add('plane',TJSONArray.Create([Double(D.WaterlinePlane.a),Double(D.WaterlinePlane.b),Double(D.WaterlinePlane.c),Double(D.WaterlinePlane.d)]));
    AddNumber(Result,'keel_z',Keel);AddNumber(Result,'draft',C.Draft);AddNumber(Result,'heel',C.HeelingAngle);AddNumber(Result,'trim',C.Trim);
    AddNumber(Result,'absolute_draft',D.AbsoluteDraft);AddNumber(Result,'volume',D.Volume);AddNumber(Result,'displacement',D.Displacement);
    AddNumber(Result,'lwl',D.LengthWaterline);AddNumber(Result,'bwl',D.BeamWaterline);AddNumber(Result,'wetted_area',D.WettedSurface);
    AddNumber(Result,'cb',D.BlockCoefficient);AddNumber(Result,'cp',D.PrismCoefficient);AddNumber(Result,'cvp',D.VertPrismCoefficient);
    AddNumber(Result,'midship_x',C.MidshipLocation);AddNumber(Result,'midship_area',D.MidshipArea);AddNumber(Result,'cm',D.MidshipCoeff);
    AddNumber(Result,'waterplane_area',D.Waterplanearea);AddNumber(Result,'cwp',D.WaterplaneCoeff);
    Result.Add('waterplane_cog',VectorJSON(D.WaterplaneCOG));
    AddNumber(Result,'waterplane_ix',D.WaterplaneMomInertia.X);AddNumber(Result,'waterplane_iy',D.WaterplaneMomInertia.Y);
    AddNumber(Result,'kmt',D.KMtransverse);AddNumber(Result,'kml',D.KMlongitudinal);
    AddNumber(Result,'lateral_area',D.LateralArea);Result.Add('lateral_cog',VectorJSON(D.LateralCOG));
    AddNumber(Result,'weight',D.Weight_);Result.Add('weight_cog',VectorJSON(D.CenterOfGravity_));
    A:=TJSONArray.Create;Result.Add('sectional_areas',A);
    for I:=0 to High(D.SAC) do A.Add(TJSONArray.Create([Double(D.SAC[I].X),Double(D.SAC[I].Y)]));
    if Asymmetric then begin
      // Original section/area integration assumes mirrored section contours.
      // Volume integration and the independent balance path do not. Never
      // present the known-invalid derived values as valid asymmetric results.
      for I:=0 to High(SymmetricOnly) do begin Result.Delete(SymmetricOnly[I]);Result.Add(SymmetricOnly[I],TJSONNull.Create) end;
      Result.Add('message','排水体积、排水量和浮心有效；非对称船体的剖面面积、系数和稳心数据尚未提供。');
      Result.Add('partial',True);
    end;
  except Result.Free;raise end;
end;
procedure THydroJob.Check;
begin
  if FileExists(CancelFile) then raise EAbort.Create('计算已取消。');
  if GetTickCount64-Started>120000 then raise EAbort.Create('计算超过两分钟，请减少工况后重试。');
end;
procedure THydroJob.PrepareBounds;
var Faces,Points:TSideMap;I,J,K,S,Index,Count,PortCount,StarboardCount,N:Integer;Mask:Byte;
    F:TFreeSubdivisionControlFace;C:TFreeSubdivisionFace;P:TFreeSubdivisionPoint;E:TFreeSubdivisionEdge;
    Boundary,MirrorSeam:Boolean;
begin
  Faces:=TSideMap.Create;Points:=TSideMap.Create;
  try
    Faces.Sorted:=True;Points.Sorted:=True;
    for I:=0 to Ship.Surface.NumberOfControlFaces-1 do begin
      F:=Ship.Surface.ControlFace[I];if not F.Layer.UseInHydrostatics then Continue;
      if not F.Layer.Symmetric then HasAsymmetricLayer:=True;
      Mask:=1;if F.Layer.Symmetric then Mask:=3;
      for J:=0 to F.ChildCount-1 do begin
        C:=F.Child[J];Faces.Add(PtrUInt(C),Mask);
        for K:=0 to C.NumberOfPoints-1 do begin
          Index:=Points.IndexOf(PtrUInt(C.Point[K]));
          if Index<0 then Points.Add(PtrUInt(C.Point[K]),Mask) else Points.Data[Index]:=Points.Data[Index] or Mask;
        end;
      end;
    end;
    SetLength(Vertices,Points.Count*2);N:=0;
    for I:=0 to Points.Count-1 do begin
      if (I mod 1024)=0 then Check;
      P:=TFreeSubdivisionPoint(Points.Keys[I]);
      for S:=0 to 1 do if (Points.Data[I] and (1 shl S))<>0 then begin
        Boundary:=False;
        for J:=0 to P.NumberOfEdges-1 do begin
          E:=P.Edge[J];PortCount:=0;StarboardCount:=0;
          for K:=0 to E.NumberOfFaces-1 do begin
            Index:=Faces.IndexOf(PtrUInt(E.Face[K]));if Index<0 then Continue;
            if (Faces.Data[Index] and 1)<>0 then Inc(PortCount);
            if (Faces.Data[Index] and 2)<>0 then Inc(StarboardCount);
          end;
          if S=0 then Count:=PortCount else Count:=StarboardCount;
          MirrorSeam:=(Abs(E.StartPoint.Coordinate.Y)<=1e-4) and (Abs(E.EndPoint.Coordinate.Y)<=1e-4) and
            (PortCount=1) and (StarboardCount=1);
          // The old boundary getters exclude Y=0 unconditionally, even for
          // non-symmetric layers. Count only participating faces on each side.
          if (Count=1) and not MirrorSeam then Boundary:=True;
          if Count>2 then raise Exception.Create('参与计算的船体有非流形边，无法确定封闭体积。');
        end;
        Vertices[N].Point:=P.Coordinate;if S=1 then Vertices[N].Point.Y:=-Vertices[N].Point.Y;
        Vertices[N].Leak:=Boundary;Inc(N);
      end;
    end;
    SetLength(Vertices,N);Prepared:=True;
  finally Points.Free;Faces.Free end;
end;
procedure THydroJob.Bounds(const Normal:T3DCoordinate;out Low,High,Leak:Double);
var I:Integer;P:T3DCoordinate;V:Double;
begin
  if not Prepared then PrepareBounds;
  Low:=1e300;High:=-1e300;Leak:=1e300;
  for I:=0 to Length(Vertices)-1 do begin
    P:=Vertices[I].Point;V:=Double(P.X)*Normal.X+Double(P.Y)*Normal.Y+Double(P.Z)*Normal.Z;
    Low:=Math.Min(Low,V);High:=Math.Max(High,V);
    if Vertices[I].Leak then Leak:=Math.Min(Leak,V);
  end;
  if High<=Low then raise Exception.Create('参与静水力计算的船体没有有效体积范围。');
end;
function THydroJob.Evaluate(const Normal:T3DCoordinate;Offset:Double):TFreeHydrostaticsData;
begin
  Check;Inc(Evaluations);Calc.CalculateVolume(SetPlane(Normal.X,Normal.Y,Normal.Z,-Offset));Result:=Calc.Data;
  if (feMakingWater in Calc.Errors) then raise Exception.Create('水线以下有漏点。');
  if IsNan(Result.Displacement) or IsInfinite(Result.Displacement) or (Result.Displacement<0) then raise Exception.Create('排水量无效，请检查面的朝向和船体闭合情况。');
end;
function THydroJob.Balance(Mass,Heel,Trim:Double;FreeTrim:Boolean;CG:T3DCoordinate):TJSONObject;
var N:T3DCoordinate;D:TFreeHydrostaticsData;G:T3DCoordinate;
    H,T,Lo,Hi,Mid,Leak,Offset,MassTol,LengthTol,Residual,Left,Right,RL,RR,BestTrim:Double;
    I,J:Integer;InitialValid:Boolean;
  procedure SolveAt(TrimValue:Double);
  var K:Integer;V:Double;BestError,BestOffset:Double;
  begin
    Check;Calc.HeelingAngle:=Heel;Calc.Trim:=TrimValue;
    H:=DegToRad(-Double(Calc.HeelingAngle));T:=DegToRad(-Double(Calc.TrimAngle));
    N:=SetPoint(-Sin(T),Sin(H)*Cos(T),Cos(H)*Cos(T));Bounds(N,Lo,Hi,Leak);
    if Leak<Hi then Hi:=Leak-Math.Max(1e-6,(Hi-Lo)*1e-7);
    if Hi<=Lo then raise Exception.Create('该横倾角下，船体在形成有效排水量之前已进水。');
    D:=Evaluate(N,Hi);
    if D.Displacement<Mass-MassTol then raise Exception.Create('目标排水量超过当前横倾下的完整浮力范围。');
    BestError:=1e300;BestOffset:=Hi;
    for K:=1 to 64 do begin
      Mid:=0.5*(Lo+Hi);D:=Evaluate(N,Mid);V:=D.Displacement-Mass;
      if Abs(V)<BestError then begin BestError:=Abs(V);BestOffset:=Mid end;
      if Abs(V)<=MassTol then Break;
      if V<0 then Lo:=Mid else Hi:=Mid;
    end;
    Offset:=BestOffset;D:=Evaluate(N,Offset);
    if Abs(D.Displacement-Mass)>MassTol then raise Exception.Create('排水量求解未收敛，未返回近似成功值。');
    G.X:=CG.X*Cos(T)+(CG.Y*Sin(H)+(CG.Z-Keel)*Cos(H))*Sin(T);
    G.Y:=CG.Y*Cos(H)-(CG.Z-Keel)*Sin(H);
    G.Z:=-CG.X*Sin(T)+(CG.Y*Sin(H)+(CG.Z-Keel)*Cos(H))*Cos(T);
    Residual:=D.CenterOfBuoyancy.X-G.X;
  end;
begin
  MassTol:=Math.Max(Mass*2e-6,1e-6);LengthTol:=Math.Max(Ship.ProjectSettings.ProjectLength*2e-6,1e-6);
  InitialValid:=False;
  try SolveAt(Trim);InitialValid:=True
  except on E:EAbort do raise;on E:Exception do if not FreeTrim then raise end;
  BestTrim:=Trim;
  if FreeTrim and (not InitialValid or (Abs(Residual)>LengthTol)) then begin
    // Scan bounded trim intervals before bisection: do not extrapolate through a
    // flooded or impossible displacement state. CG is transformed with the hull.
    Left:=0;RL:=0;J:=0;
    for I:=0 to 40 do begin
      Right:=Ship.ProjectSettings.ProjectLength*(-0.5+I/40);
      try SolveAt(Right) except on E:EAbort do raise;on E:Exception do begin J:=0;Continue end end;
      RR:=Residual;
      if Abs(RR)<=LengthTol then begin BestTrim:=Right;J:=2;Break end;
      if (J=1) and (RL*RR<0) then begin J:=2;Break end;
      Left:=Right;RL:=RR;J:=1;
    end;
    if J<>2 then raise Exception.Create('未找到纵倾平衡，请检查重心和排水量。');
    if Abs(Residual)>LengthTol then for I:=1 to 48 do begin
      BestTrim:=0.5*(Left+Right);SolveAt(BestTrim);
      if Abs(Residual)<=LengthTol then Break;
      if Residual*RL>0 then begin Left:=BestTrim;RL:=Residual end else Right:=BestTrim;
    end;
    if Abs(Residual)>LengthTol then raise Exception.Create('纵倾平衡未收敛。');
  end;
  Result:=TJSONObject.Create;
  try
    Result.Add('status','ok');AddNumber(Result,'target_displacement',Mass);AddNumber(Result,'heel',Heel);
    AddNumber(Result,'trim',Calc.Trim);AddNumber(Result,'displacement',D.Displacement);AddNumber(Result,'volume',D.Volume);
    AddNumber(Result,'mass_residual',D.Displacement-Mass);AddNumber(Result,'longitudinal_residual',Residual);
    AddNumber(Result,'mass_tolerance',MassTol);AddNumber(Result,'length_tolerance',LengthTol);
    AddNumber(Result,'absolute_draft',D.AbsoluteDraft);AddNumber(Result,'keel_z',Keel);
    AddNumber(Result,'kn',D.CenterOfBuoyancy.Y);AddNumber(Result,'gz',D.CenterOfBuoyancy.Y-G.Y);
    Result.Add('buoyancy',VectorJSON(D.CenterOfBuoyancy));Result.Add('gravity_projected',VectorJSON(G));
    Result.Add('plane',TJSONArray.Create([Double(N.X),Double(N.Y),Double(N.Z),-Offset]));
    Result.Add('evaluations',Evaluations);
  except Result.Free;raise end;
end;
function CalculateHydrostatics(Ship:TFreeShip;Request:TJSONObject;const CancelFile:string):TJSONObject;
var Job:THydroJob;Mode:string;Drafts,Angles,Masses,Rows:TJSONArray;Row,Inputs:TJSONObject;
    Draft,Heel,Trim,Start,Stop,Step,Low,High,Leak,PreviousAngle,PreviousGZ,Integral:Double;
    CG,N:T3DCoordinate;I,J,Count:Integer;FreeTrim,PreviousValid:Boolean;D:TFreeHydrostaticsData;
    Auxiliary:TStringList;AuxJSON,AuxFiles:TJSONObject;Notice:string;
begin
  Mode:=Request.Get('mode','');if (Mode<>'single') and (Mode<>'table') and (Mode<>'cross') then raise Exception.Create('未知静水力计算类型。');
  if Ship.ProjectSettings.ProjectLength<=0 then raise Exception.Create('请先设置有效船长。');
  Trim:=Number(Request,'trim',-Ship.ProjectSettings.ProjectLength,Ship.ProjectSettings.ProjectLength);
  Drafts:=nil;Angles:=nil;Masses:=nil;CG:=ZERO;FreeTrim:=False;
  if Mode='single' then begin Draft:=Number(Request,'draft',0,1e6);Heel:=Number(Request,'heel',-85,85) end
  else if Mode='table' then begin
    Start:=Number(Request,'start',0,1e6);Stop:=Number(Request,'end',Start,1e6);Step:=Number(Request,'step',1e-6,1e6);
    if (Stop-Start)/Step>199+1e-8 then raise Exception.Create('吃水表最多包含 200 行。');
    Count:=Floor((Stop-Start)/Step+1e-8)+1;Drafts:=TJSONArray.Create;
    for I:=0 to Count-1 do Drafts.Add(Start+I*Step);Heel:=0;
  end else begin
    Angles:=Numbers(Request,'angles',-180,180,181);Masses:=Numbers(Request,'displacements',1e-6,1e9,100);
    if Angles.Count*Masses.Count>500 then raise Exception.Create('稳性表最多包含 500 个工况。');
    CG:=SetPoint(Number(Request,'lcg',-1e9,1e9),Number(Request,'tcg',-1e9,1e9),Number(Request,'vcg',-1e9,1e9));
    if (Request.Find('free_trim')=nil) or (Request.Find('free_trim').JSONType<>jtBoolean) then raise Exception.Create('缺少纵倾模式。');
    FreeTrim:=Request.Booleans['free_trim'];
  end;
  Job:=THydroJob.Create;Result:=TJSONObject.Create;
  try
   try
    Job.Ship:=Ship;Job.CancelFile:=CancelFile;Job.Started:=GetTickCount64;Job.Check;
    if not Ship.Built then Ship.RebuildModel;
    Job.Keel:=Ship.FindLowestHydrostaticsPoint;Job.Calc:=TFreeHydrostaticCalc.Create(Ship);
    Result.Add('schema',1);Result.Add('mode',Mode);Result.Add('keel_z',Job.Keel);
    if Ship.ProjectSettings.ProjectUnits=fuMetric then Result.Add('units','metric') else Result.Add('units','imperial');
    Result.Add('water_density',Double(Ship.ProjectSettings.ProjectWaterDensity));Result.Add('appendage_coefficient',Double(Ship.ProjectSettings.ProjectAppendageCoefficient));
    Inputs:=TJSONObject.Create;
    for I:=0 to Request.Count-1 do
      if (Request.Names[I]='mode') or (Request.Names[I]='model_name') or (Request.Names[I]='draft') or
         (Request.Names[I]='heel') or (Request.Names[I]='trim') or (Request.Names[I]='start') or
         (Request.Names[I]='end') or (Request.Names[I]='step') or (Request.Names[I]='angles') or
         (Request.Names[I]='displacements') or (Request.Names[I]='lcg') or (Request.Names[I]='tcg') or
         (Request.Names[I]='vcg') or (Request.Names[I]='free_trim') then
        Inputs.Add(Request.Names[I],Request.Items[I].Clone);
    Result.Add('inputs',Inputs);
    Rows:=TJSONArray.Create;Result.Add('rows',Rows);Result.Add('cancelled',False);
    try
      if Mode='cross' then begin
        for I:=0 to Masses.Count-1 do begin PreviousValid:=False;Integral:=0;PreviousAngle:=0;PreviousGZ:=0;
          for J:=0 to Angles.Count-1 do begin Job.Check;
            try
              Row:=Job.Balance(Masses.Floats[I],Angles.Floats[J],Trim,FreeTrim,CG);
              if PreviousValid then Integral:=Integral+0.5*(PreviousGZ+Row.Floats['gz'])*DegToRad(Angles.Floats[J]-PreviousAngle);
              if (J=0) and (Angles.Floats[J]<>0) then PreviousValid:=False;
              if (J=0) and (Angles.Floats[J]=0) then PreviousValid:=True;
              if PreviousValid then Row.Add('gz_integral',Integral) else Row.Add('gz_integral',TJSONNull.Create);
              PreviousAngle:=Angles.Floats[J];PreviousGZ:=Row.Floats['gz'];
            except on E:EAbort do raise;on E:Exception do begin
              PreviousValid:=False;Row:=TJSONObject.Create(['status','error','message',E.Message,'heel',Angles.Floats[J],'target_displacement',Masses.Floats[I]]);
            end end;
            Rows.Add(Row);
          end;
        end;
      end else begin
        if Mode='single' then Count:=1 else Count:=Drafts.Count;
        for I:=0 to Count-1 do begin Job.Check;if Mode='table' then Draft:=Drafts.Floats[I];
          try
            Job.Calc.Clear;Job.Calc.Draft:=Draft;Job.Calc.HeelingAngle:=Heel;Job.Calc.Trim:=Trim;
            N:=SetPoint(Job.Calc.WaterlinePlane.a,Job.Calc.WaterlinePlane.b,Job.Calc.WaterlinePlane.c);Job.Bounds(N,Low,High,Leak);
            if -Job.Calc.WaterlinePlane.d>Leak+1e-7 then raise Exception.Create('水线以下有漏点。');
            Job.Calc.Calculate;Job.Check;
            if Job.Calc.Errors<>[] then raise Exception.Create(Job.Calc.ErrorString);
            D:=Job.Calc.Data;if D.Volume<=0 then raise Exception.Create('没有有效排水体积，请检查面的朝向。');
            Job.Calc.CalculateGravity;Row:=HydroData(Job.Calc,Job.Keel,Job.HasAsymmetricLayer);
          except on E:EAbort do raise;on E:Exception do Row:=TJSONObject.Create(['status','error','message',E.Message,'draft',Draft,'heel',Heel,'trim',Trim]) end;
          Rows.Add(Row);
        end;
      end;
    except on E:EAbort do begin Result.Booleans['cancelled']:=True;Result.Add('message',E.Message) end end;
    // Read-only oracle hook: ordinary bridge callers retain the old schema.
    if Request.Get('legacy_auxiliary_probe',False) and (Mode='single') and
       (Rows.Count=1) and (TJSONObject(Rows[0]).Get('status','')='ok') then begin
      Auxiliary:=TStringList.Create;
      try
        Notice:=Job.Calc.BuildAuxiliaryReports(Auxiliary);
        AuxJSON:=TJSONObject.Create;Result.Add('legacy_auxiliary',AuxJSON);
        AuxJSON.Add('message',Notice);AuxFiles:=TJSONObject.Create;AuxJSON.Add('files',AuxFiles);
        for I:=0 to Auxiliary.Count-1 do AuxFiles.Add(Auxiliary[I],TStringList(Auxiliary.Objects[I]).Text);
      finally
        for I:=0 to Auxiliary.Count-1 do Auxiliary.Objects[I].Free;
        Auxiliary.Free;
      end;
    end;
    Result.Add('elapsed_ms',Int64(GetTickCount64-Job.Started));
   except Result.Free;raise end;
  finally Job.Calc.Free;Job.Free;Drafts.Free end;
end;
end.
