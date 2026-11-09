"""Classify review/not_run evidence without inferring product requirements."""
import argparse
import hashlib
from collections import Counter
import json
from pathlib import Path

def classify(case, cases):
    ident, status = case.get('id',''), case.get('status')
    if status == 'not_run':
        if ident == 'dynamic.coverage':
            return 'dynamic_coverage_limit', 'Unknown callbacks or unconstructed dialogs are not exercised.'
        if ident == 'inventory.coverage':
            return 'inventory_coverage_limit', 'Static snapshots and the separate approved dynamic probe do not cover every possible entry.'
        if ident in ('labels.visual-quality','labels.line-association','panel.image-review'):
            return 'visual_acceptance_limit', 'Offscreen diagnostic data cannot establish real-screen appearance or association.'
        if ident == 'model.preview-real-timer':
            return 'event_loop_limit', 'The real timer and normal application event loop were not exercised.'
        return 'native_interaction_limit', 'Native focus, input, actual window behavior, or real editing is not exercised.'
    if ident.startswith('display.'):
        return 'design_observation', 'State is recorded for AI to compare with current requirements; no fixed entry policy is assumed.'
    if ident in ('panel.layout-overlaps','panel.ancestor-clipping'):
        if ident.endswith('layout-overlaps'):
            return 'geometry_measurement_uncertain', 'Historical overlap used full child bounds; ancestor clipping can remove the apparent overlap.'
        return 'possible_panel_clipping', 'Hidden-control ancestor bounds indicate clipping; actual window rendering remains unverified.'
    if '.layout.clipped-labels.' in ident or ident.endswith('.bounds'):
        return 'edge_clip_review', 'Recorded text bounds cross the viewport; intentional edge clipping or readability must be assessed in context.'
    if ident.endswith(('.baseline','.design-waterline')):
        actual = case.get('actual') or {}
        if isinstance(actual,dict) and actual.get('attempted',0) and not actual.get('visible',0):
            return 'reference_label_visibility_review', 'A reference label was requested but not visible; check its anchor, clipping and requirement.'
        return 'translation_observation', 'A historical fixed translation was inspected; its wording is not a default design verdict.'
    if ident.endswith('.overlap'):
        parent = cases.get(ident.rsplit('.',1)[0],{})
        placements = [p for p in parent.get('placement_trace',[]) if p.get('visible')]
        if placements and not all('clip_rect' in p for p in placements):
            return 'geometry_measurement_uncertain', 'Historical check compared full text bounds, not the actual clipping rectangles.'
        return 'label_intersection_review', 'Recorded clipping rectangles intersect; visual impact and intended arrangement still require review.'
    if ident.endswith('.line-crossings'):
        return 'line_crossing_review', 'Recorded line segments intersect a label region; line association, paint order and readability are not established.'
    if ident.endswith('.omissions') or '.draw.all-labels.' in ident or '.draw.label.' in ident:
        return 'label_omission_review', 'A layout omission is recorded; assess its anchor and the requested UI behavior.'
    return 'manual_review', 'The available evidence requires contextual review.'

def audit(report):
    cases = {c.get('id'):c for c in report.get('cases',[])}
    entries=[]
    for c in report.get('cases',[]):
        if c.get('status') not in ('review','not_run'): continue
        category,reason=classify(c,cases)
        if c.get('id','').startswith('labels.') and '.' in c['id']:
            parent=cases.get(c['id'].rsplit('.',1)[0],{})
            if 'placement_trace' in parent and 'layout_settled' not in parent:
                reason+=' This historical snapshot did not establish whether layout animation had settled.'
        entries.append({'id':c.get('id'),'status':c['status'],'category':category,'reason':reason,
                        'actual':c.get('actual'),'image':c.get('image')})
    return {'report_status':report.get('status'),'source_sha256':report.get('source_sha256'),
            'source_unchanged_during_run':report.get('source_unchanged_during_run'),
            'categories':dict(Counter(x['category'] for x in entries)), 'entries':entries,
            'note':'Categories classify evidence and coverage; they are not a claim that every UI observation is a defect.'}

if __name__ == '__main__':
    ap=argparse.ArgumentParser(); ap.add_argument('report',type=Path); ap.add_argument('output',type=Path); a=ap.parse_args()
    report_bytes=a.report.read_bytes()
    result=audit(json.loads(report_bytes.decode('utf-8-sig')))
    result['report_sha256']=hashlib.sha256(report_bytes).hexdigest()
    result['classifier_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'output':str(a.output.resolve()),'categories':result['categories'],'count':len(result['entries'])},ensure_ascii=False))
