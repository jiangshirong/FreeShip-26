#!/usr/bin/env python3
"""Link the published 0.1.17 caption binary with only the current update UI adapter."""
import pathlib,sys,subprocess
old,work=map(lambda x:pathlib.Path(x).resolve(),sys.argv[1:])
source=old/'source/FreeShip-Qt/src/app/update_controller.cpp'
import hashlib
assert hashlib.sha256(source.read_bytes()).hexdigest()=='41ae6ad1d4686a21879ab050f9304265be68829d4628042928de0974612cbdf0'
commands=subprocess.check_output(['ninja','-t','commands','FreeShipQt'],cwd=old/'build',text=True).splitlines()
obj='CMakeFiles/FreeShipQt.dir/src/app/update_controller.cpp.o'
cmd=next(s for s in commands if ' -c ' in s and obj in s)
new=work/'starter-ui.cpp'
assert hashlib.sha256(new.read_bytes()).hexdigest()=='c187661ed9fee6cea3b8488c193c24b6236d3fe7fb4c65095892bfb496143d1f'
subprocess.run(cmd.replace(str(source),str(new)).replace(obj,str(work/'starter.o'))+' -iquote '+str(source.parent),cwd=old/'build',shell=True,check=True)
link=commands[-1]
assert ' -o FreeShipQt ' in link and obj in link and 'libproperty_ui.a' in link
subprocess.run(link.replace(' -o FreeShipQt ',' -o '+str(work/'starter-bin')+' ').replace(obj,str(work/'starter.o')).replace('libproperty_ui.a',str(old/'caption-r2/libproperty_ui.a')),cwd=old/'build',shell=True,check=True)
