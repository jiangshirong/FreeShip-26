"""Version-checked extraction of the existing resistance/propeller contracts.

Only isolated build copies are changed. Formula bodies and file protocols stay
in their original units; hidden forms are temporary calculation data containers,
never the application's UI. The generated field map retains source label IDs.
"""
import re

METHODS = [
    ('delft','Resistance_Delft','Delft 帆船阻力'),
    ('holtrop','Resistance_Holtr','Holtrop 海船阻力'),
    ('hollenbach','Resistance_Hollen','Hollenbach 阻力'),
    ('oortmerssen','Resistance_Oortmer','van Oortmerssen 阻力'),
    ('fung','Resistance_FungLeib','Fung–Leibman 阻力'),
    ('ost','Resistance_OST','OST 阻力'),
    ('rbhs','Resistance_RBHS','圆舭高速艇阻力'),
    ('mh','Resistance_MH','多体船阻力'),
    ('kaper','Resistance_Kaper','KAPER 独木舟及皮艇阻力'),
    ('planing','Resistance_Planing','滑行艇阻力'),
    ('propeller1','Propeller_Task1','螺旋桨优化与主机选型'),
    ('propeller2','Propeller_Task2','主机与螺旋桨匹配'),
    ('propeller3','Propeller_Task3','船舶推进特性图（Passport）'),
    ('propeller4','Propeller_Task4','螺旋桨初步设计'),
    ('propeller5','Propeller_Task5','螺旋桨系列计算'),
]

def pas(s): return "'"+s.replace("'","''")+"'"

def lfm_nodes(text):
    nodes=[]; stack=[]
    for line in text.splitlines():
        m=re.match(r'(\s*)object (\w+): (\w+)',line)
        if m:
            n={'name':m[2],'class':m[3],'parent':stack[-1] if stack else None,'indent':len(m[1])}
            nodes.append(n);stack.append(n)
        elif stack and line.strip()=='end' and len(line)-len(line.lstrip())==stack[-1]['indent']:
            stack.pop()
        elif stack:
            m=re.match(r'\s*(\w+) = (.*)',line)
            if m and len(line)-len(line.lstrip())==stack[-1]['indent']+2:stack[-1][m[1]]=m[2]
    return nodes

def field_map(nodes):
    fields=[]
    for n in nodes:
        if n['class'] not in ('TFloatSpinEdit','TSpinEdit','TCheckBox','TComboBox','TRadioGroup'):continue
        if n['name'].lower()=='checkbox2':continue # explicit extract action, never implicit stale-cache input
        label=unit='';container=n
        if n['class'] in ('TFloatSpinEdit','TSpinEdit','TComboBox'):
            # Resolve declared LFM rows, including numeric controls wrapped in a
            # layout panel. A map is generated once, not guessed from live UI.
            while container.get('parent'):
                siblings=[x for x in nodes if x.get('parent') is container['parent'] and x['class']=='TLabel']
                top=int(container.get('Top','0'));height=int(container.get('Height','0'))
                labels=[x for x in siblings if abs(int(x.get('Top','0'))-top)<=max(3,height//3)]
                left=int(container.get('Left','0'))
                before=[x for x in labels if int(x.get('Left','0'))<left]
                after=[x for x in labels if int(x.get('Left','0'))>left]
                if before:
                    label=max(before,key=lambda x:int(x.get('Left','0')))['name']
                    if after:unit=min(after,key=lambda x:int(x.get('Left','0')))['name']
                    break
                container=container['parent']
        group='';p=n.get('parent')
        while p:
            if p['class'] in ('TGroupBox','TTabSheet') and 'Caption' in p:
                group=p['name'];break
            p=p.get('parent')
        fields.append((n['name'],label,unit,group))
    return fields

def adapt(work, bridge, replace_once):
    import shutil
    for name in ('PerformanceEngine.pas','PerformanceHooks.pas'):
        shutil.copy2(bridge/name,work/'Units'/name)
    edit=(work/'Units/FreeEdit.inc').read_text('utf-8-sig')
    factory=[];runner=[];storer=[];schema=[];uses=[]
    for key,entry,title in METHODS:
        unit='Free'+entry+'Dlg';cls='TFree'+entry;uses.append(unit)
        path=work/'Forms'/f'{unit}.pas';s=path.read_text('utf-8-sig')
        s=re.sub(r'(?m)^(\s*)private\b',r'\1public',s,flags=re.I)
        body=re.search(r'procedure TFreeEdit\.'+entry+r';(.*?)end;\{TFreeEdit\.'+entry+r'\}',edit,re.S|re.I)[1]
        init=body[body.index('//ShowTranslatedValues(Dialog);')+len('//ShowTranslatedValues(Dialog);'):body.index('  if Dialog.Execute')]
        store=body[body.index('  if Dialog.Execute'):body.index('  FreeAndNil(Dialog);')]
        store=store[store.index('  begin')+7:store.rindex('  end;')]
        def rewrite(x):return re.sub(r'\bFreeShip\b','Ship',x,flags=re.I).replace('Dialog.',cls+'(Form).')
        control_fix=f'  {cls}(Form).StepSpeedBox.MaxValue:=1050;\n' if key=='hollenbach' else ''
        if key=='mh':control_fix+=f'  {cls}(Form).NserBox.MaxValue:=24;\n'
        choices=[]
        if key=='rbhs':choices=['模型试验 Fr/Cr','Braun','Volodin','Nordstrom','de Groot','SSPA','NPL','Series 64','Doubrovsky']
        if key=='mh':choices=['模型试验 Fr/Cr','Doubrovsky 双体船','Doubrovsky 三体船','折角双体船','Voytkunsky 1985','Molland 1994','Pham / Sahoo 2001','Alferiev 内河双体船','Zips 1995']
        restore_choices=''
        if choices:
            restore_choices=f'  {cls}(Form).ComboBox.Items.Clear;\n'+''.join(f'  {cls}(Form).ComboBox.Items.Add({pas(label)});\n' for label in choices)+f'  {cls}(Form).ComboBox.ItemIndex:=0;\n'
        factory.append(f"if Method={pas(key)} then begin\n  Form:={cls}.Create(nil);Result:=Form;\n"+control_fix+restore_choices+rewrite(init)+f'  {cls}(Form).Execute(Ship,False);Exit;\nend;')
        runner.append(f"if Method={pas(key)} then begin {cls}(Form)."+('doCalculate' if key in ('holtrop','oortmerssen','rbhs','planing') else 'Calculate')+';Exit end;')
        storer.append(f"if Method={pas(key)} then begin\n"+rewrite(store)+'Exit end;')
        fields=field_map(lfm_nodes((work/'Forms'/f'{unit}.lfm').read_text('utf-8-sig')))
        if key=='propeller2':
            fields=[(name,'Label17' if name.startswith('Edit17_') else 'Label18' if name.startswith('Edit18_') else label,unitname,group) for name,label,unitname,group in fields]
        if key=='delft':fields=[(n,l,'' if n=='CpBox' else u,g) for n,l,u,g in fields]
        schema.append(f"if Method={pas(key)} then begin\n  Result.Add('title',{pas(title)});\n"+
            '\n'.join('  DescribeField(Form,Fields,'+','.join(map(pas,f))+');' for f in fields)+'\nend;')
        # Initialization may set controls whose events call Calculate. Suppress
        # only those entrypoints; equations below them are unchanged.
        s=replace_once(s,'implementation','implementation\nuses PerformanceHooks;') if not re.search(r'implementation\s+uses',s,re.I) else re.sub(r'(implementation\s+uses)',r'\1 PerformanceHooks,',s,count=1,flags=re.I)
        s,n=re.subn(r'(procedure '+cls+r'\.(?:Calculate|doCalculate|CalculateInWorkspace);.*?\bbegin\b)',r'\1\n  if PerformancePreparing then Exit;PerformanceCheck;',s,flags=re.S|re.I)
        if n<1:raise RuntimeError('No calculation entrypoint: '+key)
        # Do not leave legacy dialogs waiting for input in the service process.
        s=re.sub(r'(?<![.\w])MessageDlg\(', 'PerformanceMessage(',s,flags=re.I)
        s=re.sub(r'(?<![.\w])ShowMessage\(', 'PerformanceNotice(',s,flags=re.I)
        # Pascal identifiers ignore case: rs_kn collided with rs_kN upstream.
        # The original string number identifies speed units unambiguously.
        s=re.sub(r'rs_kn\s*\{UserString\[326\]\}',"'kn' {UserString[326]}",s)
        start=s.index('function '+cls+'.Execute(')
        end=s.index('end;{'+cls+'.Execute}',start)
        part=s[start:end];part,n=re.subn(r'\bShowModal;', 'ModalResult:=mrCancel;',part)
        if n!=1:raise RuntimeError('Execute changed: '+key)
        s=s[:start]+part+s[end:]
        # Each original speed iteration remains intact, with a bounded cancel
        # checkpoint before each curve sample (also prevents float-step stalls).
        s=re.sub(r'(?im)^(\s*)(Speed\s*:=\s*Speed\s*\+)',r'\1PerformanceCheck;\n\1\2',s)
        if key in ('ost','rbhs','mh'):
            a=s.index('procedure '+cls+'.CalculateInWorkspace;');b=s.index('begin',a)
            s=s[:b+5]+'\n  FillChar(R_r,SizeOf(R_r),0);'+s[b+5:]
        if key=='rbhs':
            s=replace_once(s,"  ExecFullName := FExecDirectory + DirectorySeparator+'hship.EXE';","  FExecDirectory := FFreeship.Preferences.ExecDirectory;\n  ExecFullName := FExecDirectory + DirectorySeparator+'hship.EXE';")
        if key=='mh':
            s=replace_once(s,'      Read(FFile, Nser);',"      Read(FFile, Nser);\n      if (Nser<>0) and ((Nser<14) or (Nser>24)) then begin CloseFile(FFile);raise Exception.Create('Unsupported multihull result series') end;")
        if key=='planing':
            a=s.index('procedure '+cls+'.CalculateInWorkspace;');b=s.index('\nbegin',a)+6
            names=re.findall(r'\b(Res\w+): array\[1\.\.16\] of single;',s[a:b],re.I)
            s=s[:b]+'\n'+''.join(f'  FillChar({n},SizeOf({n}),0);\n' for n in names)+s[b:]
            s=replace_once(s,'  for ii := 2 to 16 do','  Imax:=0;\n  for ii := 2 to 16 do')
            s=replace_once(s,'  for ii := Imax - 3 to Imax do',"  if Imax<4 then begin CloseFile(FFile);raise Exception.Create('航速范围内不足四个有效计算点，请检查最大航速及方法适用范围。') end;\n  for ii := Imax - 3 to Imax do")
        if key=='propeller5':
            s=replace_once(s,'Res: array[1..8, 1..70] of single;','Res: TPerformanceFppMatrix;\n  FppCount:Integer;')
            a=s.index('  // Построение кривых действия ГВ');b=s.index('  if Combobox.ItemIndex > 0 then\n  begin\n    Jpopt',a)
            old=s[a:b]
            s=s[:a]+'''  if Combobox.ItemIndex>0 then begin
    PerformanceReadFpp('OUT.',Res,FppCount);
    for II:=1 to FppCount do
      if (Res[4,II]>=0) and ((II=1) or (Res[1,II]>Res[1,II-1])) then begin
        Series1.AddXY(Res[1,II],Res[4,II],'',clTeeColor);
        Series2.AddXY(Res[1,II],Res[7,II]*10,'',clTeeColor);
        Series3.AddXY(Res[1,II],Res[8,II],'',clTeeColor);
      end;
    II:=FppCount+2;
  end else begin
'''+old+'  end;\n'+s[b:]
        path.write_text(s,encoding='utf8')
    generated='\n'.join([
        'function CreatePerformanceForm(Ship:TFreeShip;const Method:string):TForm;\nvar Form:TForm;\nbegin\nResult:=nil;Form:=nil;try',*factory,
        "raise Exception.Create('未知计算方法：'+Method);except Form.Free;raise end;end;",
        'procedure RunPerformanceForm(Form:TForm;const Method:string);begin',*runner,'end;',
        'procedure StorePerformanceForm(Ship:TFreeShip;Form:TForm;const Method:string);begin',*storer,'end;',
        'procedure DescribePerformanceForm(Form:TForm;const Method:string;Result:TJSONObject;Fields:TJSONArray);begin',*schema,'end;'])
    (work/'Units/PerformanceForms.inc').write_text(generated,encoding='utf8')
    path=work/'Units/PerformanceEngine.pas';s=path.read_text('utf8').replace('{PERFORMANCE_UNITS}',','.join(uses));path.write_text(s,encoding='utf8')
    # The original persisted calculator records are already accessed by
    # FreeEdit; expose the same records only in this disposable service copy.
    path=work/'Units/FreeShipUnit.pas';s=path.read_text('utf8');s=replace_once(s,'    FPropellerTask1Data:','  public\n    FPropellerTask1Data:');path.write_text(s,encoding='utf8')
    # Pin resistance->propeller exchange to the editing document and revision.
    # Legacy calculators may adjust scratch settings while producing a report.
    # Those scratch changes must not switch the cache key halfway through a job.
    s=path.read_text('utf8');s=replace_once(s,'  FreePropeller_Task5Dlg,','  FreePropeller_Task5Dlg, PerformanceHooks,');path.write_text(s,encoding='utf8')
    path=work/'Units/FreeShip.inc';s=path.read_text('utf8')
    pos=s.index('function TFreeShip.CalculationRoot: string;');start=s.index('\nbegin',pos)+6
    s=s[:start]+"\n  if PerformanceCacheKey<>'' then Exit(IncludeTrailingPathDelimiter(Preferences.TempDirectory)+PerformanceCacheKey);"+s[start:];path.write_text(s,encoding='utf8')
    # Reuse the established bounded process/validated-result implementation,
    # adding cooperative cancellation without pumping UI events.
    path=work/'Units/FreeProcess.pas';s=path.read_text('utf-8-sig')
    s=replace_once(s,'uses Pipes, FreeLogger, AsyncProcess, Forms;','uses Pipes, FreeLogger, AsyncProcess, Forms, PerformanceHooks;')
    s,n=re.subn(r'if Assigned\(CancelCheck\) and CancelCheck\(\) then',r'if PerformanceCancelled or (Assigned(CancelCheck) and CancelCheck()) then',s)
    if n!=2:raise RuntimeError('Process cancel sites changed')
    s=replace_once(s,'      Child.CloseInput;',"      // PropPred waits for Enter after writing its result; EOF alone loops.\n      if SameText(ExtractFileName(ExecutableName),'PropPred.exe') then\n        Child.Input.WriteBuffer(PChar(#13#10)^,2);\n      Child.CloseInput;")
    s=replace_once(s,'  if Run.State <> fpsSuccess then raise EFreePluginError.Create(ExecFullName, Run);',
        '  PerformanceProcessLog(ExecFullName,Run.OutputText,Run.ErrorText);\n  if Run.State <> fpsSuccess then raise EFreePluginError.Create(ExecFullName, Run);')
    path.write_text(s,encoding='utf8')
    path=work/'Units/FreeCalculationFiles.pas';s=path.read_text('utf-8-sig')
    s=replace_once(s,'FreeLogger, Forms;','FreeLogger, Forms, PerformanceHooks;')
    # Holtrop writes 20 resistance rows; the old check admitted at most ten.
    # Keep exact five-number rows and both headers, accept the real row limit.
    s=replace_once(s,'RequireLines(L,Name,4,13);','RequireLines(L,Name,4,23);')
    s=replace_once(s,'  if ActiveScope = Self then ActiveScope := nil;',
        '  PerformanceCapture(FDirectory);\n  if ActiveScope = Self then ActiveScope := nil;')
    s=replace_once(s,'  Result:=ExecuteFreePlugin(Directory,ExecutableName,TimeoutMS);',
        '  PerformanceCapture(Directory);\n  Result:=ExecuteFreePlugin(Directory,ExecutableName,TimeoutMS);')
    s=replace_once(s,'    ValidateFreeCalculationOutput(Directory,Protocol);','    ValidateFreeCalculationOutput(Directory,Protocol);\n    PerformanceCapture(Directory);')
    s=replace_once(s,'      fcPropol,fcFppcalc: begin', '''      fcFppcalc: begin
        RequireLines(L,Name,27,100); N:=0; SeriesNo:=0;
        for I:=25 to L.Count-1 do begin
          if Trim(L[I])='' then Continue;
          if SeriesNo<>0 then raise EFreeCalculationOutput.Create('Unexpected data after FPP summary');
          if NumericCount(L[I],Name)=7 then begin SeriesNo:=1; RequireNumbers(L[I],Name,7,7) end
          else begin RequireNumbers(L[I],Name,8,8);Inc(N) end;
        end;
        if (N<2) or (N>68) or (SeriesNo<>1) then raise EFreeCalculationOutput.Create('Incomplete FPP table or summary');
      end;
      fcPropol: begin''')
    path.write_text(s,encoding='utf8')
