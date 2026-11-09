"""Summarize individual evidence without implying that an issue is fully covered."""
from collections import Counter


def coverage_matrix(cases, manifest):
    result=[]
    for issue in manifest['cases']:
        issue_id=issue['id']
        matching=[c for c in cases if c.get('id','').startswith(issue_id+'.')]
        counts=dict(Counter(c.get('status','error') for c in matching))
        result.append({'id':issue_id,'title':issue['title'],'record':issue['record'],
                       'counts':counts,'cases':[{'id':c.get('id'),'status':c.get('status','error'),
                                                'expected':c.get('expected'),'actual':c.get('actual')} for c in matching],
                       'coverage':'unknown' if not matching else 'incomplete' if any(c.get('status') in ('error','not_run','review') for c in matching) else 'observed',
                       'note':'observed means cases ran, not that all scenarios or the issue are covered'})
    return result
