"""Isolated four-edge gravity regression using the production Qt layout source."""
import argparse,json,os,shutil,subprocess,tempfile,hashlib
import freeship_ui_test as common
from pathlib import Path
ROOT=Path(__file__).resolve().parent;QT=ROOT.parent/'FreeShip-Qt'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--legacy-snap-control',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);r={'status':'error'}
 sources=[QT/'src/render/label_layout.cpp',QT/'src/render/label_layout.h',ROOT/'qt/label_boundary_tests.cpp'];before={str(p):sha(p) for p in sources}
 try:
  with tempfile.TemporaryDirectory(prefix='fs-qt-labels-') as td:
   w=Path(td)
   for p in sources:shutil.copy2(p,w/p.name)
   if a.legacy_snap_control:
    p=w/'label_layout.cpp';s=p.read_text();s=s.replace('if(!clip.isEmpty())reserve(clip);','if(clip.isEmpty()&&!named){current=best;position=anchor+best;clip=available(QRectF(position,size));entry.velocity={};}\n    if(!clip.isEmpty())reserve(clip);');p.write_text(s)
   tools=common.junction_for(ROOT.parent/'FreeShip-QtPilot/.tools',Path(tempfile.gettempdir()));prefix=tools/'Qt/6.8.3/mingw_64';compiler=Path(os.environ['LOCALAPPDATA'])/'FreeShip26QtTools/mingw1310_64/bin';cmake=tools/'python/Scripts/cmake.exe'
   env=os.environ.copy();env['PATH']=str(compiler)+';'+str(prefix/'bin')+';'+str(cmake.parent)+';'+env['PATH'];env['QT_QPA_PLATFORM']='windows' if os.name=='nt' else 'offscreen'
   (w/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(LabelBoundary LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 17)\nfind_package(Qt6 REQUIRED COMPONENTS Gui)\nadd_executable(boundary label_layout.cpp label_boundary_tests.cpp)\ntarget_link_libraries(boundary PRIVATE Qt6::Gui)\n')
   with (out/'build.log').open('wb') as log:
    for cmd in ([str(cmake),'-S',str(w),'-B',str(w/'build'),'-G','Ninja','-DCMAKE_PREFIX_PATH='+str(prefix),'-DCMAKE_CXX_COMPILER='+str(compiler/'g++.exe')],[str(cmake),'--build',str(w/'build'),'--parallel','2']):subprocess.run(cmd,env=env,stdout=log,stderr=log,check=True,timeout=120)
   subprocess.run([str(w/'build/boundary.exe'),str(out)],env=env,cwd=w,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30)
   r=json.loads((out/'report.json').read_text());r['legacy_snap_control']=a.legacy_snap_control
 except Exception as e:r['error']=str(e)
 r['sources_sha256']=before;r['source_unchanged']=all(sha(Path(p))==h for p,h in before.items())
 if not r['source_unchanged']:r['status']='error'
 (out/'report.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps({'status':r['status'],'cases':len(r.get('cases',[])),'error':r.get('error'),'report':str(out/'report.json')}));return 0 if r['status']=='pass' else 1
if __name__=='__main__':raise SystemExit(main())
