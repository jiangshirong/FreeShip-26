function TDocument.HydroDefaults:TJSONObject;
var P:TFreeProjectSettings;A:TJSONArray;I:Integer;
begin
  P:=Ship.ProjectSettings;Result:=TJSONObject.Create;
  Result.Add('start',Double(P.Hydrostatics_Startdraft));Result.Add('end',Double(P.Hydrostatics_EndDraft));
  Result.Add('step',Double(P.Hydrostatics_DraftStep));Result.Add('trim',Double(P.Hydrostatics_Trim));
  Result.Add('draft',Double(P.ProjectDraft));Result.Add('keel_z',Double(Ship.FindLowestHydrostaticsPoint));
  Result.Add('vcg',Double(P.ServiceVCG));Result.Add('free_trim',P.ServiceFreeTrim);
  if P.ServiceNoStabTrims>0 then Result.Add('cross_trim',Double(P.ServiceStabTrims[0])) else Result.Add('cross_trim',0);
  Result.Add('length',Double(P.ProjectLength));Result.Add('name',P.ProjectName);
  Result.Add('coefficient_basis',Ord(P.ProjectCoefficients));
  A:=TJSONArray.Create;Result.Add('angles',A);for I:=0 to P.ServiceNoAngles-1 do A.Add(Double(P.ServiceAngles[I]));
  A:=TJSONArray.Create;Result.Add('displacements',A);
  if P.ServiceUseIncrements and (P.ServiceDisplIncrement>0) and
    ((P.ServiceMaxDispl-P.ServiceMinDispl)/P.ServiceDisplIncrement<=99) then begin
    I:=0;while (I<100) and (P.ServiceMinDispl+I*P.ServiceDisplIncrement<=P.ServiceMaxDispl+1e-6) do begin
      if P.ServiceMinDispl+I*P.ServiceDisplIncrement>0 then A.Add(Double(P.ServiceMinDispl+I*P.ServiceDisplIncrement));Inc(I);
    end;
  end else for I:=0 to P.ServiceNoDisplacements-1 do A.Add(Double(P.ServiceDisplacements[I]));
  // A failed transaction can restore a model without its presentation cache.
  // Defaults must not change merely because that cache has not been painted.
  try
    if not Ship.Built then Ship.RebuildModel;
    if OverlayCalc=nil then OverlayCalc:=TFreeHydrostaticCalc.Create(Ship);
    OverlayCalc.Draft:=P.ProjectDraft;
    if not OverlayCalc.Calculated then begin OverlayCalc.Calculate;OverlayCalc.CalculateGravity end;
    if (OverlayCalc.Errors=[]) and (OverlayCalc.Data.Displacement>0) then
      Result.Add('design_displacement',Double(OverlayCalc.Data.Displacement));
  except
    // Missing design buoyancy does not prevent editing a calculation range.
  end;
end;
function TDocument.HydroSettings(Request:TJSONObject):Boolean;
var P:TFreeProjectSettings;A:TJSONArray;Angles,Masses,Trims:TFloatArray;Start,Stop,Step,Trim,Value,VCG:Double;FreeTrim:Boolean;
    I,JHydro,Limit:Integer;Mode:string;
begin
  P:=Ship.ProjectSettings;Mode:=StringField(Request,'mode');
  if Mode='table' then begin
    Start:=Field(Request,'start',jtNumber).AsFloat;Stop:=Field(Request,'end',jtNumber).AsFloat;
    Step:=Field(Request,'step',jtNumber).AsFloat;Trim:=Field(Request,'trim',jtNumber).AsFloat;
    CheckCoordinate(Start);CheckCoordinate(Stop);CheckCoordinate(Step);CheckCoordinate(Trim);
    if (Start<0) or (Stop<Start) or (Stop>1e6) or (Step<1e-6) or (Step>1e6) or
      ((Stop-Start)/Step>199+1e-8) or (Abs(Trim)>P.ProjectLength) then raise Exception.Create('无效的吃水表范围。');
    Result:=(P.Hydrostatics_Startdraft<>TFloatType(Start)) or (P.Hydrostatics_EndDraft<>TFloatType(Stop)) or
      (P.Hydrostatics_DraftStep<>TFloatType(Step)) or (P.Hydrostatics_Trim<>TFloatType(Trim));
  end else if Mode='cross' then begin
    VCG:=Field(Request,'vcg',jtNumber).AsFloat;CheckCoordinate(VCG);Trim:=Field(Request,'trim',jtNumber).AsFloat;CheckCoordinate(Trim);
    if Abs(Trim)>P.ProjectLength then raise Exception.Create('纵倾超出范围。');
    FreeTrim:=Field(Request,'free_trim',jtBoolean).AsBoolean;
    for I:=0 to 1 do begin
      if I=0 then A:=TJSONArray(Field(Request,'angles',jtArray)) else A:=TJSONArray(Field(Request,'displacements',jtArray));
      if I=0 then Limit:=181 else Limit:=100;
      if (A.Count<1) or (A.Count>Limit) then raise Exception.Create('保存列表长度超出范围。');
      if I=0 then SetLength(Angles,A.Count) else SetLength(Masses,A.Count);
      for JHydro:=0 to A.Count-1 do begin
        if A[JHydro].JSONType<>jtNumber then raise Exception.Create('列表值必须为数值。');
        Value:=A.Floats[JHydro];CheckCoordinate(Value);
        if ((I=0) and (Abs(Value)>180)) or ((I=1) and ((Value<1e-6) or (Value>1e9))) then raise Exception.Create('列表值超出范围。');
        if (JHydro>0) and (Value<=A.Floats[JHydro-1]) then raise Exception.Create('列表必须严格递增。');
        if I=0 then begin
          Angles[JHydro]:=Value;
          if (JHydro>0) and (Angles[JHydro]<=Angles[JHydro-1]) then raise Exception.Create('角度间隔小于 FBM 能保存的精度。');
        end else begin
          Masses[JHydro]:=Value;
          if (JHydro>0) and (Masses[JHydro]<=Masses[JHydro-1]) then raise Exception.Create('排水量间隔小于 FBM 能保存的精度。');
        end;
      end;
    end;
    if Length(Angles)*Length(Masses)>500 then raise Exception.Create('稳性表最多包含 500 个工况。');
    Result:=P.ServiceUseIncrements or (P.ServiceVCG<>TFloatType(VCG)) or (P.ServiceFreeTrim<>FreeTrim) or
      (P.ServiceNoStabTrims<>1) or (P.ServiceNoAngles<>Length(Angles)) or (P.ServiceNoDisplacements<>Length(Masses));
    if not Result then begin
      Result:=P.ServiceStabTrims[0]<>TFloatType(Trim);
      for I:=0 to High(Angles) do if P.ServiceAngles[I]<>Angles[I] then Result:=True;
      for I:=0 to High(Masses) do if P.ServiceDisplacements[I]<>Masses[I] then Result:=True;
    end;
  end else raise Exception.Create('只有吃水表和稳性曲线的范围可保存到 FBM。');
  if not Result then Exit;
  RequireVersion(Ship,fv250);
  if Mode='table' then begin
    P.Hydrostatics_Startdraft:=Start;P.Hydrostatics_EndDraft:=Stop;P.Hydrostatics_DraftStep:=Step;P.Hydrostatics_Trim:=Trim;
  end else begin
    P.ServiceAngles:=Angles;P.ServiceNoAngles:=Length(Angles);
    P.ServiceDisplacements:=Masses;P.ServiceNoDisplacements:=Length(Masses);P.ServiceUseIncrements:=False;
    P.ServiceVCG:=VCG;P.ServiceFreeTrim:=FreeTrim;SetLength(Trims,1);Trims[0]:=Trim;P.ServiceStabTrims:=Trims;P.ServiceNoStabTrims:=1;
  end;
end;
