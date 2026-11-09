"""Capture real compositor frames and control ONLY a newly launched private probe.

User authorization, 2026-10-30: a dedicated test may control its own isolated
instance; never discover, attach to, or send commands to an existing FreeShip.
"""
from pathlib import Path
import argparse, collections, ctypes, json, os, queue, shutil, subprocess, sys
import threading, time, uuid, xml.etree.ElementTree as ET
from contextlib import contextmanager
import av
from fractions import Fraction
import numpy as np
from PIL import Image, ImageDraw
import freeship_ui_test as common

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'.capture-deps'))
WM_SYSCOMMAND=0x112

def image_metrics(image):
    a=np.asarray(image.convert('RGB'))
    # Window decoration and toolbar are outside the drawing-area observation.
    a=a[max(60,int(a.shape[0]*.07)):-8,8:-8]
    a=a[::max(1,a.shape[0]//400),::max(1,a.shape[1]//640)].astype(np.int16)
    hi=a.max(axis=2);lo=a.min(axis=2)
    return {'colored_fraction':float(((hi-lo)>45).mean()),
            'white_fraction':float((lo>245).mean()),
            'black_fraction':float((hi<18).mean()),
            'neutral_dark_fraction':float(((hi<48)&((hi-lo)<12)).mean())}

def visual_window_rect(image):
    """Locate test-owned window against its known neutral backdrop, not the
    already-committed HWND bounds (DWM may still be animating smaller bounds)."""
    a=np.asarray(image.convert('RGB'))[::4,::4].astype(np.int16)
    occupied=np.max(np.abs(a-112),axis=2)>18
    ys,xs=np.where(occupied)
    if not len(xs):return (0,0,image.width,image.height)
    return (max(0,int(xs.min())*4-4),max(0,int(ys.min())*4-4),
            min(image.width,int(xs.max())*4+8),min(image.height,int(ys.max())*4+8))

def classify_frames(frames):
    """A colored model must exist before a temporary near-empty drawing frame.

    Reports whole-view white/black loss as fail, lesser losses as review.
    This intentionally does not infer flicker from render timing alone.
    """
    reference=max((f['colored_fraction'] for f in frames),default=0)
    stable_dark=[f.get('desktop_dark_fraction',f.get('neutral_dark_fraction',0)) for f in frames
                 if f.get('phase') in ('baseline','settled') and f['colored_fraction']>.02]
    dark_reference=float(np.median(stable_dark)) if stable_dark else 0
    anomalies=[]
    for f in frames:
        if f.get('phase','').startswith(('maximize','restore')) and reference>.02:
            loss=f['colored_fraction']/reference
            # A 7.9% black edge band was plainly visible in a previously
            # accepted native frame. Whole-view-only thresholds missed it.
            if max(f.get('neutral_dark_fraction',0),f.get('desktop_dark_fraction',0))>max(.02,dark_reference+.015):
                anomalies.append({**f,'severity':'fail','reason':'large black region in drawing area'})
            elif loss<.12 and (f['white_fraction']>.86 or f['black_fraction']>.75):
                anomalies.append({**f,'severity':'fail','reason':'temporary white/black model loss'})
            elif loss<.45:
                anomalies.append({**f,'severity':'review','reason':'partial model loss or changing window geometry'})
    return reference,anomalies

def reanalyze(a):
    """Keep raw capture evidence immutable when the detector changes."""
    original=a.reanalyze.resolve();old=json.loads(original.read_text(encoding='utf-8'))
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    reference,anomalies=classify_frames(old['frames'])
    status='fail' if any(f['severity']=='fail' for f in anomalies) else 'review' if anomalies else 'pass'
    if old.get('capture_errors') or old.get('error') or not old.get('source_unchanged') or not old.get('model_unchanged'):
        status='error'
    if not old['frames'] or reference<=.02:status='error'
    report={'status':status,'original_status':old['status'],'evidence_report':str(original),
      'evidence_sha256':common.digest_file(original),'detector_sha256':common.digest_file(Path(__file__)),
      'source_sha256':old['source_sha256'],'frame_count':old['frame_count'],
      'reference_colored_fraction':reference,'anomalies':anomalies,
      'scope':'Reanalysis of immutable original compositor frames; no new app execution'}
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'status':status,'anomalies':len(anomalies),'report':str(out/'report.json')},ensure_ascii=False))
    return report

def build(repo,work,lazarus,out,variant,host,probe=None,app_probe=None,prepare=None):
    for name in common.FOLDERS:
        shutil.copytree(repo/name,work/name,ignore=shutil.ignore_patterns('*.exe','*.o','*.ppu','*.bmp','*.log'))
    for p in repo.iterdir():
        if p.is_file() and p.suffix.lower() in common.ROOT_SUFFIXES:shutil.copy2(p,work/p.name)
    (work/'Tests').mkdir();shutil.copy2(ROOT/'NativeLinesplanProbe.pas',work/'Tests/NativeLinesplanProbe.pas')
    if probe is not None:
        if (ROOT/'UiNativeRender.pas').exists():shutil.copy2(ROOT/'UiNativeRender.pas',work/'Units/UiNativeRender.pas')
        content=probe.read_text(encoding='utf-8-sig')
        content=content.replace(';',';\n{$APPTYPE CONSOLE}',1)
        (work/'Tests/NativeLinesplanProbe.pas').write_text(content,encoding='utf-8')
    for name in ('Ships','Import','Manuals','Exec','Export','UserData/Ships','UserData/Import','UserData/Export'):
        (work/name).mkdir(parents=True,exist_ok=True)
    if host=='full-app':
        shutil.copy2(app_probe or ROOT/'NativeAppCaptureProbe.pas',work/'Units/NativeAppCaptureProbe.pas')
        p=work/'FreeShip.dpr';s=p.read_text(encoding='utf-8-sig')
        anchors={"  Main in 'Forms/Main.pas'":"  NativeAppCaptureProbe,\n  Main in 'Forms/Main.pas'",
          '   InitByParameters;':"   sOpenFile:=ParamStr(1); // private probe arguments",
          '   Application.Run;':'   BeginNativeAppProbe(MainForm);\n   Application.Run;'}
        for old,new in anchors.items():
            if s.count(old)!=1:raise RuntimeError('Full-app entry adapter changed: '+old)
            s=s.replace(old,new,1)
        s=s.replace('   checkInstallation;','   ConfigureNativeAppProbe(MainForm);\n   checkInstallation;')
        s=s.replace("Application.Title := 'FreeShip_26';","Application.Title := 'FreeShip_26 - isolated native test';")
        p.write_text(s,encoding='utf-8')
        p=work/'Forms/Main.pas';s=p.read_text(encoding='utf-8-sig')
        old='procedure TMainForm.RefreshNativeLanguageHotkey;\nbegin'
        if s.count(old)!=1:raise RuntimeError('Private hotkey adapter changed')
        p.write_text(s.replace(old,old+'\n  Exit; // Never register a system hotkey in the private probe.'),encoding='utf-8')
    if variant=='no-background-fix':
        # One-variable negative control, applied only to the disposable copy.
        p=work/'Forms/FreeLinesplanFrm.pas';s=p.read_text(encoding='utf-8-sig')
        a=s.index('  ResizeCommand:=(M=$0112)')
        b=s.index('  Host:=TFreeLinesplanForm(Data);',a)
        s=s[:a]+'  Result:=LinesplanDefaultSubclass(W,M,WP,LP);\n'+s[b:]
        p.write_text(s,encoding='utf-8')
    if variant=='black-background-control':
        p=work/'Forms/FreeLinesplanFrm.pas';s=p.read_text(encoding='utf-8-sig')
        old='LinesplanSetClassBackground(W,-10,PtrInt(LCLIntf.GetStockObject(0)))'
        if s.count(old)!=1:raise RuntimeError('Background-color control adapter changed')
        p.write_text(s.replace(old,old.replace('GetStockObject(0)','GetStockObject(4)')),encoding='utf-8')
    shutil.copy2(work/'Forms/FreeLinesplanFrm.pas',out/'compiled-form-source.pas')
    tools=common.junction_for(lazarus,Path(os.environ['TEMP']))
    if prepare is not None:prepare(work)
    tree=ET.parse(work/'FreeShip.lpi')
    for parent in tree.iter():
        for child in list(parent):
            if child.tag in ('ExecuteBefore','ExecuteAfter'):parent.remove(child)
    target='FreeShip' if host=='release' else 'NativeLinesplanProbe'
    for n in tree.findall('.//Target/Filename'):n.set('Value',target)
    for n in tree.findall('.//Units/Unit0/Filename'):n.set('Value','FreeShip.dpr' if host in ('full-app','release') else 'Tests/NativeLinesplanProbe.pas')
    units=tools/'fpc/3.2.2/units/x86_64-win64'
    for n in tree.iter('OtherUnitFiles'):
        n.set('Value',n.get('Value','')+';'+';'.join(str(p) for p in units.iterdir() if p.is_dir() and p.name!='fv'))
    tree.write(work/'native.lpi',encoding='utf-8',xml_declaration=True)
    cmd=[str(tools/'lazbuild.exe'),'--lazarusdir='+str(tools),'--pcp='+str(work/'lazarus-config'),
         '--build-mode=Win64 Release','--ws=win32',str(work/'native.lpi')]
    r=subprocess.run(cmd,cwd=work,capture_output=True,timeout=120,creationflags=common.FLAGS)
    (out/'build.log').write_bytes(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError('Build failed; see build.log')
    return {'instrumented_source_sha256':common.fingerprint(work)[0],
            'executable_sha256':common.digest_file(work/(target+'.exe'))}

def build_release(a):
    """Compile an uninstrumented candidate; never change the installed entry."""
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    repo=a.source.resolve();before=common.fingerprint(repo)[0]
    report={'status':'error','source_sha256':before,'mode':'uninstrumented release build only'}
    try:
        with common.temporary_workspace('fs-candidate-') as td:
            work=Path(td);report['build']=build(repo,work,a.lazarus.resolve(),out,'current','release')
            shutil.copy2(work/'FreeShip.exe',out/'FreeShip.exe')
            report['source_unchanged']=common.fingerprint(repo)[0]==before
            report['status']='pass' if report['source_unchanged'] else 'error'
    except Exception as e:report['error']=str(e)
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False));return report

def run(a):
    ctypes.windll.user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
    import win32gui,win32process,win32con
    from windows_capture import WindowsCapture, DxgiDuplicationSession
    class_background=ctypes.windll.user32.GetClassLongPtrW
    class_background.argtypes=[ctypes.c_void_p,ctypes.c_int]
    class_background.restype=ctypes.c_size_t
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    repo=a.source.resolve();original=common.fingerprint(repo)[0]
    model_hash=common.digest_file(a.model)
    report={'status':'error','source_sha256':original,'model_sha256':model_hash,'variant':a.variant,
      'host':a.host,'language':a.language,'fault_injection':a.inject,'frames':[],'events':[],'limitations':[
        'WGC captures the real window compositor surface, not the app drawing buffer.',
        'Outer DWM desktop animation/occlusion is not covered by window-only capture.',
        'Event-driven capture delivers changed surfaces; it is not a fixed display-refresh clock.',
        'No existing app HWND is accepted; only PID and nonce verified probe handles are controlled.']}
    report['test_code_sha256']={name:common.digest_file(ROOT/name) for name in
      ('native_flicker_test.py','NativeLinesplanProbe.pas','NativeAppCaptureProbe.pas')}
    process=None;control=None;frame_queue=queue.Queue(maxsize=128);errors=[];phase=['baseline'];stop_screen=threading.Event();screen_thread=None
    def writer():
        container=None;stream=None;start_qpc=None;last_pts=-1
        while True:
            item=frame_queue.get()
            if item is None:break
            meta,pixels=item
            try:
                if a.backend=='screen':
                    if container is None:
                        container=av.open(str(out/'capture-lossless.mkv'),'w')
                        stream=container.add_stream('libx264rgb',rate=120)
                        stream.width=pixels.shape[1];stream.height=pixels.shape[0];stream.pix_fmt='bgr0'
                        stream.options={'crf':'0','preset':'ultrafast','threads':'4'}
                        stream.time_base=Fraction(1,1000);stream.codec_context.time_base=Fraction(1,1000)
                        start_qpc=meta['callback_qpc_s']
                    frame=av.VideoFrame.from_ndarray(pixels,format='bgra')
                    last_pts=max(last_pts+1,round((meta['callback_qpc_s']-start_qpc)*1000))
                    frame.pts=last_pts;frame.time_base=Fraction(1,1000);meta['video_pts_ms']=last_pts
                    for packet in stream.encode(frame):container.mux(packet)
                else:
                    im=Image.fromarray(pixels[:,:,2::-1].copy())
                    meta.update(image_metrics(im));im.save(out/meta['file'],compress_level=1)
                report['frames'].append(meta)
            except Exception as e:errors.append(str(e))
        if container is not None:
            for packet in stream.encode():container.mux(packet)
            container.close()
    thread=threading.Thread(target=writer,daemon=True);thread.start()
    @contextmanager
    def private_workspace():
        with common.temporary_workspace('fs-native-') as td:
            try:yield td
            finally:
                # Release this run's executable before the verified temporary
                # directory is removed, including startup/dialog failures.
                stop_screen.set()
                if process is not None and process.poll() is None:
                    process.terminate();process.wait(timeout=10)
    try:
      with private_workspace() as td:
        work=Path(td);report['build']=build(repo,work,a.lazarus.resolve(),out,a.variant,a.host)
        if common.fingerprint(repo)[0]!=original:raise RuntimeError('Source changed during copy/build')
        env=os.environ.copy()
        for k in ('APPDATA','LOCALAPPDATA','TEMP','TMP'):
            d=work/'profile'/k;d.mkdir(parents=True,exist_ok=True);env[k]=str(d)
        shutil.copy2(a.model,work/'input.fbm')
        ready=work/'ready.json';nonce='FreeShip-Native-Test-'+uuid.uuid4().hex
        output=(out/'runtime.log').open('wb')
        process=subprocess.Popen([str(work/'NativeLinesplanProbe.exe'),str(work/'input.fbm'),str(ready),nonce,'-trace-linesplan',a.language],
            cwd=work,env=env,stdout=output,stderr=subprocess.STDOUT,creationflags=common.FLAGS)
        deadline=time.monotonic()+30
        while not ready.exists():
            if process.poll() is not None or time.monotonic()>deadline:raise RuntimeError('Private probe did not become ready')
            time.sleep(.05)
        info=json.loads(ready.read_text(encoding='utf-8-sig'));hwnd=info['hwnd'];report['probe']=info
        report['initial_class_background']=class_background(hwnd,-10)
        def owned():
            if process.poll() is not None or not win32gui.IsWindow(hwnd):raise RuntimeError('Private probe exited')
            if win32process.GetWindowThreadProcessId(hwnd)[1]!=process.pid or info['pid']!=process.pid:raise RuntimeError('PID ownership mismatch')
            if win32gui.GetWindowText(hwnd)!=nonce:raise RuntimeError('Window nonce mismatch')
        owned()
        if a.backend=='screen':
            # Topmost applies only to these two verified private HWNDs. We do
            # not activate, move, hide or resize any existing user window.
            for h in (info['backdrop_hwnd'],hwnd):
                if win32process.GetWindowThreadProcessId(h)[1]!=process.pid:raise RuntimeError('Private backdrop PID mismatch')
                win32gui.SetWindowPos(h,win32con.HWND_TOPMOST,0,0,0,0,
                    win32con.SWP_NOMOVE|win32con.SWP_NOSIZE|win32con.SWP_NOACTIVATE)
        capture=WindowsCapture(window_hwnd=hwnd,cursor_capture=False,draw_border=False,
            secondary_window=False,minimum_update_interval=0)
        serial=[0]
        @capture.event
        def on_frame_arrived(frame,capture_control):
            serial[0]+=1
            meta={'index':serial[0],'file':f'frames/{serial[0]:06d}.png',
                  'surface_time_s':frame.timespan/10000000,'callback_qpc_s':time.perf_counter(),
                  'width':frame.width,'height':frame.height,'phase':phase[0]}
            try:frame_queue.put_nowait((meta,frame.frame_buffer.copy()))
            except queue.Full:errors.append('Capture queue overflow: lost a compositor frame')
        @capture.event
        def on_closed():pass
        (out/'frames').mkdir()
        if a.backend=='window':control=capture.start_free_threaded()
        else:
            report['limitations'][0]='DXGI captures actual desktop composition, cropped to an owned fullscreen backdrop.'
            report['limitations'][1]='Only frames whose foreground and sampled backdrop coverage belong to the private PID are retained.'
            def screen_capture():
                try:
                    session=DxgiDuplicationSession()
                    l,t,r,b=win32gui.GetWindowRect(info['backdrop_hwnd'])
                    # Exclude the outer shadow/corner/taskbar activation strip.
                    bounds=(l+12,t+12,r-12,b-12)
                    taskbar=win32gui.FindWindow('Shell_TrayWnd',None)
                    if taskbar:
                        tl,tt,tr,tb=win32gui.GetWindowRect(taskbar)
                        if tr-tl>tb-tt:
                            reserve=min(120,tb-tt+4)
                            bounds=(bounds[0],max(bounds[1],t+reserve) if tt<t+(b-t)/2 else bounds[1],
                                    bounds[2],min(bounds[3],b-reserve) if tt>=t+(b-t)/2 else bounds[3])
                    report['capture_bounds']=list(bounds)
                    report['limitations'].append('The taskbar edge strip is excluded, including its auto-hide reveal area.')
                    while not stop_screen.is_set():
                        frame=session.acquire_frame(timeout_ms=16)
                        if frame is None:continue
                        # Refuse screenshots if any sampled point reveals another process.
                        left,top,right,bottom=bounds
                        foreign=[];above=[True]
                        def check_top(h,_):
                            if h==info['backdrop_hwnd']:above[0]=False
                            if not above[0] or not win32gui.IsWindowVisible(h):return
                            if win32process.GetWindowThreadProcessId(h)[1]==process.pid:return
                            cloaked=ctypes.c_uint32()
                            ctypes.windll.dwmapi.DwmGetWindowAttribute(ctypes.c_void_p(h),14,ctypes.byref(cloaked),4)
                            if cloaked.value:return
                            l,t,r,b=win32gui.GetWindowRect(h)
                            if max(l,left)<min(r,right) and max(t,top)<min(b,bottom):
                                foreign.append({'hwnd':h,'class':win32gui.GetClassName(h),'rect':[l,t,r,b]})
                        win32gui.EnumWindows(check_top,None)
                        if foreign:
                            report['foreign_window_observation']=foreign
                            errors.append('Non-private window above backdrop; screen recording stopped');break
                        points=[(x,y) for x in range(left+4,right-4,160) for y in range(top+4,bottom-4,160)]
                        occluded=[pt for pt in points if win32process.GetWindowThreadProcessId(win32gui.WindowFromPoint(pt))[1]!=process.pid]
                        if occluded:
                            report['occlusion_observation']=[{'point':pt,'class':win32gui.GetClassName(win32gui.WindowFromPoint(pt))} for pt in occluded[:4]]
                            errors.append('Private backdrop occluded; screen recording stopped');break
                        serial[0]+=1
                        pixels=frame.to_numpy()[top:bottom,left:right].copy()
                        if frame.color_format=='rgba8':pixels=pixels[:,:,[2,1,0,3]].copy()
                        meta={'index':serial[0],'file':f'frames/{serial[0]:06d}.png','callback_qpc_s':time.perf_counter(),
                              'width':pixels.shape[1],'height':pixels.shape[0],'phase':phase[0],
                              'capture_backend':'dxgi-owned-region','chart_rect':list(win32gui.GetWindowRect(hwnd))}
                        l,t,r,b=meta['chart_rect']
                        meta['observation_rect']=[max(0,l-left),max(0,t-top),min(right-left,r-left),min(bottom-top,b-top)]
                        try:frame_queue.put_nowait((meta,pixels))
                        except queue.Full:errors.append('Capture queue overflow: lost a desktop frame')
                except Exception as e:errors.append('DXGI: '+str(e))
            screen_thread=threading.Thread(target=screen_capture,daemon=True);screen_thread.start()
        time.sleep(.8)
        for cycle in range(a.cycles):
            for name,command in [('maximize',win32con.SC_MAXIMIZE),('restore',win32con.SC_RESTORE)]:
                owned();phase[0]=f'{name}-{cycle+1}'
                report['events'].append({'action':phase[0],'qpc_s':time.perf_counter(),'target_hwnd':hwnd,'target_pid':process.pid})
                win32gui.PostMessage(hwnd,WM_SYSCOMMAND,command,0)
                if a.inject!='none' and name=='maximize':
                    time.sleep(.35);owned();win32gui.PostMessage(hwnd,win32con.WM_APP+201,1 if a.inject=='white' else 2,0)
                time.sleep(a.settle)
                report['events'][-1]['actual_window_placement']=list(win32gui.GetWindowPlacement(hwnd))
                report['events'][-1]['class_background_restored']=class_background(hwnd,-10)==report['initial_class_background']
                if not report['events'][-1]['class_background_restored']:
                    errors.append('Native shared class background was not restored after the operation')
        phase[0]='settled';time.sleep(.5)
        if control is not None:control.stop();control=None
        stop_screen.set()
        if screen_thread is not None:screen_thread.join(timeout=3)
        owned();win32gui.PostMessage(hwnd,win32con.WM_CLOSE,0,0)
        process.wait(timeout=10);output.close()
        for p in (work/'profile/TEMP').glob('FreeShip-linesplan-*.tsv'):shutil.copy2(p,out/p.name)
    except Exception as e:report['error']=str(e)
    finally:
        stop_screen.set()
        if screen_thread is not None:screen_thread.join(timeout=3)
        if control is not None:
            try:control.stop()
            except Exception as e:errors.append(str(e))
        if process is not None and process.poll() is None:
            # This is the Popen object created above, never a process-name search.
            process.terminate();process.wait(timeout=10)
        frame_queue.put(None);thread.join(timeout=30)
        if thread.is_alive():errors.append('Frame writer did not finish; evidence is incomplete')
        if a.backend=='screen' and (out/'capture-lossless.mkv').exists() and not thread.is_alive():
            with av.open(str(out/'capture-lossless.mkv')) as video:
                decoded=0
                for decoded,frame in enumerate(video.decode(video=0),1):
                    if decoded>len(report['frames']):errors.append('Encoded frame count mismatch');break
                    meta=report['frames'][decoded-1];im=frame.to_image()
                    meta['visual_rect']=visual_window_rect(im)
                    meta.update(image_metrics(im.crop(meta['visual_rect'])))
                    desktop_metrics=image_metrics(im)
                    meta['desktop_black_fraction']=desktop_metrics['black_fraction']
                    meta['desktop_dark_fraction']=desktop_metrics['neutral_dark_fraction']
                    im.save(out/meta['file'],compress_level=1)
                if decoded!=len(report['frames']):errors.append('Missing encoded compositor frames')
        report['capture_errors']=errors
        report['model_unchanged']=common.digest_file(a.model)==model_hash
        report['source_unchanged']=common.fingerprint(repo)[0]==original
        reference,anomalies=classify_frames(report['frames']);report['reference_colored_fraction']=reference
        report['anomalies']=anomalies;report['frame_count']=len(report['frames'])
        if 'error' not in report and not errors and report['frame_count']>=3 and reference>.02:
            report['status']='fail' if any(f['severity']=='fail' for f in anomalies) else 'review' if anomalies else 'pass'
        if not report['source_unchanged'] or not report['model_unchanged']:report['status']='error'
        (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        if report['frames']:
            selected=report['frames'][:1]+[f for f in anomalies][:24]+report['frames'][-1:]
            sheet=Image.new('RGB',(960,290*((len(selected)+2)//3)),'#ddd');draw=ImageDraw.Draw(sheet)
            for i,f in enumerate(selected):
                im=Image.open(out/f['file']);im.thumbnail((320,260));x=i%3*320;y=i//3*290
                sheet.paste(im,(x,y+24));draw.text((x+4,y+3),f"{f['index']} {f['phase']} color={f['colored_fraction']:.3f}",fill='black')
            sheet.save(out/'contact.jpg')
        print(json.dumps({'status':report['status'],'frames':report['frame_count'],'anomalies':len(anomalies),'report':str(out/'report.json')},ensure_ascii=False))
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    p.add_argument('--lazarus',type=Path,default=common.DEFAULT_LAZARUS)
    p.add_argument('--model',type=Path,default=common.FIXTURE)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--cycles',type=int,default=3)
    p.add_argument('--settle',type=float,default=1.2)
    p.add_argument('--variant',choices=['current','no-background-fix','black-background-control'],default='current')
    p.add_argument('--inject',choices=['none','white','black'],default='none')
    p.add_argument('--backend',choices=['window','screen'],default='screen')
    p.add_argument('--host',choices=['standalone','full-app'],default='full-app')
    p.add_argument('--language',choices=['en','cn','ru'],default='cn')
    p.add_argument('--build-release-only',action='store_true',help='Build an uninstrumented FreeShip.exe candidate without starting any window or installing it')
    p.add_argument('--reanalyze',type=Path,help='Reevaluate an existing report using current detector rules, preserving the original evidence')
    args=p.parse_args()
    if args.host=='full-app' and args.inject!='none':p.error('Fault injection requires --host standalone')
    if not 0<=args.cycles<=30 or args.settle<.5:p.error('Use 0..30 cycles and at least 0.5 second settling')
    result=reanalyze(args) if args.reanalyze else build_release(args) if args.build_release_only else run(args)
    raise SystemExit({'pass':0,'fail':1,'review':2,'error':3}[result['status']])
