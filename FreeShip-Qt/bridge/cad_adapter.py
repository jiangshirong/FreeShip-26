"""Extract the existing NURBS conversion, without its dialogs or IGES writer.

Only the disposable build copy is read/changed. The numerical patch construction
remains the Plus algorithm; precision-preserving file serialization is in C++.
"""
import hashlib
import shutil


def adapt(work, bridge, replace):
    source = work/'Units/FreeEdit.inc'
    if hashlib.sha256(source.read_bytes()).hexdigest() != '09592b0547071baf0f1c511607b15612e5d1e70c50dd3548b77639c712b62908':
        raise RuntimeError('Re-review CAD extraction against changed FreeEdit.inc')
    s = source.read_text('utf-8-sig')
    s = s[s.index('procedure TFreeEdit.File_ExportIGES;'):s.index('end;{TFreeEdit.File_ExportIGES}')]
    helpers = s[s.index('  function FindFourtPoint'):s.index('  procedure AssembleTriangle')]
    process = s[s.index('  procedure ProcessGrid'):s.index('  end;{ProcessGrid}')+len('  end;{ProcessGrid}')]
    process = replace(process, 'NURB, NURB2: TFreeNURBsurface;', 'NURB: TFreeNURBsurface;')
    process = replace(process, '      Setlength(BottomRow, Cols);', '      Checkpoint;\n      Setlength(BottomRow, Cols);')
    process = replace(process, '      NURB.SetCapacity(Cols + 2, Rows + 2);', '      try\n      NURB.SetCapacity(Cols + 2, Rows + 2);')
    start = process.index('      IGESList.Add_Entity_128(NURB, ColorInd);')
    end = process.index('      FreeAndNil(NURB);', start) + len('      FreeAndNil(NURB);')
    process = process[:start] + '      AddPatch(NURB, ColorInd);\n      finally NURB.Free end;' + process[end:]
    (work/'Units/CadNurbs.inc').write_text(helpers+process, encoding='utf8')
    shutil.copy2(bridge/'CadExportEngine.pas',work/'Units/CadExportEngine.pas')
