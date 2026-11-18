"""Read-only unfolding access and lifecycle fixes, applied only to build copies."""
import shutil

def adapt(work, bridge, replace):
    for name in ('PlateEngine.pas','PlateHooks.pas','PlateSnapshot.inc'):
        shutil.copy2(bridge/name,work/'Units'/name)
    path=work/'Units/FreeGeometry.pas';s=path.read_text('utf-8-sig')
    s=replace(s,'  FreeTypes,','  fpjson, PlateHooks, FreeTypes,')
    s=replace(s,'    property NormalizedError: extended read FNormalizedError;',
        '    property NormalizedError: extended read FNormalizedError;\n    function ServicePlateData:TJSONObject;')
    path.write_text(s,encoding='utf8')
    path=work/'Units/FreeDevelopedPatch.inc';s=path.read_text('utf-8-sig')
    s='{$I PlateSnapshot.inc}\n'+s
    # Exception-safe cancellation during the expensive seed search.
    s=replace(s,'    ToDoList.AddList(FFaces);','    try\n    ToDoList.AddList(FFaces);')
    s=replace(s,'        Face := FDoneList[I - 1];','        PlateCheckpoint;\n        Face := FDoneList[I - 1];')
    s=replace(s,'    FreeAndNil(ToDoList);','    finally FreeAndNil(ToDoList) end;')
    s=replace(s,'  FFaces := TFasterListTFreeSubdivisionFace.Create;',
        '  SeedFaces:=nil;TmpEdges:=nil;\n  FFaces := TFasterListTFreeSubdivisionFace.Create;\n  try')
    s=replace(s,'  FreeAndNil(Seedfaces);\n  FreeAndNil(FFaces);',
        '  finally FreeAndNil(Seedfaces);FreeAndNil(FFaces) end;')
    s=replace(s,'    MessageDlg(rsLocSeedMissing, mtError, [mbOK], 0);',
        '    raise Exception.Create(rsLocSeedMissing);')
    # This loop ran zero times (2 to 1), so bent centreline seams were accepted.
    start=s.index('                  for J := 2 to 1 do')
    end=s.index('                  if FMirror then',start)
    s=s[:start]+'''                  for K := 0 to FPoints.Count-1 do
                  if Abs(FPoints[K].Coordinate.Y)<=1e-4 then begin
                    P3D3.X:=F2DCoordinates[K].Coordinate.X;
                    P3D3.Y:=F2DCoordinates[K].Coordinate.Y;P3D3.Z:=0;
                    if DistancepointToLine(P3D3,P3D1,P3D2)>1e-3 then begin
                      FMirror:=False;Break;
                    end;
                  end;
'''+s[end:]
    # A section exactly through an existing edge used to disappear. Add each
    # coplanar edge once, but do not invent a curve for a whole coplanar face.
    start=s.index('procedure TFreeDevelopedPatch.IntersectPlane(')
    end=s.index('end;{TFreeDevelopedPatch.IntersectPlane}',start)
    part=s[start:end]
    part=replace(part,'  NoPoints: integer;', '  NoPoints: integer;\n  SeenEdges:TFPList;AllInPlane:Boolean;')
    part=replace(part,'  Dest := TFasterListTFreeSpline.Create;',
        '  Dest := TFasterListTFreeSpline.Create;SeenEdges:=TFPList.Create;\n  try')
    part=replace(part,'    NoPoints := 0;', '''    PlateCheckpoint;NoPoints := 0;AllInPlane:=True;
    for K:=0 to Face.NumberOfPoints-1 do begin
      P1:=Face.Point[K];
      if Abs(Plane.A*P1.Coordinate.X+Plane.B*P1.Coordinate.Y+Plane.C*P1.Coordinate.Z+Plane.D)>1e-5 then AllInPlane:=False;
    end;
    if AllInPlane then Continue;''')
    part=replace(part,'        begin\n        end\n        else if abs(Side2) < 1e-5 then', '''        begin
          Edge:=Owner.Surface.EdgeExists(P1,P2);
          if (Edge<>nil) and (SeenEdges.IndexOf(Edge)<0) then begin
            SeenEdges.Add(Edge);Spline:=TFreeSpline.Create(Owner.Surface);
            Spline.Color:=Color;Dest.Add(Spline);
            Index1:=FPoints.SortedIndexOf(P1);Index2:=FPoints.SortedIndexOf(P2);
            Output:=SetPoint(F2DCoordinates[Index1].Coordinate.X,F2DCoordinates[Index1].Coordinate.Y,0);Spline.Add(Output);Spline.Knuckle[0]:=True;
            Output:=SetPoint(F2DCoordinates[Index2].Coordinate.X,F2DCoordinates[Index2].Coordinate.Y,0);Spline.Add(Output);Spline.Knuckle[1]:=True;
          end;
        end
        else if abs(Side2) < 1e-5 then''')
    # Ownership of completed curves transfers to the patch below. Exceptions
    # during collection must release the temporary spline list and its entries.
    split=part.index('  for I := 1 to Dest.Count do\n  begin\n    Spline := Dest[I - 1];')
    part=part[:split]+'''  except
    for I:=0 to Dest.Count-1 do Dest[I].Free;
    Dest.Free;SeenEdges.Free;raise;
  end;
  SeenEdges.Free;
'''+part[split:]
    s=s[:start]+part+s[end:]
    path.write_text(s,encoding='utf8')
    path=work/'Units/FreeSubdivisionLayer.inc';s=path.read_text('utf-8-sig')
    start=s.index('procedure TFreeSubdivisionLayer.Unroll(');part=s[start:]
    part=replace(part,'        FindAttachedFaces(Current, Face);\n        DoneList.Add(Current);',
        '        DoneList.Add(Current);\n        FindAttachedFaces(Current, Face);')
    part=replace(part,'          Patch.Unroll(Current);','          try Patch.Unroll(Current) except Patch.Free;raise end;')
    part=replace(part,'        FreeAndNil(Current);','        FreeAndNil(Current);DoneList[I-1]:=nil;')
    part=replace(part,'    FreeAndNil(ToDoList);\n    FreeAndNil(DoneList);',
        '    for I:=0 to DoneList.Count-1 do DoneList[I].Free;\n    FreeAndNil(ToDoList);\n    FreeAndNil(DoneList);')
    path.write_text(s[:start]+part,encoding='utf8')
