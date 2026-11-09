unit UiWindowProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure RunWindowProbe(Form:TMainForm);
implementation
uses SysUtils, Classes, Math, fpjson, FreeTypes, FreeGeometry, Windows, FreeProcess, FreeFileBuffer, FreeShipUnit, Graphics, FreeHydrostaticsFrm;
procedure Add(C:TJSONArray; const ID:string; Actual,Expected:Double; Tolerance:Double);
var O:TJSONObject;
begin O:=TJSONObject.Create; O.Add('id',ID); if Abs(Actual-Expected)<=Tolerance then O.Add('status','pass') else O.Add('status','fail'); O.Add('actual',Actual); O.Add('expected',Expected); O.Add('tolerance',Tolerance); C.Add(O) end;
function EqualFiles(const First,Second:string):Boolean;
var X,Y:TFileStream; BX,BY:Byte; K:Int64;
begin
 Result:=False; X:=TFileStream.Create(First,fmOpenRead); Y:=TFileStream.Create(Second,fmOpenRead);
 try if X.Size<>Y.Size then Exit; for K:=1 to X.Size do begin X.ReadBuffer(BX,1); Y.ReadBuffer(BY,1); if BX<>BY then Exit end; Result:=True finally X.Free; Y.Free end;
end;
function CanReopenExclusive(const FileName:string):Boolean;
var Stream:TFileStream;
begin
  Result:=False;
  try
    Stream:=TFileStream.Create(FileName,fmOpenReadWrite or fmShareExclusive);
    try Result:=True finally Stream.Free end;
  except
    on E:Exception do Result:=False;
  end;
end;
type TCounter=class Count:Integer; procedure Changed(Sender:TObject); end;
procedure TCounter.Changed(Sender:TObject); begin Inc(Count) end;
procedure Mark(const X:string); var F:TextFile; begin AssignFile(F,IncludeTrailingPathDelimiter(SysUtils.GetEnvironmentVariable('FS_UI_PROBE_OUTPUT'))+'stage.txt'); Rewrite(F); Write(F,X); CloseFile(F) end;
function STLBounds(Lines:TStringList; out MinY,MaxY:Double):Boolean;
var T:TStringList; I,N:Integer; Y:Double; F:TFormatSettings; Line:string;
begin
  Result:=False; N:=0; F:=DefaultFormatSettings; F.DecimalSeparator:='.';
  T:=TStringList.Create;
  try T.Delimiter:=' '; T.StrictDelimiter:=False;
    for I:=0 to Lines.Count-1 do begin
      Line:=Trim(Lines[I]); if Pos('vertex ',Line)<>1 then Continue;
      T.DelimitedText:=Line;
      if (T.Count<>4) or not TryStrToFloat(T[2],Y,F) then Exit(False);
      if N=0 then begin MinY:=Y; MaxY:=Y end else begin if Y<MinY then MinY:=Y; if Y>MaxY then MaxY:=Y end;
      Inc(N);
    end;
    Result:=N>0;
  finally T.Free end;
end;
procedure RunWindowProbe(Form:TMainForm);
var R,O:TJSONObject; C:TJSONArray; S:TStringList; D,A,B,W,MinY1,MaxY1,MinY2,MaxY2:Double; N1,N2,N3,Code:Integer; Sub,Err,IOPath:string; Buffer:TFreeFileBuffer; Saved:Boolean; UndoObj:TFreeUndoObject; P:TFreeSubdivisionControlPoint; Coord:T3DCoordinate; BeforeCount:Integer; Counter:TCounter; J:TJPEGImage; H:TFreeHydrostaticCalc; I,Count1,Count2:Integer; Lines:TStringList; BeforeName,BeforeFile,AfterFile,LoadErr:string; OldUndoCount,OldUndoPos:Integer; OldFilenameSet,RedoPreserved,PreservedState:Boolean; BeforeX:Double; HF:TFreeHydrostaticsForm; Patches,Patches2:TFasterListTFreeDevelopedPatch;
begin
 R:=TJSONObject.Create; C:=TJSONArray.Create; R.Add('cases',C);
 try
  try
   Sub:=SysUtils.GetEnvironmentVariable('FS_UPSTREAM_SUBCASE');
   Mark('show'); Form.FormShow(Form); Mark('load'); Err:='';
   try Form.LoadNamedFile(SysUtils.GetEnvironmentVariable('FS_UI_PROBE_MODEL')) except on E:Exception do Err:=E.ClassName+': '+E.Message end;
   Mark('loaded');
   if Sub='legacy' then begin
     O:=TJSONObject.Create; O.Add('id','17.motorboat3-desktop-load'); O.Add('expected','historical Motorboat3 fixture loads and contains geometry');
     if (Err='') and Form.FreeShip.ModelIsLoaded and (Form.FreeShip.Surface.NumberOfControlPoints>0) then O.Add('status','review') else O.Add('status','fail');
     O.Add('note','successful loading and nonempty geometry do not establish historical model fidelity');
     O.Add('exception',Err); O.Add('loaded',Form.FreeShip.ModelIsLoaded); O.Add('points',Form.FreeShip.Surface.NumberOfControlPoints); C.Add(O);
   end else begin
   if (Err<>'') or not Form.FreeShip.ModelIsLoaded then raise Exception.Create('Fixture not loaded: '+Err);
   if Sub='drafts' then begin
     Form.FreeShip.ProjectSettings.DisableModelCheck:=True; HF:=TFreeHydrostaticsForm.Create(nil); UiDraftTrace:=TStringList.Create;
     try HF.UiSetShip(Form.FreeShip); HF.StartDraft:=1; HF.EndDraft:=2; HF.DraftStep:=0.5; HF.Trim:=0; HF.Button2Click(nil);
       O:=TJSONObject.Create; O.Add('id','12.no-duplicate-terminal-draft'); O.Add('expected','drafts 1, 1.5, 2 calculated once each'); O.Add('actual',UiDraftTrace.CommaText);
       if UiDraftTrace.Count<3 then O.Add('status','not_run') else if (UiDraftTrace.Count=3) and (Abs(StrToFloat(UiDraftTrace[0])-1)<0.001) and (Abs(StrToFloat(UiDraftTrace[1])-1.5)<0.001) and (Abs(StrToFloat(UiDraftTrace[2])-2)<0.001) then O.Add('status','pass') else O.Add('status','fail'); C.Add(O);
     finally UiDraftTrace.Free; UiDraftTrace:=nil; HF.Free end;
   end else if Sub='patch' then begin
     Patches:=TFasterListTFreeDevelopedPatch.Create; try Form.FreeShip.Surface.Rebuild; Form.FreeShip.Layer[0].Unroll(Patches);
       O:=TJSONObject.Create; O.Add('id','16.patch-error-observation'); O.Add('expected','observe edge and area errors for the box fixture'); O.Add('patch_count',Patches.Count);
       if Patches.Count=0 then O.Add('status','not_run') else begin
         O.Add('status','review'); O.Add('max_signed_edge_error',Double(Patches[0].MaxError)); O.Add('area_error',Double(Patches[0].TotalAreaError)); O.Add('max_area_error',Double(Patches[0].MaxAreaError));
         // The scaled fixture is a second observation, not proof of corresponding patch identity.
         for I:=0 to Form.FreeShip.Surface.NumberOfControlPoints-1 do begin
           P:=Form.FreeShip.Surface.ControlPoint[I]; Coord:=P.Coordinate;
           Coord.X:=Coord.X*2; Coord.Y:=Coord.Y*2; Coord.Z:=Coord.Z*2; P.Coordinate:=Coord;
         end;
         Form.FreeShip.Surface.Built:=False; Form.FreeShip.Surface.Rebuild;
         Patches2:=TFasterListTFreeDevelopedPatch.Create;
         try Form.FreeShip.Layer[0].Unroll(Patches2); O.Add('scaled_patch_count',Patches2.Count);
           if Patches2.Count>0 then begin
             O.Add('scaled_max_signed_edge_error',Double(Patches2[0].MaxError));
             O.Add('scaled_area_error',Double(Patches2[0].TotalAreaError));
             O.Add('scaled_max_area_error',Double(Patches2[0].MaxAreaError));
           end;
         finally Patches2.Free end;
         O.Add('note','2x geometry scaled in isolated model; first patch identity, scoring choice and scale invariance require further validation');
       end; C.Add(O);
     finally Patches.Free end;
   end else if Sub='midship' then begin
     H:=TFreeHydrostaticCalc.Create(Form.FreeShip);
     try H.UiSetModelBounds(100,120); H.CalculateMidshipLocation; D:=H.MidshipLocation;
       O:=TJSONObject.Create; O.Add('id','07.translated-bounds'); O.Add('expected','midship location lies inside translated hull bounds [100,120] when there are no stations');
       if Form.FreeShip.NumberOfStations<>0 then O.Add('status','not_run') else if (D>=100) and (D<=120) then O.Add('status','pass') else O.Add('status','fail'); O.Add('actual',D); O.Add('stations',Form.FreeShip.NumberOfStations); C.Add(O);
     finally H.Free end;
   end else if Sub='stl' then begin
     Form.FreeShip.ProjectSettings.DisableModelCheck:=True; Form.FreeShip.Surface.Rebuild;
     Form.FreeShip.Layer[0].Symmetric:=False; Form.FreeShip.Edit.File_ExportSTL;
     Lines:=TStringList.Create; try Lines.LoadFromFile(SysUtils.GetEnvironmentVariable('FS_UPSTREAM_STL')); Count1:=0;
       for I:=0 to Lines.Count-1 do if Pos('facet normal',Lines[I])>0 then Inc(Count1);
       if not STLBounds(Lines,MinY1,MaxY1) then raise Exception.Create('cannot parse first STL vertex bounds');
       Form.FreeShip.Layer[0].Symmetric:=True; Form.FreeShip.Edit.File_ExportSTL; Lines.LoadFromFile(SysUtils.GetEnvironmentVariable('FS_UPSTREAM_STL')); Count2:=0;
       for I:=0 to Lines.Count-1 do if Pos('facet normal',Lines[I])>0 then Inc(Count2);
       if not STLBounds(Lines,MinY2,MaxY2) then raise Exception.Create('cannot parse mirrored STL vertex bounds');
       O:=TJSONObject.Create; O.Add('id','13.stl-layer-symmetry'); O.Add('expected','symmetric setting changes mirrored facet count for the same half-model');
       if (Count1=0) or (MinY1< -0.0001) or (MaxY1<=0.0001) then O.Add('status','not_run') else if (Count2<=Count1) or (Abs(MinY2+MaxY1)>0.001) or (Abs(MaxY2-MaxY1)>0.001) then O.Add('status','fail') else O.Add('status','review');
       O.Add('note','facet count change is necessary evidence for this fixture but does not establish geometry, normals or duplicate-free output');
       O.Add('non_symmetric_facets',Count1); O.Add('symmetric_facets',Count2); O.Add('non_symmetric_min_y',MinY1); O.Add('non_symmetric_max_y',MaxY1); O.Add('symmetric_min_y',MinY2); O.Add('symmetric_max_y',MaxY2); C.Add(O);
     finally Lines.Free end;
   end else if Sub='hydro-files' then begin
     Form.FreeShip.Edit.Intersection_Add(fiStation,3); Form.FreeShip.Edit.Intersection_Add(fiWaterline,2);
     Form.FreeShip.ProjectSettings.EnableBonjeanSAC:=True; H:=TFreeHydrostaticCalc.Create(Form.FreeShip); Err:='';
     try H.Draft:=3; try H.Calculate except on E:Exception do Err:=E.ClassName+': '+E.Message end;
       O:=TJSONObject.Create; O.Add('id','15.hydro-no-implicit-files'); O.Add('expected','normal hydrostatic calculation does not write report files to working directory');
       if FileExists('SACs.txt') or FileExists('Bonjean.txt') then O.Add('status','fail') else if Err<>'' then O.Add('status','review') else O.Add('status','pass');
       O.Add('note','an exception without observed output requires independent diagnosis; it does not prove implicit file creation');
       O.Add('sac_exists',FileExists('SACs.txt')); O.Add('bonjean_exists',FileExists('Bonjean.txt')); O.Add('exception',Err); C.Add(O);
     finally H.Free end;
   end else if Sub='save-fault' then begin
     Form.FreeShip.FileVersion:=Form.FreeShip.DetectMinFileVersion; Form.FreeShip.FileChanged:=True;
     Windows.SetEnvironmentVariable('FS_UPSTREAM_SAVE_FAULT','1');
     Windows.SetEnvironmentVariable('FS_UPSTREAM_SAVE_FAULT_SEEN','');
     Windows.SetEnvironmentVariable('FS_UPSTREAM_SAVE_MESSAGE',''); Err:=''; Saved:=False;
     try Saved:=Form.FreeShip.Edit.File_Save except on E:Exception do Err:=E.ClassName+': '+E.Message end;
     O:=TJSONObject.Create; O.Add('id','01.save-serialization-failure'); O.Add('expected','failure preserves original file and modified state');
     if SysUtils.GetEnvironmentVariable('FS_UPSTREAM_SAVE_FAULT_SEEN')<>'1' then O.Add('status','not_run')
     else if not Saved and (Err='') and (Pos('injected serialization failure',SysUtils.GetEnvironmentVariable('FS_UPSTREAM_SAVE_MESSAGE'))>0)
       and FileExists(SysUtils.GetEnvironmentVariable('FS_UI_PROBE_MODEL')) and Form.FreeShip.FileChanged then O.Add('status','pass') else O.Add('status','fail');
     O.Add('file_exists',FileExists(SysUtils.GetEnvironmentVariable('FS_UI_PROBE_MODEL'))); O.Add('modified',Form.FreeShip.FileChanged); O.Add('exception',Err);
     O.Add('returned_success',Saved); O.Add('error_message',SysUtils.GetEnvironmentVariable('FS_UPSTREAM_SAVE_MESSAGE')); C.Add(O);
   end else if Sub='load-invalid' then begin
     P:=Form.FreeShip.Surface.ControlPoint[0]; P.Name:='UpstreamLoadUndo'; Coord:=P.Coordinate; BeforeX:=Coord.X;
     Form.FreeShip.Edit.Undo_Clear; Form.FreeShip.Edit.CreateUndoObject('load rollback fixture',True);
     Coord.X:=BeforeX+1; P.Coordinate:=Coord; Form.FreeShip.FileChanged:=True; Form.FreeShip.Edit.Undo;
     P:=nil; for I:=0 to Form.FreeShip.Surface.NumberOfControlPoints-1 do if Form.FreeShip.Surface.ControlPoint[I].Name='UpstreamLoadUndo' then P:=Form.FreeShip.Surface.ControlPoint[I];
     if (P=nil) or (Abs(P.Coordinate.X-BeforeX)>0.001) or (Form.FreeShip.UndoCount<=Form.FreeShip.UndoPosition) then raise Exception.Create('Load rollback fixture could not establish redo history');
     BeforeCount:=Form.FreeShip.Surface.NumberOfControlPoints; Form.FreeShip.FileChanged:=True;
     BeforeName:=Form.FreeShip.Filename; OldFilenameSet:=Form.FreeShip.FilenameSet;
     OldUndoCount:=Form.FreeShip.UndoCount; OldUndoPos:=Form.FreeShip.UndoPosition;
     BeforeFile:=IncludeTrailingPathDelimiter(SysUtils.GetEnvironmentVariable('FS_UI_PROBE_OUTPUT'))+'before.bin';
     AfterFile:=IncludeTrailingPathDelimiter(SysUtils.GetEnvironmentVariable('FS_UI_PROBE_OUTPUT'))+'after.bin';
     Buffer:=TFreeFileBuffer.Create;
     try Form.FreeShip.SaveProject(Buffer); if not Buffer.SaveToFile(BeforeFile) then raise Exception.Create('Could not capture before model'); finally Buffer.Free end;
     Buffer:=TFreeFileBuffer.Create; Err:='';
     try Buffer.Add('INVALID'); try Form.FreeShip.LoadProject(Buffer) except on E:Exception do Err:=E.ClassName+': '+E.Message end finally Buffer.Free end;
     LoadErr:=Err; Saved:=False; Buffer:=TFreeFileBuffer.Create;
     try try Form.FreeShip.SaveProject(Buffer); if Buffer.SaveToFile(AfterFile) then Saved:=EqualFiles(BeforeFile,AfterFile) except on E:Exception do Err:=Err+'; snapshot: '+E.Message end finally Buffer.Free end;
     PreservedState:=Form.FreeShip.FileChanged and Form.FreeShip.ModelIsLoaded and (Form.FreeShip.Filename=BeforeName) and (Form.FreeShip.FilenameSet=OldFilenameSet) and (Form.FreeShip.UndoCount=OldUndoCount) and (Form.FreeShip.UndoPosition=OldUndoPos);
     RedoPreserved:=False;
     if Saved and Form.FreeShip.ModelIsLoaded and (Form.FreeShip.UndoCount=OldUndoCount) and (Form.FreeShip.UndoPosition=OldUndoPos) then begin
       Form.FreeShip.Edit.Redo; P:=nil;
       for I:=0 to Form.FreeShip.Surface.NumberOfControlPoints-1 do if Form.FreeShip.Surface.ControlPoint[I].Name='UpstreamLoadUndo' then P:=Form.FreeShip.Surface.ControlPoint[I];
       RedoPreserved:=Assigned(P) and (Abs(P.Coordinate.X-(BeforeX+1))<0.001);
       if RedoPreserved then Form.FreeShip.Edit.Undo;
     end;
     O:=TJSONObject.Create; O.Add('id','02.invalid-header-preserves-model'); O.Add('expected','invalid model reports failure and preserves serialized model, filename, modified state, load status and undo history');
     if (LoadErr<>'') and Saved and RedoPreserved and PreservedState then O.Add('status','pass') else O.Add('status','fail');
     O.Add('model_bytes_equal',Saved); O.Add('redo_behavior_preserved',RedoPreserved); O.Add('points_before',BeforeCount); O.Add('points_after',Form.FreeShip.Surface.NumberOfControlPoints); O.Add('modified',Form.FreeShip.FileChanged); O.Add('loaded',Form.FreeShip.ModelIsLoaded); O.Add('filename_preserved',Form.FreeShip.Filename=BeforeName); O.Add('undo_count_before',OldUndoCount); O.Add('undo_count_after',Form.FreeShip.UndoCount); O.Add('undo_position_before',OldUndoPos); O.Add('undo_position_after',Form.FreeShip.UndoPosition); O.Add('exception',Err); C.Add(O);
   end else if Sub='rebuild-fault' then begin
     Form.FreeShip.Surface.Built:=False; Windows.SetEnvironmentVariable('FS_UPSTREAM_REBUILD_FAULT','1'); Err:='';
     try Form.FreeShip.Surface.Rebuild except on E:Exception do Err:=E.ClassName+': '+E.Message end;
     Windows.SetEnvironmentVariable('FS_UPSTREAM_REBUILD_FAULT','');
     O:=TJSONObject.Create; O.Add('id','08.rebuild-recovers-after-exception'); O.Add('expected','injected exception clears building state and retry succeeds');
     Saved:=not Form.FreeShip.Surface.IsBuilding;
     try Form.FreeShip.Surface.Rebuild except on E:Exception do Err:=Err+'; retry: '+E.Message end;
     if Pos('injected rebuild failure',Err)=0 then O.Add('status','not_run') else if Saved and Form.FreeShip.Surface.Built then O.Add('status','pass') else O.Add('status','fail');
     O.Add('flag_cleared',Saved); O.Add('built_after_retry',Form.FreeShip.Surface.Built); O.Add('exception',Err); C.Add(O);
   end else if Sub='undo-cancel' then begin
     P:=Form.FreeShip.Surface.ControlPoint[0]; P.Name:='UpstreamUndoProbe'; Coord:=P.Coordinate; A:=Coord.X; Form.FreeShip.Edit.Undo_Clear;
     Form.FreeShip.Edit.CreateUndoObject('edit',True); Coord.X:=A+1; P.Coordinate:=Coord; Form.FreeShip.FileChanged:=True; Form.FreeShip.Edit.Undo;
     P:=nil; for I:=0 to Form.FreeShip.Surface.NumberOfControlPoints-1 do if Form.FreeShip.Surface.ControlPoint[I].Name='UpstreamUndoProbe' then P:=Form.FreeShip.Surface.ControlPoint[I];
     if (P=nil) or (Abs(P.Coordinate.X-A)>0.001) then begin
       O:=TJSONObject.Create; O.Add('id','10.cancel-preserves-redo'); O.Add('status','not_run'); O.Add('expected','undo succeeds before cancellation'); O.Add('actual','undo prerequisite or point identity unavailable'); C.Add(O);
     end else begin
       BeforeCount:=Form.FreeShip.UndoCount; UndoObj:=Form.FreeShip.Edit.CreateUndoObject('cancel',False); UndoObj.Delete;
       if Form.FreeShip.UndoCount>Form.FreeShip.UndoPosition then Form.FreeShip.Edit.Redo;
       P:=nil; for I:=0 to Form.FreeShip.Surface.NumberOfControlPoints-1 do if Form.FreeShip.Surface.ControlPoint[I].Name='UpstreamUndoProbe' then P:=Form.FreeShip.Surface.ControlPoint[I];
       if P=nil then begin O:=TJSONObject.Create; O.Add('id','10.cancel-preserves-redo'); O.Add('status','fail'); O.Add('expected','redo restores named point'); O.Add('actual','named point missing'); C.Add(O) end
       else Add(C,'10.cancel-preserves-redo',P.Coordinate.X,A+1,0.001);
     end;
   end else if Sub='load-notify' then begin
     Buffer:=TFreeFileBuffer.Create; Counter:=TCounter.Create;
     try
       Form.FreeShip.Surface.SaveBinary(Buffer); Buffer.Reset; Form.FreeShip.Surface.AddOnChangeItemListener(@Counter.Changed);
       Windows.SetEnvironmentVariable('FS_UPSTREAM_LOAD_EVENT','1');
       try Form.FreeShip.Surface.LoadBinary(Buffer) finally Windows.SetEnvironmentVariable('FS_UPSTREAM_LOAD_EVENT',''); Form.FreeShip.Surface.RemoveOnChangeItemListener(@Counter.Changed) end;
       Add(C,'09.no-intermediate-change-event',Counter.Count,0,0);
     finally Counter.Free; Buffer.Free end;
   end else if Sub='jpeg-bounds' then begin
     Buffer:=TFreeFileBuffer.Create; J:=TJPEGImage.Create; Err:='';
     try Buffer.Add(1); Buffer.Add(1); Buffer.Add(1024); Buffer.Reset;
       try Buffer.LoadTJPEGImage(J) except on E:Exception do Err:=E.ClassName+': '+E.Message end;
       O:=TJSONObject.Create; O.Add('id','04.jpeg-length-bounds'); O.Add('expected','reject embedded JPEG exceeding remaining bytes without advancing beyond the buffer');
       if (Err<>'') and (Buffer.Position<=Buffer.Count) then O.Add('status','review') else O.Add('status','fail');
       O.Add('position',Buffer.Position); O.Add('count',Buffer.Count); O.Add('exception',Err); C.Add(O);
     finally J.Free; Buffer.Free end;
   end else if (Sub='io-read') or (Sub='io-write') then begin
     IOPath:=IncludeTrailingPathDelimiter(SysUtils.GetEnvironmentVariable('FS_UI_PROBE_OUTPUT'))+'data.bin';
     Buffer:=TFreeFileBuffer.Create; Err:=''; Saved:=False;
     try
       Buffer.Add(12345);
       if Sub='io-read' then begin
         Buffer.SaveToFile(IOPath); Buffer.Clear; Windows.SetEnvironmentVariable('FS_UPSTREAM_IO_FAULT','io-read'); Mark('io-read-start');
         try Buffer.LoadFromFile(IOPath) except on E:Exception do Err:=E.ClassName+': '+E.Message end;
       end else begin
         Windows.SetEnvironmentVariable('FS_UPSTREAM_IO_FAULT','io-write'); Mark('io-write-start');
         try Saved:=Buffer.SaveToFile(IOPath) except on E:Exception do Err:=E.ClassName+': '+E.Message end;
       end;
       O:=TJSONObject.Create; O.Add('id','03.'+Sub+'-zero-progress'); O.Add('expected','zero progress terminates and signals an error');
       if (((Sub='io-read') and (Pos('EReadError:',Err)=1)) or
         ((Sub='io-write') and (Pos('EWriteError:',Err)=1) and (not Saved))) and
         CanReopenExclusive(IOPath) then O.Add('status','pass') else O.Add('status','fail');
       O.Add('actual',Err); O.Add('handle_closed',CanReopenExclusive(IOPath)); C.Add(O);
     finally Windows.SetEnvironmentVariable('FS_UPSTREAM_IO_FAULT',''); Buffer.Free end;
   end else if Sub='jpg' then begin
     Err:='';
     try Form.GetMDIChildren(0).Viewport.BackgroundImage.Open('') except on E:Exception do Err:=E.ClassName+': '+E.Message end;
     O:=TJSONObject.Create; O.Add('id','14.valid-jpg-open'); O.Add('expected','valid JPEG opens without exception and produces an image');
     if (Err='') and Assigned(Form.GetMDIChildren(0).Viewport.BackgroundImage.Bitmap) and (Form.GetMDIChildren(0).Viewport.BackgroundImage.Bitmap.Width=8) then O.Add('status','pass') else O.Add('status','fail');
     O.Add('actual',Err); C.Add(O);
   end else if Sub='process' then begin
     Code:=ExecuteFreePlugin(SysUtils.GetEnvironmentVariable('FS_UPSTREAM_HELPERS'),IncludeTrailingPathDelimiter(SysUtils.GetEnvironmentVariable('FS_UPSTREAM_HELPERS'))+'mock-ok.exe');
     Add(C,'11.successful-process',Code,0,0);
     Code:=0; Err:='';
     try ExecuteFreePlugin(SysUtils.GetEnvironmentVariable('FS_UPSTREAM_HELPERS'),IncludeTrailingPathDelimiter(SysUtils.GetEnvironmentVariable('FS_UPSTREAM_HELPERS'))+'mock-fail.exe');
     except on E:EFreePluginError do begin Code:=E.ExitCode; Err:=E.Message end end;
     O:=TJSONObject.Create; O.Add('id','11.nonzero-process'); O.Add('expected','external exit code 7 raises before result parsing'); O.Add('actual',Code); O.Add('error',Err);
     if (Code=7) and (Err<>'') then O.Add('status','pass') else O.Add('status','fail'); C.Add(O);
   end else begin
   if Form.FreeShip.ProjectSettings.ProjectUnits<>fuMetric then raise Exception.Create('Fixture is not metric');
   Form.FreeShip.ProjectSettings.ProjectWaterDensity:=1.025;
   Mark('intersections'); Form.FreeShip.Edit.Intersection_Add(fiStation,3);
   Form.FreeShip.Edit.Intersection_Add(fiWaterline,2);
   Form.FreeShip.Edit.Intersection_Add(fiButtock,1);
   N1:=Form.FreeShip.NumberOfStations-1; N2:=Form.FreeShip.NumberOfWaterlines-1; N3:=Form.FreeShip.NumberOfButtocks-1;
   if (N1<0) or (N2<0) or (N3<0) then raise Exception.Create('Intersection fixture unavailable');
   A:=Form.FreeShip.Station[N1].Distance; W:=Form.FreeShip.Waterline[N2].Distance; B:=Form.FreeShip.Buttock[N3].Distance;
   Mark('imperial'); Form.FreeShip.ProjectSettings.ProjectUnits:=fuImperial; Mark('imperial-done');
   D:=Form.FreeShip.ProjectSettings.ProjectWaterDensity;
   Add(C,'05.water-density-metric-to-imperial',D,1.025*1000*0.3048*0.3048*0.3048/0.45359237,0.001);
   Add(C,'06.station-metric-to-imperial',Form.FreeShip.Station[N1].Distance,A/0.3048,0.001);
   Add(C,'06.waterline-metric-to-imperial',Form.FreeShip.Waterline[N2].Distance,W/0.3048,0.001);
   Add(C,'06.buttock-metric-to-imperial',Form.FreeShip.Buttock[N3].Distance,B/0.3048,0.001);
   Mark('metric'); Form.FreeShip.ProjectSettings.ProjectUnits:=fuMetric; Mark('metric-done');
   Add(C,'05.water-density-round-trip',Form.FreeShip.ProjectSettings.ProjectWaterDensity,1.025,0.0001);
   Add(C,'06.station-round-trip',Form.FreeShip.Station[N1].Distance,A,0.001);
   Add(C,'06.waterline-round-trip',Form.FreeShip.Waterline[N2].Distance,W,0.001);
   Add(C,'06.buttock-round-trip',Form.FreeShip.Buttock[N3].Distance,B,0.001);
   end;
   end;
  except on E:Exception do R.Add('error',E.ClassName+': '+E.Message) end;
  S:=TStringList.Create; try S.Text:=R.FormatJSON; S.SaveToFile(IncludeTrailingPathDelimiter(SysUtils.GetEnvironmentVariable('FS_UI_PROBE_OUTPUT'))+'probe.json') finally S.Free end;
 finally R.Free end;
 ExitProcess(0); // Isolated process: skip dirty-model close prompts after flushing evidence.
end;
end.

