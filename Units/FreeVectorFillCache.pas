unit FreeVectorFillCache;
{$mode objfpc}{$H+}
interface
uses Graphics,Types;
type
 TVectorFillCache=class
 private
  FHandle,FDC:PtrUInt;
  FCommands:array of record Points:array[0..2]of TPoint;Color:TColor end;
  FCommandCount:Integer;
  FNativeReady:Boolean;
  FCanvas:TCanvas;
  FAtlas:Graphics.TBitmap;
  FMips:array[1..12]of Graphics.TBitmap;
  FLastDrawMode:string;
  FBackground:TColor;
  FPaints:array of record Color:LongWord;Brush,Pen:PtrUInt end;
  FCurrent,FNext:Integer;
  FMinX,FMinY,FMaxX,FMaxY,FScale:Double;
  procedure ReleasePaints;
  procedure SubmitPolygon(const Points:array of TPoint;Color:TColor);
  procedure EnsureNative;
 public
  destructor Destroy;override;
  procedure Clear;
  function BeginRecord(Reference:TCanvas;MinX,MinY,MaxX,MaxY:Double;Background:TColor=clWhite):Boolean;
  procedure Polygon(const Points:array of TPoint;Color:TColor);
  function EndRecord:Boolean;
  function Project(X,Y:Double):TPoint;
  function Draw(Target:TCanvas;const R:TRect):Boolean;
  function Valid:Boolean;
  function LoadDisk(const Filename:string):Boolean;
  procedure SaveDisk(const Filename:string);
  property LastDrawMode:string read FLastDrawMode;
  property Canvas:TCanvas read FCanvas;
  property MinX:Double read FMinX;
  property MinY:Double read FMinY;
  property MaxX:Double read FMaxX;
  property MaxY:Double read FMaxY;
 end;
implementation
uses Math,SysUtils,Classes,SHA1{$IFDEF MSWINDOWS},Windows{$ENDIF};
{$IFDEF MSWINDOWS}
function BCryptOpenAlgorithmProvider(out Alg:PtrUInt;Name,Provider:PWideChar;Flags:LongWord):LongInt;stdcall;external 'bcrypt.dll';
function BCryptCloseAlgorithmProvider(Alg:PtrUInt;Flags:LongWord):LongInt;stdcall;external 'bcrypt.dll';
function BCryptCreateHash(Alg:PtrUInt;out Hash:PtrUInt;Obj:Pointer;ObjSize:LongWord;Secret:Pointer;SecretSize,Flags:LongWord):LongInt;stdcall;external 'bcrypt.dll';
function BCryptHashData(Hash:PtrUInt;Data:Pointer;Size,Flags:LongWord):LongInt;stdcall;external 'bcrypt.dll';
function BCryptFinishHash(Hash:PtrUInt;Data:Pointer;Size,Flags:LongWord):LongInt;stdcall;external 'bcrypt.dll';
function BCryptDestroyHash(Hash:PtrUInt):LongInt;stdcall;external 'bcrypt.dll';
{$ENDIF}
{$IFDEF MSWINDOWS}
type TCapturedTransform=record Value:TXForm;Ready:Boolean end;
function NativeEnumEnhMetaFile(DC,Meta:PtrUInt;Callback,Data,Bounds:Pointer):LongBool;stdcall;external 'gdi32.dll' name 'EnumEnhMetaFile';
function CaptureMetaTransform(DC:HDC;Table,Rec:Pointer;Count:Integer;Data:PtrInt):Integer;stdcall;
var P:^TCapturedTransform;
begin
 P:=Pointer(Data);P^.Ready:=Windows.GetWorldTransform(DC,P^.Value);Result:=0;
end;
{$ENDIF}
function CacheDigest(Data:Pointer;Size:LongWord):TSHA1Digest;
{$IFDEF MSWINDOWS}var Alg,Hash:PtrUInt;{$ENDIF}
begin
 {$IFDEF MSWINDOWS}
 Alg:=0;Hash:=0;
 if BCryptOpenAlgorithmProvider(Alg,'SHA1',nil,0)=0 then
 try
  if BCryptCreateHash(Alg,Hash,nil,0,nil,0,0)=0 then
  try
   if (BCryptHashData(Hash,Data,Size,0)=0) and
      (BCryptFinishHash(Hash,@Result,SizeOf(Result),0)=0) then Exit;
  finally BCryptDestroyHash(Hash) end;
 finally BCryptCloseAlgorithmProvider(Alg,0) end;
 {$ENDIF}
 Result:=SHA1Buffer(Data^,Size);
end;

destructor TVectorFillCache.Destroy;
begin Clear;inherited Destroy end;
procedure TVectorFillCache.ReleasePaints;
var I:Integer;
begin
 {$IFDEF MSWINDOWS}
 for I:=0 to High(FPaints) do begin
  if FPaints[I].Brush<>0 then Windows.DeleteObject(HGDIOBJ(FPaints[I].Brush));
  if FPaints[I].Pen<>0 then Windows.DeleteObject(HGDIOBJ(FPaints[I].Pen));
 end;
 {$ENDIF}
 SetLength(FPaints,0);FCurrent:=-1;FNext:=0;
end;

procedure TVectorFillCache.Polygon(const Points:array of TPoint;Color:TColor);
var I:Integer;
begin
 if (FDC=0) or (Length(Points)<>3) then Exit;
 if FCommandCount=Length(FCommands) then SetLength(FCommands,FCommandCount+8192);
 for I:=0 to 2 do FCommands[FCommandCount].Points[I]:=Points[I];
 FCommands[FCommandCount].Color:=Color;Inc(FCommandCount);
end;

procedure TVectorFillCache.EnsureNative;
{$IFDEF MSWINDOWS}var DC:HDC;R:TRect;I:Integer;Header:TEnhMetaHeader;{$ENDIF}
begin
 {$IFDEF MSWINDOWS}
 if FNativeReady or (FHandle=0) then Exit;
 FillChar(Header,SizeOf(Header),0);
 if GetEnhMetaFileHeader(HENHMETAFILE(FHandle),SizeOf(Header),@Header)=0 then Exit;
 R:=Types.Rect(Header.rclFrame.Left,Header.rclFrame.Top,Header.rclFrame.Right,Header.rclFrame.Bottom);DC:=CreateEnhMetaFileW(0,nil,@R,nil);if DC=0 then Exit;
 FDC:=PtrUInt(DC);
 for I:=0 to FCommandCount-1 do SubmitPolygon(FCommands[I].Points,FCommands[I].Color);
 DeleteEnhMetaFile(HENHMETAFILE(FHandle));FHandle:=PtrUInt(CloseEnhMetaFile(DC));FDC:=0;ReleasePaints;
 FNativeReady:=FHandle<>0;
 {$ENDIF}
end;

procedure TVectorFillCache.SubmitPolygon(const Points:array of TPoint;Color:TColor);
{$IFDEF MSWINDOWS}var I,J:Integer;RGBColor:LongWord;Pts:array[0..2]of TPoint;{$ENDIF}
begin
 {$IFDEF MSWINDOWS}
 if (FDC=0) or (Length(Points)<>3) then Exit;
 RGBColor:=LongWord(Graphics.ColorToRGB(Color));
 I:=FCurrent;
 if (I<0) or (FPaints[I].Color<>RGBColor) then begin
  I:=-1;
  for J:=0 to High(FPaints) do if FPaints[J].Color=RGBColor then begin I:=J;Break end;
  if I<0 then begin
   if Length(FPaints)<512 then begin I:=Length(FPaints);SetLength(FPaints,I+1) end
   else begin
    I:=FNext;if I=FCurrent then I:=(I+1) mod Length(FPaints);
    FNext:=(I+1) mod Length(FPaints);
    Windows.DeleteObject(HGDIOBJ(FPaints[I].Brush));Windows.DeleteObject(HGDIOBJ(FPaints[I].Pen));
   end;
   FPaints[I].Color:=RGBColor;
   FPaints[I].Brush:=PtrUInt(Windows.CreateSolidBrush(RGBColor));
   FPaints[I].Pen:=PtrUInt(Windows.CreatePen(PS_SOLID,1,RGBColor));
  end;
  Windows.SelectObject(HDC(FDC),HGDIOBJ(FPaints[I].Brush));
  Windows.SelectObject(HDC(FDC),HGDIOBJ(FPaints[I].Pen));FCurrent:=I;
 end;
 for J:=0 to 2 do Pts[J]:=Points[J];
 Windows.Polygon(HDC(FDC),Pts[0],3);
 {$ENDIF}
end;

procedure TVectorFillCache.Clear;
var I:Integer;
{$IFDEF MSWINDOWS}Temp:HENHMETAFILE;{$ENDIF}
begin
 if FCanvas<>nil then begin FCanvas.Handle:=0;FreeAndNil(FCanvas) end;
 {$IFDEF MSWINDOWS}
 if FDC<>0 then begin Temp:=CloseEnhMetaFile(HDC(FDC));if Temp<>0 then DeleteEnhMetaFile(Temp) end;
 if FHandle<>0 then DeleteEnhMetaFile(HENHMETAFILE(FHandle));
 {$ENDIF}
 FCommandCount:=0;SetLength(FCommands,0);FNativeReady:=False;
 FDC:=0;FHandle:=0;ReleasePaints;FreeAndNil(FAtlas);
 for I:=Low(FMips) to High(FMips) do FreeAndNil(FMips[I]);
end;
function TVectorFillCache.BeginRecord(Reference:TCanvas;MinX,MinY,MaxX,MaxY:Double;Background:TColor):Boolean;
{$IFDEF MSWINDOWS}var R:TRect;PW,PH,DX,DY,MX,MY:Integer;{$ENDIF}
begin
 Clear;Result:=False;
 {$IFDEF MSWINDOWS}
 if (Reference=nil) or (MaxX<=MinX) or (MaxY<=MinY) then Exit;
 FMinX:=MinX;FMinY:=MinY;FMaxX:=MaxX;FMaxY:=MaxY;FBackground:=Background;
 // Canonical model-space coordinates, independent of the current window.
 // Replaying these GDI commands rasterizes at the destination resolution.
 FScale:=1000000.0/Math.Max(MaxX-MinX,MaxY-MinY);
 PW:=Math.Max(1,Round((MaxX-MinX)*FScale));PH:=Math.Max(1,Round((MaxY-MinY)*FScale));
 // EMF frame units are hundredths of a physical millimetre. Logical DPI
 // follows Windows scaling and is not the reference device's physical ratio.
 DX:=Math.Max(1,GetDeviceCaps(HDC(Reference.Handle),HORZRES));
 DY:=Math.Max(1,GetDeviceCaps(HDC(Reference.Handle),VERTRES));
 MX:=Math.Max(1,GetDeviceCaps(HDC(Reference.Handle),HORZSIZE));
 MY:=Math.Max(1,GetDeviceCaps(HDC(Reference.Handle),VERTSIZE));
 R:=Types.Rect(0,0,Round(PW*(MX*100.0)/DX),Round(PH*(MY*100.0)/DY));
 FDC:=PtrUInt(CreateEnhMetaFileW(HDC(Reference.Handle),nil,@R,nil));
 if FDC=0 then Exit;
 FCurrent:=-1;FNext:=0;
 FCanvas:=TCanvas.Create;FCanvas.Handle:=HDC(FDC);
 FCanvas.Pen.Cosmetic:=True;FCanvas.Pen.Width:=1;FCanvas.Pen.Style:=psSolid;
 FCanvas.Pen.Mode:=pmCopy;FCanvas.Brush.Style:=bsSolid;
 Result:=True;
 {$ENDIF}
end;
function TVectorFillCache.EndRecord:Boolean;
begin
 Result:=False;
 {$IFDEF MSWINDOWS}
 if FDC=0 then Exit;
 if FCanvas<>nil then begin FCanvas.Handle:=0;FreeAndNil(FCanvas) end;
 FHandle:=PtrUInt(CloseEnhMetaFile(HDC(FDC)));FDC:=0;ReleasePaints;
 Result:=FHandle<>0;
 {$ENDIF}
end;
function TVectorFillCache.LoadDisk(const Filename:string):Boolean;
{$IFDEF MSWINDOWS}
var S:TMemoryStream;F:TFileStream;Digest,Actual:TSHA1Digest;Magic,N:LongWord;Info:TBitmapInfo;Bits:Pointer;Bmp:HBITMAP;W,H:LongInt;
{$ENDIF}
begin
 Result:=False;
 {$IFDEF MSWINDOWS}
 S:=TMemoryStream.Create;
 try
  try
   if not FileExists(Filename) then Exit;
   F:=TFileStream.Create(Filename,fmOpenRead or fmShareDenyWrite);
   try if (F.Size<100) or (F.Size>80000000) then Exit;S.CopyFrom(F,0) finally F.Free end;
   S.Position:=S.Size-SizeOf(Digest);S.ReadBuffer(Digest,SizeOf(Digest));
   Actual:=CacheDigest(S.Memory,S.Size-SizeOf(Digest));
   if not CompareMem(@Digest,@Actual,SizeOf(Digest)) then Exit;
   S.Position:=0;S.ReadBuffer(Magic,SizeOf(Magic));if Magic<>$3243464C then Exit;
   Clear;
   S.ReadBuffer(FMinX,SizeOf(Double));S.ReadBuffer(FMinY,SizeOf(Double));
   S.ReadBuffer(FMaxX,SizeOf(Double));S.ReadBuffer(FMaxY,SizeOf(Double));
   S.ReadBuffer(FScale,SizeOf(Double));S.ReadBuffer(FBackground,SizeOf(FBackground));
   S.ReadBuffer(N,SizeOf(N));if (N=0) or (N>S.Size-S.Position-SizeOf(Digest)) then Exit;
   FHandle:=PtrUInt(SetEnhMetaFileBits(N,PByte(S.Memory)+S.Position));
   if FHandle=0 then Exit;
   S.Position:=S.Position+N;
   S.ReadBuffer(W,SizeOf(W));S.ReadBuffer(H,SizeOf(H));
   if (W<1) or (H<1) or (Int64(W)*H>12000000) or
     (Int64(W)*H*4<>S.Size-S.Position-SizeOf(Digest)) then begin Clear;Exit end;
   FillChar(Info,SizeOf(Info),0);Info.bmiHeader.biSize:=SizeOf(TBitmapInfoHeader);
   Info.bmiHeader.biWidth:=W;Info.bmiHeader.biHeight:=-H;
   Info.bmiHeader.biPlanes:=1;Info.bmiHeader.biBitCount:=32;Info.bmiHeader.biCompression:=BI_RGB;
   Bmp:=CreateDIBSection(0,Info,DIB_RGB_COLORS,Bits,0,0);if Bmp=0 then begin Clear;Exit end;
   Move((PByte(S.Memory)+S.Position)^,Bits^,W*H*4);
   FAtlas:=Graphics.TBitmap.Create;FAtlas.Handle:=Bmp;
   if (FAtlas.Width<1) or (FAtlas.Height<1) or (Int64(FAtlas.Width)*FAtlas.Height>12000000) then begin Clear;Exit end;
   FNativeReady:=True;Result:=True;
  except Clear;Result:=False end;
 finally S.Free end;
 {$ENDIF}
end;

procedure TrimFillCache(const Directory,Keep:string);
var R:TSearchRec;Total:Int64;Oldest,Path:string;OldTime:LongInt;
begin
 repeat
  Total:=0;Oldest:='';OldTime:=High(LongInt);
  if FindFirst(IncludeTrailingPathDelimiter(Directory)+'*.fsc',faAnyFile,R)=0 then
  try
   repeat
    if (R.Attr and faDirectory)<>0 then Continue;
    Inc(Total,R.Size);Path:=IncludeTrailingPathDelimiter(Directory)+R.Name;
    if (Path<>Keep) and (R.Time<OldTime) then begin OldTime:=R.Time;Oldest:=Path end;
   until FindNext(R)<>0;
  finally SysUtils.FindClose(R) end;
  if (Total<=256*1024*1024) or (Oldest='') then Exit;
  if not SysUtils.DeleteFile(Oldest) then Exit;
 until False;
end;

procedure TVectorFillCache.SaveDisk(const Filename:string);
{$IFDEF MSWINDOWS}
var S:TMemoryStream;Digest:TSHA1Digest;Magic,N:LongWord;Bytes:array of Byte;Tmp:string;Info:TBitmapInfo;W,H:LongInt;
{$ENDIF}
begin
 {$IFDEF MSWINDOWS}
 EnsureNative;
 if (FHandle=0) or (FAtlas=nil) then Exit;
 S:=TMemoryStream.Create;
 try
  try
   ForceDirectories(ExtractFilePath(Filename));
   Magic:=$3243464C;S.WriteBuffer(Magic,SizeOf(Magic));
   S.WriteBuffer(FMinX,SizeOf(Double));S.WriteBuffer(FMinY,SizeOf(Double));
   S.WriteBuffer(FMaxX,SizeOf(Double));S.WriteBuffer(FMaxY,SizeOf(Double));
   S.WriteBuffer(FScale,SizeOf(Double));S.WriteBuffer(FBackground,SizeOf(FBackground));
   N:=GetEnhMetaFileBits(HENHMETAFILE(FHandle),0,nil);if N=0 then Exit;
   SetLength(Bytes,N);if GetEnhMetaFileBits(HENHMETAFILE(FHandle),N,@Bytes[0])<>N then Exit;
   S.WriteBuffer(N,SizeOf(N));S.WriteBuffer(Bytes[0],N);
   W:=FAtlas.Width;H:=FAtlas.Height;S.WriteBuffer(W,SizeOf(W));S.WriteBuffer(H,SizeOf(H));
   FillChar(Info,SizeOf(Info),0);Info.bmiHeader.biSize:=SizeOf(TBitmapInfoHeader);
   Info.bmiHeader.biWidth:=W;Info.bmiHeader.biHeight:=-H;
   Info.bmiHeader.biPlanes:=1;Info.bmiHeader.biBitCount:=32;Info.bmiHeader.biCompression:=BI_RGB;
   SetLength(Bytes,W*H*4);
   if GetDIBits(HDC(FAtlas.Canvas.Handle),HBITMAP(FAtlas.Handle),0,H,@Bytes[0],Info,DIB_RGB_COLORS)<>H then Exit;
   S.WriteBuffer(Bytes[0],Length(Bytes));
   if S.Size+SizeOf(Digest)>80000000 then Exit;
   Digest:=CacheDigest(S.Memory,S.Size);S.WriteBuffer(Digest,SizeOf(Digest));
   Tmp:=Filename+'.'+IntToStr(GetCurrentProcessId)+'.tmp';S.SaveToFile(Tmp);
   if not MoveFileExW(PWideChar(UTF8Decode(Tmp)),PWideChar(UTF8Decode(Filename)),MOVEFILE_REPLACE_EXISTING) then SysUtils.DeleteFile(Tmp);
   TrimFillCache(ExtractFilePath(Filename),Filename);
  except {Display caches are optional; saving the model must never depend on them.} end;
 finally S.Free end;
 {$ENDIF}
end;

function TVectorFillCache.Project(X,Y:Double):TPoint;
begin Result:=Types.Point(Round((X-FMinX)*FScale),Round((FMaxY-Y)*FScale)) end;
function TVectorFillCache.Valid:Boolean;
begin Result:=FHandle<>0 end;
function TVectorFillCache.Draw(Target:TCanvas;const R:TRect):Boolean;
{$IFDEF MSWINDOWS}var W,H,Saved,Level,NW,NH,I:Integer;Factor:Double;Bounds:TRect;Source:Graphics.TBitmap;Header:TEnhMetaHeader;Transform:TXForm;Captured:TCapturedTransform;{$ENDIF}
begin
 Result:=False;
 {$IFDEF MSWINDOWS}
 if (FHandle=0) or (Target=nil) or (R.Right<=R.Left) or (R.Bottom<=R.Top) then Exit;
 if FAtlas=nil then begin
  Factor:=8192.0/Math.Max(FMaxX-FMinX,FMaxY-FMinY);
  W:=Math.Max(1,Round((FMaxX-FMinX)*Factor));H:=Math.Max(1,Round((FMaxY-FMinY)*Factor));
  // Bounded to 48 MB of display pixels, independent of window/zoom size.
  if Double(W)*H>12000000 then begin
   Factor:=Sqrt(12000000.0/(Double(W)*H));W:=Math.Max(1,Trunc(W*Factor));H:=Math.Max(1,Trunc(H*Factor));
  end;
  FAtlas:=Graphics.TBitmap.Create;FAtlas.PixelFormat:=pf32bit;FAtlas.SetSize(W,H);
  FAtlas.Canvas.Brush.Color:=FBackground;FAtlas.Canvas.FillRect(0,0,W,H);
  Bounds:=Types.Rect(0,0,W,H);
  if FCommandCount>0 then begin
   FDC:=PtrUInt(FAtlas.Canvas.Handle);Saved:=Windows.SaveDC(HDC(FDC));
   try
    FillChar(Header,SizeOf(Header),0);GetEnhMetaFileHeader(HENHMETAFILE(FHandle),SizeOf(Header),@Header);
    FillChar(Transform,SizeOf(Transform),0);
    Transform.eM11:=W*Header.szlMillimeters.cx*100.0/(Header.rclFrame.Right*Header.szlDevice.cx);
    Transform.eM22:=H*Header.szlMillimeters.cy*100.0/(Header.rclFrame.Bottom*Header.szlDevice.cy);
    Windows.SetGraphicsMode(HDC(FDC),GM_ADVANCED);
    FillChar(Captured,SizeOf(Captured),0);
    NativeEnumEnhMetaFile(FDC,FHandle,@CaptureMetaTransform,@Captured,@Bounds);
    if Captured.Ready then Transform:=Captured.Value;
    Windows.SetWorldTransform(HDC(FDC),Transform);
    for I:=0 to FCommandCount-1 do SubmitPolygon(FCommands[I].Points,FCommands[I].Color);
   finally Windows.RestoreDC(HDC(FDC),Saved);FDC:=0;ReleasePaints end;
  end else if not Windows.PlayEnhMetaFile(HDC(FAtlas.Canvas.Handle),HENHMETAFILE(FHandle),Bounds) then begin
   FreeAndNil(FAtlas);Exit;
  end;
 end;
 if ((R.Right-R.Left)>FAtlas.Width) or ((R.Bottom-R.Top)>FAtlas.Height) then begin
  EnsureNative;
  FLastDrawMode:='native-vector';
  Exit(Windows.PlayEnhMetaFile(HDC(Target.Handle),HENHMETAFILE(FHandle),R));
 end;
 Source:=FAtlas;Level:=0;
 // Select the smallest prefiltered image that still has at least one source
 // pixel per destination pixel. Small views need not resample all 12M pixels.
 while Level<High(FMips) do begin
  NW:=Math.Max(1,Source.Width div 2);NH:=Math.Max(1,Source.Height div 2);
  if (NW<R.Right-R.Left) or (NH<R.Bottom-R.Top) then Break;
  Inc(Level);
  if FMips[Level]=nil then begin
   FMips[Level]:=Graphics.TBitmap.Create;FMips[Level].PixelFormat:=pf32bit;FMips[Level].SetSize(NW,NH);
   Saved:=Windows.SaveDC(HDC(FMips[Level].Canvas.Handle));
   try
    Windows.SetStretchBltMode(HDC(FMips[Level].Canvas.Handle),HALFTONE);
    Windows.SetBrushOrgEx(HDC(FMips[Level].Canvas.Handle),0,0,nil);
    Windows.StretchBlt(HDC(FMips[Level].Canvas.Handle),0,0,NW,NH,HDC(Source.Canvas.Handle),0,0,Source.Width,Source.Height,SRCCOPY);
   finally Windows.RestoreDC(HDC(FMips[Level].Canvas.Handle),Saved) end;
  end;
  Source:=FMips[Level];
 end;
 FLastDrawMode:='filtered-level-'+IntToStr(Level);
 // The same high-resolution source and filtering are used during interaction
 // and at rest. No delayed quality upgrade, and no upscaling coarse pixels.
 Saved:=Windows.SaveDC(HDC(Target.Handle));
 try
  Windows.SetStretchBltMode(HDC(Target.Handle),HALFTONE);
  Windows.SetBrushOrgEx(HDC(Target.Handle),0,0,nil);
  Result:=Windows.StretchBlt(HDC(Target.Handle),R.Left,R.Top,R.Right-R.Left,R.Bottom-R.Top,
    HDC(Source.Canvas.Handle),0,0,Source.Width,Source.Height,SRCCOPY);
 finally Windows.RestoreDC(HDC(Target.Handle),Saved) end;
 {$ENDIF}
end;
end.
