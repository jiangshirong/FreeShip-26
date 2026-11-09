unit UndoBackgroundProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure RunUndoBackgroundProbe(Form:TMainForm);
implementation
uses Classes, SysUtils, Math, Graphics, Windows, Types, fpjson, FreeShipUnit,
  FreeFileBuffer, FreeGeometry, FreeTypes, FreeVersionUnit;

function Env(const Name:string):string;
begin Result:=SysUtils.GetEnvironmentVariable(Name) end;

procedure Check(C:TJSONArray; const ID:string; OK:Boolean; const Detail:string='');
var Item:TJSONObject;
begin
  Item:=TJSONObject.Create; Item.Add('id',ID);
  if OK then Item.Add('status','pass') else Item.Add('status','fail');
  Item.Add('actual',Detail); C.Add(Item);
end;

function History(Ship:TFreeShip):RawByteString;
var I:Integer; M:TMemoryStream; S:RawByteString;
begin
  Result:=IntToStr(Ship.UndoCount)+':'+IntToStr(Ship.UndoPosition)+':';
  M:=TMemoryStream.Create;
  try
    for I:=0 to Ship.UndoCount-1 do
    begin
      M.Clear; Ship.UndoObject[I].UndoData.SaveToStream(M); SetLength(S,M.Size);
      if M.Size>0 then Move(M.Memory^,S[1],M.Size);
      Result:=Result+IntToHex(PtrUInt(Ship.UndoObject[I]),16)+':'+Ship.UndoObject[I].UndoText+':'+IntToStr(M.Size)+':'+S;
    end;
  finally M.Free end;
end;

function ProbePoint(Ship:TFreeShip):TFreeSubdivisionControlPoint;
var I:Integer;
begin
  for I:=0 to Ship.Surface.NumberOfControlPoints-1 do
    if Ship.Surface.ControlPoint[I].Name='UndoBackgroundWitness' then Exit(Ship.Surface.ControlPoint[I]);
  raise Exception.Create('named point missing');
end;

procedure AddX(Ship:TFreeShip; Value:TFloatType);
var P:TFreeSubdivisionControlPoint; XYZ:T3DCoordinate;
begin P:=ProbePoint(Ship); XYZ:=P.Coordinate; XYZ.X:=XYZ.X+Value; P.Coordinate:=XYZ; Ship.FileChanged:=True end;

procedure SeedHistory(Ship:TFreeShip; UndoOnce:Boolean);
var B:TFreeFileBuffer; I:Integer;
begin
  B:=TFreeFileBuffer.Create;
  try B.LoadFromFile(Env('FS_UB_FIXTURE')); Ship.LoadProject(B) finally B.Free end;
  Ship.ClearUndo; Ship.Surface.ControlPoint[0].Name:='UndoBackgroundWitness';
  Ship.ProjectSettings.ProjectLength:=Ship.ProjectSettings.ProjectLength+0.125;
  Ship.ProjectSettings.SavePreview:=False;
  for I:=1 to 3 do begin Ship.Edit.CreateUndoObject('edit '+IntToStr(I),True); AddX(Ship,1) end;
  if UndoOnce then Ship.Edit.Undo;
end;

function RedoEquals(Ship:TFreeShip; Expected:TFloatType):Boolean;
begin
  Result:=False;
  if Ship.UndoPosition >= Ship.UndoCount then Exit;
  try Ship.Edit.Redo; Result:=ProbePoint(Ship).Coordinate.X=Expected
  except on E:Exception do Result:=False end;
end;

procedure UndoCases(Form:TMainForm; C:TJSONArray);
const Modes:array[0..7]of string=('cancel','unchanged','cancel-edited','accept-edited','throw-edited','cancel-unreported','unchanged-unreported','accept-fault');
var Ship:TFreeShip; U:TFreeUndoObject; Before:RawByteString; X:TFloatType;
  I,J,N,Position:Integer; Raised,Changed,Preview,Locked:Boolean; Version:TFreeFileVersion; Mode,Err:string;
begin
  Ship:=Form.FreeShip;
  SeedHistory(Ship,True); Before:=History(Ship); X:=ProbePoint(Ship).Coordinate.X;
  U:=Ship.Edit.CreateUndoObject('discarded',False);
  Check(C,'10.snapshot-does-not-cut-redo',History(Ship)=Before); U.Delete;
  Check(C,'10.discard-keeps-identical-history',History(Ship)=Before);
  Check(C,'10.discard-redo-restores-point',RedoEquals(Ship,X+1));
  for I:=Low(Modes) to High(Modes) do
  begin
    Mode:=Modes[I]; SeedHistory(Ship,True);
    if Pos('unreported',Mode)>0 then ProbePoint(Ship).Locked:=True;
    Before:=History(Ship); X:=ProbePoint(Ship).Coordinate.X; Locked:=ProbePoint(Ship).Locked;
    N:=Ship.UndoCount; Position:=Ship.UndoPosition; Changed:=Ship.FileChanged; Raised:=False; Err:='';
    Windows.SetEnvironmentVariable('FS_UB_DIALOG',PChar(Mode));
    try Ship.Edit.Model_LackenbyTransformation except on E:Exception do begin Raised:=True; Err:=E.Message end end;
    Windows.SetEnvironmentVariable('FS_UB_DIALOG','');
    Windows.SetEnvironmentVariable('FS_UB_ACCEPT_FAULT','');
    if Mode='accept-edited' then
    begin
      Check(C,'10.commit-cuts-old-redo',not Raised and (Ship.UndoCount=Position+1) and (Ship.UndoPosition=Ship.UndoCount) and
        (ProbePoint(Ship).Coordinate.X=X+0.5),Err);
      Ship.Edit.Undo; Check(C,'10.commit-undo-original',ProbePoint(Ship).Coordinate.X=X);
      Ship.Edit.Redo; Check(C,'10.commit-redo-new-branch',ProbePoint(Ship).Coordinate.X=X+0.5);
    end
    else
    begin
      Check(C,'10.'+Mode+'-history', (History(Ship)=Before) and (Raised=((Mode='throw-edited') or (Mode='accept-fault'))),Err);
      Check(C,'10.'+Mode+'-model',(ProbePoint(Ship).Coordinate.X=X) and (Ship.FileChanged=Changed) and (ProbePoint(Ship).Locked=Locked));
      Check(C,'10.'+Mode+'-redo',RedoEquals(Ship,X+1));
    end;
  end;
  SeedHistory(Ship,False); X:=ProbePoint(Ship).Coordinate.X;
  for J:=1 to 2 do
  begin
    for I:=1 to 3 do begin Ship.Edit.Undo; Check(C,'10.cycle-'+IntToStr(J)+'-undo-'+IntToStr(I),ProbePoint(Ship).Coordinate.X=X-I) end;
    for I:=1 to 3 do begin Ship.Edit.Redo; Check(C,'10.cycle-'+IntToStr(J)+'-redo-'+IntToStr(I),ProbePoint(Ship).Coordinate.X=X-3+I) end;
  end;
  for I:=0 to 1 do
  begin
    SeedHistory(Ship,I=0); Before:=History(Ship); Version:=Ship.FileVersion; Preview:=Ship.ProjectSettings.SavePreview;
    Windows.SetEnvironmentVariable('FS_UB_SNAPSHOT_FAULT','1'); Raised:=False; Err:='';
    try
      if I=0 then U:=Ship.Edit.CreateUndoObject('fault',False) else Ship.Edit.Undo;
    except on E:Exception do begin Raised:=True; Err:=E.Message end end;
    Windows.SetEnvironmentVariable('FS_UB_SNAPSHOT_FAULT','');
    Check(C,'10.snapshot-failure-'+IntToStr(I),Raised and (History(Ship)=Before) and
      (Ship.FileVersion=Version) and (Ship.ProjectSettings.SavePreview=Preview),Err);
  end;
end;

function ImageState(Image:TFreeBackgroundImage):RawByteString;
var M:TMemoryStream; PixelData:RawByteString;
begin
  M:=TMemoryStream.Create;
  try
    if Image.Bitmap<>nil then Image.Bitmap.SaveToStream(M);
    SetLength(PixelData,M.Size); if M.Size>0 then Move(M.Memory^,PixelData[1],M.Size);
    Result:=IntToStr(Image.Origin.X)+'/'+IntToStr(Image.Origin.Y)+'/'+FloatToStr(Image.Scale)+'/'+IntToStr(Image.Quality)+'/'+
      IntToStr(Image.Alpha)+'/'+IntToStr(Image.Tolerance)+'/'+IntToStr(Ord(Image.Transparent))+'/'+
      IntToStr(Image.TransparentColor)+'/'+IntToStr(Ord(Image.Visible))+'/'+IntToStr(Ord(Image.ShowInView))+'/'+PixelData;
  finally M.Free end;
end;

function MatchesImage(Image:TFreeBackgroundImage; const FileName:string):Boolean;
var J:TJPEGImage; B:Graphics.TBitmap; X,Y:Integer;
begin
  Result:=False; J:=TJPEGImage.Create; B:=Graphics.TBitmap.Create;
  try
    if UpperCase(ExtractFileExt(FileName))='.JPG' then begin J.LoadFromFile(FileName); B.Assign(J) end else B.LoadFromFile(FileName);
    if (Image.Bitmap=nil) or (Image.Bitmap.Width<>B.Width) or (Image.Bitmap.Height<>B.Height) then Exit;
    for Y:=0 to B.Height-1 do for X:=0 to B.Width-1 do
      if ColorToRGB(Image.Bitmap.Canvas.Pixels[X,Y])<>ColorToRGB(B.Canvas.Pixels[X,Y]) then Exit;
    Result:=(Image.Origin.X=0) and (Image.Origin.Y=B.Height);
    if UpperCase(ExtractFileExt(FileName))='.JPG' then Result:=Result and (Image.Quality=J.CompressionQuality)
    else Result:=Result and (Image.Quality=100);
  finally B.Free; J.Free end;
end;

procedure BackgroundCases(Form:TMainForm; C:TJSONArray);
var Image:TFreeBackgroundImage; Before:RawByteString; FileName,Err:string;
  I,Entry,OldCount:Integer; Raised:Boolean; Prefix:string; DataBefore:RawByteString;
  B:TFreeFileBuffer; M:TMemoryStream;
const GoodNames:array[0..3]of string=('first.jpg','second.jpg','valid.bmp','first.jpg');

  procedure OpenSelected;
  begin
    if Entry=0 then Image.Open('')
    else Form.FreeShip.Edit.BackgroundImage_Open(Image.Owner);
  end;

  function DataState:RawByteString;
  var J:Integer;
  begin
    B:=TFreeFileBuffer.Create; M:=TMemoryStream.Create;
    try
      B.Add(Form.FreeShip.NumberofBackgroundImages);
      for J:=0 to Form.FreeShip.NumberofBackgroundImages-1 do
        if Form.FreeShip.BackgroundImage[J].Image.Empty then B.Add('empty')
        else begin B.Add('image'); Form.FreeShip.BackgroundImage[J].SaveBinary(B) end;
      B.SaveToStream(M); SetLength(Result,M.Size); if M.Size>0 then Move(M.Memory^,Result[1],M.Size);
    finally M.Free; B.Free end;
  end;

begin
  Image:=Form.GetMDIChildren(0).Viewport.BackgroundImage;
  for Entry:=0 to 1 do
  begin
  if Entry=0 then Prefix:='14.viewport-' else Prefix:='14.menu-';
  for I:=Low(GoodNames) to High(GoodNames) do
  begin
    FileName:=IncludeTrailingPathDelimiter(Env('FS_UB_IMAGES'))+GoodNames[I];
    Windows.SetEnvironmentVariable('FS_UB_IMAGE',PChar(FileName)); Raised:=False; Err:='';
    try OpenSelected except on E:Exception do begin Raised:=True; Err:=E.ClassName+': '+E.Message end end;
    Check(C,Prefix+'valid-image-'+IntToStr(I),not Raised and MatchesImage(Image,FileName),Err);
    Image.Origin:=Types.Point(4,7); Image.Scale:=2.25; Image.Alpha:=123; Image.Tolerance:=7;
    Before:=ImageState(Image); DataBefore:=DataState; OldCount:=Form.FreeShip.NumberofBackgroundImages;
    Form.FreeShip.FileChanged:=False;
    Windows.SetEnvironmentVariable('FS_UB_IMAGE',PChar(IncludeTrailingPathDelimiter(Env('FS_UB_IMAGES'))+'broken.jpg'));
    Raised:=False; Err:='';
    try OpenSelected except on E:Exception do begin Raised:=True; Err:=E.ClassName+': '+E.Message end end;
    Check(C,Prefix+'broken-image-keeps-state-'+IntToStr(I),Raised and (ImageState(Image)=Before) and
      (DataState=DataBefore) and (Form.FreeShip.NumberofBackgroundImages=OldCount) and not Form.FreeShip.FileChanged,Err);
  end;
  Before:=ImageState(Image); DataBefore:=DataState; Windows.SetEnvironmentVariable('FS_UB_IMAGE',''); OpenSelected;
  Check(C,Prefix+'cancel-keeps-state',(ImageState(Image)=Before) and (DataState=DataBefore));
  end;
end;

procedure RunUndoBackgroundProbe(Form:TMainForm);
var Report:TJSONObject; C:TJSONArray; Output:TStringList;
begin
  Report:=TJSONObject.Create; C:=TJSONArray.Create; Report.Add('cases',C); Output:=TStringList.Create;
  try
    try Form.FormShow(Form); Form.LoadNamedFile(Env('FS_UB_FIXTURE')); UndoCases(Form,C); BackgroundCases(Form,C)
    except on E:Exception do Report.Add('error',E.ClassName+': '+E.Message) end;
    Output.Text:=Report.FormatJSON; Output.SaveToFile(IncludeTrailingPathDelimiter(Env('FS_UB_OUTPUT'))+'probe.json');
  finally Output.Free; Report.Free end;
end;
end.
