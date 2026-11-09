{ FreeShip Qt migration adapter. GPL-2.0-or-later; see COPYING. }
unit StableEdgeLoops;
{$mode objfpc}{$H+}
interface
uses FreeGeometry,FasterList;
procedure ExtractStableEdgeLoops(Surface:TFreeSubdivisionSurface;
  Destination:TFasterListTFasterListTFreeSubdivisionPoint);
implementation
uses Classes,SysUtils,FGL,FreeTypes;
type
  TEdgeEntry=record Edge:TFreeSubdivisionEdge;Key:string end;
  TEdgeEntries=array of TEdgeEntry;
  TEdgeIndex=specialize TFPGMap<PtrUInt,Integer>;

function PointKey(P:TFreeSubdivisionPoint):string;
var C:T3DCoordinate;Bits:LongWord;
begin
  C:=P.Coordinate;
  Move(C.X,Bits,4);Result:=IntToHex(Bits,8);
  Move(C.Y,Bits,4);Result:=Result+IntToHex(Bits,8);
  Move(C.Z,Bits,4);Result:=Result+IntToHex(Bits,8);
end;
function EdgeKey(E:TFreeSubdivisionEdge):string;
var A,B:string;
begin
  A:=PointKey(E.StartPoint);B:=PointKey(E.EndPoint);
  if A<B then Result:=A+B else Result:=B+A;
end;
function NeighborhoodKey(P:TFreeSubdivisionPoint):string;
var Parts:TStringList;I:Integer;
begin
  Parts:=TStringList.Create;
  try
    Parts.Sorted:=True;
    for I:=0 to P.NumberOfEdges-1 do if P.Edge[I].Crease then Parts.Add(EdgeKey(P.Edge[I]));
    Result:=Parts.Text;
  finally Parts.Free end;
end;
function StableKey(E:TFreeSubdivisionEdge):string;
var Parts,Face:TStringList;A,B:string;I,J:Integer;
begin
  A:=NeighborhoodKey(E.StartPoint);B:=NeighborhoodKey(E.EndPoint);
  if A>B then begin Result:=A;A:=B;B:=Result end;
  Result:=EdgeKey(E)+':'+A+'|'+B;
  // Coincident edges can belong to different faces. Include their geometric
  // neighborhoods so allocation order never serves as a tie breaker.
  Parts:=TStringList.Create;Face:=TStringList.Create;
  try
    Parts.Sorted:=True;Face.Sorted:=True;
    for I:=0 to E.NumberOfFaces-1 do begin
      Face.Clear;
      for J:=0 to E.Face[I].NumberOfPoints-1 do Face.Add(PointKey(E.Face[I].Point[J]));
      Parts.Add(Face.Text);
    end;
    Result:=Result+'|'+Parts.Text;
  finally Face.Free;Parts.Free end;
end;
procedure SortEntries(var Entries:TEdgeEntries;Left,Right:Integer);
var I,J:Integer;Pivot:string;Temp:TEdgeEntry;
begin
  I:=Left;J:=Right;Pivot:=Entries[(Left+Right) div 2].Key;
  repeat
    while Entries[I].Key<Pivot do Inc(I);
    while Entries[J].Key>Pivot do Dec(J);
    if I<=J then begin Temp:=Entries[I];Entries[I]:=Entries[J];Entries[J]:=Temp;Inc(I);Dec(J) end;
  until I>J;
  if Left<J then SortEntries(Entries,Left,J);
  if I<Right then SortEntries(Entries,I,Right);
end;
function SharesFace(A,B:TFreeSubdivisionEdge):Boolean;
var I,J:Integer;
begin
  for I:=0 to A.NumberOfFaces-1 do
    for J:=0 to B.NumberOfFaces-1 do if A.Face[I]=B.Face[J] then Exit(True);
  Result:=False;
end;
procedure ExtractStableEdgeLoops(Surface:TFreeSubdivisionSurface;
  Destination:TFasterListTFasterListTFreeSubdivisionPoint);
var Entries:TEdgeEntries;Index:TEdgeIndex;Visited:array of Boolean;
    I,N,Seed,Next,CurrentIndex:Integer;Current,Other:TFreeSubdivisionPoint;
    Points:TFasterListTFreeSubdivisionPoint;E:TFreeSubdivisionEdge;
  function NextAt(P:TFreeSubdivisionPoint;Previous:TFreeSubdivisionEdge):Integer;
  var J,K,Candidate:Integer;Edge:TFreeSubdivisionEdge;
  begin
    Result:=-1;
    if not P.RegularPoint or (P.VertexType=svCorner) then Exit;
    for J:=0 to P.NumberOfEdges-1 do begin
      Edge:=P.Edge[J];
      if (Edge=Previous) or not Edge.Crease or SharesFace(Edge,Previous) then Continue;
      K:=Index.IndexOf(PtrUInt(Edge));if K<0 then Continue;
      Candidate:=Index.Data[K];
      if (Result<0) or (Candidate<Result) then Result:=Candidate;
    end;
  end;
  function Opposite(Edge:TFreeSubdivisionEdge;P:TFreeSubdivisionPoint):TFreeSubdivisionPoint;
  begin
    if Edge.StartPoint=P then Result:=Edge.EndPoint
    else if Edge.EndPoint=P then Result:=Edge.StartPoint
    else raise Exception.Create('Disconnected crease edge');
  end;
begin
  SetLength(Entries,Surface.NumberOfSubDivEdges);N:=0;
  for I:=0 to Surface.NumberOfSubDivEdges-1 do begin
    E:=Surface.SubDivEdge[I];if not E.Crease then Continue;
    Entries[N].Edge:=E;Entries[N].Key:=StableKey(E);Inc(N);
  end;
  SetLength(Entries,N);if N=0 then Exit;
  SortEntries(Entries,0,N-1);SetLength(Visited,N);Index:=TEdgeIndex.Create;
  try
    for I:=0 to N-1 do Index.Add(PtrUInt(Entries[I].Edge),I);
    Index.Sorted:=True;
    for Seed:=0 to N-1 do begin
      if Visited[Seed] then Continue;
      Visited[Seed]:=True;E:=Entries[Seed].Edge;
      Points:=TFasterListTFreeSubdivisionPoint.Create(False,False);
      try
        if PointKey(E.StartPoint)<PointKey(E.EndPoint) then begin Current:=E.StartPoint;Other:=E.EndPoint end
        else begin Current:=E.EndPoint;Other:=E.StartPoint end;
        Points.Add(Current);Points.Add(Other);CurrentIndex:=Seed;
        // Trace the same crease/regular-point/non-shared-face relation as the
        // legacy code, keeping orientation in local variables. Its getters
        // mutate shared edge endpoints; its pointer-based seed splits chains.
        repeat
          Next:=NextAt(Current,Entries[CurrentIndex].Edge);
          if (Next<0) or Visited[Next] then Break;
          Visited[Next]:=True;Current:=Opposite(Entries[Next].Edge,Current);
          Points.Insert(0,Current);CurrentIndex:=Next;
        until False;
        Current:=Other;CurrentIndex:=Seed;
        repeat
          Next:=NextAt(Current,Entries[CurrentIndex].Edge);
          if (Next<0) or Visited[Next] then Break;
          Visited[Next]:=True;Current:=Opposite(Entries[Next].Edge,Current);
          Points.Add(Current);CurrentIndex:=Next;
        until False;
        Destination.Add(Points);Points:=nil;
      finally Points.Free end;
    end;
  finally Index.Free end;
end;
end.
