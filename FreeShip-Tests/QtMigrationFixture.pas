program QtMigrationFixture;
{$mode objfpc}{$H+}
// Test-only oracle and locked-input writer. Never accepts a user destination.
uses Interfaces,Forms,Graphics,SysUtils,Classes,fpjson,FreeShipUnit,FreeFileBuffer,
 FreeGeometry,FreeTypes,FreeSafeLanguage;
var S:TFreeShip;B:TFreeFileBuffer;I,J,Children,Triangles:Integer;
 F:TFreeSubdivisionControlFace;Counts:TJSONObject;Text:TStringList;Preview:TJPEGImage;
 OriginalLength,OriginalDraft:TFloatType;
begin
 try
  DefaultFormatSettings.DecimalSeparator:='.';
  Application.Initialize;S:=TFreeShip.Create(nil);GlobalFreeShip:=S;
  S.IsRebuildAsyncDisabled:=True;S.Preferences.Load;
  ApplyLanguageSafely('cn',ExtractFilePath(ParamStr(0))+'locale');
  B:=TFreeFileBuffer.Create;
  try B.LoadFromFile(ParamStr(1));S.LoadProject(B,False) finally B.Free end;
  S.RebuildModel;Children:=0;Triangles:=0;
  // Read the source core's mesh directly, without the new snapshot serializer.
  for I:=0 to S.Surface.NumberOfControlFaces-1 do begin
   F:=S.Surface.ControlFace[I];Inc(Children,F.ChildCount);
   for J:=0 to F.ChildCount-1 do
    if F.Child[J].NumberOfPoints>=3 then Inc(Triangles,F.Child[J].NumberOfPoints-2);
  end;
  Counts:=TJSONObject.Create;
  Counts.Add('control_points',S.Surface.NumberOfControlPoints);
  Counts.Add('control_edges',S.Surface.NumberOfControlEdges);
  Counts.Add('control_faces',S.Surface.NumberOfControlFaces);
  Counts.Add('layers',S.Surface.NumberOfLayers);
  Counts.Add('source_child_faces',Children);Counts.Add('source_triangle_count',Triangles);
  Counts.Add('save_preview',S.ProjectSettings.SavePreview);
  if S.ProjectSettings.SavePreview then begin
   Preview:=TJPEGImage.Create;
   try
    S.LoadPreview(ParamStr(1),Preview);
    Counts.Add('preview_width',Preview.Width);Counts.Add('preview_height',Preview.Height);
    Preview.SaveToFile(ParamStr(2)+'/embedded-preview.jpg');
   finally Preview.Free end;
  end;
  Text:=TStringList.Create;
  try Text.Text:=Counts.AsJSON;Text.SaveToFile(ParamStr(2)+'/reference-counts.json')
  finally Text.Free;Counts.Free end;
  if ParamStr(3)='inspect' then begin S.Free;Halt(0) end;
  if S.Surface.NumberOfControlPoints=0 then raise Exception.Create('No fixture point');
  S.Surface.ControlPoint[0].Locked:=True;
  B:=TFreeFileBuffer.Create;
  try S.SaveProject(B);B.SaveToFile(ParamStr(2)+'/locked.fbm') finally B.Free end;
  // Structurally valid FBMs with metadata the Qt snapshot contract cannot accept.
  OriginalLength:=S.ProjectSettings.ProjectLength;
  S.ProjectSettings.ProjectLength:=-1;
  B:=TFreeFileBuffer.Create;
  try S.SaveProject(B);B.SaveToFile(ParamStr(2)+'/negative-length.fbm') finally B.Free end;
  S.ProjectSettings.ProjectLength:=OriginalLength;
  OriginalDraft:=S.ProjectSettings.ProjectDraft;
  S.ProjectSettings.ProjectDraft:=1.0e10;
  B:=TFreeFileBuffer.Create;
  try S.SaveProject(B);B.SaveToFile(ParamStr(2)+'/unbounded-draft.fbm') finally B.Free end;
  S.ProjectSettings.ProjectDraft:=OriginalDraft;
  S.ProjectSettings.ProjectName:='invalid'+#0+'name';
  B:=TFreeFileBuffer.Create;
  try S.SaveProject(B);B.SaveToFile(ParamStr(2)+'/nul-name.fbm') finally B.Free end;
  S.Free;Halt(0);
 except on E:Exception do begin WriteLn(E.ClassName+': '+E.Message);Halt(1) end end;
end.
