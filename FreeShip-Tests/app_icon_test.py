"""Inspect PE and Windows shell icon extraction without operating a window."""
import argparse,ctypes,json,hashlib
from pathlib import Path

def check(exe):
    k=ctypes.WinDLL('kernel32',use_last_error=True)
    k.LoadLibraryExW.argtypes=[ctypes.c_wchar_p,ctypes.c_void_p,ctypes.c_uint];k.LoadLibraryExW.restype=ctypes.c_void_p
    k.FindResourceW.argtypes=[ctypes.c_void_p,ctypes.c_wchar_p,ctypes.c_void_p];k.FindResourceW.restype=ctypes.c_void_p
    k.FreeLibrary.argtypes=[ctypes.c_void_p]
    h=k.LoadLibraryExW(str(exe.resolve()),None,2)
    embedded=bool(h and k.FindResourceW(h,'APPICON26',14))
    if h:k.FreeLibrary(h)
    shell=ctypes.WinDLL('shell32');shell.ExtractIconExW.argtypes=[ctypes.c_wchar_p,ctypes.c_int,ctypes.POINTER(ctypes.c_void_p),ctypes.POINTER(ctypes.c_void_p),ctypes.c_uint]
    big=ctypes.c_void_p();small=ctypes.c_void_p()
    n=shell.ExtractIconExW(str(exe.resolve()),0,ctypes.byref(big),ctypes.byref(small),1)
    valid=bool(n and big.value and small.value)
    u=ctypes.WinDLL('user32');u.DestroyIcon.argtypes=[ctypes.c_void_p]
    for icon in (big,small):
        if icon.value:u.DestroyIcon(icon)
    return {'status':'pass' if embedded and valid else 'fail','exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'embedded_app_icon':embedded,'shell_large_and_small_icons':valid,'limitations':['Taskbar cached shortcut icon and physical desktop appearance require review']}
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--exe',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--refresh-shell',action='store_true');a=p.parse_args();r=check(a.exe);
    if a.refresh_shell:
        notify=ctypes.WinDLL('shell32').SHChangeNotify;notify.argtypes=[ctypes.c_long,ctypes.c_uint,ctypes.c_void_p,ctypes.c_void_p]
        path=ctypes.c_wchar_p(str(a.exe.resolve()));notify(0x2000,0x1005,ctypes.cast(path,ctypes.c_void_p),None);r['targeted_shell_refresh_notified']=True
    a.output.mkdir(parents=True,exist_ok=True);(a.output/'report.json').write_text(json.dumps(r,indent=2),encoding='utf8');print(json.dumps(r));raise SystemExit(r['status']!='pass')
