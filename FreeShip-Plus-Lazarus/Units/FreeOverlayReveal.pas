unit FreeOverlayReveal;
{$mode objfpc}{$H+}
interface
uses Graphics,Types,FreeSmoothLines;
type
 TRevealLine=record A,B:TSmoothPoint;Color:TColor;Width:Integer;Style:TPenStyle end;
 TRevealLines=array of TRevealLine;
 TRevealNode=record P:TSmoothPoint;Level:Integer;Distance:Double end;
 TRevealEdge=record A,B:Integer;Paint:TRevealLine;Span:Double end;
 TOverlayReveal=class
 private
  FPending,FPrevious:array of TRevealLine;
  FPendingCount,FBuildCount:Integer;
  FVisibleSegments:array of TSmoothSegment;
  FNodes:array of TRevealNode;FEdges:array of TRevealEdge;FCenter:TPoint;FMaxLevel:Integer;FMaxDistance:Double;
  function Vertex(const P:TSmoothPoint):Integer;
  procedure Build(const Center:TPoint);
 public
  function PendingCount:Integer;
  function PendingTail(Start:Integer):TRevealLines;
  procedure Append(const Lines:TRevealLines);
  procedure BeginFrame;
  procedure AddLine(X1,Y1,X2,Y2:Double;Color:TColor;Width:Integer;Style:TPenStyle);
  procedure Draw(Canvas:TCanvas;Progress:Double;const Center:TPoint);
  function LevelAt(X,Y:Double):Integer;
  property BuildCount:Integer read FBuildCount;
 end;
implementation
uses Math,SysUtils;
function Pnt(X,Y:Double):TSmoothPoint;begin Result.X:=X;Result.Y:=Y end;
function TOverlayReveal.PendingCount:Integer;begin Result:=FPendingCount end;
function TOverlayReveal.PendingTail(Start:Integer):TRevealLines;
begin Result:=Copy(FPending,Start,FPendingCount-Start) end;
procedure TOverlayReveal.Append(const Lines:TRevealLines);
var I:Integer;
begin for I:=0 to High(Lines) do with Lines[I] do AddLine(A.X,A.Y,B.X,B.Y,Color,Width,Style) end;
procedure TOverlayReveal.BeginFrame;begin FPendingCount:=0 end;
procedure TOverlayReveal.AddLine(X1,Y1,X2,Y2:Double;Color:TColor;Width:Integer;Style:TPenStyle);
var N:Integer;
begin
 if Hypot(X2-X1,Y2-Y1)<0.01 then Exit;
 N:=FPendingCount;Inc(FPendingCount);if FPendingCount>Length(FPending) then SetLength(FPending,Max(64,Length(FPending)*2));FillChar(FPending[N],SizeOf(TRevealLine),0);
 FPending[N].A:=Pnt(X1,Y1);FPending[N].B:=Pnt(X2,Y2);FPending[N].Color:=Color;FPending[N].Width:=Width;FPending[N].Style:=Style;
end;
function TOverlayReveal.Vertex(const P:TSmoothPoint):Integer;
var I:Integer;
begin
 for I:=0 to High(FNodes) do if Hypot(P.X-FNodes[I].P.X,P.Y-FNodes[I].P.Y)<0.5 then Exit(I);
 Result:=Length(FNodes);SetLength(FNodes,Result+1);FNodes[Result].P:=P;FNodes[Result].Level:=-1;
end;
procedure TOverlayReveal.Build(const Center:TPoint);
type TSplits=array of Double;
var Splits:array of TSplits;Queue:array of Integer;
 I,J,K,N,A,B,Root,Source,Head,Tail,V,W,Visited,NearLevel:Integer;
 Used:array of Boolean;
 D,T,U,DX,DY,EX,EY,QX,QY,Temp,Best:Double;P:TSmoothPoint;
 procedure PushCut(Index:Integer;Value:Double);
 var C:Integer;
 begin C:=Length(Splits[Index]);SetLength(Splits[Index],C+1);Splits[Index][C]:=Max(0.0,Min(1.0,Value)) end;
 procedure Flood(Start,Level:Integer);
 var E:Integer;
 begin
  Head:=0;Tail:=1;Queue[0]:=Start;FNodes[Start].Level:=Level;
  while Head<Tail do begin V:=Queue[Head];Inc(Head);
   for E:=0 to High(FEdges) do begin
    W:=-1;if FEdges[E].A=V then W:=FEdges[E].B else if FEdges[E].B=V then W:=FEdges[E].A;
    if (W>=0) and (FNodes[W].Level<0) then begin FNodes[W].Level:=FNodes[V].Level+1;Queue[Tail]:=W;Inc(Tail) end;
   end;
  end;
 end;
begin
 Inc(FBuildCount);Visited:=0;
 FPrevious:=Copy(FPending,0,FPendingCount);FCenter:=Center;SetLength(FNodes,0);SetLength(FEdges,0);FMaxLevel:=0;
 SetLength(Splits,FPendingCount);
 for I:=0 to FPendingCount-1 do begin
  PushCut(I,0);PushCut(I,1);
  N:=Max(1,Ceil(Hypot(FPending[I].B.X-FPending[I].A.X,FPending[I].B.Y-FPending[I].A.Y)/32));
  for K:=1 to N-1 do PushCut(I,K/N);
 end;
 // Split crossing lines first: shared interior intersections are real BFS junctions.
 for I:=0 to FPendingCount-1 do for J:=I+1 to FPendingCount-1 do begin
  DX:=FPending[I].B.X-FPending[I].A.X;DY:=FPending[I].B.Y-FPending[I].A.Y;
  EX:=FPending[J].B.X-FPending[J].A.X;EY:=FPending[J].B.Y-FPending[J].A.Y;D:=DX*EY-DY*EX;
  if Abs(D)<1e-8 then Continue;
  QX:=FPending[J].A.X-FPending[I].A.X;QY:=FPending[J].A.Y-FPending[I].A.Y;
  T:=(QX*EY-QY*EX)/D;U:=(QX*DY-QY*DX)/D;
  if (T>=0) and (T<=1) and (U>=0) and (U<=1) then begin PushCut(I,T);PushCut(J,U) end;
 end;
 for I:=0 to FPendingCount-1 do begin
  for J:=0 to High(Splits[I])-1 do for K:=J+1 to High(Splits[I]) do if Splits[I][J]>Splits[I][K] then begin Temp:=Splits[I][J];Splits[I][J]:=Splits[I][K];Splits[I][K]:=Temp end;
  for J:=0 to High(Splits[I])-1 do begin
   T:=Splits[I][J];U:=Splits[I][J+1];if U-T<1e-7 then Continue;
   P:=Pnt(FPending[I].A.X+(FPending[I].B.X-FPending[I].A.X)*T,FPending[I].A.Y+(FPending[I].B.Y-FPending[I].A.Y)*T);A:=Vertex(P);
   P:=Pnt(FPending[I].A.X+(FPending[I].B.X-FPending[I].A.X)*U,FPending[I].A.Y+(FPending[I].B.Y-FPending[I].A.Y)*U);B:=Vertex(P);
   if A=B then Continue;N:=Length(FEdges);SetLength(FEdges,N+1);FEdges[N].A:=A;FEdges[N].B:=B;FEdges[N].Paint:=FPending[I];
  end;
 end;
 if Length(FNodes)=0 then Exit;SetLength(Queue,Length(FNodes));Best:=1e100;Root:=0;
 for I:=0 to High(FNodes) do begin D:=Hypot(FNodes[I].P.X-Center.X,FNodes[I].P.Y-Center.Y);if D<Best then begin Best:=D;Root:=I end end;
 Source:=Root;Flood(Root,0);
 // Continue disconnected components from the nearest reached node, without drawing a fake bridge.
 repeat
  Root:=-1;NearLevel:=0;Best:=1e100;
  for I:=0 to High(FNodes) do if FNodes[I].Level<0 then
   for J:=0 to High(FNodes) do if FNodes[J].Level>=0 then begin D:=Hypot(FNodes[I].P.X-FNodes[J].P.X,FNodes[I].P.Y-FNodes[J].P.Y);if D<Best then begin Best:=D;Root:=I;NearLevel:=FNodes[J].Level+1 end end;
  if Root>=0 then Flood(Root,NearLevel);
 until Root<0;
 for I:=0 to High(FNodes) do FMaxLevel:=Max(FMaxLevel,FNodes[I].Level);
 // Travel within BFS-connected branches uses pixel distance, not tessellation count.
 SetLength(Used,Length(FNodes));FMaxDistance:=0;
 for I:=0 to High(FNodes) do FNodes[I].Distance:=1e100;
 FNodes[Source].Distance:=0;
 for K:=0 to High(FNodes) do begin
  V:=-1;Best:=1e100;
  for I:=0 to High(FNodes) do if not Used[I] and (FNodes[I].Distance<Best) then begin V:=I;Best:=FNodes[I].Distance end;
  if V<0 then begin
   for I:=0 to High(FNodes) do if not Used[I] then
    for J:=0 to High(FNodes) do if Used[J] then begin D:=Hypot(FNodes[I].P.X-FNodes[J].P.X,FNodes[I].P.Y-FNodes[J].P.Y);if D<Best then begin V:=I;Best:=D;Visited:=J end end;
   if V<0 then Break;FNodes[V].Distance:=FNodes[Visited].Distance+Best;
  end;
  Used[V]:=True;FMaxDistance:=Max(FMaxDistance,FNodes[V].Distance);
  for I:=0 to High(FEdges) do begin
   W:=-1;if FEdges[I].A=V then W:=FEdges[I].B else if FEdges[I].B=V then W:=FEdges[I].A;
   if (W>=0) and not Used[W] then begin D:=FNodes[V].Distance+Hypot(FNodes[V].P.X-FNodes[W].P.X,FNodes[V].P.Y-FNodes[W].P.Y);FNodes[W].Distance:=Min(FNodes[W].Distance,D) end;
  end;
 end;

 // Orient and measure edges once; animation frames only interpolate.
 for I:=0 to High(FEdges) do begin
  A:=FEdges[I].A;B:=FEdges[I].B;
  if FNodes[A].Distance>FNodes[B].Distance then begin FEdges[I].A:=B;FEdges[I].B:=A end;
  FEdges[I].Span:=Max(0.001,Hypot(FNodes[A].P.X-FNodes[B].P.X,FNodes[A].P.Y-FNodes[B].P.Y));
 end;
 SetLength(FVisibleSegments,Length(FEdges));
end;
procedure TOverlayReveal.Draw(Canvas:TCanvas;Progress:Double;const Center:TPoint);
var Rebuild:Boolean;I,A,B,N:Integer;T:Double;X,Y:TSmoothPoint;Saved:TPen;Paint:TRevealLine;
 procedure Flush;
 var J:Integer;
 begin
  if N=0 then Exit;
  Canvas.Pen.Color:=Paint.Color;Canvas.Pen.Width:=Paint.Width;Canvas.Pen.Style:=Paint.Style;
  if not DrawSmoothSegmentPath(Canvas,Slice(FVisibleSegments,N)) then
    for J:=0 to N-1 do Canvas.Line(Round(FVisibleSegments[J].A.X),Round(FVisibleSegments[J].A.Y),Round(FVisibleSegments[J].B.X),Round(FVisibleSegments[J].B.Y));
  N:=0;
 end;
begin
 Rebuild:=(FPendingCount<>Length(FPrevious)) or (Center.X<>FCenter.X) or (Center.Y<>FCenter.Y);
 if not Rebuild and (FPendingCount>0) then Rebuild:=not CompareMem(@FPending[0],@FPrevious[0],FPendingCount*SizeOf(TRevealLine));
 if Rebuild then Build(Center);
 Saved:=TPen.Create;Saved.Assign(Canvas.Pen);N:=0;
 try
 for I:=0 to High(FEdges) do begin
  A:=FEdges[I].A;B:=FEdges[I].B;
  T:=Max(0.0,Min(1.0,(Progress*FMaxDistance-FNodes[A].Distance)/FEdges[I].Span));if Progress>=1 then T:=1;if T<=0 then Continue;
  X:=FNodes[A].P;Y:=FNodes[B].P;Y.X:=X.X+(Y.X-X.X)*T;Y.Y:=X.Y+(Y.Y-X.Y)*T;
  if (N>0) and ((Paint.Color<>FEdges[I].Paint.Color) or (Paint.Width<>FEdges[I].Paint.Width) or (Paint.Style<>FEdges[I].Paint.Style)) then Flush;
  if N=0 then Paint:=FEdges[I].Paint;
  FVisibleSegments[N].A:=X;FVisibleSegments[N].B:=Y;Inc(N);
 end;
 Flush;
 finally Canvas.Pen.Assign(Saved);Saved.Free end;
end;
function TOverlayReveal.LevelAt(X,Y:Double):Integer;
var I:Integer;
begin Result:=-1;for I:=0 to High(FNodes) do if Hypot(X-FNodes[I].P.X,Y-FNodes[I].P.Y)<0.5 then Exit(FNodes[I].Level) end;
end.
