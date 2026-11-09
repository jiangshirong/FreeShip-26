unit PlateErrorsProbe;
{$mode objfpc}{$H+}
interface
uses Main, fpjson;
procedure RunPlateErrorsProbe(Form:TMainForm);
implementation
uses Classes, SysUtils, Math, Graphics, FreeShipUnit, FreeGeometry, FreeTypes,
  FreeExpanedPlatesDlg;
type
  TSample = record
    MaxEdge, SignedArea, AbsoluteArea, ReferenceArea, Score: Double;
    Distances: array of Double;
  end;
procedure Check(C:TJSONArray; const ID,Expected:string; Passed:Boolean; const Actual:string='');
var O:TJSONObject;
begin
  O:=TJSONObject.Create; O.Add('id',ID); O.Add('expected',Expected);
  if Passed then O.Add('status','pass') else O.Add('status','fail');
  O.Add('actual',Actual); C.Add(O);
end;
function MakePatch(Ship:TFreeShip; Shape:Integer; Scale:Double):TFreeDevelopedPatch;
var Points:array[0..3,0..3] of TFreeSubdivisionControlPoint;
  I,J:Integer; P:T3DCoordinate; FacePoints:TFasterListTFreeSubdivisionControlPoint;
begin
  Ship.IsRebuildAsyncDisabled:=True;
  for I:=0 to 3 do for J:=0 to 3 do begin
    P.X:=I; P.Y:=J; P.Z:=0;
    if Shape=1 then begin P.X:=I*0.8; P.Y:=J*0.6; P.Z:=0.3*I+0.4*J end;
    if Shape=2 then begin P.X:=2*Cos(I*Pi/6); P.Y:=2*Sin(I*Pi/6); P.Z:=J end;
    if Shape=3 then P.Z:=0.25*(I-1.5)*(J-1.5);
    P.X:=P.X*Scale; P.Y:=P.Y*Scale; P.Z:=P.Z*Scale;
    Points[I,J]:=Ship.Surface.AddControlPoint(P);
  end;
  FacePoints:=TFasterListTFreeSubdivisionControlPoint.Create;
  try
    for I:=0 to 2 do for J:=0 to 2 do begin
      FacePoints.Clear; FacePoints.Add(Points[I,J]); FacePoints.Add(Points[I+1,J]);
      FacePoints.Add(Points[I+1,J+1]); FacePoints.Add(Points[I,J+1]);
      Ship.Surface.AddControlFace(FacePoints,True,Ship.Layer[0]);
    end;
  finally FacePoints.Free end;
  for I:=0 to Ship.Surface.NumberOfControlEdges-1 do Ship.Surface.ControlEdge[I].Crease:=True;
  Ship.Surface.DesiredSubdivisionLevel:=1;
  Ship.Surface.Rebuild;
  Result:=TFreeDevelopedPatch.Create(Ship.Layer[0]);
  Result.Unroll(Ship.Surface.ControlFaces);
end;
procedure Sample(Patch:TFreeDevelopedPatch; Scale:Double; out S:TSample);
var I,J,K,L:Integer; V:Double;
begin
  S.MaxEdge:=Max(Abs(Patch.MinError),Abs(Patch.MaxError))/Scale;
  S.SignedArea:=Patch.TotalAreaError/Sqr(Scale);
  S.AbsoluteArea:=Patch.AbsoluteAreaError/Sqr(Scale);
  S.ReferenceArea:=Patch.ReferenceArea/Sqr(Scale);
  S.Score:=Patch.NormalizedError;
  L:=Patch.ProbePointCount;
  SetLength(S.Distances,L*(L-1) div 2); K:=0;
  for I:=0 to L-1 do for J:=I+1 to L-1 do begin
    S.Distances[K]:=DistPP3D(Patch.Point[I],Patch.Point[J])/Scale; Inc(K)
  end;
  for I:=1 to High(S.Distances) do begin
    V:=S.Distances[I]; J:=I-1;
    while (J>=0) and (S.Distances[J]>V) do begin S.Distances[J+1]:=S.Distances[J]; Dec(J) end;
    S.Distances[J+1]:=V;
  end;
end;
procedure RunPlateErrorsProbe(Form:TMainForm);
const Names:array[0..3] of string=('plane','tilted-plane','faceted-cylinder','saddle');
  Factors:array[0..2] of Double=(1,3.280839895013123,2);
var R:TJSONObject; C:TJSONArray; S:TStringList; Ship:TFreeShip; Patch,Mirror:TFreeDevelopedPatch;
  Shape,ScaleIndex,I:Integer; Base,Value:TSample; Prefix:string; DistError:Double;
  Dialog:TFreeExpanedplatesDialog; Patches:TFasterListTFreeDevelopedPatch;
begin
  R:=TJSONObject.Create; C:=TJSONArray.Create; R.Add('cases',C);
  try
    try
      for Shape:=0 to 3 do for ScaleIndex:=0 to 2 do begin
        Ship:=TFreeShip.Create(nil); Patch:=nil; Mirror:=nil;
        try
          Patch:=MakePatch(Ship,Shape,Factors[ScaleIndex]);
          Sample(Patch,Factors[ScaleIndex],Value);
          Prefix:=Format('16.%s.scale-%d',[Names[Shape],ScaleIndex]);
          Check(C,Prefix+'.finite','a finite dimensionless score and positive reference area',
            not IsNan(Value.Score) and not IsInfinite(Value.Score) and (Value.ReferenceArea>0),
            Format('edge=%.9g signed_area=%.9g absolute_area=%.9g area=%.9g score=%.9g',
              [Value.MaxEdge,Value.SignedArea,Value.AbsoluteArea,Value.ReferenceArea,Value.Score]));
          Check(C,Prefix+'.noncancelling','absolute face differences cannot cancel below signed total',
            Value.AbsoluteArea+1e-7>=Abs(Value.SignedArea));
          if Shape<3 then
            Check(C,Prefix+'.developable','planar or faceted cylindrical surface unfolds with negligible edge/area error',
              (Value.MaxEdge<2e-5) and (Value.AbsoluteArea<2e-4),
              Format('edge=%.9g; area=%.9g',[Value.MaxEdge,Value.AbsoluteArea]));
          if Shape=0 then
            Check(C,Prefix+'.known-area','the 3 by 3 square has area 9 square model units',Abs(Value.ReferenceArea-9)<2e-5,FloatToStr(Value.ReferenceArea));
          if ScaleIndex=0 then Base:=Value
          else begin
            DistError:=0;
            if Length(Value.Distances)<>Length(Base.Distances) then DistError:=1e10
            else for I:=0 to High(Value.Distances) do DistError:=Max(DistError,Abs(Value.Distances[I]-Base.Distances[I]));
            Check(C,Prefix+'.scale-metrics','physical area and dimensionless error agree after length-unit conversion',
              (Abs(Value.ReferenceArea-Base.ReferenceArea)<2e-4) and
              (Abs(Value.AbsoluteArea-Base.AbsoluteArea)<2e-4) and
              (Abs(Value.Score-Base.Score)<2e-5),
              Format('delta_area=%.9g; delta_abs_error=%.9g; delta_score=%.9g',
                [Value.ReferenceArea-Base.ReferenceArea,Value.AbsoluteArea-Base.AbsoluteArea,Value.Score-Base.Score]));
            Check(C,Prefix+'.scale-shape','normalized pairwise distances preserve the unrolled shape up to rotation/reflection',
              DistError<2e-4,FloatToStr(DistError));
          end;
          Mirror:=TFreeDevelopedPatch.Create(Ship.Layer[0]); Mirror.Assign(Patch,True);
          Check(C,Prefix+'.copy-metrics','mirroring a developed patch preserves its error measurements',
            (Mirror.AbsoluteAreaError=Patch.AbsoluteAreaError) and (Mirror.ReferenceArea=Patch.ReferenceArea) and
            (Mirror.NormalizedError=Patch.NormalizedError));
          if Shape=0 then begin
            Patches:=TFasterListTFreeDevelopedPatch.Create; Dialog:=TFreeExpanedplatesDialog.Create(nil);
            try
              Patches.Add(Patch);
              if ScaleIndex=1 then Patch.Units:=fuImperial else Patch.Units:=fuMetric;
              Dialog.ProbeBind(Ship,Patches);
              Check(C,Prefix+'.display-units','edge values include length units and area values include area units',
                (Pos(LengthStr(Patch.Units),Dialog._Label2.Caption)>0) and
                (Pos(LengthStr(Patch.Units),Dialog._Label4.Caption)>0) and
                (Pos(AreaStr(Patch.Units),Dialog._Label16.Caption)>0) and
                (Pos(AreaStr(Patch.Units),Dialog._Label18.Caption)>0),
                Dialog._Label2.Caption+'; '+Dialog._Label16.Caption);
              Check(C,Prefix+'.explanation','signed difference and noncancelling aggregate explanations are available',
                Dialog._Label2.ShowHint and Dialog._Label18.ShowHint and (Length(Dialog._Label2.Hint)>0) and
                (Pos(FloatToStrF(Patch.AbsoluteAreaError,ffFixed,7,6),Dialog._Label18.Hint)>0));
            finally Dialog.Free; Patches.Free end;
          end;
        finally Mirror.Free; Patch.Free; Ship.Free end;
      end;
    except on E:Exception do R.Add('error',E.ClassName+': '+E.Message) end;
    S:=TStringList.Create;
    try S.Text:=R.FormatJSON; S.SaveToFile(IncludeTrailingPathDelimiter(SysUtils.GetEnvironmentVariable('FS_PLATE_OUTPUT'))+'probe.json')
    finally S.Free end;
  finally R.Free end;
end;
end.
