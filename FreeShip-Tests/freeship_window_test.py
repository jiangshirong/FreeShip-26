"""Isolated FreeShip hidden-window movement probe. Never inject desktop input."""
from __future__ import annotations
import argparse, hashlib, json, os, re, shutil, statistics, subprocess, tempfile, time
from collections import Counter
from pathlib import Path
import xml.etree.ElementTree as ET
import freeship_ui_test as common
ROOT=Path(__file__).resolve().parent
CODE=('freeship_window_test.py','UiWindowProbe.pas','UiWindowEvents.pas','freeship_ui_test.py')

def code_hashes(): return {p:common.digest_file(ROOT/p) for p in CODE}
def insert_once(path,pattern,replacement,label):
    text=path.read_text(encoding='utf8'); text,n=re.subn(pattern,replacement,text,count=0,flags=re.M)
    if n!=1: raise RuntimeError(f'window instrumentation {label}: expected one structural match, got {n} in {path.name}')
    path.write_text(text,encoding='utf8',newline='\n')
def replace_exact(path,old,new,label,required=True):
    text=path.read_text(encoding='utf8'); count=text.count(old)
    if count!=1:
        if not required and count==0: return False
        raise RuntimeError(f'{label}: expected one match, got {count} in {path.name}')
    path.write_text(text.replace(old,new),encoding='utf8',newline='\n'); return True

def prepare(src,work,mutation):
    for folder in common.FOLDERS: shutil.copytree(src/folder,work/folder)
    for p in src.iterdir():
        if p.is_file() and p.suffix.lower() in common.ROOT_SUFFIXES: shutil.copy2(p,work/p.name)
    source_snapshot_sha=common.fingerprint(work)[0]
    for name in ('Ships','Manuals','Import'): (work/name).mkdir(exist_ok=True)
    shutil.copy2(common.FIXTURE,work/'fixture.fbm')
    mdi=work/'Units/MDIPanel.inc'; vp=work/'Units/FreeViewport.inc'
    if mutation=='no_cache':
        replace_exact(vp,'if FPresentationFrozen and FPresentationBufferValid and not Printing then begin',
                      'if False then begin','no_cache mutation')
    elif mutation=='blank_erase':
        p=vp.read_text(encoding='utf8'); start=p.index('procedure TFreeViewport.WMEraseBkgnd'); end=p.index('\nend;',start)
        fragment=p[start:end];
        if fragment.count('Message.Result:=1;')!=1: raise RuntimeError('blank_erase mutation anchor unavailable')
        p=p[:start]+fragment.replace('Message.Result:=1;','Message.Result:=0;')+p[end:];vp.write_text(p,encoding='utf8',newline='\n')
    elif mutation=='no_dirty':
        replace_exact(mdi,'LCLIntf.RedrawWindow(Parent.Handle,@DirtyBounds,0,Flags)','begin end','no_dirty mutation')
    elif mutation=='no_time_compensation':
        replace_exact(vp,'    if FProgressiveStart<>0 then Inc(FProgressiveStart,Paused);',
                      '    { mutation: progressive time compensation omitted }','progressive timer mutation')
        replace_exact(vp,'    if FLabelRevealStart<>0 then Inc(FLabelRevealStart,Paused);',
                      '    { mutation: label time compensation omitted }','label timer mutation')
    elif mutation=='label_whitespace':
        path=work/'Units/FreeLabelLayout.pas'
        replace_exact(path,'  Result.Visible:=False; Result.Collisions:=0;',
                      '  Result.Visible := False; Result.Collisions := 0;','unrelated label instrumentation mutation')
    elif mutation=='snap_disabled':
        replace_exact(mdi,'  if DistanceX<=Threshold then X:=BestX;',
                      '  if False then X:=BestX;','snap_disabled mutation')
    elif mutation=='no_copybits':
        replace_exact(mdi,'WindowPos^.flags or SWP_NOCOPYBITS','WindowPos^.flags','no_copybits mutation')
    elif mutation=='min_overlap':
        replace_exact(mdi,'P.SetBounds((N mod Cols)*W,Max(0,Parent.ClientHeight-((N div Cols)+1)*H),W,H)',
                      'P.SetBounds(0,0,W,H)','min_overlap mutation')
    snapshot_sha=common.fingerprint(work)[0]
    for name in ('UiWindowProbe.pas','UiWindowEvents.pas'): shutil.copy2(ROOT/name,work/'Units'/name)
    insert_once(work/'Units/MDIPanel.pas',r'^(\s*procedure SetBounds\(ALeft, ATop, AWidth, AHeight: Integer\); override;)$',
                r'\1\n    procedure UiWindowMotion(Value:TWindowPositionState);\n    function UiWindowSnap(var X,Y:Integer):Boolean;','motion accessor')
    snap_implemented = bool(re.search(r'procedure\s+TCustomMDIPanel\.SnapMovePosition\s*\(', mdi.read_text(encoding='utf8'),re.I))
    snap_body = 'begin SnapMovePosition(X,Y); Result:=True end;' if snap_implemented else 'begin Result:=False end;'
    insert_once(mdi,r'^(procedure TCustomMDIPanel\.SetBounds\()',
                'procedure TCustomMDIPanel.UiWindowMotion(Value:TWindowPositionState);\nbegin SetMotionState(Value) end;\n'
                'function TCustomMDIPanel.UiWindowSnap(var X,Y:Integer):Boolean;\n'+snap_body+'\n\n'+r'\1','motion adapter')
    insert_once(work/'Units/MDIPanel.pas',r'^(uses\s+intfgraphics, lazcanvas, FPImage, lcl;)$',
                r'uses UiWindowEvents, intfgraphics, lazcanvas, FPImage, lcl;','redraw interception uses')
    text=mdi.read_text(encoding='utf8'); count=text.count('LCLIntf.RedrawWindow(')
    if not count: raise RuntimeError('window repaint observation has no RedrawWindow calls; cannot safely instrument')
    mdi.write_text(text.replace('LCLIntf.RedrawWindow(','UiWindowEvents.RedrawWindowProbe('),encoding='utf8',newline='\n')
    insert_once(work/'Units/FreeGeometry.pas',r'^(\s*property PresentationFrozen:Boolean read FPresentationFrozen write SetPresentationFrozen;)$',
                r'    procedure UiWindowPaint;\n    function UiWindowBuffer:TBitmap;\n    function UiWindowBufferValid:Boolean;\n    procedure UiWindowInvalidateBuffer;\n    function UiWindowProgressiveStart:QWord;\n    function UiWindowRevealStart:QWord;\n    function UiWindowPreviewTimerEnabled:Boolean;\n\1','viewport accessors')
    insert_once(vp,r'^(procedure TFreeViewport\.Paint;)$',
                r'procedure TFreeViewport.UiWindowPaint; begin Paint end;\nfunction TFreeViewport.UiWindowBuffer:TBitmap; begin Result:=FDrawingBuffer end;\nfunction TFreeViewport.UiWindowBufferValid:Boolean; begin Result:=FPresentationBufferValid end;\nprocedure TFreeViewport.UiWindowInvalidateBuffer; begin FPresentationBufferValid:=False end;\nfunction TFreeViewport.UiWindowProgressiveStart:QWord; begin Result:=FProgressiveStart end;\nfunction TFreeViewport.UiWindowRevealStart:QWord; begin Result:=FLabelRevealStart end;\nfunction TFreeViewport.UiWindowPreviewTimerEnabled:Boolean; begin Result:=FProgressiveTimer.Enabled end;\n\n\1','viewport paint adapter')
    insert_once(work/'Units/FreeGeometry.pas',r'^(uses\s+)(FreeStringsUnit\s*,)',
                r'\1UiWindowEvents, \2','paint observation uses')
    insert_once(vp,r'(procedure TFreeViewport\.Paint;[\s\S]*?\nbegin\n)',
                r'\1  UiWindowEvents.RecordPaint(Self);\n','paint count')
    insert_once(vp,r'^(\s*)(Canvas\.StretchDraw\(\s*ClientRect\s*,\s*FDrawingBuffer\s*\)\s*;)',
                r'\1UiWindowEvents.TimedCacheCopy(Self,Canvas,ClientRect,FDrawingBuffer);','cache copy timing')
    insert_once(vp,r'^(\s*)(FDrawingCanvas\.TextRect\(\s*R\s*,\s*x\s*,\s*y\s*,\s*val\s*,\s*Style\s*\)\s*;)',
                r'\1UiWindowEvents.RecordDrawnText(Self,val); \2','clipped text output')
    replace_exact(vp,'    end else FDrawingCanvas.TextOut(x,y,val);',
                  '    end else begin UiWindowEvents.RecordDrawnText(Self,val); FDrawingCanvas.TextOut(x,y,val) end;',
                  'ordinary text output')
    project=work/'FreeShip.dpr'
    if mutation=='app_activation_redirect':
        insert_once(project,r'^(\s*)Application\.Run\s*;\s*$',
                    r'\1Application.OnActivate := MainForm.OnActivate;\n\1Application.Run;',
                    'application activation redirection mutation')
    insert_once(project,r"^(\s*Main\s+in\s+'Forms/Main\.pas'\s+\{MainForm\},)$",r"  UiWindowProbe in 'Units/UiWindowProbe.pas',\n\1",'program uses')
    insert_once(project,r'^(\s*)Application\.Run\s*;\s*$',r'\1UiWindowProbe.RunWindowProbe(MainForm);','isolated entry')
    edit=work/'Units/FreeEdit.inc'
    replace_exact(edit,'        ExceptionDlg.ShowModal;','        raise Exception.Create(ErrMsg);','fixture load modal guard')
    lpi=work/'FreeShip.lpi'; tree=ET.parse(lpi)
    for parent in tree.iter():
        for child in list(parent):
            if child.tag=='ExecuteBefore': parent.remove(child)
    return tree,lpi,source_snapshot_sha,snapshot_sha

def enrich(result,output):
    samples=result.get('move_samples',[])
    def missing_request(sample):
        a,b=sample['old_bounds'],sample['new_bounds']
        required={'left':min(a['left'],b['left']),'top':min(a['top'],b['top']),
                  'right':max(a['right'],b['right']),'bottom':max(a['bottom'],b['bottom'])}
        ok=any(q.get('result') and (q.get('flags',0)&0x1) and (q.get('flags',0)&0x4) and
               (q.get('flags',0)&0x80) and (q.get('flags',0)&0x100) and q.get('rect') and
               q['rect']['left']<=required['left'] and q['rect']['top']<=required['top'] and
               q['rect']['right']>=required['right'] and q['rect']['bottom']>=required['bottom']
               for q in sample.get('requests',[]))
        return None if ok else {'iteration':sample.get('iteration'),'required':required,'requests':sample.get('requests',[])}
    missing=[item for sample in samples if (item:=missing_request(sample)) is not None]
    result['cases'].append({'id':'window.redraw-request-coverage','status':'pass' if len(samples)==40 and not missing else 'fail',
        'expected':'every move issues successful invalidate/erase/all-children request covering old and new bounds',
        'actual':{'samples':len(samples),'missing':missing},
        'note':'Only intercepted LCL RedrawWindow requests are measured; alternative repaint APIs and screen composition are not observed.'})
    resize=result.get('resize_sample')
    resize_missing=missing_request(resize) if resize else None
    result['cases'].append({'id':'window.resize-redraw-request',
        'status':'not_run' if resize is None else 'fail' if resize_missing else 'pass',
        'expected':'resizing requests repaint covering old and new bounds',
        'actual':resize_missing if resize_missing else ('resize sample unavailable' if resize is None else resize),
        'note':'Intercepts RedrawWindow requests in the hidden process; not proof of actual desktop painting.'})
    def intersection(a,b):
        box={'left':max(a['left'],b['left']),'top':max(a['top'],b['top']),
             'right':min(a['right'],b['right']),'bottom':min(a['bottom'],b['bottom'])}
        return box if box['left']<box['right'] and box['top']<box['bottom'] else None
    overlaps=exposures=0
    for sample in samples:
        old,new=sample['old_bounds'],sample['new_bounds']
        for sibling in sample.get('sibling_bounds',[]):
            if intersection(new,sibling): overlaps+=1
            exposed=intersection(old,sibling)
            if exposed and not (new['left']<=exposed['left'] and new['top']<=exposed['top'] and
                                new['right']>=exposed['right'] and new['bottom']>=exposed['bottom']): exposures+=1
    result['cases'].append({'id':'window.overlap-exposure-scenario','status':'pass' if overlaps and exposures else 'fail',
        'expected':'movement includes overlapping siblings and a previously covered sibling region exposed later',
        'actual':{'overlap_pairs':overlaps,'exposed_pairs':exposures},
        'note':'Geometric fixture coverage, not proof of a rendered desktop frame.'})
    result['cases'].append({'id':'window.sibling-visible-repaint','status':'not_run',
        'expected':'exposed sibling pixels visibly repaint in the composed desktop',
        'actual':'dirty region request observed; actual screen composition and sibling presentation not measured'})
    for field in ('move_ms','four_paints_ms','cache_copy_ms','model_redraw_ms'):
        values=sorted(x[field] for x in samples)
        if values:
            result.setdefault('timings_ms',{})[field]={'n':len(values),'p50':statistics.median(values),
                'p95':values[max(0,__import__('math').ceil(len(values)*.95)-1)],'max':values[-1], 'total':sum(values)}
    result.setdefault('limitations',[]).append('Hidden-window timings include scheduling and instrumentation, not real-screen input, DWM composition or performance acceptance.')
    try:
        from PIL import Image,ImageChops
        for name in (f'window-buffer-{phase}-{i}.bmp' for phase in ('before','after') for i in range(4)):
            with Image.open(output/name) as im:
                rgb=im.convert('RGB'); extrema=rgb.getextrema()
                nonuniform=any(lo!=hi for lo,hi in extrema)
                result['cases'].append({'id':'window.bitmap.'+name,'status':'pass' if nonuniform else 'fail',
                    'expected':'actual fixture model buffer is not uniform','actual':{'size':list(im.size),'extrema':extrema},'image':name})
        with Image.open(output/'window-model-before.bmp') as a, Image.open(output/'window-model-frozen.bmp') as b, Image.open(output/'window-model-thawed.bmp') as c:
            aa,bb,cc=a.convert('RGB'),b.convert('RGB'),c.convert('RGB')
            if aa.size != bb.size or aa.size != cc.size:
                raise ValueError('model buffer sizes differ')
            frozen_diff=ImageChops.difference(aa,bb); thaw_diff=ImageChops.difference(aa,cc)
            frozen_change=frozen_diff.getbbox(); thaw_change=thaw_diff.getbbox()
            thaw_diff.save(output/'window-model-thaw-diff.png')
            result['cases'].append({'id':'window.model-change-freeze-thaw',
                'status':'pass' if frozen_change is None and thaw_change is not None and result.get('model_text_evidence',{}).get('before') and not result.get('model_text_evidence',{}).get('frozen') and result.get('model_text_evidence',{}).get('thawed') else 'fail',
                'expected':'isolated model edit keeps cached frame while frozen, then redraws a changed frame after thaw',
                'actual':{'frozen_pixel_difference':frozen_change,'thawed_pixel_difference':thaw_change, 'drawn_name':result.get('model_text_evidence')},
                'image':'window-model-thawed.bmp','diff_image':'window-model-thaw-diff.png'})
    except (ImportError,OSError,ValueError) as e:
        result['cases'].append({'id':'window.bitmap.evidence','status':'not_run','expected':'model buffer pixel evidence','actual':str(e)})

def summarize(cases):
    themes={
      'motion':('window.motion-','window.snap-','window.capture-cancel-focus'),
      'cache_and_model':('window.initial-real-model-buffer','window.frozen-','window.unfreeze-','window.resize-frozen-',
                         'window.resize-rebuild-','window.invalid-cache-','window.model-change-','window.bitmap.'),
      'repaint_and_flicker':('window.redraw-request-','window.resize-redraw-','window.overlap-exposure-',
                             'window.sibling-visible-','window.position-no-copy-','window.erase-','window.caption-prepaint','window.compositor-'),
      'window_states':('window.minimize-','window.restore-','window.repeated-maximize-','window.maximize-z-',
                       'window.tile-','window.resize-parent-','window.real-stacking'),
      'new_view':('window.new-view-',),
      'timers':('window.preview-timer-','window.timer-'),
      'printing':('window.print-export',),
      'performance':('window.performance-',),
    }
    groups={}
    for case in cases:
        ident=case.get('id','')
        name=next((theme for theme,prefixes in themes.items() if ident.startswith(prefixes)),'other')
        groups.setdefault(name,[]).append(case.get('status'))
    return {'status_counts':dict(Counter(c.get('status') for c in cases)),
            'themes':{name:{'status':common.classify_statuses(statuses),
                            'counts':dict(Counter(statuses))} for name,statuses in groups.items()}}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    ap.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS);ap.add_argument('--output',type=Path,default=ROOT/'reports'/('window-'+time.strftime('%Y%m%d-%H%M%S')))
    ap.add_argument('--timeout',type=int,default=240);ap.add_argument('--mutation',choices=('none','app_activation_redirect','no_cache','blank_erase','no_dirty','min_overlap','no_copybits','label_whitespace','snap_disabled','no_time_compensation'),default='none');a=ap.parse_args()
    output=a.output.resolve();output.mkdir(parents=True,exist_ok=True);src=a.source.resolve(); before=None
    result={'status':'error','mutation':a.mutation,'test_code_sha256':code_hashes(),'fixture_sha256':common.digest_file(common.FIXTURE),'cases':[],
            'limitations':['No real desktop input, focus, compositor, or on-screen visual acceptance.']}
    try:
        before,count=common.fingerprint(src);result.update(source_sha256=before,source_file_count=count)
        tools=common.junction_for(a.lazarus.resolve(),Path(tempfile.gettempdir()))
        with common.temporary_workspace('fs-window-run-') as run:
            work=run/'source';work.mkdir(); evidence=run/'evidence';evidence.mkdir();profile=run/'profile';profile.mkdir()
            tree,project,copy_sha,snapshot_sha=prepare(src,work,a.mutation);result['source_snapshot_sha256']=copy_sha;result['mutated_snapshot_sha256']=snapshot_sha
            if copy_sha!=before:raise RuntimeError('Source snapshot does not match the fingerprinted checkout')
            if common.fingerprint(src)[0]!=before:raise RuntimeError('Source changed during snapshot')
            if code_hashes()!=result['test_code_sha256']:raise RuntimeError('Test code changed during snapshot')
            for name in ('UiWindowProbe.pas','UiWindowEvents.pas'):
                if common.digest_file(work/'Units'/name)!=result['test_code_sha256'][name]:raise RuntimeError('Probe copy changed during snapshot')
            if common.digest_file(work/'fixture.fbm')!=result['fixture_sha256']:raise RuntimeError('Fixture copy changed')
            runtime=tools/'fpc'/'3.2.2'/'units'/'x86_64-win64'
            for p in tree.iter('OtherUnitFiles'):p.set('Value',p.get('Value','')+';'+';'.join(str(x) for x in sorted(runtime.iterdir()) if x.is_dir() and x.name!='fv'))
            for target in tree.findall('.//CompilerOptions/Target/Filename'): target.set('Value','window-probe')
            tree.write(project,encoding='utf8',xml_declaration=True)
            result['instrumented_source_sha256']=common.fingerprint(work)[0]
            env=os.environ.copy();env.update(TEMP=str(run),TMP=str(run),APPDATA=str(profile),LOCALAPPDATA=str(profile),FS_UI_PROBE_OUTPUT=str(evidence),FS_UI_PROBE_MODEL=str(work/'fixture.fbm'))
            compiler=tools/'fpc'/'3.2.2'/'bin'/'x86_64-win64'/'fpc.exe';result['compiler_sha256']=common.digest_file(compiler)
            cmd=[str(tools/'lazbuild.exe'),f'--lazarusdir={tools}',f'--compiler={compiler}',f'--pcp={run/"config"}','--build-mode=Win32 Debug','--ws=win32','--cpu=x86_64','--os=win64',str(project)]
            build=subprocess.run(cmd,cwd=work,env=env,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS);(output/'build.log').write_bytes(build.stdout+b'\n'+build.stderr)
            if build.returncode:raise RuntimeError(f'Build failed ({build.returncode}); see build.log')
            exe=work/'window-probe.exe';result['instrumented_exe_sha256']=common.digest_file(exe)
            process=subprocess.run([str(exe)],cwd=work,env=env,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS)
            (output/'runtime.log').write_bytes(process.stdout+b'\n'+process.stderr);p=evidence/'probe.json'
            if not p.is_file():raise RuntimeError(f'Probe produced no JSON; exit={process.returncode}')
            native=json.loads(p.read_text(encoding='utf-8-sig'));result.update(native);result['process_exit_code']=process.returncode
            for img in evidence.glob('*.bmp'):shutil.copy2(img,output/img.name)
            if process.returncode or native.get('error'):raise RuntimeError(f'Probe execution failed: exit={process.returncode}; {native.get("error")}')
            # The new-view action can leave a partially constructed child; exercise it in a separate process.
            trial=run/'new-view';trial.mkdir(); (trial/'profile').mkdir(); new_model=trial/'fixture.fbm'
            shutil.copy2(common.FIXTURE,new_model)
            if common.digest_file(new_model)!=result['fixture_sha256']:raise RuntimeError('New-view fixture copy changed')
            new_env=env.copy();new_env.update(TEMP=str(trial),TMP=str(trial),APPDATA=str(trial/'profile'),
                LOCALAPPDATA=str(trial/'profile'),FS_UI_PROBE_OUTPUT=str(trial),FS_UI_PROBE_MODEL=str(new_model),FS_UI_WINDOW_SUBCASE='new-view')
            new_process=subprocess.run([str(exe)],cwd=work,env=new_env,capture_output=True,timeout=a.timeout,creationflags=common.FLAGS)
            (output/'new-view-runtime.log').write_bytes(new_process.stdout+b'\n'+new_process.stderr)
            new_probe=trial/'probe.json';result['new_view_process_exit']=new_process.returncode
            if new_probe.is_file():
                new_native=json.loads(new_probe.read_text(encoding='utf-8-sig'));result['cases'].extend(new_native.get('cases',[]))
            else:
                result['cases'].append({'id':'window.new-view-lifecycle','status':'error','expected':'independent new-view probe completes',
                    'actual':'new-view process produced no JSON'})
            if new_process.returncode:
                result['cases'].append({'id':'window.new-view-process-exit','status':'error','expected':'new-view probe exits normally',
                    'actual':{'exit_code':new_process.returncode,'note':'process exit is an independent diagnostic; cause not established'}})
            enrich(result,output)
            ids=[c.get('id') for c in result['cases']];statuses=[c.get('status') for c in result['cases']]
            required={'window.initial-real-model-buffer','window.motion-start-all','window.frozen-reuses-model-buffer','window.motion-end-all','window.unfreeze-redraw','window.resize-frozen-cache','window.resize-rebuild-size','window.invalid-cache-first-redraw','window.invalid-cache-subsequent-reuse','window.new-view-lifecycle','window.minimize-distinct','window.restore-original-bounds','window.repeated-maximize-restores','window.tile-restores-state','window.overlap-exposure-scenario','window.sibling-visible-repaint','window.redraw-request-coverage','window.resize-redraw-request','window.erase-handled','window.snap-neighbor','window.snap-free-move','window.position-no-copy-bits','window.preview-timer-state','window.timer-pause-compensation','window.caption-prepaint','window.compositor-trails-flicker','window.capture-cancel-focus','window.print-export','window.timer-recovery'}
            if len(ids)!=len(set(ids)) or not required.issubset(ids):raise RuntimeError('Incomplete or duplicate window cases')
            result['source_unchanged_during_run']=common.fingerprint(src)[0]==before
            if not result['source_unchanged_during_run']:raise RuntimeError('Source changed during test; results rejected')
            if code_hashes()!=result['test_code_sha256'] or common.digest_file(common.FIXTURE)!=result['fixture_sha256']:raise RuntimeError('Probe or fixture changed during test; results rejected')
            result['status']=common.classify_statuses(statuses)
    except (Exception,subprocess.TimeoutExpired) as e:
        result['status']='error';result['error']=f'{type(e).__name__}: {e}'
        if before is not None:
            try: result['source_unchanged_during_run']=common.fingerprint(src)[0]==before
            except Exception: result['source_unchanged_during_run']=None
    result['summary']=summarize(result['cases'])
    (output/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':result['status'],'report':str(output/'report.json'),'cases':len(result['cases']),'error':result.get('error')},ensure_ascii=False))
    return {'pass':0,'fail':1,'partial':2,'error':3}[result['status']]
if __name__=='__main__':raise SystemExit(main())
