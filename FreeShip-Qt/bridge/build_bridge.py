"""Build the version-checked adapter in an isolated copy; product sources stay intact."""
import argparse, json, shutil, sys, zipfile, re
from pathlib import Path
PILOT=Path(__file__).resolve().parents[1]
TESTS=PILOT.parent/'FreeShip-Tests'
sys.path.insert(0,str(TESTS))
import freeship_ui_test as common
import native_flicker_test as native
sys.path.insert(0,str(PILOT/'bridge'))
import performance_adapter
import plate_adapter
import cad_adapter

def replace_once(s,a,b):
    if s.count(a)!=1:raise RuntimeError('Bridge adapter no longer matches: '+a)
    return s.replace(a,b,1)

def adapt(work):
    shutil.copy2(PILOT/'bridge/PilotCapture.pas',work/'Units/PilotCapture.pas')
    path=work/'Forms/FreeLinesplanFrme.pas';s=path.read_text('utf-8-sig')
    s=replace_once(s,'uses FreeStringsUnit;','uses FreeStringsUnit, PilotCapture;')
    s=replace_once(s,'  UsingInteractionPreview := DrawInteractionPreview;',
        '  if PilotActive then UsingInteractionPreview:=False else UsingInteractionPreview := DrawInteractionPreview;')
    # Observe coordinates before any screen or atlas integer rounding.
    a=s.index('  procedure DrawLineAtt(');b=s.index('  procedure DrawDiagonalLine(',a)
    part=s[a:b];part=replace_once(part,'    Viewport.Polyline(Pts);',
        '    PilotLine(Proj1.X,Proj1.Y,Proj2.X,Proj2.Y,Viewport.PenColor);\n    Viewport.Polyline(Pts);')
    s=s[:a]+part+s[b:]
    a=s.index('  procedure DrawDiagonalLine(');b=s.index('  procedure QueueKnuckle(',a)
    part=s[a:b].replace('    Pt := Viewport.Project(P);','    PilotPoint(P.X,P.Y);\n    Pt := Viewport.Project(P);')
    part=part.replace('    Viewport.LineTo(Pt.X, Pt.Y);','    PilotFlush(2,Viewport.PenColor);\n    Viewport.LineTo(Pt.X, Pt.Y);')
    s=s[:a]+part+s[b:]
    a=s.index('  procedure DrawLine(P1');b=s.index('  {$ifdef NOUSE}',a)
    part=s[a:b].replace('QueueKnuckle(Pts[0],Pts[1]);',
        'PilotLine(Proj1.X,Proj1.Y,Proj2.X,Proj2.Y,Viewport.PenColor);\n      QueueKnuckle(Pts[0],Pts[1]);')
    s=s[:a]+part+s[b:]
    a=s.index('  procedure DrawSpline(',s.index('{$endif NOUSE}'));b=s.index('  procedure DrawIntersection(',a)
    part=s[a:b].replace('Pts[i] := Viewport.Project(Pr);','PilotPoint(Pr.X,Pr.Y);\n              Pts[i] := Viewport.Project(Pr);')
    part=part.replace('Viewport.Polyline(Pts);','PilotFlush(2,Viewport.PenColor);\n      Viewport.Polyline(Pts);')
    s=s[:a]+part+s[b:]
    a=s.index('  procedure DrawTriangles(');b=s.index('  procedure PrepareFillAtlas;',a)
    part=s[a:b];part=replace_once(part,'      if VectorRecording then Result:=',
        '      PilotPoint(P.X,P.Y);\n      if VectorRecording then Result:=')
    part=part.replace('if VectorRecording then FFillVector.Polygon(Pts,Col)',
        'PilotFlush(1,Col);\n        if VectorRecording then FFillVector.Polygon(Pts,Col)')
    s=s[:a]+part+s[b:]
    # Diagonal curves use a separate path outside DrawSpline.
    a=s.index('    // Draw diagonals',s.index('  PaintStarted :='));b=s.index('    DrawPendingLabels;',a)
    part=s[a:b].replace('Pts[K] := Viewport.Project(P);','PilotPoint(P.X,P.Y);\n          Pts[K] := Viewport.Project(P);')
    part=replace_once(part,'        Viewport.Polyline(Pts);','        PilotFlush(2,Viewport.PenColor);\n        Viewport.Polyline(Pts);')
    s=s[:a]+part+s[b:]
    s=replace_once(s,'  var Placement: TLabelPlacement; Kind: TLabelKind;',
        '  var Placement: TLabelPlacement; Kind: TLabelKind; Origin:TPoint;')
    s=replace_once(s,'    Inc(LabelIndex);',
        "    Origin:=Viewport.Project(ZERO);\n    if PilotActive then PilotLabel((X-Origin.X)/(Viewport.Scale*Viewport.Zoom),\n      (Origin.Y-Y)/(Viewport.Scale*Viewport.Zoom),Viewport.FontColor,Viewport.TextHeight('Mg'),Text);\n    Inc(LabelIndex);")
    a=s.index('procedure TFreeLinesplanFrame.ExportDXFExecute');part=s[a:]
    part=replace_once(part,'  if SaveDialog.Execute then',
        "  SaveDialog.FileName:=ParamStr(2)+'/reference.dxf';\n  if True then")
    s=s[:a]+part;path.write_text(s,encoding='utf8')

def adapt_scene(work):
    shutil.copy2(PILOT/'bridge/ExchangeImport.inc',work/'Units/ExchangeImport.inc')
    origin=json.loads((PILOT/'bridge/scene-origin.json').read_text('utf8'))
    for name,sha in origin.items():
        if common.digest_file(work/name)!=sha:raise RuntimeError('Scene extraction is stale: '+name)
    for name in ('FreeLinesplanScene.pas','PilotCapture.pas','ModelService.pas','DocumentAdapter.inc','ModelEdits.pas','StableEdgeLoops.pas','NativeLinesplanInput.pas','TopologyEdits.inc','SectionEdits.inc','OverlaySnapshot.inc','BackgroundEdits.inc','DisplayEdits.inc','AuxiliaryCurves.inc','AdvancedEdits.inc','ModelingEdits.inc','ModelTools.inc','NewHull.inc','LackenbyEngine.pas','HydrostaticsEngine.pas','HydrostaticsEdits.inc'):
        shutil.copy2(PILOT/'bridge'/name,work/'Units'/name)
    # This process has no editing UI. Avoid constructing the two eager dialogs
    # in the legacy model constructor (including LoadProject's candidate model).
    # All numerical initialization and the transactional loader stay intact.
    path=work/'Units/FreeShip.inc';s=path.read_text('utf-8-sig')
    s=replace_once(s,'constructor TFreeShip.Create(AOwner: TComponent);', '''function TFreeShip.ServiceSectionList(Kind: Integer): TFasterListTFreeIntersection;
begin
  case Kind of
    1:Result:=FStations;2:Result:=FButtocks;3:Result:=FWaterlines;4:Result:=FDiagonals;
    else raise Exception.Create('Invalid section kind');
  end;
end;

constructor TFreeShip.Create(AOwner: TComponent);''')
    s=replace_once(s,'  if not (csDesigning in ComponentState) then\n    FIntersectionDialog := TFreeIntersectionDialog.Create(self);',
        '  FIntersectionDialog := nil; // scene-only bridge')
    s=replace_once(s,'  if not (csDesigning in ComponentState) then\n  begin\n    FControlpointForm := TFreeControlPointForm.Create(Self);\n    FControlpointForm.FreeShip := self;\n  end;',
        '  FControlpointForm := nil; // scene-only bridge')
    path.write_text(s,encoding='utf8')

    # ProjectSettings previously discarded the loaded JPEG and SaveBinary
    # regenerated it by dereferencing MainForm. The window-free backend retains
    # that embedded JPEG (and the user's SavePreview flag) through save/undo.
    path=work/'Units/FreeShipUnit.pas';s=path.read_text('utf-8-sig')
    s=replace_once(s,'    property MainparticularsHasBeenset: boolean read FMainparticularsHasBeenset;',
        '    property ServiceParticularsSet: boolean read FMainparticularsHasBeenset write FMainparticularsHasBeenset;\n    property MainparticularsHasBeenset: boolean read FMainparticularsHasBeenset;')
    s=replace_once(s,'    property AssignedView: TFreeViewType read FAssignedView;',
        '''    property ServiceView: TFreeViewType read FAssignedView write FAssignedView;
    property ServiceOrigin: TPoint read FOrigin write FOrigin;
    property ServiceScale: TFloatType read FScale write FScale;
    property ServiceVisible: Boolean read FVisible write FVisible;
    property ServiceAlpha: Integer read FBlendingValue write FBlendingValue;
    property ServiceTransparent: Boolean read FTransparent write FTransparent;
    property ServiceColor: TColor read FTransparentColor write FTransparentColor;
    property ServiceTolerance: Integer read FTolerance write FTolerance;
    property ServiceQuality: Integer read FQuality write FQuality;
    property AssignedView: TFreeViewType read FAssignedView;''')
    s=replace_once(s,'    property NumberOfFlowLines: integer read FGetNumberOfFlowLines;',
        '    property ServiceFlowlines: TFasterListTFreeFlowline read FFlowLines;\n    property NumberOfFlowLines: integer read FGetNumberOfFlowLines;')
    s=replace_once(s,'    property Build: boolean read FBuild write FSetBuild;',
        '    property ServiceSpline: TFreeSpline read FFlowLine;\n    property ServiceSource: T2DCoordinate read FProjectionPoint write FProjectionPoint;\n    property ServiceView: TFreeViewType read FProjectionView write FProjectionView;\n    property Build: boolean read FBuild write FSetBuild;')
    s=replace_once(s,'    property NumberofBackgroundImages: integer read FGetNumberofBackgroundImages;',
        '    property ServiceBackgrounds: TFasterListTFreebackgroundImagedata read FBackgroundImages;\n    property NumberofBackgroundImages: integer read FGetNumberofBackgroundImages;')
    s=replace_once(s,'    property ShowHydrostaticData: boolean read FShowHydrostaticData',
        ''.join(f'    property Service{name}: boolean read FShowHydrost{name};\n' for name in
            ('Displacement','LateralArea','SectionalAreas','MetacentricHeight','LCF'))+
        '    property ShowHydrostaticData: boolean read FShowHydrostaticData')
    s=replace_once(s,'    constructor Create(AOwner: TComponent); override;',
        '    function ServiceSectionList(Kind: Integer): TFasterListTFreeIntersection;\n    constructor Create(AOwner: TComponent); override;')
    s=replace_once(s,'    FSavePreview: boolean;',
        '    FSavePreview: boolean;\n    FServicePreview: TJPEGImage; // retained source thumbnail')
    s=replace_once(s,'    property ProjectSplitSectionLocation: TFloatType',
        '    property ServiceStoredSplitSectionLocation: TFloatType read FProjectSplitSectionLocation;\n    property ProjectSplitSectionLocation: TFloatType')
    s=replace_once(s,'    property Hydrostatics_Startdraft: TFloatType',
        ''.join(f'    property Service{name}: {kind} read {field} write {field};\n' for name,kind,field in (
            ('Angles','TFloatArray','FAngles'),('Displacements','TFloatArray','FDisplacements'),
            ('NoAngles','Integer','FNoAngles'),('NoDisplacements','Integer','FNoDisplacements'),
            ('UseIncrements','Boolean','FUseDisplIncrements'),('MinDispl','TFloatType','FMinimumDisplacement'),
            ('MaxDispl','TFloatType','FMaximumDisplacement'),('DisplIncrement','TFloatType','FDisplIncrement'),
            ('VCG','TFloatType','FVCG'),('FreeTrim','Boolean','FFreeTrim'),
            ('StabTrims','TFloatArray','FStabTrims'),('NoStabTrims','Integer','FNoStabTrims')))+
        '    property Hydrostatics_Startdraft: TFloatType')
    s=replace_once(s,'    constructor Create(Owner: TFreeShip);\n    procedure Edit;\n    // User input of mainparticulars',
        '    constructor Create(Owner: TFreeShip);\n    destructor Destroy; override;\n    procedure Edit;\n    // User input of mainparticulars')
    path.write_text(s,encoding='utf8')
    # Clear's FillChar skipped the managed SAC array; repeated report rows also
    # leaked the separately allocated bulb section. These changes release only
    # scratch storage and leave all integration operations in their original order.
    hydro=work/'Units/FreeHydrostaticCalc.inc';text=hydro.read_text('utf-8-sig')
    text,n=re.subn(r'(?im)^(\s*)Fillchar\(FData, SizeOf\(FData\), 0\);',r'\1SetLength(FData.SAC,0);\n\1Fillchar(FData, SizeOf(FData), 0);',text)
    if n!=3:raise RuntimeError('Hydrostatic managed-data clear sites changed: '+str(n))
    text=replace_once(text,'    FBulbSection := TFreeIntersection.Create(Owner);','    FreeAndNil(FBulbSection);\n    FBulbSection := TFreeIntersection.Create(Owner);')
    text=replace_once(text,'  FreeAndNil(FMidshipSection);','  FreeAndNil(FMidshipSection);\n  FreeAndNil(FBulbSection);')
    hydro.write_text(text,encoding='utf8')
    path=work/'Units/FreeProjectSettings.inc';s=path.read_text('utf-8-sig')
    s=replace_once(s,'procedure TFreeProjectSettings.Clear;\nbegin',
        'procedure TFreeProjectSettings.Clear;\nbegin\n  FreeAndNil(FServicePreview);')
    s=replace_once(s,'end;{TFreeProjectSettings.Create}',
        'end;{TFreeProjectSettings.Create}\n\ndestructor TFreeProjectSettings.Destroy;\nbegin\n  FServicePreview.Free;\n  inherited Destroy;\nend;')
    s=replace_once(s,'      if Image <> nil then\n        Image.Assign(JPG);\n      FreeAndNil(Jpg);',
        '      FreeAndNil(FServicePreview);\n      FServicePreview := Jpg;\n      if Image <> nil then\n        Image.Assign(JPG);')
    s=replace_once(s,'        Jpg := FreeShip.FGetPreview;\n        Destination.Add(Jpg);\n        FreeAndNil(Jpg);',
        "        if FServicePreview=nil then raise EWriteError.Create('No retained source preview');\n        Destination.Add(FServicePreview);")
    s=replace_once(s,'    FStartDraft := FStartDraft * UnitConversionFactor;',
        '''    // Model_Scale converts the active manual split only. Preserve the
    // physical backup value too while the effective split uses length / 2.
    if FUseDefaultSplitSectionLocation then
      FProjectSplitSectionLocation := FProjectSplitSectionLocation * UnitConversionFactor;
    // Image origins are pixels; scale and flowline seeds are model lengths.
    for I := 0 to FreeShip.NumberOfBackgroundImages - 1 do
      FreeShip.BackgroundImage[I].FScale := FreeShip.BackgroundImage[I].FScale * UnitConversionFactor;
    for I := 0 to FreeShip.NumberOfFlowlines - 1 do
    begin
      FreeShip.Flowline[I].FProjectionPoint.X := FreeShip.Flowline[I].FProjectionPoint.X * UnitConversionFactor;
      FreeShip.Flowline[I].FProjectionPoint.Y := FreeShip.Flowline[I].FProjectionPoint.Y * UnitConversionFactor;
      FreeShip.Flowline[I].Build := False;
    end;
    FStartDraft := FStartDraft * UnitConversionFactor;''')
    path.write_text(s,encoding='utf8')

    path=work/'Units/FreeSubdivisionLayer.inc';s=path.read_text('utf-8-sig')
    s=replace_once(s,'Uppercase(Val) <> Uppercase(FDescription)','Val <> FDescription')
    path.write_text(s,encoding='utf8')
    path=work/'Units/FreeGeometry.pas';s=path.read_text('utf-8-sig')
    s=replace_once(s,'    property Name: string\n      read FGetName write FSetName;',
        '    property ServiceStoredName: string read FDescription;\n'
        '    property ServiceFaces: TFasterListTFreeSubdivisionControlFace read FPatches;\n'
        '    property Name: string\n      read FGetName write FSetName;')
    s=replace_once(s,'    property ActiveLayer: TFreeSubdivisionLayer\n',
        '    property ServiceLastUsedLayerID: integer read FLastusedLayerID write FLastusedLayerID;\n'
        '    property ActiveLayer: TFreeSubdivisionLayer\n')
    # Read/reorder the existing reverse lists in this isolated adapter only.
    # FBM stores forward relations, but not live setter insertion order.
    s=replace_once(s,'    property AnchorPoint: TFreeSubdivisionControlPoint read FAnchorPoint write SetAnchorPoint;',
        '    property ServiceAnchoredPoints: TFasterListTFreeSubdivisionControlPoint read FAnchoredPoints;\n'
        '    property ServiceLinearPoints: TFasterListTFreeSubdivisionControlPoint read FLinearConstrainedPoints;\n'
        '    property AnchorPoint: TFreeSubdivisionControlPoint read FAnchorPoint write SetAnchorPoint;')
    path.write_text(s,encoding='utf8')

    # M2 changes image calibration only, never pixels or compression quality.
    # Assigning CompressionQuality invalidates the loaded JPEG stream even if
    # its stored quality value is unchanged; each history capture then loses
    # another generation. Save the unmodified original JPEG bytes in the service.
    path=work/'Units/FreeBackgroundImageData.inc';s=path.read_text('utf-8-sig')
    s=replace_once(s,'  FImageData.CompressionQuality := FQuality;',
        "  if ParamStr(1)<>'--serve' then FImageData.CompressionQuality := FQuality;")
    path.write_text(s,encoding='utf8')

    # Only the configuration-loading paths are reachable here. Make their
    # existing errors fail the request instead of waiting on an invisible box.
    path=work/'Units/FreePreferences.inc';s=path.read_text('utf-8-sig')
    for expression in ('rsGlobalConfigLoadErrorMessage + E.Message',
                       'rsUserConfigLoadErrorMessage + E.Message',
                       'format(rsThemeLoadErrorMessage,[FThemeName]) + E.Message'):
        s=replace_once(s,'ShowMessage('+expression+');','raise Exception.Create('+expression+');')
    s=replace_once(s,"MessageDlg(rs_Could_not_load_program_preferences_from_the_following_location {UserString[176]} + ':' + EOL + Filename, mtError, [mbOK], 0);",
        "raise EReadError.Create(rs_Could_not_load_program_preferences_from_the_following_location + ':' + EOL + Filename);")
    path.write_text(s,encoding='utf8')

    # Integrity failures must be protocol errors, including malformed files
    # rejected before the transactional loader adopts their contents.
    path=work/'Units/FreeSubdivisionSurface.inc';s=path.read_text('utf-8-sig')
    for message in ('rsLocLoadedModelIntegrity','rsLocBeforeBuildIntegrity','rsLocAfterBuildIntegrity'):
        s=replace_once(s,f'then ShowMessage({message});',f'then raise EReadError.Create({message});')
    path.write_text(s,encoding='utf8')

    # Errors must reach QProcess, not a modal exception window in a hidden child.
    path=work/'Units/FreeLogger.pas';s=path.read_text('utf-8-sig')
    s=replace_once(s,'  ExceptionDlg := TExceptionDlg.Create(nil);','  ExceptionDlg := nil; // scene-only bridge')
    s=replace_once(s,'  if ExceptionDlg<>nil then','  if ExceptionDlg=nil then raise Exception.Create(Report);\n  if ExceptionDlg<>nil then')
    path.write_text(s,encoding='utf8')
    # Independent compatibility oracle for flow tracing. The original helper
    # leaves its out parameter uninitialized; the caller needs the edge's mub.
    # Correct only the isolated copy, never the protected Plus baseline.
    path=work/'Units/FreeGometry_Functions.inc';s=path.read_text('utf-8-sig')
    a=s.index('function Lines3DIntersect(');b=s.index('end;{Lines3DIntersect}',a)
    part=s[a:b];part=replace_once(part,'    Int := Pa;','    Param := mub; // initialized edge parameter, not ray parameter\n    Int := Pa;')
    s=s[:a]+part+s[b:];path.write_text(s,encoding='utf8')
    performance_adapter.adapt(work,PILOT/'bridge',replace_once)
    plate_adapter.adapt(work,PILOT/'bridge',replace_once)
    cad_adapter.adapt(work,PILOT/'bridge',replace_once)


def build(source,out,reference=False):
    out.mkdir(parents=True,exist_ok=False)
    before=common.fingerprint(source)[0]
    report={'source_sha256':before,'status':'error','adapter_sha256':adapter_hashes(),'reference':reference}
    try:
        with common.temporary_workspace('fs-qtbridge-') as td:
            work=Path(td)
            report['build']=native.build(source,work,common.DEFAULT_LAZARUS,out,'current','standalone',
                probe=TESTS/'QtLegacyBridge.pas' if reference else PILOT/'bridge/PilotBridge.pas',
                prepare=adapt if reference else adapt_scene)
            shutil.copy2(work/'NativeLinesplanProbe.exe',out/'FreeShipGeometryBridge.exe')
            # Rebuildable baseline, including original licenses and dirty-tree files.
            with zipfile.ZipFile(out/'legacy-source.zip','w',zipfile.ZIP_DEFLATED) as z:
                for f in common.source_files(source):z.write(f,f.relative_to(source))
                for name in ('COPYING','README.txt','Whatsnew.txt'):
                    if (source/name).exists():z.write(source/name,name)
            for name in ('locale','Themes','Languages'):shutil.copytree(source/name,out/name)
            (out/'source-id.txt').write_text(before,encoding='ascii')
        if common.fingerprint(source)[0]!=before:raise RuntimeError('Source changed during snapshot/build')
        report['status']='pass'
    except Exception as e:report['error']=str(e)
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    return report

def adapter_hashes():
    files=[PILOT/'bridge'/n for n in ('build_bridge.py','PilotBridge.pas','ModelService.pas','DocumentAdapter.inc','ModelEdits.pas','NativeLinesplanInput.pas','TopologyEdits.inc','SectionEdits.inc','OverlaySnapshot.inc','BackgroundEdits.inc','DisplayEdits.inc','AuxiliaryCurves.inc','AdvancedEdits.inc','ModelingEdits.inc','ModelTools.inc','NewHull.inc','LackenbyEngine.pas','StableEdgeLoops.pas','PilotCapture.pas','FreeLinesplanScene.pas','scene-origin.json','extract_scene.py','SceneHeader.inc')]
    files += [PILOT/'bridge'/'HydrostaticsEngine.pas',PILOT/'bridge'/'HydrostaticsEdits.inc',PILOT/'bridge'/'ExchangeImport.inc']
    files += [PILOT/'bridge'/n for n in ('PerformanceEngine.pas','PerformanceHooks.pas','performance_adapter.py')]
    files += [PILOT/'bridge'/n for n in ('PlateEngine.pas','PlateHooks.pas','PlateSnapshot.inc','plate_adapter.py')]
    files += [PILOT/'bridge'/n for n in ('CadExportEngine.pas','cad_adapter.py')]
    files += [TESTS/'native_flicker_test.py',TESTS/'freeship_ui_test.py',TESTS/'QtLegacyBridge.pas']
    return {str(f.relative_to(PILOT.parent)):common.digest_file(f) for f in files}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE);p.add_argument('--output',type=Path,required=True);p.add_argument('--reference',action='store_true');a=p.parse_args()
    r=build(a.source.resolve(),a.output.resolve(),a.reference);print(json.dumps(r));sys.exit(0 if r['status']=='pass' else 1)
