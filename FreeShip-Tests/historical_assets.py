"""Check decoded icon dimensions; preserve inventory without fixed UI counts."""
import argparse
from collections import Counter
import json
from pathlib import Path
import freeship_ui_test as common

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,default=common.DEFAULT_SOURCE)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args(); source=a.source.resolve(); out=a.output.resolve()
    if out==source or source in out.parents:p.error('Output must be outside product source')
    out.mkdir(parents=True,exist_ok=False)
    cases=[]; report={'status':'error','cases':cases,'limitations':[
        'Asset dimensions and decoding only; runtime bindings, glyph fallback and aesthetics require separate evidence.']}
    try:
        from PIL import Image
        root=source/'Themes/StudioLight/toolbar'
        paths=sorted(root.glob('*/*.png'))
        before={str(f.relative_to(source)):common.digest_file(f) for f in paths}
        for f in paths:
            row={'id':str(f.relative_to(source)),'status':'error'}
            try:
                with Image.open(f) as im:
                    im.load(); row.update(width=im.width,height=im.height,mode=im.mode)
                    if f.parent.name.isdecimal():
                        n=int(f.parent.name);row.update(expected=[n,n],status='pass' if im.size==(n,n) else 'fail')
                    else:row.update(status='review',reason='No declared size directory')
            except Exception as e:row['error']=str(e)
            cases.append(row)
        if not paths:cases.append({'id':'assets','status':'not_run','reason':'No assets found'})
        if before!={str(f.relative_to(source)):common.digest_file(f) for f in sorted(root.glob('*/*.png'))}:
            raise RuntimeError('Resources changed during run')
        report.update(status=common.classify_statuses([c['status'] for c in cases]),resource_sha256=before)
    except Exception as e:report['error']=str(e)
    report['summary']=dict(Counter(c['status'] for c in cases))
    (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':report['status'],'summary':report['summary']},ensure_ascii=False))
    return {'pass':0,'partial':2,'fail':1,'error':3}.get(report['status'],2)

if __name__=='__main__':raise SystemExit(main())
