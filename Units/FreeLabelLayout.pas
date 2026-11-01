{ Pixel-space label placement adapted from the user's spatial-model reuse package.
  The geometry and text remain owned by the caller; no model data is changed. }
unit FreeLabelLayout;
{$mode objfpc}{$H+}
interface
uses Classes, SysUtils, Types, Math;
type
  TLabelKind = (lkPoint, lkNamedLine, lkVerticalTick, lkHorizontalTick);
  TLabelExtraCost = function(const R: TRect): integer of object;
  TLabelPlacement = record
    Position: TPoint;
    Rect: TRect;
    Collisions: integer;
    Visible: boolean;
  end;
  TLabelSegment = record A, B: TPoint; Bounds: TRect end;
  TLabelCacheEntry = class
    Text: string;
    Width, Height: integer;
    Offset, Target, PreferredOffset: TPoint;
    Kind: TLabelKind;
    Outward: integer;
    LastSeen: QWord;
    ShownLeft, ShownRight: Integer;
    MotionX, MotionY, VelocityX, VelocityY, EdgeX, EdgeY: Double;
    MotionReady: Boolean;
    DrawnRect: TRect;
    DrawnPosition: TPoint;
  end;
  TFreeLabelLayout = class
  private
    FBounds: TRect;
    FContext: string;
    FActive: boolean;
    FAnimating: boolean;
    FAnimationEnabled: boolean;
    FTranslationOnly: Boolean;
    FReusedPlacementCount: Integer;
    FLastFrame: QWord;
    FFrame: QWord;
    FPlacingCache: TLabelCacheEntry;
    FPlacingKind: TLabelKind;
    FFrameStep: double;
    FOnFrameReady: TNotifyEvent;
    FReferenceValues:array of record Value:string;Anchor:TPoint;Width,Height:Integer end;
    FRects: array of TRect;
    FSegments: array of TLabelSegment;
    FCache: TStringList;
    FCoordinateClaims:TStringList;
    FExtraCost: TLabelExtraCost;
    function FindCache(const Key: string): TLabelCacheEntry;
    function CollisionCount(const R: TRect): integer;
    function RectCollides(const R: TRect): boolean;
    function AvailableRect(const Full: TRect): TRect;
  public
    constructor Create;
    destructor Destroy; override;
    procedure Clear;
    procedure TranslateHistory(DX,DY:Integer);
    procedure BeginFrame(const Bounds: TRect; const Context: string = '');
    procedure EndFrame;
    property Active: boolean read FActive;
    property Animating: boolean read FAnimating;
    property AnimationEnabled: boolean read FAnimationEnabled write FAnimationEnabled;
    property TranslationOnly: Boolean read FTranslationOnly write FTranslationOnly;
    property ReusedPlacementCount: Integer read FReusedPlacementCount;
    property OnFrameReady: TNotifyEvent read FOnFrameReady write FOnFrameReady;
    function ClaimCoordinateLabel(const Anchor:TPoint;const Value:string):Boolean;
    procedure Reserve(const R: TRect);
    procedure AddSegment(const A, B: TPoint);
    property ExtraCost: TLabelExtraCost read FExtraCost write FExtraCost;
    function Place(const Key, Text: string; const Anchor, Preferred: TPoint;
      Width, Height: integer; Kind: TLabelKind; Outward: integer = 0): TLabelPlacement;
    class function EdgeAttraction(const Bounds, Full: TRect): TPoint; static;
    class function SegmentHitsRect(const R: TRect; const A, B: TPoint): boolean; static;
  end;
implementation
constructor TFreeLabelLayout.Create;
begin
  inherited Create;
  FAnimationEnabled := True;
  FCoordinateClaims:=TStringList.Create;
  FCoordinateClaims.Sorted:=True;FCoordinateClaims.CaseSensitive:=True;
  FCache := TStringList.Create;
  FCache.Sorted := True;
  FCache.CaseSensitive := True;
end;
destructor TFreeLabelLayout.Destroy;
begin Clear; FCoordinateClaims.Free;FCache.Free; inherited Destroy end;
procedure TFreeLabelLayout.Clear;
var I: integer;
begin
  for I := 0 to FCache.Count-1 do FCache.Objects[I].Free;
  FCache.Clear;
  FCoordinateClaims.Clear;
  SetLength(FRects, 0); SetLength(FSegments, 0);
end;
procedure TFreeLabelLayout.TranslateHistory(DX,DY:Integer);
var I:Integer; C:TLabelCacheEntry;
begin
  if (DX=0) and (DY=0) then Exit;
  for I:=0 to FCache.Count-1 do begin
    C:=TLabelCacheEntry(FCache.Objects[I]);
    OffsetRect(C.DrawnRect,DX,DY);
    Inc(C.DrawnPosition.X,DX); Inc(C.DrawnPosition.Y,DY);
  end;
end;

procedure TFreeLabelLayout.BeginFrame(const Bounds: TRect; const Context: string);
begin
  if FContext <> Context then Clear;
  Inc(FFrame);
  FCoordinateClaims.Clear;
  SetLength(FReferenceValues,0);
  FReusedPlacementCount:=0;
  FPlacingCache:=nil;
  if FCache.Count > 20000 then Clear;
  FBounds := Bounds;
  FContext := Context;
  if FLastFrame=0 then FFrameStep:=0
  else FFrameStep:=Min(8.0,Min(0.05,(GetTickCount64-FLastFrame)/1000.0)*180.0);
  FLastFrame:=GetTickCount64;
  FAnimating:=False;
  FActive := True;
  SetLength(FRects, 0); SetLength(FSegments, 0);
end;
procedure TFreeLabelLayout.EndFrame;
begin FActive := False; if not FAnimationEnabled then FAnimating := False; if Assigned(FOnFrameReady) then FOnFrameReady(Self) end;
function TFreeLabelLayout.ClaimCoordinateLabel(const Anchor:TPoint;const Value:string):Boolean;
var Key:string;Index:Integer;
begin
 Key:=IntToStr(Anchor.X)+':'+IntToStr(Anchor.Y)+':'+Value;
 Result:=not FCoordinateClaims.Find(Key,Index);
 if Result then FCoordinateClaims.Add(Key);
end;

procedure TFreeLabelLayout.Reserve(const R: TRect);
var N: integer;
begin N:=Length(FRects); SetLength(FRects,N+1); FRects[N]:=R end;
procedure TFreeLabelLayout.AddSegment(const A, B: TPoint);
var N: integer;
begin
  N:=Length(FSegments); SetLength(FSegments,N+1);
  FSegments[N].A:=A; FSegments[N].B:=B;
  FSegments[N].Bounds:=Types.Rect(Min(A.X,B.X),Min(A.Y,B.Y),Max(A.X,B.X)+1,Max(A.Y,B.Y)+1);
end;
function TFreeLabelLayout.FindCache(const Key: string): TLabelCacheEntry;
var I: integer;
begin
  Result:=nil;
  if (Key <> '') and FCache.Find(Key,I) then Result:=TLabelCacheEntry(FCache.Objects[I]);
end;
class function TFreeLabelLayout.EdgeAttraction(const Bounds, Full: TRect): TPoint;
var Radius,Strength:Double;
  function Pull(Distance:Integer):Integer;
  var T:Double;
  begin
    T:=Max(0.0,Min(1.6,1-Distance/Radius));
    Result:=Round(Strength*T*T);
  end;
begin
  Radius:=Max(24,(Full.Bottom-Full.Top)*2);
  Strength:=Max(8,(Full.Bottom-Full.Top)*0.75);
  Result:=Point(0,0);
  if Full.Left-Bounds.Left<Bounds.Right-Full.Right then
    Result.X:=-Pull(Full.Left-Bounds.Left)
  else Result.X:=Pull(Bounds.Right-Full.Right);
  if Full.Top-Bounds.Top<Bounds.Bottom-Full.Bottom then
    Result.Y:=-Pull(Full.Top-Bounds.Top)
  else Result.Y:=Pull(Bounds.Bottom-Full.Bottom);
end;

class function TFreeLabelLayout.SegmentHitsRect(const R: TRect; const A, B: TPoint): boolean;
var Lo,Hi,D,T0,T1: double;
  function Clip(Start, Delta, LowBound, HighBound: double): boolean;
  begin
    if Abs(Delta)<0.000001 then Exit((Start>=LowBound) and (Start<=HighBound));
    T0:=(LowBound-Start)/Delta; T1:=(HighBound-Start)/Delta;
    Lo:=Max(Lo,Min(T0,T1)); Hi:=Min(Hi,Max(T0,T1)); Result:=Lo<=Hi;
  end;
begin
  Lo:=0; Hi:=1; D:=B.X-A.X;
  Result:=Clip(A.X,D,R.Left,R.Right) and Clip(A.Y,B.Y-A.Y,R.Top,R.Bottom);
end;
function TFreeLabelLayout.RectCollides(const R: TRect): boolean;
var I: integer; B: TRect; Entry: TLabelCacheEntry;
begin
  // A later label still owns its last displayed rectangle while earlier labels
  // are being placed. Otherwise processing order can make it blink out.
  for I:=0 to FCache.Count-1 do begin
    Entry:=TLabelCacheEntry(FCache.Objects[I]);
    if (FPlacingKind=lkNamedLine) and (Entry.Kind<>lkNamedLine) then Continue;
    if (Entry=FPlacingCache) or (Entry.LastSeen+1<>FFrame) then Continue;
    B:=Entry.DrawnRect;
    if (R.Left<B.Right+2) and (R.Right+2>B.Left) and
       (R.Top<B.Bottom+2) and (R.Bottom+2>B.Top) then Exit(True);
  end;
  for I:=0 to High(FRects) do begin
    B:=FRects[I];
    if (R.Left<B.Right+2) and (R.Right+2>B.Left) and
       (R.Top<B.Bottom+2) and (R.Bottom+2>B.Top) then Exit(True);
  end;
  Result:=False;
end;
function TFreeLabelLayout.AvailableRect(const Full: TRect): TRect;
var I:Integer; C:TLabelCacheEntry;
  procedure Cut(const B:TRect);
  var LeftSpace,RightSpace:Integer;
  begin
    if (Result.Top>=B.Bottom+2) or (Result.Bottom<=B.Top-2) or
       (Result.Left>=B.Right+2) or (Result.Right<=B.Left-2) then Exit;
    LeftSpace:=Max(0,B.Left-2-Result.Left);
    RightSpace:=Max(0,Result.Right-B.Right-2);
    if LeftSpace>=RightSpace then Result.Right:=Max(Result.Left,B.Left-2)
    else Result.Left:=Min(Result.Right,B.Right+2);
  end;
begin
  Result:=Types.Rect(Max(Full.Left,FBounds.Left),Max(Full.Top,FBounds.Top),
    Min(Full.Right,FBounds.Right),Min(Full.Bottom,FBounds.Bottom));
  if FPlacingKind<>lkNamedLine then begin
    // A clipped number can become another valid number. Only viewport edges
    // may crop numeric labels; internal obstacles require a complete placement.
    if RectCollides(Result) then Result.Right:=Result.Left;
    Exit;
  end;
  for I:=0 to High(FRects) do Cut(FRects[I]);
  for I:=0 to FCache.Count-1 do begin
    C:=TLabelCacheEntry(FCache.Objects[I]);
    if (FPlacingKind=lkNamedLine) and (C.Kind<>lkNamedLine) then Continue;
    if (C<>FPlacingCache) and (C.LastSeen+1=FFrame) then Cut(C.DrawnRect);
  end;
end;

function TFreeLabelLayout.CollisionCount(const R: TRect): integer;
var I:integer; B:TRect;
begin
  Result:=0;
  for I:=0 to High(FRects) do begin
    B:=FRects[I];
    if (R.Left<B.Right+2) and (R.Right+2>B.Left) and
       (R.Top<B.Bottom+2) and (R.Bottom+2>B.Top) then Inc(Result, 20);
  end;
  B:=R; InflateRect(B,3,3);
  for I:=0 to High(FSegments) do
    if (B.Left<=FSegments[I].Bounds.Right) and (B.Right>=FSegments[I].Bounds.Left) and
       (B.Top<=FSegments[I].Bounds.Bottom) and (B.Bottom>=FSegments[I].Bounds.Top) and
       SegmentHitsRect(B,FSegments[I].A,FSegments[I].B) then Inc(Result);
end;
function TFreeLabelLayout.Place(const Key, Text: string; const Anchor, Preferred: TPoint;
  Width, Height: integer; Kind: TLabelKind; Outward: integer): TLabelPlacement;
const Dist: array[0..11] of integer=(0,6,12,18,24,36,48,64,80,96,128,160);
var Cache: TLabelCacheEntry; CacheIndex, I,J, DX,DY, N, Hits, BestHits:integer;
    X,Y,ShowStep,DesiredLeft,DesiredRight:integer; Candidate, Best,EdgeShift:TPoint; R,Clip:TRect; Score,BestScore:double;
    OldOffset,Target,Current,NextOffset:TPoint; Distance,Step:double; Moving:boolean; HasOld, InBounds,AvoidVanishing:boolean; Radius, EdgePos, MaxRadius: integer;
    Dt,Decayed,Delta,Temporary,EdgeBlend,ReferenceNumber:Double;
    ReferenceText:string;ReferenceIndex:Integer;
  procedure Follow(var Position, Velocity:Double; Destination:Double);
  begin
    Delta:=Position-Destination; Temporary:=(Velocity+12*Delta)*Dt;
    Position:=Destination+(Delta+Temporary)*Decayed;
    Velocity:=(Velocity-12*Temporary)*Decayed;
  end;
  procedure Evaluate(const C:TPoint; IsOld:boolean);
  var R2:TRect; HC,Margin:integer; SC:double; CandidateEdge:TPoint; ReuseTranslation:Boolean;
  begin
    R2:=Types.Rect(Anchor.X+C.X,Anchor.Y+C.Y,Anchor.X+C.X+Width,Anchor.Y+C.Y+Height);
    if (R2.Right<=FBounds.Left) or (R2.Bottom<=FBounds.Top) or
       (R2.Left>=FBounds.Right) or (R2.Top>=FBounds.Bottom) then Exit;
    // A point candidate must remain drawable after the existing edge effect.
    // Validate the same transformed rectangle that will be used for painting;
    // otherwise a small visible fragment can be pushed completely off-screen.
    if (Kind=lkPoint) and (Anchor.X>=FBounds.Left) and (Anchor.X<FBounds.Right) and
       (Anchor.Y>=FBounds.Top) and (Anchor.Y<FBounds.Bottom) then begin
      CandidateEdge:=EdgeAttraction(FBounds,R2);
      OffsetRect(R2,CandidateEdge.X,CandidateEdge.Y);
    end;
    // Text overlap is forbidden; geometry crossings remain a scored trade-off.
    R2:=AvailableRect(R2);
    if (R2.Right<=R2.Left) or (R2.Bottom<=R2.Top) then Exit;
    if IsOld then begin
      // The caller has verified an unchanged scene and an integer translation.
      // Interior annotations retain their relative geometry/background cost.
      // Still check the current rectangles, so newly visible labels cannot
      // overlap those already placed this frame. Edge effects use the solver.
      Margin:=Ceil(Max(24,Height*2)+Max(8,Height*0.75));
      ReuseTranslation:=FTranslationOnly and not FAnimationEnabled and
        (Cache.LastSeen+1=FFrame) and
        (Cache.DrawnPosition.X=Anchor.X+C.X) and (Cache.DrawnPosition.Y=Anchor.Y+C.Y) and
        (R2.Left>FBounds.Left+Margin) and (R2.Top>FBounds.Top+Margin) and
        (R2.Right<FBounds.Right-Margin) and (R2.Bottom<FBounds.Bottom-Margin);
      if (R2.Right-R2.Left=Width) and (R2.Bottom-R2.Top=Height) and
         (Cache.Text=Text) and (Cache.Width=Width) and (Cache.Height=Height) and
         (Cache.PreferredOffset.X=Preferred.X-Anchor.X) and
         (Cache.PreferredOffset.Y=Preferred.Y-Anchor.Y) and
         (not Assigned(FExtraCost) or ReuseTranslation) then
      begin
        if ReuseTranslation then Inc(FReusedPlacementCount);
        BestScore:=-1; Best:=C; BestHits:=0;
        Exit;
      end;
    end;
    HC:=CollisionCount(R2);
    if Kind=lkNamedLine then HC:=0;
    if Assigned(FExtraCost) then Inc(HC, FExtraCost(R2));
    SC:=(Width-(R2.Right-R2.Left))*0.01+HC*100000.0 + Hypot(C.X-(Preferred.X-Anchor.X),C.Y-(Preferred.Y-Anchor.Y));
    if AvoidVanishing then
      SC:=SC+1000000.0*((Width-(R2.Right-R2.Left))+(Height-(R2.Bottom-R2.Top)));
    if HasOld then SC:=SC+0.5*Hypot(C.X-OldOffset.X,C.Y-OldOffset.Y);
    if IsOld then SC:=SC-8;
    if SC<BestScore then begin BestScore:=SC; Best:=C; BestHits:=HC end;
  end;
begin
  Result.Visible:=False; Result.Collisions:=0;
  if (Width<=0) or (Height<=0) then Exit;
  if Kind in [lkVerticalTick,lkHorizontalTick] then
    for ReferenceIndex:=0 to High(FReferenceValues) do
      if (Text=FReferenceValues[ReferenceIndex].Value) and
         (Abs(Anchor.X-FReferenceValues[ReferenceIndex].Anchor.X)<=Max(Width,FReferenceValues[ReferenceIndex].Width)+24) and
         (Abs(Anchor.Y-FReferenceValues[ReferenceIndex].Anchor.Y)<=Max(Height,FReferenceValues[ReferenceIndex].Height)+12) then Exit;
  Cache:=FindCache(Key);
  FPlacingCache:=Cache; FPlacingKind:=Kind;
  AvoidVanishing:=False;
  if (Kind=lkPoint) and (Anchor.X>=FBounds.Left) and (Anchor.X<FBounds.Right) and
     (Anchor.Y>=FBounds.Top) and (Anchor.Y<FBounds.Bottom) then begin
    R:=Types.Rect(Preferred.X,Preferred.Y,Preferred.X+Width,Preferred.Y+Height);
    EdgeShift:=EdgeAttraction(FBounds,R); OffsetRect(R,EdgeShift.X,EdgeShift.Y);
    AvoidVanishing:=(R.Right<=FBounds.Left) or (R.Left>=FBounds.Right) or
      (R.Bottom<=FBounds.Top) or (R.Top>=FBounds.Bottom);
  end;
  // Identity comes from Key, not its changing formatted value or font metrics.
  HasOld:=Assigned(Cache) and
    (Cache.Kind=Kind) and (Cache.Outward=Outward);
  if HasOld then OldOffset:=Cache.Target else OldOffset:=Point(0,0);
  BestScore:=1e100; BestHits:=High(integer); Best:=Point(0,0);
  if HasOld then Evaluate(OldOffset,True);
  // A collision-free preferred position has the minimum possible cost. Avoid
  // scanning dozens of candidates and segments for the common case.
  if BestScore > 0 then
    Evaluate(Point(Preferred.X-Anchor.X,Preferred.Y-Anchor.Y),False);
  // Do not relocate a wholly off-screen annotation back into the viewport.
  if (BestScore=1e100) and ((Anchor.X<FBounds.Left) or (Anchor.X>=FBounds.Right) or
    (Anchor.Y<FBounds.Top) or (Anchor.Y>=FBounds.Bottom)) then begin
    if HasOld and (Kind=lkNamedLine) then begin Best:=Cache.Target;BestScore:=0 end
    else Exit;
  end;
  if BestScore > 0 then begin
  if Kind=lkPoint then
  begin
    // Candidate directions surround the point and remain close to it.
    for I:=0 to High(Dist) do begin
      N:=Dist[I];
      Evaluate(Point(6+N, -Height-6-N),False);
      Evaluate(Point(6+N, 6+N),False);
      Evaluate(Point(-Width-6-N, -Height-6-N),False);
      Evaluate(Point(-Width-6-N, 6+N),False);
      Evaluate(Point(-Width div 2, -Height-8-N),False);
      Evaluate(Point(-Width div 2, 8+N),False);
    end;
  end;
  for I:=0 to High(Dist) do begin
    N:=Dist[I];
    if (Kind in [lkVerticalTick,lkHorizontalTick]) and (N>24) then Break;
    for J:=0 to 7 do begin
      DX:=0; DY:=0;
      case J of
        0: begin DX:=0; DY:=0 end;
        1: DX:=N; 2: DX:=-N; 3: DY:=N; 4: DY:=-N;
        5: begin DX:=N; DY:=N end; 6: begin DX:=-N; DY:=N end;
        7: begin DX:=N; DY:=-N end;
      end;
      // A tick can move only along its own line, preserving its value/line mapping.
      if (Kind=lkVerticalTick) and (DX<>0) then Continue;
      if (Kind=lkHorizontalTick) and (DY<>0) then Continue;
      // Tick positions may move along the grid line, preferably away from its
      // endpoint and the hull; this avoids sliding a value into the drawing.
      if (Outward<>0) and (Kind=lkVerticalTick) and (DY*Outward<0) then Continue;
      if (Outward<>0) and (Kind=lkHorizontalTick) and (DX*Outward<0) then Continue;
      Candidate:=Point(Preferred.X-Anchor.X+DX,Preferred.Y-Anchor.Y+DY);
      Evaluate(Candidate,False);
    end;
  end;
  end; // candidate search needed
  if (BestScore=1e100) and (Kind=lkPoint) then
  begin
    // Local candidates are exhausted. Search progressively wider rings before
    // omitting a coordinate; the caller draws a leader for distant placements.
    MaxRadius:=Max(FBounds.Right-FBounds.Left,FBounds.Bottom-FBounds.Top)+Max(Width,Height);
    Radius:=192;
    while (Radius<=MaxRadius) and (BestScore=1e100) do begin
      EdgePos:=-Radius;
      while EdgePos<=Radius do begin
        Evaluate(Point(Preferred.X-Anchor.X+EdgePos,Preferred.Y-Anchor.Y-Radius),False);
        Evaluate(Point(Preferred.X-Anchor.X+EdgePos,Preferred.Y-Anchor.Y+Radius),False);
        Evaluate(Point(Preferred.X-Anchor.X-Radius,Preferred.Y-Anchor.Y+EdgePos),False);
        Evaluate(Point(Preferred.X-Anchor.X+Radius,Preferred.Y-Anchor.Y+EdgePos),False);
        Inc(EdgePos,32);
      end;
      Inc(Radius,32);
    end;
  end;
  if (BestScore=1e100) and HasOld then begin
    // Keep an existing readable label while the neighbourhood is congested.
    // Never discard it merely because the next destination is unavailable.
    Evaluate(Cache.Offset,True);
  end;
  if (BestScore=1e100) and HasOld and (Kind=lkNamedLine) then begin
    Best:=Cache.Target;BestScore:=0;
  end;
  if BestScore=1e100 then begin
    if Cache<>nil then Cache.ShownRight:=Cache.ShownLeft;
    Exit;
  end;
  // All visible labels must avoid already placed labels and protected points.
  // If no local candidate is clear, omit it instead of drawing over another.
  X:=Anchor.X+Best.X; Y:=Anchor.Y+Best.Y;
  R:=Types.Rect(X,Y,X+Width,Y+Height);
  if (Kind=lkPoint) and (Anchor.X>=FBounds.Left) and (Anchor.X<FBounds.Right) and
     (Anchor.Y>=FBounds.Top) and (Anchor.Y<FBounds.Bottom) then begin
    EdgeShift:=EdgeAttraction(FBounds,R);
    OffsetRect(R,EdgeShift.X,EdgeShift.Y);
  end;
  Clip:=AvailableRect(R);
  if ((Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top)) and (Kind<>lkNamedLine) then Exit;
  if Kind in [lkVerticalTick,lkHorizontalTick] then begin
    if Assigned(FExtraCost) and (FExtraCost(R) >= 6) and not HasOld then Exit;
  end;
  Target:=Best; Current:=Best;
  if HasOld and FAnimationEnabled then begin
    if not Cache.MotionReady then begin
      Cache.MotionX:=Cache.Offset.X; Cache.MotionY:=Cache.Offset.Y;
      Cache.MotionReady:=True;
    end;
    Dt:=Max(0.001,Min(0.05,FFrameStep/180.0)); Decayed:=Exp(-12*Dt);
    Follow(Cache.MotionX,Cache.VelocityX,Target.X);
    Follow(Cache.MotionY,Cache.VelocityY,Target.Y);
    Current:=Point(Round(Cache.MotionX),Round(Cache.MotionY));
    FAnimating:=FAnimating or (Abs(Cache.MotionX-Target.X)>0.25) or
      (Abs(Cache.MotionY-Target.Y)>0.25) or (Abs(Cache.VelocityX)>0.5) or (Abs(Cache.VelocityY)>0.5);
  end;
  X:=Anchor.X+Current.X; Y:=Anchor.Y+Current.Y; R:=Types.Rect(X,Y,X+Width,Y+Height);
  EdgeShift:=EdgeAttraction(FBounds,R);
  if HasOld and FAnimationEnabled then begin
    EdgeBlend:=1-Exp(-Max(0.001,Min(0.05,FFrameStep/180.0))/0.08);
    Cache.EdgeX:=Cache.EdgeX+(EdgeShift.X-Cache.EdgeX)*EdgeBlend;
    Cache.EdgeY:=Cache.EdgeY+(EdgeShift.Y-Cache.EdgeY)*EdgeBlend;
    FAnimating:=FAnimating or (Abs(Cache.EdgeX-EdgeShift.X)>0.25) or (Abs(Cache.EdgeY-EdgeShift.Y)>0.25);
    EdgeShift:=Point(Round(Cache.EdgeX),Round(Cache.EdgeY));
  end;
  Inc(X,EdgeShift.X); Inc(Y,EdgeShift.Y);
  R:=Types.Rect(X,Y,X+Width,Y+Height);
  Clip:=AvailableRect(R);
  if (Kind=lkPoint) and ((Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top)) and
     (R.Left>FBounds.Left) and (R.Right<FBounds.Right) and
     (R.Top>FBounds.Top) and (R.Bottom<FBounds.Bottom) then begin
    // Internal obstacles may require a complete numeric placement. Never
    // reset the spring or discard edge attraction while crossing a viewport edge.
    Current:=Target; X:=Anchor.X+Current.X; Y:=Anchor.Y+Current.Y;
    R:=Types.Rect(X,Y,X+Width,Y+Height); Clip:=AvailableRect(R);
    if Cache<>nil then begin
      Cache.MotionX:=Current.X; Cache.MotionY:=Current.Y;
      Cache.VelocityX:=0; Cache.VelocityY:=0;
    end;
  end;
  // While a point remains inside the viewport, an animated intermediate
  // position can briefly move fully outside even though its destination is
  // valid. Keep the previous drawable position until the spring returns.
  if (Kind=lkPoint) and (Anchor.X>=FBounds.Left) and (Anchor.X<FBounds.Right) and
     (Anchor.Y>=FBounds.Top) and (Anchor.Y<FBounds.Bottom) and
     ((Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top)) then begin
    if HasOld and (Cache.LastSeen+1=FFrame) then begin
      X:=Cache.DrawnPosition.X; Y:=Cache.DrawnPosition.Y;
      R:=Types.Rect(X,Y,X+Width,Y+Height); Clip:=AvailableRect(R);
    end;
    if (Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top) then begin
      X:=Anchor.X+Target.X; Y:=Anchor.Y+Target.Y;
      R:=Types.Rect(X,Y,X+Width,Y+Height); EdgeShift:=EdgeAttraction(FBounds,R);
      Inc(X,EdgeShift.X); Inc(Y,EdgeShift.Y);
      R:=Types.Rect(X,Y,X+Width,Y+Height); Clip:=AvailableRect(R);
      if (Clip.Right>Clip.Left) and (Clip.Bottom>Clip.Top) then begin
        Current:=Target;
        if Cache<>nil then begin
          Cache.MotionX:=Current.X; Cache.MotionY:=Current.Y;
          Cache.VelocityX:=0; Cache.VelocityY:=0;
          Cache.EdgeX:=EdgeShift.X; Cache.EdgeY:=EdgeShift.Y;
        end;
      end;
    end;
  end;
  if (Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top) then begin
    if Cache<>nil then begin
      Cache.ShownRight:=Cache.ShownLeft;
      // An occluded label still has a moving anchor, spring and edge attraction.
      Cache.Offset:=Current;Cache.Target:=Target;Cache.LastSeen:=FFrame;
      Cache.DrawnPosition:=Point(X,Y);Cache.DrawnRect:=Types.Rect(X,Y,X,Y);
    end;
    Exit;
  end;
  DesiredLeft:=Clip.Left-X; DesiredRight:=Clip.Right-X;
  ShowStep:=Max(1,Ceil(Width*FFrameStep/43.2));
  if (Cache=nil) and (Key<>'') then begin
    Cache:=TLabelCacheEntry.Create; CacheIndex:=FCache.AddObject(Key,Cache);
    Cache.ShownLeft:=DesiredLeft; Cache.ShownRight:=DesiredLeft;
  end;
  if (Cache<>nil) and (not FAnimationEnabled) then begin
    Cache.ShownLeft:=DesiredLeft; Cache.ShownRight:=DesiredRight;
  end;
  if (Cache<>nil) and FAnimationEnabled and
     (Cache.ShownRight>Cache.ShownLeft) and
     ((DesiredLeft>=Cache.ShownRight) or (DesiredRight<=Cache.ShownLeft)) then begin
    Cache.ShownRight:=Cache.ShownLeft;
  end;
  if Cache<>nil then begin
    Cache.ShownLeft:=Min(DesiredRight,Max(DesiredLeft,Cache.ShownLeft-ShowStep));
    Cache.ShownRight:=Min(DesiredRight,Max(Cache.ShownLeft,Cache.ShownRight)+ShowStep);
    FAnimating:=FAnimating or (Cache.ShownLeft<>DesiredLeft) or (Cache.ShownRight<>DesiredRight);
    Clip.Left:=X+Cache.ShownLeft; Clip.Right:=X+Cache.ShownRight;
  end;
  R:=Clip;
  Result.Position:=Point(X,Y); Result.Rect:=R; Result.Collisions:=BestHits; Result.Visible:=True;
  Reserve(Types.Rect(X+DesiredLeft,Clip.Top,X+DesiredRight,Clip.Bottom));
  if Kind=lkNamedLine then begin
    ReferenceIndex:=Length(Text);
    while (ReferenceIndex>0) and (Text[ReferenceIndex]<>' ') do Dec(ReferenceIndex);
    ReferenceText:=Copy(Text,ReferenceIndex+1,MaxInt);
    if (ReferenceIndex>0) and TryStrToFloat(ReferenceText,ReferenceNumber) then begin
      ReferenceIndex:=Length(FReferenceValues);SetLength(FReferenceValues,ReferenceIndex+1);
      FReferenceValues[ReferenceIndex].Value:=ReferenceText;
      FReferenceValues[ReferenceIndex].Anchor:=Anchor;
      FReferenceValues[ReferenceIndex].Width:=Width;FReferenceValues[ReferenceIndex].Height:=Height;
    end;
  end;
  if Key<>'' then begin
    if Cache=nil then begin
      Cache:=TLabelCacheEntry.Create; CacheIndex:=FCache.AddObject(Key,Cache);
      Cache.ShownLeft:=R.Left-X; Cache.ShownRight:=R.Right-X;
    end;
    if not Cache.MotionReady then begin
      Cache.MotionX:=Current.X; Cache.MotionY:=Current.Y;
      Cache.EdgeX:=EdgeShift.X; Cache.EdgeY:=EdgeShift.Y; Cache.MotionReady:=True;
    end;
    Cache.LastSeen:=FFrame; Cache.DrawnRect:=R; Cache.DrawnPosition:=Point(X,Y);
    Cache.Text:=Text; Cache.Width:=Width; Cache.Height:=Height; Cache.Offset:=Current; Cache.Target:=Target;
    Cache.Kind:=Kind; Cache.Outward:=Outward;
    Cache.PreferredOffset:=Point(Preferred.X-Anchor.X,Preferred.Y-Anchor.Y);
  end;
end;
end.
