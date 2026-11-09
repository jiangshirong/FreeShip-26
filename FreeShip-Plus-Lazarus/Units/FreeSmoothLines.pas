unit FreeSmoothLines;

{$mode objfpc}{$H+}

interface

uses Graphics, Types;

type TSmoothPoint = record X, Y: Double; end;
  TSmoothSegment=record A,B:TSmoothPoint end;

// Returns false when the native renderer should be used instead.
function DrawSmoothSegmentPath(Canvas:TCanvas; const Segments:array of TSmoothSegment):Boolean;
function DrawSmoothSegments(Canvas:TCanvas; const Segments:array of TSmoothSegment):Boolean;
function DrawSmoothLine(Canvas: TCanvas; X1, Y1, X2, Y2: Double): Boolean;
function DrawSmoothPolyline(Canvas: TCanvas; const Points: array of TPoint): Boolean;
function DrawSmoothFloatPolyline(Canvas: TCanvas; const Points: array of TSmoothPoint): Boolean;

implementation

{$IFDEF MSWINDOWS}
uses Windows, SysUtils;

{$PUSH}{$PACKRECORDS C}
type
  TGdiPlusStartupInput = record
    GdiplusVersion: LongWord;
    DebugEventCallback: Pointer;
    SuppressBackgroundThread: LongBool;
    SuppressExternalCodecs: LongBool;
  end;
  TGdiPointF = record X, Y: Single; end;
{$POP}

function GdiplusStartup(out Token: PtrUInt; const Input: TGdiPlusStartupInput;
  Output: Pointer): Integer; stdcall; external 'gdiplus.dll';
procedure GdiplusShutdown(Token: PtrUInt); stdcall; external 'gdiplus.dll';
function GdipCreateFromHDC(DC: HDC; out Graphics: Pointer): Integer; stdcall; external 'gdiplus.dll';
function GdipDeleteGraphics(Graphics: Pointer): Integer; stdcall; external 'gdiplus.dll';
function GdipSetSmoothingMode(Graphics: Pointer; Mode: Integer): Integer; stdcall; external 'gdiplus.dll';
function GdipCreatePen1(Color: LongWord; Width: Single; UnitType: Integer; out Pen: Pointer): Integer; stdcall; external 'gdiplus.dll';
function GdipDeletePen(Pen: Pointer): Integer; stdcall; external 'gdiplus.dll';
function GdipSetPenStartCap(Pen: Pointer; Cap: Integer): Integer; stdcall; external 'gdiplus.dll';
function GdipSetPenEndCap(Pen: Pointer; Cap: Integer): Integer; stdcall; external 'gdiplus.dll';
function GdipSetPenLineJoin(Pen: Pointer; LineJoin: Integer): Integer; stdcall; external 'gdiplus.dll';
function GdipDrawLines(Graphics, Pen, Points: Pointer; Count: Integer): Integer; stdcall; external 'gdiplus.dll';

function GdipCreatePath2(Points,PointTypes:Pointer;Count,FillMode:Integer;out Path:Pointer):Integer;stdcall;external 'gdiplus.dll';
function GdipDrawPath(Graphics,Pen,Path:Pointer):Integer;stdcall;external 'gdiplus.dll';
function GdipDeletePath(Path:Pointer):Integer;stdcall;external 'gdiplus.dll';

var GdiPlusToken: PtrUInt = 0;

function Render(Canvas: TCanvas; const Points: array of TGdiPointF): Boolean;
var GraphicsHandle, PenHandle: Pointer;
    ColorValue, Argb: LongWord;
    Width: Single;
begin
  Result := False;
  if (GdiPlusToken = 0) or (Canvas = nil) or (Length(Points) < 2) or
     (Canvas.Pen.Style <> psSolid) or (Canvas.Pen.Mode <> pmCopy) then Exit;
  GraphicsHandle := nil;
  PenHandle := nil;
  if GdipCreateFromHDC(HDC(Canvas.Handle), GraphicsHandle) <> 0 then Exit;
  try
    if GdipSetSmoothingMode(GraphicsHandle, 4) <> 0 then Exit;
    ColorValue := LongWord(ColorToRGB(Canvas.Pen.Color));
    Argb := $FF000000 or ((ColorValue and $FF) shl 16) or
      (ColorValue and $FF00) or ((ColorValue shr 16) and $FF);
    Width := Canvas.Pen.Width;
    if Width < 1 then Width := 1;
    if GdipCreatePen1(Argb, Width, 2, PenHandle) <> 0 then Exit;
    try
      GdipSetPenStartCap(PenHandle, 2);
      GdipSetPenEndCap(PenHandle, 2);
      GdipSetPenLineJoin(PenHandle, 2);
      Result := GdipDrawLines(GraphicsHandle, PenHandle, @Points[0], Length(Points)) = 0;
    finally
      GdipDeletePen(PenHandle);
    end;
  finally
    GdipDeleteGraphics(GraphicsHandle);
  end;
end;

function DrawSmoothSegments(Canvas:TCanvas; const Segments:array of TSmoothSegment):Boolean;
var GraphicsHandle,PenHandle:Pointer;ColorValue,Argb:LongWord;Width:Single;
 I:Integer;Points:array[0..1]of TGdiPointF;
begin
 Result:=False;
 if (GdiPlusToken=0) or (Canvas=nil) or (Length(Segments)=0) or
    (Canvas.Pen.Style<>psSolid) or (Canvas.Pen.Mode<>pmCopy) then Exit;
 for I:=0 to High(Segments) do if (Abs(Segments[I].A.X)>1000000) or (Abs(Segments[I].A.Y)>1000000) or
    (Abs(Segments[I].B.X)>1000000) or (Abs(Segments[I].B.Y)>1000000) then Exit;
 GraphicsHandle:=nil;PenHandle:=nil;
 if GdipCreateFromHDC(HDC(Canvas.Handle),GraphicsHandle)<>0 then Exit;
 try
  if GdipSetSmoothingMode(GraphicsHandle,4)<>0 then Exit;
  ColorValue:=LongWord(ColorToRGB(Canvas.Pen.Color));
  Argb:=$FF000000 or ((ColorValue and $FF) shl 16) or (ColorValue and $FF00) or ((ColorValue shr 16) and $FF);
  Width:=Max(1,Canvas.Pen.Width);
  if GdipCreatePen1(Argb,Width,2,PenHandle)<>0 then Exit;
  try
   GdipSetPenStartCap(PenHandle,2);GdipSetPenEndCap(PenHandle,2);GdipSetPenLineJoin(PenHandle,2);
   for I:=0 to High(Segments) do begin
    Points[0].X:=Segments[I].A.X;Points[0].Y:=Segments[I].A.Y;
    Points[1].X:=Segments[I].B.X;Points[1].Y:=Segments[I].B.Y;
    if GdipDrawLines(GraphicsHandle,PenHandle,@Points[0],2)<>0 then Exit;
   end;
   Result:=True;
  finally GdipDeletePen(PenHandle) end;
 finally GdipDeleteGraphics(GraphicsHandle) end;
end;

function DrawSmoothSegmentPath(Canvas:TCanvas; const Segments:array of TSmoothSegment):Boolean;
var GraphicsHandle,PenHandle:Pointer;ColorValue,Argb:LongWord;Width:Single;
 I:Integer;Points:array of TGdiPointF;PointTypes:array of Byte;Path:Pointer;
begin
 Result:=False;
 if (GdiPlusToken=0) or (Canvas=nil) or (Length(Segments)=0) or
    (Canvas.Pen.Style<>psSolid) or (Canvas.Pen.Mode<>pmCopy) then Exit;
 for I:=0 to High(Segments) do if (Abs(Segments[I].A.X)>1000000) or (Abs(Segments[I].A.Y)>1000000) or
    (Abs(Segments[I].B.X)>1000000) or (Abs(Segments[I].B.Y)>1000000) then Exit;
 GraphicsHandle:=nil;PenHandle:=nil;
 if GdipCreateFromHDC(HDC(Canvas.Handle),GraphicsHandle)<>0 then Exit;
 try
  if GdipSetSmoothingMode(GraphicsHandle,4)<>0 then Exit;
  ColorValue:=LongWord(ColorToRGB(Canvas.Pen.Color));
  Argb:=$FF000000 or ((ColorValue and $FF) shl 16) or (ColorValue and $FF00) or ((ColorValue shr 16) and $FF);
  Width:=Max(1,Canvas.Pen.Width);
  if GdipCreatePen1(Argb,Width,2,PenHandle)<>0 then Exit;
  try
   GdipSetPenStartCap(PenHandle,2);GdipSetPenEndCap(PenHandle,2);GdipSetPenLineJoin(PenHandle,2);
   SetLength(Points,2*Length(Segments));SetLength(PointTypes,Length(Points));
   for I:=0 to High(Segments) do begin
    Points[2*I].X:=Segments[I].A.X;Points[2*I].Y:=Segments[I].A.Y;
    Points[2*I+1].X:=Segments[I].B.X;Points[2*I+1].Y:=Segments[I].B.Y;
    PointTypes[2*I]:=0;PointTypes[2*I+1]:=1;
   end;
   Path:=nil;
   if GdipCreatePath2(@Points[0],@PointTypes[0],Length(Points),0,Path)<>0 then Exit;
   try Result:=GdipDrawPath(GraphicsHandle,PenHandle,Path)=0
   finally GdipDeletePath(Path) end;
  finally GdipDeletePen(PenHandle) end;
 finally GdipDeleteGraphics(GraphicsHandle) end;
end;

function DrawSmoothLine(Canvas: TCanvas; X1, Y1, X2, Y2: Double): Boolean;
var Points: array[0..1] of TGdiPointF;
begin
  // Extremely distant projected coordinates can overflow GDI+'s rasterizer.
  if (Abs(X1) > 1000000) or (Abs(Y1) > 1000000) or
     (Abs(X2) > 1000000) or (Abs(Y2) > 1000000) then Exit(False);
  Points[0].X := X1; Points[0].Y := Y1;
  Points[1].X := X2; Points[1].Y := Y2;
  Result := Render(Canvas, Points);
end;

function DrawSmoothPolyline(Canvas: TCanvas; const Points: array of TPoint): Boolean;
var I: Integer; FloatPoints: array of TGdiPointF;
begin
  Result := False;
  if Length(Points) < 2 then Exit;
  SetLength(FloatPoints, Length(Points));
  for I := 0 to High(Points) do
  begin
    if (Abs(Int64(Points[I].X)) > 1000000) or (Abs(Int64(Points[I].Y)) > 1000000) then Exit;
    FloatPoints[I].X := Points[I].X;
    FloatPoints[I].Y := Points[I].Y;
  end;
  Result := Render(Canvas, FloatPoints);
end;

function DrawSmoothFloatPolyline(Canvas: TCanvas; const Points: array of TSmoothPoint): Boolean;
var I: Integer; FloatPoints: array of TGdiPointF;
begin
  Result := False;
  if Length(Points) < 2 then Exit;
  SetLength(FloatPoints, Length(Points));
  for I := 0 to High(Points) do
  begin
    if (Abs(Points[I].X) > 1000000) or (Abs(Points[I].Y) > 1000000) then Exit;
    FloatPoints[I].X := Points[I].X;
    FloatPoints[I].Y := Points[I].Y;
  end;
  Result := Render(Canvas, FloatPoints);
end;

var StartupInput: TGdiPlusStartupInput;
initialization
  FillChar(StartupInput, SizeOf(StartupInput), 0);
  StartupInput.GdiplusVersion := 1;
  if GdiplusStartup(GdiPlusToken, StartupInput, nil) <> 0 then GdiPlusToken := 0;
finalization
  if GdiPlusToken <> 0 then GdiplusShutdown(GdiPlusToken);
{$ELSE}
function DrawSmoothSegmentPath(Canvas:TCanvas; const Segments:array of TSmoothSegment):Boolean;
begin Result := False end;
function DrawSmoothSegments(Canvas:TCanvas; const Segments:array of TSmoothSegment):Boolean;
begin Result:=False end;

function DrawSmoothLine(Canvas: TCanvas; X1, Y1, X2, Y2: Double): Boolean;
begin Result := False end;
function DrawSmoothPolyline(Canvas: TCanvas; const Points: array of TPoint): Boolean;
begin Result := False end;
function DrawSmoothFloatPolyline(Canvas: TCanvas; const Points: array of TSmoothPoint): Boolean;
begin Result := False end;
{$ENDIF}
end.
