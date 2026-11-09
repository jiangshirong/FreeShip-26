unit CalculationContractsProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure RunCalculationContracts(Form:TMainForm);
implementation
uses Classes, SysUtils, fpjson, jsonparser, SHA1, FreeCalculationFiles, FreeProcess;

const Names:array[TFreeCalculationProtocol] of string=(
  'OUTA.','OUT.','RVRSRES.dat','OUT.','RES1.tsk','RES2.tsk','RES3.tsk',
  'OUT.','OUT.','OUT.','fungleib.res','hollenba.res','OUT.TXT','OUT.',
  'ostres.tmp','oortmers.res','clempoup.res','clemblou.res','bunkov00.res',
  'Compton0.res','Wolfson0.res','Radojcic.res');

procedure RunCalculationContracts(Form:TMainForm);
var Report,Row:TJSONObject; Cases:TJSONArray; L:TStringList;
  Root,Dir,Original,ErrorText,Name,FirstDir,FirstSet,SecondSet,ExecDir:string;
  Job:TFreeCalculationScope; P:TFreeCalculationProtocol; I,J,Count:Integer;
  Failed:Boolean;
  Samples:TJSONArray; Sample:TJSONObject; InputName,OutputName,RealRoot:string;
  procedure Check(const ID,Expected:string; OK:Boolean; const Detail:string='');
  begin
    Row:=TJSONObject.Create;Row.Add('id','11.contract.'+ID);Row.Add('expected',Expected);
    if OK then Row.Add('status','pass') else Row.Add('status','fail');
    Row.Add('actual',Detail); Cases.Add(Row);
  end;
  procedure Save(const FileName,Text:string);
  var T:TStringList;
  begin T:=TStringList.Create;try T.Text:=Text;T.SaveToFile(FileName) finally T.Free end end;
  function Numbers(N:Integer):string;
  var K:Integer;
  begin Result:='';for K:=1 to N do Result:=Result+' 1.25';Result:=Result+LineEnding end;
  procedure Valid(Protocol:TFreeCalculationProtocol);
  var N,K:Integer;
  begin
    L.Clear;
    case Protocol of
      fcPropeller1:N:=45; fcPropeller2:N:=70; fcPropeller3:N:=165;
      fcRvrs:N:=120; fcFung,fcOortmer:N:=80; fcHollen:N:=90;
      fcSeaMargin:N:=1; fcClemPoup,fcWolfson:N:=60;fcClemBlou:N:=42;
      fcBunkov:N:=96; fcCompton:N:=66; fcRadojcic:N:=56;
      fcHship:N:=245;
      else N:=0;
    end;
    if N>0 then begin
      L.Text:=Numbers(N);
      if Protocol=fcHship then L.Text:='1 '+Numbers(N-1);
    end else begin
      case Protocol of
        fcAddedMass:N:=30;fcManeuv:N:=193;fcIshercof:N:=54;
        fcPropeller4:N:=59;fcPowerPrediction:N:=85;
        fcPropol:N:=31;fcFppcalc:N:=28;
      end;
      for K:=1 to N do L.Add('Report line');
      case Protocol of
        fcAddedMass:begin L[11]:=Numbers(2);L[19]:=Numbers(5) end;
        fcIshercof:for K:=35 to 53 do L[K]:=Trim(Numbers(7));
        fcPropeller4:begin
          for K:=46 to 55 do L[K]:='1.25';
          Save(Dir+DirectorySeparator+'POPGR.',Numbers(2)+Numbers(4)+Numbers(4));
        end;
        fcFppcalc:for K:=25 to N-1 do L[K]:=Trim(Numbers(8));
        fcPropol:begin
          for K:=25 to N-4 do L[K]:=Trim(Numbers(8));
          L[N-2]:='KT = 1 2 3 4';L[N-1]:='KQ = 1 2 3 4';
        end;
      end;
    end;
    // Do not embed newlines inside individual report lines.
    for K:=0 to L.Count-1 do L[K]:=Trim(L[K]);
    L.SaveToFile(Dir+DirectorySeparator+Names[Protocol]);
  end;
  function Rejected(Protocol:TFreeCalculationProtocol):Boolean;
  begin
    Result:=False;ErrorText:='';
    try ValidateFreeCalculationOutput(Dir,Protocol)
    except on E:EFreeCalculationOutput do begin Result:=True;ErrorText:=E.Message end end;
  end;
begin
  Root:=GetEnvironmentVariable('FS_CONTRACT_ROOT');Original:=GetCurrentDir;
  Report:=TJSONObject.Create;Cases:=TJSONArray.Create;Report.Add('cases',Cases);
  L:=TStringList.Create;Job:=nil;
  try
    try
      Job:=TFreeCalculationScope.Create(Root,'',[]);Dir:=Job.Directory;FirstDir:=Dir;
      Check('directory.private','calculation directory is unique and current',
        (Dir<>Root) and (GetCurrentDir=Dir),Dir);
      for P:=Low(P) to High(P) do begin
        Valid(P);Failed:=Rejected(P);
        Check(IntToStr(Ord(P))+'.complete','complete declared output is accepted',not Failed,ErrorText);
        Name:=Dir+DirectorySeparator+Names[P];DeleteFile(Name);
        Check(IntToStr(Ord(P))+'.missing','missing result is rejected',Rejected(P),ErrorText);
        Save(Name,'');
        Check(IntToStr(Ord(P))+'.empty','empty result is rejected',Rejected(P),ErrorText);
        Valid(P);Save(Name,'NaN');
        Check(IntToStr(Ord(P))+'.nan','non-finite or incomplete result is rejected',Rejected(P),ErrorText);
        Valid(P);
        if P in [fcAddedMass,fcManeuv,fcIshercof,fcPropeller4,fcPowerPrediction,fcPropol,fcFppcalc] then begin
          L.LoadFromFile(Name);
          if P in [fcPropol,fcFppcalc] then L[L.Count-1]:='1 2' else L.Clear;
          L.SaveToFile(Name);
        end else Save(Name,Numbers(2));
        Check(IntToStr(Ord(P))+'.truncated','short required table/report is rejected',Rejected(P),ErrorText);
      end;
      Valid(fcPropeller4);DeleteFile(Dir+DirectorySeparator+'POPGR.');
      Check('paired','OUT without the companion graph is rejected',Rejected(fcPropeller4),ErrorText);
      Valid(fcPropeller1);Save(Dir+DirectorySeparator+'TMP1.tsk','input');
      Failed:=False;
      try ExecuteFreeCalculation(Dir,GetEnvironmentVariable('FS_CONTRACT_HELPER'),fcPropeller1,2000)
      except on E:EFreeCalculationOutput do Failed:=True end;
      Check('stale','zero exit cannot reuse a previously valid result',Failed and not FileExists(Dir+DirectorySeparator+'RES1.tsk'));
      FreeAndNil(Job);
      Check('restore','cwd restored and owned job removed',
        (GetCurrentDir=Original) and not DirectoryExists(FirstDir));
      Job:=TFreeCalculationScope.Create(Root,'',[]);Dir:=Job.Directory;
      Check('unique','next run has a different directory',Dir<>FirstDir);
      Save(Dir+DirectorySeparator+'RESIST.dat',Numbers(25));Job.Complete;
      FreeAndNil(Job);FirstSet:=FreeCalculationDirectory(Root);
      Check('published','validated generation can be read',FileExists(FirstSet+DirectorySeparator+'RESIST.dat'));
      Job:=TFreeCalculationScope.Create(Root,'',['RESIST.dat']);Dir:=Job.Directory;
      Save(Dir+DirectorySeparator+'RESIST.dat','NaN');Failed:=False;
      try Job.Complete except on E:EFreeCalculationOutput do Failed:=True end;
      FreeAndNil(Job);SecondSet:=FreeCalculationDirectory(Root);
      Check('failed-generation','failed recomputation leaves no current resistance result',
        Failed and (SecondSet<>FirstSet) and not FileExists(SecondSet+DirectorySeparator+'RESIST.dat'));
      Check('immutable','previous generation remains unchanged',FileExists(FirstSet+DirectorySeparator+'RESIST.dat'));
      // Reference files come from a direct isolated control run of the
      // author's binaries, with the exact same inputs and no product wrapper.
      ExecDir:=GetEnvironmentVariable('FS_CONTRACT_SOLVERS');
      if ExecDir<>'' then begin
        RealRoot:=GetEnvironmentVariable('FS_CONTRACT_REAL');
        L.LoadFromFile(RealRoot+DirectorySeparator+'manifest.json');
        Samples:=TJSONArray(GetJSON(L.Text));
        try
          for I:=0 to Samples.Count-1 do begin
            Sample:=Samples.Objects[I];
            InputName:=Sample.Get('input','');OutputName:=Sample.Get('output','');
            Job:=TFreeCalculationScope.Create(Root,ExecDir,[]);Dir:=Job.Directory;
            L.LoadFromFile(RealRoot+DirectorySeparator+InputName);
            L.SaveToFile(Dir+DirectorySeparator+InputName);
            ExecuteFreeCalculation(Dir,ExecDir+DirectorySeparator+Sample.Get('exe',''),
              TFreeCalculationProtocol(Sample.Get('protocol',0)),15000);
            Check('real.'+Sample.Get('id',''),'isolated real solver output equals direct control output',
              SHA1Print(SHA1File(Dir+DirectorySeparator+OutputName))=
              SHA1Print(SHA1File(RealRoot+DirectorySeparator+OutputName)));
            FreeAndNil(Job);
          end;
        finally Samples.Free end;
      end;
    except on E:Exception do Report.Add('error',E.ClassName+': '+E.Message) end;
    L.Text:=Report.FormatJSON;L.SaveToFile(GetEnvironmentVariable('FS_CONTRACT_REPORT'));
  finally Job.Free;L.Free;Report.Free end;
end;
end.
