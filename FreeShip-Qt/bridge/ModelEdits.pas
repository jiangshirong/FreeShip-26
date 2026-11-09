{ FreeShip Qt migration adapter. GPL-2.0-or-later; see COPYING. }
unit ModelEdits;
{$mode objfpc}{$H+}{$codepage utf8}
interface
uses fpjson,FreeShipUnit,FreeGeometry,FreeVersionUnit;
procedure RequireVersion(Ship:TFreeShip;Version:TFreeFileVersion);
function ProjectValues(Ship:TFreeShip):TJSONObject;
function LayerValues(Layer:TFreeSubdivisionLayer):TJSONObject;
function ApplyProjectChanges(Ship:TFreeShip;Changes:TJSONObject):Boolean;
function ApplyLayerChanges(Ship:TFreeShip;Layer:TFreeSubdivisionLayer;Changes:TJSONObject):Boolean;
function TextValue(Ship:TFreeShip;Changes:TJSONObject;const Name:string;WillStore:Boolean=True):string;
implementation
uses SysUtils,Math,Graphics,FreeTypes,LConvEncoding;

function Required(Changes:TJSONObject;const Name:string;Kind:TJSONType):TJSONData;
begin
  Result:=Changes.Find(Name);
  if (Result=nil) or (Result.JSONType<>Kind) then raise Exception.Create('Invalid '+Name);
end;
function Numeric(Changes:TJSONObject;const Name:string;Nonnegative:Boolean=True):TFloatType;
var N:Double;
begin
  N:=Required(Changes,Name,jtNumber).AsFloat;
  if IsNan(N) or IsInfinite(N) or (Abs(N)>1e9) or (Nonnegative and (N<0)) then
    raise Exception.Create('Invalid bounded number '+Name);
  Result:=N;
end;
function IntegerValue(Changes:TJSONObject;const Name:string;Maximum:Integer):Integer;
var N:Double;
begin
  N:=Required(Changes,Name,jtNumber).AsFloat;
  if IsNan(N) or IsInfinite(N) or (N<0) or (N>Maximum) or (Frac(N)<>0) then
    raise Exception.Create('Invalid integer '+Name);
  Result:=Trunc(N);
end;
function TextValue(Ship:TFreeShip;Changes:TJSONObject;const Name:string;WillStore:Boolean):string;
var Encoded,Decoded,Part:string;Wide:UnicodeString;I,N:Integer;
begin
  Result:=Required(Changes,Name,jtString).AsString;
  if Pos(#0,Result)>0 then raise Exception.Create('Text field '+Name+' contains NUL');
  if UTF8Encode(UTF8Decode(Result))<>Result then raise Exception.Create('Text field '+Name+' is not valid UTF-8');
  if not WillStore then Exit;
  Encoded:=ConvertEncoding(Result,'utf8',Ship.Preferences.FbmEncoding);
  Decoded:=ConvertEncoding(Encoded,Ship.Preferences.FbmEncoding,'utf8');
  if Decoded=Result then Exit;
  // FBM has no per-file encoding tag. Never accept text that the next history
  // capture or save would silently replace with question marks.
  Wide:=UTF8Decode(Result);I:=1;Part:='';
  while I<=Length(Wide) do begin
    N:=1;
    if (Ord(Wide[I])>=$D800) and (Ord(Wide[I])<=$DBFF) and (I<Length(Wide)) then N:=2;
    Part:=UTF8Encode(Copy(Wide,I,N));
    if ConvertEncoding(ConvertEncoding(Part,'utf8',Ship.Preferences.FbmEncoding),Ship.Preferences.FbmEncoding,'utf8')<>Part then Break;
    Inc(I,N);
  end;
  raise Exception.Create('字段 '+Name+' 含当前 FBM 编码 '+Ship.Preferences.FbmEncoding+' 无法保存的字符：'+Part);
end;
procedure UniqueKeys(Changes:TJSONObject);
var I,J:Integer;
begin
  for I:=0 to Changes.Count-1 do
    for J:=0 to I-1 do if Changes.Names[I]=Changes.Names[J] then
      raise Exception.Create('Duplicate change field '+Changes.Names[I]);
end;
procedure RequireVersion(Ship:TFreeShip;Version:TFreeFileVersion);
begin
  if Version<=Ship.FileVersion then Exit;
  // Versions before 2.10 had no preview record. There is no source JPEG to
  // retain when adding newer settings; do not invoke the old window screenshot.
  if (Ship.FileVersion<fv210) and (Version>=fv210) then Ship.ProjectSettings.SavePreview:=False;
  Ship.FileVersion:=Version;
end;
function ProjectValues(Ship:TFreeShip):TJSONObject;
var P:TFreeProjectSettings;
begin
  P:=Ship.ProjectSettings;Result:=TJSONObject.Create;
  Result.Add('name',P.ProjectName);Result.Add('designer',P.ProjectDesigner);Result.Add('comment',P.ProjectComment);
  if P.ProjectUnits=fuMetric then Result.Add('units','metric') else Result.Add('units','imperial');
  Result.Add('length',Double(P.ProjectLength));Result.Add('beam',Double(P.ProjectBeam));Result.Add('draft',Double(P.ProjectDraft));
  Result.Add('water_density',Double(P.ProjectWaterDensity));Result.Add('appendage_coefficient',Double(P.ProjectAppendageCoefficient));
  Result.Add('split_section_location',Double(P.ProjectSplitSectionLocation));
  Result.Add('use_default_split_section_location',P.UseDefaultSplitSectionLocation);
  Result.Add('simplify_intersections',P.ProjectSimplifyIntersections);Result.Add('precision',Ord(Ship.Precision));
  Result.Add('shade_underwater',P.ProjectShadeUnderwaterShip);Result.Add('underwater_color',Int64(ColorToRGB(P.ProjectUnderWaterColor)));
end;
function LayerValues(Layer:TFreeSubdivisionLayer):TJSONObject;
begin
  Result:=TJSONObject.Create;
  Result.Add('name',Layer.Name);Result.Add('color',Int64(ColorToRGB(Layer.Color)));
  if Layer.AlphaBlend<>255 then Result.Add('alpha_blend',Layer.AlphaBlend);
  Result.Add('visible',Layer.SurfaceVisible);Result.Add('symmetric',Layer.Symmetric);Result.Add('developable',Layer.Developable);
  Result.Add('use_in_hydrostatics',Layer.UseInHydrostatics);Result.Add('show_in_linesplan',Layer.ShowInLinesplan);
  Result.Add('use_for_intersections',Layer.UseForIntersections);
  Result.Add('thickness',Double(Layer.Thickness));Result.Add('material_density',Double(Layer.MaterialDensity));
end;
function ApplyProjectChanges(Ship:TFreeShip;Changes:TJSONObject):Boolean;
var I:Integer;Key,Text,Before,After:string;Values:TJSONObject;P:TFreeProjectSettings;StoredSplit:TFloatType;
begin
  UniqueKeys(Changes);P:=Ship.ProjectSettings;
  // Validate the entire patch before the first setter, independently of key order.
  for I:=0 to Changes.Count-1 do begin
    Key:=Changes.Names[I];
    if (Key='name') or (Key='designer') or (Key='comment') then TextValue(Ship,Changes,Key)
    else if Key='units' then begin
      Text:=Required(Changes,Key,jtString).AsString;
      if (Text<>'metric') and (Text<>'imperial') then raise Exception.Create('Invalid units');
    end else if (Key='length') or (Key='beam') or (Key='draft') or (Key='water_density') or (Key='appendage_coefficient') then Numeric(Changes,Key)
    else if Key='split_section_location' then Numeric(Changes,Key,False)
    else if Key='precision' then IntegerValue(Changes,Key,3)
    else if Key='underwater_color' then IntegerValue(Changes,Key,$FFFFFF)
    else if (Key='use_default_split_section_location') or (Key='simplify_intersections') or (Key='shade_underwater') then Required(Changes,Key,jtBoolean)
    else raise Exception.Create('Unsupported project field '+Key);
  end;
  Values:=ProjectValues(Ship);try Before:=Values.AsJSON finally Values.Free end;
  StoredSplit:=P.ServiceStoredSplitSectionLocation;
  // Explicit dimensional values use the requested destination units. The
  // original setter converts the whole document exactly once before overrides.
  if Changes.Find('units')<>nil then
    if Changes.Strings['units']='metric' then P.ProjectUnits:=fuMetric else P.ProjectUnits:=fuImperial;
  if Changes.Find('name')<>nil then P.ProjectName:=Changes.Strings['name'];
  if Changes.Find('designer')<>nil then P.ProjectDesigner:=Changes.Strings['designer'];
  if Changes.Find('comment')<>nil then P.ProjectComment:=Changes.Strings['comment'];
  if Changes.Find('length')<>nil then P.ProjectLength:=Numeric(Changes,'length');
  if Changes.Find('beam')<>nil then P.ProjectBeam:=Numeric(Changes,'beam');
  if Changes.Find('draft')<>nil then P.ProjectDraft:=Numeric(Changes,'draft');
  if Changes.Find('water_density')<>nil then P.ProjectWaterDensity:=Numeric(Changes,'water_density');
  if Changes.Find('appendage_coefficient')<>nil then P.ProjectAppendageCoefficient:=Numeric(Changes,'appendage_coefficient');
  if Changes.Find('split_section_location')<>nil then P.ProjectSplitSectionLocation:=Numeric(Changes,'split_section_location',False);
  if Changes.Find('use_default_split_section_location')<>nil then P.UseDefaultSplitSectionLocation:=Changes.Booleans['use_default_split_section_location'];
  if Changes.Find('simplify_intersections')<>nil then P.ProjectSimplifyIntersections:=Changes.Booleans['simplify_intersections'];
  if Changes.Find('precision')<>nil then Ship.Precision:=TFreePrecisionType(IntegerValue(Changes,'precision',3));
  if Changes.Find('shade_underwater')<>nil then P.ProjectShadeUnderwaterShip:=Changes.Booleans['shade_underwater'];
  if Changes.Find('underwater_color')<>nil then
    if ColorToRGB(P.ProjectUnderWaterColor)<>IntegerValue(Changes,'underwater_color',$FFFFFF) then
      P.ProjectUnderWaterColor:=IntegerValue(Changes,'underwater_color',$FFFFFF);
  Values:=ProjectValues(Ship);try After:=Values.AsJSON finally Values.Free end;
  Result:=(Before<>After) or (StoredSplit<>P.ServiceStoredSplitSectionLocation);
  if Result then begin
    RequireVersion(Ship,fv120);
    if not P.UseDefaultSplitSectionLocation or (StoredSplit<>P.ServiceStoredSplitSectionLocation) then RequireVersion(Ship,fv160);
    if not P.ProjectSimplifyIntersections then RequireVersion(Ship,fv230);
  end;
end;
function ApplyLayerChanges(Ship:TFreeShip;Layer:TFreeSubdivisionLayer;Changes:TJSONObject):Boolean;
var I:Integer;Key,Before,After,StoredName:string;Values:TJSONObject;
begin
  UniqueKeys(Changes);
  for I:=0 to Changes.Count-1 do begin
    Key:=Changes.Names[I];
    if Key='name' then TextValue(Ship,Changes,Key,
      not ((Layer.ServiceStoredName='') and (Required(Changes,Key,jtString).AsString=Layer.Name)))
    else if Key='color' then IntegerValue(Changes,Key,$FFFFFF)
    else if (Key='thickness') or (Key='material_density') then Numeric(Changes,Key)
    else if (Key='visible') or (Key='symmetric') or (Key='developable') or (Key='use_in_hydrostatics') or
      (Key='show_in_linesplan') or (Key='use_for_intersections') then Required(Changes,Key,jtBoolean)
    else raise Exception.Create('Unsupported layer field '+Key);
  end;
  Values:=LayerValues(Layer);try Before:=Values.AsJSON finally Values.Free end;
  StoredName:=Layer.ServiceStoredName;
  // The getter generates a translated label for an unnamed layer. Repeating
  // that label must preserve its empty stored name (even in a legacy encoding).
  if Changes.Find('name')<>nil then
    if not ((StoredName='') and (Changes.Strings['name']=Layer.Name)) then
      Layer.Name:=Changes.Strings['name'];
  // An inherited Windows system color carries a theme-dependent flag. An
  // equivalent resolved RGB is a no-op, not permission to replace that flag.
  if Changes.Find('color')<>nil then
    if ColorToRGB(Layer.Color)<>IntegerValue(Changes,'color',$FFFFFF) then
      Layer.Color:=IntegerValue(Changes,'color',$FFFFFF);
  if Changes.Find('visible')<>nil then Layer.SurfaceVisible:=Changes.Booleans['visible'];
  if Changes.Find('developable')<>nil then Layer.Developable:=Changes.Booleans['developable'];
  if Changes.Find('use_in_hydrostatics')<>nil then Layer.UseInHydrostatics:=Changes.Booleans['use_in_hydrostatics'];
  if Changes.Find('show_in_linesplan')<>nil then Layer.ShowInLinesplan:=Changes.Booleans['show_in_linesplan'];
  if Changes.Find('use_for_intersections')<>nil then Layer.UseForIntersections:=Changes.Booleans['use_for_intersections'];
  if Changes.Find('thickness')<>nil then
    if Layer.Thickness<>Numeric(Changes,'thickness') then Layer.Thickness:=Numeric(Changes,'thickness');
  if Changes.Find('material_density')<>nil then
    if Layer.MaterialDensity<>Numeric(Changes,'material_density') then Layer.MaterialDensity:=Numeric(Changes,'material_density');
  // Preserve the old automatic symmetry on enabling hydrostatics, but honor an
  // explicit symmetry choice last. Non-symmetric hydrostatic layers are valid.
  if Changes.Find('symmetric')<>nil then Layer.Symmetric:=Changes.Booleans['symmetric'];
  Values:=LayerValues(Layer);try After:=Values.AsJSON finally Values.Free end;
  Result:=(Before<>After) or (StoredName<>Layer.ServiceStoredName);
  if Result then begin
    if not Layer.UseInHydrostatics or not Layer.UseForIntersections then RequireVersion(Ship,fv180);
    if (Layer.Thickness<>0) or (Layer.MaterialDensity<>0) then RequireVersion(Ship,fv191);
    if not Layer.ShowInLinesplan then RequireVersion(Ship,fv201);
  end;
end;
end.
