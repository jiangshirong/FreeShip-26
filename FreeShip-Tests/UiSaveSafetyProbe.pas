unit UiSaveSafetyProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure RunSaveSafety(Form: TMainForm);
implementation
uses Classes, SysUtils, Windows, fpjson, FreeShipUnit, FreeFileBuffer,
  FreeVersionUnit, FreeTypes, FreeGeometry, SaveSafetyHooks;

function Bytes(const Name: string): RawByteString;
var S:TFileStream;
begin
  S:=TFileStream.Create(Name, fmOpenRead or fmShareDenyNone);
  try SetLength(Result,S.Size); if S.Size>0 then S.ReadBuffer(Result[1],S.Size)
  finally S.Free end;
end;

procedure Put(const Name:string; const Data:RawByteString);
var S:TFileStream;
begin
  S:=TFileStream.Create(Name,fmCreate);
  try if Length(Data)>0 then S.WriteBuffer(Data[1],Length(Data)) finally S.Free end;
end;

function TempCount(const Dir:string):Integer;
var R:TSearchRec;
begin
  Result:=0;
  if FindFirst(IncludeTrailingPathDelimiter(Dir)+'.*.tmp',faAnyFile,R)=0 then
  try repeat Inc(Result) until SysUtils.FindNext(R)<>0 finally SysUtils.FindClose(R) end;
end;

procedure RunSaveSafety(Form:TMainForm);
var
  Report, Item:TJSONObject;
  Cases:TJSONArray;
  Output:TStringList;
  Folder,Fixture,Target,Backup,BeforeName,Scenario,Err:string;
  SourceBytes,BinarySourceBytes,TextSourceBytes,OldBackup:RawByteString;
  Saved,Pass,AsNew,BeforeSet,BeforeReadOnly,RoundTrip:Boolean;
  I,J,BeforeUndoCount,BeforeUndoPosition:Integer;
  OriginalLock:THandle;
  BeforeVersion:TFreeFileVersion;
  Point:TFreeSubdivisionControlPoint;
  XYZ:T3DCoordinate;
  Buffer:TFreeFileBuffer;
  Reload:TFreeShip;
const
  Scenarios:array[0..12] of string=('serialize','write','flush','backup-copy',
    'backup-flush','backup-install','replace','locked-target',
    'save-as-failure','new-write','success-existing','success-existing-text','success-new-text');
begin
  Report:=TJSONObject.Create; Cases:=TJSONArray.Create; Report.Add('cases',Cases);
  Folder:=SysUtils.GetEnvironmentVariable('FS_SAVE_OUTPUT');
  Fixture:=SysUtils.GetEnvironmentVariable('FS_SAVE_FIXTURE');
  try
    try
      Form.FormShow(Form);
      Form.LoadNamedFile(Fixture);
      if not Form.FreeShip.ModelIsLoaded then raise Exception.Create('Fixture failed to load');
      BinarySourceBytes:=Bytes(Fixture); OldBackup:='previous backup kept until complete new backup';
      Buffer:=TFreeTextBuffer.Create;
      try
        Buffer.Encoding:=Form.FreeShip.Preferences.FbmEncoding;
        Form.FreeShip.SaveProject(Buffer);
        Target:=IncludeTrailingPathDelimiter(Folder)+'text-original.ftm';
        Buffer.SaveToFile(Target); TextSourceBytes:=Bytes(Target);
      finally Buffer.Free end;
      Point:=Form.FreeShip.Surface.ControlPoint[0];
      XYZ:=Point.Coordinate; XYZ.X:=XYZ.X+0.125; Point.Coordinate:=XYZ;
      for I:=Low(Scenarios) to High(Scenarios) do
      begin
        Scenario:=Scenarios[I];
        Target:=IncludeTrailingPathDelimiter(Folder)+'保存-Женя-'+Scenario+'.fbm';
        SourceBytes:=BinarySourceBytes;
        if Pos('-text',Scenario)>0 then
        begin Target:=ChangeFileExt(Target,'.ftm'); SourceBytes:=TextSourceBytes end;
        Backup:=ChangeFileExt(Target,'.Bak');
        AsNew:=(Scenario='success-new-text') or (Scenario='new-write');
        if not AsNew then begin Put(Target,SourceBytes); Put(Backup,OldBackup) end;
        Form.FreeShip.Filename:=Target; Form.FreeShip.FilenameSet:=True;
        Form.FreeShip.FileIsReadOnly:=False;
        Form.FreeShip.FileVersion:=CurrentVersion;
        Form.FreeShip.FileChanged:=True;
        BeforeName:=Form.FreeShip.Filename; BeforeSet:=Form.FreeShip.FilenameSet;
        BeforeReadOnly:=Form.FreeShip.FileIsReadOnly;
        BeforeVersion:=Form.FreeShip.FileVersion;
        BeforeUndoCount:=Form.FreeShip.UndoCount; BeforeUndoPosition:=Form.FreeShip.UndoPosition;
        Fault:=Scenario; LastMessage:=''; SelectedName:=''; FaultReached:=False; FlushCount:=0;
        if Scenario='new-write' then Fault:='write';
        if Scenario='save-as-failure' then
        begin
          Fault:='serialize'; SelectedName:=IncludeTrailingPathDelimiter(Folder)+'另存为.fbm';
          Form.FreeShip.FileIsReadOnly:=True; BeforeReadOnly:=True;
        end;
        OriginalLock:=INVALID_HANDLE_VALUE;
        if Scenario='locked-target' then
        begin
          Fault:='';
          OriginalLock:=CreateFileW(PWideChar(UTF8Decode(Target)),GENERIC_READ,
            FILE_SHARE_READ,nil,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
          if OriginalLock=INVALID_HANDLE_VALUE then raise Exception.Create('Cannot establish target lock');
        end;
        if Pos('success-',Scenario)=1 then Fault:='';
        Err:=''; Saved:=False;
        try
          try Saved:=Form.FreeShip.Edit.File_Save
          except on E:Exception do Err:=E.ClassName+': '+E.Message end;
        finally
          if OriginalLock<>INVALID_HANDLE_VALUE then CloseHandle(OriginalLock);
        end;
        Item:=TJSONObject.Create; Cases.Add(Item); Item.Add('id','01.'+Scenario);
        Item.Add('saved',Saved); Item.Add('exception',Err); Item.Add('message',LastMessage);
        Item.Add('fault_reached',FaultReached); Item.Add('temporary_files',TempCount(Folder));
        if Pos('success-',Scenario)=1 then
        begin
          RoundTrip:=False;
          if Saved then
          begin
            if ExtractFileExt(Target)='.ftm' then Buffer:=TFreeTextBuffer.Create else Buffer:=TFreeFileBuffer.Create;
            Reload:=TFreeShip.Create(nil);
            try
              Buffer.Encoding:=Form.FreeShip.Preferences.FbmEncoding;
              Buffer.LoadFromFile(Target); Reload.LoadProject(Buffer);
              RoundTrip:=Reload.ModelIsLoaded and
                (Reload.Surface.NumberOfControlPoints=Form.FreeShip.Surface.NumberOfControlPoints);
              if RoundTrip then
                for J:=0 to Reload.Surface.NumberOfControlPoints-1 do
                  if (Abs(Reload.Surface.ControlPoint[J].Coordinate.X-Form.FreeShip.Surface.ControlPoint[J].Coordinate.X)>0.000001) or
                     (Abs(Reload.Surface.ControlPoint[J].Coordinate.Y-Form.FreeShip.Surface.ControlPoint[J].Coordinate.Y)>0.000001) or
                     (Abs(Reload.Surface.ControlPoint[J].Coordinate.Z-Form.FreeShip.Surface.ControlPoint[J].Coordinate.Z)>0.000001) then RoundTrip:=False;
            finally Reload.Free; Buffer.Free end;
          end;
          Pass:=Saved and (Err='') and (LastMessage='') and not Form.FreeShip.FileChanged
            and RoundTrip and (TempCount(Folder)=0);
          if not AsNew then Pass:=Pass and (Bytes(Backup)=SourceBytes) and (Bytes(Target)<>SourceBytes)
          else Pass:=Pass and not FileExists(Backup);
          Item.Add('round_trip_geometry',RoundTrip);
          Item.Add('expected','complete readable model; old formal bytes in backup when replacing; no temporary files');
        end
        else
        begin
          Pass:=not Saved and (Err='') and (LastMessage<>'') and
            Form.FreeShip.FileChanged and (Form.FreeShip.Filename=BeforeName) and
            (Form.FreeShip.FilenameSet=BeforeSet) and
            (Form.FreeShip.FileIsReadOnly=BeforeReadOnly) and
            (Form.FreeShip.FileVersion=BeforeVersion) and
            (Form.FreeShip.UndoCount=BeforeUndoCount) and
            (Form.FreeShip.UndoPosition=BeforeUndoPosition) and (TempCount(Folder)=0);
          if AsNew then Pass:=Pass and not FileExists(Target) and not FileExists(Backup)
          else Pass:=Pass and FileExists(Target) and (Bytes(Target)=SourceBytes);
          if Scenario<>'locked-target' then Pass:=Pass and FaultReached;
          if AsNew then { no backup was created }
          else if (Scenario='replace') or (Scenario='locked-target') then
            Pass:=Pass and (Bytes(Backup)=SourceBytes)
          else Pass:=Pass and (Bytes(Backup)=OldBackup);
          if Scenario='save-as-failure' then Pass:=Pass and not FileExists(SelectedName);
          Item.Add('formal_bytes_preserved',FileExists(Target) and (Bytes(Target)=SourceBytes));
          Item.Add('expected','reported failure; exact original bytes, filename, flags, version and undo state preserved; no temporary files');
        end;
        if Pass then Item.Add('status','pass') else Item.Add('status','fail');
      end;
    except on E:Exception do Report.Add('error',E.ClassName+': '+E.Message) end;
    Output:=TStringList.Create;
    try Output.Text:=Report.FormatJSON; Output.SaveToFile(IncludeTrailingPathDelimiter(Folder)+'probe.json')
    finally Output.Free end;
  finally Report.Free end;
end;
end.
