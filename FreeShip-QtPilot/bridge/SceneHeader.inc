unit FreeLinesplanScene;
{$mode delphi}{$H+}
// Window-free scene generation, extracted from the original lines-plan algorithms.
// Graphics is used only for a 1x1 font-metrics surface, never hull rasterization.
interface
uses Classes,SysUtils,Types,Graphics,Math,Windows,FreeTypes,FreeGeometry,
 FreeShipUnit,FasterList,FreeStringsUnit,FreeStringUtils,PilotCapture;
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
