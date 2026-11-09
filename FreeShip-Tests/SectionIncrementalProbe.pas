program SectionIncrementalProbe;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Math,Main,FreeShipUnit,FreeFileBuffer,FreeGeometry,FreeTypes,FreeMeshFingerprint;
var S:TFreeShip;B:TFreeFileBuffer;Items:array of TFreeIntersection;I,J,N,Reused,Changed,Before:Integer;
 P:TFreeSubdivisionControlPoint;Old,NewPoint:T3DCoordinate;OriginalKey:string;
procedure CompareFresh(A:TFreeIntersection);
var F:TFreeIntersection;K,L:Integer;X,Y:T3DCoordinate;
begin
 F:=TFreeIntersection.Create(S);
 try
  F.Plane:=A.Plane;F.IntersectionType:=A.IntersectionType;
  F.UseHydrostaticsSurfacesOnly:=A.UseHydrostaticsSurfacesOnly;F.Rebuild;
  if F.Count<>A.Count then raise Exception.Create('Spline count mismatch');
  for K:=0 to A.Count-1 do begin
   if A.Items[K].NumberOfPoints<>F.Items[K].NumberOfPoints then raise Exception.Create('Point count mismatch');
   for L:=0 to A.Items[K].NumberOfPoints-1 do begin
    X:=A.Items[K].Point[L];Y:=F.Items[K].Point[L];
    if (Abs(X.X-Y.X)>1e-9) or (Abs(X.Y-Y.Y)>1e-9) or (Abs(X.Z-Y.Z)>1e-9) or
       (A.Items[K].Knuckle[L]<>F.Items[K].Knuckle[L]) then raise Exception.Create('Coordinates or knuckle differ');
   end;
  end;
 finally F.Free end;
end;
procedure Add(A:TFreeIntersection);
begin N:=Length(Items);SetLength(Items,N+1);Items[N]:=A end;
procedure Check(const Name:string;ExpectReuse:Boolean);
begin
 Reused:=0;Changed:=0;
 for I:=0 to High(Items) do begin
  Before:=Items[I].ReuseCount;Items[I].Rebuild;
  if Items[I].ReuseCount>Before then Inc(Reused) else Inc(Changed);
  CompareFresh(Items[I]);
 end;
 WriteLn(Name,': reused=',Reused,' rebuilt=',Changed);
 if ExpectReuse and (Reused=0) then raise Exception.Create('Expected unaffected sections to be reused');
end;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);S:=MainForm.FreeShip;
 B:=TFreeFileBuffer.Create;B.LoadFromFile(SysUtils.GetEnvironmentVariable('FREESHIP_TEST_MODEL'));S.LoadProject(B);B.Free;S.RebuildModel;
 if S.NumberOfStations=0 then
  for J:=1 to 9 do S.Edit.Intersection_Add(fiStation,S.Surface.Min.X+(S.Surface.Max.X-S.Surface.Min.X)*J/10);
 for J:=0 to S.NumberOfStations-1 do Add(S.Station[J]);
 for J:=0 to S.NumberOfButtocks-1 do Add(S.Buttock[J]);
 for J:=0 to S.NumberOfWaterlines-1 do Add(S.Waterline[J]);
 for J:=0 to S.NumberOfDiagonals-1 do Add(S.Diagonal[J]);
 if Length(Items)=0 then raise Exception.Create('Fixture has no persistent intersections');
 Check('initial',False);Check('unchanged',True);OriginalKey:=MeshFillFingerprint(S.Surface);
 P:=nil;
 for J:=0 to S.Surface.NumberOfControlPoints-1 do if not S.Surface.ControlPoint[J].Locked then begin P:=S.Surface.ControlPoint[J];Break end;
 if P=nil then raise Exception.Create('No editable fixture control point');
 Old:=P.Coordinate;NewPoint:=Old;NewPoint.Z:=Old.Z+0.01;P.Coordinate:=NewPoint;S.Built:=False;S.RebuildModel;
 if MeshFillFingerprint(S.Surface)=OriginalKey then raise Exception.Create('Changed model retained disk identity');
 Check('moved',False);
 if Changed=0 then raise Exception.Create('Expected affected sections to rebuild');
 P.Coordinate:=Old;S.Built:=False;S.RebuildModel;Check('restored',False);
 if MeshFillFingerprint(S.Surface)<>OriginalKey then raise Exception.Create('Restored model has unstable disk identity');
 S.ProjectSettings.ProjectSimplifyIntersections:=not S.ProjectSettings.ProjectSimplifyIntersections;
 Check('simplification',False);
 if Reused<>0 then raise Exception.Create('Simplification change used old result');
 WriteLn('PASS incremental sections match full recomputation');Halt(0);
end.
