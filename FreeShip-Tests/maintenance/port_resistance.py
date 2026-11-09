"""Auditable source port of Plus resistance arithmetic into ordinary C++.

Generation is an offline maintenance step, never a runtime interpreter. The
retained Pascal bodies are the formula source; process isolation, parameters,
file validation and document ownership live in handwritten resistance.cpp.
Unsupported syntax fails loudly. --check compares all generated source bytes.
"""
import argparse,hashlib,json,re,sys,struct
from decimal import Decimal
from pathlib import Path
try:
    from .port_propulsion_reports import clean
except ImportError:
    from port_propulsion_reports import clean

ROOT=Path(__file__).resolve().parents[2]
PLUS=ROOT/'FreeShip-Plus-Lazarus'
DEST=ROOT/'FreeShip-Qt/src/document'
CONTRACT=ROOT/'FreeShip-Tests/fixtures/resistance/schema.json'
METHODS=dict(delft='Delft',holtrop='Holtr',hollenbach='Hollen',oortmerssen='Oortmer',fung='FungLeib',ost='OST',rbhs='RBHS',mh='MH',kaper='Kaper',planing='Planing')

def quote(s):return 'QStringLiteral('+json.dumps(s,ensure_ascii=False)+')'
def cppname(s):return s.lower()+'_v' if s.lower() in ('new','delete','float','int','char','long','short','auto','register','inline') else s.lower()
def labelname(s):return 'label_'+s.lower()
def preprocess(s):
    # Select the FPC branch before comments are removed.
    stack=[];active=True;out=[];at=0
    for m in re.finditer(r'\{\$\s*(ifn?def|else|endif|ifopt)\b([^}]*)\}',s,re.I):
        if active:out.append(s[at:m.start()])
        kind=m[1].lower()
        if kind.startswith('if'):
            test=m[2].strip().upper() in ('FPC','MSWINDOWS','WINDOWS','CPUX86_64')
            if kind=='ifndef':test=not test
            stack.append((active,test));active=active and test
        elif kind=='else':active=stack[-1][0] and not stack[-1][1]
        else:active=stack.pop()[0]
        at=m.end()
    if active:out.append(s[at:])
    assert not stack
    return clean(''.join(out))

TOKEN=r"'(?:''|[^'])*'|#[0-9]+|[A-Za-z_]\w*|\d+(?:\.(?!\.)\d*)?(?:[Ee][+-]?\d+)?|:=|<>|<=|>=|\.\.|[^\s]"
def toks(s):return re.findall(TOKEN,s)

class Port:
    def __init__(self,s,props=None,locals=None,methods=None,controls=None,returns=False):
        self.t=toks(s);self.p=0;self.props=props or {};self.locals=locals or set();self.methods=methods or set();self.controls=controls or set();self.returns=returns
    def peek(self,n=0):return self.t[self.p+n].lower() if self.p+n<len(self.t) else ''
    def pop(self):t=self.t[self.p];self.p+=1;return t
    def want(self,t):
        actual=self.pop().lower()
        if actual!=t:raise ValueError((t,actual,self.t[max(0,self.p-8):self.p+15]))
    def expr(self,ends):
        # Preserve the original explicit parentheses. Pascal / is real division;
        # integer literal quotients must not turn into C++ integer division.
        out=[]
        while self.p<len(self.t) and self.peek() not in ends:
            t=self.pop();low=t.lower()
            if low=='self' and self.peek()=='.':
                self.pop();prop=self.pop().lower();out.append(cppname(self.props[prop][0])+'()');continue
            if low=='string' and self.peek()=='.' and self.peek(1)=='format':self.pop();self.pop();low='format'
            if low=='format':
                self.want('(');fmt=self.expr({','});self.want(',');self.want('[');args=[]
                while self.peek()!=']':
                    args.append(self.expr({',',']'}))
                    if self.peek()==',':self.pop()
                self.want(']');self.want(')');out.append('pascalFormat('+fmt+', {'+', '.join('QVariant('+a+')' for a in args)+'})');continue
            if t=='(':
                out.append('('+self.expr({')'})+')');self.want(')');continue
            if t=='[':
                parts=[]
                while True:
                    parts.append(self.expr({',',']'}))
                    if self.pop()==']':break
                out.append(''.join('['+p+']' for p in parts));continue
            if t.startswith("'"):v=quote(t[1:-1].replace("''","'"))
            elif t.startswith('#'):v='QString(QChar('+t[1:]+'))'
            elif low.startswith(('rs_','rsloc')):v='tr('+quote(low)+')'
            elif low in self.props and low not in self.locals and (not out or out[-1]!='.'):
                v=cppname(self.props[low][0])+'()'
            elif low in self.controls and low not in self.locals:v='field('+quote(low)+')'
            elif low in self.methods and self.peek()!='(':v=cppname(low)+'()'
            else:
                v={'and':'&&','or':'||','not':'!','=':'==','<>':'!=','div':'/','mod':'%',
                   'power':'power','sqrt':'sqrtp','sqr':'square','sin':'sinp','cos':'cosp','tan':'tanp','arctan':'atanp',
                   'log10':'logp','ln':'lnp','exp':'expp','abs':'absp','trunc':'truncp','round':'roundp',
                   'ffFixed'.lower():'0','ffgeneral':'1','ffexponent':'2','floattostrf':'number',
                   'makelength':'pad','space':'space','userstring':'userstring','nil':'nullptr',
                   'freeship':'this','ffreeship':'this','true':'true','false':'false',
                   'fuimperial':'1','fumetric':'0','getcurrentdir':'work','getcurrentdirutf8':'work',
                   'clteecolor':'0','cldefault':'0','clyellow':'1','clgray':'2','calright':'0',
                   'mtinformation':'0','mterror':'0','mbok':'0','mrok':'1','mrcancel':'0',
                   'directoryseparator':quote('/'),'self':'nullptr','eof':'atEnd','isnan':'std::isnan',
                   'pos':'position','length':'length','fileexistsutf8':'exists','filesearchutf8':'findFile',
                   'includetrailingpathdelimiter':'trailing','extractfilename':'baseName',
                   'inttostr':'QString::number','tchartseries':'chartSeries'}.get(low,cppname(t))
            if re.fullmatch(r'\d+[eE]\+?1000',t):v='std::numeric_limits<float>::infinity()'
            elif re.fullmatch(r'\d+\.\d*|\d+(?:\.\d*)?[Ee][+-]?\d+',t):
                # Win64 FPC uses Single for exactly representable constants,
                # Double otherwise. In particular 0.5 must not widen Cf products.
                try:
                    f=struct.unpack('f',struct.pack('f',float(t)))[0]
                    if Decimal.from_float(f)==Decimal(t):v=t+'f'
                except OverflowError:pass
            elif t.isdigit() and self.peek()=='/' and self.peek(1).isdigit() and (not out or out[-1] in ('+', '-', ',', '==','!=','<','>','<=','>=')):
                v=t+'.0'
            if (t.startswith("'") or t.startswith('#')) and out and (out[-1].startswith('QStringLiteral(') or out[-1].startswith('QString(QChar(')):out.append('+')
            out.append(v)
        text=' '.join(out)
        text=text.replace('this .','this->').replace('this-> projectsettings .','settings.')
        text=text.replace('this-> preferences . execdirectory','solverDirectory').replace('this-> preferences . initdirectory','work')
        text=text.replace('this-> calculationdirectory','work')
        text=re.sub(r'hydobject \. data \. (\w+)(?: \. ([xyz]))?',lambda m:'hydValue(hydobject, '+quote(m[1]+('.'+m[2] if m[2] else ''))+')',text)
        return text
    def args(self):
        self.want('(');a=[]
        if self.peek()!=')':
            while True:
                a.append(self.expr({',',')'}))
                if self.peek()!=',':break
                self.pop()
        self.want(')');return a
    def statement(self,indent=1):
        pre='    '*indent;t=self.peek()
        if t==';':self.pop();return ''
        if t=='begin':
            self.pop();out=pre+'{\n'
            while self.peek()!='end':out+=self.statement(indent+1)
            self.pop();return out+pre+'}\n'
        if t=='if':
            self.pop();condition=self.expr({'then'});self.want('then');out=pre+'if ('+condition+')\n'+self.statement(indent+1)
            if self.peek()=='else':self.pop();out+=pre+'else\n'+self.statement(indent+1)
            return out
        if t=='for':
            self.pop();var=cppname(self.pop());self.want(':=');start=self.expr({'to','downto'});down=self.pop().lower()=='downto';end=self.expr({'do'});self.want('do')
            return pre+f'for ({var}={start}; {var}{">=" if down else "<="}{end}; {"--" if down else "++"}{var})\n'+pre+'{\n'+pre+'    check();\n'+self.statement(indent+1)+pre+'}\n'
        if t=='while':
            self.pop();cond=self.expr({'do'});self.want('do');return pre+'while ('+cond+') {\n'+pre+'    check();\n'+self.statement(indent+1)+pre+'}\n'
        if t=='repeat':
            self.pop();out=pre+'do {\n'+pre+'    check();\n'
            while self.peek()!='until':out+=self.statement(indent+1)
            self.pop();return out+pre+'} while (!('+self.expr({';'})+'));\n'
        if t=='try':
            self.pop();out=''
            while self.peek() not in ('except','finally','end'):out+=self.statement(indent)
            if self.peek()=='finally':
                self.pop()
                while self.peek()!='end':out+=self.statement(indent)
            elif self.peek()=='except':
                # File/process cleanup is RAII in the runtime. A legacy handler
                # must never suppress an exception or open a modal window.
                self.pop();depth=0
                while self.p<len(self.t):
                    x=self.peek()
                    if x=='end' and depth==0:break
                    if x in ('begin','try'):depth+=1
                    elif x=='end':depth-=1
                    self.pop()
            self.want('end');return out
        if t=='goto':self.pop();return pre+'goto '+labelname(self.pop())+';\n'
        if self.peek(1)==':' and t not in ('str','write','writeln'):
            label=labelname(self.pop());self.pop();return pre+label+':;\n'
        if t in ('write','writeln','str'):
            self.pop();self.want('(');args=[]
            while True:
                parts=[self.expr({':',',',')'})]
                while self.peek()==':':self.pop();parts.append(self.expr({':',',',')'}))
                if len(parts)>1:args.append('formatValue('+', '.join(parts)+')')
                else:args.append(parts[0])
                if self.peek()!=',':break
                self.pop()
            self.want(')')
            if t=='str':return pre+args[1]+' = '+args[0]+';\n'
            return pre+('writeLine' if t=='writeln' else 'writeText')+'('+', '.join(args)+');\n'
        if t in ('read','readln'):
            self.pop();args=self.args();return pre+('readLine' if t=='readln' else 'readValues')+'('+', '.join(args)+');\n'
        if t in ('setlength','inc','delete','assignfile','reset','rewrite','closefile','executefreecalculation','messagedlg','showmessage','setcurrentdir','setcurrentdirutf8','sleep','deletefileutf8','forcedirectoriesutf8','freeandnil'):
            self.pop();a=self.args()
            if t=='setlength':x=a[0]+'.resize('+a[1]+')'
            elif t=='inc':x=a[0]+' += '+(a[1] if len(a)>1 else '1')
            elif t=='delete':x=a[0]+'.remove(('+a[1]+')-1, '+a[2]+')'
            elif t=='assignfile':x=a[0]+'.name = safePath('+a[1]+')'
            elif t in ('reset','rewrite','closefile'):x={'reset':'readFile','rewrite':'newFile','closefile':'closeFile'}[t]+'('+a[0]+')'
            elif t=='executefreecalculation':x='runSolver('+a[1]+', '+a[2]+')'
            elif t in ('messagedlg','showmessage'):x='warning('+a[0]+')'
            else:x='check()'
            return pre+x+';\n'
        expr=self.expr({';','end','else','until','except','finally'})
        if self.peek()==';':self.pop()
        if not expr:raise ValueError(('Empty statement',self.t[self.p:self.p+12]))
        if expr=='exit':return pre+('return result;' if self.returns else 'return;')+'\n'
        if expr=='break':return pre+'break;\n'
        if ':=' in expr:
            left,right=map(str.strip,expr.split(':=',1))
            if left=='hydobject' and right.startswith('tfreehydrostaticcalc'):return pre+'; // local immutable hydrostatic request\n'
            for prop,(getter,setter) in self.props.items():
                if left==cppname(getter)+'()':return pre+cppname(setter)+'('+right+');\n'
            if any(k in left for k in (' . color',' . font',' . readonly',' . visible',' . autosize',' . text . text')):return pre+'; // presentation only\n'
            if ' . axisindexy' in left:return pre+'setAxis('+quote(left.split(' . ')[0])+', '+right+');\n'
            if re.fullmatch(r'series\w* \. title',left):return pre+'setSeriesTitle('+quote(left.split(' . ')[0])+', '+right+');\n'
            if ' . value' in left:
                return pre+left.split(' . value')[0]+'.set('+right+');\n'
            if left in ('resultsmemo . text','resultsmemo2 . text'):return pre+left.split(' . ')[0]+'.clear();\n'
            if left.startswith('rightaxis') or 'chart . leftaxis' in left or 'chart . bottomaxis' in left or 'chart . axislist' in left:
                target='rightTitle' if left.startswith('rightaxis') or 'axislist' in left else 'xTitle' if 'bottomaxis' in left else 'yTitle'
                if left=='rightaxis':return pre+';\n'
                return pre+target+' = '+right+';\n'
            expr=left+' = '+right
        expr=re.sub(r'(resultsmemo2?|resultsMemo) \. lines \. add \(',r'\1.append(',expr,flags=re.I)
        expr=re.sub(r'(resultsmemo2?) \. (?:lines \. )?clear\b(?:\(\))?',r'\1.clear()',expr)
        m=re.fullmatch(r'(series\w*) \. addxy \((.*)\)',expr)
        if m:
            a=[];depth=0;start=0
            for i,c in enumerate(m[2]):
                if c in '([':depth+=1
                elif c in ')]':depth-=1
                elif c==',' and depth==0:a.append(m[2][start:i]);start=i+1
            a.append(m[2][start:])
            # AddXY accepts Double: FPC promotes real constants in this call
            # context, but retains Single-only subexpressions without them.
            a=[re.sub(r'(\d+(?:\.\d*)?(?:[eE][+-]?\d+)?)f\b',r'\1',x) for x in a]
            expr='point('+quote(m[1])+', '+a[0]+', '+a[1]+')'
        expr=re.sub(r'(series\w*) \. clear\b',lambda m:'clearSeries('+quote(m[1])+')',expr)
        expr=expr.replace('this-> createoutputheader','header')
        if expr in ('hydobject . calculate','hydobject . calculate()'):expr='hydobject.data = hydro(hydobject.draft)'
        expr=expr.replace('resultsmemo . lines','resultsmemo').replace('resultsmemo2 . lines','resultsmemo2')
        if expr in self.methods:expr+='()'
        expr=re.sub(r'chartSeries \(chart \. series \[(\d+)\]\) \. addxy \((.*)\)',lambda m:'pointIndex('+m[1]+', '+m[2]+')',expr)
        expr=re.sub(r'chartSeries \(chart \. series \[(\d+)\]\) \. clear',r'clearIndex(\1)',expr)
        return pre+expr+';\n'
    def run(self):
        out=''
        while self.p<len(self.t):out+=self.statement()
        return out

TYPES={'single':'float','tfloatType'.lower():'float','double':'double','extended':'double','integer':'int','boolean':'bool','string':'QString','textfile':'TextFile','tfreeunittype':'int','tcolor':'int','tchartaxis':'int','tobject':'void*','tfreehydrostaticcalc':'HydroInput'}
def typename(t):
    t=t.strip().lower()
    if t in ('tfreeship','tfreecalculationscope','tcursor','cardinal'):return 'int'
    if t in TYPES:return TYPES[t]
    if t=='array of single':return 'std::vector<float>'
    m=re.fullmatch(r'array\s*\[(.*?)\]\s*of\s*(\w+)',t,re.S)
    if m:
        ty=typename(m[2])
        for d in reversed(m[1].split(',')):
            lo,hi=d.strip().split('..');ty=f'Array<{ty},{lo},{hi}>'
        return ty
    if t=='ttable':return 'Array<Array<float,0,13>,0,16>'
    raise ValueError('Unsupported type '+t)

def declarations(s):
    s=re.sub(r'\b(var|const)\b','',s,flags=re.I);s=re.sub(r'\blabel\b.*','',s,flags=re.S|re.I)
    s=re.sub(r'\btype\s+TTable\s*=.*?;','',s,flags=re.S|re.I)
    out=[];names=set()
    for part in s.split(';'):
        part=part.strip()
        if not part:continue
        m=re.fullmatch(r'([\w,\s]+)(?::\s*([^=]+))?(?:=\s*(.*))?',part,re.S)
        if not m:raise ValueError(('declaration',part))
        ns=[cppname(n.strip()) for n in m[1].split(',')];names.update(ns)
        ty=typename(m[2]) if m[2] else 'double'
        for n in ns:
            initial='{}'
            if m[3]:
                value=m[3].strip()
                if value.startswith('('):initial=' = '+value.replace('(','{').replace(')','}')
                else:initial=' = '+Port(value).expr(set())
            out.append(f'{"const " if m[3] else ""}{ty} {n}{initial};')
    return '\n'.join(out),names

def bodies(s):
    hits=list(re.finditer(r'(?mi)^(procedure|function) T\w+\.(\w+)',s))
    out={}
    for i,m in enumerate(hits):
        if not re.match(r'^(fget|fset|correctinput|calculate$|calculateinworkspace|calculateresistance|resist$|optimum|file_export|selectcalc|selectpropulsor|combobox\w*click$|combobox\w*change$|checkbox[2-6]click$|estimate[2-4]?boxclick$|drafttotalboxaftersetvalue$|execute$)',m[2],re.I):continue
        end=hits[i+1].start() if i+1<len(hits) else len(s)
        header_end=m.end();depth=0
        while header_end<end:
            c=s[header_end]
            if c=='(':depth+=1
            elif c==')':depth-=1
            elif c==';' and depth==0:break
            header_end+=1
        header=s[m.end():header_end].strip()
        signature=re.fullmatch(r'(\(.*\))?\s*(?::\s*(\w+))?',header,re.S)
        assert signature,(m[2],header)
        body=s[header_end+1:end];last=re.search(r'end\s*;\s*$',body.strip())
        # The final unit terminator is not part of the final method.
        body=re.sub(r'\bend\s*\.\s*$','',body,flags=re.I).strip()
        at=re.search(r'\bbegin\b',body,re.I)
        if not at:raise ValueError(('method begin',m[2]))
        params=[];locals=set()
        if signature[1]:
            for group in signature[1].strip()[1:-1].split(';'):
                a,t=group.strip().split(':',1);byref=a.lower().startswith('var ');a=re.sub(r'^var\s+','',a,flags=re.I)
                for n in a.split(','):
                    n=cppname(n.strip());locals.add(n);ty='Span<float>' if t.strip().lower()=='array of single' else typename(t)
                    params.append(ty+(' &' if byref and ty!='Span<float>' else ' ')+n)
        decl,declnames=declarations(body[:at.start()]);locals|=declnames
        last=re.search(r'end\s*;\s*$',body,re.I)
        if not last:raise ValueError(('method end',m[2],body[-100:]))
        out[m[2].lower()]=dict(name=m[2].lower(),ret=typename(signature[2]) if m[1].lower()=='function' else 'void',params=params,locals=locals,decl=decl,body=body[at.end():last.start()])
    return out

def fix_source(s,key):
    # Isolated adapter fixes from performance_adapter.py, with initialization
    # supplied by Array's zero-filled storage instead of uninitialized locals.
    if key=='planing':
        s=s.replace('  for ii := 2 to 16 do','  Imax:=0;\n  for ii := 2 to 16 do')
        # A partially filled curve is not a successful calculation. Throw at
        # the adapter's existing four-point boundary, before publishing cache.
        s=s.replace('  for ii := Imax - 3 to Imax do',"  if Imax<4 then Bad('航速范围内不足四个有效计算点，请检查最大航速及方法适用范围。');\n  for ii := Imax - 3 to Imax do")
    # No process-wide cwd changes, sleeps, file polling or modal error handlers.
    s=re.sub(r'(?im)^\s*(?:ProcessCallerDirectory|ProcessCallerCursor|PathFileOld|PathFile|CurDir)\s*:=.*?;','',s)
    s=re.sub(r'(?im)^\s*Screen\.Cursor\s*:=.*?;','',s)
    s=re.sub(r'(?im)^\s*\w+\s*:=\s*SetCurrentDir(?:UTF8)?\([^;]*;','',s)
    s=re.sub(r'(?mi)(ResultsMemo\w*\.Lines\.Add)\((.*),\s*(\[[^;]*\])\);',r'\1(Format(\2,\3));',s)
    return s

def generate(capture=None):
    if capture:
        catalog=json.loads(capture.read_text('utf8'));CONTRACT.parent.mkdir(parents=True,exist_ok=True)
        CONTRACT.write_text(json.dumps({k:{f:catalog[k][f] for f in ('fields','calculator','title')} for k in METHODS},ensure_ascii=False,indent=2)+'\n','utf8')
    schema=json.loads(CONTRACT.read_text('utf8'));allmeta={};origin={};output={};factory=[]
    sys.path.insert(0,str(ROOT/'FreeShip-Qt/bridge'))
    from performance_adapter import lfm_nodes
    po=(PLUS/'locale/FreeShip.cn.po').read_text('utf-8-sig');strings={}
    for block in po.split('\n\n'):
        refs=re.findall(r'^#: (.*)',block,re.M);m=re.search(r'^msgstr (.*)',block,re.M)
        if not m:continue
        value=''.join(json.loads(x) for x in re.findall(r'^".*"$',block[m.start()+7:],re.M))
        if value:
            for ref in ' '.join(refs).split():strings[ref.split('.')[-1].lower()]=value
    resources=(PLUS/'Units/FreeResourceStrings.inc').read_text('utf-8-sig')
    for k,body in re.findall(r"(?m)^\s*(?:resourcestring\s+)?(rs\w+)\s*=\s*((?:'(?:''|[^'])*'|#[0-9]+|\+|\s)+);",resources,re.I):
        strings.setdefault(k.lower(),''.join(chr(int(t[1:])) if t.startswith('#') else t[1:-1].replace("''","'") for t in re.findall(r"'(?:''|[^'])*'|#\d+",body)))
    indexed={n:k.lower() for n,k in re.findall(r'UserStrings\[\s*(\d+)\]\s*:=\s*(\w+);',(PLUS/'Units/FreeUserStringsArray.inc').read_text('utf-8-sig'))}
    for key,short in METHODS.items():
        path=PLUS/f'Forms/FreeResistance_{short}Dlg.pas';raw=path.read_text('utf-8-sig');origin[str(path.relative_to(PLUS))]=hashlib.sha256(path.read_bytes()).hexdigest()
        text=fix_source(preprocess(raw),key);interface,implementation=text.split('implementation',1)
        props={a.lower():(b.lower(),c.lower()) for a,b,c in re.findall(r'property\s+(\w+)\s*:\s*\w+\s+read\s+(\w+)\s+write\s+(\w+)',interface,re.I)}
        controls=set(re.findall(r'(?m)^\s*(\w+)\s*:\s*T(?:FloatSpinEdit|SpinEdit|CheckBox|ComboBox|RadioGroup|Label|GroupBox|SpeedButton|ToolButton)\s*;',interface,re.I));controls={c.lower() for c in controls}
        allmethods=bodies(implementation)
        selected={n:m for n,m in allmethods.items() if re.match(r'^(?:fget|fset|correctinput|calculateresistance|resist$|optimum|file_export|selectcalc|selectpropulsor|combobox\w*click$|combobox\w*change$|checkbox[2-6]click$|estimate[2-4]?boxclick$|drafttotalboxaftersetvalue$)',n)}
        entry='calculate' if key in ('delft','kaper') else 'calculateinworkspace';selected[entry]=allmethods[entry]
        for n,m in selected.items():
            m['body']=re.sub(r'(?mi)^\s*(?:Calculate|doCalculate);','',m['body']) if n!=entry else m['body']
        selected['execute']=allmethods['execute']
        m=selected['execute'];m['params']=[];m['locals']-={'freeship','autoextract'};m['body']=re.sub(r'(?mi)^\s*(?:FFreeship := Freeship;|Calculate;|ShowModal;|ToolBar1\..*?;|Freeship.Preferences.LoadImageIntoList\(.*?;|Checkbox2.Enabled.*?;)','',m['body'])
        m['body']=m['body'].replace('AutoExtract','False');m['body']=re.sub(r'(?si)if Checkbox2.Checked then\s*CheckBox2Click\(self\);','',m['body'])
        m['body']=re.sub(r'ModalResult = mrOk','False',m['body'],flags=re.I)
        # Private scalar/array fields occur between complete method declarations.
        classbody=interface.split('= class',1)[1].split('  end;',1)[0]
        classbody=re.sub(r'(?m)^\s*(?:procedure|function)\s+\w+\s*(?:\([^)]*\))?\s*(?::\s*\w+)?\s*;', '',classbody,flags=re.I)
        members=[]
        for line in re.findall(r'(?m)^\s*\w+(?:\s*,\s*\w+)*\s*:\s*(?:single|integer|boolean|extended|string|array\[.*?\] of single)\s*;',classbody,re.I):
            m=re.fullmatch(r'\s*([\w,\s]+)\s*:\s*((?:single|integer|boolean|extended|string)|array\[.*\] of single)\s*;',line,re.I)
            if m and not re.search(r'\b(function|procedure|property|var)\b',m[1],re.I):members.append(m[0])
        memberdecl,_=declarations('\n'.join(members))
        constants=[]
        for sec in (interface.split('type',1)[0] if key=='delft' else '',implementation.split('function',1)[0] if key=='kaper' else ''):
            # Array coefficient tables only, with every bound retained.
            for m in re.finditer(r'(\w+)\s*:\s*array\s*\[[^]]+\]\s*of\s*\w+\s*=\s*\([^;]+;',sec,re.I|re.S):constants.append(m[0])
        globals_,_=declarations('\n'.join(constants))
        cls='Resistance_'+key;decls=[];defs=[]
        for n,m in selected.items():
            decls.append(m['ret']+' '+cppname(n)+'('+', '.join(m['params'])+');')
            port=Port(m['body'],props,m['locals'],set(selected),controls,m['ret']!='void')
            try:code=port.run()
            except Exception as ex:raise RuntimeError(f'{key}.{n}: {ex}') from ex
            if m['ret']!='void':m['decl']=m['ret']+' result{};\n'+m['decl'];code+='    return result;\n'
            defs.append(m['ret']+' '+cls+'::'+cppname(n)+'('+', '.join(m['params'])+') {\n'+m['decl']+'\n'+code+'}\n')
        if key=='holtrop':
            # CorrectInputData queried estimated area before setting its draft.
            defs=[d.replace('bool result{};','bool result{};\nif(tc==0)tc=fgetdrafttotal();',1) if d.startswith('bool '+cls+'::correctinputdata(') else d for d in defs]
        propinit=[]
        if 'density' in props:propinit.append('if (member=="projectwaterdensity") { '+props['density'][1]+'(value); return; }')
        if key in ('kaper','planing'):
            for stored,prop in {'draft':'h','lwl':'ewl','displacement':'displ','wettedsurface':'ws','entranceangle':'ie'}.items():
                propinit.append('if (member=='+quote(stored)+') { '+props[prop][1]+'(value); return; }')
        for prop,(getter,setter) in props.items():
            if prop=='extractfromhull':continue
            propinit.append(f'if (member=={quote(prop)}) {{ {setter}(value); return; }}')
        choice=[];booleans=[]
        for n in selected:
            if re.fullmatch(r'(?:estimate[2-4]?box|checkbox[3-6])click',n):booleans.append('if (key=='+quote(n[:-5])+') '+n+'(nullptr);')
        for keyname,n in [('combobox','comboboxclick'),('combobox1','combobox1click')]:
            if n in selected:choice.append('if (key=='+quote(keyname)+') '+n+'(nullptr);')
        body='// GPL-2.0-or-later. Generated from FreeResistance_'+short+'Dlg.pas.\nnamespace {\n'+globals_+'\nstruct '+cls+' final: ResistanceBase {\nusing ResistanceBase::ResistanceBase;\n'+memberdecl+'\n'+'\n'.join(decls)+'\nvoid loadProperty(const QString &member,float value) override {\n'+'\n'.join(propinit)+'\nResistanceBase::loadProperty(member,value);\n}\nvoid initialise() override { execute(); }\nvoid choiceChanged(const QString &key) override { '+''.join(choice)+' }\nvoid calculate() override { '+entry+'(); }\n};\n'+'\n'.join(defs)+'}\n'
        body=body.replace('void initialise() override', 'void booleanChanged(const QString &key) override { '+''.join(booleans)+' }\nvoid initialise() override')
        # Avoid same name as the virtual dispatch when Calculate is the formula.
        if entry=='calculate':body=body.replace('void calculate() override { calculate(); }','void runCalculation() override { calculate(); }')
        else:body=body.replace('void calculate() override','void runCalculation() override')
        body=body.replace('void initialise() override', 'void extract() override { field("checkbox2").checked=true;checkbox2click(nullptr);field("checkbox2").checked=false;checkbox2click(nullptr); }\nvoid initialise() override')
        output['resistance_'+key+'.inc']=body
        factory.append(f'if (method=={quote(key)}) return std::make_unique<{cls}>(source,request,output,bindings);')
        lfm=PLUS/f'Forms/FreeResistance_{short}Dlg.lfm';nodes=lfm_nodes(lfm.read_text('utf-8-sig'));control={};series=[]
        for node in nodes:
            if node['name'].lower() in controls:
                c={k:node[k] for k in ('Caption','Value','MinValue','MaxValue','Checked','ItemIndex','Enabled','Visible') if k in node}
                control[node['name'].lower()]=c
            if node['class']=='TLineSeries':series.append(dict(source=node['name'],name=node.get('Title',"''")[1:-1],axis=int(node.get('AxisIndexY','-1'))))
        allmeta[key]={'schema':schema[key],'controls':control,'series':series}
        origin[str(lfm.relative_to(PLUS))]=hashlib.sha256(lfm.read_bytes()).hexdigest()
    allmeta['strings']=strings;allmeta['numbered']={n:strings.get(k,'') for n,k in indexed.items()}
    function_file=PLUS/'Units/FreeShipUnit_Functions.inc'
    # Extract the complete procedure, including nested blocks, up to the next declaration.
    interpolation=re.split(r'(?mi)^\s*(?:procedure|function)\s',preprocess(function_file.read_text('utf-8-sig')).split('procedure SFINEX1',1)[1])[0].strip()
    at=re.search(r'\bbegin\b',interpolation,re.I);tail=re.search(r'end\s*;\s*$',interpolation,re.I)
    decl,_=declarations(interpolation[interpolation.index(');')+2:at.start()])
    code=Port(interpolation[at.end():tail.start()]).run().replace('check();','')
    output['resistance_interpolation.inc']='// GPL-2.0-or-later. Plus SFINEX1, zero-based open arrays.\nvoid sfinex1(int n,Span<const float> x,Span<const float> y,float x0,float &yy) {\n'+decl+'\nif(n<1 || n>x.n || n>y.n) bad("Invalid interpolation size");\n'+code+'}\n'
    origin[str(function_file.relative_to(PLUS))]=hashlib.sha256(function_file.read_bytes()).hexdigest()
    for path in (PLUS/'locale/FreeShip.cn.po',PLUS/'Units/FreeResourceStrings.inc',PLUS/'Units/FreeUserStringsArray.inc',CONTRACT):origin[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
    output['resistance_tables.h']='// GPL-2.0-or-later. Generated resistance metadata and original captions.\nstatic constexpr auto ResistanceTables=R"res('+json.dumps(allmeta,ensure_ascii=False,separators=(',',':'))+')res";\n'
    output['resistance_factory.inc']='\n'.join(factory)+'\n'
    output['resistance-origin.json']=json.dumps(origin,sort_keys=True,indent=2)+'\n'
    return output,allmeta

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');p.add_argument('--capture-catalog',type=Path);a=p.parse_args()
    output,meta=generate(a.capture_catalog)
    for name,body in output.items():
        path=DEST/name
        if a.check:assert path.read_text('utf8')==body,name
        else:path.write_text(body,'utf8')
    print('resistance arithmetic generation: pass',len(output))
if __name__=='__main__':main()
