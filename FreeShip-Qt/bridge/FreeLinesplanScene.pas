{#############################################################################################}
{    This code is distributed as part of the FREE!ship project. FREE!ship is an               }
{    open source surface-modelling program based on subdivision surfaces and intended for     }
{    designing ships.                                                                         }

{    Copyright © 2005, by Martijn van Engeland                                                }
{    e-mail                  : Info@FREEship.org                                              }
{    FREE!ship project page  : https://sourceforge.net/projects/freeship                      }
{    FREE!ship homepage      : www.FREEship.org                                               }

{    This program is free software; you can redistribute it and/or modify it under            }
{    the terms of the GNU General Public License as published by the                          }
{    Free Software Foundation; either version 2 of the License, or (at your option)           }
{    any later version.                                                                       }

{    This program is distributed in the hope that it will be useful, but WITHOUT ANY          }
{    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A          }
{    PARTICULAR PURPOSE. See the GNU General Public License for more details.                 }

{    You should have received a copy of the GNU General Public License along with             }
{    this program; if not, write to the Free Software Foundation, Inc.,                       }
{    59 Temple Place, Suite 330, Boston, MA 02111-1307 USA                                    }

{#############################################################################################}

unit FreeLinesplanScene;
{$mode delphi}{$H+}
// Window-free scene generation, extracted from the original lines-plan algorithms.
// Graphics is used only for a 1x1 font-metrics surface, never hull rasterization.
interface
uses Classes,SysUtils,Types,Graphics,Math,Windows,FreeTypes,FreeGeometry,
 FreeShipUnit,FasterList,FreeStringsUnit,FreeStringUtils,PilotCapture,StableEdgeLoops;
type
 TLinesplanView=(lvProfile,lvAftBody,lvFrontBody,lvPlan);
 TLinesplanViews=set of TLinesplanView;
 TSceneProjection=class
 private
  Metrics:Graphics.TBitmap;
  FMin3D,FMax3D,FMidPoint,FCameraLocation:T3DCoordinate;
  FDistance,FScale,FMargin,FAngle,FElevation,FCosAngle,FSinAngle,FCosElevation,FSinElevation:TFloatType;
  FScreenCenter,FPan:TPoint;
  FZoom:TFloatType;
  function RotatedPoint(P:T3DCoordinate):T3DCoordinate;
  function GetFont:TFont;
 public
  ClientWidth,ClientHeight:Integer;
  PenColor,Color:TColor;
  constructor Create;
  destructor Destroy;override;
  procedure Initialize(Min,Max:T3DCoordinate);
  function Project(P:T3DCoordinate):TPoint;
  function TextWidth(const Text:string):Integer;
  function TextHeight(const Text:string):Integer;
  property Font:TFont read GetFont;
  property Scale:TFloatType read FScale;
  property Zoom:TFloatType read FZoom;
 end;
 TLinesplanScene=class
 private
  FFreeShip:TFreeShip;
  FMin3D,FMax3D,FProfileOrigin,FAftOrigin,FFrontOrigin,FPlanOrigin:T3DCoordinate;
  FModelHeight,FModelBeam,FModelLength,FDiagonalWidth:TFloatType;
  Layout:TSceneProjection;
  procedure CalculateExtents(var Min,Max:T3DCoordinate);
  procedure EmitScene;
 public
  constructor Create(Ship:TFreeShip);
  destructor Destroy;override;
  procedure Generate(const Filename:string);
  procedure ExportDXF(const Filename:string);
  property FreeShip:TFreeShip read FFreeShip;
 end;
implementation
const SpacePercentage=0.20;Textspace=0.05;
function CalculateSpace(Percentage,Min,Max:TFloatType):TFloatType;
begin Result:=Percentage*(Max-Min) end;
constructor TSceneProjection.Create;
begin
 inherited Create;ClientWidth:=1280;ClientHeight:=800;Color:=clWhite;
 FMargin:=1;FAngle:=90;FElevation:=90;FZoom:=1;FPan:=Types.Point(0,0);
 Metrics:=Graphics.TBitmap.Create;Metrics.SetSize(1,1);
 Metrics.Canvas.Font.Name:='Arial';Metrics.Canvas.Font.Size:=7;
end;
destructor TSceneProjection.Destroy;
begin Metrics.Free;inherited Destroy end;
function TSceneProjection.GetFont:TFont;
begin Result:=Metrics.Canvas.Font end;
function TSceneProjection.TextWidth(const Text:string):Integer;
begin Result:=Metrics.Canvas.TextWidth(Text) end;
function TSceneProjection.TextHeight(const Text:string):Integer;
begin Result:=Metrics.Canvas.TextHeight(Text) end;
constructor TLinesplanScene.Create(Ship:TFreeShip);
begin inherited Create;FFreeShip:=Ship;Layout:=TSceneProjection.Create end;
destructor TLinesplanScene.Destroy;
begin Layout.Free;inherited Destroy end;
procedure TLinesplanScene.Generate(const Filename:string);
var Min,Max:T3DCoordinate;
begin
 Min:=ZERO;Max:=ZERO;CalculateExtents(Min,Max);Layout.Initialize(Min,Max);
 PilotBegin(Filename,Min.X,Min.Y,Max.X,Max.Y);
 try EmitScene;PilotEnd except PilotCancel;raise end;
end;

procedure TSceneProjection.Initialize(Min, Max: T3DCoordinate);
// This procedure initializes the viewports and sets the scale in such a way
// that the model completely fills the viewport
var
  P: array[1..8] of T3DCoordinate;
  Projected: T2DCoordinate;
  Min2D, Max2D: T2DCoordinate;
  P3D: T3DCoordinate;
  I: integer;
  VertCorr: integer;
  HorCorr: integer;
  Width, Height: TFloatType;
  Tmp: TFloatType;
  XScale: TFloatType;
  YScale: TFloatType;
  Pt1, Pt2: TPoint;
  Diff: T3DCoordinate;

  procedure MinMax(P: T2DCoordinate);
  begin
    if P.X < Min2D.X then
      Min2D.X := P.X;
    if P.Y < Min2D.Y then
      Min2D.Y := P.Y;
    if P.X > Max2D.X then
      Max2D.X := P.X;
    if P.Y > Max2D.Y then
      Max2D.Y := P.Y;
  end;

begin
  // Add margin
  Diff := ScalePoint(0.01 * FMargin, Subtract(Max, Min));
  FMin3D := Subtract(Min, Diff);
  Diff := ScalePoint(-1.0, Diff);
  FMax3D := Subtract(Max, diff);
  // Calculate the midpoint of the boundingbox, which is used as the center of the model for rotating the model
  FMidPoint := MidPoint(FMin3D, FMax3D);
  // Calculate the distance of the camera to the center of the model, following from the field of view from the camera
  Tmp := Sqrt(Sqr(FMax3D.Y - FMin3D.Y) + Sqr(FMax3D.Z - FMin3D.Z));
  if Tmp = 0 then
    Tmp := 1e-2;
  FDistance:=1e8;
  FCameraLocation.X := FMax3D.X + FDistance;
  FCameraLocation.Y := FMidPoint.Y;
  FCameraLocation.Z := FMidPoint.Z;

  FCosAngle := Cos(DegToRad(FAngle));
  FSinAngle := sin(DegToRad(FAngle));
  FCosElevation := Cos(DegToRad(FElevation));
  FSinElevation := sin(DegToRad(FElevation));

  // now project all 8 cornerpoints of the bounding box to 2D
  // in order to determin the min. and max. 2D coordinates of the 2D viewport.
  P[1].X := FMin3D.X;
  P[1].Y := FMin3D.Y;
  P[1].Z := FMin3D.Z;
  P[2].X := FMax3D.X;
  P[2].Y := FMin3D.Y;
  P[2].Z := FMin3D.Z;
  P[3].X := FMax3D.X;
  P[3].Y := FMax3D.Y;
  P[3].Z := FMin3D.Z;
  P[4].X := FMin3D.X;
  P[4].Y := FMax3D.Y;
  P[4].Z := FMin3D.Z;
  P[5].X := FMin3D.X;
  P[5].Y := FMin3D.Y;
  P[5].Z := FMax3D.Z;
  P[6].X := FMax3D.X;
  P[6].Y := FMin3D.Y;
  P[6].Z := FMax3D.Z;
  P[7].X := FMax3D.X;
  P[7].Y := FMax3D.Y;
  P[7].Z := FMax3D.Z;
  P[8].X := FMin3D.X;
  P[8].Y := FMax3D.Y;
  P[8].Z := FMax3D.Z;
  for I := 1 to 8 do
  begin
    P3D := RotatedPoint(P[i]);
    // apply perspective projection
    Tmp := FCameralocation.X - P3D.X;
    // apply perspective correction
    Projected.X := FCameralocation.X * P3D.Y / Tmp;
    Projected.Y := FCameralocation.X * P3D.Z / Tmp;
    if I = 1 then
    begin
      Min2D := Projected;
      Max2D := Projected;
    end;
    MinMax(Projected);
  end;
  Width := Max2D.X - Min2D.X;
  Height := Max2D.Y - Min2D.Y;
  if (abs(Width) > 1e-7) and (abs(Height) > 1e-7) then
  begin
    XScale := ClientWidth / Width;
    YScale := ClientHeight / Height;
    if XScale < YScale then
      FScale := XScale
    else
      FScale := YScale;
  end
  else
    FScale := 1;
  // Decrease Scale with 1% to keep the model free from the edges
  FScale := 0.99 * FScale;
  FScreencenter.X := ClientWidth div 2;
  FScreencenter.Y := ClientHeight div 2;
  // Calculate correction (pan vector) to make sure
  // that the ship appears in the middle of the viewport
  Pt1.X := FScreencenter.X + Round(FScale * Min2D.X);
  Pt1.Y := FScreencenter.Y - Round(FScale * Min2D.Y);
  Pt2.X := FScreencenter.X + Round(FScale * Max2D.X);
  Pt2.Y := FScreencenter.Y - Round(FScale * Max2D.Y);
  HorCorr := (Pt1.X + Pt2.X) div 2 - ClientWidth div 2;
  VertCorr := (Pt1.Y + Pt2.Y) div 2 - ClientHeight div 2;
  FScreencenter.X := FScreencenter.X - HorCorr;
  FScreencenter.Y := FScreencenter.Y - vertCorr;
  // Remember the min/max values
  FMin3D := Min;
  FMax3D := Max;
end;{TSceneProjection.Initialize}

function TSceneProjection.Project(P: T3DCoordinate): TPoint;
var
  P3D: T3DCoordinate;
  P2D: T2DCoordinate;
  Dist: TFloatType;
  dX,dY:double;
begin
  P3D := RotatedPoint(P);
  Dist := FCameralocation.X - P3D.X;
  // apply perspective correction
  P2D.X := FCameralocation.X * P3D.Y / Dist;
  P2D.Y := FCameralocation.X * P3D.Z / Dist;
  dX := FPan.X + FScreencenter.X + (FZoom * FScale * P2D.X);
  dY := FPan.Y + FScreencenter.Y - (FZoom * FScale * P2D.Y);
  if dX > MAXINT then dX := MAXINT;
  if dX < -MAXINT-1 then dX := -MAXINT-1;
  if dY > MAXINT then dY := MAXINT;
  if dY < -MAXINT-1 then dY := -MAXINT-1;
  Result.X := Round(dX);
  Result.Y := Round(dY);
end;{TSceneProjection.Project}


// Projects 2d screen coordinate as a ray/line that goes through camera and the 3d "screen" coordinate.
function TSceneProjection.RotatedPoint(P: T3DCoordinate): T3DCoordinate;
begin
  // This function takes a point from worldspace and rotates it around the midpoint
  // of the scene as specified by the viewing-parameters (angle/elevation)
  // translate midpoint back to origin
  P.X := P.X - FMidpoint.X;
  P.Y := P.Y - FMidpoint.Y;
  P.Z := P.Z - FMidpoint.Z;
  // Rotate around the origin
  Result.x := (P.x * FCosAngle - P.y * FSinAngle) * FCosElevation + P.z * FSinElevation;
  Result.y := P.x * FSinAngle + P.y * FCosAngle;
  Result.z := -(P.x * FCosAngle - P.y * FSinAngle) * FSinElevation + P.z * FCosElevation;
  // Translate origin back to midpoint
  Result.X := Result.X + FMidPoint.X;
  Result.Y := Result.Y + FMidPoint.Y;
  Result.Z := Result.Z + FMidPoint.Z;
end;{TSceneProjection.RotatedPoint}

procedure TLinesplanScene.CalculateExtents(
  var Min, Max: T3DCoordinate);
var
  Space, Tmp: TFloatType;
  Min3D, Max3D: T3DCoordinate;
  P, Diff: T3DCoordinate;
  I, J, K: integer;
  Diagonal: TFreeIntersection;
  Spline: TFreeSpline;
  Plane: T3DPlane;
  First: boolean;
  Layer: TFreeSubdivisionLayer;
begin
  if Freeship <> nil then
  begin
    // No layer faces may be included. Intersections have independent flags;
    // retain finite full-model extents until a visible face supplies them.
    Min3D := FreeShip.Surface.Min;
    Max3D := FreeShip.Surface.Max;
    First := True;
    for I := 1 to Freeship.NumberOfLayers do
    begin
      Layer := Freeship.Layer[I - 1];
      if Layer.ShowInLinesplan then
      begin
        for J := 1 to Layer.Count do
        begin
          if First then
          begin
            Min3D := Layer.Items[J - 1].Min;
            Max3D := Min3D;
            First := False;
          end;
          MinMax(Layer.Items[J - 1].Min, Min3D, Max3D);
          MinMax(Layer.Items[J - 1].Max, Min3D, Max3D);
        end;
      end;
    end;
    for I := 1 to Freeship.NumberofStations do
      Freeship.Station[I - 1].Extents(Min3D, Max3D);
    for I := 1 to Freeship.NumberofButtocks do
      Freeship.Buttock[I - 1].Extents(Min3D, Max3D);
    for I := 1 to Freeship.NumberofWaterlines do
      Freeship.Waterline[I - 1].Extents(Min3D, Max3D);
    for I := 1 to Freeship.NumberofDiagonals do
      Freeship.Diagonal[I - 1].Extents(Min3D, Max3D);

    FMin3D := Min3D;
    FMax3D := Max3D;
    FModelHeight := Max3D.Z - Min3D.Z;
    FModelBeam := 2 * Max3D.Y;
    FModelLength := Max3D.X - Min3D.X;
    Space := SpacePercentage * FModelHeight;
    if 2 * FModelBeam + Space > FModelLength then
      FModelLength := 2 * FModelBeam + Space;
    FDiagonalWidth := 0;
    for I := 1 to Freeship.NumberofDiagonals do
    begin
      Diagonal := Freeship.Diagonal[I - 1];
      Plane.a := 0.0;
      Plane.b := 1 / Sqrt(2);
      Plane.c := -1 / sqrt(2);
      Plane.d := -Diagonal.Plane.d;
      if not Diagonal.Built then
        Diagonal.Rebuild;
      for J := 1 to Diagonal.Count do
      begin
        Spline := Diagonal.Items[J - 1];
        for k := 1 to Spline.NumberOfPoints do
        begin
          P := Spline.Point[K - 1];
          Tmp := abs(Plane.A * P.x + Plane.B * P.y + Plane.C * P.z + Plane.D);
          if Tmp > FDiagonalWidth then
            FDiagonalWidth := Tmp;
        end;
      end;
    end;

    Min.X := Min3D.X - 0.5 * space;
    // if no diagonals present in model, the mirror planview
    if Freeship.NumberofDiagonals > 0 then
      Min.Y := Min3D.Z - 2 * Space - FModelHeight - 0.5 * FModelBeam - 0.5 * space - space - FDiagonalWidth
    else
    if (Freeship.NumberofDiagonals = 0) and (False) then
      Min.Y := Min3D.Z - 2 * Space - FModelHeight - FModelBeam - 0.5 * space
    else
      Min.Y := Min3D.Z - 2 * Space - FModelHeight - 0.5 * FModelBeam - 0.5 * space;

    Min.Z := 0.0;
    Max.X := Min.X + FModelLength + 0.5 * space;
    Max.Y := -Min3D.Z + FModelHeight + 0.5 * space;
    Max.Z := 0.0;
    Tmp := (FModelLength - 2 * FModelbeam - 3 * space) / 3;

    // Attachpoint for profileview
    FProfileOrigin.X := 0.0;
    FProfileOrigin.Y := -Min3D.Z;
    FProfileOrigin.Z := 0.0;
    // Attachpoint for aft view of bodyplan
    FAftOrigin.X := Min.X + Tmp + 0.5 * FModelbeam;
    FAftOrigin.Y := FProfileOrigin.Y - Space - FModelHeight;
    FAftOrigin.Z := 0.0;
    // Attachpoint for front view of bodyplan
    FFrontOrigin := FAftOrigin;
    FFrontOrigin.X := Max.X - Tmp - 0.5 * FModelbeam;
    // Attachpoint for plan view
    FPlanOrigin.X := FProfileOrigin.X;
    FPlanOrigin.Y := FAftOrigin.Y - space - 0.5 * FModelBeam + Min3D.Z;
    FPlanOrigin.Z := 0.0;

    // Blowup the boundary box by 3%
    Diff := ScalePoint(0.03, Subtract(Max3D, Min3D));
    Min := Subtract(Min, Diff);
    Diff := ScalePoint(-1.0, Diff);
    Max := Subtract(Max, diff);

  end
  else
  begin
    Min.X := -1;
    Min.Y := Min.X;
    Min.Z := Min.X;
    Max.X := -Min.X;
    Max.Y := Max.X;
    Max.Z := Max.X;
  end;
end;{TFreeLinesplanFrame.ViewportRequestExtents}

procedure TLinesplanScene.EmitScene;
type
  TriangleData = record
    P1, P2, P3: T3DCoordinate;
    Center: T3DCoordinate;
    Color: TColor;
    Symmetric: boolean;
  end;

  TriangleArray = record
    Capacity: integer;
    Count: integer;
    Triangles: array of TriangleData;
  end;
var
  I, J, N, K, L: integer;
  Edge: TFreeSubdivisionEdge;
  Layer: TFreeSubdivisionLayer;
  Face: TFreeSubdivisionControlface;
  Child: TFreeSubdivisionFace;
  MidshipLocation: TFloatType;
  Tmp, Space: TFloatType;
  Min, Max: TFloatType;
  //ResFactor: TFloatType;
  //ScaleFactor: TFloatType;
  WlPlane: T3DPlane;
  MainPlane: T3DPlane;
  Plane: T3DPlane;
  Below: TriangleArray;
  Above: TriangleArray;
  Done: boolean;
  Diagonal: TFreeIntersection;
  Spline: TFreeSpline;
  P, P1, P2, Pkn: T3DCoordinate;
  Pts: array of TPoint;
  Pts3D: array of T3DCoordinate;
  //Pt: TPoint;
  Steps: integer;
  PenwidthFactor: integer;
  Str: string;
  SubmColor:TColor;
  Pn,Pn1,Pn2:Integer;
  PendingLabels:array of record X,Y,Outward,Index,FontSize:Integer;Text,FontName:string;Vertical:Boolean;Color:TColor end;


  procedure DrawLineLabel(X,Y:Integer;const Text:string;Vertical:Boolean;Outward:Integer);
  var Origin:TPoint;
  begin
    Origin:=Layout.Project(ZERO);
    PilotLabel((X-Origin.X)/(Layout.Scale*Layout.Zoom),
      (Origin.Y-Y)/(Layout.Scale*Layout.Zoom),Layout.Font.Color,Layout.TextHeight('Mg'),Text);
  end;

  procedure PlaceLineLabel(X,Y:Integer;const Text:string;Vertical:Boolean;Outward:Integer);
  var N:Integer;
  begin

    N:=Length(PendingLabels);SetLength(PendingLabels,N+1);
    PendingLabels[N].X:=X;PendingLabels[N].Y:=Y;PendingLabels[N].Text:=Text;
    PendingLabels[N].Vertical:=Vertical;PendingLabels[N].Outward:=Outward;
    PendingLabels[N].Index:=N+1;PendingLabels[N].FontName:=Layout.Font.Name;
    PendingLabels[N].FontSize:=Layout.Font.Size;PendingLabels[N].Color:=Layout.Font.Color;
  end;

  procedure DrawPendingLabels;
  var Pass,N:Integer;Numeric:Boolean;
  begin
    for Pass:=0 to 1 do for N:=0 to High(PendingLabels) do begin
      Numeric:=(PendingLabels[N].Text<>'') and (PendingLabels[N].Text[1] in ['0'..'9','-']);
      if (Pass=1)<>Numeric then Continue;
      Layout.Font.Name:=PendingLabels[N].FontName;Layout.Font.Size:=PendingLabels[N].FontSize;
      Layout.Font.Color:=PendingLabels[N].Color;
      DrawLineLabel(PendingLabels[N].X,PendingLabels[N].Y,PendingLabels[N].Text,PendingLabels[N].Vertical,PendingLabels[N].Outward);
    end;
  end;

  procedure DrawLineAtt(Attachpoint, P1, P2: T3DCoordinate; Text: string; CenterText: boolean);
  var
    Proj1: T3DCoordinate;
    Proj2: T3DCoordinate;
    Pts: array[0..1] of TPoint;
    W: integer;
  begin
    Proj1.X := Attachpoint.X + P1.X;
    Proj1.Y := Attachpoint.Y + P1.Y;
    Proj1.Z := 0.0;
    Pts[0] := Layout.Project(Proj1);
    Proj2.X := Attachpoint.X + P2.X;
    Proj2.Y := Attachpoint.Y + P2.Y;
    Proj2.Z := 0.0;
    Pts[1] := Layout.Project(Proj2);
    PilotLine(Proj1.X,Proj1.Y,Proj2.X,Proj2.Y,Layout.PenColor);

    if Text <> '' then
    begin
      if (CenterText) and (Pts[0].X = Pts[1].X) then
      begin
        W := Layout.TextWidth(Text) div 2;
        PlaceLineLabel(Pts[0].X - W, Pts[0].Y, Text, Pts[0].X = Pts[1].X, Sign(Pts[0].Y - Pts[1].Y));
        PlaceLineLabel(Pts[1].X - W, Pts[1].Y, Text, Pts[0].X = Pts[1].X, Sign(Pts[1].Y - Pts[0].Y));
      end
      else
      begin
        W := 0;
        PlaceLineLabel(Pts[1].X - W, Pts[0].Y, Text, Pts[0].X = Pts[1].X, Sign(Pts[1].X - Pts[0].X));
        if (Pts[0].Y = Pts[1].Y) and (Pts[1].X <> Pts[0].X) then
          W := Layout.TextWidth(Text)
        else
          W := 0;
        PlaceLineLabel(Pts[0].X - W, Pts[1].Y, Text, Pts[0].X = Pts[1].X, Sign(Pts[0].X - Pts[1].X));
      end;
    end;
  end;{DrawLineAtt}

  procedure DrawDiagonalLine(Origin: T3DCoordinate; P1, P2: T3DCoordinate);
  var
    Pt: TPoint;
    P: T3DCoordinate;
  begin
    P.X := Origin.X + P1.Y;
    P.Y := Origin.Y + P1.Z;
    P.Z := 0.0;
    PilotPoint(P.X,P.Y);
    Pt := Layout.Project(P);

    P.X := Origin.X + P2.Y;
    P.Y := Origin.Y + P2.Z;
    PilotPoint(P.X,P.Y);
    Pt := Layout.Project(P);
    PilotFlush(2,Layout.PenColor);

    P.X := Origin.X - P1.Y;
    P.Y := Origin.Y + P1.Z;
    PilotPoint(P.X,P.Y);
    Pt := Layout.Project(P);

    P.X := Origin.X - P2.Y;
    P.Y := Origin.Y + P2.Z;
    PilotPoint(P.X,P.Y);
    Pt := Layout.Project(P);
    PilotFlush(2,Layout.PenColor);

  end;{DrawDiagonalLine}

  procedure DrawLine(P1, P2: T3DCoordinate);
  var
    Proj1: T3DCoordinate;
    Proj2: T3DCoordinate;
    Pts: array[0..1] of TPoint;
  begin
    // Draw in profileview
    Proj1.X := FProfileOrigin.X + P1.X;
    Proj1.Y := FProfileOrigin.Y + P1.Z;
    Proj1.Z := 0.0;
    Pts[0] := Layout.Project(Proj1);
    Proj2.X := FProfileOrigin.X + P2.X;
    Proj2.Y := FProfileOrigin.Y + P2.Z;
    Proj2.Z := 0.0;
    Pts[1] := Layout.Project(Proj2);
    PilotLine(Proj1.X,Proj1.Y,Proj2.X,Proj2.Y,Layout.PenColor);

    // Draw in aft view of bodyplan
    if (P1.X <= MidshipLocation) and (P2.X < MidshipLocation) then
    begin
      Proj1.X := FAftOrigin.X - P1.Y;
      Proj1.Y := FAftOrigin.Y + P1.Z;
      Pts[0] := Layout.Project(Proj1);
      Proj2.X := FAftOrigin.X - P2.Y;
      Proj2.Y := FAftOrigin.Y + P2.Z;
      Pts[1] := Layout.Project(Proj2);
      PilotLine(Proj1.X,Proj1.Y,Proj2.X,Proj2.Y,Layout.PenColor);

      Proj1.X := FAftOrigin.X + P1.Y;
      Proj1.Y := FAftOrigin.Y + P1.Z;
      Pts[0] := Layout.Project(Proj1);
      Proj2.X := FAftOrigin.X + P2.Y;
      Proj2.Y := FAftOrigin.Y + P2.Z;
      Pts[1] := Layout.Project(Proj2);
      PilotLine(Proj1.X,Proj1.Y,Proj2.X,Proj2.Y,Layout.PenColor);

    end;

    // Draw in front view of bodyplan
    if (P1.X >= MidshipLocation) and (P2.X > MidshipLocation) then
    begin
      Proj1.X := FFrontOrigin.X - P1.Y;
      Proj1.Y := FFrontOrigin.Y + P1.Z;
      Pts[0] := Layout.Project(Proj1);
      Proj2.X := FFrontOrigin.X - P2.Y;
      Proj2.Y := FFrontOrigin.Y + P2.Z;
      Pts[1] := Layout.Project(Proj2);
      PilotLine(Proj1.X,Proj1.Y,Proj2.X,Proj2.Y,Layout.PenColor);

      Proj1.X := FFrontOrigin.X + P1.Y;
      Proj1.Y := FFrontOrigin.Y + P1.Z;
      Pts[0] := Layout.Project(Proj1);
      Proj2.X := FFrontOrigin.X + P2.Y;
      Proj2.Y := FFrontOrigin.Y + P2.Z;
      Pts[1] := Layout.Project(Proj2);
      PilotLine(Proj1.X,Proj1.Y,Proj2.X,Proj2.Y,Layout.PenColor);

    end;
    // Draw in plan view
    Proj1.X := FPlanOrigin.X + P1.X;
    Proj1.Y := FPlanOrigin.Y + P1.Y;
    Pts[0] := Layout.Project(Proj1);
    Proj2.X := FPlanOrigin.X + P2.X;
    Proj2.Y := FPlanOrigin.Y + P2.Y;
    Pts[1] := Layout.Project(Proj2);
    PilotLine(Proj1.X,Proj1.Y,Proj2.X,Proj2.Y,Layout.PenColor);

    if (Freeship.NumberofDiagonals = 0) and (False) then
    begin
      Proj1.X := FPlanOrigin.X + P1.X;
      Proj1.Y := FPlanOrigin.Y - P1.Y;
      Pts[0] := Layout.Project(Proj1);
      Proj2.X := FPlanOrigin.X + P2.X;
      Proj2.Y := FPlanOrigin.Y - P2.Y;
      Pts[1] := Layout.Project(Proj2);
      PilotLine(Proj1.X,Proj1.Y,Proj2.X,Proj2.Y,Layout.PenColor);

    end;
  end;{DrawLine}



  procedure DrawSpline(Spline: TFreeSpline; Views: TLinesplanViews; Style: TPenStyle);
  var
    i: integer;
    P, Pr: T3DCoordinate;
    VPs: array of T3DCoordinate;
    Pts: array of TPoint;
    YSign:integer;
  begin
    VPs := Spline.GetValues;
    Setlength(Pts, length(VPs));
    if lvProfile in views then
    begin
      for i := 0 to length(VPs)-1 do
        begin
          P := VPs[i];
          Pr := Point3D(FProfileOrigin.X + P.X, FProfileOrigin.Y + P.Z, 0);
          PilotPoint(Pr.X,Pr.Y);

        end;
      PilotFlush(2,Layout.PenColor);

    end;

    if (lvAftBody in views) and (Spline.Max.X <= MidshipLocation) then
    begin
      for YSign:=-1 to +1 do
        if YSign <> 0 then
        begin
          for i := 0 to length(VPs)-1 do
            begin
              P := VPs[i];
              Pr := Point3D(FAftOrigin.X + YSign * P.Y, FAftOrigin.Y + P.Z, 0);
              PilotPoint(Pr.X,Pr.Y);

            end;
          PilotFlush(2,Layout.PenColor);

        end;
    end;

    if (lvFrontBody in views) and (Spline.Min.X >= MidshipLocation) then
    begin
      for YSign:=-1 to +1 do
        if YSign <> 0 then
        begin
          for i := 0 to length(VPs)-1 do
            begin
              P := VPs[i];
              Pr := Point3D(FFrontOrigin.X + YSign * P.Y, FFrontOrigin.Y + P.Z, 0);
              PilotPoint(Pr.X,Pr.Y);

            end;
          PilotFlush(2,Layout.PenColor);

        end;
    end;

    if lvPLan in views then
    begin
      for YSign:=-1 to +1 do
        if (YSign=1)
          or ((YSign=0) and (Freeship.NumberofDiagonals = 0)
             and (False))
        then
        begin
          for i := 0 to length(VPs)-1 do
            begin
              P := VPs[i];
              Pr := Point3D(FPlanOrigin.X + YSign * P.Y, FPlanOrigin.Y + P.Y, 0);
              PilotPoint(Pr.X,Pr.Y);

            end;
          PilotFlush(2,Layout.PenColor);

        end;
    end;

  end;{DrawSpline}


  procedure DrawIntersection(Intersection: TFreeIntersection;
    Views: TLinesplanViews; Style: TPenStyle);
  var
    I: integer;
  begin
    if not Intersection.Built then
      Intersection.Rebuild;
    for I := 1 to Intersection.Count do
      DrawSpline(Intersection.Items[I - 1], Views, Style);
  end;{DrawIntersection}

  procedure AddTriangle(P1, P2, P3: T3DCoordinate; Color: TColor;
  var Destination: TriangleArray; Symmetric: boolean);
  var
    C: T3DCoordinate;
  begin
    if Destination.Count = Destination.Capacity then
    begin
      Destination.Capacity := Math.Max(64, Destination.Capacity * 2);
      Setlength(Destination.Triangles, Destination.Capacity);
    end;
    C.X := (P1.X + P2.X + P3.X) / 3;
    C.Y := (P1.Y + P2.Y + P3.Y) / 3;
    C.Z := (P1.Z + P2.Z + P3.Z) / 3;
    Destination.Triangles[Destination.Count].P1 := P1;
    Destination.Triangles[Destination.Count].P2 := P2;
    Destination.Triangles[Destination.Count].P3 := P3;
    Destination.Triangles[Destination.Count].Center := C;
    Destination.Triangles[Destination.Count].Color := Color;
    Destination.Triangles[Destination.Count].Symmetric := Symmetric;
    Inc(Destination.Count);
  end;{AddTriangle}

  procedure ProcessFace(Face: TFreeSubdivisionFace; Color: TColor; Symmetric: boolean);
  var
    I, J: integer;
    Nabove: integer;
    Nbelow: integer;
    AbovePoints: TFreeCoordinateArray;
    BelowPoints: TFreeCoordinateArray;
  begin
    for I := 3 to Face.NumberOfpoints do
    begin
      ClipTriangle(Face.Point[0].Coordinate,
        Face.Point[I - 2].Coordinate,
        Face.Point[I - 1].Coordinate,
        WlPlane, Nabove, Nbelow, AbovePoints, BelowPoints);
      // proces submerged area
      for J := 3 to NBelow do
        AddTriangle(BelowPoints[0], BelowPoints[J - 2], BelowPoints[J - 1], SubmColor, Below, Symmetric);
      for J := 3 to NAbove do
        AddTriangle(AbovePoints[0], AbovePoints[J - 2], AbovePoints[J - 1], Color, Above, Symmetric);
    end;
  end;{Processface}

  procedure SortTriangles(var Triangles: TriangleArray; SortType: byte);
  // Sorttype 1=X, 2=Y, 3=Z

    procedure QuickSort(L, R: integer);
    var
      I, J: integer;
      T1: TriangleData;

      procedure Swap(I, J: integer);
      var
        Tmp: TriangleData;
      begin
        Tmp := Triangles.Triangles[I];
        Triangles.Triangles[I] := Triangles.Triangles[J];
        Triangles.Triangles[J] := Tmp;
      end;{swap two triangles}

    begin
      I := L;
      J := R;
      T1 := Triangles.Triangles[(L + R) div 2];
      repeat
        if SortType = 1 then
        begin
          while Triangles.Triangles[I].Center.X < T1.Center.X do
            Inc(I);
          while T1.Center.X < Triangles.Triangles[J].Center.X do
            Dec(J);
        end
        else if SortType = 2 then
        begin
          while Triangles.Triangles[I].Center.Y < T1.Center.Y do
            Inc(I);
          while T1.Center.Y < Triangles.Triangles[J].Center.Y do
            Dec(J);
        end
        else
        begin
          while Triangles.Triangles[I].Center.Z < T1.Center.Z do
            Inc(I);
          while T1.Center.Z < Triangles.Triangles[J].Center.Z do
            Dec(J);
        end;
        if I <= J then
        begin
          Swap(I, J);
          Inc(I);
          Dec(J);
        end;
      until I > J;
      if L < J then
        QuickSort(L, J);
      if I < R then
        QuickSort(I, R);
    end;{QuickSort}

  begin
    if Triangles.Count > 1 then
      QuickSort(0, Triangles.Count - 1);
  end;{SortTriangles}

  procedure DrawTriangles(Triangles: TriangleArray; Views: TLinesplanViews);
  var
    I: integer;
    Triangle: TriangleData;
    Pts: array[0..2] of TPoint;
    P: T3DCoordinate;
    L: T3DCoordinate;
    Col: TColor;

    procedure SetFillColor(Color:TColor);
    begin Col:=Color end;

    function ProjectFillPoint(const P:T3DCoordinate):TPoint;
    begin
      PilotPoint(P.X,P.Y);
      Result:=Types.Point(0,0);
    end;

    function GetColor(Lightvector: T3DCoordinate; DpScale, Contrast: TFloatType;
      Triangle: TriangleData): TColor;
    var
      N: T3DCoordinate;
      Dp: TFloatType;
      R, G, B: byte;
    begin
      N := UnifiedNormal(Triangle.P1, Triangle.P2, Triangle.P3);
      Dp := DotProduct(N, L);
      if Dp < 0.0 then
        Dp := -Dp;

      Dp := Power(Dp, Contrast);
      Dp := (DpScale - 1.0) + DpScale * Dp;
      if Dp > 1.0 then
        Dp := 1.0;
      R := Round(Dp * GetRValue(Triangle.Color));
      G := Round(Dp * GetGValue(Triangle.Color));
      B := Round(Dp * GetBValue(Triangle.Color));
      Result := RGB(R, G, B);
          {
          Layout.ShadedColor(Dp,GetRValue(Triangle.Color),GetGValue(Triangle.Color),GetBValue(Triangle.Color),R,G,B);
          Result:=RGB(R,G,B);
          }
    end;{GetColor}

  begin
    if lvAftBody in views then
    begin
      P.Z := 0.0;
      L := SetPoint(-1.0, 0.0, 0.0);
      for I := Triangles.Count downto 1 do
      begin
        Triangle := Triangles.Triangles[I - 1];
        if not True then
        begin
          SetFillColor(Triangle.Color);
        end
        else
        begin
          Col := GetColor(L, 1.4, 1.0, Triangle);
          SetFillColor(Col);
        end;
        // Draw in aft view
        P.X := FAftOrigin.X - Triangle.P1.Y;
        P.Y := FAftOrigin.Y + Triangle.P1.Z;
        Pts[0] := ProjectFillPoint(P);
        P.X := FAftOrigin.X - Triangle.P2.Y;
        P.Y := FAftOrigin.Y + Triangle.P2.Z;
        Pts[1] := ProjectFillPoint(P);
        P.X := FAftOrigin.X - Triangle.P3.Y;
        P.Y := FAftOrigin.Y + Triangle.P3.Z;
        Pts[2] := ProjectFillPoint(P);
        PilotFlush(1,Col);
        if Triangle.Symmetric then
        begin
          P.X := FAftOrigin.X + Triangle.P1.Y;
          P.Y := FAftOrigin.Y + Triangle.P1.Z;
          Pts[0] := ProjectFillPoint(P);
          P.X := FAftOrigin.X + Triangle.P2.Y;
          P.Y := FAftOrigin.Y + Triangle.P2.Z;
          Pts[1] := ProjectFillPoint(P);
          P.X := FAftOrigin.X + Triangle.P3.Y;
          P.Y := FAftOrigin.Y + Triangle.P3.Z;
          Pts[2] := ProjectFillPoint(P);
          PilotFlush(1,Col);
        end;
      end;
    end
    else
    begin
      L := SetPoint(-1.0, 0.0, 0.0);
      for I := 1 to Triangles.Count do
      begin
        Triangle := Triangles.Triangles[I - 1];
        P.Z := 0.0;
        if lvProfile in Views then
        begin
          L := SetPoint(0.0, -1.0, 0.0);
          if not True then
          begin
            SetFillColor(Triangle.Color);
          end
          else
          begin
            Col := GetColor(L, 1.1, 1.5, Triangle);
            SetFillColor(Col);
          end;
          // Draw in profile view
          P.X := FProfileOrigin.X + Triangle.P1.X;
          P.Y := FProfileOrigin.Y + Triangle.P1.Z;
          Pts[0] := ProjectFillPoint(P);
          P.X := FProfileOrigin.X + Triangle.P2.X;
          P.Y := FProfileOrigin.Y + Triangle.P2.Z;
          Pts[1] := ProjectFillPoint(P);
          P.X := FProfileOrigin.X + Triangle.P3.X;
          P.Y := FProfileOrigin.Y + Triangle.P3.Z;
          Pts[2] := ProjectFillPoint(P);
          PilotFlush(1,Col);
        end;

        if lvFrontBody in views then
        begin
          L := SetPoint(1.0, 0.0, 0.0);
          if not True then
          begin
            SetFillColor(Triangle.Color);
          end
          else
          begin
            Col := GetColor(L, 1.4, 1.0, Triangle);
            SetFillColor(Col);
          end;
          // Draw in Front view
          P.X := FFrontOrigin.X - Triangle.P1.Y;
          P.Y := FFrontOrigin.Y + Triangle.P1.Z;
          Pts[0] := ProjectFillPoint(P);
          P.X := FFrontOrigin.X - Triangle.P2.Y;
          P.Y := FFrontOrigin.Y + Triangle.P2.Z;
          Pts[1] := ProjectFillPoint(P);
          P.X := FFrontOrigin.X - Triangle.P3.Y;
          P.Y := FFrontOrigin.Y + Triangle.P3.Z;
          Pts[2] := ProjectFillPoint(P);
          PilotFlush(1,Col);
          if Triangle.Symmetric then
          begin
            P.X := FFrontOrigin.X + Triangle.P1.Y;
            P.Y := FFrontOrigin.Y + Triangle.P1.Z;
            Pts[0] := ProjectFillPoint(P);
            P.X := FFrontOrigin.X + Triangle.P2.Y;
            P.Y := FFrontOrigin.Y + Triangle.P2.Z;
            Pts[1] := ProjectFillPoint(P);
            P.X := FFrontOrigin.X + Triangle.P3.Y;
            P.Y := FFrontOrigin.Y + Triangle.P3.Z;
            Pts[2] := ProjectFillPoint(P);
            PilotFlush(1,Col);
          end;
        end;
        if lvPlan in views then
        begin
          L := SetPoint(0.0, 0.0, -1.0);
          if not True then
          begin
            SetFillColor(Triangle.Color);
          end
          else
          begin
            Col := GetColor(L, 1.1, 1.5, Triangle);
            SetFillColor(Col);
          end;
          // Draw portside in plan view
          P.X := FPlanOrigin.X + Triangle.P1.X;
          P.Y := FPlanOrigin.Y + Triangle.P1.Y;
          Pts[0] := ProjectFillPoint(P);
          P.X := FPlanOrigin.X + Triangle.P2.X;
          P.Y := FPlanOrigin.Y + Triangle.P2.Y;
          Pts[1] := ProjectFillPoint(P);
          P.X := FPlanOrigin.X + Triangle.P3.X;
          P.Y := FPlanOrigin.Y + Triangle.P3.Y;
          Pts[2] := ProjectFillPoint(P);
          PilotFlush(1,Col);
          if (Freeship.NumberofDiagonals = 0) and (False) and
            (Triangle.Symmetric) then
          begin
            P.X := FPlanOrigin.X + Triangle.P1.X;
            P.Y := FPlanOrigin.Y - Triangle.P1.Y;
            Pts[0] := ProjectFillPoint(P);
            P.X := FPlanOrigin.X + Triangle.P2.X;
            P.Y := FPlanOrigin.Y - Triangle.P2.Y;
            Pts[1] := ProjectFillPoint(P);
            P.X := FPlanOrigin.X + Triangle.P3.X;
            P.Y := FPlanOrigin.Y - Triangle.P3.Y;
            Pts[2] := ProjectFillPoint(P);
            PilotFlush(1,Col);
          end;
        end;
      end;
    end;
  end;{DrawTriangles}

begin
  if Freeship = nil then
    exit;
  Steps:=200;
  WlPlane.a := 0.0;
  WlPlane.b := 0.0;
  WlPlane.c := 1.0;
  WlPlane.d := -(Freeship.Surface.Min.Z + Freeship.ProjectSettings.ProjectDraft);
  //MidshipLocation := Freeship.ProjectSettings.ProjectSplitSectionLocation;
  if not FFreeship.HydrostaticCalculation[0].Calculated then
    FFreeship.HydrostaticCalculation[0].Calculate;
  MidshipLocation := //FFreeship.ProjectSettings.ProjectSplitSectionLocation;
                     FFreeship.HydrostaticCalculation[0].MidshipLocation;
  Mainplane.a := 1.0;
  Mainplane.b := 0.0;
  Mainplane.c := 0.0;
  Mainplane.d := -MidshipLocation;

  Below.Count:=0;Below.Capacity:=0;Above.Count:=0;Above.Capacity:=0;
  Layout.PenColor:=clSilver;Layout.Font.Name:='Arial';Layout.Font.Color:=clBlack;
  Layout.Font.Size:=7;
      for I := 1 to Freeship.Surface.NumberOfLayers do
        if Freeship.Layer[I - 1].ShowInLinesplan then
        begin
          Layer := Freeship.Surface.Layer[I - 1];
                    if Freeship.ProjectSettings.ProjectShadeUnderwaterShip then
            SubmColor := Freeship.ProjectSettings.ProjectUnderWaterColor
          else
            SubmColor := Layer.Color;
          for j := 1 to Layer.Count do
          begin
            Face := Layer.Items[J - 1];
            Done := False;
            if Face.Max.Z < Freeship.Surface.Min.Z +
            Freeship.ProjectSettings.ProjectDraft then
            begin
              for N := 1 to Face.ChildCount do
              begin
                Child := Face.Child[N - 1];
                for K := 3 to Child.NumberOfpoints do
                  AddTriangle(Child.Point[0].Coordinate, Child.Point[K - 2].Coordinate,
                    Child.Point[K - 1].Coordinate, SubmColor, Below, Layer.Symmetric);
              end;
              Done := True;
            end
            else if Face.Min.Z > Freeship.Surface.Min.Z + Freeship.ProjectSettings.ProjectDraft then
            begin
              for N := 1 to Face.ChildCount do
              begin
                Child := Face.Child[N - 1];
                for K := 3 to Child.NumberOfpoints do
                  AddTriangle(Child.Point[0].Coordinate, Child.Point[K - 2].Coordinate,
                    Child.Point[K - 1].Coordinate, Layer.Color, Above, Layer.Symmetric);
              end;
              Done := True;
            end;
            if not done then
              for N := 1 to Face.ChildCount do
                Processface(Face.Child[N - 1], Layer.Color, Layer.Symmetric);
          end;
        end;


      SortTriangles(Below, 2);
      SortTriangles(Above, 2);
      DrawTriangles(Below, [lvProfile]);
      DrawTriangles(Above, [lvProfile]);
      SortTriangles(Below, 1);
      SortTriangles(Above, 1);
      // Aft bodyplan
      DrawTriangles(Below, [lvAftBody]);
      DrawTriangles(Above, [lvAftBody]);
      // frontview on bodyplan
      DrawTriangles(Below, [lvFrontBody]);
      DrawTriangles(Above, [lvFrontBody]);
      // plan view
      SortTriangles(Below, 3);
      SortTriangles(Above, 3);
      DrawTriangles(Below, [lvPlan]);
      DrawTriangles(Above, [lvPlan]);
    // Keep fixed-width strokes out of the transformable fill atlas as well.
    // The waterline band is a vector stroke, so its width stays native while
    // the hull fill is scaled during resize/pan/zoom.
    if (not False) and True then
    begin
      // Draw dwl as a white band in profile and bodyplan views
      Layout.PenColor := Layout.Color;

      Space := CalculateSpace(0.5 * textspace, FMin3D.X, FMax3D.X);

      // draw dwl in profile
      DrawLineAtt(FProfileOrigin, SetPoint(FMin3D.X - Space, FMin3D.Z +
        FFreeship.ProjectSettings.ProjectDraft, 0), SetPoint(
        FMax3D.X + Space, FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft, 0), '', False);
      // draw dwl in aft bodyplan
      Space := CalculateSpace(textspace, FMin3D.Y, FMax3D.Y);
      DrawLineAtt(FAftOrigin, SetPoint(-FMax3D.Y - Space, FMin3D.Z +
        FFreeship.ProjectSettings.ProjectDraft, 0), SetPoint(
        FMax3D.Y + Space, FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft, 0), '', False);
      // draw dwl in front bodyplan
      DrawLineAtt(FFrontOrigin, SetPoint(-FMax3D.Y - Space, FMin3D.Z +
        FFreeship.ProjectSettings.ProjectDraft, 0), SetPoint(
        FMax3D.Y + Space, FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft, 0), '', False);
    end;

    // Reserve the feature lines before placing any grid labels.

    // Draw grid
    if False then
      Layout.PenColor := clSilver
    else
      Layout.PenColor := clSilver;

    // first draw grid in profile view

    Space := CalculateSpace(0.2 * textspace, FMin3D.X, FMax3D.X);
    if False then
      Layout.Font.Color := clBlack
    else
      Layout.Font.Color := clRed;
    // draw baseline
    DrawLineAtt(FProfileOrigin, SetPoint(FMin3D.X - Space, FMin3D.Z, 0),
      SetPoint(FMax3D.X + Space, FMin3D.Z, 0), rs_Base {UserString[184]} + #32 +
      ConvertDimension(
      FMin3D.Z, Freeship.ProjectSettings.ProjectUnits), False);
    // draw dwl
    DrawLineAtt(FProfileOrigin, SetPoint(FMin3D.X - Space, FMin3D.Z +
      FFreeship.ProjectSettings.ProjectDraft, 0), SetPoint(
      FMax3D.X + Space, FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft, 0), rs_DWL {UserString[185]} + #32 +
      ConvertDimension(FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft,
      Freeship.ProjectSettings.ProjectUnits), False);

    Layout.Font.Color := clBlack;
    for I := 1 to FFreeship.NumberofWaterlines do
    begin
      Tmp := -FFreeship.Waterline[I - 1].Plane.D;
      Str := ConvertDimension(Tmp, Freeship.ProjectSettings.ProjectUnits);
      DrawLineAtt(FProfileOrigin, SetPoint(FMin3D.X - Space, Tmp, 0),
        SetPoint(FMax3D.X + Space, Tmp, 0), Str, False);
    end;
    Space := CalculateSpace(textspace, FMin3D.Z, FMax3D.Z);
    for I := 1 to FFreeship.NumberofStations do
    begin
      Tmp := -FFreeship.Station[I - 1].Plane.D;
      Str := ConvertDimension(Tmp, Freeship.ProjectSettings.ProjectUnits);
      DrawLineAtt(FProfileOrigin, SetPoint(Tmp, FMin3D.Z - space, 0),
        SetPoint(Tmp, FMax3D.Z + space, 0), Str, True);
    end;
    // draw grid in aft body view

    Space := CalculateSpace(textspace, FMin3D.Y, FMax3D.Y);
    if False then
      Layout.Font.Color := clBlack
    else
      Layout.Font.Color := clRed;
    // draw base
    DrawLineAtt(FAftOrigin, SetPoint(-FMax3D.Y - Space, FMin3D.Z, 0),
      SetPoint(FMax3D.Y + Space, FMin3D.Z, 0), rs_Base {UserString[184]} + #32 + ConvertDimension(
      FMin3D.Z, Freeship.ProjectSettings.ProjectUnits), False);
    // draw dwl
    DrawLineAtt(FAftOrigin, SetPoint(-FMax3D.Y - Space, FMin3D.Z +
      FFreeship.ProjectSettings.ProjectDraft, 0), SetPoint(
      FMax3D.Y + Space, FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft, 0), rs_DWL {UserString[185]} + #32 +
      ConvertDimension(FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft,
      Freeship.ProjectSettings.ProjectUnits), False);
    Space := CalculateSpace(textspace, FMin3D.Z, FMax3D.Z);
    DrawLineAtt(FAftOrigin, SetPoint(0.0, FMin3D.Z - space, 0), SetPoint(
      0.0, FMax3D.Z + space, 0), rs_Center {UserString[245]}, True);
    Space := CalculateSpace(textspace, FMin3D.Y, FMax3D.Y);
    Layout.Font.Color := clBlack;

    for I := 1 to FFreeship.NumberofWaterlines do
    begin
      Tmp := -FFreeship.Waterline[I - 1].Plane.D;
      Str := ConvertDimension(Tmp, Freeship.ProjectSettings.ProjectUnits);
      DrawLineAtt(FAftOrigin, SetPoint(-FMax3D.Y - Space, Tmp, 0),
        SetPoint(FMax3D.Y + Space, Tmp, 0), Str, False);
    end;
    Space := CalculateSpace(textspace, FMin3D.Z, FMax3D.Z);
    for I := 1 to FFreeship.NumberofButtocks do
    begin
      Tmp := -FFreeship.Buttock[I - 1].Plane.D;
      Str := ConvertDimension(Tmp, Freeship.ProjectSettings.ProjectUnits);
      DrawLineAtt(FAftOrigin, SetPoint(Tmp, FMin3D.Z - space, 0), SetPoint(
        Tmp, FMax3D.Z + space, 0), Str, True);
      DrawLineAtt(FAftOrigin, SetPoint(-Tmp, FMin3D.Z - space, 0),
        SetPoint(-Tmp, FMax3D.Z + space, 0), Str, True);
    end;
    // draw grid in front body view

    Space := CalculateSpace(textspace, FMin3D.Y, FMax3D.Y);
    if False then
      Layout.Font.Color := clBlack
    else
      Layout.Font.Color := clRed;
    // draw baseline
    DrawLineAtt(FFrontOrigin, SetPoint(-FMax3D.Y - Space, FMin3D.Z, 0),
      SetPoint(FMax3D.Y + Space, FMin3D.Z, 0), rs_Base {UserString[184]} + #32 +
      ConvertDimension(
      FMin3D.Z, Freeship.ProjectSettings.ProjectUnits), False);
    // draw dwl
    DrawLineAtt(FFrontOrigin, SetPoint(-FMax3D.Y - Space, FMin3D.Z +
      FFreeship.ProjectSettings.ProjectDraft, 0), SetPoint(
      FMax3D.Y + Space, FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft, 0), rs_DWL {UserString[185]} + #32 +
      ConvertDimension(FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft,
      Freeship.ProjectSettings.ProjectUnits), False);
    Space := CalculateSpace(textspace, FMin3D.Z, FMax3D.Z);
    DrawLineAtt(FFrontOrigin, SetPoint(0.0, FMin3D.Z - space, 0), SetPoint(
      0.0, FMax3D.Z + space, 0), rs_Center {UserString[245]}, True);
    Space := CalculateSpace(textspace, FMin3D.Y, FMax3D.Y);
    Layout.Font.Color := clBlack;

    for I := 1 to FFreeship.NumberofWaterlines do
    begin
      Tmp := -FFreeship.Waterline[I - 1].Plane.D;
      Str := ConvertDimension(Tmp, Freeship.ProjectSettings.ProjectUnits);
      DrawLineAtt(FFrontOrigin, SetPoint(-FMax3D.Y - Space, Tmp, 0),
        SetPoint(FMax3D.Y + Space, Tmp, 0), Str, False);
    end;
    Space := CalculateSpace(textspace, FMin3D.Z, FMax3D.Z);
    for I := 1 to FFreeship.NumberofButtocks do
    begin
      Tmp := -FFreeship.Buttock[I - 1].Plane.D;
      Str := ConvertDimension(Tmp, Freeship.ProjectSettings.ProjectUnits);
      DrawLineAtt(FFrontOrigin, SetPoint(Tmp, FMin3D.Z - space, 0),
        SetPoint(Tmp, FMax3D.Z + space, 0), Str, True);
      DrawLineAtt(FFrontOrigin, SetPoint(-Tmp, FMin3D.Z - space, 0),
        SetPoint(-Tmp, FMax3D.Z + space, 0), Str, True);
    end;
    // draw grid in plan view

    Space := CalculateSpace(0.2 * textspace, FMin3D.X, FMax3D.X);
    if False then
      Layout.Font.Color := clBlack
    else
      Layout.Font.Color := clRed;
    DrawLineAtt(FPlanOrigin, SetPoint(FMin3D.X - Space, 0, 0), SetPoint(
      FMax3D.X + Space, 0, 0), rs_Center {UserString[245]}, False);
    Layout.Font.Color := clBlack;

    for I := 1 to FFreeship.NumberofButtocks do
    begin
      Tmp := -FFreeship.Buttock[I - 1].Plane.D;
      Str := ConvertDimension(Tmp, Freeship.ProjectSettings.ProjectUnits);
      DrawLineAtt(FPlanOrigin, SetPoint(FMin3D.X - Space, Tmp, 0),
        SetPoint(FMax3D.X + Space, Tmp, 0), Str, False);
      if (Freeship.NumberofDiagonals = 0) and (False) then
        DrawLineAtt(FPlanOrigin, SetPoint(FMin3D.X - Space, -Tmp, 0), SetPoint(
          FMax3D.X + Space, -Tmp, 0), Str, False);
    end;
    Space := CalculateSpace(textspace, FMin3D.Y, FMax3D.Y);
    for I := 1 to FFreeship.NumberofStations do
    begin
      Tmp := -FFreeship.Station[I - 1].Plane.D;
      Str := ConvertDimension(Tmp, Freeship.ProjectSettings.ProjectUnits);
      if (Freeship.NumberofDiagonals = 0) and (False) then
        DrawLineAtt(FPlanOrigin, SetPoint(Tmp, -FMax3D.Y - space, 0), SetPoint(
          Tmp, FMax3D.Y + space, 0), Str, True)
      else
        DrawLineAtt(FPlanOrigin, SetPoint(Tmp, -FDiagonalWidth - space, 0), SetPoint(
          Tmp, FMax3D.Y + space, 0), Str, True);
    end;

    // draw knuckle-lines
    //if not True then
    begin

      if False then
        Layout.PenColor := clBlack
      else
        Layout.PenColor := Freeship.Preferences.CreaseColor;
      for I := 1 to FreeShip.NumberOfLayers do
      begin
        Layer := FreeShip.Layer[I - 1];
        if Layer.ShowInLinesplan then
          for J := 1 to Layer.Count do
          begin
            Face := layer.Items[J - 1];
            for K := 1 to Face.ControlDescendantEdgeCount do
            begin
              Edge := Face.ControlDescendantEdge[K - 1];
              if Edge.Crease then
                DrawLine(Edge.StartPoint.Coordinate, Edge.EndPoint.Coordinate);
            end;
          end;
      end;
    end;

    MidshipLocation := Freeship.ProjectSettings.ProjectSplitSectionLocation;
    // Draw stations
    if False then
      Layout.PenColor := clBlack
    else
      Layout.PenColor := Freeship.Preferences.StationColor;
    for i := 1 to Freeship.NumberofStations do
      DrawIntersection(Freeship.Station[I - 1], [lvAftBody, lvFrontBody], psSolid);
    // Draw Buttocks
    if False then
      Layout.PenColor := clBlack
    else
      Layout.PenColor := Freeship.Preferences.ButtockColor;
    for i := 1 to Freeship.NumberofButtocks do
      DrawIntersection(Freeship.Buttock[I - 1], [lvProfile], psSolid);
    // Draw Waterlines
    if False then
      Layout.PenColor := clBlack
    else
      Layout.PenColor := Freeship.Preferences.WaterlineColor;
    for i := 1 to Freeship.NumberofWaterlines do
      DrawIntersection(Freeship.Waterline[I - 1], [lvPlan], psSolid);

    // Draw diagonals

    for I := 1 to Freeship.NumberofDiagonals do
    begin
      Diagonal := Freeship.Diagonal[I - 1];
      Plane.a := 0.0;
      Plane.b := 1 / Sqrt(2);
      Plane.c := -1 / sqrt(2);
      Plane.d := -Diagonal.Plane.d;
      if not Diagonal.Built then
        Diagonal.Rebuild;
      for J := 1 to Diagonal.Count do
      begin

        if False then
          Layout.PenColor := clBlack
        else
          Layout.PenColor := Freeship.Preferences.DiagonalColor;

        Spline := Diagonal.Items[J - 1];
        Spline.Fragments:=Steps;
        Pts3D := Spline.GetValues;
        Setlength(Pts, length(Pts3D));

        Min := 0;
        Max := 0;
        L:=0; Pn:=0;
        for K := 0 to length(Pts3D)-1 do
        begin
          P := Pts3D[K];
          Tmp := abs(Plane.A * P.x + Plane.B * P.y + Plane.C * P.z + Plane.D);
          if K = 0 then
          begin
            Min := Tmp;
            Max := Tmp;
          end
          else
          begin
            if Tmp < min then
              Min := Tmp;
            if Tmp > max then
              Max := Tmp;
          end;
          P:=Point3D(FPlanOrigin.X + P.X, FPlanOrigin.Y - abs(Tmp), 0.0);
          PilotPoint(P.X,P.Y);

        end;

       {  // changed to above
        for K := 0 to steps do
        begin
          P := Spline.Value(K / steps, Pn1, Pn2);
          if (Pn<>Pn1) and Spline.Knuckle[Pn1] then // add knuckle
          begin
            Pkn := Spline.Point[Pn1];
            Pn:=Pn1;
            Tmp := abs(Plane.A * Pkn.x + Plane.B * Pkn.y + Plane.C * Pkn.z + Plane.D);
            if K = 0 then
            begin
              Min := Tmp;
              Max := Tmp;
            end
            else
            begin
              if Tmp < min then
                Min := Tmp;
              if Tmp > max then
                Max := Tmp;
            end;
            P:=Point3D(FPlanOrigin.X + Pkn.X, FPlanOrigin.Y - abs(Tmp), 0.0);
            Pts[L] := Layout.Project(P);
            inc(L);
          end;

          Tmp := abs(Plane.A * P.x + Plane.B * P.y + Plane.C * P.z + Plane.D);
          if K = 0 then
          begin
            Min := Tmp;
            Max := Tmp;
          end
          else
          begin
            if Tmp < min then
              Min := Tmp;
            if Tmp > max then
              Max := Tmp;
          end;
          P:=Point3D(FPlanOrigin.X + P.X, FPlanOrigin.Y - abs(Tmp), 0.0);
          Pts[L] := Layout.Project(P);
          inc(L);
        end;
        SetLength(Pts, L);
        }

        PilotFlush(2,Layout.PenColor);

        // draw as grid in bodyplan views

        if False then
          Layout.PenColor := clBlack
        else
          Layout.PenColor := clSilver;
        // calculate height of intersection of diagonal with centerplane
        Tmp := -Diagonal.PLane.d / Diagonal.Plane.c;
        P1 := SetPoint(0.0, Min * Sin(DegToRad(45)), Tmp - Min * Sin(DegToRad(45)));
        P2 := SetPoint(0.0, Max * Sin(DegToRad(45)), Tmp - Max * Sin(DegToRad(45)));
        DrawDiagonalLine(FAftOrigin, P1, P2);
        DrawDiagonalLine(FFrontOrigin, P1, P2);
      end;
    end;
    DrawPendingLabels;
end;

procedure TLinesplanScene.ExportDXF(const Filename:string);
var

  Strings: TStringList;
  Str: string;
  Space, Tmp: TFloatType;
  SplitSectionLocation: TFloatType;
  Min, Max: TFloatType;
  I, J, K: integer;
  Edge: TFreeSubdivisionEdge;
  P1, P2: T3DCoordinate;
  Diagonal: TFreeIntersection;
  Plane: T3DPlane;
  Spline: TFreeSpline;
  EdgePointLists: TFasterListTFasterListTFreeSubdivisionPoint;
  Points: TFasterListTFreeSubdivisionPoint;
  SplineValues: T3DCoordinateArray;

  procedure WriteDXFPoint(N: integer; P: T3DCoordinate);
  begin
    Strings.Add(IntToStr(N) + EOL + FloatToDec(P.X, 6));
    Strings.Add(IntToStr(N + 10) + EOL + FloatToDec(P.Y, 6));
  end;{WriteDXFPoint}

  procedure AddLine(P1, P2: T3DCoordinate; Views: TLinesplanViews;
    Layername: string; Color: TColor);
  var
    Startp, Endp: T3DCoordinate;
  begin
    if lvProfile in views then
    begin
      StartP.X := FProfileOrigin.X + P1.X;
      StartP.Y := FProfileOrigin.Y + P1.Z;
      StartP.Z := 0.0;
      EndP.X := FProfileOrigin.X + P2.X;
      EndP.Y := FProfileOrigin.Y + P2.Z;
      EndP.Z := 0.0;
    end;
    if lvAftBody in views then
    begin
      StartP.X := FAftOrigin.X - P1.Y;
      StartP.Y := FAftOrigin.Y + P1.Z;
      StartP.Z := 0.0;
      EndP.X := FAftOrigin.X - P2.Y;
      EndP.Y := FAftOrigin.Y + P2.Z;
      EndP.Z := 0.0;
    end;
    if lvFrontBody in views then
    begin
      StartP.X := FFrontOrigin.X - P1.Y;
      StartP.Y := FFrontOrigin.Y + P1.Z;
      StartP.Z := 0.0;
      EndP.X := FFrontOrigin.X - P2.Y;
      EndP.Y := FFrontOrigin.Y + P2.Z;
      EndP.Z := 0.0;
    end;
    if lvPlan in views then
    begin
      StartP.X := FPlanOrigin.X + P1.X;
      StartP.Y := FPlanOrigin.Y + P1.Y;
      StartP.Z := 0.0;
      EndP.X := FPlanOrigin.X + P2.X;
      EndP.Y := FPlanOrigin.Y + P2.Y;
      EndP.Z := 0.0;
    end;
    Strings.Add('0' + EOL + 'LINE');
    Strings.Add('8' + EOL + LayerName);
    Strings.Add('62' + EOL + IntToStr(FindDXFColorIndex(Color)));
    WriteDXFPoint(10, StartP);
    WriteDXFPoint(11, EndP);
  end;{AddLine}

  procedure AddSpline(Spline: TFreeSpline; Views: TLinesplanViews;
    Layername: string; Color: TColor);
  var
    Points: array of T3DCoordinate;
    P: T3DCoordinate;
    I,J: integer;
    Params: TFloatArray;
    NParams, NValues: integer;
    Pn, Pn1, Pn2: integer;
  begin
    NParams := 0;
    Setlength(Params, Spline.NumberOfPoints);
    // count number of knucklepoints
    if not Spline.Built then
      Spline.Rebuild;
    for I := 2 to Spline.NumberOfPoints - 1 do
    begin
      if Spline.Knuckle[I - 1] then
      begin
        Params[NParams] := Spline.Parameter[I - 1];
        Inc(NParams);
      end;
    end;
    Spline.Fragments := 100;
    Setlength(Params, NParams + Spline.Fragments);
    for I := 1 to Spline.Fragments do
    begin
      Params[NParams] := (I - 1) / (Spline.Fragments - 1);
      Inc(NParams);
    end;
    SortFloatArray(Params, NParams);

    {Setlength(Points, NParams + Spline.NumberOfPoints);
    J:=0; Pn:=0;
    for I := 0 to NParams - 1 do
    begin
      P:=Spline.Value(Params[I], Pn1, Pn2);
      if (Pn<>Pn1) and Spline.Knuckle[Pn1] then
      begin
        Points[J] := Spline.Point[Pn1];
        Pn:=Pn1;
        inc(J);
      end;
      Points[J] := P;
    end;
    Setlength(Points, J+1);
    }
    Points := Spline.GetValues;
    NValues := length(Points);

    Strings.Add('0' + EOL + 'POLYLINE');
    Strings.Add('8' + EOL + LayerName);   // layername
    Strings.Add('62' + EOL + IntToStr(FindDXFColorIndex(Color)));
    Strings.Add('66' + EOL + '1');    // vertices follow
    for I := 0 to NValues - 1 do
    begin
      P.Z := 0.0;
      if lvProfile in views then
      begin
        P.X := FProfileOrigin.X + Points[I].X;
        P.Y := FProfileOrigin.Y + Points[I].Z;
      end;
      if lvAftBody in views then
      begin
        P.X := FAftOrigin.X - Points[I].Y;
        P.Y := FAftOrigin.Y + Points[I].Z;
      end;
      if lvFrontBody in views then
      begin
        P.X := FFrontOrigin.X - Points[I].Y;
        P.Y := FFrontOrigin.Y + Points[I].Z;
      end;
      if lvPlan in views then
      begin
        P.X := FPlanOrigin.X + Points[I].X;
        P.Y := FPlanOrigin.Y + Points[I].Y;
      end;
      Strings.Add('0' + EOL + 'VERTEX');
      Strings.Add('8' + EOL + LayerName);
      Strings.Add('10' + EOL + FloatToDec(P.X, 4));
      Strings.Add('20' + EOL + FloatToDec(P.Y, 4));
    end;
    Strings.Add('0' + EOL + 'SEQEND');

    if (lvAftBody in views) or (lvFrontBody in views) or
      ((lvPlan in views) and (Freeship.NumberofDiagonals = 0) and (False)) then
    begin
      // mirror line
      Strings.Add('0' + EOL + 'POLYLINE');
      Strings.Add('8' + EOL + LayerName);   // layername
      Strings.Add('62' + EOL + IntToStr(FindDXFColorIndex(Color)));
      Strings.Add('66' + EOL + '1');    // vertices follow
      for I := 0 to NValues - 1 do
      begin
        P.Z := 0.0;
        if lvAftBody in views then
        begin
          P.X := FAftOrigin.X + Points[I].Y;
          P.Y := FAftOrigin.Y + Points[I].Z;
        end;
        if lvFrontBody in views then
        begin
          P.X := FFrontOrigin.X + Points[I].Y;
          P.Y := FFrontOrigin.Y + Points[I].Z;
        end;
        if lvPlan in views then
        begin
          P.X := FPlanOrigin.X + Points[I].X;
          P.Y := FPlanOrigin.Y - Points[I].Y;
        end;

        Strings.Add('0' + EOL + 'VERTEX');
        Strings.Add('8' + EOL + LayerName);
        Strings.Add('10' + EOL + FloatToDec(P.X, 4));
        Strings.Add('20' + EOL + FloatToDec(P.Y, 4));
      end;
      Strings.Add('0' + EOL + 'SEQEND');
    end;
  end;{AddSpline}

  procedure AddEdgeLoop(const Points: TFasterListTFreeSubdivisionPoint;
    Views: TLinesplanViews; Layername: string; Color: TColor);
  var
    Point: TFreeSubdivisionPoint;
    P: T3DCoordinate;
    I: integer;
  begin
    Strings.Add('0' + EOL + 'POLYLINE');
    Strings.Add('8' + EOL + LayerName);   // layername
    Strings.Add('62' + EOL + IntToStr(FindDXFColorIndex(Color)));
    Strings.Add('66' + EOL + '1');    // vertices follow
    for I := 1 to Points.Count do
    begin
      Point := Points[I - 1];
      P.Z := 0.0;
      if lvProfile in views then
      begin
        P.X := FProfileOrigin.X + Point.Coordinate.X;
        P.Y := FProfileOrigin.Y + Point.Coordinate.Z;
      end;
      if lvAftBody in views then
      begin
        P.X := FAftOrigin.X - Point.Coordinate.Y;
        P.Y := FAftOrigin.Y + Point.Coordinate.Z;
      end;
      if lvFrontBody in views then
      begin
        P.X := FFrontOrigin.X - Point.Coordinate.Y;
        P.Y := FFrontOrigin.Y + Point.Coordinate.Z;
      end;
      if lvPlan in views then
      begin
        P.X := FPlanOrigin.X + Point.Coordinate.X;
        P.Y := FPlanOrigin.Y + Point.Coordinate.Y;
      end;
      Strings.Add('0' + EOL + 'VERTEX');
      Strings.Add('8' + EOL + LayerName);
      Strings.Add('10' + EOL + FloatToDec(P.X, 4));
      Strings.Add('20' + EOL + FloatToDec(P.Y, 4));
    end;
    Strings.Add('0' + EOL + 'SEQEND');

    if (lvAftBody in views) or (lvFrontBody in views) or
      ((lvPlan in views) and (Freeship.NumberofDiagonals = 0) and (False)) then
    begin
      // mirror line
      Strings.Add('0' + EOL + 'POLYLINE');
      Strings.Add('8' + EOL + LayerName);   // layername
      Strings.Add('62' + EOL + IntToStr(FindDXFColorIndex(Color)));
      Strings.Add('66' + EOL + '1');    // vertices follow
      for I := 1 to Points.Count do
      begin
        Point := Points[I - 1];
        P.Z := 0.0;
        if lvAftBody in views then
        begin
          P.X := FAftOrigin.X + Point.Coordinate.Y;
          P.Y := FAftOrigin.Y + Point.Coordinate.Z;
        end;
        if lvFrontBody in views then
        begin
          P.X := FFrontOrigin.X + Point.Coordinate.Y;
          P.Y := FFrontOrigin.Y + Point.Coordinate.Z;
        end;
        if lvPlan in views then
        begin
          P.X := FPlanOrigin.X + Point.Coordinate.X;
          P.Y := FPlanOrigin.Y - Point.Coordinate.Y;
        end;

        Strings.Add('0' + EOL + 'VERTEX');
        Strings.Add('8' + EOL + LayerName);
        Strings.Add('10' + EOL + FloatToDec(P.X, 4));
        Strings.Add('20' + EOL + FloatToDec(P.Y, 4));
      end;
      Strings.Add('0' + EOL + 'SEQEND');
    end;
  end;{AddSpline}

  procedure AddIntersection(Intersection: TFreeIntersection;
    Views: TLinesplanViews; Layername: string; Color: TColor);
  var
    I: integer;
  begin
    if not Intersection.Built then
      Intersection.Rebuild;
    for I := 1 to Intersection.Count do
      AddSpline(Intersection.Items[I - 1], Views, Layername, Color);
  end;
  {AddIntersection}
begin
    SplitSectionLocation := Freeship.ProjectSettings.ProjectSplitSectionLocation;

    EdgePointLists := TFasterListTFasterListTFreeSubdivisionPoint.Create;
    if ParamStr(1)='--serve' then ExtractStableEdgeLoops(Freeship.Surface,EdgePointLists)
    else Freeship.Surface.ExtractAllEdgeLoops(EdgePointLists);
    Strings := TStringList.Create;
    Strings.Add('0' + EOL + 'SECTION');
    Strings.Add('2' + EOL + 'ENTITIES');
    // PROFILE VIEW
    Space := CalculateSpace(0.5 * textspace, FMin3D.X, FMax3D.X);
    // draw baseline
    AddLine(SetPoint(FMin3D.X - Space, FMin3D.Z, 0), SetPoint(
      FMax3D.X + Space, FMin3D.Z, 0), [lvProfile], rs_Base {UserString[184]}, Freeship.Preferences.GridColor);
    // draw dwl
    AddLine(SetPoint(FMin3D.X - Space, 0, FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft),
      SetPoint(FMax3D.X + Space, 0, FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft),
      [lvProfile], rs_DWL {UserString[185]}, clRed);
    for I := 1 to FFreeship.NumberofWaterlines do
    begin
      Tmp := -FFreeship.Waterline[I - 1].Plane.D;
      AddLine(SetPoint(FMin3D.X - Space, 0, Tmp), SetPoint(FMax3D.X + Space, 0, Tmp),
        [lvProfile], 'wlgrid', Freeship.Preferences.GridColor);
    end;
    Space := CalculateSpace(textspace, FMin3D.Z, FMax3D.Z);
    for I := 1 to FFreeship.NumberofStations do
    begin
      Tmp := -FFreeship.Station[I - 1].Plane.D;
      AddLine(SetPoint(Tmp, 0, FMin3D.Z - space), SetPoint(Tmp, 0, FMax3D.Z + space),
        [lvProfile], 'stationgrid', Freeship.Preferences.GridColor);
    end;
    // draw buttocks
    for I := 1 to Freeship.NumberofButtocks do
      AddIntersection(Freeship.Buttock[I - 1], [lvProfile], 'buttocks',
        Freeship.Preferences.ButtockColor);
    // Add knuckle lines
    for I := 1 to EdgePointLists.Count do
    begin
      Points:=EdgePointLists[I-1];
      //Points := TFasterListTFreeSubdivisionPoint.Create;
      //Points.Add(EdgePointLists[I - 1].StartPoint);
      //Points.Add(EdgePointLists[I - 1].EndPoint);
      AddEdgeLoop(Points, [lvProfile], 'Knuckle_lines',
        Freeship.Preferences.CreaseColor);
    end;

    // AFT VIEW OF BODYPLAN
    Space := CalculateSpace(0.5 * textspace, -FMax3D.Y, FMax3D.Y);
    // draw baseline
    AddLine(SetPoint(0.0, -FMax3D.Y - Space, 0.0), SetPoint(0.0, FMax3D.Y + Space, 0.0),
      [lvAftBody], rs_Base {UserString[184]}, Freeship.Preferences.GridColor);
    // draw dwl
    AddLine(SetPoint(0.0, -FMax3D.Y - Space, FMin3D.Z +
      FFreeship.ProjectSettings.ProjectDraft), SetPoint(0.0, FMax3D.Y + Space,
      FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft), [lvAftBody], rs_DWL {UserString[185]}, clRed);
    for I := 1 to FFreeship.NumberofWaterlines do
    begin
      Tmp := -FFreeship.Waterline[I - 1].Plane.D;
      AddLine(SetPoint(0.0, -FMax3D.Y - Space, Tmp), SetPoint(
        0.0, FMax3D.Y + Space, Tmp), [lvAftBody], 'wlgrid', Freeship.Preferences.GridColor);
    end;
    Space := CalculateSpace(textspace, FMin3D.Z, FMax3D.Z);
    // draw centerline
    AddLine(SetPoint(0.0, 0.0, FMin3D.Z - space), SetPoint(0.0, 0.0, FMax3D.Z + space),
      [lvAftBody], rs_Center {UserString[245]}, clRed);
    for I := 1 to FFreeship.NumberofButtocks do
    begin
      Tmp := -FFreeship.Buttock[I - 1].Plane.D;
      AddLine(SetPoint(0.0, Tmp, FMin3D.Z - space), SetPoint(
        0.0, Tmp, FMax3D.Z + space), [lvAftBody], 'buttockgrid', Freeship.Preferences.GridColor);
      AddLine(SetPoint(0.0, -Tmp, FMin3D.Z - space), SetPoint(
        0.0, -Tmp, FMax3D.Z + space), [lvAftBody], 'buttockgrid', Freeship.Preferences.GridColor);
    end;
    // draw stations
    for I := 1 to Freeship.NumberofStations do
      if -Freeship.Station[I - 1].Plane.d <= SplitSectionLocation then
        AddIntersection(Freeship.Station[I - 1], [lvAftBody], 'stations',
          Freeship.Preferences.StationColor);
    // Add knuckle lines
    for I := 1 to FreeShip.Surface.NumberOfSubDivEdges do
    begin
      Edge := FreeShip.Surface.SubDivEdge[I - 1];
      if (Edge.Crease) and (Edge.StartPoint.Coordinate.X <= SplitSectionLocation) and
        (Edge.EndPoint.Coordinate.X <= SplitSectionLocation) then
      begin
        P1 := Edge.StartPoint.Coordinate;
        P2 := Edge.EndPoint.Coordinate;
        AddLine(P1, P2, [lvAftBody], 'Knuckle_lines', Freeship.Preferences.CreaseColor);
        P1.Y := -P1.Y;
        P2.Y := -P2.Y;
        AddLine(P1, P2, [lvAftBody], 'Knuckle_lines', Freeship.Preferences.CreaseColor);
      end;
    end;

    // FRONT VIEW OF BODYPLAN
    Space := CalculateSpace(0.5 * textspace, -FMax3D.Y, FMax3D.Y);
    // draw baseline
    AddLine(SetPoint(0.0, -FMax3D.Y - Space, 0.0), SetPoint(0.0, FMax3D.Y + Space, 0.0),
      [lvFrontBody], rs_Base {UserString[184]}, Freeship.Preferences.GridColor);
    // draw dwl
    AddLine(SetPoint(0.0, -FMax3D.Y - Space, FMin3D.Z +
      FFreeship.ProjectSettings.ProjectDraft), SetPoint(0.0, FMax3D.Y + Space,
      FMin3D.Z + FFreeship.ProjectSettings.ProjectDraft), [lvFrontBody], rs_DWL {UserString[185]}, clRed);
    for I := 1 to FFreeship.NumberofWaterlines do
    begin
      Tmp := -FFreeship.Waterline[I - 1].Plane.D;
      AddLine(SetPoint(0.0, -FMax3D.Y - Space, Tmp), SetPoint(
        0.0, FMax3D.Y + Space, Tmp), [lvFrontBody], 'wlgrid', Freeship.Preferences.GridColor);
    end;
    Space := CalculateSpace(textspace, FMin3D.Z, FMax3D.Z);
    // draw centerline
    AddLine(SetPoint(0.0, 0.0, FMin3D.Z - space), SetPoint(0.0, 0.0, FMax3D.Z + space),
      [lvFrontBody], rs_Center {UserString[245]}, clRed);
    for I := 1 to FFreeship.NumberofButtocks do
    begin
      Tmp := -FFreeship.Buttock[I - 1].Plane.D;
      AddLine(SetPoint(0.0, Tmp, FMin3D.Z - space), SetPoint(
        0.0, Tmp, FMax3D.Z + space), [lvFrontBody], 'buttockgrid', Freeship.Preferences.GridColor);
      AddLine(SetPoint(0.0, -Tmp, FMin3D.Z - space), SetPoint(
        0.0, -Tmp, FMax3D.Z + space), [lvFrontBody], 'buttockgrid', Freeship.Preferences.GridColor);
    end;
    // draw stations
    for I := 1 to Freeship.NumberofStations do
      if -Freeship.Station[I - 1].Plane.d >= SplitSectionLocation then
        AddIntersection(Freeship.Station[I - 1], [lvFrontBody], 'stations',
          Freeship.Preferences.StationColor);
    // Add knuckle lines
    for I := 1 to FreeShip.Surface.NumberOfSubDivEdges do
    begin
      Edge := FreeShip.Surface.SubDivEdge[I - 1];
      if (Edge.Crease) and (Edge.StartPoint.Coordinate.X >= SplitSectionLocation) and
        (Edge.EndPoint.Coordinate.X >= SplitSectionLocation) then
      begin
        P1 := Edge.StartPoint.Coordinate;
        P2 := Edge.EndPoint.Coordinate;
        AddLine(P1, P2, [lvFrontBody], 'Knuckle_lines',
          Freeship.Preferences.CreaseColor);
        P1.Y := -P1.Y;
        P2.Y := -P2.Y;
        AddLine(P1, P2, [lvFrontBody], 'Knuckle_lines',
          Freeship.Preferences.CreaseColor);
      end;
    end;
    // PLAN VIEW
    Space := CalculateSpace(0.5 * textspace, FMin3D.X, FMax3D.X);
    // draw centerline
    AddLine(SetPoint(FMin3D.X - Space, 0.0, 0.0), SetPoint(FMax3D.X + Space, 0.0, 0.0),
      [lvPlan], rs_Center {UserString[245]}, clRed);
    for I := 1 to FFreeship.NumberofButtocks do
    begin
      Tmp := -FFreeship.Buttock[I - 1].Plane.D;
      AddLine(SetPoint(FMin3D.X - Space, Tmp, 0.0), SetPoint(
        FMax3D.X + space, Tmp, 0.0), [lvPlan], 'buttockgrid', Freeship.Preferences.GridColor);
      if (Freeship.NumberofDiagonals = 0) and (False) then
        AddLine(SetPoint(FMin3D.X - space, -Tmp, 0.0), SetPoint(FMax3D.X + space, -Tmp, 0.0),
          [lvPlan], 'buttockgrid', Freeship.Preferences.GridColor);
    end;
    // stations
    Space := CalculateSpace(textspace, FMin3D.Y, FMax3D.Y);
    for I := 1 to FFreeship.NumberofStations do
    begin
      Tmp := -FFreeship.Station[I - 1].Plane.D;
      if (Freeship.NumberofDiagonals = 0) and (False) then
        AddLine(SetPoint(Tmp, -FMax3D.Y - space, 0), SetPoint(Tmp, FMax3D.Y + space, 0),
          [lvPlan], 'stationgrid', Freeship.Preferences.GridColor)
      else
        AddLine(SetPoint(Tmp, -FDiagonalWidth - space, 0), SetPoint(Tmp, FMax3D.Y + space, 0),
          [lvPlan], 'stationgrid', Freeship.Preferences.GridColor);
    end;
    // draw waterlines
    for I := 1 to Freeship.NumberofWaterlines do
      AddIntersection(Freeship.Waterline[I - 1], [lvPLan], 'waterlines',
        Freeship.Preferences.WaterlineColor);

    // draw diagonals
    for I := 1 to Freeship.NumberofDiagonals do
    begin
      Diagonal := Freeship.Diagonal[I - 1];
      Plane.a := 0.0;
      Plane.b := 1 / Sqrt(2);
      Plane.c := -1 / sqrt(2);
      Plane.d := -Diagonal.Plane.d;
      if not Diagonal.Built then
        Diagonal.Rebuild;
      for J := 1 to Diagonal.Count do
      begin
        Spline := Diagonal.Items[J - 1];
        Min := 0;
        Max := 0;
        Strings.Add('0' + EOL + 'POLYLINE');
        Strings.Add('8' + EOL + 'Diagonals');   // layername
        Strings.Add('62' + EOL + IntToStr(FindDXFColorIndex(
          Freeship.Preferences.DiagonalColor)));
        Strings.Add('66' + EOL + '1');    // vertices follow

        Spline.Fragments:=100;
        SplineValues := Spline.GetValues;
        for k := 0 to length(SplineValues)-1 do
        begin
          //P1 := Spline.Value(K / 100, Pn1, Pn2);
          P1:=SplineValues[k];
          Tmp := abs(Plane.A * P1.x + Plane.B * P1.y + Plane.C * P1.z + Plane.D);
          if K = 0 then
          begin
            Min := Tmp;
            Max := Tmp;
          end
          else
          begin
            if Tmp < min then
              Min := Tmp;
            if Tmp > max then
              Max := Tmp;
          end;
          P2.X := FPlanOrigin.X + P1.X;
          P2.Y := FPlanOrigin.Y - abs(Tmp);
          P2.Z := 0.0;
          Strings.Add('0' + EOL + 'VERTEX');
          Strings.Add('8' + EOL + 'Diagonals');
          Strings.Add('10' + EOL + FloatToDec(P2.X, 4));
          Strings.Add('20' + EOL + FloatToDec(P2.Y, 4));
        end;
        Strings.Add('0' + EOL + 'SEQEND');
        // draw as grid in bodyplan views
        // calculate height of intersection of diagonal with centerplane
        Tmp := -Diagonal.Plane.d / Diagonal.Plane.c;
        P1 := SetPoint(0.0, Min * Sin(DegToRad(45)), Tmp - Min * Sin(DegToRad(45)));
        P2 := SetPoint(0.0, Max * Sin(DegToRad(45)), Tmp - Max * Sin(DegToRad(45)));
        AddLine(P1, P2, [lvAftBody], 'diagonalgrid', freeship.Preferences.GridColor);
        AddLine(P1, P2, [lvFrontBody], 'diagonalgrid', freeship.Preferences.GridColor);
        P1.Y := -P1.Y;
        P2.Y := -P2.Y;
        AddLine(P1, P2, [lvAftBody], 'diagonalgrid', freeship.Preferences.GridColor);
        AddLine(P1, P2, [lvFrontBody], 'diagonalgrid', freeship.Preferences.GridColor);
      end;
    end;
    // Add knuckle lines
    for I := 1 to EdgePointLists.Count do
    begin
      Points:=EdgePointLists[I-1];
      AddEdgeLoop(Points, [lvPlan], 'Knuckle_lines', Freeship.Preferences.CreaseColor);
    end;

    Strings.Add('0' + EOL + 'ENDSEC');
    Strings.Add('0' + EOL + 'EOF');
    Strings.SaveToFile(Filename);
    FreeAndNil(Strings);

    // Destroy extracted edgeloops
    for I := 1 to EdgePointLists.Count do
    begin
      Points:=EdgePointLists[I-1];
      FreeAndNil(Points);
    end;
    FreeAndNil(EdgePointLists);
end;

end.
