"""Regression for private cache retirement; only generated fixture files.

Checks ownership, evidence retention, unknown content, links and explicit keep.
"""
import argparse
import json
import os
import subprocess
from pathlib import Path
import test_artifacts as storage


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    cases = []
    def check(name, good):
        cases.append(dict(id=name, status='pass' if good else 'fail'))
    def run(name):
        root = out/name
        root.mkdir()
        token = storage.claim(root)
        (root/'report.json').write_text('{"status":"pass"}', encoding='utf8')
        cache = root/'nested/native-lines-cache'/('a'*64)
        cache.mkdir(parents=True)
        for file in storage.CACHE_FILES:
            (cache/file).write_bytes(b'cached fixture')
        return root, token, cache
    try:
        config={'report_roots':[str(out/'drive-a'),str(out/'drive-b')], 'minimum_free_gib':1}
        chosen, _=storage.choose_report_root(config,lambda p:3*2**30 if p.name=='drive-b' else 2*2**30)
        check('default-prefers-more-free-space', chosen.name=='drive-b')
        try:
            storage.choose_report_root(config,lambda p:1024)
            check('low-space-rejected',False)
        except ValueError:
            check('low-space-rejected',True)
        chosen,_=storage.choose_report_root({'report_root':str(out/'legacy')},lambda p:2**31)
        check('single-root-config-compatible',chosen.name=='legacy')
        root, token, cache = run('completed')
        evidence = {name:b'evidence '+name.encode() for name in ('scene.fslp','model.json','reference.dxf','report.json','test.py','image.bmp')}
        for name, data in evidence.items():
            (root/name).write_bytes(data)
        result = storage.finish(root, token)
        check('only-cache-removed', result['removed_files']==5 and not cache.exists())
        check('published-evidence-unchanged', all((root/n).read_bytes()==b for n,b in evidence.items()))
        check('receipt-retained', (root/'artifact-retention.json').is_file())
        check('repeat-is-empty', storage.finish(root, token)['removed_files']==0)
        root, token, cache = run('keep')
        check('keep-switch', storage.finish(root, token, keep=True)['removed_files']==0 and (cache/'scene.fslp').is_file())
        root, token, cache = run('unknown')
        (cache/'user-model.fbm').write_bytes(b'unrecognized')
        result=storage.finish(root, token)
        check('unknown-content-keeps-whole-cache', result['removed_files']==0 and len(result['skipped'])==1 and (cache/'scene.fslp').exists())
        root, token, cache = run('wrong-token')
        try:
            storage.finish(root,'wrong')
            check('wrong-token-rejected',False)
        except ValueError:
            check('wrong-token-rejected', (cache/'scene.fslp').exists())
        nonempty=out/'nonempty';nonempty.mkdir();(nonempty/'sample.fbm').write_bytes(b'keep')
        try:
            storage.claim(nonempty)
            check('existing-output-not-claimed', False)
        except ValueError:
            check('existing-output-not-claimed', True)
        root, token, cache = run('missing-report')
        (root/'report.json').unlink()
        try:
            storage.finish(root,token)
            check('unfinished-run-kept',False)
        except FileNotFoundError:
            check('unfinished-run-kept',(cache/'scene.fslp').exists())
        root, token, cache = run('links')
        external=out/'external/native-lines-cache'/('b'*64);external.mkdir(parents=True);(external/'scene.fslp').write_bytes(b'external evidence')
        try:
            os.symlink(external.parent,root/'linked-cache',target_is_directory=True)
            os.symlink(external/'scene.fslp',cache/'extra-link')
        except OSError:
            # Directory junctions are the common Windows hazard and do not
            # require the symbolic-link privilege. Only private fixture paths.
            def junction(link,target):
                if link.exists() or link.is_symlink():
                    return
                quote=lambda x:"'"+str(x).replace("'","''")+"'"
                command='New-Item -ItemType Junction -Path '+quote(link)+' -Target '+quote(target)+' | Out-Null'
                subprocess.run(['powershell','-NoProfile','-Command',command],check=True,capture_output=True,
                               creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
            junction(root/'linked-cache',external.parent)
            junction(cache/'extra-link',external.parent)
        if (root/'linked-cache').exists():
            result=storage.finish(root,token)
            check('directory-link-not-traversed',(external/'scene.fslp').read_bytes()==b'external evidence' and len(result['skipped'])==2)
            check('linked-child-keeps-whole-cache',(cache/'scene.fslp').is_file() and result['removed_files']==0)
    except Exception as ex:
        cases.append(dict(id='execution',status='error',reason=f'{type(ex).__name__}: {ex}'))
    counts={s:sum(c['status']==s for c in cases) for s in ('pass','fail','review','not_run','error')}
    report=dict(status='error' if counts['error'] else 'fail' if counts['fail'] else 'partial' if counts['not_run'] else 'pass',cases=cases,counts=counts,
                scope='Storage fixture operations only; no application behavior claim')
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(counts=counts,report=str(out/'report.json'))))
    return int(bool(counts['error'] or counts['fail']))


if __name__ == '__main__':
    raise SystemExit(main())
