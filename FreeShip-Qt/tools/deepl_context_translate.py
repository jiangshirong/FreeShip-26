"""Resumable English -> DeepL target catalogs, with source-bound English context.

Credentials are read only from the explicitly supplied local key file. They are
never copied to plans, caches, requests, receipts, stdout or exception messages.
No external LLM is used. Conflicting old translations require assistant decisions.
"""
import argparse,collections,concurrent.futures,hashlib,html,json,re,sys,threading,time
from datetime import datetime,timezone
from pathlib import Path
import xml.etree.ElementTree as ET
import requests
QT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(QT.parent/'FreeShip-Tests'))
from qt_text_audit import inventory,digest,TOKENS

VERSION='deepl-context-v1'
TARGETS={'ru':'RU','ja':'JA','de':'DE','fr':'FR','es':'ES'}
KEY_PATTERN=r'\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}(?::fx)?\b'
DOMAIN='FreeShip is desktop ship-hull design and naval-architecture software. The strings below are independent interface labels, messages or technical report text. Use the stated function and physical meaning. Preserve mathematical symbols, placeholders, units and file patterns.'
LOCK=threading.Lock()

def now():return datetime.now(timezone.utc).isoformat()
def load(p):return json.loads(p.read_text('utf8'))
def rows(p):return [json.loads(s) for s in p.read_text('utf8').splitlines() if s.strip()] if p.exists() else []
def write(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n','utf8')
def emit(p,d):
    with LOCK:
        with p.open('a',encoding='utf8') as f:f.write(json.dumps(d,ensure_ascii=False)+'\n');f.flush()
def require_delivery_active(out):
    hold=out/'delivery-hold.json'
    if hold.exists() and load(hold).get('active'):
        raise ValueError('User requested preservation while source editing continues. Reconcile the new source/context revision and explicitly release the recorded hold before new translation calls or application.')
def require_deepl_enabled(out):
    for policy in (out/'translation-authoring-policy.json',QT/'docs/migration/TRANSLATION-AUTHORING-POLICY.json'):
        if policy.exists() and load(policy).get('automatic_paid_MT_enabled') is False:
            raise ValueError('User selected direct translation by the current repository assistant. New DeepL translation requests are disabled; existing results remain available as reference.')
def key_for(binding):return (binding.get('catalog_context')+'\x04' if binding.get('catalog_context') else '')+binding['source_key']

def prepare(revision,out,targets):
    if (out/'plan.json').exists():raise ValueError('A plan already exists; resume it rather than rebuilding.')
    mf=load(revision/'manifest.json');current=inventory(QT)
    if current['errors'] or current['files']!=mf['source_files']:raise ValueError('Reviewed source snapshot is stale')
    english_path=QT/'src/app/languages/en.json'
    if digest(english_path.read_bytes())!=mf['english_sha256']:raise ValueError('English catalog is stale')
    english=load(english_path)['messages'];bykey=collections.defaultdict(list)
    for u in rows(revision/'translation_units.deduplicated.jsonl'):
        for binding in u['source_bindings']:bykey[key_for(binding)].append(u)
    for u in rows(revision/'generated_translation_units.jsonl'):bykey[u['source_key']].append(u)
    jobs=[];resolutions=[];extra=[]
    for key in sorted(set(bykey)|set(english)):
        uses={u['id']:u for u in bykey[key]};values={u['text'] for u in uses.values()}
        if key in english:source=english[key]
        elif len(values)==1:source=next(iter(values))
        elif key in {'Invalid coordinate','Invalid extraction flag'}:
            source=key;resolutions.append(dict(source_key=key,canonical_en=source,variants=sorted(values),reason_en='The shared source describes the same broad validation failure at all these sites. Retain the generic source wording; the context includes the more specific causes. A period is not a semantic distinction.'))
        else:raise ValueError('Conflicting canonical English needs explicit resolution: '+key)
        contexts=list(dict.fromkeys(u['context'] for u in uses.values()))
        roles=sorted({u['role'] for u in uses.values()});dependencies={f:h for u in uses.values() for f,h in u.get('dependency_files',{}).items()}
        if not contexts:
            if key.startswith("'") and key.endswith("'"):
                context='Choice caption naming a hull appendage in the Holtrop appendage-resistance inputs. Preserve the quotation marks of this inherited caption. The noun names the appendage shape, not a command.'
            elif key.startswith('Cx'):
                context='Legend listing aerodynamic result coefficients for a ship. Parenthetical words distinguish drag and lift. Preserve Cx, Cy, m_z, Cd and 0.1*K exactly.'
            elif key=='横向受风投影面积 [m2]':
                context='Input label for the wind-exposed projected area in the merchant-ship aerodynamics calculator, expressed in square metres. Preserve the unit m².'
            else:raise ValueError('English catalog member has no context: '+key)
            contexts=[context];roles=['field'];extra.append(dict(source_key=key,canonical_en=source,context_en=context,reason_en='Supplemental source-bound alias or generated legend already present in the English catalog; context added for this translation handoff.'))
        context='\n'.join(contexts)
        units=sorted({s for u in uses.values() for s in u.get('constraints',{}).get('unit_symbols',[])})
        protected=sorted({s for u in uses.values() for s in u.get('constraints',{}).get('protected_spans',[])})
        identity_data=[VERSION,source,context,roles,units]
        if protected:identity_data.append(protected)
        identity=digest(json.dumps(identity_data,ensure_ascii=False,sort_keys=True).encode())
        jobs.append(dict(id='deepl.'+identity[:24],source_keys=[key],text=source,context=context,roles=roles,unit_symbols=units,protected_spans=protected,use_site_ids=sorted({i for u in uses.values() for i in u.get('use_site_ids',[])}),unit_ids=sorted(uses),dependency_files=dependencies,domain=next(iter(dependencies),'supplemental')))
    # Identical effective inputs may share one paid translation, even if keys differ.
    dedup={}
    for job in jobs:
        if job['id'] not in dedup:dedup[job['id']]=job
        else:
            d=dedup[job['id']];d['source_keys']+=job['source_keys'];d['use_site_ids']=sorted(set(d['use_site_ids']+job['use_site_ids']));d['unit_ids']=sorted(set(d['unit_ids']+job['unit_ids']));d['dependency_files'].update(job['dependency_files'])
    out.mkdir(parents=True,exist_ok=True);(out/'preimage').mkdir(exist_ok=True)
    catalogs={}
    for locale in ['en']+targets:
        p=QT/f'src/app/languages/{locale}.json';(out/'preimage'/p.name).write_bytes(p.read_bytes());catalogs[locale]=dict(sha256=digest(p.read_bytes()),entries=len(load(p)['messages']))
    plan=dict(version=VERSION,created_utc=now(),revision=str(revision),source_files=mf['source_files'],english_sha256=mf['english_sha256'],targets={k:TARGETS[k] for k in targets},catalogs=catalogs,jobs=list(dedup.values()),source_key_resolutions=resolutions,supplemental_contexts=extra,blocked_evidence=str(revision/'evidence_insufficient.jsonl'),policy='DeepL with functional English context; missing old translation accepts structurally valid DeepL output; different old translation requires a recorded repository-assistant decision. No Alibaba LLM or other external LLM.',Chinese_policy='Chinese has not had an independent complete linguistic review. Chinese is not a target of this five-language run and is not assumed authoritative.')
    write(out/'plan.json',plan);write(out/'summary.json',dict(status='prepared',jobs=len(dedup),source_keys=len(bykey.keys()|english.keys()),English_characters_per_language=sum(len(j['text']) for j in dedup.values()),targets=targets,conflict_reviewer='current repository assistant',translation_api_calls=0))
    print(json.dumps(load(out/'summary.json'),ensure_ascii=False))

# Protect format contracts rather than asking an MT engine to obey instructions.
SYMBOLS=r'(?:LWL|LPP|LOA|LCB|LCF|COG|DWL|CWP|CB|CM|CP|KM|BM|GM|KG|KB|Cx|Cy|Cb|Cm|Cp|Cwp|Fn|Fr|Re|Rt|Rf|Rr|Pe|Pd|Ae/Ao|P/D|m_z|0\.1\*K|Wt|EtaH|EtaR|KT|KQ|DKT|DKQ)'
UNITS=r'(?:long ton|kg|kN|kW|kPa|MPa|Pa|mm|cm|km|m|ft|in|kn|rad|rpm|Hz|s|t|N|W|hp|lb|lbs|°C|°|%)(?:[²³⁴]|\^[234]|[234])?(?:[/·*](?:kg|mm|cm|m|ft|s|N|rad)(?:[²³⁴]|\^[234]|[234])?)*'
PROTECTED=re.compile(r'%L?[1-9][0-9]?|%n|×?10[⁻⁰¹²³⁴⁵⁶⁷⁸⁹]+|\r\n|\n|\t| {2,}|\*?\.[A-Za-z][A-Za-z0-9]{1,7}\b|(?<![\w\'’])'+SYMBOLS+r'(?![\w\'’])|(?<![\w\'’])'+UNITS+r'(?![\w\'’])|(?<![\w])\d+(?:[.,]\d+)*(?:[eE][+-]?\d+)?(?![\w])')
def legacy_encode(source,extra_symbols=()):
    tokens=[];at=0;parts=[]
    extras=sorted({s for s in extra_symbols if s},key=len,reverse=True)
    pattern=re.compile(r'(?<![\w\'’])(?:'+'|'.join(re.escape(s) for s in extras)+r')(?![\w\'’])|'+PROTECTED.pattern) if extras else PROTECTED
    for m in pattern.finditer(source):
        parts.append(html.escape(source[at:m.start()]));parts.append(f'<ph id="{len(tokens)}">'+html.escape(m[0])+'</ph>');tokens.append(m[0]);at=m.end()
    parts.append(html.escape(source[at:]));return '<t>'+''.join(parts)+'</t>',tokens
def legacy_decode(value,source,tokens):
    root=ET.fromstring(value)
    seen=[]
    for p in root.iter('ph'):
        index=int(p.attrib['id'])
        if index<0 or index>=len(tokens):raise ValueError('Invalid protected token')
        seen.append(index);p.text=tokens[index]
    if sorted(seen)!=list(range(len(tokens))):raise ValueError('Protected token missing or duplicated')
    text=''.join(root.itertext())
    leading=re.match(r'^\s*',source)[0];trailing=re.search(r'\s*$',source)[0]
    text=leading+text.strip()+trailing if source.strip() else source
    if sorted(TOKENS.findall(text))!=sorted(TOKENS.findall(source)):raise ValueError('Placeholder contract failed')
    return text
SEGMENT_PROTECTED=re.compile(r'\$[^$\r\n]+\$|%L?[1-9][0-9]?|%n|×?10[⁻⁰¹²³⁴⁵⁶⁷⁸⁹]+|\b[A-Za-z][A-Za-z0-9]*_[A-Za-z0-9_]+\b|\*?\.[A-Za-z][A-Za-z0-9]{1,7}\b|(?<![\w\'’])'+SYMBOLS+r'(?![\w\'’])|(?<![\w\'’])'+UNITS.replace('|in|','|')+r'(?![\w\'’])|(?<![\w])\d+(?:[.,]\d+)*(?:[eE][+-]?\d+)?(?![\w])')
def encode(source,extra_symbols=()):
    # Keep table borders, columns, indentation and line breaks local. The MT
    # engine receives one explicit XML text segment per language-bearing cell.
    pieces=re.split(r'(\r\n|\n|\t|\|| {2,})',source);parts=[];tokens=[];xml=[]
    extras=sorted({s for s in extra_symbols if s and s!='in'},key=len,reverse=True)
    if re.fullmatch(r'[\[(]?in[\])]?',source.strip()):extras.append('in')
    pattern=re.compile(r'(?<![\w\'’])(?:'+'|'.join(re.escape(s) for s in extras)+r')(?![\w\'’])|'+SEGMENT_PROTECTED.pattern) if extras else SEGMENT_PROTECTED
    for value in pieces:
        if not value:continue
        part=dict(raw=value,translate=False);at=0;bits=[]
        if re.fullmatch(r'\s*[A-Za-z]\s*',value):parts.append(part);continue
        for m in pattern.finditer(value):
            bits.append(html.escape(value[at:m.start()]));bits.append(f'<ph id="{len(tokens)}">'+html.escape(m[0])+'</ph>');tokens.append(m[0]);at=m.end()
        bits.append(html.escape(value[at:]));body=''.join(bits)
        rest=re.sub(r'<ph\b[^>]*>.*?</ph>','',body,flags=re.S)
        if re.search(r'[A-Za-z\u3400-\u9fff]',html.unescape(rest)):
            part['translate']=True;part['id']=len(parts);part['token_ids']=[int(x) for x in re.findall(r'<ph id="(\d+)"',body)];xml.append(f'<seg id="{len(parts)}">'+body+'</seg>')
        parts.append(part)
    return '<root>'+''.join(xml)+'</root>',dict(parts=parts,tokens=tokens,submitted_token_ids=[i for p in parts for i in p.get('token_ids',[])])

def decode(value,source,info):
    root=ET.fromstring(value);segments={};seen_tokens=[]
    for seg in root.iter('seg'):
        index=int(seg.attrib['id'])
        if index in segments:raise ValueError('Duplicated text segment')
        local=[]
        for ph in seg.iter('ph'):
            token=int(ph.attrib['id']);seen_tokens.append(token);local.append(token)
            if token<0 or token>=len(info['tokens']):raise ValueError('Invalid protected token')
            ph.text=info['tokens'][token]
        if index<0 or index>=len(info['parts']) or sorted(local)!=sorted(info['parts'][index].get('token_ids',[])):raise ValueError('A protected value moved between cells')
        segments[index]=''.join(seg.itertext())
    expected={i for i,p in enumerate(info['parts']) if p['translate']}
    if set(segments)!=expected:raise ValueError('Text segment missing or duplicated')
    # Numeric-only cells have no XML segment and their tokens need no response.
    # Validate token occurrences in each actually submitted text segment.
    if len(seen_tokens)!=len(set(seen_tokens)):raise ValueError('Protected token duplicated')
    text=''
    for i,part in enumerate(info['parts']):
        if not part['translate']:text+=part['raw'];continue
        raw=part['raw'];leading=re.match(r'^\s*',raw)[0];trailing=re.search(r'\s*$',raw)[0]
        text+=leading+segments[i].strip()+trailing
    if sorted(TOKENS.findall(text))!=sorted(TOKENS.findall(source)):raise ValueError('Placeholder contract failed')
    # Check literal numerals and units by per-segment required token IDs.
    for token in info.get('submitted_token_ids',[]):
        if token not in seen_tokens:raise ValueError('Protected token missing')
    return text

def job_symbols(job):return job['unit_symbols']+job.get('protected_spans',[])
def identity_text(job):return not any(p['translate'] for p in encode(job['text'],job_symbols(job))[1]['parts'])

class Credentials:
    def __init__(self,path):
        content=path.read_text('utf-8-sig');secrets=list(dict.fromkeys(re.findall(KEY_PATTERN,content)));del content
        if not secrets:raise ValueError('No DeepL credentials found in the local file')
        self.items=[];self.lock=threading.Lock();self.index=0
        for i,secret in enumerate(secrets,1):
            host='https://api-free.deepl.com' if secret.endswith(':fx') else 'https://api.deepl.com'
            for attempt in range(5):
                response=requests.get(host+'/v2/usage',headers={'Authorization':'DeepL-Auth-Key '+secret},timeout=35)
                if response.status_code!=429 and response.status_code<500:break
                time.sleep(min(12,2**attempt))
            if response.status_code!=200:continue
            d=response.json();remaining=min(d.get('character_limit',0)-d.get('character_count',0),d.get('api_key_character_limit',10**12)-d.get('api_key_character_count',0))
            if remaining>0:self.items.append(dict(alias=f'K{i}',secret=secret,host=host,remaining=remaining,last=0,disabled=False))
        if not self.items:raise ValueError('No official DeepL key currently has usable quota')
    def reserve(self,count):
        with self.lock:
            for _ in self.items:
                item=self.items[self.index%len(self.items)];self.index+=1
                if not item['disabled'] and item['remaining']>=count:
                    item['remaining']-=count;return item
        raise ValueError('All validated key quotas are exhausted')

def packets(jobs,size):
    groups=collections.defaultdict(list)
    for j in jobs:groups[j['domain']].append(j)
    result=[]
    for items in groups.values():
        pending=[];texts=set();budget=0
        for j in items:
            cost=len((j['text']+j['context']).encode('utf8'))*2+500
            if pending and (len(pending)>=size or j['text'] in texts or budget+cost>105000):result.append(pending);pending=[];texts=set();budget=0
            pending.append(j);texts.add(j['text']);budget+=cost
        if pending:result.append(pending)
    return result

def run(out,key_file,workers,batch_size,limit,selection=None):
    require_delivery_active(out)
    require_deepl_enabled(out)
    plan=load(out/'plan.json');cache=out/'results.jsonl';finished=set(valid_results(out))
    glossaries={x['key_alias']:x['glossary_id'] for x in load(out/'glossary-identities.json')['keys'] if x['status']==201} if (out/'glossary-identities.json').exists() else {}
    terminology_sha256=digest((out/'approved-terminology.json').read_bytes()) if glossaries else None
    credentials=Credentials(key_file);tasks=[];counts=collections.Counter()
    for locale,target in plan['targets'].items():
        remaining=[j for j in plan['jobs'] if (locale,j['id']) not in finished]
        if selection is not None:remaining=[j for j in remaining if j['id'] in selection]
        if limit:
            old=load(out/'preimage'/f'{locale}.json')['messages'];chosen=[];seen=set()
            def take(candidates,n):
                for j in candidates:
                    if j['id'] in seen:continue
                    chosen.append(j);seen.add(j['id']);n-=1
                    if n<=0:break
            take([j for j in remaining if any(k in old for k in j['source_keys'])],min(12,limit))
            for role in ('action','field','heading','error','warning','explanation','filter','unit','feedback','axis_title'):
                if len(chosen)>=limit:break
                take([j for j in remaining if role in j['roles']],min(3,limit-len(chosen)))
            if len(chosen)<limit:take(remaining,limit-len(chosen))
            remaining=chosen
        paid=[]
        for j in remaining:
            if identity_text(j):emit(cache,dict(locale=locale,job_id=j['id'],status='ok',translation=j['text'],provider='protected_invariant',billed_characters=0,created_utc=now()))
            else:paid.append(j)
        tasks.extend((locale,target,p) for p in packets(paid,batch_size))
    write(out/'run-state.json',dict(status='running',started_utc=now(),packets=len(tasks),workers=workers,batch_size=batch_size,already_cached=len(finished)))
    def translate(task):
        locale,target,jobs=task;texts=[];protected=[]
        for j in jobs:
            xml,tokens=encode(j['text'],job_symbols(j));tokens['submitted_token_ids']=[int(x) for x in re.findall(r'<ph id="(\d+)"',xml)];texts.append(xml);protected.append(tokens)
        context=DOMAIN+'\n\n'+('\n\n'.join('For the source text '+json.dumps(j['text'],ensure_ascii=False)+':\n'+j['context'] for j in jobs) if len(jobs)>1 else jobs[0]['context'])
        payload=dict(text=texts,source_lang='EN',target_lang=target,context=context,model_type='prefer_quality_optimized',tag_handling='xml',tag_handling_version='v2',ignore_tags=['ph'],splitting_tags=['seg'],outline_detection=False,split_sentences='nonewlines',preserve_formatting=True,show_billed_characters=True)
        encoded=json.dumps(payload,ensure_ascii=False).encode('utf8')
        if len(encoded)>128*1024:raise ValueError('Request exceeds official body size limit')
        request_id=digest(encoded);count=sum(len(j['text']) for j in jobs)
        for attempt in range(6):
            item=credentials.reserve(count)
            if glossaries:
                if item['alias'] not in glossaries:raise ValueError('Selected key lacks the approved terminology glossary')
                payload['glossary_id']=glossaries[item['alias']];encoded=json.dumps(payload,ensure_ascii=False).encode('utf8');request_id=digest(encoded)
            try:
                started=time.monotonic();response=requests.post(item['host']+'/v2/translate',data=encoded,headers={'Authorization':'DeepL-Auth-Key '+item['secret'],'Content-Type':'application/json'},timeout=(15,120))
                if response.status_code in (403,456):
                    with credentials.lock:item['disabled']=True
                    emit(out/'request-receipts.jsonl',dict(request_id=request_id,key_alias=item['alias'],locale=locale,status=response.status_code,attempt=attempt));continue
                if response.status_code==429 or response.status_code>=500:
                    with credentials.lock:item['remaining']+=count
                    time.sleep(min(20,2**attempt));continue
                if response.status_code!=200:
                    error={'request_id':request_id,'locale':locale,'status':response.status_code,'code':response.json().get('code') if response.headers.get('Content-Type','').startswith('application/json') else None}
                    emit(out/'request-errors.jsonl',error);return dict(error=True,**error)
                result=response.json()['translations']
                if len(result)!=len(jobs):raise ValueError('Wrong translation response count')
                for j,tokens,translated in zip(jobs,protected,result):
                    try:text=decode(translated['text'],j['text'],tokens)
                    except Exception as ex:
                        emit(out/'structural-review.jsonl',dict(locale=locale,job_id=j['id'],provider_text=translated['text'],error_type=type(ex).__name__,request_id=request_id));continue
                    emit(cache,dict(locale=locale,job_id=j['id'],status='ok',translation=text,provider='DeepL',request_id=request_id,codec='segmented-xml-v2',glossary_id=payload.get('glossary_id'),terminology_sha256=terminology_sha256,model_type_used=translated.get('model_type_used'),billed_characters=translated.get('billed_characters'),created_utc=now()))
                emit(out/'request-receipts.jsonl',dict(request_id=request_id,locale=locale,key_alias=item['alias'],endpoint=item['host'],status=200,glossary_id=payload.get('glossary_id'),terminology_sha256=terminology_sha256,job_ids=[j['id'] for j in jobs],source_characters=count,provider_billed_characters=sum(x.get('billed_characters',0) for x in result),elapsed_s=round(time.monotonic()-started,3),context_sha256=digest(context.encode()),context_per_source_explicit=True,batch_size=len(jobs)))
                return dict(locale=locale,ok=len(jobs))
            except requests.RequestException as ex:
                emit(out/'request-errors.jsonl',dict(request_id=request_id,locale=locale,key_alias=item['alias'],error_type=type(ex).__name__,billing_outcome='unknown',attempt=attempt));time.sleep(min(10,2**attempt))
        return dict(locale=locale,error=True,reason='retry budget exhausted')
    done=0
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
        for result in executor.map(translate,tasks):
            done+=1;counts['errors' if result.get('error') else 'ok_packets']+=1
            if done%25==0 or done==len(tasks):
                write(out/'run-state.json',dict(status='running' if done<len(tasks) else 'finished',completed_packets=done,total_packets=len(tasks),counts=dict(counts),updated_utc=now()))
                print(json.dumps(dict(completed_packets=done,total_packets=len(tasks),counts=dict(counts))),flush=True)
    reconcile(out)

def valid_results(out):
    invalid={(x['locale'],x['job_id'],x.get('request_id')) for x in load(out/'cache-invalidations.json').get('results',[])} if (out/'cache-invalidations.json').exists() else set()
    sensitive=set(load(out/'terminology-job-selection.json')) if (out/'glossary-identities.json').exists() else set()
    wanted=digest((out/'approved-terminology.json').read_bytes()) if sensitive else None
    return {(x['locale'],x['job_id']):x for x in rows(out/'results.jsonl') if x.get('status')=='ok' and (x['locale'],x['job_id'],x.get('request_id')) not in invalid and (x['provider']!='DeepL' or x['job_id'] not in sensitive or x.get('terminology_sha256')==wanted)}

def reconcile(out):
    plan=load(out/'plan.json');cache=valid_results(out);conflicts=[];automatic=[];missing=[]
    for locale in plan['targets']:
        old=load(out/'preimage'/f'{locale}.json')['messages']
        for job in plan['jobs']:
            result=cache.get((locale,job['id']))
            if result is None:missing.append(dict(locale=locale,job_id=job['id']));continue
            for key in job['source_keys']:
                candidate=result['translation'];before=old.get(key)
                record=dict(id=digest((locale+'\0'+key).encode())[:24],locale=locale,source_key=key,canonical_en=job['text'],context_en=job['context'],roles=job['roles'],old=before,deepl=candidate,job_id=job['id'],unit_ids=job['unit_ids'])
                if not before:record.update(action='fill_missing',final=candidate,reason_en='No old translation exists; use the structurally checked DeepL result as explicitly requested.');automatic.append(record)
                elif before==candidate:record.update(action='retain_equal',final=before,reason_en='The old translation and the contextual DeepL result are identical.');automatic.append(record)
                else:conflicts.append(record)
    for filename,records in [('automatic.jsonl',automatic),('conflicts.jsonl',conflicts),('missing-results.jsonl',missing)]:
        (out/filename).write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in records),'utf8')
    write(out/'reconciliation-summary.json',dict(automatic=len(automatic),conflicts=len(conflicts),missing=len(missing),per_locale={locale:dict(automatic=sum(x['locale']==locale for x in automatic),conflicts=sum(x['locale']==locale for x in conflicts),missing=sum(x['locale']==locale for x in missing)) for locale in plan['targets']}));print(json.dumps(load(out/'reconciliation-summary.json')),flush=True)

def propose(out):
    reconcile(out);plan=load(out/'plan.json');decisions={d['id']:d for d in rows(out/'assistant-decisions.jsonl')};final=rows(out/'automatic.jsonl');pending=[];stale=[]
    for row in rows(out/'conflicts.jsonl'):
        d=decisions.get(row['id'])
        if not d:pending.append(row);continue
        if any(d[k]!=digest(row[field].encode()) for k,field in [('old_sha256','old'),('deepl_sha256','deepl'),('context_sha256','context_en')]):stale.append(row['id']);continue
        if not d.get('reason_zh') or d['reviewer']!='repository_assistant_current_chat':raise ValueError('Conflict needs a recorded assistant judgment')
        row.update(action='assistant_'+d['choice'],final=d['final'],reason_zh=d['reason_zh'],reviewer=d['reviewer']);final.append(row)
    (out/'pending-conflicts.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in pending),'utf8')
    write(out/'proposal-status.json',dict(accepted_records=len(final),pending_conflicts=len(pending),stale_decisions=stale,missing_results=len(rows(out/'missing-results.jsonl'))))
    if pending or stale or rows(out/'missing-results.jsonl'):print(json.dumps(load(out/'proposal-status.json')));return False
    (out/'final-records.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in final),'utf8')
    proposed=out/'proposed';proposed.mkdir(exist_ok=True)
    for locale in plan['targets']:
        data=load(out/'preimage'/f'{locale}.json')
        for key in plan.get('retired_keys',[]):data['messages'].pop(key,None)
        for row in final:
            if row['locale']!=locale:continue
            if sorted(TOKENS.findall(row['source_key']))!=sorted(TOKENS.findall(row['final'])):raise ValueError('Final placeholder mismatch: '+row['id'])
            data['messages'][row['source_key']]=row['final']
        write(proposed/f'{locale}.json',data)
        if (proposed/f'{locale}.json').stat().st_size>4*1024*1024:raise ValueError('Language resource exceeds the runtime size limit')
    print(json.dumps(load(out/'proposal-status.json')));return True

def apply_proposal(out):
    require_delivery_active(out)
    if not propose(out):raise ValueError('Resolve missing translations and conflicts before applying')
    plan=load(out/'plan.json');current=inventory(QT)
    if current['files']!=plan['source_files'] or current['errors']:raise ValueError('Source changed; revalidate the affected translation contexts before application')
    if digest((QT/'src/app/languages/en.json').read_bytes())!=plan['english_sha256']:raise ValueError('English source changed')
    for locale in plan['targets']:
        if digest((QT/f'src/app/languages/{locale}.json').read_bytes())!=plan['catalogs'][locale]['sha256']:raise ValueError('Target catalog changed: '+locale)
    changes=[]
    for locale in plan['targets']:
        source=out/'proposed'/f'{locale}.json';target=QT/f'src/app/languages/{locale}.json';temp=target.with_suffix('.deepl.tmp');temp.write_bytes(source.read_bytes());temp.replace(target)
        changes.append(dict(locale=locale,before_sha256=plan['catalogs'][locale]['sha256'],after_sha256=digest(target.read_bytes()),entries=len(load(target)['messages'])))
    write(out/'applied.json',dict(applied_utc=now(),changes=changes,source_files=current['files'],english_sha256=plan['english_sha256'],backup=str(out/'preimage'),conflict_decisions_sha256=digest((out/'assistant-decisions.jsonl').read_bytes())))
    print(json.dumps(dict(applied_catalogs=len(changes))))

def fallback_missing(out,key_file,workers):
    """Translate language spans only; rebuild troublesome values/markup locally."""
    require_delivery_active(out)
    require_deepl_enabled(out)
    plan=load(out/'plan.json');cached=valid_results(out);creds=Credentials(key_file)
    glossaries={x['key_alias']:x['glossary_id'] for x in load(out/'glossary-identities.json')['keys'] if x['status']==201} if (out/'glossary-identities.json').exists() else {}
    terminology_sha256=digest((out/'approved-terminology.json').read_bytes()) if glossaries else None
    tasks=[(locale,target,j) for locale,target in plan['targets'].items() for j in plan['jobs'] if (locale,j['id']) not in cached]
    def process(task):
        locale,target,job=task;xml,info=encode(job['text'],job_symbols(job));root=ET.fromstring(xml);segments={int(s.attrib['id']):s for s in root};pieces=[];sources=[]
        def span(value):
            if not value:return
            if re.search(r'[A-Za-z\u3400-\u9fff]',value):
                if value not in sources:sources.append(value)
                pieces.append(('translate',value))
            else:pieces.append(('literal',value))
        for i,part in enumerate(info['parts']):
            if not part['translate']:pieces.append(('literal',part['raw']));continue
            seg=segments[i];span(seg.text)
            for ph in seg:pieces.append(('literal',info['tokens'][int(ph.attrib['id'])]));span(ph.tail)
        translated={};request_ids=[];billed=0
        for start in range(0,len(sources),40):
            batch=sources[start:start+40];count=sum(map(len,batch))
            context=DOMAIN+'\nFull source message or report: '+job['text']+'\nIts function: '+job['context']+'\nOnly language-bearing spans are submitted; numerical data, symbols, units and layout are retained locally.'
            for attempt in range(7):
                item=creds.reserve(count);payload=dict(text=batch,source_lang='EN',target_lang=target,context=context,model_type='prefer_quality_optimized',preserve_formatting=True,show_billed_characters=True)
                if glossaries:payload['glossary_id']=glossaries[item['alias']]
                request_id=digest(json.dumps(payload,ensure_ascii=False,sort_keys=True).encode())
                try:
                    response=requests.post(item['host']+'/v2/translate',json=payload,headers={'Authorization':'DeepL-Auth-Key '+item['secret']},timeout=(15,120))
                    if response.status_code==429 or response.status_code>=500:
                        with creds.lock:item['remaining']+=count
                        time.sleep(min(15,2**attempt));continue
                    if response.status_code in (403,456):
                        with creds.lock:item['disabled']=True
                        continue
                    if response.status_code!=200:emit(out/'request-errors.jsonl',dict(codec='plain-span-fallback',request_id=request_id,locale=locale,status=response.status_code));return False
                    values=response.json()['translations']
                    if len(values)!=len(batch):return False
                    for source,value in zip(batch,values):
                        leading=re.match(r'^\s*',source)[0];trailing=re.search(r'\s*$',source)[0];translated[source]=leading+value['text'].strip()+trailing
                    request_ids.append(request_id);used=sum(x.get('billed_characters',0) for x in values);billed+=used
                    emit(out/'request-receipts.jsonl',dict(request_id=request_id,locale=locale,key_alias=item['alias'],endpoint=item['host'],status=200,codec='plain-span-fallback',job_ids=[job['id']],source_characters=count,provider_billed_characters=used,glossary_id=payload.get('glossary_id'),terminology_sha256=terminology_sha256,context_sha256=digest(context.encode()),context_per_source_explicit=True,batch_size=len(batch)))
                    break
                except requests.RequestException:time.sleep(min(10,2**attempt))
            else:return False
        result=''.join(value if kind=='literal' else translated[value] for kind,value in pieces)
        if sorted(TOKENS.findall(result))!=sorted(TOKENS.findall(job['text'])):raise ValueError('Fallback placeholder mismatch')
        emit(out/'results.jsonl',dict(locale=locale,job_id=job['id'],status='ok',translation=result,provider='DeepL',codec='plain-span-fallback',request_id=digest(json.dumps(request_ids).encode()),component_request_ids=request_ids,glossary_id='per-request',terminology_sha256=terminology_sha256,billed_characters=billed,created_utc=now()))
        return True
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as ex:
        values=list(ex.map(process,tasks))
    print(json.dumps(dict(fallback_jobs=len(tasks),successful=sum(values))),flush=True);reconcile(out)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('stage',choices=['prepare','run','fallback','reconcile','propose','apply']);p.add_argument('--revision',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--keys-file',type=Path);p.add_argument('--targets',nargs='+',default=list(TARGETS),choices=list(TARGETS));p.add_argument('--workers',type=int,default=4);p.add_argument('--batch-size',type=int,default=10);p.add_argument('--limit',type=int,default=0);p.add_argument('--job-selection',type=Path);a=p.parse_args()
    if a.stage=='prepare':prepare(a.revision.resolve(),a.output.resolve(),a.targets)
    elif a.stage=='run':run(a.output.resolve(),a.keys_file.resolve(),a.workers,a.batch_size,a.limit,set(load(a.job_selection)) if a.job_selection else None)
    elif a.stage=='reconcile':reconcile(a.output.resolve())
    elif a.stage=='propose':propose(a.output.resolve())
    elif a.stage=='fallback':fallback_missing(a.output.resolve(),a.keys_file.resolve(),a.workers)
    else:apply_proposal(a.output.resolve())
if __name__=='__main__':main()
