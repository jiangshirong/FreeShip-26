"""Retained diagnostic: run trusted historical solvers in an ASCII workspace.
No model/settings writes. It distinguishes a calculator failure from Unicode
paths or Qt/service plumbing; output parity does not certify the algorithm.
"""
import argparse,json,shutil,subprocess,tempfile,os
from pathlib import Path
import freeship_ui_test as common

def probe(executable,inputs,out,timeout=8,enter=False,output_names=()):
    out.mkdir(parents=True,exist_ok=False)
    with tempfile.TemporaryDirectory(prefix='fs-solver-') as temp:
        work=Path(temp);exe=work/executable.name;shutil.copy2(executable,exe)
        for file in inputs.iterdir():
            if file.is_file() and file.suffix.lower() not in ('.log','.exe') and file.name.rstrip('.').lower() not in {n.rstrip('.').lower() for n in output_names}:shutil.copy2(file,work/file.name)
        for name in ('CONF.PRO','RESIST.CNF'):
            if (executable.parent/name).exists():shutil.copy2(executable.parent/name,work/name)
        database=executable.parent.parent/'BD'
        if database.exists():shutil.copytree(database,work/'BD')
        before={p.name:common.digest_file(p) for p in work.iterdir() if p.is_file()}
        r={'exe':str(executable),'exe_sha256':common.digest_file(executable),'input_sha256':before,'timeout':timeout,'enter':enter}
        try:
            p=subprocess.run([str(exe)],cwd=work,input=b'\r\n' if enter else b'',stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=timeout,creationflags=common.FLAGS)
            r.update(exit_code=p.returncode,stdout=p.stdout.decode('utf8','replace'),stderr=p.stderr.decode('utf8','replace'))
        except subprocess.TimeoutExpired as ex:r.update(timed_out=True,stdout=(ex.stdout or b'').decode('utf8','replace'),stderr=(ex.stderr or b'').decode('utf8','replace'))
        r['output_sha256']={}
        for file in work.iterdir():
            if file.is_file() and file.suffix.lower()!='.exe' and file.stat().st_size<2*1024*1024:
                r['output_sha256'][file.name]=common.digest_file(file);shutil.copy2(file,out/file.name)
    (out/'probe.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');return r
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--inputs',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--enter',action='store_true');a=p.parse_args();r=probe(a.executable.resolve(),a.inputs.resolve(),a.output.resolve(),enter=a.enter)
    print(json.dumps({k:v for k,v in r.items() if k not in ('stdout','stderr','input_sha256','output_sha256')},ensure_ascii=False))
