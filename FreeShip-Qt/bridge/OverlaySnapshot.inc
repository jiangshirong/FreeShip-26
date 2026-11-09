{ Read-only presentation data from the original design-hydrostatics calculation.
  GPL-2.0-or-later. No legacy viewport or editing command is invoked. }
procedure TDocument.WriteOverlay(Root:TJSONObject);
var O,H,M:TJSONObject;Markers,Areas,Path:TJSONArray;Calc:TFreeHydrostaticCalc;
    Data:TFreeHydrostaticsData;Curve:TFreeSpline;P:T3DCoordinate;I:Integer;
  procedure Marker(const Key,Caption:string;Position:T3DCoordinate;Compensate:Boolean);
  begin
    if Compensate then Position.Z:=Position.Z+Data.ModelMin.Z;
    M:=TJSONObject.Create;Markers.Add(M);M.Add('key',Key);M.Add('text',Caption);M.Add('p',Coordinates(Position));
  end;
begin
  O:=TJSONObject.Create;Root.Add('display',O);
  O.Add('both_sides',Ship.Visibility.ModelView=mvBoth);
  O.Add('show_curvature',Ship.Visibility.ShowCurvature);
  O.Add('show_control_curves',Ship.Visibility.ShowControlCurves);
  O.Add('show_flowlines',Ship.Visibility.ShowFlowlines);
  O.Add('show_normals',Ship.Visibility.ShowNormals);O.Add('show_markers',Ship.Visibility.ShowMarkers);
  O.Add('curvature_scale',Double(Ship.Visibility.CurvatureScale));
  O.Add('curvature_color',Int64(ColorToRGB(Ship.Preferences.CurvaturePlotColor)));
  O:=TJSONObject.Create;Root.Add('overlay',O);
  O.Add('show_grid',Ship.Visibility.ShowGrid);O.Add('show_hydrostatics',Ship.Visibility.ShowHydrostaticData);
  O.Add('main_particulars',Ship.ProjectSettings.MainParticularsHasBeenSet);
  O.Add('grid_color',Int64(ColorToRGB(Ship.Preferences.GridColor)));
  O.Add('grid_font_color',Int64(ColorToRGB(Ship.Preferences.GridFontColor)));
  O.Add('hydro_color',Int64(ColorToRGB(Ship.Preferences.HydrostaticsFontColor)));
  O.Add('point_size',Ship.Preferences.PointSize);O.Add('font_size',Ship.Preferences.FontSize);
  H:=TJSONObject.Create;O.Add('hydro',H);Markers:=TJSONArray.Create;H.Add('markers',Markers);
  Areas:=TJSONArray.Create;H.Add('areas',Areas);Path:=TJSONArray.Create;H.Add('path',Path);
  // The legacy design calculation is also HydrostaticCalculation[0], which
  // the accepted lines-plan scene reads. Changing its draft for annotations
  // would alter subsequent scene exports. Cache a separate calculation here.
  if OverlayCalc=nil then OverlayCalc:=TFreeHydrostaticCalc.Create(Ship);
  Calc:=OverlayCalc;
  if Calc.Draft<>Ship.ProjectSettings.ProjectDraft then Calc.Draft:=Ship.ProjectSettings.ProjectDraft;
  try
    if not Calc.Calculated then begin Calc.Calculate;Calc.CalculateGravity end;
    H.Add('available',Calc.Errors=[]);
    if Calc.Errors<>[] then Exit;
    Data:=Calc.Data;
    // The positions and captions deliberately follow DrawToViewport. The
    // legacy KM caption/height convention is retained, not reinterpreted here.
    if Ship.Visibility.ServiceDisplacement then
      Marker('displacement','Displ='+FloatToStrF(Data.Displacement,ffFixed,7,2),Data.CenterOfBuoyancy,True);
    if Abs(Data.WaterplaneCOG.Y)<0.01 then begin
      P:=SetPoint(Data.CenterOfBuoyancy.X,0,Data.KMtransverse);
      if Ship.Visibility.ServiceMetacentricHeight then
        Marker('km','KM='+FloatToStrF(Data.KMtransverse+Data.ModelMin.Z,ffFixed,7,2),P,True);
      if Ship.Visibility.ServiceLCF then
        Marker('lcf','LCF='+FloatToStrF(Data.WaterplaneCOG.X,ffFixed,7,2),Data.WaterplaneCOG,False);
    end;
    if Ship.Visibility.ServiceDisplacement and (Data.Weight_>0) then
      Marker('weight','Weight='+FloatToStrF(Data.Weight_,ffFixed,7,2),Data.CenterOfGravity_,True);
    if Ship.Visibility.ServiceLateralArea then begin
      Marker('lateral',rs_Lateral_area+'='+FloatToStrF(Data.LateralArea,ffFixed,7,2),Data.LateralCOG,True);
      Marker('wind',rs_Lateral_wind_area+'='+FloatToStrF(Data.SDP,ffFixed,7,2),Data.SDPCOG,True);
    end;
    if not Ship.Visibility.ServiceSectionalAreas then Exit;
    Curve:=TFreeSpline.Create(Ship.Surface);
    try
      for I:=0 to High(Data.SAC) do begin
        P:=SetPoint(Data.SAC[I].X,0,Data.SAC[I].Y);
        if Data.BeamWaterline*Calc.Draft<>0 then
          P.Z:=2*(Data.ModelMax.Z-Data.ModelMin.Z)*Data.SAC[I].Y/(Data.BeamWaterline*Calc.Draft);
        P.Z:=P.Z+Data.ModelMin.Z;Curve.Add(P);
        M:=TJSONObject.Create;Areas.Add(M);M.Add('p',Coordinates(P));M.Add('value',Double(Data.SAC[I].Y));
      end;
      if Curve.NumberOfPoints>1 then for I:=0 to 600 do Path.Add(Coordinates(Curve.Value(I/600)));
    finally Curve.Free end;
  except
    // A presentation failure must not discard an otherwise editable hull.
    H.Delete('available');H.Add('available',False);Markers.Clear;Areas.Clear;Path.Clear;
  end;
end;
