program BufferSafetyProbe;
{$mode objfpc}{$H+}
uses Interfaces, Classes, SysUtils, Graphics, fpjson, FreeFileBuffer, FreeTypes, FreeVersionUnit;

type
  TLimitedStream = class(TMemoryStream)
  public
    Chunk, Fault, Calls: Integer;
    PretendSize: Int64;
    function Read(var Buffer; Count: LongInt): LongInt; override;
    function Write(const Buffer; Count: LongInt): LongInt; override;
  protected
    function GetSize: Int64; override;
  end;

var Cases: TJSONArray; Failures: Integer;

function TLimitedStream.GetSize: Int64;
begin
  if PretendSize > 0 then Result := PretendSize else Result := inherited GetSize;
end;

function TLimitedStream.Read(var Buffer; Count: LongInt): LongInt;
begin
  Inc(Calls);
  case Fault of
    1: Exit(0);
    2: if Calls > 1 then Exit(0);
    3: Exit(-1);
    4: Exit(Count + 1);
    5: raise EReadError.Create('injected read error');
  end;
  if (Chunk > 0) and (Count > Chunk) then Count := Chunk;
  Result := inherited Read(Buffer, Count);
end;

function TLimitedStream.Write(const Buffer; Count: LongInt): LongInt;
begin
  Inc(Calls);
  case Fault of
    1: Exit(0);
    2: if Calls > 1 then Exit(0);
    3: Exit(-1);
    4: Exit(Count + 1);
    5: raise EWriteError.Create('injected write error');
  end;
  if (Chunk > 0) and (Count > Chunk) then Count := Chunk;
  Result := inherited Write(Buffer, Count);
end;

procedure Check(const ID: string; Passed: Boolean; const Detail: string = '');
var Item: TJSONObject;
begin
  Item := TJSONObject.Create;
  Item.Add('id', ID);
  if Passed then Item.Add('status', 'pass') else begin Item.Add('status', 'fail'); Inc(Failures) end;
  Item.Add('actual', Detail);
  Cases.Add(Item);
end;

function EqualStreams(A, B: TMemoryStream): Boolean;
begin
  Result := (A.Size = B.Size) and ((A.Size = 0) or CompareMem(A.Memory, B.Memory, A.Size));
end;

procedure TransferCases(TextMode: Boolean);
var Source, Target: TFreeFileBuffer; Encoded, Actual: TMemoryStream;
  Stream: TLimitedStream; Prefix, ErrorClass: string; I, V, SavedMode: Integer;
  Raised: Boolean; Text: string;
begin
  if TextMode then begin Source := TFreeTextBuffer.Create; Target := TFreeTextBuffer.Create; Prefix := 'text' end
  else begin Source := TFreeFileBuffer.Create; Target := TFreeFileBuffer.Create; Prefix := 'binary' end;
  Encoded := TMemoryStream.Create; Actual := TMemoryStream.Create; Stream := TLimitedStream.Create;
  try
    for I := 1 to 3000 do Source.Add(Integer(I * 37));
    Source.Add('abcd'); Source.SaveToStream(Encoded);
    Stream.CopyFrom(Encoded, 0); Stream.Position := 0; Stream.Chunk := 3; Stream.Calls := 0;
    Target.LoadFromStream(Stream); Target.SaveToStream(Actual);
    Check(Prefix+'.short-read-complete', EqualStreams(Encoded, Actual) and (Stream.Calls > 1));
    Target.Reset; Target.LoadInteger(V); Check(Prefix+'.short-read-value', V = 37);
    Stream.Clear; Stream.Calls := 0; Source.SaveToStream(Stream);
    Check(Prefix+'.short-write-complete', EqualStreams(Encoded, Stream) and (Stream.Calls > 1));
    for I := 1 to 5 do
    begin
      Stream.Fault := 0; Stream.Clear; Stream.CopyFrom(Encoded, 0); Stream.Position := 0;
      Stream.Calls := 0; Stream.Fault := I; Raised := False; ErrorClass := '';
      Target.Clear; Target.Add(4321);
      try Target.LoadFromStream(Stream) except on E: Exception do begin Raised := E is EReadError; ErrorClass := E.ClassName end end;
      Target.Reset; Target.LoadInteger(V);
      Check(Prefix+'.read-fault-'+IntToStr(I), Raised and (V=4321), ErrorClass);
      Stream.Fault := 0; Stream.Clear; Stream.Calls := 0; Stream.Fault := I;
      Raised := False; ErrorClass := '';
      try Source.SaveToStream(Stream) except on E: Exception do begin Raised := E is EWriteError; ErrorClass := E.ClassName end end;
      Check(Prefix+'.write-fault-'+IntToStr(I), Raised, ErrorClass);
    end;
    Stream.Fault := 0; Stream.Clear; Stream.PretendSize := Int64(High(Integer)) + 1;
    Raised := False;
    try Target.LoadFromStream(Stream) except on E: EReadError do Raised := True end;
    Check(Prefix+'.oversized-file-rejected', Raised); Stream.PretendSize := 0;
    Source.Clear; Stream.Clear; Source.SaveToStream(Stream);
    Target.LoadFromStream(Stream); Actual.Clear; Target.SaveToStream(Actual);
    Check(Prefix+'.empty-stream', Actual.Size=0);
    Source.Add(789); SavedMode := FileMode;
    Source.SaveToFile(ParamStr(1)+PathDelim+Prefix+'.tmp');
    Target.LoadFromFile(ParamStr(1)+PathDelim+Prefix+'.tmp'); Target.Reset; Target.LoadInteger(V);
    Check(Prefix+'.file-round-trip', (V=789) and (SavedMode=FileMode));
    Raised := False; ErrorClass := '';
    try Target.LoadFromFile(ParamStr(1)+PathDelim+'does-not-exist')
    except on E: Exception do begin Raised := E is EFOpenError; ErrorClass := E.ClassName end end;
    Check(Prefix+'.open-failure-preserves-error', Raised, ErrorClass);
    Raised := False; ErrorClass := '';
    try Source.SaveToFile(ParamStr(1)+PathDelim+'missing-directory'+PathDelim+'model.tmp')
    except on E: Exception do begin Raised := E is EFCreateError; ErrorClass := E.ClassName end end;
    Check(Prefix+'.create-failure-preserves-error', Raised, ErrorClass);
    Check(Prefix+'.file-handles-closed', RenameFile(ParamStr(1)+PathDelim+Prefix+'.tmp', ParamStr(1)+PathDelim+Prefix+'.closed'));
  finally Stream.Free; Actual.Free; Encoded.Free; Target.Free; Source.Free end;
end;

procedure BoundsCases;
var Buffer: TFreeFileBuffer; TextBuffer: TFreeTextBuffer; J, K: TJPEGImage;
  Encoded: TStringStream; ImageBytes: TMemoryStream; Raised: Boolean; I, N: Integer;
  S: string; Words: TStrings; Data: TFreeDelftSeriesResistanceData;
begin
  Buffer := TFreeFileBuffer.Create; TextBuffer := TFreeTextBuffer.Create;
  J := TJPEGImage.Create; K := TJPEGImage.Create; Words := TStringList.Create; ImageBytes := TMemoryStream.Create;
  try
    for I := 0 to 2 do
    begin
      case I of 0: N := -1; 1: N := High(Integer); 2: N := 8 end;
      Buffer.Clear; Buffer.Add(8); Buffer.Add(6); Buffer.Add(N); Buffer.Add(0); Buffer.Reset;
      Raised := False;
      try Buffer.LoadTJPEGImage(J) except on E: EReadError do Raised := True end;
      Check('bounds.jpeg-length-'+IntToStr(I), Raised and (Buffer.Position=12));
      Buffer.Clear; Buffer.Add(N); Buffer.Reset; Raised := False;
      try Buffer.LoadString(S) except on E: EReadError do Raised := True end;
      Check('bounds.string-length-'+IntToStr(I), Raised and (Buffer.Position=4));
      Buffer.Reset; Raised := False;
      try Buffer.LoadTStrings(Words) except on E: EReadError do Raised := True end;
      Check('bounds.string-count-'+IntToStr(I), Raised and (Words.Count=0));
    end;
    Buffer.Clear; Buffer.Add(123); Buffer.Reset; Raised := False;
    try Buffer.LoadTFreeDelftSeriesResistanceData(Data) except on E: EReadError do Raised := True end;
    Check('bounds.truncated-record', Raised and (Buffer.Position=0));
    Buffer.Clear; Buffer.Add(''); Buffer.Reset; Buffer.LoadString(S);
    Check('bounds.empty-string-valid', (S='') and (Buffer.Position=4));
    J.LoadFromFile(ParamStr(2)); Buffer.Clear; Buffer.Add(J); Buffer.Reset; Buffer.LoadTJPEGImage(K);
    Check('bounds.valid-binary-jpeg', (J.Width=K.Width) and (J.Height=K.Height) and (Buffer.Position=Buffer.Count));
    TextBuffer.Add(J); TextBuffer.Reset; TextBuffer.LoadTJPEGImage(K);
    Check('bounds.valid-text-jpeg', (J.Width=K.Width) and (J.Height=K.Height));
    for I := 0 to 2 do
    begin
      case I of 0: S := '-1'; 1: S := IntToStr(High(Integer)); 2: S := '2' end;
      Encoded := TStringStream.Create('8'+LineEnding+'6'+LineEnding+S+LineEnding+'zz'+LineEnding);
      try TextBuffer.LoadFromStream(Encoded) finally Encoded.Free end;
      TextBuffer.Reset; Raised := False;
      try TextBuffer.LoadTJPEGImage(K) except on E: EReadError do Raised := True end;
      Check('bounds.text-jpeg-length-'+IntToStr(I), Raised);
    end;
    Encoded := TStringStream.Create('8'+LineEnding+'6'+LineEnding+'1'+LineEnding+'zz'+LineEnding);
    try TextBuffer.LoadFromStream(Encoded) finally Encoded.Free end;
    TextBuffer.Reset; Raised := False;
    try TextBuffer.LoadTJPEGImage(K) except on E: EReadError do Raised := True end;
    Check('bounds.text-jpeg-invalid-hex', Raised);
  finally ImageBytes.Free; Words.Free; K.Free; J.Free; TextBuffer.Free; Buffer.Free end;
end;

procedure RecordCases(TextMode: Boolean);
var Source, Target: TFreeFileBuffer; Encoded: TMemoryStream;
  Delft, LoadedDelft: TFreeDelftSeriesResistanceData;
  Kaper, LoadedKaper: TFreeKAPERResistanceData;
  Words, LoadedWords: TStrings; Prefix, ErrorClass: string; V: Integer;
  Rejected: Boolean;
begin
  if TextMode then begin Source:=TFreeTextBuffer.Create; Target:=TFreeTextBuffer.Create; Prefix:='text' end
  else begin Source:=TFreeFileBuffer.Create; Target:=TFreeFileBuffer.Create; Prefix:='binary' end;
  Encoded:=TMemoryStream.Create; Words:=TStringList.Create; LoadedWords:=TStringList.Create;
  try
    FillChar(Delft,SizeOf(Delft),0); FillChar(Kaper,SizeOf(Kaper),0);
    Delft.StartSpeed:=1.25; Delft.EndSpeed:=12.5; Delft.StepSpeed:=0.5;
    Delft.Bwl:=7.25; Delft.Cp:=0.625; Delft.Displacement:=123.5;
    Delft.WlArea:=45.75; Delft.EstimateWetSurf:=True; Delft.Extract:=False;
    Kaper.Draft:=2.5; Kaper.Lwl:=70.25; Kaper.EntranceAngle:=11.25; Kaper.Extract:=True;
    Words.Add('alpha'); Words.Add('beta');
    Source.Add(Delft); Source.Add(Kaper); Source.Add(Words); Source.Add(991);
    Source.SaveToStream(Encoded); Encoded.Position:=0; Target.LoadFromStream(Encoded);
    Target.LoadTFreeDelftSeriesResistanceData(LoadedDelft);
    Check(Prefix+'.nonzero-delft-record',
      (LoadedDelft.StartSpeed=1.25) and (LoadedDelft.EndSpeed=12.5) and
      (LoadedDelft.StepSpeed=0.5) and (LoadedDelft.Bwl=7.25) and
      (LoadedDelft.Cp=0.625) and (LoadedDelft.Displacement=123.5) and
      (LoadedDelft.WlArea=45.75) and LoadedDelft.EstimateWetSurf and not LoadedDelft.Extract);
    Target.LoadTFreeKAPERResistanceData(LoadedKaper);
    Check(Prefix+'.nonzero-kaper-record',
      (LoadedKaper.Draft=2.5) and (LoadedKaper.Lwl=70.25) and
      (LoadedKaper.EntranceAngle=11.25) and LoadedKaper.Extract);
    Target.LoadTStrings(LoadedWords);
    Check(Prefix+'.string-list-after-records',
      (LoadedWords.Count=2) and (LoadedWords[0]='alpha') and (LoadedWords[1]='beta'));
    Target.LoadInteger(V); Check(Prefix+'.record-following-sentinel',V=991);
    Source.Clear; Source.Add(Delft); Encoded.Clear; Source.SaveToStream(Encoded);
    Encoded.Size:=Encoded.Size-4; Encoded.Position:=0; Target.LoadFromStream(Encoded);
    Rejected:=False; ErrorClass:='';
    try Target.LoadTFreeDelftSeriesResistanceData(LoadedDelft)
    except on E:Exception do begin
      Rejected:=(E is EReadError) or (TextMode and (E is EStringListError)); ErrorClass:=E.ClassName;
    end end;
    Check(Prefix+'.truncated-record-after-stream-load',Rejected,ErrorClass);
  finally LoadedWords.Free; Words.Free; Encoded.Free; Target.Free; Source.Free end;
end;

procedure LegacyLayoutCases;
var B:TFreeFileBuffer; M:TMemoryStream; Variant,I,Mode:Integer;
  H:TFreeHoltrSeriesResistanceData; O:TFreeOSTSeriesResistanceData;
  T1:TFreeTask1PropellerData; T2:TFreeTask2PropellerData; T3:TFreeTask3PropellerData;
  P:TFreePlaningResistanceData; R:TFreeRvrsPropellerData;
  HB:TFreeHollenSeriesResistanceData; T4:TFreeTask4PropellerData; T5:TFreeTask5PropellerData;
  Version:TFreeFileVersion; Bad:Boolean; Name:string; Bits:LongWord;
  procedure PutRecord(Floats,Flags:Integer);
  var K:Integer; Value:Single; Flag,Padding:Byte;
  begin
    for K:=1 to Floats do begin Value:=K+0.25;M.WriteBuffer(Value,4) end;
    for K:=1 to Flags do begin Flag:=Byte(K=1);M.WriteBuffer(Flag,1) end;
    Padding:=$CC;for K:=Flags+1 to 4 do M.WriteBuffer(Padding,1);
  end;
  procedure Make(V:Integer);
  begin
    M.Clear;PutRecord(43,2);PutRecord(52,2);PutRecord(15,1);
    if V=4 then PutRecord(15,1) else PutRecord(25,1);
    if V>=3 then PutRecord(15,1) else PutRecord(16,1);
    if V<3 then begin
      PutRecord(13-2*V,1);PutRecord(16,1);PutRecord(43,2);PutRecord(15,1);PutRecord(5,1);
      Version:=fv305;
    end else Version:=fv270;
    M.Position:=0;
  end;
  procedure Prepare;
  begin M.Position:=0;B.LoadFromStream(M);B.Version:=Version;B.PrepareLegacyCalculations end;
  procedure ReadAll;
  begin
    B.LoadTFreeHoltrSeriesResistanceData(H);B.LoadTFreeOSTSeriesResistanceData(O);
    B.LoadTFreeTask1PropellerData(T1);B.LoadTFreeTask2PropellerData(T2);B.LoadTFreeTask3PropellerData(T3);
    if Version=fv305 then begin
      B.LoadTFreePlaningResistanceData(P);B.LoadTFreeRvrsPropellerData(R);
      B.LoadTFreeHollenSeriesResistanceData(HB);B.LoadTFreeTask4PropellerData(T4);B.LoadTFreeTask5PropellerData(T5);
    end;
  end;
begin
  B:=TFreeFileBuffer.Create;M:=TMemoryStream.Create;
  try
    for Variant:=0 to 4 do begin
      Name:='legacy-layout.'+IntToStr(Variant);Make(Variant);Prepare;ReadAll;
      Check(Name+'.position',B.Position=B.Count);
      Check(Name+'.prefix',(H.A11=43.25) and (O.Dat18_5=52.25) and
        (T1.Dat16=15.25) and (T2.Dat16=15.25) and (T3.Dat16=15.25));
      if Variant<3 then
        Check(Name+'.suffix',(P.Draft=1.25) and (P.EntranceAngle=9.25) and P.Extract and
          (R.Dat2=1.25) and (T5.Dat6=5.25) and T5.Extract and
          (((Variant=0) and (P.K=13.25)) or ((Variant>0) and (P.K=0))))
      else
        Check(Name+'.suffix',(T3.Dat17=0) and T3.Extract and
          (((Variant=3) and (T2.Dat18_5=25.25)) or ((Variant=4) and (T2.Dat18_5=0))));
      Check(Name+'.notice',(B.LegacyCalculationLayout<>'')=(Variant>0));
    end;
    for Mode:=0 to 3 do begin
      Make(1);
      case Mode of
        0:M.Size:=M.Size-1;
        1:begin M.Position:=0;Bits:=$7FC00000;M.WriteBuffer(Bits,4) end;
        2:PByte(M.Memory)[172]:=2;
        3:M.Size:=M.Size+4;
      end;
      Bad:=False;try Prepare except on E:EReadError do Bad:=True end;
      Check('legacy-layout.reject.'+IntToStr(Mode),Bad);
    end;
    Make(1);M.Position:=M.Size-24;Bits:=$7FC00000;M.WriteBuffer(Bits,4);
    Prepare;ReadAll;
    Check('legacy-layout.invalid-task5-explicit',(T5.Dat2=0) and not T5.Extract and
      (Pos('invalid Propeller Task5',B.LegacyCalculationLayout)>0));
  finally M.Free;B.Free end;
end;

var Report: TJSONObject; Output: TStringList;
begin
  Cases := TJSONArray.Create; Report := TJSONObject.Create; Output := TStringList.Create;
  try
    try
      TransferCases(False); TransferCases(True); BoundsCases;
      RecordCases(False); RecordCases(True); LegacyLayoutCases;
    except on E: Exception do begin Report.Add('error', E.ClassName+': '+E.Message); Inc(Failures) end end;
    Report.Add('cases', Cases); Report.Add('failures', Failures);
    Output.Text := Report.FormatJSON; Output.SaveToFile(ParamStr(1)+PathDelim+'probe.json');
  finally Output.Free; Report.Free end;
  if Failures > 0 then Halt(1);
end.
