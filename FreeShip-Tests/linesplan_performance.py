"""Profile an isolated source copy. No desktop input or production configuration."""
from pathlib import Path
import sys, subprocess, tempfile, shutil, hashlib, json
import xml.etree.ElementTree as E
import argparse, os, time, re, statistics
from collections import Counter
import freeship_ui_test as common
ROOT=Path(__file__).resolve().parent
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
p.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS)
p.add_argument('--output',type=Path,default=ROOT/'reports'/('linesplan-'+time.strftime('%Y%m%d-%H%M%S')))
p.add_argument('--model',type=Path,action='append')
p.add_argument('--reference-frame',type=Path)
p.add_argument('--fill-reference',type=Path,help='Observe filled silhouette differences against a correct raster report; review only, because edge rasterizers differ')
p.add_argument('--physical-dpi-negative-control',action='store_true',help='In the isolated copy, use incorrect logical-DPI EMF conversion')
p.add_argument('--compare',type=Path,help='Reference output directory from this tool')
p.add_argument('--event-pause-ms',type=int,default=0)
p.add_argument('--trace-linesplan',action='store_true')
p.add_argument('--erase-negative-control',action='store_true',help='In the isolated copy only, restore the old no-op background erase to verify pixel checks fail')
p.add_argument('--host-erase-negative-control',action='store_true',help='In the isolated copy, restore solid host background erasure')
p.add_argument('--native-commit-negative-control',action='store_true',help='In the isolated copy, allow drawing before native geometry commit')
p.add_argument('--real-label-clock',action='store_true')
p.add_argument('--width',type=int,default=1200)
p.add_argument('--height',type=int,default=760)
p.add_argument('--pan-steps',type=int,default=12)
p.add_argument('--initial-zoom',type=float,default=1.0)
p.add_argument('--timeout',type=int,default=180)
args=p.parse_args();repo=args.source.resolve();out=args.output.resolve()
if out.exists() or out==repo or repo in out.parents:raise SystemExit('Output must be new and outside source')
out.mkdir(parents=True)
tools=common.junction_for(args.lazarus.resolve(),Path(tempfile.gettempdir()))
before,count=common.fingerprint(repo)
models=[m.resolve() for m in (args.model or [common.FIXTURE])]
cases=[]
report={'label_clock':'real' if args.real_label_clock else 'fixed', 'status':'error','source_sha256':before,'source_file_count':count,'cases':cases,
 'test_code_sha256':{n:common.digest_file(ROOT/n) for n in ['linesplan_performance.py','LinesplanPerformance.pas','dxf_geometry.py','LineplanTextMetrics.pas']},
 'limitations':['Hidden viewport at fixed dimensions; no native input or full-window latency measurement',
 'Printing uses the printing branch on a bitmap canvas, not a physical printer',
 'Timings use millisecond ticks; no cross-machine performance threshold',
 'Label frame clock is fixed in the disposable copy for deterministic fully revealed text',
 'DXF entity order is observed separately; numerical records must match exactly'], 'models':[], 'viewport':{'width':args.width,'height':args.height,'initial_zoom':args.initial_zoom,'pan_steps':args.pan_steps}}
from dxf_geometry import records
try:
 with common.temporary_workspace('fs-lines-') as td:
  work=Path(td)
  for name in common.FOLDERS:
   shutil.copytree(repo/name,work/name,ignore=shutil.ignore_patterns('*.exe','*.log','*.bmp','linesplan-performance','*.o','*.ppu'))
  (work/'Tests').mkdir(); shutil.copy2(ROOT/'LinesplanPerformance.pas',work/'Tests/LinesplanPerformance.pas')
  for pat in ['*.inc','*.res','FreeShip.lpi','FreeShip.dpr']:
   for f in repo.glob(pat):shutil.copy2(f,work/f.name)
  shutil.copy2(ROOT/'LineplanTextMetrics.pas',work/'Units/LineplanTextMetrics.pas')
  viewport=work/'Units/FreeViewport.inc'
  vs=viewport.read_text(encoding='utf-8-sig')
  text_anchor='procedure TFreeViewport.TextOutLabel(const Placement: TLabelPlacement; const Value: string);\nbegin'
  if vs.count(text_anchor)!=1:raise RuntimeError('Text drawing adapter mismatch')
  vs=vs.replace(text_anchor,text_anchor+'\n  LineplanTextMetrics.RecordText(FDrawingCanvas.Font.Size,Screen.Cursor=crHourglass);')
  va=vs.index('procedure TFreeViewport.SaveAsBitmap('); vb=vs.index('end;{TFreeViewport.SaveAsBitmap}',va)
  vp=vs[va:vb].replace('      Refresh;', '      Paint;').replace('self.Canvas, Rect(0,0,CanvasWidth,CanvasHeight)', 'FDrawingBuffer.Canvas, Rect(0,0,CanvasWidth,CanvasHeight)')
  vs=vs[:va]+vp+vs[vb:]
  for method in ['MouseDown','MouseUp']:
   ma=vs.index('procedure TFreeViewport.'+method+'(')
   mb=vs.index('end;{TFreeViewport.'+method+'}',ma)
   part=vs[ma:mb]
   if part.count('if not focused then')!=1:raise RuntimeError('Hidden focus adapter mismatch')
   part=part.replace('if not focused then','if False then // Native focus is not tested on a hidden form')
   vs=vs[:ma]+part+vs[mb:]
  viewport.write_text(vs,encoding='utf-8')
  layout=work/'Units/FreeLabelLayout.pas'
  ls=layout.read_text(encoding='utf-8-sig')
  if ls.count('  FLastFrame:=GetTickCount64;')!=1:raise RuntimeError('Label clock adapter mismatch')
  if not args.real_label_clock:ls=ls.replace('  FLastFrame:=GetTickCount64;', '  FLastFrame:=GetTickCount64;\n  FFrameStep:=1000; // Deterministic fully revealed text for image comparison.')
  layout.write_text(ls,encoding='utf-8')
  geometry=work/'Units/FreeGeometry.pas'
  gs=geometry.read_text(encoding='utf-8-sig').replace('interface\n\nuses','interface\n\nuses\n  LineplanTextMetrics,',1)
  gs=gs.replace('property Printing: boolean read FGetPrinting;', 'property Printing: boolean read FPrinting write FPrinting;')
  geometry.write_text(gs,encoding='utf-8')
  if args.physical_dpi_negative_control:
   cache=work/'Units/FreeVectorFillCache.pas';cs=cache.read_text(encoding='utf-8-sig')
   old='R:=Types.Rect(0,0,Round(PW*(MX*100.0)/DX),Round(PH*(MY*100.0)/DY));'
   if cs.count(old)!=1:raise RuntimeError('Physical-DPI negative-control adapter changed')
   cs=cs.replace(old,'R:=Types.Rect(0,0,Round(PW*2540.0/GetDeviceCaps(HDC(Reference.Handle),LOGPIXELSX)),Round(PH*2540.0/GetDeviceCaps(HDC(Reference.Handle),LOGPIXELSY)));')
   cache.write_text(cs,encoding='utf-8');report['negative_control']='incorrect logical DPI instead of physical EMF device units'
  original=(repo/'Forms/FreeLinesplanFrme.pas').read_bytes()
  if args.reference_frame:
   original=args.reference_frame.read_bytes()
  if args.erase_negative_control:
   text=original.decode('utf-8-sig');old='Viewport := TLinesplanViewport.Create(Self);'
   if text.count(old)!=1:raise RuntimeError('erase negative-control adapter changed')
   original=text.replace(old,'Viewport := TFreeViewport.Create(Self);').encode('utf-8')
   report['negative_control']='restored old no-op viewport erase in isolated copy'
  if args.host_erase_negative_control:
   original=original.decode('utf-8-sig')
   a=original.index('procedure TFreeLinesplanFrame.EraseBackground(');b=original.index('procedure TFreeLinesplanFrame.TraceEvent(',a)
   original=(original[:a]+'procedure TFreeLinesplanFrame.EraseBackground(DC:HDC);\nbegin inherited EraseBackground(DC) end;\n\n'+original[b:]).encode('utf-8')
   form=work/'Forms/FreeLinesplanFrm.pas';fs=form.read_text(encoding='utf-8-sig')
   a=fs.index('procedure TFreeLinesplanForm.EraseBackground(');b=fs.index('constructor TFreeLinesplanForm.Create(',a)
   form.write_text(fs[:a]+'procedure TFreeLinesplanForm.EraseBackground(DC:HDC);\nbegin inherited EraseBackground(DC) end;\n\n'+fs[b:],encoding='utf-8')
   report['negative_control']='restored solid host background erase in isolated copy'
  if args.native_commit_negative_control:
   original=original.decode('utf-8-sig')
   a=original.index('function LinesplanNativeLayoutReady(',original.index('implementation'))
   b=original.index('procedure TLinesplanViewport.WndProc(',a)
   original=(original[:a]+'function LinesplanNativeLayoutReady(Control:TWinControl;DC:HDC):Boolean;\nbegin Result:=True end;\n\n'+original[b:]).encode('utf-8')
   report['negative_control']='allow premature native-window presentation in isolated copy'
  (out/'frame-source.pas').write_bytes(original)
  s=original.decode('utf-8-sig').replace('  FreeStringUtils, FreeLabelLayout', '  LineplanTextMetrics, FreeStringUtils, FreeLabelLayout',1)
  if args.reference_frame:
   if 'procedure InvalidateRenderCache;' not in s:
    s=s.replace('    procedure UpdateMenu;', '    procedure InvalidateRenderCache;\n    procedure UpdateMenu;',1)
    s=s.replace('procedure TFreeLinesplanFrame.UpdateMenu;', 'procedure TFreeLinesplanFrame.InvalidateRenderCache; begin end;\nprocedure TFreeLinesplanFrame.UpdateMenu;',1)
   if 'procedure ViewportMouseUp(' not in s:
    s=s.replace('    procedure UpdateMenu;', '    procedure ViewportMouseUp(Sender:TObject; Button:TMouseButton; Shift:TShiftState; X,Y:Integer);\n    procedure UpdateMenu;',1)
    s=s.replace('procedure TFreeLinesplanFrame.UpdateMenu;', 'procedure TFreeLinesplanFrame.ViewportMouseUp(Sender:TObject; Button:TMouseButton; Shift:TShiftState; X,Y:Integer); begin end;\nprocedure TFreeLinesplanFrame.UpdateMenu;',1)
  s=s.replace('procedure TFreeLinesplanFrame.CreateViewport;', "function PerfCounter(out Value:Int64):LongBool;stdcall;external 'kernel32.dll' name 'QueryPerformanceCounter';\nfunction PerfFrequencyCounter(out Value:Int64):LongBool;stdcall;external 'kernel32.dll' name 'QueryPerformanceFrequency';\nprocedure TFreeLinesplanFrame.CreateViewport;",1)
  a=s.index('procedure TFreeLinesplanFrame.ViewportRedraw');b=s.index('procedure TFreeLinesplanFrame.ZoomExtentsExecute',a)
  part=s[a:b].replace('  LabelIndex: integer;', '  LabelIndex: integer;\n  PerfTick,PerfFrequency,PerfNow: Int64;')
  part=part.replace('  procedure PlaceLineLabel',"  procedure Phase(const Name:string);\n  begin PerfCounter(PerfNow);WriteLn('PHASE ',Name,' ',FormatFloat('0.000',(PerfNow-PerfTick)*1000.0/PerfFrequency)); PerfTick:=PerfNow end;\n  procedure PlaceLineLabel",1)
  part=part.replace('  PaintStarted := GetTickCount64;', '  PerfFrequencyCounter(PerfFrequency);PerfCounter(PerfTick);\n  PaintStarted := GetTickCount64;')
  for token,name in [('  LabelIndex := 0;','preview'),('    DrawPendingLabels;','splines'),('  Mainplane.a := 1.0;','hydro'),('    // Reserve the feature lines','fill'),('    // Draw grid','features'),('    // draw knuckle-lines','grid_labels'),('    // Draw stations','knuckles'),('    Viewport.LabelLayout.EndFrame;','curves')]:
   assert token in part,token
   part=part.replace(token,"    Phase('"+name+"');\n"+token,1)
  label_call='      Viewport.TextOutLabel(Placement,Text);'
  if part.count(label_call)!=1:raise RuntimeError('Label placement adapter mismatch')
  part=part.replace(label_call,'''      begin
        LineplanTextMetrics.RecordPlacement(LabelIndex,Text,Placement.Position.X,Placement.Position.Y,
          Placement.Rect.Left,Placement.Rect.Top,Placement.Rect.Right,Placement.Rect.Bottom);
        Viewport.TextOutLabel(Placement,Text);
      end;''',1)
  s=s[:a]+part+s[b:]
  fill_anchor='    // Reserve the feature lines before placing any grid labels.'
  if s.count(fill_anchor)!=1:raise RuntimeError('Fill observation adapter mismatch')
  s=s.replace(fill_anchor,'    LineplanTextMetrics.RecordFill(Viewport.DrawingCanvas,Viewport.ClientWidth,Viewport.ClientHeight);\n'+fill_anchor,1)
  a=s.index('procedure TFreeLinesplanFrame.ExportDXFExecute');part=s[a:]
  assert '  if SaveDialog.Execute then' in part
  part=part.replace('  if SaveDialog.Execute then',"  SaveDialog.FileName:=ParamStr(2)+'/lines.dxf';\n  if True then",1)
  s=s[:a]+part;(work/'Forms/FreeLinesplanFrme.pas').write_text(s,encoding='utf-8')
  tree=E.parse(work/'FreeShip.lpi')
  for parent in tree.iter():
   for child in list(parent):
    if child.tag in ('ExecuteBefore','ExecuteAfter'):parent.remove(child)
  units=tools/'fpc/3.2.2/units/x86_64-win64'
  for n in tree.iter('OtherUnitFiles'):n.set('Value',n.get('Value','')+';'+';'.join(str(p) for p in units.iterdir() if p.is_dir() and p.name!='fv'))
  for n in tree.findall('.//CompilerOptions/Target/Filename'):n.set('Value','LinesplanPerformance')
  for n in tree.findall('.//Units/Unit0/Filename'):n.set('Value','Tests/LinesplanPerformance.pas')
  report['source_unchanged_during_copy']=common.fingerprint(repo)[0]==before
  if not report['source_unchanged_during_copy']:raise RuntimeError('Source changed during copy')
  project=work/'probe.lpi';tree.write(project,encoding='utf-8',xml_declaration=True)
  cmd=[str(tools/'lazbuild.exe'),'--lazarusdir='+str(tools),'--compiler='+str(tools/'fpc/3.2.2/bin/x86_64-win64/fpc.exe'),'--pcp='+str(work/'lazarus-config'),'--build-mode=Win64 Release','--ws=win32','--cpu=x86_64','--os=win64',str(project)]
  r=subprocess.run(cmd,cwd=work,capture_output=True,timeout=args.timeout);(out/'build.log').write_bytes(r.stdout+r.stderr)
  if r.returncode:raise SystemExit((r.stdout+r.stderr).decode('utf-8',errors='replace')[-2000:])
  env=os.environ.copy()
  for key in ['APPDATA','LOCALAPPDATA','TEMP','TMP']:
   dest=work/'profile'/key;dest.mkdir(parents=True,exist_ok=True);env[key]=str(dest)
  for model in models:
   original_hash=common.digest_file(model);dest=out/model.stem;dest.mkdir()
   shutil.copy2(model,work/'input.fbm')
   if common.digest_file(work/'input.fbm')!=original_hash:raise RuntimeError('Model copy mismatch')
   r=subprocess.run([str(work/'LinesplanPerformance.exe'),str(work/'input.fbm'),str(dest),str(args.event_pause_ms),str(args.width),str(args.height),str(args.pan_steps),str(args.initial_zoom)]+(['-trace-linesplan'] if args.trace_linesplan else []),cwd=work,env=env,capture_output=True,timeout=args.timeout,creationflags=common.FLAGS)
   text=(r.stdout+r.stderr).decode('utf-8',errors='replace');(dest/'timing.log').write_text(text,encoding='utf-8')
   report['models'].append({'path':str(model),'sha256':original_hash,'unchanged':common.digest_file(model)==original_hash,'exit':r.returncode})
   cases.append({'id':model.stem+'.runtime','status':'pass' if r.returncode==0 else 'fail','actual':text[-500:]})
   for key,value in re.findall(r'^STARTUP (\S+) ([01])',text,re.M):
    cases.append({'id':model.stem+'.startup.'+key,'status':'pass' if value=='1' else 'fail','expected':'Recorded hidden-startup or cache resource/geometry behavior must hold; see key and timing log','actual':value})
   for ex_style,class_style in re.findall(r'^NATIVESTYLE (\d+) (\d+)',text,re.M):
    cases.append({'id':model.stem+'.native-window-composition','status':'review',
     'ex_style':int(ex_style),'class_style':int(class_style),
     'descendant_composition_enabled':bool(int(ex_style)&0x02000000),
     'incompatible_class_styles':int(class_style)&(0x20|0x40|0x80),
     'scope':'Native HWND configuration observation only; no actual resize animation or blank-frame acceptance'})
   resize_frames=[]
   for row in re.findall(r'^RESIZEFRAME (\d+) (\d+) (-?\d+) (-?\d+) (\d+) (\d+) (\d+) (\d+) (\d+) ([0-9.]+)',text,re.M):
    i,c,mn,mx,bw,bh,cw,ch,scaled=map(int,row[:9]);ms=float(row[9])
    sample={'frame':i,'glyph_calls':c,'font_points':[mn,mx],'buffer':[bw,bh],'client':[cw,ch],'scaled_composite_blits':scaled,'elapsed_ms':ms}
    resize_frames.append(sample)
    cases.append({'id':model.stem+'.resize-native-text.'+str(i),
     'status':'pass' if c>0 and mn==7 and mx==7 and (bw,bh)==(cw,ch) and scaled==0 else 'fail',
     'expected':'First presentation after every resize draws seven-point text into a native-size buffer without scaling the composed text image', 'actual':sample})
   cases.append({'id':model.stem+'.resize-native-text.coverage','status':'pass' if len(resize_frames)==15 else 'fail','expected':'maximize, restore, maximize and twelve consecutive drag sizes','actual':len(resize_frames)})
   outputs=[tuple(map(int,row)) for row in re.findall(r'^RESIZEOUTPUT (\d+) (\d+) (\d+) (\d+) (\d+)',text,re.M)]
   cases.append({'id':model.stem+'.resize-exclusive-presentation','status':'pass' if len(outputs)==15 and all((b,c,tb,tc)==(0,0,1,0) for i,b,c,tb,tc in outputs) else 'fail',
    'expected':'Only one blit to the requested target, no clears and no incidental drawing to the previous window canvas while composing', 'actual':outputs})
   report['models'][-1]['resize_frames']=resize_frames
   projections=[int(v) for i,v in re.findall(r'^RESIZEPROJECTION (\d+) ([01])',text,re.M)]
   cases.append({'id':model.stem+'.resize-native-projection','status':'pass' if len(projections)==15 and all(projections) else 'fail',
    'expected':'The first presented frame already uses the final current-size projection, not just a larger bitmap with old geometry', 'actual':projections})
   for bitmap in dest.glob('*.bmp'):
    from PIL import Image
    im=Image.open(bitmap).convert('RGB')
    colors=im.getcolors(maxcolors=16777216)
    cases.append({'id':model.stem+'.content.'+bitmap.name,'status':'pass' if colors and len(colors)>2 else 'fail','expected':'nonempty raster with multiple colors','color_count':len(colors or [])})
   for name in re.findall(r'^CHECK (.+) pass',text,re.M):cases.append({'id':model.stem+'.'+name,'status':'pass','expected':'cache image equals forced full render'})
   for name in re.findall(r'^BEHAVIOR (.+) pass',text,re.M):cases.append({'id':model.stem+'.'+name,'status':'pass','expected':'independent translation reuse rule'})
   reuse=re.findall(r'LABELREUSE (pan|wheel) (\d+) (\d+)',text)
   for kind in ['pan','wheel']:
    values=[int(n) for k,i,n in reuse if k==kind]
    ok=len(values)==(args.pan_steps if kind=='pan' else 12) and (all(n>0 for n in values) if kind=='pan' else not any(values))
    cases.append({'id':model.stem+'.layout-reuse.'+kind,'status':'pass' if ok else 'fail','actual':values,
     'expected':'reuse interior placements during pan; solve normally during zoom'})
   times=[(int(i),int(t)) for i,t in re.findall(r'RENDER (\d+) (\d+)',text)]
   if times:report['models'][-1]['timing_ms']={'first':times[0][1],'repeat_median':statistics.median(t for i,t in times if i>0)}
   report['models'][-1]['interaction_ms']={kind:[float(v) for k,idx,v in re.findall(r'INTERACT (pan|wheel) (\d+) ([0-9.]+)',text) if k==kind] for kind in ['pan','wheel']}
   report['models'][-1]['pump_overhead_ms']={kind:[float(v) for k,v in re.findall(r'PUMP (pan|wheel) ([0-9.]+)',text) if k==kind] for kind in ['pan','wheel']}
   for trace in (work/'profile'/'TEMP').glob('FreeShip-linesplan-*.tsv'):shutil.copy2(trace,dest/trace.name)
   glyphs=re.findall(r'GLYPHS (pan|wheel) (\d+) (\d+) (-?\d+) (-?\d+) (\d+)',text)
   report['models'][-1]['glyph_redraw']=glyphs
   for kind in ['pan','wheel']:
    samples=[(int(c),int(mn),int(mx)) for k,i,c,mn,mx,busy in glyphs if k==kind]
    ok=len(samples)==(args.pan_steps if kind=='pan' else 12) and all(c>0 and mn==7 and mx==7 for c,mn,mx in samples)
    cases.append({'id':model.stem+'.crisp-text.'+kind,'status':'pass' if ok else 'fail',
     'expected':'Each input frame redraws visible labels at the original seven-point font size', 'actual':samples})
   for kind in ['pan','wheel']:
    cursor=[int(busy) for k,i,c,mn,mx,busy in glyphs if k==kind]
    animated=[int(v) for k,i,v in re.findall(r'ANIMATING (pan|wheel) (\d+) (\d+)',text) if k==kind]
    cases.append({'id':model.stem+'.no-busy-cursor.'+kind,'status':'pass' if len(cursor)==(args.pan_steps if kind=='pan' else 12) and not any(cursor) else 'fail','actual':cursor})
    cases.append({'id':model.stem+'.immediate-labels.'+kind,'status':'pass' if len(animated)==(args.pan_steps if kind=='pan' else 12) and not any(animated) else 'fail','actual':animated})
   following=[int(v) for i,v in re.findall(r'MICROPAN (\d+) (-?\d+)',text)]
   for reveal in ['zoom','pan','resize']:
    preview=dest/f'reveal-{reveal}-preview.bmp';fresh=dest/f'reveal-{reveal}-fresh.bmp'
    if preview.exists() and fresh.exists():
     from PIL import Image,ImageChops,ImageFilter
     def colored(path):
      im=Image.open(path).convert('RGB');r,g,b=im.split()
      # Ignore antialiasing/fringes: a filled interior has all channels below 245.
      return ImageChops.darker(ImageChops.darker(r,g),b).point(lambda v:255 if v<245 else 0)
     interior=colored(fresh).filter(ImageFilter.MinFilter(9))
     missing=ImageChops.subtract(interior,colored(preview));n=missing.histogram()[255];total=interior.histogram()[255]
     cases.append({'id':model.stem+'.fill-reveal.'+reveal,'status':'pass' if total>100 and n==0 else 'fail','expected':'No background holes inside the exact filled hull, excluding a four-pixel boundary','interior_pixels':total,'missing_pixels':n})
     difference=ImageChops.difference(Image.open(preview).convert('RGB'),Image.open(fresh).convert('RGB'))
     dr,dg,db=difference.split();mask=ImageChops.lighter(ImageChops.lighter(dr,dg),db)
     changed=mask.width*mask.height-mask.histogram()[0]
     cases.append({'id':model.stem+'.fill-quality-stable.'+reveal,'status':'pass' if changed==0 else 'fail',
      'expected':'The interaction frame and a fresh fill at the same pose have identical pixels, including silhouette edges; no idle quality swap', 'changed_pixels':changed})
    else:cases.append({'id':model.stem+'.fill-reveal.'+reveal,'status':'error','expected':'both fill-only observations exist'})
   pan_samples=[(int(n),int(errors)) for i,n,errors in re.findall(r'PANLABEL (\d+) (\d+) (\d+)',text)]
   cases.append({'id':model.stem+'.pan.pixel-follow','status':'pass' if len(following)==8 and all(v==1 for v in following) else 'fail','actual':following})
   cases.append({'id':model.stem+'.pan.label-translation','status':'pass' if len(pan_samples)==args.pan_steps and all(n>0 and errors==0 for n,errors in pan_samples) else 'fail','expected':'Interior labels and clipping rectangles translate by exactly the pan delta','actual':pan_samples})
   if args.fill_reference and r.returncode==0:
    from PIL import Image,ImageChops,ImageFilter
    reference=args.fill_reference/model.stem
    for pose in ['zoom','pan','resize']:
     name='reveal-'+pose+'-fresh.bmp';old=reference/name;new=dest/name
     if not old.exists() or not new.exists():
      cases.append({'id':model.stem+'.fill-geometry.'+pose,'status':'error','expected':'both independently rendered fill observations exist'});continue
     def fill_mask(path):
      im=Image.open(path).convert('RGB');r,g,b=im.split()
      return ImageChops.darker(ImageChops.darker(r,g),b).point(lambda v:255 if v<245 else 0)
     expected=fill_mask(old);actual=fill_mask(new)
     if expected.size!=actual.size:raise RuntimeError('Fill reference dimensions differ')
     # Filtering changes a narrow raster edge, never the model silhouette.
     missing=ImageChops.subtract(expected,actual.filter(ImageFilter.MaxFilter(5))).histogram()[255]
     outside=ImageChops.subtract(actual,expected.filter(ImageFilter.MaxFilter(5))).histogram()[255]
     cases.append({'id':model.stem+'.fill-geometry.'+pose,'status':'review',
      'expected':'Cross-renderer raster fringes require image review; independent model-rectangle check asserts scale and position',
      'missing_pixels_beyond_two_pixel_distance':missing,'outside_pixels_beyond_two_pixel_distance':outside,
      'expected_bounds':expected.getbbox(),'actual_bounds':actual.getbbox(),
      'reference':str(old)})
   if args.compare and r.returncode==0:
    reference=args.compare/model.stem
    for file in dest.iterdir():
     if file.suffix not in ('.bmp','.dxf'):continue
     # Historical previews contain the missing-fill bug. Their replacement is
     # judged against the fresh hull interior above, not equality to that bug.
     if file.name.startswith('reveal-') and file.name.endswith('-preview.bmp'):continue
     old=reference/file.name;equal=False;byte_equal=False
     if old.exists():
      byte_equal=old.read_bytes()==file.read_bytes()
      if file.suffix=='.bmp':
       from PIL import Image,ImageChops
       equal=ImageChops.difference(Image.open(old).convert('RGB'),Image.open(file).convert('RGB')).getbbox() is None
      else:equal=records(old)==records(file)
     cases.append({'id':model.stem+'.reference.'+file.name,'status':'pass' if equal else 'fail','byte_equal':byte_equal,'expected':'identical pixels or exact DXF coordinates and connectivity, allowing line reversal and closed-loop starting-point changes'})
  if report['test_code_sha256'] != {n:common.digest_file(ROOT/n) for n in report['test_code_sha256']}:
   raise RuntimeError('Test code changed during run')
  report['source_unchanged_during_run']=common.fingerprint(repo)[0]==before
  if not report['source_unchanged_during_run']:raise RuntimeError('Source changed during run')
  if not all(m['unchanged'] for m in report['models']):raise RuntimeError('Original model changed')
  cases.append({'id':'desktop.interaction','status':'not_run','expected':'native window and user input acceptance'})
  report['status']='fail' if any(c['status']=='fail' for c in cases) else 'partial'
except Exception as exc:
 report['error']=str(exc)
finally:
 (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps({'status':report['status'],'report':str(out/'report.json')},ensure_ascii=False))
if report['status']=='error':raise SystemExit(3)
if report['status']=='fail':raise SystemExit(1)
raise SystemExit(2)
