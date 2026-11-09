unit UnitsConversionProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure RunUnitsConversion(Form:TMainForm);
implementation
uses Classes, SysUtils, Math, Windows, fpjson, FreeTypes, FreeGeometry,
  FreeShipUnit, FreeFileBuffer, FreeResistance_KaperDlg, FreeResistance_PlaningDlg;

type TPointArray=array of T3DCoordinate;
     TBounds=record Min,Max:T3DCoordinate; Count:Integer end;

procedure Near(Cases:TJSONArray; const ID:string; Actual,Expected:Double; Tol:Double=0.00005);
var Row:TJSONObject;
begin
  Row:=TJSONObject.Create; Row.Add('id',ID); Row.Add('actual',Actual); Row.Add('expected',Expected);
  Row.Add('tolerance',Tol*Math.Max(1,Abs(Expected)));
  if not IsNan(Actual) and not IsInfinite(Actual) and
    (Abs(Actual-Expected)<=Tol*Math.Max(1,Abs(Expected))) then Row.Add('status','pass')
  else Row.Add('status','fail');
  Cases.Add(Row);
end;

function Bounds(Section:TFreeIntersection):TBounds;
var I,J:Integer; P:T3DCoordinate;
begin
  Section.Rebuild; Result.Count:=0;
  Result.Min:=Point3D(MaxSingle,MaxSingle,MaxSingle);
  Result.Max:=Point3D(-MaxSingle,-MaxSingle,-MaxSingle);
  for I:=0 to Section.Count-1 do
    for J:=0 to Section.Items[I].NumberOfPoints-1 do
    begin
      P:=Section.Items[I].Point[J]; Inc(Result.Count);
      Result.Min.X:=Math.Min(Result.Min.X,P.X); Result.Min.Y:=Math.Min(Result.Min.Y,P.Y); Result.Min.Z:=Math.Min(Result.Min.Z,P.Z);
      Result.Max.X:=Math.Max(Result.Max.X,P.X); Result.Max.Y:=Math.Max(Result.Max.Y,P.Y); Result.Max.Z:=Math.Max(Result.Max.Z,P.Z);
    end;
  if Result.Count=0 then raise Exception.Create('No intersection geometry');
end;

procedure RunUnitsConversion(Form:TMainForm);
var
  R:TJSONObject; C:TJSONArray; OutText:TStringList;
  Ship,Loaded:TFreeShip; Settings:TFreeProjectSettings;
  Section:array[0..3] of TFreeIntersection;
  OriginalPlanes:array[0..3] of T3DPlane;
  OriginalBounds:array[0..3] of TBounds;
  CurrentBounds:TBounds;
  Points:TPointArray;
  Hydro:TFreeHydrostaticCalc;
  MetricVolume,MetricDisplacement,MetricLayerMass,MetricArea,Factor,MassFactor:Double;
  I,J,Turn:Integer; P:T3DCoordinate; Tag:string;
  MetricInputs:array[0..9] of Double;
  ResistanceInputs:array[0..6] of Double;
  Buffer:TFreeFileBuffer;
const
  Kinds:array[0..3] of TFreeIntersectionType=(fiStation,fiButtock,fiWaterline,fiDiagonal);
  Distances:array[0..3] of Double=(-6.75,1.25,2.25,2.75);
  Names:array[0..3] of string=('station','buttock','waterline','diagonal');
begin
  R:=TJSONObject.Create; C:=TJSONArray.Create; R.Add('cases',C);
  try
    try
      Form.FormShow(Form); Form.LoadNamedFile(SysUtils.GetEnvironmentVariable('FS_UNITS_FIXTURE'));
      Ship:=Form.FreeShip; Settings:=Ship.ProjectSettings;
      if not Ship.ModelIsLoaded or (Settings.ProjectUnits<>fuMetric) then raise Exception.Create('Metric fixture required');
      Settings.DisableModelCheck:=True; Settings.EnableModelAutoMove:=False; Settings.EnableBonjeanSAC:=False;
      Settings.ProjectWaterDensity:=1.025;
      Settings.Hydrostatics_StartDraft:=0.5; Settings.Hydrostatics_EndDraft:=2.5;
      Settings.Hydrostatics_DraftStep:=0.25; Settings.Hydrostatics_Trim:=-0.4;
      Settings.UseDefaultSplitSectionLocation:=False; Settings.ProjectSplitSectionLocation:=-5;
      Settings.UiSeedUnitInputs;
      for I:=0 to 9 do MetricInputs[I]:=Settings.UiUnitInput(I);
      Ship.UiSeedResistanceInputs;
      for I:=0 to 6 do ResistanceInputs[I]:=Ship.UiResistanceInput(I);
      for I:=0 to Ship.Surface.NumberOfLayers-1 do
      begin Ship.Surface.Layer[I].MaterialDensity:=7.85; Ship.Surface.Layer[I].Thickness:=8 end;
      SetLength(Points,Ship.Surface.NumberOfControlPoints);
      for I:=0 to High(Points) do
      begin
        P:=Ship.Surface.ControlPoint[I].Coordinate; P.X:=P.X-10;
        Ship.Surface.ControlPoint[I].Coordinate:=P; Points[I]:=P;
      end;
      Ship.RebuildModel;
      Ship.Surface.ControlPoint[0].Locked:=True;
      Near(C,'units.pound-force-existing-gravity',PoundsToNewton(1),0.45359237*9.81);
      for I:=0 to 3 do
      begin
        Section[I]:=Ship.Edit.Intersection_Add(Kinds[I],Distances[I]);
        if Section[I]=nil then raise Exception.Create('Unique section fixture unavailable: '+Names[I]);
        OriginalPlanes[I]:=Section[I].Plane; OriginalBounds[I]:=Bounds(Section[I]);
      end;
      Hydro:=TFreeHydrostaticCalc.Create(Ship);
      try
        Hydro.Draft:=2; Hydro.Trim:=0; Hydro.Calculate;
        MetricVolume:=Hydro.Data.Volume; MetricDisplacement:=Hydro.Data.Displacement;
        if (MetricVolume<=0) or (MetricDisplacement<=0) then raise Exception.Create('Hydrostatic fixture has no displacement');
        MetricLayerMass:=Ship.Surface.Layer[0].SurfaceProperties.Weight;
        MetricArea:=Ship.Surface.Layer[0].SurfaceProperties.SurfaceArea;
        if MetricLayerMass<=0 then raise Exception.Create('Layer fixture has no weight');
        for Turn:=1 to 4 do
        begin
          if Odd(Turn) then
          begin Settings.ProjectUnits:=fuImperial; Factor:=1/0.3048; MassFactor:=1000/(2240*0.45359237) end
          else begin Settings.ProjectUnits:=fuMetric; Factor:=1; MassFactor:=1 end;
          Settings.ProjectUnits:=Settings.ProjectUnits; // A repeated selection must be a no-op.
          Tag:='units.'+IntToStr(Turn)+'.';
          if Odd(Turn) then
          begin
            Near(C,Tag+'water-density',Settings.ProjectWaterDensity,63.98865959054823);
            Near(C,Tag+'material-density',Ship.Surface.Layer[0].MaterialDensity,7.85*1000*Power(0.3048,3)/0.45359237);
            Near(C,Tag+'material-thickness',Ship.Surface.Layer[0].Thickness,8/25.4);
          end
          else
          begin
            Near(C,Tag+'water-density',Settings.ProjectWaterDensity,1.025);
            Near(C,Tag+'material-density',Ship.Surface.Layer[0].MaterialDensity,7.85);
            Near(C,Tag+'material-thickness',Ship.Surface.Layer[0].Thickness,8);
          end;
          for I:=0 to High(Points) do
          begin
            P:=Ship.Surface.ControlPoint[I].Coordinate;
            Near(C,Tag+'point-'+IntToStr(I)+'-x',P.X,Points[I].X*Factor);
            Near(C,Tag+'point-'+IntToStr(I)+'-y',P.Y,Points[I].Y*Factor);
            Near(C,Tag+'point-'+IntToStr(I)+'-z',P.Z,Points[I].Z*Factor);
          end;
          Near(C,Tag+'negative-split-position',Settings.ProjectSplitSectionLocation,-5*Factor);
          Near(C,Tag+'point-lock-preserved',Ord(Ship.Surface.ControlPoint[0].Locked),1,0);
          Near(C,Tag+'draft-start',Settings.Hydrostatics_StartDraft,0.5*Factor);
          Near(C,Tag+'draft-end',Settings.Hydrostatics_EndDraft,2.5*Factor);
          Near(C,Tag+'draft-step',Settings.Hydrostatics_DraftStep,0.25*Factor);
          Near(C,Tag+'trim',Settings.Hydrostatics_Trim,-0.4*Factor);
          for I:=0 to 9 do
          begin
            if I<5 then Near(C,Tag+'mass-setting-'+IntToStr(I),Settings.UiUnitInput(I),MetricInputs[I]*MassFactor)
            else if I<8 then Near(C,Tag+'length-setting-'+IntToStr(I),Settings.UiUnitInput(I),MetricInputs[I]*Factor)
            else Near(C,Tag+'angle-setting-'+IntToStr(I),Settings.UiUnitInput(I),MetricInputs[I]);
          end;
          Near(C,Tag+'manual-kaper-mass',Ship.UiResistanceInput(0),ResistanceInputs[0]*MassFactor);
          Near(C,Tag+'manual-planing-mass',Ship.UiResistanceInput(1),ResistanceInputs[1]*MassFactor);
          Near(C,Tag+'manual-delft-volume',Ship.UiResistanceInput(2),ResistanceInputs[2]*Power(Factor,3));
          Near(C,Tag+'manual-kaper-extract',Ship.UiResistanceInput(3),0,0);
          Near(C,Tag+'manual-planing-extract',Ship.UiResistanceInput(4),0,0);
          Near(C,Tag+'manual-kaper-length',Ship.UiResistanceInput(5),ResistanceInputs[5]*Factor);
          Near(C,Tag+'manual-planing-length',Ship.UiResistanceInput(6),ResistanceInputs[6]*Factor);
          Ship.Edit.Resistance_Kaper;
          Near(C,Tag+'kaper-form-mass',FreeResistance_KaperDlg.UnitsProbeDisplacement,ResistanceInputs[0]*MassFactor);
          Near(C,Tag+'kaper-form-manual',Ord(FreeResistance_KaperDlg.UnitsProbeAutoExtract),0,0);
          Near(C,Tag+'kaper-form-executed',FreeResistance_KaperDlg.UnitsProbeCalls,Turn,0);
          Ship.Edit.Resistance_Planing;
          Near(C,Tag+'planing-form-mass',FreeResistance_PlaningDlg.UnitsProbeDisplacement,ResistanceInputs[1]*MassFactor);
          Near(C,Tag+'planing-form-manual',Ord(FreeResistance_PlaningDlg.UnitsProbeAutoExtract),0,0);
          Near(C,Tag+'planing-form-executed',FreeResistance_PlaningDlg.UnitsProbeCalls,Turn,0);
          for I:=0 to 3 do
          begin
            Near(C,Tag+Names[I]+'-distance',Section[I].Distance,Distances[I]*Factor);
            Near(C,Tag+Names[I]+'-normal-a',Section[I].Plane.a,OriginalPlanes[I].a);
            Near(C,Tag+Names[I]+'-normal-b',Section[I].Plane.b,OriginalPlanes[I].b);
            Near(C,Tag+Names[I]+'-normal-c',Section[I].Plane.c,OriginalPlanes[I].c);
            Near(C,Tag+Names[I]+'-offset',Section[I].Plane.d,OriginalPlanes[I].d*Factor);
            CurrentBounds:=Bounds(Section[I]);
            Near(C,Tag+Names[I]+'-curve-min-x',CurrentBounds.Min.X,OriginalBounds[I].Min.X*Factor);
            Near(C,Tag+Names[I]+'-curve-min-y',CurrentBounds.Min.Y,OriginalBounds[I].Min.Y*Factor);
            Near(C,Tag+Names[I]+'-curve-min-z',CurrentBounds.Min.Z,OriginalBounds[I].Min.Z*Factor);
            Near(C,Tag+Names[I]+'-curve-max-x',CurrentBounds.Max.X,OriginalBounds[I].Max.X*Factor);
            Near(C,Tag+Names[I]+'-curve-max-y',CurrentBounds.Max.Y,OriginalBounds[I].Max.Y*Factor);
            Near(C,Tag+Names[I]+'-curve-max-z',CurrentBounds.Max.Z,OriginalBounds[I].Max.Z*Factor);
          end;
          Near(C,Tag+'hydro-draft',Hydro.Draft,2*Factor);
          Hydro.Calculate;
          Near(C,Tag+'physical-volume',Hydro.Data.Volume/Power(Factor,3),MetricVolume,0.0002);
          Near(C,Tag+'physical-displacement',Hydro.Data.Displacement/MassFactor,MetricDisplacement,0.0002);
          Near(C,Tag+'physical-layer-area',Ship.Surface.Layer[0].SurfaceProperties.SurfaceArea/Power(Factor,2),MetricArea,0.0002);
          if Odd(Turn) then
            Near(C,Tag+'physical-layer-mass',Ship.Surface.Layer[0].SurfaceProperties.Weight*0.45359237/12,MetricLayerMass,0.0002)
          else Near(C,Tag+'physical-layer-mass',Ship.Surface.Layer[0].SurfaceProperties.Weight,MetricLayerMass,0.0002);
        end;
      finally Hydro.Free end;

      // Ordinary nonuniform resizing remains a geometry operation: mass at
      // constant density and volume both scale by the geometric volume factor.
      Loaded:=TFreeShip.Create(nil);
      try
        Loaded.IsRebuildAsyncDisabled:=True; Loaded.UiSeedResistanceInputs;
        Loaded.Edit.Model_Scale(Point3D(2,3,4),True,True);
        Near(C,'units.geometric-scale-kaper-mass',Loaded.UiResistanceInput(0),23.75*24);
        Near(C,'units.geometric-scale-planing-mass',Loaded.UiResistanceInput(1),125.5*24);
        Near(C,'units.geometric-scale-delft-volume',Loaded.UiResistanceInput(2),75*24);
      finally Loaded.Free end;

      // Existing imperial input is retained on save/load; it is not silently migrated.
      Settings.ProjectUnits:=fuImperial; Settings.ProjectWaterDensity:=65.01548;
      Buffer:=TFreeFileBuffer.Create; Loaded:=TFreeShip.Create(nil);
      try
        Ship.SaveProject(Buffer); Loaded.LoadProject(Buffer);
        Near(C,'units.legacy-imperial-density-preserved',Loaded.ProjectSettings.ProjectWaterDensity,65.01548);
        Near(C,'units.legacy-imperial-unit-preserved',Ord(Loaded.ProjectSettings.ProjectUnits),Ord(fuImperial),0);
      finally Loaded.Free; Buffer.Free end;
    except on E:Exception do R.Add('error',E.ClassName+': '+E.Message) end;
    OutText:=TStringList.Create;
    try OutText.Text:=R.FormatJSON; OutText.SaveToFile(IncludeTrailingPathDelimiter(SysUtils.GetEnvironmentVariable('FS_UNITS_OUTPUT'))+'probe.json')
    finally OutText.Free end;
  finally R.Free end;
  ExitProcess(0);
end;
end.
