from pathlib import Path
import subprocess,xml.etree.ElementTree as E
repo=Path('K:/');project=repo/'FreeShip.lpi'
protected=[project,repo/'GITLastChangeRevision.inc',repo/'GitCommit.inc']
saved={p:p.read_bytes() for p in protected}
log=Path(__file__).with_name('build-post-commit-render.log')
try:
 tree=E.parse(project)
 for n in tree.findall('.//Target/Filename'):n.set('Value','FreeShip-stage')
 units=Path('L:/fpc/3.2.2/units/x86_64-win64')
 for n in tree.iter('OtherUnitFiles'):n.set('Value','Units;Forms;'+';'.join(p.as_posix() for p in units.iterdir() if p.is_dir() and p.name!='fv'))
 tree.write(project,encoding='utf-8',xml_declaration=True)
 with log.open('wb') as f:
  r=subprocess.run(['L:/lazbuild.exe','--lazarusdir=L:/','--pcp=K:/.lazarus-config','--build-all','--build-mode=Win64 Release','K:/FreeShip.lpi'],cwd=repo,stdout=f,stderr=subprocess.STDOUT)
 print('BUILD_EXIT',r.returncode,'LOG',log)
 if r.returncode: print(log.read_text(encoding='utf-8',errors='replace')[-4000:])
 raise SystemExit(r.returncode)
finally:
 for p,b in saved.items():p.write_bytes(b)
