program QtPlateFixture;
{$mode objfpc}{$H+}
{ Independent synthetic FBM writer and original unfolding observer. No Qt adapter. }
uses Interfaces,Forms,Graphics,Classes,SysUtils,Math,fpjson,
  FreeShipUnit,FreeGeometry,FreeTypes,FreeFileBuffer;
const Names:array[0..10] of string=('plane','tilted','cylinder','saddle','feet','mirror-bottom','mirror-side','hole','disconnected','none','bent-centre');
var Ship:TFreeShip;Points:array[0..3,0..3] of TFreeSubdivisionControlPoint;
    List:TFasterListTFreeSubdivisionControlPoint;Plates:TFasterListTFreeDevelopedPatch;
    I,J,K,Shape,N,A,B:Integer;P:T3DCoordinate;Scale:Double;Buffer:TFreeFileBuffer;
    Root,Row:TJSONObject;Refs,Distances:TJSONArray;Patch:TFreeDevelopedPatch;S:TStringList;
begin
  try
    Application.Initialize;DefaultFormatSettings.DecimalSeparator:='.';
    ForceDirectories(ParamStr(1));Root:=TJSONObject.Create;
    for Shape:=0 to High(Names) do begin
      Ship:=TFreeShip.Create(nil);GlobalFreeShip:=Ship;Ship.IsRebuildAsyncDisabled:=True;
      Ship.ProjectSettings.SavePreview:=False;Ship.Precision:=fpLow;
      Ship.Layer[0].Name:=Names[Shape];Ship.Layer[0].Developable:=Shape<>9;Ship.Layer[0].Symmetric:=(Shape=5) or (Shape=6) or (Shape=10);
      Scale:=1;if Shape=4 then begin Scale:=1/0.3048;Ship.ProjectSettings.ProjectUnits:=fuImperial end;
      Ship.ProjectSettings.ProjectLength:=3*Scale;Ship.ProjectSettings.ProjectBeam:=3*Scale;Ship.ProjectSettings.ProjectDraft:=Scale;
      for I:=0 to 3 do for J:=0 to 3 do begin
        P:=SetPoint(I,J,0);
        if (Shape=1) or (Shape=6) then P:=SetPoint(0.8*I,2+0.6*J,0.3*I+0.4*J);
        if Shape=2 then P:=SetPoint(2*Cos(I*Pi/6),2*Sin(I*Pi/6),J);
        if Shape=3 then P.Z:=0.25*(I-1.5)*(J-1.5);
        if Shape=10 then P.Z:=0.5*I*I+0.009*J;
        P.X:=P.X*Scale;P.Y:=P.Y*Scale;P.Z:=P.Z*Scale;
        Points[I,J]:=Ship.Surface.AddControlPoint(P);
      end;
      List:=TFasterListTFreeSubdivisionControlPoint.Create;
      for I:=0 to 2 do for J:=0 to 2 do begin
        if ((Shape=7) and (I=1) and (J=1)) or ((Shape=8) and (I=1)) then Continue;
        List.Clear;List.Add(Points[I,J]);List.Add(Points[I+1,J]);List.Add(Points[I+1,J+1]);List.Add(Points[I,J+1]);
        Ship.Surface.AddControlFace(List,True,Ship.Layer[0]);
      end;
      List.Free;
      for I:=0 to Ship.Surface.NumberOfControlEdges-1 do Ship.Surface.ControlEdge[I].Crease:=True;
      Ship.RebuildModel;
      Buffer:=TFreeFileBuffer.Create;
      try Ship.SaveProject(Buffer);Buffer.SaveToFile(IncludeTrailingPathDelimiter(ParamStr(1))+Names[Shape]+'.fbm') finally Buffer.Free end;
      Plates:=TFasterListTFreeDevelopedPatch.Create;Refs:=TJSONArray.Create;Root.Add(Names[Shape],Refs);
      if Shape<>9 then Ship.Layer[0].Unroll(Plates);
      for K:=0 to Plates.Count-1 do begin
        Patch:=Plates[K];Row:=TJSONObject.Create;Refs.Add(Row);
        Row.Add('name',Patch.Name);Row.Add('reference_area',Double(Patch.ReferenceArea));
        Row.Add('absolute_area_error',Double(Patch.AbsoluteAreaError));Row.Add('normalized_error',Double(Patch.NormalizedError));
        Row.Add('min_edge_error',Double(Patch.MinError));Row.Add('max_edge_error',Double(Patch.MaxError));
        N:=Patch.ProbePointCount;Row.Add('point_count',N);Distances:=TJSONArray.Create;Row.Add('distances',Distances);
        for A:=0 to N-1 do for B:=A+1 to N-1 do Distances.Add(Double(DistPP3D(Patch.Point[A],Patch.Point[B])));
      end;
      for K:=0 to Plates.Count-1 do Plates[K].Free;Plates.Free;Ship.Free;
    end;
    S:=TStringList.Create;S.Text:=Root.AsJSON;S.SaveToFile(IncludeTrailingPathDelimiter(ParamStr(1))+'oracle.json');S.Free;Root.Free;
  except on E:Exception do begin WriteLn(E.ClassName+': '+E.Message);Halt(1) end end;
end.
