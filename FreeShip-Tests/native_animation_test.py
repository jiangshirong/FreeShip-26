"""Capture only an owned native four-view animation instance; no existing HWND input."""
from pathlib import Path
import argparse,json,os,subprocess,shutil,time,uuid,statistics
import freeship_ui_test as c
import native_flicker_test as n
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--model',type=Path,default=c.FIXTURE);p.add_argument('--snapshot',action='store_true');p.add_argument('--without-base-cache',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
r={'status':'error','scope':'Visible isolated full app; WGC changed-surface intervals plus per-view real OnRedraw QPC timings','limitations':['WGC changed surfaces are not fixed monitor refresh samples; no claim about GPU scanout','Only the newly launched PID and nonce are controlled; no existing windows touched']};process=None;control=None
try:
 import ctypes,ctypes.wintypes
 ctypes.windll.user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
 import win32gui,win32process,numpy as np
 from windows_capture import WindowsCapture
 before=c.fingerprint(c.DEFAULT_SOURCE)[0];model_hash=c.digest_file(a.model)
 with c.temporary_workspace('fs-animation-') as td:
  def prepare(work):
   if a.without_base_cache:
    path=work/'Units/FreeViewport.inc';text=path.read_text(encoding='utf-8-sig');text=text.replace(' Result:=False;\n if not FOverlayCollecting', ' Result:=False;Exit;\n if not FOverlayCollecting',1).replace('procedure TFreeViewport.SaveAnimationBase;\nbegin', 'procedure TFreeViewport.SaveAnimationBase;\nbegin\n Exit;',1);path.write_text(text,encoding='utf-8')
   shutil.copy2(Path(__file__).with_name('UiAnimationProfile.pas'),work/'Units/UiAnimationProfile.pas')
   path=work/'Units/FreeShipUnit.pas';text=path.read_text(encoding='utf-8-sig');text=text.replace('uses Math,','uses UiAnimationProfile, Math,',1);path.write_text(text,encoding='utf-8')
   path=work/'Units/FreeShip.inc';text=path.read_text(encoding='utf-8-sig')
   for anchor,tag in [('  Viewport.BeginOverlayFrame;','start'),('  Surface.Draw(Viewport);  // Main Draw','surface'),('  Viewport.EndOverlayFrame;','overlay'),('  DrawQueuedGridLabels;','labels')]:
    assert text.count(anchor)==1,anchor
    text=text.replace(anchor,anchor+"\n  ProfileStage(PtrUInt(Viewport),'"+tag+"');",1)
   text=text.replace('  Surface.Draw(Viewport);  // Main Draw',"  ProfileStage(PtrUInt(Viewport),'before-surface');\n  Surface.Draw(Viewport);  // Main Draw",1)
   path.write_text(text,encoding='utf-8')
   path=work/'Units/FreeGeometry.pas';text=path.read_text(encoding='utf-8-sig').replace('uses FreeStringsUnit,','uses UiAnimationProfile, FreeStringsUnit,',1);path.write_text(text,encoding='utf-8')
   path=work/'Units/FreeSubdivisionSurface.inc';text=path.read_text(encoding='utf-8-sig');start=text.index('procedure TFreeSubdivisionSurface.Draw(Viewport: TFreeViewport);');end=text.index('end;{TFreeSubdivisionSurface.Draw}',start)
   section=text[start:end]
   section=section.replace('  Viewport.AlphaBuffer.Draw;',"  ProfileStage(PtrUInt(Viewport),'layers');\n  Viewport.AlphaBuffer.Draw;",1)
   section=section.replace('    // Restore the shared drawing font',"    ProfileStage(PtrUInt(Viewport),'edges');\n    // Restore the shared drawing font",1)
   text=text[:start]+section+text[end:];path.write_text(text,encoding='utf-8')
  w=Path(td);r['build']=n.build(c.DEFAULT_SOURCE,w,c.DEFAULT_LAZARUS,out,'current','full-app',app_probe=Path(__file__).with_name('NativeAnimationProbe.pas'),prepare=prepare)
  env=os.environ.copy()
  for k in ('APPDATA','LOCALAPPDATA','TEMP','TMP'):
   d=w/'profile'/k;d.mkdir(parents=True,exist_ok=True);env[k]=str(d)
  shutil.copy2(a.model,w/'input.fbm');ready=w/'ready.json';nonce='FS-animation-'+uuid.uuid4().hex
  log=(out/'runtime.log').open('wb')
  process=subprocess.Popen([str(w/'NativeLinesplanProbe.exe'),str(w/'input.fbm'),str(ready),nonce],cwd=w,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=c.FLAGS)
  try:
   deadline=time.monotonic()+40
   while not ready.exists():
    if process.poll() is not None or time.monotonic()>deadline:raise RuntimeError('Private animation instance startup failed: exit='+str(process.poll())+' error='+(Path(str(ready)+'.error').read_text() if Path(str(ready)+'.error').exists() else 'none'))
    time.sleep(.05)
   info=json.loads(ready.read_text());hwnd=info['hwnd'];r['probe']=info;r['animation_base_cache_disabled']=a.without_base_cache
   if info['pid']!=process.pid or win32process.GetWindowThreadProcessId(hwnd)[1]!=process.pid or win32gui.GetWindowText(hwnd)!=nonce:raise RuntimeError('Owned window verification failed')
   bounds=ctypes.wintypes.RECT()
   if ctypes.windll.dwmapi.DwmGetWindowAttribute(ctypes.c_void_p(hwnd),9,ctypes.byref(bounds),ctypes.sizeof(bounds))<0:
    bounds.left,bounds.top,bounds.right,bounds.bottom=win32gui.GetWindowRect(hwnd)
   origin=(bounds.left,bounds.top)
   view_rects=[win32gui.GetWindowRect(h) for h in info['viewports']];r['view_rects_screen']=view_rects
   last_surface=[None]
   frames=[];previous=[None]*4;changed=[[] for _ in range(4)];errors=[]
   capture=WindowsCapture(window_hwnd=hwnd,cursor_capture=False,draw_border=False,secondary_window=False,minimum_update_interval=0)
   @capture.event
   def on_frame_arrived(frame,capture_control):
    try:
     stamp=time.perf_counter();frames.append({'qpc_s':stamp,'surface_s':frame.timespan/10000000})
     pixels=frame.frame_buffer;h,width=pixels.shape[:2]
     if a.snapshot:last_surface[0]=pixels.copy()
     for i in range(4):
      left,top,right,bottom=view_rects[i]
      x0=max(0,left-origin[0]+20);y0=max(0,top-origin[1]+20)
      x1=min(width,right-origin[0]-20);y1=min(h,bottom-origin[1]-20)
      patch=pixels[y0:y1:4,x0:x1:4,:3].copy()
      if previous[i] is not None and patch.shape==previous[i].shape and np.any(patch!=previous[i]):changed[i].append(stamp)
      previous[i]=patch
    except Exception as e:errors.append(str(e))
   @capture.event
   def on_closed():pass
   control=capture.start_free_threaded();time.sleep(.3);Path(str(ready)+'.start').write_text('start')
   deadline=time.monotonic()+15
   while not Path(str(ready)+'.done').exists():
    if process.poll() is not None or time.monotonic()>deadline:raise RuntimeError('Animation timed out')
    time.sleep(.05)
   control.stop();control=None
   if a.snapshot and last_surface[0] is not None:
    from PIL import Image
    Image.fromarray(last_surface[0][:,:,2::-1].copy()).save(out/'native-window.png')
    r['snapshot']='native-window.png';r['limitations'].append('Snapshot copies may add capture overhead; do not use snapshot mode for performance comparisons')
   shutil.copy2(str(ready)+'.profile',out/'profile.csv')
   start=int(Path(str(ready)+'.done').read_text())/info['frequency'];rows=[]
   for line in Path(str(ready)+'.frames').read_text().splitlines():
    i,stamp,duration=map(int,line.split(','));rows.append({'view':i,'qpc_s':stamp/info['frequency'],'draw_ms':1000*duration/info['frequency']})
   def stats(values):
    v=sorted(values)
    return {'count':len(v),'p50':statistics.median(v),'p95':v[min(len(v)-1,int(len(v)*.95))],'max':max(v)} if v else {'count':0}
   def intervals(stamps):return stats([1000*(b-a) for a,b in zip(stamps,stamps[1:])])
   bfs=lambda t:start<=t<=start+1.3
   r.update(source_sha256=before,model_sha256=model_hash,capture_errors=errors,frames=frames,draws=rows)
   r['bfs_capture_intervals_ms']=intervals([f['qpc_s'] for f in frames if bfs(f['qpc_s'])]);r['views']=[]
   for i in range(4):
    times=[x['qpc_s'] for x in rows if x['view']==i and bfs(x['qpc_s'])];change=[t for t in changed[i] if bfs(t)]
    r['views'].append({'view':i,'draw_intervals_ms':intervals(times),'draw_cost_ms':stats([x['draw_ms'] for x in rows if x['view']==i and bfs(x['qpc_s'])]),'visible_change_intervals_ms':intervals(change),'visible_updates_per_second':len(change)/1.3})
   r['status']='pass' if len(frames)>5 and all(v['draw_cost_ms']['count']>3 for v in r['views']) and not errors else 'fail'
  finally:
   if control:control.stop();control=None
   if process.poll() is None:process.terminate();process.wait(timeout=10)
   log.close()
 r['source_unchanged']=c.fingerprint(c.DEFAULT_SOURCE)[0]==before;r['model_unchanged']=c.digest_file(a.model)==model_hash
 if not r['source_unchanged'] or not r['model_unchanged']:r['status']='error'
except Exception as e:r['error']=str(e)
(out/'report.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps({k:v for k,v in r.items() if k not in ('frames','draws')},ensure_ascii=False));raise SystemExit(0 if r['status']=='pass' else 1)
