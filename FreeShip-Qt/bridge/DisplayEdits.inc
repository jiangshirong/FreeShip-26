{ Presentation options stored in the original FBM visibility block.
  FreeShip_26, GPL-2.0-or-later. }
function TDocument.EditDisplay(Request:TJSONObject):Boolean;
var Changes:TJSONObject;Flag:Boolean;Value:Double;Scale:TFloatType;I:Integer;
begin
  Changes:=TJSONObject(Field(Request,'changes',jtObject));Result:=False;
  for I:=0 to Changes.Count-1 do if (Changes.Names[I]<>'both_sides') and (Changes.Names[I]<>'show_curvature') and (Changes.Names[I]<>'curvature_scale') and (Changes.Names[I]<>'show_control_curves') and (Changes.Names[I]<>'show_flowlines') and (Changes.Names[I]<>'show_normals') and (Changes.Names[I]<>'show_markers') then
    raise Exception.Create('Unsupported display property');
  if Changes.Find('show_normals')<>nil then begin
    Flag:=Field(Changes,'show_normals',jtBoolean).AsBoolean;
    if Flag<>Ship.Visibility.ShowNormals then begin Ship.Visibility.ShowNormals:=Flag;Result:=True end;
  end;
  if Changes.Find('show_markers')<>nil then begin
    Flag:=Field(Changes,'show_markers',jtBoolean).AsBoolean;
    if Flag<>Ship.Visibility.ShowMarkers then begin Ship.Visibility.ShowMarkers:=Flag;Result:=True end;
  end;
  if Changes.Find('show_control_curves')<>nil then begin
    Flag:=Field(Changes,'show_control_curves',jtBoolean).AsBoolean;
    if Flag<>Ship.Visibility.ShowControlCurves then begin RequireVersion(Ship,fv195);Ship.Visibility.ShowControlCurves:=Flag;Result:=True end;
  end;
  if Changes.Find('show_flowlines')<>nil then begin
    Flag:=Field(Changes,'show_flowlines',jtBoolean).AsBoolean;
    if Flag<>Ship.Visibility.ShowFlowlines then begin RequireVersion(Ship,fv250);Ship.Visibility.ShowFlowlines:=Flag;Result:=True end;
  end;
  if Changes.Find('both_sides')<>nil then begin
    Flag:=Field(Changes,'both_sides',jtBoolean).AsBoolean;
    if Flag<>(Ship.Visibility.ModelView=mvBoth) then begin
      if Flag then Ship.Visibility.ModelView:=mvBoth else Ship.Visibility.ModelView:=mvPort;
      Result:=True;
    end;
  end;
  if Changes.Find('show_curvature')<>nil then begin
    Flag:=Field(Changes,'show_curvature',jtBoolean).AsBoolean;
    if Flag<>Ship.Visibility.ShowCurvature then begin Ship.Visibility.ShowCurvature:=Flag;Result:=True end;
  end;
  if Changes.Find('curvature_scale')<>nil then begin
    Value:=Field(Changes,'curvature_scale',jtNumber).AsFloat;CheckCoordinate(Value);
    if Value<0 then raise Exception.Create('Curvature scale must not be negative');
    Scale:=Value;
    if Abs(Scale-Ship.Visibility.CurvatureScale)>1e-7 then begin Ship.Visibility.CurvatureScale:=Scale;Result:=True end;
  end;
end;
