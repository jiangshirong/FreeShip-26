"""Extract parameter-page ownership from the preserved LFM forms (no formulas).

The mapping restores General / Added data separation, not translated labels or
numeric contracts. Unknown source pages fail regeneration rather than disappear.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'FreeShip-Qt/bridge'))
from performance_adapter import METHODS,lfm_nodes,field_map

def generate():
    methods=METHODS+[(m,'Hydrodyn_'+e,'') for m,e in [('wind','Task1'),('maneuver','Maneuv'),('acceleration','RVRS')]]
    result={};hashes={}
    for method,entry,_ in methods:
        name='Forms/Free'+entry+'Dlg.lfm';raw=(ROOT/'FreeShip-Plus-Lazarus'/name).read_bytes();hashes[name]=hashlib.sha256(raw).hexdigest()
        nodes=lfm_nodes(raw.decode('utf-8-sig'));by={n['name']:n for n in nodes};fields={}
        for key,_,_,_ in field_map(nodes):
            node=by[key]
            while node and node['class']!='TTabSheet':node=node['parent']
            caption=node.get('Caption','').strip("'") if node else ''
            assert caption in ('General','Added data'),(method,key,caption)
            fields[key]='additional' if caption=='Added data' else 'general'
        result[method]=fields
    result['propeller4']['WaterViscosity']='general' # explicit Qt water input
    return result,hashes

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');a=p.parse_args();data,hashes=generate()
    text='// GPL-2.0-or-later. Generated from preserved LFM by generate_calculation_pages.py.\nstatic constexpr auto CalculationPages=R"pages('+json.dumps(data,separators=(',',':'))+')pages";\n'
    for name,value in [('calculation_pages.h',text),('calculation_pages-origin.json',json.dumps(hashes,indent=2,sort_keys=True)+'\n')]:
        path=ROOT/'FreeShip-Qt/src/app'/name
        if a.check:assert path.read_text('utf8')==value,name
        else:path.write_text(value,encoding='utf8')
    print('Calculation page source map: pass')
if __name__=='__main__':main()
