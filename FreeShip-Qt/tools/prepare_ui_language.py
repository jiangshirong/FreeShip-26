"""Retained migration tool: wrap UI-owned CJK literals and reuse existing PO text.

No machine translation. Ambiguous reverse mappings are omitted. Use --wrap only
for the reviewed UI source files; --catalog regenerates packs and missing lists.
"""
import argparse,ast,hashlib,json,re
import os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
LEX=re.compile(r'R"(?P<delim>\w*)\(.*?\)(?P=delim)"|//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',re.S)
CJK=re.compile('[\u3400-\u9fff]')
NAMES={'en':'English','ru':'Русский','de':'Deutsch','es':'Español','fr':'Français','ja':'日本語'}
def files():
    return [p for folder in ['app','render'] for p in (ROOT/'src'/folder).glob('*.cpp') if not p.stem.endswith('_workflow') and p.name not in ('ui_language.cpp','model_session.cpp','file_history.cpp','preferences_store.cpp')]+[ROOT/'src/view.cpp']
def po(path):
    result={}
    for block in path.read_text('utf-8-sig').split('\n\n'):
        if '#, fuzzy' in block:continue
        pair=re.search(r'^msgid (.*?)(?=^msgstr )',block,re.M|re.S);val=re.search(r'^msgstr (.*)',block,re.M|re.S)
        if pair and val:
            decode=lambda s:''.join(ast.literal_eval(x) for x in re.findall(r'^".*"$',s,re.M))
            k,v=decode(pair[1]),decode(val[1])
            if k and v:result[k]=v
    return result
def norm(s):return re.sub(r'\(&[A-Za-z]\)','',s).replace('&','').strip('| .…\n\t:：')
def sources():
    found={}
    for path in files():
        text=path.read_text('utf-8-sig')
        for m in LEX.finditer(text):
            if m[0].startswith('"') and CJK.search(m[0]):
                try:s=ast.literal_eval(m[0])
                except (ValueError,SyntaxError):continue
                found.setdefault(s,[]).append(f'{path.relative_to(ROOT).as_posix()}:{text.count(chr(10),0,m.start())+1}')
    # Qt standard buttons are supplied through QTranslator, not UI builders.
    helper=(ROOT/'src/app/ui_language.cpp').read_text('utf-8')
    for s in ('确定','取消','保存','打开','关闭','应用','是','否','帮助'):
        at=helper.find('"'+s+'"');found.setdefault(s,[]).append('src/app/ui_language.cpp:'+str(helper.count('\n',0,at)+1))
    return found
def main():
    p=argparse.ArgumentParser();p.add_argument('--wrap',action='store_true');p.add_argument('--catalog',action='store_true');a=p.parse_args()
    if a.wrap:
        changed={}
        for path in files():
            text=path.read_text('utf-8-sig');count=0
            def wrap(m):
                nonlocal count
                if not m[0].startswith('"') or not CJK.search(m[0]):return m[0]
                prefix=text[max(0,m.start()-30):m.start()]
                if re.search(r'(uiText|QStringLiteral)\(\s*$',prefix):return m[0]
                count+=1;return 'uiText('+m[0]+')'
            new=LEX.sub(wrap,text)
            if count:
                if '#include "app/ui_language.h"' not in new and '#include "ui_language.h"' not in new:new='#include "app/ui_language.h"\n'+new
                path.write_text(new,'utf-8');changed[str(path.relative_to(ROOT))]=count
        print(json.dumps({'wrapped':changed},ensure_ascii=False))
    if a.catalog:
        locale=Path(os.environ.get('FREESHIP_LEGACY_SOURCE') or ROOT.parent/'FreeShip-Plus-Lazarus')/'Locale';cn=po(locale/'FreeShip.cn.po');index={}
        for key,value in cn.items():index.setdefault(norm(value),set()).add(key)
        # Explicit aliases to existing source IDs; no new target-language prose.
        aliases={'首选项':'Preferences','界面语言 / Language':'Language','启用双语言切换 (Ctrl+Alt+L)':'Enable (Ctrl+Alt+L)',
                 '第一语言':'First language','第二语言':'Second language','打开模型…':'Open','工程设置':'Project settings',
                 '图形':'Graphics','目录':'Directories','绘图配色':'Theme','全图':'Zoom all','视图':'View',
                 '确定':'OK','取消':'Cancel','是':'Yes','关闭':'Close','应用':'Apply','打开':'Open',
                 '文件(&F)':'File','计算(&C)':'Calculations','新建…':'New...','关于':'About',
                 '点':'Point','边':'Edge','面':'Face','图层':'Layer','重做':'Redo',
                 '切换语言 / Language':'Switch interface language'}
        englishKeys={norm(k).lower():k for k in cn}
        src=sources();packs={};catalogRoot=ROOT/'src/app/languages';catalogRoot.mkdir(exist_ok=True)
        provenance={'source_po_sha256':{},'entries':{},'missing':{}}
        for code,name in NAMES.items():
            path=locale/f'FreeShip.{code}.po';target=po(path);messages={};refs={}
            provenance['source_po_sha256'][path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
            for s in src:
                keys=index.get(norm(s),set());key=next(iter(keys)) if len(keys)==1 else None
                if s in aliases:key=englishKeys.get(norm(aliases[s]).lower(),key)
                if not key:continue
                v=target.get(key,key) if code=='en' else target.get(key,'')
                # Resource identifiers are not English captions (e.g. DeleteSelected).
                if code=='en' and re.search(r'[a-z][A-Z]',v) and key not in target:continue
                if not v:continue
                # Strip Pascal caption wrappers; retain Qt placeholders unchanged.
                v=v.strip('|');v=re.sub(r'\(&[A-Za-z]\)','',v).replace('&','')
                if sorted(re.findall(r'%L?[1-9][0-9]?|%n',s))!=sorted(re.findall(r'%L?[1-9][0-9]?|%n',v)):continue
                if s.endswith('…') and not v.endswith(('…','...')):v+='…'
                if s.rstrip().endswith(('：',':')) and not v.rstrip().endswith((':','：')):v=v.rstrip()+':'
                # Fragments keep their layout delimiters (e.g. "Import " + filename).
                v=re.match(r'^\s*',s)[0]+v.strip()+re.search(r'\s*$',s)[0]
                messages[s]=v;refs[s]=key
            pack={'format':'FreeShipQtLanguage','version':1,'id':code,'name':name,'messages':messages}
            (catalogRoot/f'{code}.json').write_text(json.dumps(pack,ensure_ascii=False,indent=2)+'\n','utf-8');packs[code]=len(messages)
            provenance['entries'][code]=refs;provenance['missing'][code]=sorted(set(src)-set(messages))
        provenance['source_po_sha256']['FreeShip.cn.po']=hashlib.sha256((locale/'FreeShip.cn.po').read_bytes()).hexdigest()
        (ROOT/'docs/migration/UI-LANGUAGE-SOURCES.json').write_text(json.dumps(provenance,ensure_ascii=False,indent=2)+'\n','utf-8')
        (ROOT/'docs/migration/UI-LANGUAGE-STRINGS.json').write_text(json.dumps(src,ensure_ascii=False,indent=2)+'\n','utf-8')
        print(json.dumps({'source_strings':len(src),'reused_translations':packs}))
if __name__=='__main__':main()
