{ GPL-2.0-or-later. Headless adaptation of FreeBackgroundImageData / FreeEdit.
  Origins remain integer image pixels; scale remains Single model units/pixel. }
function BackgroundBytes(Data:TFreeBackgroundImageData):RawByteString;
var Stream:TMemoryStream;
begin
  Stream:=TMemoryStream.Create;
  try
    Data.Image.SaveToStream(Stream);
    if (Stream.Size=0) or (Stream.Size>32*1024*1024) then raise Exception.Create('Invalid background JPEG size');
    SetLength(Result,Stream.Size);Stream.Position:=0;Stream.ReadBuffer(Result[1],Length(Result));
  finally Stream.Free end;
end;
procedure CheckBackground(Data:TFreeBackgroundImageData);
begin
  if not (Data.AssignedView in [fvBodyplan,fvProfile,fvPlan]) then raise Exception.Create('Invalid background view');
  if Data.Image.Empty or (Data.Image.Width<=0) or (Data.Image.Height<=0) or
    (Data.Image.Width>16384) or (Data.Image.Height>16384) or
    (Int64(Data.Image.Width)*Data.Image.Height>32*1024*1024) then raise Exception.Create('Background dimensions exceed limits');
  CheckCoordinate(Data.Scale);
  if Data.Scale<1e-9 then raise Exception.Create('Background scale must be positive');
  if (Abs(Int64(Data.Origin.X))>1000000000) or (Abs(Int64(Data.Origin.Y))>1000000000) then raise Exception.Create('Background origin exceeds limits');
  if (Data.BlendingValue<0) or (Data.BlendingValue>255) or (Data.Tolerance<0) or (Data.Tolerance>255) or
     (Data.Quality<1) or (Data.Quality>100) or (Data.TransparentColor<0) or (Data.TransparentColor>$FFFFFF) then
    raise Exception.Create('Invalid background appearance');
end;
procedure TDocument.WriteBackgrounds(Root:TJSONObject);
var Items:TJSONArray;Obj:TJSONObject;Data:TFreeBackgroundImageData;I:Integer;Seen:set of TFreeViewType;
begin
  Items:=TJSONArray.Create;Root.Add('backgrounds',Items);Seen:=[];
  if Ship.NumberOfBackgroundImages>3 then raise Exception.Create('Too many background images');
  for I:=0 to Ship.NumberOfBackgroundImages-1 do begin
    Data:=Ship.BackgroundImage[I];CheckBackground(Data);
    if Data.AssignedView in Seen then raise Exception.Create('Duplicate background view');
    Include(Seen,Data.AssignedView);Obj:=TJSONObject.Create;Items.Add(Obj);
    Obj.Add('view',Ord(Data.AssignedView));Obj.Add('visible',Data.ServiceVisible);
    Obj.Add('origin',TJSONArray.Create([Data.Origin.X,Data.Origin.Y]));Obj.Add('scale',Double(Data.Scale));
    Obj.Add('alpha',Data.BlendingValue);Obj.Add('transparent',Data.Transparent);
    Obj.Add('transparent_color',Int64(Data.TransparentColor));Obj.Add('tolerance',Data.Tolerance);
    Obj.Add('quality',Data.Quality);Obj.Add('jpeg',EncodeStringBase64(BackgroundBytes(Data)));
  end;
end;
function TDocument.EditBackground(Request:TJSONObject):Boolean;
var Data,Loaded:TFreeBackgroundImageData;Operation,Name:string;I,Index,View:Integer;
    Wide:Int64;Changes:TJSONObject;Origin:TPoint;Scale:TFloatType;Alpha,Tolerance,Color:Integer;
    Visible,Transparent:Boolean;Pair:TJSONArray;Stream:TFileStream;Bitmap:Graphics.TBitmap;V:Double;
begin
  Operation:=StringField(Request,'operation');Wide:=IntegerField(Request,'view');
  if Wide>2 then raise Exception.Create('Background requires an orthographic view');View:=Wide;
  Data:=nil;Index:=-1;
  for I:=0 to Ship.NumberOfBackgroundImages-1 do if Ord(Ship.BackgroundImage[I].AssignedView)=View then begin
    if Data<>nil then raise Exception.Create('Duplicate background view');Data:=Ship.BackgroundImage[I];Index:=I;
  end;
  if Operation='remove' then begin
    Result:=Data<>nil;if not Result then Exit;
    Ship.ServiceBackgrounds.Delete(Index);Data.Free;Exit;
  end;
  if Operation='import' then begin
    // Decode into a private object before touching the current image or history.
    Loaded:=TFreeBackgroundImageData.Create(Ship);
    try
      Loaded.Clear;Stream:=TFileStream.Create(StringField(Request,'path'),fmOpenRead or fmShareDenyWrite);
      try
        if (Stream.Size<4) or (Stream.Size>32*1024*1024) then raise Exception.Create('Invalid image size');
        Loaded.Image.LoadFromStream(Stream);
      finally Stream.Free end;
      Loaded.ServiceView:=TFreeViewType(View);Loaded.ServiceQuality:=100;
      Origin.X:=0;Origin.Y:=Loaded.Image.Height;Loaded.ServiceOrigin:=Origin;
      V:=Field(Request,'scale',jtNumber).AsFloat;CheckCoordinate(V);Loaded.ServiceScale:=V;
      CheckBackground(Loaded);
      Bitmap:=Graphics.TBitmap.Create;try Bitmap.Assign(Loaded.Image) finally Bitmap.Free end;
      if (Data<>nil) and (BackgroundBytes(Data)=BackgroundBytes(Loaded)) and
         (Data.Origin.X=0) and (Data.Origin.Y=Origin.Y) and (Data.Scale=Loaded.Scale) and
         Data.ServiceVisible and (Data.BlendingValue=255) and not Data.Transparent and
         (Data.TransparentColor=0) and (Data.Tolerance=3) and (Data.Quality=100) then Exit(False);
      if Data<>nil then begin Ship.ServiceBackgrounds.Delete(Index);Data.Free end;
      Ship.ServiceBackgrounds.Add(Loaded);Loaded:=nil;
      RequireVersion(Ship,fv250);
      Result:=True;
    finally Loaded.Free end;
    Exit;
  end;
  if Operation<>'update' then raise Exception.Create('Unknown background operation');
  if Data=nil then raise Exception.Create('No background image in this view');
  Changes:=TJSONObject(Field(Request,'changes',jtObject));
  Origin:=Data.Origin;Scale:=Data.Scale;Alpha:=Data.BlendingValue;Tolerance:=Data.Tolerance;
  Color:=Data.TransparentColor;Visible:=Data.ServiceVisible;Transparent:=Data.Transparent;
  for I:=0 to Changes.Count-1 do begin
    Name:=Changes.Names[I];
    if Name='origin' then begin
      Pair:=TJSONArray(Field(Changes,Name,jtArray));if Pair.Count<>2 then raise Exception.Create('Origin requires two pixels');
      if (Pair[0].JSONType<>jtNumber) or (Pair[1].JSONType<>jtNumber) then raise Exception.Create('Invalid origin');
      V:=Pair.Floats[0];CheckCoordinate(V);if Frac(V)<>0 then raise Exception.Create('Origin must use integer pixels');Origin.X:=Trunc(V);
      V:=Pair.Floats[1];CheckCoordinate(V);if Frac(V)<>0 then raise Exception.Create('Origin must use integer pixels');Origin.Y:=Trunc(V);
    end else if Name='scale' then begin
      V:=Field(Changes,Name,jtNumber).AsFloat;CheckCoordinate(V);Scale:=V;if Scale<1e-9 then raise Exception.Create('Scale must be positive');
    end else if Name='alpha' then begin Wide:=IntegerField(Changes,Name);if Wide>255 then raise Exception.Create('Invalid alpha');Alpha:=Wide end
    else if Name='tolerance' then begin Wide:=IntegerField(Changes,Name);if Wide>255 then raise Exception.Create('Invalid tolerance');Tolerance:=Wide end
    else if Name='transparent_color' then begin Wide:=IntegerField(Changes,Name);if Wide>$FFFFFF then raise Exception.Create('Invalid transparent color');Color:=Wide end
    else if Name='visible' then Visible:=Field(Changes,Name,jtBoolean).AsBoolean
    else if Name='transparent' then Transparent:=Field(Changes,Name,jtBoolean).AsBoolean
    else raise Exception.Create('Unknown background property '+Name);
  end;
  Result:=(Origin.X<>Data.Origin.X) or (Origin.Y<>Data.Origin.Y) or (Scale<>Data.Scale) or
    (Alpha<>Data.BlendingValue) or (Tolerance<>Data.Tolerance) or (Color<>Data.TransparentColor) or
    (Visible<>Data.ServiceVisible) or (Transparent<>Data.Transparent);
  if not Result then Exit;
  Data.ServiceOrigin:=Origin;Data.ServiceScale:=Scale;Data.ServiceAlpha:=Alpha;
  Data.ServiceTolerance:=Tolerance;Data.ServiceColor:=Color;Data.ServiceVisible:=Visible;Data.ServiceTransparent:=Transparent;
end;
