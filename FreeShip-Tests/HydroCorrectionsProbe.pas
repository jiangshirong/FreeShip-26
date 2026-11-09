unit HydroCorrectionsProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure RunHydroCorrectionsProbe(Form:TMainForm);
implementation
uses SysUtils, Classes, Math, fpjson, FreeShipUnit, FreeGeometry, FreeTypes,
  FreeHydrostaticsFrm, FreeHydrostaticsDlg, Translations, Windows;

procedure Check(C:TJSONArray; const ID,Expected:string; Passed:Boolean; const Actual:string='');
var O:TJSONObject;
begin
  O:=TJSONObject.Create; O.Add('id',ID); O.Add('expected',Expected); O.Add('actual',Actual);
  if Passed then O.Add('status','pass') else O.Add('status','fail'); C.Add(O);
end;

procedure CloseTo(C:TJSONArray; const ID:string; Actual,Expected,Tolerance:Double);
begin Check(C,ID,FloatToStr(Expected)+' +/- '+FloatToStr(Tolerance),
  Abs(Actual-Expected)<=Tolerance,FloatToStr(Actual)) end;

function BuildHull(Offset:Single; Narrow:Boolean=False):TFreeShip;
var XS,YS:array of Single; S:TFreeShip; I,N:Integer; P:array of array[0..3] of T3DCoordinate;
begin
  if Narrow then begin
    N:=5; SetLength(XS,N); SetLength(YS,N);
    XS[0]:=0; XS[1]:=6.999; XS[2]:=7; XS[3]:=7.001; XS[4]:=20;
    YS[0]:=1; YS[1]:=1; YS[2]:=4; YS[3]:=1; YS[4]:=1;
  end else begin
    N:=3; SetLength(XS,N); SetLength(YS,N);
    XS[0]:=0; XS[1]:=7; XS[2]:=20; YS[0]:=1; YS[1]:=4; YS[2]:=2;
  end;
  SetLength(P,N);
  S:=TFreeShip.Create(nil);
  try
    S.IsRebuildAsyncDisabled:=True;
    S.ProjectSettings.DisableModelCheck:=True;
    S.ProjectSettings.EnableModelAutoMove:=False;
    S.ProjectSettings.ProjectLength:=20; S.ProjectSettings.ProjectBeam:=8;
    S.ProjectSettings.ProjectDraft:=3; S.ProjectSettings.ProjectCoefficients:=fcActualData;
    for I:=0 to N-1 do begin
      P[I,0]:=SetPoint(Offset+XS[I],0,0); P[I,1]:=SetPoint(Offset+XS[I],YS[I],0);
      P[I,2]:=SetPoint(Offset+XS[I],YS[I],8); P[I,3]:=SetPoint(Offset+XS[I],0,8);
    end;
    for I:=0 to N-2 do begin
      S.Surface.AddControlFace([P[I,0],P[I,1],P[I+1,1],P[I+1,0]],4).Layer:=S.Layer[0];
      S.Surface.AddControlFace([P[I,1],P[I,2],P[I+1,2],P[I+1,1]],4).Layer:=S.Layer[0];
    end;
    S.Surface.AddControlFace([P[0,0],P[0,3],P[0,2],P[0,1]],4).Layer:=S.Layer[0];
    S.Surface.AddControlFace([P[N-1,0],P[N-1,1],P[N-1,2],P[N-1,3]],4).Layer:=S.Layer[0];
    for I:=0 to S.Surface.NumberOfControlEdges-1 do S.Surface.ControlEdge[I].Crease:=True;
    S.Layer[0].Symmetric:=True; S.Layer[0].UseInHydrostatics:=True;
    S.Precision:=fpLow; S.ModelIsLoaded:=True; S.Surface.Rebuild;
    if (S.Layer[0].Count<>2*N) or (S.Surface.ControlFace[0].ChildCount=0) then
      raise Exception.Create('analytic fixture has no rebuilt hydrostatic faces');
    Result:=S;
  except S.Free; raise end;
end;

procedure CheckMaximum(C:TJSONArray; S:TFreeShip; Offset:Single; const Name:string; ExpectedVolume:Double=339);
var H:TFreeHydrostaticCalc;
begin
  H:=TFreeHydrostaticCalc.Create(S);
  try
    H.Draft:=3; H.Calculate;
    Check(C,'07.'+Name+'.calculated','the analytic watertight hull calculates without errors',H.Calculated and (H.Errors=[]),H.ErrorString);
    CloseTo(C,'07.'+Name+'.location',H.MidshipLocation,Offset+7,0.001);
    CloseTo(C,'07.'+Name+'.area',H.Data.MidshipArea,24,0.003);
    CloseTo(C,'07.'+Name+'.volume',H.Data.Volume,ExpectedVolume,0.03);
    CloseTo(C,'07.'+Name+'.cm',H.Data.MidshipCoeff,1,0.0002);
    CloseTo(C,'07.'+Name+'.cp',H.Data.PrismCoefficient,ExpectedVolume/(24*20),0.0002);
    H.CalculateVolume(H.WaterlinePlane);
    CloseTo(C,'07.'+Name+'.volume-only-location',H.MidshipLocation,Offset+7,0.001);
    CloseTo(C,'07.'+Name+'.volume-only-volume',H.Data.Volume,ExpectedVolume,0.03);
  finally H.Free end;
end;

procedure MidshipCases(C:TJSONArray);
const Offsets:array[0..2] of Single=(0,100,-50);
var S:TFreeShip; I:Integer;
begin
  for I:=0 to High(Offsets) do begin
    S:=BuildHull(Offsets[I]);
    try
      CheckMaximum(C,S,Offsets[I],IntToStr(I)+'.no-stations');
      S.Edit.Intersection_Add(fiStation,Offsets[I]+1);
      S.Edit.Intersection_Add(fiStation,Offsets[I]+4);
      S.Edit.Intersection_Add(fiStation,Offsets[I]+19);
      Check(C,'07.'+IntToStr(I)+'.station-fixture','three off-maximum display stations were added',S.NumberOfStations=3);
      CheckMaximum(C,S,Offsets[I],IntToStr(I)+'.with-stations');
      while S.NumberOfStations>0 do S.Station[0].Delete(False);
      CheckMaximum(C,S,Offsets[I],IntToStr(I)+'.removed-stations');
    finally S.Free end;
  end;
  S:=BuildHull(0,True);
  try CheckMaximum(C,S,0,'narrow-peak',120.018) finally S.Free end;
end;

procedure DraftCase(C:TJSONArray; const ID:string; A,B,Step:Single; const Expected:array of Single);
var Values:TFreeDraftSequence; I:Integer; Equal,Increasing:Boolean; Actual:string;
begin
  Values:=BuildHydrostaticDraftSequence(A,B,Step);
  Equal:=Length(Values)=Length(Expected); Increasing:=True; Actual:='';
  for I:=0 to High(Values) do begin
    if I<=High(Expected) then Equal:=Equal and (Abs(Values[I]-Expected[I])<1e-6);
    if I>0 then Increasing:=Increasing and (Values[I]>Values[I-1]);
    Actual:=Actual+' '+FloatToStr(Values[I]);
  end;
  Check(C,'12.'+ID,'expected endpoint-inclusive values occur once, in strictly increasing order',Equal and Increasing,Actual);
end;

procedure InvalidDraft(C:TJSONArray; const ID:string; A,B,Step:Single);
var Values:TFreeDraftSequence; ErrorText:string;
begin
  ErrorText:='';
  try Values:=BuildHydrostaticDraftSequence(A,B,Step) except on E:EArgumentException do ErrorText:=E.Message end;
  Check(C,'12.'+ID,'invalid or unrepresentable inputs raise a controlled argument error',ErrorText<>'',ErrorText);
end;

procedure DraftCases(C:TJSONArray);
var V:TFreeDraftSequence; I:Integer; Increasing:Boolean; S:TFreeShip; F:TFreeHydrostaticsForm;
  OldCalcCount:Integer;
begin
  DraftCase(C,'aligned',1,2,0.5,[1,1.5,2]);
  DraftCase(C,'unaligned',1,2,0.4,[1,1.4,1.8,2]);
  DraftCase(C,'larger-step',1,2,3,[1,2]);
  DraftCase(C,'single-value',2,2,0.5,[2]);
  DraftCase(C,'decimal-rounded-end',0.1,0.3,0.1,[0.1,0.2,0.3]);
  DraftCase(C,'decimal-whole-range',0,1,0.1,[0,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1]);
  DraftCase(C,'negative-start',-1,1,0.5,[-1,-0.5,0,0.5,1]);
  DraftCase(C,'small-step',0,0.00003,0.00001,[0,0.00001,0.00002,0.00003]);
  InvalidDraft(C,'zero-step',1,2,0);
  InvalidDraft(C,'negative-step',1,2,-1);
  InvalidDraft(C,'reversed-range',2,1,0.5);
  InvalidDraft(C,'nan-start',NaN,1,0.5);
  InvalidDraft(C,'infinite-end',0,Infinity,0.5);
  InvalidDraft(C,'nan-step',0,1,NaN);
  InvalidDraft(C,'too-many-rows',0,10000,1);
  InvalidDraft(C,'partial-row-over-limit',0,9999.5,1);
  InvalidDraft(C,'tiny-step-over-limit',0,1,1e-30);
  InvalidDraft(C,'underresolved-step',16777216,16777220,1);
  V:=BuildHydrostaticDraftSequence(0,9999,1); Increasing:=True;
  for I:=1 to High(V) do Increasing:=Increasing and (V[I]>V[I-1]);
  Check(C,'12.maximum-row-boundary','exactly 10000 distinct rows are accepted',
    (Length(V)=MaxHydrostaticDraftRows) and (V[0]=0) and (V[High(V)]=9999) and Increasing,IntToStr(Length(V)));
  V:=BuildHydrostaticDraftSequence(0,999.9,0.1); Increasing:=True;
  for I:=1 to High(V) do Increasing:=Increasing and (V[I]>V[I-1]);
  Check(C,'12.rounded-maximum-row-boundary','Single endpoint rounding still allows exactly 10000 distinct decimal-step rows',
    (Length(V)=MaxHydrostaticDraftRows) and Increasing,IntToStr(Length(V)));
  S:=BuildHull(0); F:=TFreeHydrostaticsForm.Create(nil);
  HydroProbeDrafts:=TStringList.Create; HydroProbeRows:=TStringList.Create;
  try
    F.ProbeAttachShip(S); F.StartDraft:=1; F.EndDraft:=2; F.DraftStep:=0.5; F.Trim:=0;
    OldCalcCount:=S.NumberOfHydrostaticCalculations;
    F.Button2Click(nil);
    Check(C,'12.actual-button-calculations','real Button2Click calculates 1, 1.5, 2 once each',
      (HydroProbeDrafts.Count=3) and (HydroProbeDrafts[0]='1') and (HydroProbeDrafts[1]='1.5') and (HydroProbeDrafts[2]='2'),HydroProbeDrafts.CommaText);
    Check(C,'12.actual-result-rows','real result grid has exactly three data rows',
      (HydroProbeRows.Count=3) and (HydroProbeRows[0]='1.000') and (HydroProbeRows[1]='1.500') and (HydroProbeRows[2]='2.000'),HydroProbeRows.CommaText);
    Check(C,'12.calculation-lifetime','the temporary calculation object is released',S.NumberOfHydrostaticCalculations=OldCalcCount);
  finally
    FreeAndNil(HydroProbeDrafts); FreeAndNil(HydroProbeRows); F.Free; S.Free;
  end;
end;

procedure FileCases(C:TJSONArray);
const Names:array[0..3] of string=('SACs.txt','Bonjean.txt','Weights.txt','Sterns.txt');
var S:TFreeShip; H:TFreeHydrostaticCalc; Texts:TStringList; I:Integer; Mode,Contents:string;
  Props:TLayerProperties;
begin
  Mode:=SysUtils.GetEnvironmentVariable('FS_HYDRO_FILE_MODE');
  Check(C,'15.'+Mode+'.working-directory','the process uses the requested isolated working directory',
    SameFileName(GetCurrentDir,SysUtils.GetEnvironmentVariable('FS_HYDRO_CWD')),GetCurrentDir);
  S:=BuildHull(0); H:=TFreeHydrostaticCalc.Create(S); Texts:=TStringList.Create;
  try
    S.Edit.Intersection_Add(fiStation,1); S.Edit.Intersection_Add(fiStation,7); S.Edit.Intersection_Add(fiStation,19);
    S.Edit.Intersection_Add(fiWaterline,2); S.Edit.Intersection_Add(fiWaterline,3);
    S.ProjectSettings.EnableBonjeanSAC:=True; S.Layer[0].Thickness:=0.01; S.Layer[0].MaterialDensity:=7850;
    H.Draft:=3; H.Calculate; H.CalculateGravity; Props:=S.Layer[0].SurfaceProperties;
    H.AddData(Texts,fhSingleCalculation,';');
    Check(C,'15.'+Mode+'.calculation','ordinary calculations and report formatting complete with valid data',
      H.Calculated and (H.Errors=[]) and (Length(H.Data.SAC)=3) and (Props.Weight>0) and (Texts.Count>0),H.ErrorString);
    for I:=0 to High(Names) do begin
      if Mode='empty' then Check(C,'15.empty.'+Names[I],'normal calculation does not create an implicit output file',not FileExists(Names[I]))
      else begin
        Texts.Clear; Texts.LoadFromFile(Names[I]); Contents:=Trim(Texts.Text);
        Check(C,'15.'+Mode+'.'+Names[I],'an existing same-name user file remains intact',Contents='user-owned '+Names[I],Contents);
      end;
    end;
  finally Texts.Free; H.Free; S.Free end;
end;

procedure ExportCases(C:TJSONArray);
const Names:array[0..3] of string=('SACs.txt','Bonjean.txt','Weights.txt','Sterns.txt');
var S:TFreeShip; H,Legacy:TFreeHydrostaticCalc; D:TFreeHydrostaticsDialog;
  Texts,Before:TStringList; I,CountBefore:Integer; Notice,DirectoryName,ErrorText:string;
  DataBefore:TFreeHydrostaticsData;
  Drafts:TFreeDraftSequence;
begin
  S:=BuildHull(0); H:=TFreeHydrostaticCalc.Create(S); Legacy:=TFreeHydrostaticCalc.Create(S);
  D:=TFreeHydrostaticsDialog.Create(nil); Texts:=TStringList.Create; Before:=TStringList.Create;
  HydroProbeMessages:=TStringList.Create;
  try
    S.Edit.Intersection_Add(fiStation,1); S.Edit.Intersection_Add(fiStation,7); S.Edit.Intersection_Add(fiStation,19);
    S.Edit.Intersection_Add(fiWaterline,2); S.Edit.Intersection_Add(fiWaterline,3);
    S.Edit.Intersection_Add(fiWaterline,0);
    S.ProjectSettings.EnableBonjeanSAC:=True; S.Layer[0].Thickness:=0.01; S.Layer[0].MaterialDensity:=7850;
    H.Draft:=3; H.Calculate; H.AddData(Texts,fhSingleCalculation,';'); D.Edit.Lines.Assign(Texts);
    DataBefore:=H.Data; CountBefore:=S.NumberOfHydrostaticCalculations;
    Notice:=H.BuildAuxiliaryReports(D.AdditionalReports);
    Check(C,'15.export.memory-tables','checkbox generates four nonempty tables in memory without writing files',
      (Notice='') and (D.AdditionalReports.Count=4) and not FileExists('SACs.txt') and not FileExists('Weights.txt'),Notice);
    Check(C,'15.export.calculation-preserved','building auxiliary tables preserves current waterplane, SAC and temporary-object count',
      (H.Data.WaterlinePlane.d=DataBefore.WaterlinePlane.d) and (H.Data.Volume=DataBefore.Volume) and
      (H.Data.SAC[1].Y=DataBefore.SAC[1].Y) and (S.NumberOfHydrostaticCalculations=CountBefore));
    Legacy.Draft:=3; Legacy.LegacyCalculate; Legacy.LegacyAddData(Texts,fhSingleCalculation,';');
    for I:=0 to High(Names) do begin
      Texts.LoadFromFile(Names[I]);
      Texts.SaveToFile(ExtractFilePath(SysUtils.GetEnvironmentVariable('FS_HYDRO_REPORT'))+'legacy-'+Names[I]);
      TStrings(D.AdditionalReports.Objects[I]).SaveToFile(ExtractFilePath(SysUtils.GetEnvironmentVariable('FS_HYDRO_REPORT'))+'current-'+Names[I]);
      Check(C,'15.export.legacy.'+Names[I],'export table exactly matches the checked-in legacy writer on the same hull',
        Texts.Text=TStrings(D.AdditionalReports.Objects[I]).Text,
        'legacy='+IntToStr(Length(Texts.Text))+' new='+IntToStr(Length(TStrings(D.AdditionalReports.Objects[I]).Text)));
    end;
    D.Edit.Lines.Text:='report-body';
    D.SaveDialog.FileName:=IncludeTrailingPathDelimiter(GetCurrentDir)+'chosen-report.txt';
    HydroProbeSaveAccepted:=False; D.ButtonSaveClick(nil);
    Check(C,'15.export.cancel','cancelled Save leaves report and data directory absent',
      not FileExists(D.SaveDialog.FileName) and not DirectoryExists('chosen-report-data'));
    HydroProbeSaveAccepted:=True; D.ButtonSaveClick(nil);
    Texts.LoadFromFile(D.SaveDialog.FileName);
    Check(C,'15.export.actual-save-button','actual Save button writes selected report and reports the data directory',
      (Trim(Texts.Text)='report-body') and (HydroProbeMessages.Count=1) and
      (Pos('chosen-report-data',HydroProbeMessages[0])>0),HydroProbeMessages.Text);
    for I:=0 to High(Names) do begin
      Texts.LoadFromFile('chosen-report-data'+DirectorySeparator+Names[I]);
      Check(C,'15.export.saved.'+Names[I],'saved file contains the full in-memory legacy-compatible table',
        Texts.Text=TStrings(D.AdditionalReports.Objects[I]).Text);
    end;
    Before.LoadFromFile('chosen-report-data'+DirectorySeparator+'SACs.txt');
    D.ButtonSaveClick(nil);
    Texts.LoadFromFile('chosen-report-data'+DirectorySeparator+'SACs.txt');
    Check(C,'15.export.repeat','repeated report saves use a new directory and preserve the first export',
      DirectoryExists('chosen-report-data-2') and (Texts.Text=Before.Text));
    Texts.Text:='user-file'; Texts.SaveToFile('collision-data');
    DirectoryName:=D.SaveReport('collision.txt'); Texts.LoadFromFile('collision-data');
    Check(C,'15.export.file-collision','a file occupying the proposed directory name remains intact',
      (ExtractFileName(DirectoryName)='collision-data-2') and (Trim(Texts.Text)='user-file'));
    ErrorText:='';
    try D.SaveReport('missing-parent'+DirectorySeparator+'report.txt') except on E:Exception do ErrorText:=E.Message end;
    Check(C,'15.export.invalid-path','an inaccessible target fails with an explicit error without creating a folder',
      (ErrorText<>'') and not DirectoryExists('missing-parent'),ErrorText);
    D.SaveDialog.FileName:='missing-parent'+DirectorySeparator+'report.txt'; HydroProbeMessages.Clear;
    D.ButtonSaveClick(nil);
    Check(C,'15.export.actual-error-message','actual Save button handles the failed target with one error message',
      (HydroProbeMessages.Count=1) and (Pos('1:',HydroProbeMessages[0])=1),HydroProbeMessages.Text);
    HydroProbeBlockFile:=True; ErrorText:='';
    try D.SaveReport('partial-output.txt') except on E:Exception do ErrorText:=E.Message end;
    HydroProbeBlockFile:=False;
    Check(C,'15.export.partial-write','failure on second auxiliary file keeps the main report and first file, with the partial directory identified',
      FileExists('partial-output.txt') and FileExists('partial-output-data'+DirectorySeparator+'SACs.txt') and
      DirectoryExists('partial-output-data'+DirectorySeparator+'Bonjean.txt') and
      (Pos('partial-output-data',ErrorText)>0) and
      ((Pos('incomplete',ErrorText)>0) or (Pos('未写入完整',ErrorText)>0)),ErrorText);
    for I:=0 to D.AdditionalReports.Count-1 do D.AdditionalReports.Objects[I].Free;
    D.AdditionalReports.Clear;
    S.ProjectSettings.EnableBonjeanSAC:=False; Notice:=H.BuildAuxiliaryReports(D.AdditionalReports);
    Check(C,'15.export.option-disabled','unchecked setting produces no auxiliary exports',(Notice='') and (D.AdditionalReports.Count=0));
    S.ProjectSettings.EnableBonjeanSAC:=True;
    while S.NumberOfStations>0 do S.Station[0].Delete(False);
    Notice:=H.BuildAuxiliaryReports(D.AdditionalReports);
    Check(C,'15.export.no-stations','without stations only Weights and Sterns are generated, with a clear reason for omitted SAC/Bonjean',
      (Notice<>'') and (D.AdditionalReports.Count=2) and (D.AdditionalReports[0]='Weights.txt') and (D.AdditionalReports[1]='Sterns.txt'),Notice);
    Legacy.LegacyAddData(Texts,fhSingleCalculation,';');
    for I:=0 to 1 do begin
      Texts.LoadFromFile(D.AdditionalReports[I]);
      Check(C,'15.export.no-stations-legacy.'+D.AdditionalReports[I],
        'available zero-station output matches the historical writer',Texts.Text=TStrings(D.AdditionalReports.Objects[I]).Text);
    end;
    Check(C,'15.export.no-leak','all temporary hydrostatic objects are released',S.NumberOfHydrostaticCalculations=CountBefore);
    TranslateUnitResourceStrings('FreeHydrostaticsDlg',SysUtils.GetEnvironmentVariable('FS_HYDRO_PO'));
    TranslateUnitResourceStrings('FreeHydrostaticsFrm',SysUtils.GetEnvironmentVariable('FS_HYDRO_PO'));
    TranslateUnitResourceStrings('FreeShipUnit',SysUtils.GetEnvironmentVariable('FS_HYDRO_PO'));
    ErrorText:='';
    try Drafts:=BuildHydrostaticDraftSequence(0,1,0) except on E:Exception do ErrorText:=E.Message end;
    Check(C,'12.chinese-error','actual invalid draft error is translated into Chinese',Pos('步长',ErrorText)>0,ErrorText);
    ErrorText:='';
    try D.SaveReport('missing-parent'+DirectorySeparator+'report.txt') except on E:Exception do ErrorText:=E.Message end;
    Check(C,'15.export.chinese-error','actual save failure includes the Chinese explanation and chosen path',
      (Pos('无法将报告保存到',ErrorText)>0) and (Pos('missing-parent',ErrorText)>0),ErrorText);
    for I:=0 to D.AdditionalReports.Count-1 do D.AdditionalReports.Objects[I].Free;
    D.AdditionalReports.Clear;
    while S.NumberOfWaterlines>0 do S.Waterline[0].Delete(False);
    Notice:=H.BuildAuxiliaryReports(D.AdditionalReports);
    Check(C,'15.export.chinese-unavailable','missing waterlines give a translated explanation and no empty exports',
      (Pos('未生成附加数据表',Notice)>0) and (D.AdditionalReports.Count=0),Notice);
  finally FreeAndNil(HydroProbeMessages); Before.Free; Texts.Free; D.Free; Legacy.Free; H.Free; S.Free end;
end;

procedure RunHydroCorrectionsProbe(Form:TMainForm);
var R:TJSONObject; C:TJSONArray; S:TStringList; Mode:string;
begin
  R:=TJSONObject.Create; C:=TJSONArray.Create; R.Add('cases',C);
  try
    try
      Mode:=SysUtils.GetEnvironmentVariable('FS_HYDRO_MODE');
      if Mode='files' then FileCases(C)
      else if Mode='export' then ExportCases(C)
      else begin MidshipCases(C); DraftCases(C) end;
    except on E:Exception do R.Add('error',E.ClassName+': '+E.Message) end;
    S:=TStringList.Create;
    try S.Text:=R.FormatJSON; S.SaveToFile(SysUtils.GetEnvironmentVariable('FS_HYDRO_REPORT')) finally S.Free end;
  finally R.Free end;
end;
end.
