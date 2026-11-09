unit LegacyCompatibilityProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure RunLegacyProbe(Form:TMainForm);
implementation
uses Classes, SysUtils, fpjson, FreeShipUnit, FreeFileBuffer, FreeVersionUnit,
  FreeTypes, FileSafetyProbe;

type
  TRecordTrace = class(TFreeFileBuffer)
    Trace:TStringList;
    procedure LoadTFreeHoltrSeriesResistanceData(var V:TFreeHoltrSeriesResistanceData); override;
    procedure LoadTFreeOSTSeriesResistanceData(var V:TFreeOSTSeriesResistanceData); override;
    procedure LoadTFreeTask1PropellerData(var V:TFreeTask1PropellerData); override;
    procedure LoadTFreeTask2PropellerData(var V:TFreeTask2PropellerData); override;
    procedure LoadTFreeTask3PropellerData(var V:TFreeTask3PropellerData); override;
    procedure LoadTFreePlaningResistanceData(var V:TFreePlaningResistanceData); override;
    procedure LoadTFreeRvrsPropellerData(var V:TFreeRvrsPropellerData); override;
    procedure LoadTFreeHollenSeriesResistanceData(var V:TFreeHollenSeriesResistanceData); override;
    procedure LoadTFreeTask4PropellerData(var V:TFreeTask4PropellerData); override;
    procedure LoadTFreeTask5PropellerData(var V:TFreeTask5PropellerData); override;
    procedure LoadTFreeOortmerSeriesResistanceData(var V:TFreeOortmerSeriesResistanceData); override;
    procedure LoadTFreeFungSeriesResistanceData(var V:TFreeFungSeriesResistanceData); override;
    procedure LoadTFreeHydrodynManeuvData(var V:TFreeHydrodynManeuvData); override;
    procedure LoadTFreeHydrodynTask1Data(var V:TFreeHydrodynTask1Data); override;
    procedure LoadTFreeRBHSSeriesResistanceData(var V:TFreeRBHSSeriesResistanceData); override;
    procedure LoadTFreeMHSeriesResistanceData(var V:TFreeMHSeriesResistanceData); override;

    procedure LoadTFreeFileVersion(var V:TFreeFileVersion); override;
    procedure LoadTFreeDelftSeriesResistanceData(var V:TFreeDelftSeriesResistanceData); override;
    procedure LoadTFreeKAPERResistanceData(var V:TFreeKAPERResistanceData); override;
  end;

function RecordHex(const V; N:Integer):string;
var P:PByte;I:Integer;
begin Result:='';P:=@V;for I:=0 to N-1 do Result:=Result+IntToHex(P[I],2) end;

procedure TRecordTrace.LoadTFreeFileVersion(var V:TFreeFileVersion);
begin
  Trace.Add('version start='+IntToStr(Position)+' width='+IntToStr(SizeOf(V)));
  inherited LoadTFreeFileVersion(V);
  Trace.Add('version end='+IntToStr(Position)+' ordinal='+IntToStr(Ord(V)));
end;
procedure TRecordTrace.LoadTFreeDelftSeriesResistanceData(var V:TFreeDelftSeriesResistanceData);
begin
  Trace.Add('Delft start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeDelftSeriesResistanceData(V);
  Trace.Add('values LoadTFreeDelftSeriesResistanceData='+RecordHex(V,SizeOf(V)));
  Trace.Add('Delft end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeKAPERResistanceData(var V:TFreeKAPERResistanceData);
begin
  Trace.Add('KAPER start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeKAPERResistanceData(V);
  Trace.Add('values LoadTFreeKAPERResistanceData='+RecordHex(V,SizeOf(V)));
  Trace.Add('KAPER end='+IntToStr(Position));
end;

procedure TRecordTrace.LoadTFreeHoltrSeriesResistanceData(var V:TFreeHoltrSeriesResistanceData);
begin
  Trace.Add('LoadTFreeHoltrSeriesResistanceData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeHoltrSeriesResistanceData(V);
  Trace.Add('values LoadTFreeHoltrSeriesResistanceData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeHoltrSeriesResistanceData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeOSTSeriesResistanceData(var V:TFreeOSTSeriesResistanceData);
begin
  Trace.Add('LoadTFreeOSTSeriesResistanceData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeOSTSeriesResistanceData(V);
  Trace.Add('values LoadTFreeOSTSeriesResistanceData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeOSTSeriesResistanceData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeTask1PropellerData(var V:TFreeTask1PropellerData);
begin
  Trace.Add('LoadTFreeTask1PropellerData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeTask1PropellerData(V);
  Trace.Add('values LoadTFreeTask1PropellerData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeTask1PropellerData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeTask2PropellerData(var V:TFreeTask2PropellerData);
begin
  Trace.Add('LoadTFreeTask2PropellerData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeTask2PropellerData(V);
  Trace.Add('values LoadTFreeTask2PropellerData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeTask2PropellerData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeTask3PropellerData(var V:TFreeTask3PropellerData);
begin
  Trace.Add('LoadTFreeTask3PropellerData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeTask3PropellerData(V);
  Trace.Add('values LoadTFreeTask3PropellerData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeTask3PropellerData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreePlaningResistanceData(var V:TFreePlaningResistanceData);
begin
  Trace.Add('LoadTFreePlaningResistanceData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreePlaningResistanceData(V);
  Trace.Add('values LoadTFreePlaningResistanceData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreePlaningResistanceData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeRvrsPropellerData(var V:TFreeRvrsPropellerData);
begin
  Trace.Add('LoadTFreeRvrsPropellerData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeRvrsPropellerData(V);
  Trace.Add('values LoadTFreeRvrsPropellerData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeRvrsPropellerData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeHollenSeriesResistanceData(var V:TFreeHollenSeriesResistanceData);
begin
  Trace.Add('LoadTFreeHollenSeriesResistanceData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeHollenSeriesResistanceData(V);
  Trace.Add('values LoadTFreeHollenSeriesResistanceData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeHollenSeriesResistanceData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeTask4PropellerData(var V:TFreeTask4PropellerData);
begin
  Trace.Add('LoadTFreeTask4PropellerData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeTask4PropellerData(V);
  Trace.Add('values LoadTFreeTask4PropellerData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeTask4PropellerData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeTask5PropellerData(var V:TFreeTask5PropellerData);
begin
  Trace.Add('LoadTFreeTask5PropellerData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeTask5PropellerData(V);
  Trace.Add('values LoadTFreeTask5PropellerData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeTask5PropellerData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeOortmerSeriesResistanceData(var V:TFreeOortmerSeriesResistanceData);
begin
  Trace.Add('LoadTFreeOortmerSeriesResistanceData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeOortmerSeriesResistanceData(V);
  Trace.Add('values LoadTFreeOortmerSeriesResistanceData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeOortmerSeriesResistanceData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeFungSeriesResistanceData(var V:TFreeFungSeriesResistanceData);
begin
  Trace.Add('LoadTFreeFungSeriesResistanceData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeFungSeriesResistanceData(V);
  Trace.Add('values LoadTFreeFungSeriesResistanceData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeFungSeriesResistanceData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeHydrodynManeuvData(var V:TFreeHydrodynManeuvData);
begin
  Trace.Add('LoadTFreeHydrodynManeuvData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeHydrodynManeuvData(V);
  Trace.Add('values LoadTFreeHydrodynManeuvData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeHydrodynManeuvData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeHydrodynTask1Data(var V:TFreeHydrodynTask1Data);
begin
  Trace.Add('LoadTFreeHydrodynTask1Data start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeHydrodynTask1Data(V);
  Trace.Add('values LoadTFreeHydrodynTask1Data='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeHydrodynTask1Data end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeRBHSSeriesResistanceData(var V:TFreeRBHSSeriesResistanceData);
begin
  Trace.Add('LoadTFreeRBHSSeriesResistanceData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeRBHSSeriesResistanceData(V);
  Trace.Add('values LoadTFreeRBHSSeriesResistanceData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeRBHSSeriesResistanceData end='+IntToStr(Position));
end;
procedure TRecordTrace.LoadTFreeMHSeriesResistanceData(var V:TFreeMHSeriesResistanceData);
begin
  Trace.Add('LoadTFreeMHSeriesResistanceData start='+IntToStr(Position)+' storage='+IntToStr(SizeOf(V)));
  inherited LoadTFreeMHSeriesResistanceData(V);
  Trace.Add('values LoadTFreeMHSeriesResistanceData='+RecordHex(V,SizeOf(V)));
  Trace.Add('LoadTFreeMHSeriesResistanceData end='+IntToStr(Position));
end;

function SettingsPart(Ship:TFreeShip; Part:Integer):RawByteString;
var B:TFreeFileBuffer; M:TMemoryStream; I:Integer;
begin
  B:=TFreeFileBuffer.Create; M:=TMemoryStream.Create;
  try
    B.Version:=Ship.FileVersion; B.Encoding:=Ship.Preferences.FbmEncoding;
    case Part of
      0: Ship.ProjectSettings.SaveBinary(B);
      1: Ship.Visibility.SaveBinary(B);
      2: begin
        B.Add(Ship.ActiveLayer.LayerIndex);
        for I:=0 to Ship.NumberOfLayers-1 do Ship.Layer[I].SaveBinary(B);
      end;
      3: for I:=0 to Ship.NumberOfStations-1 do Ship.Station[I].SaveBinary(B);
      4: for I:=0 to Ship.NumberOfButtocks-1 do Ship.Buttock[I].SaveBinary(B);
      5: for I:=0 to Ship.NumberOfWaterlines-1 do Ship.Waterline[I].SaveBinary(B);
      6: for I:=0 to Ship.NumberOfDiagonals-1 do Ship.Diagonal[I].SaveBinary(B);
      7: for I:=0 to Ship.NumberOfMarkers-1 do Ship.Marker[I].SaveBinary(B);
      8: for I:=0 to Ship.NumberOfFlowlines-1 do Ship.Flowline[I].SaveBinary(B);
      9: for I:=0 to Ship.NumberOfBackgroundImages-1 do Ship.BackgroundImage[I].SaveBinary(B);
    end;
    B.SaveToStream(M); SetLength(Result,M.Size);
    if M.Size>0 then Move(M.Memory^,Result[1],M.Size);
  finally M.Free; B.Free end;
end;

function BackgroundMetadata(Ship:TFreeShip):RawByteString;
var B:TFreeFileBuffer; M:TMemoryStream; I:Integer; Header:RawByteString;
begin
  Result:=IntToStr(Ship.NumberOfBackgroundImages)+':';
  for I:=0 to Ship.NumberOfBackgroundImages-1 do begin
    B:=TFreeFileBuffer.Create; M:=TMemoryStream.Create;
    try
      Ship.BackgroundImage[I].SaveBinary(B); B.SaveToStream(M);
      // Binary background header: 34 bytes of settings followed by width/height.
      // JPEG length and payload are assessed separately: re-encoding is lossy.
      if M.Size<46 then raise EReadError.Create('Incomplete background witness');
      SetLength(Header,42); Move(M.Memory^,Header[1],42);
      Result:=Result+Header;
    finally M.Free; B.Free end;
  end;
end;

procedure RunLegacyProbe(Form:TMainForm);
var Ship,Reload:TFreeShip; B,OutBuffer:TFreeFileBuffer; Trace:TStringList;
  R:TJSONObject; Output,Target,Err,Before,After:string; BeforeSettings:RawByteString;
  I,J,ReadPosition,ReadCount,Version:Integer; Loaded,RoundTrip,Alpha:Boolean;
  PartsBefore:array[0..9] of RawByteString; PartAfter:RawByteString;
  BackgroundBefore:RawByteString; OnlyJpegDifference:Boolean;
begin
  Target:=GetEnvironmentVariable('FS_LEGACY_MODEL');
  Output:=IncludeTrailingPathDelimiter(GetEnvironmentVariable('FS_LEGACY_OUTPUT'));
  R:=TJSONObject.Create; Trace:=TStringList.Create; Ship:=nil; Reload:=nil; B:=nil; OutBuffer:=nil;
  Loaded:=False; RoundTrip:=False; Err:=''; ReadPosition:=0; ReadCount:=0; Version:=-1;
  Alpha:=GetEnvironmentVariable('FS_LEGACY_ALPHA')='1'; OnlyJpegDifference:=False;
  try
    try
      Form.FormShow(Form);
      Ship:=TFreeShip.Create(nil); Ship.IsRebuildAsyncDisabled:=True;
      if LowerCase(ExtractFileExt(Target))='.ftm' then B:=TFreeTextBuffer.Create
      else begin B:=TRecordTrace.Create; TRecordTrace(B).Trace:=Trace end;
      B.LoadFromFile(Target);
      Ship.LoadProject(B);
      Loaded:=Ship.ModelIsLoaded;
      R.Add('legacy_calculation_layout',Ship.LegacyCalculationLayout);
      ReadPosition:=B.Position; ReadCount:=B.Count; Version:=Ord(Ship.FileVersion);
      R.Add('points',Ship.Surface.NumberOfControlPoints);
      R.Add('faces',Ship.Surface.NumberOfControlFaces);
      if Loaded then begin
        // Stable names permit a complete graph comparison across native pointer sorting.
        Ship.FileVersion:=CurrentVersion;
        if Alpha then begin Ship.FileVersion:=fv500; Ship.ProjectSettings.ProjectUnderWaterColorAlpha:=123 end;
        Ship.ProjectSettings.SavePreview:=False;
        for I:=0 to Ship.Surface.NumberOfControlPoints-1 do
          Ship.Surface.ControlPoint[I].Name:='LegacyWitness-'+IntToStr(I)+':'+Ship.Surface.ControlPoint[I].Name;
        Before:=GeometryWitness(Ship); BeforeSettings:=SettingsWitness(Ship);
        for I:=0 to 9 do PartsBefore[I]:=SettingsPart(Ship,I);
        BackgroundBefore:=BackgroundMetadata(Ship);
        OutBuffer:=TRecordTrace.Create; TRecordTrace(OutBuffer).Trace:=Trace; Ship.SaveProject(OutBuffer);
        OutBuffer.SaveToFile(Output+'roundtrip.fbm');
        Reload:=TFreeShip.Create(nil); Reload.IsRebuildAsyncDisabled:=True;
        Reload.LoadProject(OutBuffer);
        After:=GeometryWitness(Reload);
        RoundTrip:=Reload.ModelIsLoaded and (Before=After) and (BeforeSettings=SettingsWitness(Reload));
        if Alpha then RoundTrip:=RoundTrip and (Reload.FileVersion=fv500) and
          (Reload.ProjectSettings.ProjectUnderWaterColorAlpha=123);
        Trace.Add('roundtrip geometry_equal='+SysUtils.BoolToStr(Before=After,True));
        R.Add('geometry_equal',Before=After);
        R.Add('settings_equal',BeforeSettings=SettingsWitness(Reload));
        OnlyJpegDifference:=not RoundTrip and (Before=After) and
          (Ship.FileVersion=Reload.FileVersion) and (Ship.Precision=Reload.Precision) and
          (PartsBefore[9]<>SettingsPart(Reload,9)) and
          (BackgroundBefore=BackgroundMetadata(Reload));
        for I:=0 to 9 do begin
          PartAfter:=SettingsPart(Reload,I);
          if PartsBefore[I]<>PartAfter then begin
            if I<>9 then OnlyJpegDifference:=False;
            Trace.Add('settings_part_differ='+IntToStr(I)+' before_bytes='+IntToStr(Length(PartsBefore[I]))+' after_bytes='+IntToStr(Length(PartAfter)));
            J:=1;
            while (J<=Length(PartsBefore[I])) and (J<=Length(PartAfter)) and (PartsBefore[I][J]=PartAfter[J]) do Inc(J);
            Trace.Add('first_different_byte='+IntToStr(J));
          end;
        end;
      end;
    except on E:Exception do begin
      Err:=E.ClassName+': '+E.Message;
      if B<>nil then begin ReadPosition:=B.Position; ReadCount:=B.Count end;
    end end;
    R.Add('loaded',Loaded); R.Add('roundtrip',RoundTrip); R.Add('error',Err);
    R.Add('source_position',ReadPosition); R.Add('source_count',ReadCount);
    R.Add('file_version_ordinal',Version); R.Add('enum_width',SizeOf(TFreeFileVersion));
    R.Add('delft_record_width',SizeOf(TFreeDelftSeriesResistanceData));
    R.Add('kaper_record_width',SizeOf(TFreeKAPERResistanceData));
    if Loaded and RoundTrip and (Err='') then R.Add('status','pass')
    else if Loaded and OnlyJpegDifference and (Err='') then begin
      R.Add('status','review');
      R.Add('review_reason','Geometry and scalar settings roundtrip exactly; background JPEG payload changes on resave. Image fidelity is not a pass.');
    end else R.Add('status','fail');
    Trace.SaveToFile(Output+'offsets.txt');
    Trace.Text:=R.FormatJSON; Trace.SaveToFile(Output+'probe.json');
  finally OutBuffer.Free; B.Free; Reload.Free; Ship.Free; Trace.Free; R.Free end;
end;
end.
