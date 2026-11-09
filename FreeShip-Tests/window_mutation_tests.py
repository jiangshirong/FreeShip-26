"""Run isolated behavioral mutations, never against the user's source checkout."""
import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys
import time
import freeship_ui_test as common

ROOT=Path(__file__).resolve().parent
EXPECTED={'no_cache':'window.frozen-reuses-model-buffer',
          'blank_erase':'window.erase-handled',
          'no_dirty':'window.redraw-request-coverage',
          'min_overlap':'window.minimize-distinct',
          'no_copybits':'window.position-no-copy-bits',
          'snap_disabled':'window.snap-neighbor',
          'label_whitespace':'window.motion-start-all',
          'no_time_compensation':'window.timer-pause-compensation'}

def main():
    base=ROOT/'reports'/('window-mutations-'+time.strftime('%Y%m%d-%H%M%S'));base.mkdir(parents=True)
    source_hash,_=common.fingerprint(common.DEFAULT_SOURCE)
    script_hash=common.digest_file(Path(__file__))
    def trial(item):
        mutation,expected=item;out=base/mutation
        p=subprocess.run([sys.executable,str(ROOT/'freeship_window_test.py'),'--mutation',mutation,
                          '--output',str(out),'--timeout','240'],capture_output=True,timeout=300)
        path=out/'report.json'; data=json.loads(path.read_text(encoding='utf8')) if path.exists() else {}
        match=next((c for c in data.get('cases',[]) if c.get('id')==expected),None)
        wanted = 'pass' if mutation=='label_whitespace' else 'fail'
        ok=bool(data.get('source_unchanged_during_run') and data.get('source_sha256')==source_hash and
                data.get('mutated_snapshot_sha256')!=data.get('source_snapshot_sha256') and
                match and match.get('status')==wanted and p.returncode in (1,2))
        return {'mutation':mutation,'expected_case':expected,'expected_status':wanted,'detected':ok,'case':match,
                'status':data.get('status'),'error':data.get('error'),'exit_code':p.returncode,'report':str(path)}
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool: trials=list(pool.map(trial,EXPECTED.items()))
    stable=common.fingerprint(common.DEFAULT_SOURCE)[0]==source_hash and common.digest_file(Path(__file__))==script_hash
    uncovered=[{'mutation':'inherited_paint_preframe','status':'not_run',
                'reason':'a hidden final buffer cannot establish whether an earlier inherited paint flashed on screen'},
               {'mutation':'non_atomic_physical_drag','status':'not_run',
                'reason':'the actual pointer/capture path is intentionally not driven by desktop input'}]
    payload={'status':'partial' if stable and all(x['detected'] for x in trials) else 'fail',
             'source_sha256':source_hash,'script_sha256':script_hash,'source_and_script_unchanged_during_run':stable,'trials':trials,'not_run':uncovered,
             'limitations':'Mutations affect only temporary copies; behavioral regressions are detected in hidden probes. label_whitespace checks that an unrelated label instrumentation anchor does not block this window suite.'}
    path=base/'mutation-summary.json';path.write_text(json.dumps(payload,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':payload['status'],'report':str(path),'detected':sum(x['detected'] for x in trials),'total':len(trials)},ensure_ascii=False))
    return 2 if payload['status']=='partial' else 1
if __name__=='__main__':raise SystemExit(main())
