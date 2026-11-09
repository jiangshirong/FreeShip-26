"""Inspect an export failure using the existing native section probe.

Diagnostic, not a regression oracle: prints raw connected fragment endpoints;
does not automatically classify a gap as acceptable or rewrite the model.
Usage: python -B ghs_cut_diagnostic.py INPUT_JSON PROBE_EXE QT_RUNTIME OUTPUT_DIR
"""
import json,os,subprocess,sys
from pathlib import Path
source,probe,runtime,out=map(Path,sys.argv[1:]);out.mkdir(parents=True,exist_ok=False)
d=json.loads(source.read_text('utf8'));g=d['geometry']
request={'format':'FreeShipNativeSectionsCases','version':1,'cases':[{'id':'ghs-hydro-stations','geometry':g,'level':g['desired_level'],'planes':[{'id':str(s['order']),'coefficients':s['plane'],'hydrostatics_layers_only':True} for s in d['sections'] if s['kind']==1]}]}
(out/'request.json').write_text(json.dumps(request),encoding='utf8');env=os.environ.copy();env['PATH']=str(runtime)+os.pathsep+env['PATH']
subprocess.run([str(probe),str(out/'request.json'),str(out/'result.json')],env=env,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0),check=True)
r=json.loads((out/'result.json').read_text('utf8'));print(r['cases'][0].keys())
for cut in r['cases'][0]['planes']:
    print('station',cut['id'],cut.keys())
    for c in cut['connected_curves']:print(len(c['points']),c['points'][0],c['points'][-1])
