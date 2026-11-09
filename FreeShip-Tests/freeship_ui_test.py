"""Build an isolated, instrumented copy of the current FreeShip desktop source.

The tool never operates the user's desktop. All files written by the tested app
and the compiler live under the private run directory; only reports are copied out.
"""
from __future__ import annotations

import argparse
from collections import Counter
from contextlib import contextmanager
import hashlib
import json
import os
import platform
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
COURSE = ROOT.parent
DEFAULT_SOURCE = COURSE / 'FreeShip-Plus-Lazarus'
DEFAULT_LAZARUS = COURSE / 'FreeShip-MCP' / '.toolchain' / 'lazarus'
FIXTURE = COURSE / 'FreeShip-MCP' / 'examples' / 'box.fbm'
FOLDERS = ('Forms', 'Units', 'Packages', 'Cursors', 'locale', 'Themes', 'Languages')
ROOT_SUFFIXES = {'.dpr', '.lpi', '.inc', '.ico', '.res'}
FLAGS = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
TEST_FILES = ('freeship_ui_test.py', 'UiProbe.pas', 'UiTrace.pas', 'UiExpanded.pas', 'UiNativeRender.pas',
              'UiDisplay.pas', 'UiShortcuts.pas', 'UiLabels.pas', 'UiPerformance.pas', 'UiPersistence.pas', 'UiModels.pas', 'UiHistory.pas', 'UiInventory.pas', 'UiDynamic.pas', 'UiDiscovery.pas', 'UiLayers.pas', 'UiLanguageMenus.pas', 'UiSelectedDetails.pas', 'dynamic_callbacks.json')
MODEL_FIXTURES = (
    ('board', 'Ships/board.fbm', '5031cc8b5c49db95e137d3b804560ac9309533fc5db3d17cfd8fefdafdd50294'),
    ('ferry', 'Ships/Ferries+deck.fbm', 'dd35dac4659cbc0bf6b0bbdce5aa840f2284dab5008a47c088537601a0b79295'),
    ('containership', 'Ships/FromRhino/Containership.fbm', 'f8198aef6878d3462b1e1d7c2babe12b96d77c31ad056495fb3a7c6fc557028f'),
)


@contextmanager
def temporary_workspace(prefix: str):
    parent = Path(tempfile.gettempdir()).resolve(strict=True)
    path = Path(tempfile.mkdtemp(prefix=prefix, dir=parent))
    try:
        yield path
    finally:
        # Delete only the uniquely created direct child of the system temp dir.
        if path.is_symlink() or path.resolve(strict=True).parent != parent or not path.name.startswith(prefix):
            raise RuntimeError('Refusing to clean up an unexpected temporary path')
        shutil.rmtree(path)


def digest_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def test_code_hashes():
    return {name: digest_file(ROOT / name) for name in TEST_FILES}


def source_files(src: Path):
    for folder in FOLDERS:
        for p in sorted((src / folder).rglob('*')):
            if p.is_file():
                yield p
    for p in sorted(src.iterdir()):
        if p.is_file() and p.suffix.lower() in ROOT_SUFFIXES:
            yield p


def fingerprint(src: Path) -> tuple[str, int]:
    h = hashlib.sha256()
    count = 0
    for p in source_files(src):
        h.update(p.relative_to(src).as_posix().encode('utf8'))
        h.update(b'\0')
        h.update(hashlib.sha256(p.read_bytes()).digest())
        count += 1
    return h.hexdigest(), count


# Each source edit must still match exactly once. Whitespace tolerance applies
# only between Pascal lexical tokens; quoted text and comments remain exact.
INSTRUMENTATION_MATCHES = []
_PASCAL_TOKENS = re.compile(r"'(?:''|[^'])*'|//[^\r\n]*|\{[^}]*\}|\(\*[\s\S]*?\*\)|[A-Za-z_][A-Za-z_0-9]*|\d+(?:\.\d+)?|:=|<>|<=|>=|\.\.|[^\s]", re.M)


def pascal_whitespace_pattern(text: str):
    tokens = list(_PASCAL_TOKENS.finditer(text))
    if not tokens or any(text[a.end():b.start()].strip() for a,b in zip(tokens,tokens[1:])):
        raise ValueError('Unable to tokenize instrumentation anchor safely')
    pieces = []
    for token in tokens:
        value = token.group()
        if re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*|\d+(?:\.\d+)?', value):
            pieces.append(r'(?<![A-Za-z_0-9])' + re.escape(value) + r'(?![A-Za-z_0-9])')
        elif value.startswith('//'):
            pieces.append(re.escape(value) + r'(?=\r?\n|$)')
        else:
            pieces.append(re.escape(value))
    return re.compile(r'\s*'.join(pieces), re.M)


def replace_once(path: Path, old: str, new: str):
    text = path.read_text(encoding='utf-8')
    count = text.count(old)
    strategy = 'exact'
    if count == 1:
        text = text.replace(old, new, 1)
    elif count > 1:
        raise RuntimeError(f'Instrumentation ambiguous ({count} exact matches): {path.name}: {old[:100]!r}')
    else:
        pattern = pascal_whitespace_pattern(old)
        matches = list(pattern.finditer(text))
        if len(matches) != 1:
            raise RuntimeError(f'Instrumentation point changed ({len(matches)} whitespace-tolerant matches): {path.name}: {old[:100]!r}')
        match = matches[0]
        text = text[:match.start()] + new + text[match.end():]
        strategy = 'token_whitespace'
    INSTRUMENTATION_MATCHES.append({'file':path.name,'anchor':old[:120], 'strategy':strategy})
    path.write_text(text, encoding='utf-8', newline='\n')


def junction_for(target: Path, temp: Path) -> Path:
    name = 'fs-ui-tools-' + hashlib.sha256(str(target).encode('utf8')).hexdigest()[:12]
    alias = temp / name
    if not alias.exists():
        env = os.environ.copy()
        env['FS_UI_LINK'] = str(alias)
        env['FS_UI_LINK_TARGET'] = str(target)
        subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-Command',
                        'New-Item -ItemType Junction -Path $env:FS_UI_LINK -Target $env:FS_UI_LINK_TARGET | Out-Null'],
                       env=env, check=True, creationflags=FLAGS, capture_output=True)
    if alias.resolve() != target.resolve():
        raise RuntimeError('The compiler alias points to an unexpected location')
    return alias


def prepare(src: Path, work: Path, expected_source_hash: str | None = None):
    for name in FOLDERS:
        shutil.copytree(src / name, work / name)
    for p in src.iterdir():
        if p.is_file() and p.suffix.lower() in ROOT_SUFFIXES:
            shutil.copy2(p, work / p.name)
    snapshot_hash, _ = fingerprint(work)
    if expected_source_hash is not None and snapshot_hash != expected_source_hash:
        raise RuntimeError('Copied source does not match the initial fingerprint')
    for name in ('Ships', 'Manuals', 'Import'):
        (work / name).mkdir(exist_ok=True)
    shutil.copy2(FIXTURE, work / 'fixture.fbm')
    shutil.copy2(ROOT / 'UiTrace.pas', work / 'Units' / 'UiTrace.pas')
    shutil.copy2(ROOT / 'UiProbe.pas', work / 'Units' / 'UiProbe.pas')
    shutil.copy2(ROOT / 'UiExpanded.pas', work / 'Units' / 'UiExpanded.pas')
    shutil.copy2(ROOT / 'UiNativeRender.pas', work / 'Units' / 'UiNativeRender.pas')
    shutil.copy2(ROOT / 'UiDisplay.pas', work / 'Units' / 'UiDisplay.pas')
    shutil.copy2(ROOT / 'UiShortcuts.pas', work / 'Units' / 'UiShortcuts.pas')
    shutil.copy2(ROOT / 'UiLabels.pas', work / 'Units' / 'UiLabels.pas')
    shutil.copy2(ROOT / 'UiPerformance.pas', work / 'Units' / 'UiPerformance.pas')
    shutil.copy2(ROOT / 'UiPersistence.pas', work / 'Units' / 'UiPersistence.pas')
    shutil.copy2(ROOT / 'UiModels.pas', work / 'Units' / 'UiModels.pas')
    shutil.copy2(ROOT / 'UiHistory.pas', work / 'Units' / 'UiHistory.pas')
    shutil.copy2(ROOT / 'UiInventory.pas', work / 'Units' / 'UiInventory.pas')
    shutil.copy2(ROOT / 'UiDynamic.pas', work / 'Units' / 'UiDynamic.pas')
    shutil.copy2(ROOT / 'UiDiscovery.pas', work / 'Units' / 'UiDiscovery.pas')
    shutil.copy2(ROOT / 'UiLayers.pas', work / 'Units' / 'UiLayers.pas')
    shutil.copy2(ROOT / 'UiLanguageMenus.pas', work / 'Units' / 'UiLanguageMenus.pas')
    shutil.copy2(ROOT / 'UiSelectedDetails.pas', work / 'Units' / 'UiSelectedDetails.pas')
    replace_once(work / 'Forms' / 'Main.pas', 'uses //FreeSplashWndw,', 'uses UiTrace, //FreeSplashWndw,')
    hull_file = work / 'Forms' / 'freehullformwindow_panel.pas'
    replace_once(hull_file, '  procedure SetCaption;\n  procedure UpdateMenu;',
                 '  procedure SetCaption;\n  procedure UpdateMenu;\n  function UiProbeCoordinateHandler: TNotifyEvent;')
    if 'procedure TFreeHullWindow.CoordinateAxisClick(Sender: TObject);' in hull_file.read_text(encoding='utf8'):
        replace_once(hull_file, 'procedure TFreeHullWindow.CoordinateAxisClick(Sender: TObject);',
                     'function TFreeHullWindow.UiProbeCoordinateHandler: TNotifyEvent;\n'
                     'begin Result:=@CoordinateAxisClick end;\n\n'
                     'procedure TFreeHullWindow.CoordinateAxisClick(Sender: TObject);')
    else:
        replace_once(hull_file, 'procedure TFreeHullWindow.PopupMenuHullPopup(Sender: TObject);',
                     'function TFreeHullWindow.UiProbeCoordinateHandler: TNotifyEvent;\n'
                     'begin Result:=nil end;\n\n'
                     'procedure TFreeHullWindow.PopupMenuHullPopup(Sender: TObject);')
    replace_once(work / 'Forms' / 'Main.pas',
                 '      procedure RefreshStudioLanguage;',
                 '      procedure RefreshStudioLanguage;\n      function UiProbeOverflowButton(Index: Integer): TSpeedButton;\n      function UiProbeOverflowHandler: TNotifyEvent;\n      function UiProbeBarResizeHandler: TNotifyEvent;')
    replace_once(work / 'Forms' / 'Main.pas',
                 'procedure TMainForm.StudioOverflowPopup(Sender: TObject);',
                 'function TMainForm.UiProbeOverflowHandler:TNotifyEvent; begin Result:=StudioOverflowPopup end;\n'
                 'function TMainForm.UiProbeBarResizeHandler:TNotifyEvent; begin Result:=StudioBarResize end;\n'
                 'function TMainForm.UiProbeOverflowButton(Index: Integer): TSpeedButton;\n'
                 'begin Result:=nil; if (Index>=Low(FStudioButtons)) and (Index<=High(FStudioButtons)) then Result:=FStudioButtons[Index] end;\n\n'
                 'procedure TMainForm.StudioOverflowPopup(Sender: TObject);')
    replace_once(work / 'Forms' / 'Main.pas',
                 '    MessageDlg(Format(rsQuickLanguageMissing, [LanguageDisplayName(Target)]), mtWarning, [mbOK], 0);',
                 '    UiTrace.SafeMessageDlg(Format(rsQuickLanguageMissing, [LanguageDisplayName(Target)]), mtWarning, [mbOK], 0);')
    replace_once(work / 'Forms' / 'Main.pas',
                 'on E: Exception do MessageDlg(Format(rsQuickCannotSave, [E.Message]), mtWarning, [mbOK], 0);',
                 'on E: Exception do UiTrace.SafeMessageDlg(Format(rsQuickCannotSave, [E.Message]), mtWarning, [mbOK], 0);')
    # Instrument only the named-load adapter; retain its failure guards.
    main_path = work / 'Forms' / 'Main.pas'
    main_text = main_path.read_text(encoding='utf8')
    start = main_text.index('procedure TMainForm.LoadNamedFile(')
    end = main_text.index('end;{TMainForm.LoadMostRecentFile}', start)
    body = main_text[start:end]
    for statement, stage in (
        ('Freeship.Edit.File_Load(Filename);', 'load_file_returned'),
        ('Freeship.RebuildModel;', 'rebuild_returned'),
        ('FOpenHullWindows;', 'views_created'),
        ('Freeship.ZoomFitAllViewports;', 'zoom_fit'),
        ('UpdateMenu;', 'menu_updated')):
        if body.count(statement) != 1:
            raise RuntimeError('Named-load trace anchor changed: ' + statement)
        body = body.replace(statement, statement + " UiTrace.MarkStage('" + stage + "');")
    main_path.write_text(main_text[:start] + body + main_text[end:], encoding='utf8', newline='\n')
    replace_once(work / 'Units' / 'FreeEdit.inc',
                 '      Source.LoadFromFile(FileName);',
                 "      UiTrace.MarkStage('file_read_begin'); Source.LoadFromFile(FileName); UiTrace.MarkStage('file_read_end');")
    replace_once(work / 'Units' / 'FreeEdit.inc',
                 '      FreeShip.LoadProject(Source, False);',
                 "      UiTrace.MarkStage('parse_begin'); FreeShip.LoadProject(Source, False); UiTrace.MarkStage('parse_end');")
    # A malformed model must be reported without opening a modal desktop dialog.
    replace_once(work / 'Units' / 'FreeEdit.inc', '        ExceptionDlg.ShowModal;',
                 '        raise Exception.Create(ErrMsg); { offscreen test: never show modal error dialog }')
    save_file = work / 'Units' / 'FreeEdit.inc'
    save_text = save_file.read_text(encoding='utf8')
    save_start = save_text.index('function TFreeEdit.File_Save:boolean;')
    save_end = save_text.index('end;{TFreeEdit.File_Save}', save_start)
    save_body = save_text[save_start:save_end]
    if not save_body.count('MessageDlg('):
        raise RuntimeError('Save dialog instrumentation changed')
    save_text = save_text[:save_start] + save_body.replace('MessageDlg(', 'UiTrace.SafeMessageDlg(') + save_text[save_end:]
    save_file.write_text(save_text, encoding='utf8', newline='\n')
    replace_once(work / 'Units' / 'FreeGeometry.pas',
                 'uses FreeStringsUnit, FreeSmoothLines,',
                 'uses UiTrace, FreeStringsUnit, FreeSmoothLines,')
    replace_once(work / 'Units' / 'FreeShipUnit.pas',
                 'uses Math,',
                 'uses UiTrace, Math,')
    replace_once(work / 'Units' / 'FreeLabelLayout.pas', 'implementation\nconstructor',
                 'implementation\nuses UiTrace;\nconstructor')
    replace_once(work / 'Units' / 'FreeLabelLayout.pas',
                 '  Result.Visible:=False; Result.Collisions:=0;',
                 "  UiTrace.SetLayoutReason('unclassified');\n  Result.Visible:=False; Result.Collisions:=0;")
    replace_once(work / 'Units' / 'FreeLabelLayout.pas',
                 '  if (Width<=0) or (Height<=0) then Exit;',
                 "  if (Width<=0) or (Height<=0) then begin UiTrace.SetLayoutReason('empty text extent'); Exit end;")
    layout_path = work / 'Units' / 'FreeLabelLayout.pas'
    layout_content = layout_path.read_text(encoding='utf8')
    old_anchor = '     (Anchor.Y<FBounds.Top-Height) or (Anchor.Y>FBounds.Bottom+Height) then Exit;'
    new_anchor = '    (Anchor.Y<FBounds.Top) or (Anchor.Y>=FBounds.Bottom)) then Exit;'
    anchor = old_anchor if old_anchor in layout_content else new_anchor
    if anchor in layout_content:
        replace_once(layout_path, anchor, anchor.replace('then Exit;',
                     "then begin UiTrace.SetLayoutReason('anchor outside layout bounds'); Exit end;"))
    elif "if HasOld and (Kind=lkNamedLine) then begin Best:=Cache.Target;BestScore:=0 end" in layout_content:
        replace_once(layout_path, '    else Exit;\n  end;\n  if BestScore > 0 then begin',
            "    else begin UiTrace.SetLayoutReason('new offscreen anchor'); Exit end;\n  end;\n  if BestScore > 0 then begin")
    else:
        raise RuntimeError('Unknown anchor-history instrumentation')
    if '  if BestScore=1e100 then Exit;' in layout_path.read_text(encoding='utf8'):
        replace_once(layout_path, '  if BestScore=1e100 then Exit;',
                     "  if BestScore=1e100 then begin UiTrace.SetLayoutReason('no valid candidate within bounds and collision constraints'); Exit end;")
    else:
        replace_once(layout_path, '  if BestScore=1e100 then begin\n    if Cache<>nil then Cache.ShownRight:=Cache.ShownLeft;',
                     "  if BestScore=1e100 then begin\n    UiTrace.SetLayoutReason('no valid candidate within bounds and collision constraints');\n    if Cache<>nil then Cache.ShownRight:=Cache.ShownLeft;")
    layout_file = work / 'Units' / 'FreeLabelLayout.pas'
    layout_text = layout_file.read_text(encoding='utf8')
    layout_text = layout_text.replace('  if ((Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top)) and (Kind<>lkNamedLine) then Exit;', '  if (Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top) then if Kind<>lkNamedLine then Exit;')
    if layout_text.count('  if RectCollides(R) then Exit;') == 2:
        layout_text = layout_text.replace('  if RectCollides(R) then Exit;',
            "  if RectCollides(R) then begin UiTrace.SetLayoutReason('chosen rectangle collides'); Exit end;")
    elif ('  if (Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top) then Exit;' in layout_text and
          '  if (Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top) then begin\n    if Cache<>nil then Cache.ShownRight:=Cache.ShownLeft;' in layout_text):
        layout_text = layout_text.replace('  if (Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top) then Exit;',
            "  if (Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top) then begin UiTrace.SetLayoutReason('candidate clipping region empty'); Exit end;")
        debug = ''
        if 'Inc(X,EdgeShift.X); Inc(Y,EdgeShift.Y);' in layout_text:
            debug = ('    UiTrace.SetLayoutDebug(Anchor.X+Best.X,Anchor.Y+Best.Y,X,Y,'
                     'EdgeShift.X,EdgeShift.Y,HasOld);\n')
        layout_text = layout_text.replace('  if (Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top) then begin\n    if Cache<>nil then Cache.ShownRight:=Cache.ShownLeft;',
            "  if (Clip.Right<=Clip.Left) or (Clip.Bottom<=Clip.Top) then begin\n    UiTrace.SetLayoutReason('edge-adjusted or animated clipping region empty');\n" + debug + '    if Cache<>nil then Cache.ShownRight:=Cache.ShownLeft;')
    elif 'An occluded label still has a moving anchor' in layout_text:
        pass  # New continuous-history path; placement evidence is still collected.
    else:
        raise RuntimeError('Layout collision instrumentation changed')
    layout_file.write_text(layout_text, encoding='utf8', newline='\n')
    replace_once(work / 'Units' / 'FreeLabelLayout.pas',
                 '    if Assigned(FExtraCost) and (FExtraCost(R) >= 6) and not HasOld then Exit;',
                 "    if Assigned(FExtraCost) and (FExtraCost(R) >= 6) and not HasOld then begin UiTrace.SetLayoutReason('background cost threshold'); Exit end;")
    replace_once(work / 'Forms' / 'FreeLinesplanFrme.pas',
                 'uses FreeStringsUnit;', 'uses UiTrace, FreeStringsUnit;')
    # Never alter the cursor on the user's active desktop during an offscreen render.
    replace_once(work / 'Forms' / 'FreeLinesplanFrme.pas',
                 '  Screen.Cursor := crHourglass;', '  { offscreen test: leave desktop cursor alone }')
    replace_once(work / 'Forms' / 'FreeLinesplanFrme.pas',
                 '    Screen.Cursor := Prevcursor;', '    { offscreen test: no cursor to restore }')
    replace_once(work / 'Units' / 'FreeHydrostaticCalc.inc',
                 '  Screen.Cursor := crHourGlass;', '  { offscreen test: leave desktop cursor alone }')
    replace_once(work / 'Units' / 'FreeHydrostaticCalc.inc',
                 '    Screen.Cursor := PrevCursor;', '    { offscreen test: no cursor to restore }')
    replace_once(work / 'Forms' / 'FreeLinesplanFrme.pas',
                 '      Point(X,Y), Point(X,Y), Viewport.TextWidth(Text), Viewport.TextHeight(Text), Kind, Outward);',
                 '      Point(X,Y), Point(X,Y), Viewport.TextWidth(Text), Viewport.TextHeight(Text), Kind, Outward);\n'
                 '    if not Placement.Visible then Placement.Position:=Point(0,0);\n'
                 "    if Kind=lkNamedLine then UiTrace.RecordPlacement('lines_named','lines:'+IntToStr(LabelIndex),Text,X,Y,Placement.Position.X,Placement.Position.Y,Viewport.TextWidth(Text),Viewport.TextHeight(Text),Placement.Visible,-1,False)\n"
                 "    else UiTrace.RecordPlacement('lines_tick','lines:'+IntToStr(LabelIndex),Text,X,Y,Placement.Position.X,Placement.Position.Y,Viewport.TextWidth(Text),Viewport.TextHeight(Text),Placement.Visible,-1,False);\n     if Placement.Visible then UiTrace.RecordPlacementClip(Placement.Rect.Left,Placement.Rect.Top,Placement.Rect.Right,Placement.Rect.Bottom);")
    replace_once(work / 'Units' / 'FreeShip.inc',
                 '          Point(Item.X,Item.Y),Viewport.TextWidth(Item.Text),Viewport.TextHeight(Item.Text),Item.Kind);',
                 '          Point(Item.X,Item.Y),Viewport.TextWidth(Item.Text),Viewport.TextHeight(Item.Text),Item.Kind);\n'
                 '        if not Placement.Visible then Placement.Position:=Point(0,0);\n'
                 "        if Item.Kind=lkNamedLine then UiTrace.RecordPlacement('grid_named',Item.Key,Item.Text,Item.X,Item.Y,Placement.Position.X,Placement.Position.Y,Viewport.TextWidth(Item.Text),Viewport.TextHeight(Item.Text),Placement.Visible,-1,False)\n"
                 "        else UiTrace.RecordPlacement('grid_tick',Item.Key,Item.Text,Item.X,Item.Y,Placement.Position.X,Placement.Position.Y,Viewport.TextWidth(Item.Text),Viewport.TextHeight(Item.Text),Placement.Visible,-1,False);\n     if Placement.Visible then UiTrace.RecordPlacementClip(Placement.Rect.Left,Placement.Rect.Top,Placement.Rect.Right,Placement.Rect.Bottom);")
    replace_once(work / 'Units' / 'FreeShip.inc',
                 '        Viewport.TextHeight(Text), lkPoint);',
                 '        Viewport.TextHeight(Text), lkPoint);\n'
                 '      if not LabelPlacement.Visible then LabelPlacement.Position:=Point(0,0);\n'
                 "      UiTrace.RecordPlacement('hydro','hydro:'+Text,Text,Pt.X,Pt.Y,LabelPlacement.Position.X,LabelPlacement.Position.Y,Viewport.TextWidth(Text),Viewport.TextHeight(Text),LabelPlacement.Visible,-1,False);\n     if LabelPlacement.Visible then UiTrace.RecordPlacementClip(LabelPlacement.Rect.Left,LabelPlacement.Rect.Top,LabelPlacement.Rect.Right,LabelPlacement.Rect.Bottom);")
    replace_once(work / 'Units' / 'FreeShip.inc',
                 '              Pt, AreaPreferred, Viewport.TextWidth(Str), Viewport.TextHeight(Str), lkPoint);',
                 '              Pt, AreaPreferred, Viewport.TextWidth(Str), Viewport.TextHeight(Str), lkPoint);\n'
                 '            if not AreaLabel.Visible then AreaLabel.Position:=Point(0,0);\n'
                 "            UiTrace.RecordPlacement('sectional-area','sectional-area:'+IntToStr(I),Str,Pt.X,Pt.Y,AreaLabel.Position.X,AreaLabel.Position.Y,Viewport.TextWidth(Str),Viewport.TextHeight(Str),AreaLabel.Visible,-1,False);\n     if AreaLabel.Visible then UiTrace.RecordPlacementClip(AreaLabel.Rect.Left,AreaLabel.Rect.Top,AreaLabel.Rect.Right,AreaLabel.Rect.Bottom);")
    vp_path = work / 'Units' / 'FreeViewport.inc'
    if '  FDrawingCanvas.TextOut(x, y, val);' in vp_path.read_text(encoding='utf8'):
        replace_once(vp_path, '  FDrawingCanvas.TextOut(x, y, val);',
                     '  UiTrace.RecordText(x, y, FDrawingCanvas.TextWidth(val), FDrawingCanvas.TextHeight(val), val);\n'
                     '  FDrawingCanvas.TextOut(x, y, val);')
    else:
        replace_once(vp_path, '        FDrawingCanvas.TextRect(R,x,y,val,Style);',
                     '        UiTrace.RecordText(x,y,FDrawingCanvas.TextWidth(val),FDrawingCanvas.TextHeight(val),val);\n'
                     '        FDrawingCanvas.TextRect(R,x,y,val,Style);')
        replace_once(vp_path, '    end else FDrawingCanvas.TextOut(x,y,val);',
                     '    end else begin UiTrace.RecordText(x,y,FDrawingCanvas.TextWidth(val),FDrawingCanvas.TextHeight(val),val); FDrawingCanvas.TextOut(x,y,val) end;')
    replace_once(work / 'Units' / 'FreeViewport.inc',
                 '  Previous := FDrawingCanvas.PenPos;',
                 '  Previous := FDrawingCanvas.PenPos;\n'
                 '  UiTrace.RecordSegment(Previous.X,Previous.Y,x,y);')
    replace_once(work / 'Units' / 'FreeViewport.inc',
                 'procedure TFreeViewport.Line(x1, y1, x2, y2: integer);\nbegin',
                 'procedure TFreeViewport.Line(x1, y1, x2, y2: integer);\nbegin\n'
                 '  UiTrace.RecordSegment(x1,y1,x2,y2);')
    replace_once(work / 'Units' / 'FreeViewport.inc',
                 '  ProjectForDrawing(Point2, X2, Y2);',
                 '  ProjectForDrawing(Point2, X2, Y2);\n'
                 '  UiTrace.RecordSegment(Round(X1),Round(Y1),Round(X2),Round(Y2));')
    viewport_inc = work / 'Units' / 'FreeViewport.inc'
    polyline_header = 'procedure TFreeViewport.Polyline(const Points: array of TPoint);\n'
    has_overlay_polyline = (polyline_header + 'var I:Integer;') in viewport_inc.read_text(encoding='utf-8')
    replace_once(viewport_inc,
                 polyline_header + ('var I:Integer;\n' if has_overlay_polyline else '') + 'begin',
                 polyline_header + ('var I, TraceIndex: Integer;\nbegin\n' if has_overlay_polyline else 'var TraceIndex: Integer;\nbegin\n') +
                 '  for TraceIndex:=1 to High(Points) do\n'
                 '    UiTrace.RecordSegment(Points[TraceIndex-1].X,Points[TraceIndex-1].Y,Points[TraceIndex].X,Points[TraceIndex].Y);')
    replace_once(work / 'Units' / 'FreeViewport.inc',
                 '    for I := 0 to High(Points) do\n      ProjectForDrawing(Points[I], FloatPoints[I].X, FloatPoints[I].Y);',
                 '    for I := 0 to High(Points) do begin\n'
                 '      ProjectForDrawing(Points[I], FloatPoints[I].X, FloatPoints[I].Y);\n'
                 '      if I>0 then UiTrace.RecordSegment(Round(FloatPoints[I-1].X),Round(FloatPoints[I-1].Y),Round(FloatPoints[I].X),Round(FloatPoints[I].Y));\n'
                 '    end;')
    replace_once(work / 'Units' / 'FreeViewport.inc',
                 '  if Hypot(X-Anchor.X,Y-Anchor.Y)<28 then Exit;',
                 '  if Hypot(X-Anchor.X,Y-Anchor.Y)<28 then Exit;\n'
                 '  UiTrace.RecordLeader(Anchor.X, Anchor.Y, X, Y);')
    replace_once(work / 'Units' / 'FreeSubdivisionControlPoint.inc',
                 '        Viewport.TextWidth(PointLabel), Viewport.TextHeight(PointLabel), lkPoint);',
                 '        Viewport.TextWidth(PointLabel), Viewport.TextHeight(PointLabel), lkPoint);\n'
                 '      if not LabelPlacement.Visible then LabelPlacement.Position := Point(0,0);\n'
                 "      UiTrace.RecordPlacement('point', '', PointLabel, P.X, P.Y,\n"
                 '        LabelPlacement.Position.X, LabelPlacement.Position.Y, Viewport.TextWidth(PointLabel),\n'
                 '        Viewport.TextHeight(PointLabel), LabelPlacement.Visible, VertexIndex, Selected);\n'
                 '      if LabelPlacement.Visible then UiTrace.RecordPlacementClip(LabelPlacement.Rect.Left,LabelPlacement.Rect.Top,LabelPlacement.Rect.Right,LabelPlacement.Rect.Bottom);')
    replace_once(work / 'FreeShip.dpr',
                 "  Main in 'Forms/Main.pas' {MainForm},",
                 "  UiProbe in 'Units/UiProbe.pas',\n  Main in 'Forms/Main.pas' {MainForm},")
    replace_once(work / 'FreeShip.dpr', '   Application.Run;', '   UiProbe.RunProbe(MainForm);')
    # Avoid upstream pre-build scripts and their writes to the original checkout.
    project = work / 'FreeShip.lpi'
    tree = ET.parse(project)
    for parent in tree.iter():
        for child in list(parent):
            if child.tag == 'ExecuteBefore':
                parent.remove(child)
    tree.write(project, encoding='utf-8', xml_declaration=True)


VIEWS = ('perspective', 'profile', 'plan', 'bodyplan')
AXES = {'off': 0, 'x': 1, 'y': 2, 'z': 3}


def classify_statuses(statuses: list[str]) -> str:
    # Accept a one-shot iterator without consuming it in the validation pass.
    statuses = list(statuses)
    valid = {'pass', 'fail', 'not_run', 'review', 'error'}
    if not statuses or any(status not in valid for status in statuses) or 'error' in statuses:
        return 'error'
    if 'fail' in statuses:
        return 'fail'
    if 'not_run' in statuses or 'review' in statuses:
        return 'partial'
    return 'pass'


def analyze_draws(cases: list[dict], evidence: Path, views=VIEWS, axes=(0, 1, 2, 3)):
    """Independent expectations for the known eight-point box fixture."""
    point_values = {
        1: ('0.000','0.000','20.000','20.000','0.000','20.000','0.000','20.000'),
        2: ('0.000','3.000','3.000','0.000','3.000','3.000','0.000','0.000'),
        3: ('0.000','0.000','0.000','0.000','8.000','8.000','8.000','8.000'),
    }
    new = []
    for view in views:
      prefix = f'view.{view}.'
      for axis in axes:
        draw = next((c for c in cases if c.get('id') == f'{prefix}draw.label.{axis}'), None)
        if not draw or 'text_trace' not in draw or 'placement_trace' not in draw:
            if draw and draw.get('status') != 'not_run':
                draw['legacy_status'] = draw.get('status')
                draw['status'] = 'not_run'
                draw['note'] = 'Point placement or text trace unavailable; a string match alone is inconclusive.'
            suffixes = ['draw.all-labels', 'layout.clipped-labels', 'layout.overlapping-labels']
            if axis: suffixes.append('draw.changed-pixels')
            for suffix in suffixes:
                new.append({'id': f'{prefix}{suffix}.{axis}', 'status': 'not_run',
                            'expected': 'known fixture labels and geometry', 'actual': 'draw trace unavailable'})
            continue
        placements = [p for p in draw.get('placement_trace', []) if p.get('category') == 'point']
        expected_by_index = ({5: 'P5'} if axis == 0 else dict(enumerate(point_values[axis])))
        got_by_index = {p['point_index']: p['text'] for p in placements}
        labels = [(p['text'], p) for p in placements if p.get('visible')]
        got = Counter(label for label, _ in labels)
        expected = Counter(expected_by_index.values())
        draws = Counter((t['text'], t['x'], t['y']) for t in draw['text_trace'])
        unpainted = []
        for _, p in labels:
            key = (p['text'], p['x'], p['y'])
            if draws[key] <= 0: unpainted.append(p['point_index'])
            else: draws[key] -= 1
        omitted = [p for p in placements if not p.get('visible')]
        indices = Counter(p['point_index'] for p in placements)
        bad_text = {p['point_index']: p['text'] for p in placements
                    if p['point_index'] not in expected_by_index or
                    p['text'] != expected_by_index[p['point_index']]}
        duplicate = sorted(i for i, n in indices.items() if n > 1)
        missing = sorted(set(expected_by_index) - set(indices))
        unsettled = draw.get('layout_settled') is False
        if bad_text or duplicate:
            status = 'fail'  # Wrong requested value or identity is independent of layout.
        elif missing or unpainted:
            status = 'not_run' if unsettled else 'fail'
        elif unsettled:
            status = 'not_run'
        elif omitted:
            status = 'review'  # The application deliberately omitted placements; acceptance is contextual.
        else:
            status = 'pass'
        new.append({'id': f'{prefix}draw.all-labels.{axis}', 'status': status,
                    'expected': {'requested_counts': dict(expected), 'by_point': expected_by_index},
                    'actual': {'visible_counts': dict(got), 'by_point': got_by_index,
                               'omitted_points': [p['point_index'] for p in omitted],
                               'omitted_details': omitted, 'not_painted_points': unpainted,
                               'missing_points': missing, 'wrong_point_values': bad_text,
                               'duplicate_points': duplicate},
                    'image': draw.get('image'),
                    'note': 'Requested point values and visible drawing are checked independently; omissions require review against UI requirements.'})
        # The native probe historically looked for a text string anywhere in the
        # viewport. Associate this check with fixture point 5 and its actual
        # placement, so unrelated identical text cannot produce a false pass.
        target = [p for p in placements if p['point_index'] == 5]
        if len(target) != 1:
            target_status = 'not_run' if unsettled and not target else 'fail'
            target_actual = {'point_index': 5, 'matches': len(target)}
        else:
            point = target[0]
            target_actual = {'point_index': 5, 'placement': point,
                             'painted': point['point_index'] not in unpainted if point.get('visible') else False}
            if point['text'] != expected_by_index[5]: target_status = 'fail'
            elif unsettled: target_status = 'not_run'
            elif not point.get('visible'): target_status = 'review'
            elif point['point_index'] in unpainted: target_status = 'fail'
            else: target_status = 'pass'
        if draw.get('status') != target_status:
            draw['legacy_status'] = draw.get('status')
        draw['status'] = target_status
        draw['expected'] = {'point_index': 5, 'text': expected_by_index[5]}
        draw['actual'] = target_actual
        draw['note'] = 'Point-specific evidence; layout omissions need review and do not establish acceptance.'
        clipped = [item for _, item in labels if item['x'] < 0 or item['y'] < 0 or
                   item['x'] + item['width'] > 800 or item['y'] + item['height'] > 500]
        new.append({'id': f'{prefix}layout.clipped-labels.{axis}',
                    'status': 'review' if clipped else 'pass',
                    'expected': 'coordinate text bounds inside 800x500 viewport',
                    'actual': clipped, 'image': draw.get('image'),
                    'note': 'Bounding-box warning; inspect image and requested behavior.'})
        overlaps = []
        for index, (_, a) in enumerate(labels):
            for _, b in labels[index + 1:]:
                if rectangles_overlap(a, b):
                    overlaps.append({'first': a, 'second': b})
        new.append({'id': f'{prefix}layout.overlapping-labels.{axis}',
                    'status': 'review' if overlaps else 'pass',
                    'expected': 'coordinate label bounding boxes do not overlap',
                    'actual': {'count': len(overlaps), 'examples': overlaps[:20]},
                    'image': draw.get('image'),
                    'note': 'Projected points can coincide; review the view and intended behavior.'})
        if axis:
            selected = next((item for _, item in labels if item.get('point_index') == 5), None)
            try:
                from PIL import Image, ImageChops
                if not selected:
                    new.append({'id': f'{prefix}draw.changed-pixels.{axis}', 'status': 'not_run',
                                'expected': 'visible pixel change near selected point label',
                                'actual': 'selected label not drawn; dependent pixel check not run',
                                'image': draw.get('image')})
                    continue
                if selected in clipped:
                    new.append({'id': f'{prefix}draw.changed-pixels.{axis}', 'status': 'not_run',
                                'expected': 'visible pixel change near selected point label',
                                'actual': 'selected label is clipped; pixel check is inconclusive',
                                'image': draw.get('image')})
                    continue
                rect = (max(0, selected['x']), max(0, selected['y']),
                        min(800, selected['x'] + selected['width']),
                        min(500, selected['y'] + selected['height']))
                with Image.open(evidence / f'{view}-axis-0.bmp') as base, Image.open(evidence / f'{view}-axis-{axis}.bmp') as image:
                    bbox = ImageChops.difference(base.convert('RGB').crop(rect),
                                                 image.convert('RGB').crop(rect)).getbbox()
                new.append({'id': f'{prefix}draw.changed-pixels.{axis}',
                            'status': 'pass' if bbox else 'fail',
                            'expected': 'visible pixel change near selected point label',
                            'actual': 'changed' if bbox else 'unchanged',
                            'image': draw.get('image'),
                            'note': 'A pixel difference is not proof of legibility.'})
            except ImportError:
                new.append({'id': f'{prefix}draw.changed-pixels.{axis}', 'status': 'not_run',
                            'expected': 'visible pixel change', 'actual': 'Pillow not installed'})
            except (OSError, ValueError) as exc:
                new.append({'id': f'{prefix}draw.changed-pixels.{axis}', 'status': 'error',
                            'expected': 'comparable images', 'actual': str(exc)})
    cases.extend(new)


def drawn_bounds(item):
    """Actual clipping rectangle when instrumented, otherwise conservative full text bounds."""
    clip = item.get('clip_rect')
    if isinstance(clip, dict) and all(k in clip for k in ('left', 'top', 'right', 'bottom')):
        return {'x':clip['left'], 'y':clip['top'],
                'width':max(0, clip['right']-clip['left']), 'height':max(0, clip['bottom']-clip['top'])}
    return {key:item[key] for key in ('x','y','width','height')}


def rectangles_overlap(a, b):
    a, b = drawn_bounds(a), drawn_bounds(b)
    return (a['width'] > 0 and a['height'] > 0 and b['width'] > 0 and b['height'] > 0 and
            a['x'] < b['x']+b['width'] and b['x'] < a['x']+a['width'] and
            a['y'] < b['y']+b['height'] and b['y'] < a['y']+a['height'])


def segment_hits_text(segment, text):
    # Ignore contact at the outer pixel boundary; crossings are review hints.
    text = drawn_bounds(text)
    left, top = text['x'] + 1, text['y'] + 1
    right = text['x'] + text['width'] - 1
    bottom = text['y'] + text['height'] - 1
    if left > right or top > bottom:
        return False
    x, y = segment['x1'], segment['y1']
    dx, dy = segment['x2'] - x, segment['y2'] - y
    if max(x, x + dx) < left or min(x, x + dx) > right or max(y, y + dy) < top or min(y, y + dy) > bottom:
        return False
    low, high = 0.0, 1.0
    for start, delta, minimum, maximum in ((x, dx, left, right), (y, dy, top, bottom)):
        if delta == 0:
            if not minimum <= start <= maximum:
                return False
        else:
            a, b = (minimum - start) / delta, (maximum - start) / delta
            low, high = max(low, min(a, b)), min(high, max(a, b))
            if low > high:
                return False
    return True


def analyze_labels(cases):
    """Check captured placements against TextOut without inventing omission reasons."""
    additions = []
    snapshots = [c for c in cases if c['id'].startswith('labels.') and 'placement_trace' in c]
    for snapshot in snapshots:
        label_id = snapshot['id']
        first_addition = len(additions)
        placements = snapshot['placement_trace']
        visible = [p for p in placements if p.get('visible')]
        omitted = [p for p in placements if not p.get('visible')]
        categories = Counter(p['category'] for p in placements)
        drawn = Counter((d['text'], d['x'], d['y']) for d in snapshot['text_trace'])
        missing = []
        for p in visible:
            key = (p['text'], p['x'], p['y'])
            if drawn[key] < 1:
                missing.append(p)
            else:
                drawn[key] -= 1
        outside = [p for p in visible if p['x'] < 0 or p['y'] < 0 or
                   p['x'] + p['width'] > snapshot['width'] or
                   p['y'] + p['height'] > snapshot['height']]
        overlaps = []
        for i, a in enumerate(visible):
            for b in visible[i + 1:]:
                if rectangles_overlap(a, b):
                    overlaps.append([a['category'], a['text'], b['category'], b['text']])
        expected_categories = {'lines_named', 'lines_tick'} if '.linesplan.' in label_id else {'point', 'grid_tick'}
        missing_categories = sorted(expected_categories - categories.keys())
        checks = [
            ('coverage', not missing_categories, 'required categories recorded',
             {'counts': dict(categories), 'missing': missing_categories}),
            ('drawn', not missing, 'every visible placement has a matching TextOut call',
             {'missing': missing}),
            ('bounds', not outside, 'visible text rectangles stay within the bitmap',
             {'outside': outside}),
            ('overlap', not overlaps, 'recorded visible label rectangles do not overlap',
             {'overlaps': overlaps}),
        ]
        for suffix, ok, expected, actual in checks:
            additions.append({'id': label_id + '.' + suffix, 'status': 'pass' if ok else 'fail',
                              'expected': expected, 'actual': actual})
        additions.append({'id': label_id + '.omissions', 'status': 'review' if omitted else 'pass',
                          'expected': 'inspect omitted labels and determine whether omission is acceptable',
                          'actual': {'counts': dict(Counter(p['category'] for p in omitted)),
                                     'reasons': dict(Counter(p.get('omission_reason','unknown') for p in omitted)),
                                     'examples': omitted[:30]}})
        segments = snapshot.get('segment_trace', [])
        crossings, affected = 0, []
        for p in visible:
            hits = 0
            first_segment = None
            for segment in segments:
                if segment_hits_text(segment, p):
                    crossings += 1
                    hits += 1
                    if first_segment is None:
                        first_segment = segment
            if hits:
                affected.append({'category': p['category'], 'text': p['text'], 'key': p.get('key'),
                                 'x': p['x'], 'y': p['y'], 'intersecting_segments': hits,
                                 'example_segment': first_segment})
        additions.append({'id': label_id + '.line-crossings',
                          'status': ('not_run' if not segments else 'review' if crossings else 'pass'),
                          'expected': 'inspect recorded line segments crossing text rectangles',
                          'actual': {'segments': len(segments), 'intersecting_segment_pairs': crossings,
                                     'labels_affected': len(affected), 'examples': affected[:30]},
                          'note': 'Candidate crossings only: untraced drawing primitives, label association, and pixel-level readability need further review.'})
        # Independent expected translations for two essential reference lines.
        if '.linesplan.' in label_id or ('.hull.' in label_id and ('.profile.' in label_id or '.bodyplan.' in label_id)):
            names = ('基线 ', 'DWL设计水线 ') if '.chinese.' in label_id else ('ОЛ ', 'КВЛ ')
            for name, prefix in zip(('baseline', 'design-waterline'), names):
                matches = [p for p in placements if p['category'] in ('grid_named', 'lines_named') and p['text'].startswith(prefix)]
                additions.append({'id': label_id + '.' + name,
                    'status': 'pass' if any(p['visible'] for p in matches) else 'fail',
                    'expected': 'important line label is present and visible at least once',
                    'actual': {'attempted': len(matches), 'visible': sum(p['visible'] for p in matches),
                               'placements': matches}})
        if snapshot.get('layout_settled') is False:
            for result in additions[first_addition:]:
                if not result['id'].endswith('.coverage'):
                    result['status'] = 'not_run'
                    result['note'] = 'Bounded offscreen redraws ended before layout animation settled; observations remain in actual.'
    additions.extend([
        {'id': 'labels.visual-quality', 'status': 'not_run',
         'expected': 'text is legible on a real display at supported DPI and contrast settings',
         'actual': 'offscreen rectangles and drawing calls do not establish on-screen readability'},
        {'id': 'labels.line-association', 'status': 'not_run',
         'expected': 'each moved label remains visually associated with the correct line or point',
         'actual': 'anchors and leaders are recorded, but association requires further geometric or visual validation'},
    ])
    for draw in list(cases):
        if not draw['id'].startswith('labels.') or 'placement_trace' not in draw:
            continue
        points=[p for p in draw['placement_trace'] if p['category']=='point']
        groups=Counter((p['anchor_x'],p['anchor_y'],p['text']) for p in points)
        duplicates=[{'projection':list(k),'count':v} for k,v in groups.items() if v>1]
        additions.append({'id':draw['id']+'.coordinate-dedup','status':('fail' if duplicates else 'pass') if points else 'not_run',
            'expected':'one coordinate label per projection pixel and formatted value; selected points are visited first',
            'actual':{'placements':len(points),'duplicates':duplicates}})
    pans=sorted((c for c in cases if c['id'].startswith('labels.pan.profile.')),key=lambda c:int(c['id'].rsplit('.',1)[1]))
    previous={};jumps=[];samples=0
    for frame in pans:
        current={p['key']:p for p in frame.get('placement_trace',[]) if p['category']=='grid_named'}
        for key,item in current.items():
            if key in previous:
                samples+=1
                dx=abs(item['anchor_x']-previous[key]['anchor_x'])
                if dx>8 or item['anchor_y']!=previous[key]['anchor_y']:
                    jumps.append({'frame':frame['id'],'key':key,'delta_x':dx})
        previous=current
    if pans:
        additions.append({'id':'labels.pan.reference-anchor-continuity','status':('fail' if jumps else 'pass') if samples else 'not_run','expected':'reference anchors follow the 8-pixel pan without switching line endpoints','actual':{'samples':samples,'jumps':jumps}})
    cases.extend(additions)


def analyze_text_visibility(snapshots):
    """Conservative geometry/measurement evidence, never a visual pass verdict."""
    observations, candidates = [], []
    for snapshot in snapshots:
        for node in snapshot.get('nodes', []):
            if node.get('kind') != 'text_control':
                continue
            item = dict(node, state=snapshot.get('state'), language=snapshot.get('language'))
            b, c = item.get('bounds', {}), item.get('ancestor_clip', {})
            width, height = b.get('width', 0), b.get('height', 0)
            cw = max(0, c.get('right', 0) - c.get('left', 0))
            ch = max(0, c.get('bottom', 0) - c.get('top', 0))
            reasons = []
            if item.get('visible_excluding_hidden_form') and item.get('text'):
                if cw < width or ch < height:
                    reasons.append('control_bounds_clipped_by_ancestor')
                if width > 0 and item.get('measured_width', 0) > width:
                    reasons.append('text_width_exceeds_control_width')
                if height > 0 and item.get('measured_height', 0) > height:
                    reasons.append('text_height_exceeds_control_height')
            item['candidate_reasons'] = reasons
            observations.append(item)
            if reasons:
                candidates.append(item)
    cases = [{'id':'text_visibility.candidates', 'status':'review' if candidates else 'not_run',
              'expected':'collect possible text clipping for AI review',
              'actual':{'observations':len(observations),'candidates':len(candidates)},
              'note':'Potential clipping only; raw control bounds are not actual text client bounds, and absence of candidates is not visual acceptance.'},
             {'id':'text_visibility.native-and-dynamic', 'status':'not_run',
              'expected':'native pixels, actual menu/dropdown item geometry, dynamic options and all languages',
              'actual':'not measured by the hidden component snapshot'}]
    return {'observations':observations,'candidates':candidates,
            'limitations':['Canvas font measurements are estimates; native padding, icons, ellipsis, wrapping, font fallback and glyph pixels are not observed.',
                           'Only constructed components and sampled states/languages are covered; menus and dynamic option geometry are unavailable.',
                           'Ancestor clipping is estimated from hidden control geometry, not real on-screen rendering.']}, cases


def analyze_panel(cases: list[dict], native: dict, evidence: Path):
    panel = next((c for c in cases if c.get('id') == 'panel.image-review'), None)
    if panel:
        try:
            from PIL import Image
            with Image.open(evidence / 'panel.bmp') as image:
                rgb = image.convert('RGB')
                extrema = rgb.getextrema()
            if all(low == high for low, high in extrema):
                panel['actual'] = 'offscreen panel is uniform; native control rendering unavailable'
            else:
                panel['actual'] = 'native widgets and LCL graphic labels rendered separately; full layout and legibility not assessed'
        except (ImportError, OSError) as exc:
            panel['actual'] = f'panel image not evaluated: {exc}'
    data = native.get('panel', {})
    controls = data.get('controls', [])
    width, height = data.get('client_width', 0), data.get('client_height', 0)
    leaves = [c for c in controls if c.get('visible') and c.get('leaf')]
    if not leaves or width <= 0 or height <= 0:
        for name in ('panel.layout-bounds', 'panel.layout-overlaps', 'panel.ancestor-clipping'):
            cases.append({'id': name, 'status': 'not_run', 'expected': 'panel geometry',
                          'actual': 'control geometry unavailable'})
        return
    clipped = [c for c in leaves if c['x'] < 0 or c['y'] < 0 or
               c['x'] + c['width'] > width or c['y'] + c['height'] > height]
    collisions = []
    for i, a in enumerate(leaves):
        for b in leaves[i + 1:]:
            if rectangles_overlap(a, b):
                collisions.append([a['name'], b['name']])
    cases.append({'id': 'panel.layout-bounds', 'status': 'review' if clipped else 'pass',
                  'expected': 'visible controls inside panel client area', 'actual': clipped})
    cases.append({'id': 'panel.layout-overlaps', 'status': 'review' if collisions else 'pass',
                  'expected': 'visible leaf controls do not overlap', 'actual': collisions,
                  'note': 'Uses ancestor-clipped control bounds when available; overlapping bounds still require review.'})
    clip_available = all('clip_rect' in c for c in leaves)
    ancestor_clipped = [c for c in leaves if c.get('clip_rect') and
                        (drawn_bounds(c)['width'] < c['width'] or drawn_bounds(c)['height'] < c['height'])]
    cases.append({'id':'panel.ancestor-clipping',
                  'status':('not_run' if not clip_available else 'review' if ancestor_clipped else 'pass'),
                  'expected':'inspect controls constrained by ancestor client bounds',
                  'actual':ancestor_clipped if clip_available else 'ancestor clip data unavailable',
                  'note':'Hidden-control geometry is diagnostic; actual native clipping and text readability require display review.'})
    try:
        from PIL import Image, ImageDraw
        image = Image.new('RGB', (width, height), '#f5f5f5')
        drawer = ImageDraw.Draw(image)
        for c in leaves:
            x, y, w, h = c['x'], c['y'], c['width'], c['height']
            drawer.rectangle((x, y, x + max(0, w-1), y + max(0, h-1)), outline='#305e95')
            # Diagnostic layout map, not an authentic widget rendering.
            drawer.text((x+2, y+2), c['name'][:18], fill='#202020')
        image.save(evidence / 'panel-layout-map.png')
        native['panel']['layout_map'] = 'panel-layout-map.png'
        # A reconstructed diagnostic map using extracted control data; this is
        # intentionally separate from the actual native-control bitmap.
        from PIL import ImageFont
        font_path = Path(os.environ.get('WINDIR', 'C:/Windows')) / 'Fonts' / 'msyh.ttc'
        font = ImageFont.truetype(str(font_path), 12) if font_path.is_file() else ImageFont.load_default()
        content = Image.new('RGB', (width, height), '#f5f5f5')
        for c in leaves:
            x, y, w, h = c['x'], c['y'], c['width'], c['height']
            if w <= 0 or h <= 0:
                continue
            tile = Image.new('RGB', (w, h), '#ffffff' if 'Edit' in c['class'] else '#f5f5f5')
            painter = ImageDraw.Draw(tile)
            if 'Edit' in c['class']:
                painter.rectangle((0, 0, w-1, h-1), outline='#999999')
            label = c.get('caption') or c.get('text') or ''
            if not label and 'value' in c:
                label = str(c['value'])
            painter.text((2, max(0, (h-14)//2)), str(label), font=font, fill='#161616')
            content.paste(tile, (x, y))
        content.save(evidence / 'panel-content-map.png')
        native['panel']['content_map'] = 'panel-content-map.png'
    except (ImportError, OSError, ValueError, UnicodeError):
        pass


def run_performance(exe, work, run_dir, env, args, output):
    """Time fresh processes; do not evict the system file cache or show dialogs."""
    unicode_dir = work / '测试-Корабль'
    unicode_dir.mkdir()
    unicode_fixture = unicode_dir / '测试-box.fbm'
    shutil.copy2(FIXTURE, unicode_fixture)
    models = [('box-ascii', work / 'fixture.fbm'), ('box-unicode', unicode_fixture)]
    original = args.model.resolve(strict=True) if args.model else None
    if original:
        if not original.is_file() or original.suffix.lower() not in ('.fbm', '.ftm'):
            raise RuntimeError('Performance model must be an existing .fbm or .ftm file')
        original_hash = digest_file(original)
        copied_model = unicode_dir / ('用户模型' + original.suffix.lower())
        shutil.copy2(original, copied_model)
        if digest_file(copied_model) != original_hash or digest_file(original) != original_hash:
            raise RuntimeError('The selected model changed during copying; timing rejected')
        models.append(('user-unicode', copied_model))
    summary = {'machine': {'platform': platform.platform(), 'machine': platform.machine(),
                           'processor': platform.processor(), 'logical_cpus': os.cpu_count()},
               'cache': 'first and repeated processes; OS cache was neither cleared nor measured',
               'timing': 'GetTickCount64 monotonic elapsed milliseconds inside instrumented process; process wall includes startup',
               'models': [], 'trials': []}
    cases = []
    logs = []
    for name, model in models:
        summary['models'].append({'name': name, 'sha256': digest_file(model), 'size_bytes': model.stat().st_size})
        for repetition in range(1, args.repetitions + 1):
            label = f'{name}.{repetition}'
            trial_dir = run_dir / f'perf-{label}'
            trial_dir.mkdir()
            appdata = trial_dir / 'appdata'; appdata.mkdir()
            localdata = trial_dir / 'localappdata'; localdata.mkdir()
            trial_env = env.copy()
            trial_env.update(FS_UI_PROBE_OUTPUT=str(trial_dir), FS_UI_PROBE_MODEL=str(model),
                             APPDATA=str(appdata), LOCALAPPDATA=str(localdata))
            start = time.perf_counter()
            process = subprocess.run([str(exe)], cwd=work, env=trial_env, capture_output=True,
                                     timeout=args.timeout, creationflags=FLAGS)
            wall = round((time.perf_counter() - start) * 1000, 1)
            logs.append(f'{label}: exit={process.returncode}, wall_ms={wall}\n'.encode('utf8') + process.stdout + b'\n' + process.stderr)
            probe = trial_dir / 'probe.json'
            if process.returncode or not probe.is_file():
                raise RuntimeError(f'Performance trial {label} failed; no valid report')
            native = json.loads(probe.read_text(encoding='utf-8-sig'))
            if native.get('error') or len(native.get('cases', [])) != 1:
                raise RuntimeError(f'Performance trial {label} failed: {native.get("error")}')
            case = native['cases'][0]
            case['id'] = 'performance.' + label + '.load'
            stages = native.get('timing_trace', [])
            required_stages = {'start', 'form_shown_handler', 'load_begin', 'file_read_begin',
                               'file_read_end', 'parse_begin', 'parse_end', 'load_file_returned',
                               'rebuild_returned', 'views_created', 'zoom_fit', 'menu_updated', 'load_returned'}
            if case['status'] == 'pass' and {s['stage'] for s in stages} != required_stages:
                case['status'] = 'error'
                case['actual'] += '; timing stages incomplete or duplicated'
            if len(stages) != len({s['stage'] for s in stages}):
                case['status'] = 'error'
            cases.append(case)
            summary['trials'].append({'model': name, 'repetition': repetition, 'wall_ms': wall, 'stages': stages})
    (output / 'runtime.log').write_bytes(b'\n'.join(logs))
    if original and digest_file(original) != original_hash:
        raise RuntimeError('The selected model changed during testing; timing rejected')
    cases.extend([
        {'id': 'performance.real-window', 'status': 'not_run',
         'expected': 'end-to-end open dialog and visible window response',
         'actual': 'hidden form, copied model, and direct load handler only'},
        {'id': 'performance.cold-cache', 'status': 'not_run',
         'expected': 'repeatable cold storage and warm cache timings',
         'actual': 'system cache is uncontrolled and was not evicted'},
        {'id': 'performance.threshold', 'status': 'review',
         'expected': 'performance assessed against an agreed baseline and hardware',
         'actual': 'timings are observations; no acceptance threshold was specified'},
    ])
    return {'cases': cases, 'performance': summary}


def run_models(exe, src, work, run_dir, env, args, output):
    cases, models, logs = [], [], []
    for name, relative, expected_hash in MODEL_FIXTURES:
        original = src / relative
        if not original.is_file() or digest_file(original) != expected_hash:
            raise RuntimeError(f'Fixed model fixture missing or changed: {relative}')
        trial = run_dir / ('model-' + name); trial.mkdir()
        copy = trial / 'model.fbm'; shutil.copy2(original, copy)
        if digest_file(copy) != expected_hash:
            raise RuntimeError(f'Model copy changed: {relative}')
        profile = trial / 'profile'; profile.mkdir()
        trial_env = env.copy()
        trial_env.update(FS_UI_PROBE_OUTPUT=str(trial), FS_UI_PROBE_MODEL=str(copy),
                         APPDATA=str(profile), LOCALAPPDATA=str(profile))
        process = subprocess.run([str(exe)], cwd=work, env=trial_env, capture_output=True,
                                 timeout=args.timeout, creationflags=FLAGS)
        logs.append(name.encode() + b'\n' + process.stdout + b'\n' + process.stderr)
        if process.returncode or not (trial / 'probe.json').is_file():
            raise RuntimeError(f'Model trial {name} failed without a report')
        native = json.loads((trial / 'probe.json').read_text(encoding='utf-8-sig'))
        if native.get('error'):
            load_failed = native.get('model_load_started') and not native.get('model_load_completed')
            cases.append({'id': name + '.model.loaded', 'status': 'fail' if load_failed else 'error',
                          'expected': 'fixed model loads and provides four views',
                          'actual': native['error']})
            models.append({'name': name, 'source': relative, 'sha256': expected_hash,
                           'error': native['error']})
            if digest_file(original) != expected_hash:
                raise RuntimeError(f'Fixed model changed during testing: {relative}')
            continue
        ids = {c.get('id') for c in native['cases']}
        view_prefix = 'model.dense.' if 'model.render-limit' in ids else 'model.view.'
        if not {'model.loaded', 'model.visual',
                *(view_prefix + v for v in ('bodyplan', 'profile', 'plan', 'perspective'))} <= ids:
            raise RuntimeError(f'Model trial {name} produced incomplete coverage')
        models.append({'name': name, 'source': relative, 'sha256': expected_hash,
                       'control_points': native.get('model_points'), 'layers': native.get('model_layers')})
        for c in native['cases']:
            c['id'] = name + '.' + c['id']
            if 'image' in c:
                image_name = name + '-' + c['image']
                shutil.copy2(trial / c['image'], output / image_name)
                c['image'] = image_name
                if '.model.dense.' in c['id']:
                    try:
                        from PIL import Image, ImageChops
                        with Image.open(output / image_name) as img:
                            rgb = img.convert('RGB')
                            nonblank = ImageChops.difference(rgb, Image.new('RGB', rgb.size, 'white')).getbbox() is not None
                        status = 'pass' if nonblank else 'fail'
                        actual = 'non-white pixels present' if nonblank else 'blank bitmap'
                    except (ImportError, OSError):
                        status, actual = 'not_run', 'Pillow unavailable or image unreadable'
                    cases.append({'id': c['id'] + '.nonblank', 'status': status,
                                  'expected': 'offscreen bitmap contains drawing', 'actual': actual})
            if 'placement_trace' in c:
                omitted = [p for p in c['placement_trace'] if p['category'] == 'point' and not p['visible']]
                draws = Counter((t['text'], t['x'], t['y']) for t in c['text_trace'])
                missing = []
                for p in c['placement_trace']:
                    if p['category'] == 'point' and p['visible']:
                        key = (p['text'], p['x'], p['y'])
                        if draws[key]: draws[key] -= 1
                        else: missing.append(p['point_index'])
                cases.append({'id': c['id'] + '.drawn', 'status': 'fail' if missing else 'pass',
                              'expected': 'visible point labels reach TextOut', 'actual': {'missing_points': missing}})
                cases.append({'id': c['id'] + '.omissions', 'status': 'review' if omitted else 'pass',
                              'expected': 'inspect omitted point labels',
                              'actual': {'count': len(omitted), 'reasons': dict(Counter(p.get('omission_reason', 'unknown') for p in omitted))}})
            cases.append(c)
        if digest_file(original) != expected_hash:
            raise RuntimeError(f'Fixed model changed during testing: {relative}')
    (output / 'runtime.log').write_bytes(b'\n'.join(logs))
    return {'cases': cases, 'models': models}


def issue_summary(result):
    """Compact index; detailed evidence stays in report.json and image files."""
    items = []
    images = {c['id']: c['image'] for c in result.get('cases', [])
              if c.get('id') and c.get('image')}
    for case in result.get('cases', []):
        if case.get('status') not in ('fail', 'error', 'review', 'not_run'):
            continue
        actual = json.dumps(case.get('actual'), ensure_ascii=False, default=str)
        expected = json.dumps(case.get('expected'), ensure_ascii=False, default=str)
        item = {'id': case.get('id'), 'status': case.get('status'),
                'expected': expected[:800], 'actual': actual[:1200]}
        image_name = case.get('image')
        if not image_name and case.get('id'):
            parents = [key for key in images if case['id'].startswith(key + '.')]
            if parents: image_name = images[max(parents, key=len)]
        if image_name: item['image'] = image_name
        items.append(item)
    return {'status': result.get('status'), 'source_sha256': result.get('source_sha256'),
            'source_unchanged_during_run': result.get('source_unchanged_during_run'),
            'selection': result.get('selection'), 'error': result.get('error'), 'issues': items,
            'evidence': 'Full case, traces, and images are in report.json and this report directory.'}


# Legacy interface-placement and wording expectations are observations, not
# automatic judgments of a design that can legitimately evolve.
LEGACY_DESIGN_IDS = {
    'toolbar.action-bindings', 'toolbar.main-menu-actions', 'toolbar.enabled-state',
    'toolbar.redo-shortcut-only', 'toolbar.wide-overflow', 'toolbar.narrow-overflow',
    'toolbar.hidden-layer-access', 'language.chinese-labels',
    'chinese.new', 'chinese.open', 'chinese.layers', 'chinese.control-net', 'chinese.point',
    'chinese.redo', 'chinese.deselect', 'chinese.edge', 'chinese.face', 'chinese.sections',
    'chinese.lines-plan', 'chinese.hydrostatics', 'chinese.hints',
}


def mark_design_observations(cases):
    for c in cases:
        name = c.get('id', '')
        placement_dependent = False
        placement_dependent |= name.startswith(('display.background-availability.',
            'display.empty-disabled.', 'display.populated-enabled.'))
        placement_dependent |= name.startswith('labels.') and name.endswith(('.baseline', '.design-waterline'))
        placement_dependent |= (name.startswith('labels.') and name.endswith(('.bounds', '.overlap')) and c.get('status') == 'fail')
        if (name in LEGACY_DESIGN_IDS or placement_dependent) and c.get('status') in ('pass', 'fail'):
            c['legacy_status'] = c['status']
            c['status'] = 'review'
            c['note'] = 'Legacy fixed interface, wording or layout expectation; AI should judge against current requirements and evidence.'
        elif name in {'toolbar.control-net-toggle', 'toolbar.point-mode-on'} and c.get('actual') == 'missing':
            c['legacy_status'] = c['status']; c['status'] = 'not_run'
            c['note'] = 'The previous button entry was not found; the behavior was not exercised.'


def inventory_diffs(snapshots):
    """Report observed changes; identities are component paths or menu index paths, not semantic verdicts."""
    states = {s['state']: s for s in snapshots}
    comparisons = [('no_model', 'model_loaded'), ('model_loaded', 'point_selected'),
                   ('model_loaded', 'russian_model'), ('model_loaded', 'chinese_model')]
    def index(snapshot):
        result, duplicates = {}, []
        for node in snapshot.get('nodes', []):
            key = (node.get('kind'), node.get('component_path') or
                   (node.get('container_id', '') + ':' + node.get('item_index_path', '')))
            if key in result:
                duplicates.append(key)
            else:
                result[key] = node
        return result, duplicates
    comparisons_out = []
    for before, after in comparisons:
        if before not in states or after not in states:
            continue
        left, left_dupes = index(states[before]); right, right_dupes = index(states[after])
        common = left.keys() & right.keys()
        changed = []
        for key in sorted(common):
            a, b = left[key], right[key]
            fields = {name: {'before': a.get(name), 'after': b.get(name)}
                      for name in sorted(a.keys() | b.keys()) if a.get(name) != b.get(name)}
            if fields: changed.append({'identity': key, 'fields': fields})
        comparisons_out.append({'before': before, 'after': after,
            'added': [right[k] for k in sorted(right.keys() - left.keys())],
            'removed': [left[k] for k in sorted(left.keys() - right.keys())],
            'changed': changed, 'duplicate_identities': left_dupes + right_dupes,
            'note': 'Menu index paths can shift when dynamic items are inserted; changes are observations, not failures.'})
    return comparisons_out


def dynamic_policy(src, policy_path=None):
    """Only explicitly reviewed callbacks in unchanged source files can run."""
    policy_path = policy_path or ROOT / 'dynamic_callbacks.json'
    config = json.loads(policy_path.read_text(encoding='utf8'))
    if config.get('schema') != 1:
        raise RuntimeError('Unsupported dynamic callback policy')
    def matches(rule):
        rel = Path(rule['source'])
        if rel.is_absolute() or '..' in rel.parts:
            raise RuntimeError('Invalid dynamic callback source path')
        path = src / rel
        return path.is_file() and digest_file(path) == rule['sha256']
    menu = config['menu_adapters']
    main = matches(menu['main_overflow'])
    hull = matches(menu['hull_popup'])
    layer = matches(menu['layer_dialog']) if 'layer_dialog' in menu else False
    dropdowns = []
    for rule in config.get('dropdown_adapters', []):
        name = rule['owner_class'] + '.' + rule['handler']
        if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*\.[A-Za-z_][A-Za-z0-9_]*', name):
            raise RuntimeError('Invalid dynamic dropdown callback identity')
        if matches(rule): dropdowns.append(name)
    return {'main_overflow_approved': main, 'hull_popup_approved': hull, 'layer_dialog_approved': layer,
            'dropdowns_approved': dropdowns, 'policy_sha256': digest_file(policy_path)}


def run_dynamic(exe, src, work, run_dir, env, args, output):
    policy = dynamic_policy(src, args.callback_policy)
    trial = run_dir / 'dynamic'; trial.mkdir()
    profile = trial / 'profile'; profile.mkdir()
    model = trial / 'model.fbm'; shutil.copy2(FIXTURE, model)
    if digest_file(model) != digest_file(FIXTURE):
        raise RuntimeError('Dynamic fixture copy differs')
    run_env = env.copy()
    run_env.update(FS_UI_PROBE_CASE='dynamic', FS_UI_PROBE_OUTPUT=str(trial),
                   FS_UI_PROBE_MODEL=str(model), APPDATA=str(profile), LOCALAPPDATA=str(profile),
                   FS_UI_DYNAMIC_MAIN='1' if policy['main_overflow_approved'] else '0',
                   FS_UI_DYNAMIC_HULL='1' if policy['hull_popup_approved'] else '0',
                   FS_UI_DYNAMIC_DROPDOWNS=';'.join(policy['dropdowns_approved']))
    process = subprocess.run([str(exe)], cwd=work, env=run_env, capture_output=True,
                             timeout=args.timeout, creationflags=FLAGS)
    (output / 'dynamic-runtime.log').write_bytes(process.stdout + b'\n' + process.stderr)
    probe = trial / 'probe.json'
    if process.returncode or not probe.is_file():
        raise RuntimeError(f'Dynamic probe failed ({process.returncode}) without report')
    result = json.loads(probe.read_text(encoding='utf-8-sig'))
    if result.get('error'):
        raise RuntimeError('Dynamic probe failed: ' + result['error'])
    events = result.get('dynamic_callbacks', [])
    failures = [e for e in events if e.get('status') == 'error']
    result['cases'].append({'id':'dynamic.callback-errors', 'status':'error' if failures else 'pass',
                            'expected':'approved callbacks run without exception',
                            'actual': {'callback_errors': failures}})
    return {'cases': result['cases'], 'callbacks': events, 'policy': policy,
            'scope': result.get('dynamic_scope')}


def run(args):
    INSTRUMENTATION_MATCHES.clear()
    src, laz = args.source.resolve(), args.lazarus.resolve()
    output = args.output.resolve()
    if output.is_relative_to(src):
        raise RuntimeError('The report directory must be outside the FreeShip source tree')
    output.mkdir(parents=True, exist_ok=False)
    result = {'schema': 1, 'status': 'error', 'source_dir': str(src),
              'cases': [],
              'selection': {'case': args.case, 'view': args.view, 'axis': args.axis,
                            'note': 'axis off is included as the image baseline for x/y/z'},
              'test_code_sha256': test_code_hashes(),
              'scope': 'instrumented desktop-source build; no desktop automation or real input',
              'limitations': ['Normal application binary is not the instrumented test binary',
                              'Normal application activation and event loop are not tested',
                              'Native mouse, focus, window placement and monitor DPI are not tested',
                              'Images are offscreen viewport renders, not full-window screenshots']}
    result['callback_policy_sha256'] = digest_file(args.callback_policy)
    try:
        if not (src / 'FreeShip.dpr').is_file() or not FIXTURE.is_file():
            raise RuntimeError('FreeShip source or the box fixture is missing')
        if not laz.is_dir():
            raise RuntimeError('Lazarus toolchain is missing')
        before, count = fingerprint(src)
        result.update(source_sha256=before, source_file_count=count, fixture_sha256=digest_file(FIXTURE))
        temp = Path(tempfile.gettempdir())
        tools = junction_for(laz, temp)
        with temporary_workspace('fs-ui-run-') as run_dir:
            work, evidence, profile = run_dir / 'source', run_dir / 'evidence', run_dir / 'profile'
            work.mkdir(); evidence.mkdir(); profile.mkdir()
            prepare(src, work, before)
            result['source_snapshot_sha256'] = before
            # The bundled FPC configuration can retain paths from before a workspace move.
            # Supply its runtime-unit directories explicitly to the isolated project only.
            project_path = work / 'FreeShip.lpi'
            project_tree = ET.parse(project_path)
            runtime_units = tools / 'fpc' / '3.2.2' / 'units' / 'x86_64-win64'
            for unit_paths in project_tree.iter('OtherUnitFiles'):
                value = unit_paths.get('Value', '')
                unit_paths.set('Value', value + ';' + ';'.join(str(p) for p in sorted(runtime_units.iterdir()) if p.is_dir() and p.name != 'fv'))
            for target in project_tree.findall('.//CompilerOptions/Target/Filename'):
                target.set('Value', 'FreeShip')
            project_tree.write(project_path, encoding='utf-8', xml_declaration=True)
            result['instrumented_source_sha256'] = fingerprint(work)[0]
            if test_code_hashes() != result['test_code_sha256']:
                raise RuntimeError('Test code changed during snapshot; no results accepted')
            if digest_file(work / 'fixture.fbm') != result['fixture_sha256']:
                raise RuntimeError('Fixture changed during copying; no results accepted')
            after_copy, _ = fingerprint(src)
            if after_copy != before:
                raise RuntimeError('Source changed during snapshot; no results accepted')
            env = os.environ.copy()
            env['FS_UI_PROBE_OUTPUT'] = str(evidence)
            env['FS_UI_PROBE_MODEL'] = str(work / 'fixture.fbm')
            env['FS_UI_PROBE_CASE'] = args.case
            env['FS_UI_PROBE_VIEW'] = args.view
            env['FS_UI_PROBE_AXIS'] = str(AXES[args.axis] if args.axis != 'all' else -1)
            policy = dynamic_policy(src, args.callback_policy)
            env['FS_UI_DYNAMIC_MAIN'] = '1' if policy['main_overflow_approved'] else '0'
            env['FS_UI_DYNAMIC_HULL'] = '1' if policy['hull_popup_approved'] else '0'
            env['FS_UI_LAYER_DIALOG'] = '1' if policy['layer_dialog_approved'] else '0'
            env['APPDATA'] = str(profile / 'appdata')
            env['LOCALAPPDATA'] = str(profile / 'localappdata')
            env['TEMP'] = str(run_dir); env['TMP'] = str(run_dir)
            Path(env['APPDATA']).mkdir(); Path(env['LOCALAPPDATA']).mkdir()
            compiler = tools / 'fpc' / '3.2.2' / 'bin' / 'x86_64-win64' / 'fpc.exe'
            result['compiler_sha256'] = digest_file(compiler)
            cmd = [str(tools / 'lazbuild.exe'), f'--lazarusdir={tools}', f'--compiler={compiler}',
                   f'--pcp={run_dir / "laz-config"}', '--build-mode=Win32 Debug',
                   '--ws=win32', '--cpu=x86_64', '--os=win64', str(work / 'FreeShip.lpi')]
            build = subprocess.run(cmd, cwd=work, env=env, capture_output=True,
                                   timeout=args.timeout, creationflags=FLAGS)
            (output / 'build.log').write_bytes(build.stdout + b'\n' + build.stderr)
            if build.returncode:
                lines = (build.stdout + b'\n' + build.stderr).decode('utf8', errors='replace').splitlines()
                result['build_errors'] = [line for line in lines if re.search(r'\b(?:Error|Fatal):', line)][:20]
                if fingerprint(src)[0] != before:
                    raise RuntimeError('Source changed during build; compiler errors apply only to the snapshot')
                raise RuntimeError(f'Build failed ({build.returncode}); see build.log')
            exe = work / 'FreeShip.exe'
            if not exe.is_file():
                raise RuntimeError('Build did not produce the expected test executable')
            result['instrumented_exe_sha256'] = digest_file(exe)
            dynamic_result = None
            if args.case in ('all', 'inventory', 'dynamic', 'toolbar'):
                dynamic_result = run_dynamic(exe, src, work, run_dir, env, args, output)
                result['dynamic_inventory'] = dynamic_result
            if args.case == 'dynamic':
                native = {'cases': dynamic_result['cases']}
                process_exit_code = 0
            elif args.case == 'models':
                native = run_models(exe, src, work, run_dir, env, args, output)
                process_exit_code = 0
                result['models'] = native['models']
            elif args.case == 'performance':
                native = run_performance(exe, work, run_dir, env, args, output)
                process_exit_code = 0
                result['performance'] = native['performance']
            else:
                process = subprocess.run([str(exe)], cwd=work, env=env, capture_output=True,
                                         timeout=args.timeout, creationflags=FLAGS)
                (output / 'runtime.log').write_bytes(process.stdout + b'\n' + process.stderr)
                probe = evidence / 'probe.json'
                if not probe.is_file():
                    raise RuntimeError(f'Test process exited {process.returncode} without a report')
                native = json.loads(probe.read_text(encoding='utf-8-sig'))
                process_exit_code = process.returncode
            result['cases'] = native.get('cases', [])
            if args.case == 'language':
                result['language_popup_evidence'] = native.get('language_popup_evidence', [])
                result['selected_details_evidence'] = native.get('selected_details_evidence', [])
            if dynamic_result and args.case != 'dynamic':
                result['cases'].extend(dynamic_result['cases'])
            if args.case in ('all', 'inventory', 'toolbar', 'chinese', 'language'):
                result['inventory'] = native.get('inventory', [])
                result['inventory_diffs'] = inventory_diffs(result['inventory'])
                result['text_visibility'], text_cases = analyze_text_visibility(result['inventory'])
                result['text_visibility']['language_attempts'] = native.get('text_language_attempts', [])
                result['cases'].extend(text_cases)
            views = VIEWS if args.view == 'all' else (args.view,)
            axes = (0, 1, 2, 3) if args.axis == 'all' else tuple(sorted({0, AXES[args.axis]}))
            if args.case in ('all', 'coordinates'):
                analyze_draws(result['cases'], evidence, views, axes)
            if args.case in ('all', 'labels'):
                analyze_labels(result['cases'])
            if args.case in ('all', 'panel'):
                analyze_panel(result['cases'], native, evidence)
                result['panel'] = native.get('panel')
            if args.case in ('all', 'layers'):
                result['layer_discovery'] = native.get('layer_discovery')
            if args.case in ('all', 'toolbar'):
                result['toolbar'] = native.get('toolbar')
            mark_design_observations(result['cases'])
            result['native_error'] = native.get('error')
            result['process_exit_code'] = process_exit_code
            for pattern in ('*.bmp', '*.png'):
                for image in evidence.glob(pattern):
                    shutil.copy2(image, output / image.name)
            statuses = [c.get('status') for c in result['cases']]
            ids = [c.get('id') for c in result['cases']]
            required = set() if args.case in ('performance', 'models', 'dynamic') else {'model.fixture', 'model.fixture.point5',
                                                                            'model.preview-transition', 'model.preview-real-timer'}
            if args.case in ('all', 'inventory', 'dynamic', 'toolbar'):
                required.update({'dynamic.synthetic-combo', 'dynamic.synthetic-deny',
                                 'dynamic.coverage', 'dynamic.callback-errors'})
            if args.case == 'models':
                required.update(f'{name}.model.loaded' for name, _, _ in MODEL_FIXTURES)
            if args.case in ('all', 'inventory'):
                required.add('inventory.coverage')
                states = {s.get('state') for s in result['inventory']}
                if not {'no_model', 'model_loaded', 'point_selected'} <= states:
                    result['error'] = 'Required inventory snapshots are missing'
            if args.case == 'performance':
                required.update({'performance.real-window', 'performance.cold-cache', 'performance.threshold'})
                names = ['box-ascii', 'box-unicode'] + (['user-unicode'] if args.model else [])
                required.update(f'performance.{name}.{i}.load' for name in names for i in range(1, args.repetitions + 1))
            if args.case in ('all', 'persistence'):
                required.update({'persistence.isolated-target', 'persistence.save-action',
                                 'persistence.reopen-geometry', 'persistence.reopen-layer-color',
                                 'persistence.editor'})
            if args.case in ('all', 'history'):
                required.update({'history.undo-enabled', 'history.undo', 'history.redo-enabled', 'history.redo'})
            if args.case in ('all', 'shortcuts'):
                required.update({'shortcuts.no-model-state', 'shortcuts.no-model', 'shortcuts.with-model-state',
                    'shortcuts.main', 'shortcuts.disabled', 'shortcuts.same-languages', 'shortcuts.editor', 'shortcuts.editor-preserves-draft',
                    'shortcuts.lcl-message.main', 'shortcuts.physical-input', 'shortcuts.modal'})
                for view in ('perspective', 'profile', 'plan', 'bodyplan'):
                    required.update({f'shortcuts.view.{view}', f'shortcuts.lcl-message.{view}',
                                     f'shortcuts.select-all.{view}', f'shortcuts.view-type.{view}'})
            if args.case in ('all', 'labels'):
                required.update({'labels.visual-quality', 'labels.line-association'})
                for language in ('chinese', 'russian'):
                    for width in (800, 480):
                        for view in ('profile', 'plan', 'bodyplan'):
                            base = f'labels.hull.{language}.{view}.{width}'
                            required.add(base)
                            required.update(f'{base}.{suffix}' for suffix in ('coverage', 'drawn', 'bounds', 'overlap', 'omissions', 'line-crossings'))
                            if view != 'plan':
                                required.update({f'{base}.baseline', f'{base}.design-waterline'})
                        base = f'labels.linesplan.{language}.{width}'
                        required.add(base)
                        required.update(f'{base}.{suffix}' for suffix in ('coverage', 'drawn', 'bounds', 'overlap', 'omissions', 'line-crossings', 'baseline', 'design-waterline'))
            if args.case in ('all', 'display'):
                required.update({'display.other-view-unchanged', 'display.point-enabled', 'display.point-disabled',
                                 'display.deselect-disabled', 'display.deselect-enabled', 'display.deselect-action',
                                 'display.actual-render-modes'})
                for view in ('profile','plan','bodyplan','perspective'):
                    required.update({f'display.view.{view}', f'display.background-availability.{view}'})
                for mode in ('wireframe','shade','gaussian','developable','zebra'):
                    required.add(f'display.mode.{mode}')
                for name in ('stations','waterlines','buttocks','diagonals','grid'):
                    required.update({f'display.empty-disabled.{name}', f'display.populated-enabled.{name}',
                                     f'display.toggle.{name}', f'display.restore.{name}'})
            if args.case in ('all', 'panel'):
                required.update({'panel.bound-point', 'panel.coordinates', 'panel.switch-point',
                                 'panel.clear-binding', 'panel.image-review', 'panel.layout-bounds',
                                 'panel.layout-overlaps', 'panel.ancestor-clipping', 'desktop.popup-visibility', 'desktop.window-layout'})
            if args.case in ('all', 'toolbar'):
                required.update({'toolbar.redo-physical-shortcut', 'toolbar.point-button-state',
                    'toolbar.control-net-toggle', 'toolbar.control-net-state', 'toolbar.control-net-restore',
                    'toolbar.point-mode-on', 'toolbar.point-mode-off', 'toolbar.actual-layout'})
            if args.case in ('all', 'chinese'):
                required.update({'chinese.can-select', 'chinese.language', 'chinese.full-coverage'})
            if args.case in ('all', 'language'):
                required.update({'language.select-russian',
                    'language.russian-preserves-input', 'language.russian-preserves-model',
                    'language.select-chinese',
                    'language.chinese-preserves-input', 'language.real-focused-input'})
            if args.case in ('all', 'layers'):
                required.update({'layers.rows', 'layers.hide-surface', 'layers.refill-state',
                    'layers.restore-surface', 'layers.hide-control-net', 'layers.restore-control-net',
                    'layers.other-surface-unchanged', 'layers.other-net-unchanged',
                    'layers.active-selection', 'layers.actual-layout', 'layers.bulk.hide-surfaces',
                    'layers.bulk.surface-keeps-nets', 'layers.bulk.show-surfaces', 'layers.bulk.hide-nets',
                    'layers.bulk.net-keeps-surfaces', 'layers.bulk.show-nets', 'layers.sorted-row-target'})
            if args.case in ('all', 'coordinates'):
                required.update({'coordinates.independent-views', 'coordinates.visibility.control-net-off',
                                 'coordinates.visibility.layer-net-off', 'coordinates.visibility.selected-hidden-layer'})
                for view in views:
                    for i in axes:
                        prefix = f'view.{view}.'
                        required.update({f'{prefix}menu.axis.{i}', f'{prefix}menu.checked.{i}',
                                         f'{prefix}draw.label.{i}', f'{prefix}draw.all-labels.{i}',
                                         f'{prefix}layout.clipped-labels.{i}', f'{prefix}layout.overlapping-labels.{i}'})
                        if i: required.add(f'{prefix}draw.changed-pixels.{i}')
            if (len(ids) != len(set(ids)) or not required.issubset(ids) or
                    any(s not in {'pass', 'fail', 'not_run', 'review', 'error'} for s in statuses)):
                result['error'] = 'Incomplete or malformed case results'
            result['status'] = ('error' if process_exit_code or native.get('error') or result.get('error')
                                else classify_statuses(statuses))
            result['source_unchanged_during_run'] = fingerprint(src)[0] == before
            if not result['source_unchanged_during_run']:
                result['status'] = 'error'
                result['error'] = 'Source changed during test; results do not describe the current tree'
            if (test_code_hashes() != result['test_code_sha256'] or digest_file(FIXTURE) != result['fixture_sha256']
                    or digest_file(args.callback_policy) != result['callback_policy_sha256']):
                result['status'] = 'error'
                result['error'] = 'Test code or fixture changed during the test; results rejected'
    except (Exception, subprocess.TimeoutExpired) as exc:
        result['error'] = f'{type(exc).__name__}: {exc}'
    result['instrumentation_matches'] = list(INSTRUMENTATION_MATCHES)
    (output / 'report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
    if 'inventory' in result:
        (output / 'inventory.json').write_text(json.dumps({
            'status': result['status'], 'source_sha256': result.get('source_sha256'),
            'source_unchanged_during_run': result.get('source_unchanged_during_run'),
            'snapshots': result['inventory'], 'state_differences': result.get('inventory_diffs', [])},
            ensure_ascii=False, indent=2), encoding='utf8')
    if 'text_visibility' in result:
        (output / 'text_visibility.json').write_text(json.dumps({
            'status': result['status'], 'source_sha256': result.get('source_sha256'),
            'source_unchanged_during_run': result.get('source_unchanged_during_run'),
            **result['text_visibility']}, ensure_ascii=False, indent=2), encoding='utf8')
    if 'dynamic_inventory' in result:
        (output / 'dynamic_inventory.json').write_text(json.dumps({
            'status': result['status'], 'source_sha256': result.get('source_sha256'),
            'source_unchanged_during_run': result.get('source_unchanged_during_run'),
            **result['dynamic_inventory']}, ensure_ascii=False, indent=2), encoding='utf8')
    summary = issue_summary(result)
    (output / 'issues.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps({'status': result['status'], 'report': str(output / 'report.json'),
                      'cases': len(result['cases']), 'error': result.get('error')}, ensure_ascii=False))
    return {'pass': 0, 'fail': 1, 'partial': 2, 'error': 3}[result['status']]


def main():
    # The lines-plan probe uses a dedicated console driver and the same isolation/report conventions.
    if '--case' in sys.argv:
        position = sys.argv.index('--case')
        if position + 1 < len(sys.argv) and sys.argv[position + 1] == 'linesplan':
            import runpy
            forwarded = sys.argv[1:]
            del forwarded[position - 1:position + 1]
            sys.argv = [str(ROOT / 'linesplan_performance.py'), *forwarded]
            runpy.run_path(str(ROOT / 'linesplan_performance.py'), run_name='__main__')
            return
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, default=DEFAULT_SOURCE)
    parser.add_argument('--lazarus', type=Path, default=DEFAULT_LAZARUS)
    parser.add_argument('--output', type=Path, default=ROOT / 'reports' / time.strftime('%Y%m%d-%H%M%S'))
    parser.add_argument('--timeout', type=int, default=180)
    parser.add_argument('--case', choices=('all', 'inventory', 'dynamic', 'coordinates', 'panel', 'toolbar', 'layers', 'chinese', 'display', 'language', 'shortcuts', 'labels', 'linesplan', 'performance', 'persistence', 'models', 'history'), default='all',
                        help='Select which tests to execute')
    parser.add_argument('--model', type=Path, help='Optional FBM/FTM model to copy for performance tests')
    parser.add_argument('--callback-policy', type=Path, default=ROOT / 'dynamic_callbacks.json',
                        help='Reviewed, hash-pinned callbacks allowed in the isolated process')
    parser.add_argument('--repetitions', type=int, default=3, help='Fresh process trials per model (1-10)')
    parser.add_argument('--view', choices=('all', *VIEWS), default='all',
                        help='Restrict coordinate tests to a view')
    parser.add_argument('--axis', choices=('all', *AXES), default='all',
                        help='Restrict coordinate tests to an axis; off is included as baseline')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This pilot currently supports Windows only')
    if args.case not in ('all', 'coordinates') and (args.view != 'all' or args.axis != 'all'):
        parser.error('--view and --axis only apply to coordinate tests')
    if args.model and args.case != 'performance':
        parser.error('--model only applies to performance tests')
    if not 1 <= args.repetitions <= 10:
        parser.error('--repetitions must be between 1 and 10')
    return run(args)


if __name__ == '__main__':
    sys.exit(main())
