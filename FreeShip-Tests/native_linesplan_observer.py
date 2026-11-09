"""Extract actual active Pascal projection routines; never translate their math."""
import hashlib
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parent


def prepare(source,work,out):
    original=(source/'Forms/FreeLinesplanFrme.pas').read_bytes()
    # The earlier DrawSpline is under {$ifdef NOUSE}; the active routine follows.
    anchor=b'  procedure DrawSpline(Spline: TFreeSpline; Views: TLinesplanViews; Style: TPenStyle);'
    if original.count(anchor)!=2:raise RuntimeError('DrawSpline declarations changed')
    start=original.rindex(anchor);stop=original.index(b'  end;{DrawSpline}',start)+len(b'  end;{DrawSpline}')
    draw=original[start:stop]
    start=original.index(b'  procedure AddSpline(Spline: TFreeSpline; Views: TLinesplanViews;')
    stop=original.index(b'  end;{AddSpline}',start)+len(b'  end;{AddSpline}')
    dxf=original[start:stop];observed=dxf
    newline=b'\r\n' if b'\r\n' in dxf else b'\n'
    insertions=[]
    for anchor,callback in [(b"Strings.Add('0' + EOL + 'POLYLINE');",b'CaptureDXFStart;'),
                            (b"Strings.Add('0' + EOL + 'VERTEX');",b'CaptureDXFPoint(P);')]:
        if observed.count(anchor)!=2:raise RuntimeError('DXF capture anchors changed')
        insertion=callback+newline+b'    '
        observed=observed.replace(anchor,insertion+anchor);insertions.append(insertion)
    restored=observed
    for insertion in insertions:restored=restored.replace(insertion,b'')
    if restored!=dxf:raise RuntimeError('Removing observers did not restore original AddSpline')
    for name,data in [('LegacyLinesplanDraw.inc',draw),('LegacyLinesplanDXF.inc',observed),('OriginalLinesplanDXF.inc',dxf)]:
        (out/name).write_bytes(data)
        if not name.startswith('Original'):(work/'Tests'/name).write_bytes(data)
    shutil.copy2(ROOT/'NativeLinesplanProbe.inc',work/'Tests/NativeLinesplanProbe.inc')
    return {'source':'Forms/FreeLinesplanFrme.pas','source_sha256':hashlib.sha256(original).hexdigest(),
        'active_draw_sha256':hashlib.sha256(draw).hexdigest(),'original_dxf_sha256':hashlib.sha256(dxf).hexdigest(),
        'instrumented_dxf_sha256':hashlib.sha256(observed).hexdigest(),
        'draw_statement_changes':0,'dxf_statement_changes':0,'dxf_readonly_callbacks':4,'stripped_equals_original':True,
        'scope':'Actual DrawSpline/AddSpline model coordinates; no pixel projection, complete scene, or complete DXF file comparison.'}
