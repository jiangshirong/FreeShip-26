unit FileSafetyProbe;
{$mode objfpc}{$H+}
interface
uses Main, FreeShipUnit;
procedure RunFileSafetyProbe(Form:TMainForm);
function GeometryWitness(Ship:TFreeShip):string;
function SettingsWitness(Ship:TFreeShip):RawByteString;
implementation
uses SysUtils, Classes, Math, fpjson, Windows, FreeGeometry,
  FreeTypes, FreeVersionUnit, FreeFileBuffer, FreeControlPointFrm, SurfaceLoadingProbe,
  GeometrySafetyProbe;

type TChangeWitness=class
  Count:Integer;
  procedure Changed(Sender:TObject);
end;
procedure TChangeWitness.Changed(Sender:TObject);
begin Inc(Count) end;

function PointEditor(Ship:TFreeShip):TFreeControlPointForm;
var I:Integer;
begin
  Result:=nil;
  for I:=0 to Ship.ComponentCount-1 do
    if Ship.Components[I] is TFreeControlPointForm then Exit(TFreeControlPointForm(Ship.Components[I]));
end;

function Env(const Name:string):string;
begin Result:=SysUtils.GetEnvironmentVariable(Name) end;

procedure Check(C:TJSONArray; const ID,Expected:string; OK:Boolean; const Detail:string='');
var O:TJSONObject;
begin
  O:=TJSONObject.Create;
  O.Add('id',ID); O.Add('expected',Expected);
  if OK then O.Add('status','pass') else O.Add('status','fail');
  O.Add('actual',Detail); C.Add(O);
end;

function Bytes(const Name:string):RawByteString;
var F:TFileStream;
begin
  F:=TFileStream.Create(Name,fmOpenRead or fmShareDenyNone);
  try SetLength(Result,F.Size); if F.Size>0 then F.ReadBuffer(Result[1],F.Size)
  finally F.Free end;
end;

procedure Snapshot(Ship:TFreeShip; const Name:string);
var B:TFreeFileBuffer;
begin
  B:=TFreeFileBuffer.Create;
  try Ship.SaveProject(B); if not B.SaveToFile(Name) then raise Exception.Create('snapshot write failed')
  finally B.Free end;
end;

function FloatBits(Value:TFloatType):string;
var Raw:LongWord;
begin
  if SizeOf(Value)<>SizeOf(Raw) then raise Exception.Create('float witness assumes a 32-bit model value');
  Move(Value,Raw,SizeOf(Raw)); Result:=IntToHex(Raw,8);
end;

function PointName(P:TFreeSubdivisionPoint):string;
begin
  if P=nil then Exit('<nil>');
  Result:=IntToStr(Length(P.Name))+':'+P.Name;
end;

function GeometryWitness(Ship:TFreeShip):string;
var L:TStringList; I,J:Integer; P:TFreeSubdivisionControlPoint;
  E:TFreeSubdivisionControlEdge; F:TFreeSubdivisionControlFace; V:TFreeSubdivisionControlCurve; X:string;
begin
  L:=TStringList.Create;
  try
    for I:=0 to Ship.Surface.NumberOfControlPoints-1 do begin
      P:=Ship.Surface.ControlPoint[I];
      L.Add('P '+PointName(P)+' '+FloatBits(P.Coordinate.X)+' '+FloatBits(P.Coordinate.Y)+' '+FloatBits(P.Coordinate.Z)
        +' '+IntToStr(Ord(P.VertexType))+' '+IntToStr(Ord(P.Locked))+' '+IntToStr(Ord(P.Selected))
        +' A '+PointName(P.AnchorPoint)+' '+IntToStr(Ord(P.IsAnchorHard))
        +' C '+PointName(P.LinearConstraintPointA)+' '+PointName(P.LinearConstraintPointB));
    end;
    for I:=0 to Ship.Surface.NumberOfControlEdges-1 do begin
      E:=Ship.Surface.ControlEdge[I];
      L.Add('E '+E.Name+' '+PointName(E.StartPoint)+' -> '+PointName(E.EndPoint)
        +' '+IntToStr(Ord(E.Crease))+' '+IntToStr(Ord(E.Selected)));
    end;
    for I:=0 to Ship.Surface.NumberOfControlFaces-1 do begin
      F:=Ship.Surface.ControlFace[I]; X:='F '+F.Name+' layer='+IntToStr(F.Layer.LayerIndex)+' selected='+IntToStr(Ord(F.Selected));
      for J:=0 to F.NumberOfPoints-1 do X:=X+' / '+PointName(F.Point[J]);
      L.Add(X);
    end;
    for I:=0 to Ship.NumberOfControlCurves-1 do begin
      V:=Ship.ControlCurve[I]; X:='C '+V.Name+' selected='+IntToStr(Ord(V.Selected));
      for J:=0 to V.NumberOfControlPoints-1 do X:=X+' / '+PointName(V.ControlPoint[J]);
      L.Add(X);
    end;
    L.Sort; Result:=L.Text;
  finally L.Free end;
end;

function SettingsWitness(Ship:TFreeShip):RawByteString;
var B:TFreeFileBuffer; M:TMemoryStream; I:Integer;
begin
  B:=TFreeFileBuffer.Create; M:=TMemoryStream.Create;
  try
    B.Version:=Ship.FileVersion; B.Encoding:=Ship.Preferences.FbmEncoding;
    B.Add(Ship.FileVersion); B.Add(Ord(Ship.Precision));
    Ship.ProjectSettings.SaveBinary(B); Ship.Visibility.SaveBinary(B);
    B.Add(Ship.NumberOfLayers);
    for I:=0 to Ship.NumberOfLayers-1 do Ship.Layer[I].SaveBinary(B);
    B.Add(Ship.ActiveLayer.LayerIndex);
    B.Add(Ship.NumberOfStations); for I:=0 to Ship.NumberOfStations-1 do Ship.Station[I].SaveBinary(B);
    B.Add(Ship.NumberOfButtocks); for I:=0 to Ship.NumberOfButtocks-1 do Ship.Buttock[I].SaveBinary(B);
    B.Add(Ship.NumberOfWaterlines); for I:=0 to Ship.NumberOfWaterlines-1 do Ship.Waterline[I].SaveBinary(B);
    B.Add(Ship.NumberOfDiagonals); for I:=0 to Ship.NumberOfDiagonals-1 do Ship.Diagonal[I].SaveBinary(B);
    B.Add(Ship.NumberOfMarkers); for I:=0 to Ship.NumberOfMarkers-1 do Ship.Marker[I].SaveBinary(B);
    B.Add(Ship.NumberOfFlowlines); for I:=0 to Ship.NumberOfFlowlines-1 do Ship.Flowline[I].SaveBinary(B);
    B.Add(Ship.NumberOfBackgroundImages); for I:=0 to Ship.NumberOfBackgroundImages-1 do Ship.BackgroundImage[I].SaveBinary(B);
    B.SaveToStream(M); SetLength(Result,M.Size);
    if M.Size>0 then Move(M.Memory^,Result[1],M.Size);
  finally M.Free; B.Free end;
end;

procedure SaveText(const Name,Value:string);
var S:TStringList;
begin S:=TStringList.Create; try S.Text:=Value; S.SaveToFile(Name) finally S.Free end end;

function FindPoint(Ship:TFreeShip; const Name:string):TFreeSubdivisionControlPoint;
var I:Integer;
begin
  Result:=nil;
  for I:=0 to Ship.Surface.NumberOfControlPoints-1 do
    if Ship.Surface.ControlPoint[I].Name=Name then Exit(Ship.Surface.ControlPoint[I]);
end;

procedure FailureCase(Form:TMainForm; C:TJSONArray; const Mode:string);
var
  Ship:TFreeShip; P,AfterP:TFreeSubdivisionControlPoint; BeforeSurface:TFreeSubdivisionSurface;
  B:TFreeFileBuffer; Coord:T3DCoordinate; OriginalX:Double;
  BeforeName,Prefix,ErrorText,Target,BeforeFile,AfterFile,LoadStage:string;
  BeforeUndo,BeforeUndoPos,BeforePoints:Integer;
  BeforeLoaded,BeforeFilenameSet,BeforeReadOnly,BeforeChanged,SameObjects:Boolean;
begin
  Ship:=Form.FreeShip;
  Ship.FileVersion:=CurrentVersion;
  Ship.ProjectSettings.SavePreview:=False;
  Ship.Edit.Undo_Clear;
  P:=Ship.Surface.ControlPoint[0]; P.Name:='FileSafetyUndoPoint';
  Coord:=P.Coordinate; OriginalX:=Coord.X;
  Ship.Edit.CreateUndoObject('file safety rollback witness',True);
  Coord.X:=OriginalX+1; P.Coordinate:=Coord; Ship.FileChanged:=True;
  Ship.Edit.Undo;
  P:=FindPoint(Ship,'FileSafetyUndoPoint');
  if (P=nil) or (Abs(P.Coordinate.X-OriginalX)>0.0001) or (Ship.UndoCount<=Ship.UndoPosition) then
    raise Exception.Create('could not establish redo history');
  P.Selected:=True; Ship.ActiveControlPoint:=P;
  Ship.FileChanged:=True; Ship.FileIsReadOnly:=False;
  BeforeName:=Ship.Filename; BeforeFilenameSet:=Ship.FilenameSet;
  BeforeChanged:=Ship.FileChanged; BeforeLoaded:=Ship.ModelIsLoaded;
  BeforeReadOnly:=Ship.FileIsReadOnly; BeforeSurface:=Ship.Surface;
  BeforeUndo:=Ship.UndoCount; BeforeUndoPos:=Ship.UndoPosition;
  BeforePoints:=Ship.Surface.NumberOfControlPoints;
  BeforeFile:=IncludeTrailingPathDelimiter(Env('FS_FILE_OUTPUT'))+'before.fbm';
  AfterFile:=IncludeTrailingPathDelimiter(Env('FS_FILE_OUTPUT'))+'after.fbm';
  Snapshot(Ship,BeforeFile);
  Target:=Env('FS_FILE_TARGET'); Prefix:='02.'+Mode+'.'+Env('FS_FILE_CASE'); ErrorText:='';
  Windows.SetEnvironmentVariable('FS_FILE_MODAL','');
  if Env('FS_FILE_CASE')='rebuild-fault' then
    Windows.SetEnvironmentVariable('FS_FILE_REBUILD_FAULT','1');
  LoadStage:='File_Load';
  if Mode='edit' then begin
    try Ship.Edit.File_Load(Target) except on E:Exception do ErrorText:=E.ClassName+': '+E.Message end;
    Check(C,Prefix+'.reported','File_Load handles failure and reports it through the error dialog',
      (ErrorText='') and (Env('FS_FILE_MODAL')='shown') and Ship.IsLoadError,
      'escaped='+ErrorText+'; dialog='+Env('FS_FILE_MODAL'));
  end else begin
    B:=TFreeFileBuffer.Create;
    try
      try
        LoadStage:='LoadFromFile'; B.LoadFromFile(Target);
        LoadStage:='LoadProject'; Ship.LoadProject(B);
      except on E:Exception do ErrorText:=E.ClassName+': '+E.Message end;
    finally B.Free end;
    Check(C,Prefix+'.reported','direct read/load rejects invalid input with an exception',ErrorText<>'',LoadStage+': '+ErrorText);
  end;
  Windows.SetEnvironmentVariable('FS_FILE_REBUILD_FAULT','');
  // Compare addresses before dereferencing any original object: failed implementations may free them.
  AfterP:=FindPoint(Ship,'FileSafetyUndoPoint');
  SameObjects:=(Ship.Surface=BeforeSurface) and (AfterP=P);
  Check(C,Prefix+'.object-identity','failed load leaves existing surface and point objects alive',SameObjects);
  Check(C,Prefix+'.selection','failed load keeps the selected and active control point',
    SameObjects and (AfterP<>nil) and AfterP.Selected and (Ship.ActiveControlPoint=AfterP));
  Check(C,Prefix+'.state','filename, filename-set, dirty, readonly, loaded and undo positions are unchanged',
    (Ship.Filename=BeforeName) and (Ship.FilenameSet=BeforeFilenameSet) and
    (Ship.FileChanged=BeforeChanged) and (Ship.FileIsReadOnly=BeforeReadOnly) and
    (Ship.ModelIsLoaded=BeforeLoaded) and (Ship.UndoCount=BeforeUndo) and
    (Ship.UndoPosition=BeforeUndoPos) and (Ship.Surface.NumberOfControlPoints=BeforePoints),
    Format('undo=%d/%d -> %d/%d; points=%d -> %d',[BeforeUndoPos,BeforeUndo,Ship.UndoPosition,Ship.UndoCount,BeforePoints,Ship.Surface.NumberOfControlPoints]));
  Snapshot(Ship,AfterFile);
  Check(C,Prefix+'.model-bytes','model serialization is byte-for-byte unchanged after failure',Bytes(BeforeFile)=Bytes(AfterFile));
  if SameObjects and (Ship.UndoCount=BeforeUndo) and (Ship.UndoPosition=BeforeUndoPos) then begin
    Ship.Edit.Redo; AfterP:=FindPoint(Ship,'FileSafetyUndoPoint');
    Check(C,Prefix+'.redo','redo still restores the original pending edit',
      (AfterP<>nil) and (Abs(AfterP.Coordinate.X-(OriginalX+1))<0.0001));
    Ship.Edit.Undo; AfterP:=FindPoint(Ship,'FileSafetyUndoPoint');
    Check(C,Prefix+'.undo','undo remains usable after redo',
      (AfterP<>nil) and (Abs(AfterP.Coordinate.X-OriginalX)<0.0001));
  end else begin
    Check(C,Prefix+'.redo','redo still restores the original pending edit',False,'unsafe to run after state loss');
    Check(C,Prefix+'.undo','undo remains usable after redo',False,'unsafe to run after state loss');
  end;
end;

procedure SuccessCase(Form:TMainForm; C:TJSONArray);
var Ship:TFreeShip; I,PointCount,FaceCount:Integer; Name,RoundTrip,BeforeFile,AfterFile:string;
  B:TFreeFileBuffer; P:TFreeSubdivisionControlPoint; Coord:T3DCoordinate;
  Witness:TChangeWitness; Editor:TFreeControlPointForm;
  BeforeGeometry,AfterGeometry:string; BeforeSettings,AfterSettings:RawByteString;
begin
  Ship:=Form.FreeShip; Name:=Env('FS_FILE_TARGET');
  Witness:=TChangeWitness.Create;
  Ship.Surface.AddOnChangeItemListener(@Witness.Changed);
  try
  for I:=1 to 3 do begin
    Windows.SetEnvironmentVariable('FS_FILE_MODAL','');
    Ship.Edit.File_Load(Name);
    Check(C,Format('02.success.load-%d',[I]),'valid file loads without an error and clears old undo history',
      Ship.ModelIsLoaded and not Ship.IsLoadError and not Ship.FileChanged and Ship.FilenameSet and
      (Ship.Filename=Name) and (Ship.UndoCount=0) and (Env('FS_FILE_MODAL')='') and
      (Ship.Surface.NumberOfControlPoints>0) and (Ship.Surface.NumberOfControlFaces>0),
      'file='+Ship.Filename+'; expected='+Name+'; dialog='+Env('FS_FILE_ERROR'));
    Editor:=PointEditor(Ship);
    Check(C,Format('02.success.editor-reset-%d',[I]),'loaded model has no stale point-editor reference',
      Assigned(Editor) and (Editor.ActiveControlPoint=nil));
    Witness.Count:=0; Ship.Surface.ExecuteOnChangeItem(Ship.Surface);
    Check(C,Format('02.success.listener-%d',[I]),'the pre-existing surface listener survives replacement exactly once',Witness.Count=1,
      IntToStr(Witness.Count));
    Ship.Surface.Rebuild;
    P:=Ship.Surface.ControlPoint[0]; Coord:=P.Coordinate;
    Editor.ActiveControlPoint:=P;
    Ship.Edit.CreateUndoObject('success history witness',True); Coord.X:=Coord.X+0.125; P.Coordinate:=Coord;
    Ship.FileChanged:=True;
  end;
  Ship.Edit.File_Load(Name);
  Ship.ProjectSettings.SavePreview:=False;
  // Stable test identities distinguish coincident vertices while the native saver
  // is free to sort point pointers and remap their binary indices on every load.
  Ship.FileVersion:=CurrentVersion;
  for I:=0 to Ship.Surface.NumberOfControlPoints-1 do
    Ship.Surface.ControlPoint[I].Name:='Roundtrip point '+IntToStr(I)+': '+Ship.Surface.ControlPoint[I].Name;
  PointCount:=Ship.Surface.NumberOfControlPoints; FaceCount:=Ship.Surface.NumberOfControlFaces;
  BeforeFile:=IncludeTrailingPathDelimiter(Env('FS_FILE_OUTPUT'))+'roundtrip-before.fbm';
  AfterFile:=IncludeTrailingPathDelimiter(Env('FS_FILE_OUTPUT'))+'roundtrip-after.fbm';
  BeforeGeometry:=GeometryWitness(Ship); BeforeSettings:=SettingsWitness(Ship);
  Snapshot(Ship,BeforeFile);
  Ship.Edit.File_Load(BeforeFile);
  AfterGeometry:=GeometryWitness(Ship); AfterSettings:=SettingsWitness(Ship);
  Snapshot(Ship,AfterFile);
  SaveText(IncludeTrailingPathDelimiter(Env('FS_FILE_OUTPUT'))+'geometry-before.txt',BeforeGeometry);
  SaveText(IncludeTrailingPathDelimiter(Env('FS_FILE_OUTPUT'))+'geometry-after.txt',AfterGeometry);
  Check(C,'02.success.roundtrip','save/reload preserves exact float bits, named vertices, directed edges, face loops, layers and control curves',
    (Env('FS_FILE_MODAL')='') and Ship.ModelIsLoaded and not Ship.IsLoadError and
    (Ship.Surface.NumberOfControlPoints=PointCount) and (Ship.Surface.NumberOfControlFaces=FaceCount) and
    (BeforeGeometry=AfterGeometry),Format('points=%d; faces=%d; bytes=%d/%d; loaded=%s; error=%s',
      [PointCount,FaceCount,Length(Bytes(BeforeFile)),Length(Bytes(AfterFile)),
       SysUtils.BoolToStr(Ship.ModelIsLoaded,True),Env('FS_FILE_ERROR')]));
  Check(C,'02.success.settings','project settings, visibility, layers, intersections, markers, flowlines and background records survive save/reload',
    BeforeSettings=AfterSettings);
  finally
    Ship.Surface.RemoveOnChangeItemListener(@Witness.Changed);
    Witness.Free;
  end;
end;

procedure RunFileSafetyProbe(Form:TMainForm);
var R:TJSONObject; C:TJSONArray; S:TStringList; Mode:string;
begin
  R:=TJSONObject.Create; C:=TJSONArray.Create; R.Add('cases',C);
  try
    try
      Form.FormShow(Form);
      Form.FreeShip.Edit.File_Load(Env('FS_FILE_MODEL'));
      if not Form.FreeShip.ModelIsLoaded or Form.FreeShip.IsLoadError then raise Exception.Create('fixture did not load');
      Mode:=Env('FS_FILE_MODE');
      if Mode='success' then SuccessCase(Form,C)
      else if Mode='surface' then RunSurfaceLoadingProbe(C)
      else if Mode='geometry' then RunGeometrySafetyProbe(C)
      else FailureCase(Form,C,Mode);
    except on E:Exception do R.Add('error',E.ClassName+': '+E.Message) end;
    S:=TStringList.Create;
    try S.Text:=R.FormatJSON; S.SaveToFile(IncludeTrailingPathDelimiter(Env('FS_FILE_OUTPUT'))+'probe.json')
    finally S.Free end;
  finally R.Free end;
end;
end.
