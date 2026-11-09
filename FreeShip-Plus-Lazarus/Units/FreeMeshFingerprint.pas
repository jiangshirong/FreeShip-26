unit FreeMeshFingerprint;
{$mode objfpc}{$H+}
interface
uses FreeGeometry,FreeTypes;
function MeshFillFingerprint(S:TFreeSubdivisionSurface):string;
function MeshSectionFingerprint(S:TFreeSubdivisionSurface;const Plane:T3DPlane;HydroOnly,Simplify:Boolean):string;
implementation
uses SHA1;
function MeshFillFingerprint(S:TFreeSubdivisionSurface):string;
var C:TSHA1Context;D:TSHA1Digest;I,J,K,L,N:Integer;F:TFreeSubdivisionFace;P:T3DCoordinate;
begin
 SHA1Init(C);
 for I:=0 to S.NumberOfLayers-1 do begin
  N:=S.Layer[I].Count;SHA1Update(C,N,SizeOf(N));
  for J:=0 to S.Layer[I].Count-1 do begin
   N:=S.Layer[I].Items[J].ChildCount;SHA1Update(C,N,SizeOf(N));
   for K:=0 to N-1 do begin
    F:=S.Layer[I].Items[J].Child[K];L:=F.NumberOfPoints;SHA1Update(C,L,SizeOf(L));
    for L:=0 to F.NumberOfPoints-1 do begin
     P:=F.Point[L].Coordinate;SHA1Update(C,P.X,SizeOf(P.X));SHA1Update(C,P.Y,SizeOf(P.Y));SHA1Update(C,P.Z,SizeOf(P.Z));
    end;
   end;
  end;
 end;
 SHA1Final(C,D);Result:=SHA1Print(D);
end;
function MeshSectionFingerprint(S:TFreeSubdivisionSurface;const Plane:T3DPlane;HydroOnly,Simplify:Boolean):string;
var C:TSHA1Context;D:TSHA1Digest;I,J,K,L,M,N:Integer;
 Ctrl:TFreeSubdivisionControlFace;F,Neighbor:TFreeSubdivisionFace;E:TFreeSubdivisionEdge;
 P:T3DCoordinate;B:Boolean;
 procedure Int(V:Integer);begin SHA1Update(C,V,SizeOf(V)) end;
 procedure PointData(Q:TFreeSubdivisionPoint);
 begin P:=Q.Coordinate;SHA1Update(C,P.X,SizeOf(P.X));SHA1Update(C,P.Y,SizeOf(P.Y));SHA1Update(C,P.Z,SizeOf(P.Z)) end;
begin
 SHA1Init(C);SHA1Update(C,Plane,SizeOf(Plane));SHA1Update(C,HydroOnly,SizeOf(HydroOnly));SHA1Update(C,Simplify,SizeOf(Simplify));
 // The whole control topology is cheap to check and prevents connectivity edits
 // outside the slice from accidentally reusing a previous intersection.
 Int(S.NumberOfControlPoints);Int(S.NumberOfControlFaces);
 for I:=0 to S.NumberOfControlFaces-1 do begin
  Ctrl:=S.ControlFace[I];Int(Ctrl.NumberOfPoints);Int(Ctrl.ChildCount);
  for J:=0 to Ctrl.NumberOfPoints-1 do Int(Ctrl.Point[J].VertexIndex);
 end;
 for I:=0 to S.NumberOfLayers-1 do begin
  if HydroOnly then B:=S.Layer[I].UseInHydrostatics else B:=S.Layer[I].UseForIntersections;
  SHA1Update(C,B,SizeOf(B));if not B then Continue;
  for J:=0 to S.Layer[I].Count-1 do begin
   Ctrl:=S.Layer[I].Items[J];
   if not PlaneIntersectsBox(Ctrl.Min,Ctrl.Max,Plane) then Continue;
   Int(I);Int(J);Int(Ctrl.ChildCount);
   for K:=0 to Ctrl.ChildCount-1 do begin
    F:=Ctrl.Child[K];Int(F.NumberOfPoints);
    for L:=0 to F.NumberOfPoints-1 do begin
     PointData(F.Point[L]);
     E:=S.EdgeExists(F.Point[L],F.Point[(L+1) mod F.NumberOfPoints]);
     B:=E<>nil;SHA1Update(C,B,SizeOf(B));if not B then Continue;
     B:=E.Crease;SHA1Update(C,B,SizeOf(B));Int(E.NumberOfFaces);
     // In-plane edge handling inspects adjacent faces too.
     for M:=0 to E.NumberOfFaces-1 do begin
      Neighbor:=E.Face[M];Int(Neighbor.NumberOfPoints);
      for N:=0 to Neighbor.NumberOfPoints-1 do PointData(Neighbor.Point[N]);
     end;
    end;
   end;
  end;
 end;
 SHA1Final(C,D);Result:=SHA1Print(D);
end;

end.
