{ GPL-2.0-or-later. Original FREE!ship/Plus calculators, isolated Qt adapter. }
unit PerformanceEngine;
{$mode objfpc}{$H+}{$codepage utf8}
interface
uses Classes,SysUtils,fpjson,FreeShipUnit;
function Performance(Ship:TFreeShip;Request:TJSONObject;const Output:string;Run:Boolean):TJSONObject;
procedure SavePerformanceSettings(Ship:TFreeShip;Request:TJSONObject;const Output:string);
implementation
uses Math,Forms,Controls,StdCtrls,ExtCtrls,Spin,ComCtrls,Graphics,TAGraph,TASeries,
  TACustomSeries,TAChartAxis,FreeTypes,FreeGeometry,PerformanceHooks,{PERFORMANCE_UNITS};

function CaptionOf(Form:TForm;const Name:string):string;
var C:TComponent;
begin
  Result:='';C:=Form.FindComponent(Name);
  if C is TControl then Result:=TControl(C).Caption;
end;
procedure DescribeField(Form:TForm;Fields:TJSONArray;const Name,LabelName,UnitName,GroupName:string);
var C:TComponent;F:TJSONObject;A:TJSONArray;I:Integer;Title,UnitText:string;
begin
  C:=Form.FindComponent(Name);if C=nil then raise Exception.Create('计算参数控件缺失：'+Name);
  F:=TJSONObject.Create;Fields.Add(F);F.Add('key',Name);
  Title:=Trim(CaptionOf(Form,LabelName));if Title='' then Title:=Trim(CaptionOf(Form,Name));
  if Title='' then Title:=Name;
  UnitText:=Trim(CaptionOf(Form,UnitName));
  F.Add('label',Title);F.Add('unit',UnitText);F.Add('group',Trim(CaptionOf(Form,GroupName)));
  F.Add('source_label',LabelName);F.Add('source_unit',UnitName);
  F.Add('visible',TControl(C).Visible);F.Add('enabled',TControl(C).Enabled);
  if C is TFloatSpinEdit then begin
    F.Add('kind','number');F.Add('value',TFloatSpinEdit(C).Value);
    F.Add('decimals',TFloatSpinEdit(C).DecimalPlaces);
    if TFloatSpinEdit(C).MaxValue>TFloatSpinEdit(C).MinValue then begin
      F.Add('minimum',TFloatSpinEdit(C).MinValue);F.Add('maximum',TFloatSpinEdit(C).MaxValue)
    end else begin F.Add('minimum',-1e9);F.Add('maximum',1e9) end;
  end else if C is TSpinEdit then begin
    F.Add('kind','number');F.Add('value',TSpinEdit(C).Value);F.Add('decimals',0);
    F.Add('minimum',TSpinEdit(C).MinValue);F.Add('maximum',TSpinEdit(C).MaxValue);
  end else if C is TCheckBox then begin F.Add('kind','boolean');F.Add('value',TCheckBox(C).Checked) end
  else begin
    F.Add('kind','choice');A:=TJSONArray.Create;F.Add('choices',A);
    if C is TComboBox then begin
      F.Add('value',TComboBox(C).ItemIndex);for I:=0 to TComboBox(C).Items.Count-1 do A.Add(TComboBox(C).Items[I]);
    end else if C is TRadioGroup then begin
      F.Add('value',TRadioGroup(C).ItemIndex);for I:=0 to TRadioGroup(C).Items.Count-1 do A.Add(TRadioGroup(C).Items[I]);
    end else raise Exception.Create('不支持的计算参数：'+Name);
  end;
end;
{$I PerformanceForms.inc}

function Schema(Form:TForm;const Method:string):TJSONObject;
var Fields:TJSONArray;I:Integer;F:TJSONObject;Key:string;
begin Result:=TJSONObject.Create;Fields:=TJSONArray.Create;Result.Add('fields',Fields);
  DescribePerformanceForm(Form,Method,Result,Fields);Result.Add('calculator',Method);
  for I:=0 to Fields.Count-1 do begin
    F:=Fields.Objects[I];Key:=F.Strings['key'];
    if (Method<>'delft') and (Copy(Method,1,9)<>'propeller') then begin
      if Key='EndSpeedBox' then F.Strings['label']:='设计航速';
      if Key='StepSpeedBox' then F.Strings['label']:='最大航速';
      if Key='DraftBox' then F.Strings['label']:='纵倾差';
    end;
    if SameText(Key,'Losbox') then F.Strings['unit']:='m';
    if Key='ViscosityBox' then begin
      if Pos('ft',F.Strings['unit'])>0 then F.Strings['unit']:='10⁻⁶ ft²/s' else F.Strings['unit']:='10⁻⁶ m²/s';
    end;
    if (Method='propeller2') and ((Copy(Key,1,7)='Edit17_') or (Copy(Key,1,7)='Edit18_')) then
      F.Strings['label']:=F.Strings['label']+' '+Copy(Key,8,1);
    if (Method='ost') or (Method='rbhs') or (Method='mh') then begin
      if Copy(Key,1,7)='Edit17_' then F.Strings['label']:='弗劳德数 Fr '+Copy(Key,8,1);
      if Copy(Key,1,7)='Edit18_' then F.Strings['label']:='剩余阻力系数 Cr '+Copy(Key,8,1);
    end;
    if (Method='hollenbach') and (Key='StepSpeedBox') then F.Floats['maximum']:=1050;
  end;
end;
procedure ApplyInputs(Form:TForm;Schema,Request:TJSONObject);
var Values:TJSONObject;Fields:TJSONArray;F:TJSONObject;D:TJSONData;C:TComponent;I,J,Pass,Order,ChoiceCount:Integer;Key,Kind:string;Value:Double;Found:Boolean;
begin
  D:=Request.Find('values');if D=nil then Exit;
  if D.JSONType<>jtObject then raise Exception.Create('计算参数必须为对象。');Values:=TJSONObject(D);
  Fields:=Schema.Arrays['fields'];
  // Apply dependent choices before scalar values, independent of JSON key order.
  for Pass:=0 to 2 do for I:=0 to Values.Count-1 do begin
    Key:=Values.Names[I];Found:=False;
    for J:=0 to Fields.Count-1 do begin
      F:=Fields.Objects[J];if F.Strings['key']<>Key then Continue;Found:=True;
      C:=Form.FindComponent(Key);D:=Values.Items[I];Kind:=F.Strings['kind'];
      Order:=2;if Kind='choice' then begin Order:=1;if SameText(Key,'ComboBox') then Order:=0 end;
      if Order<>Pass then Break;
      if Kind='boolean' then begin
        if D.JSONType<>jtBoolean then raise Exception.Create('开关值无效：'+Key);
        TCheckBox(C).Checked:=D.AsBoolean;
      end else begin
        if D.JSONType<>jtNumber then raise Exception.Create('参数不是数值：'+Key);Value:=D.AsFloat;
        if IsNan(Value) or IsInfinite(Value) then raise Exception.Create('参数不是有限数值：'+Key);
        if Kind='number' then begin
          if (Value<F.Floats['minimum']) or (Value>F.Floats['maximum']) then raise Exception.Create('参数超出范围：'+F.Strings['label']);
          if C is TFloatSpinEdit then TFloatSpinEdit(C).Value:=Value else TSpinEdit(C).Value:=Round(Value);
        end else begin
          ChoiceCount:=F.Arrays['choices'].Count;if C is TComboBox then ChoiceCount:=TComboBox(C).Items.Count;
          if (Value<>Trunc(Value)) or (Value<-1) or (Value>=ChoiceCount) then raise Exception.Create('选项无效：'+F.Strings['label']);
          if C is TComboBox then begin
            TComboBox(C).ItemIndex:=Round(Value);
            if Assigned(TComboBox(C).OnClick) then TComboBox(C).OnClick(C);
            if Assigned(TComboBox(C).OnChange) then TComboBox(C).OnChange(C);
          end
          else TRadioGroup(C).ItemIndex:=Round(Value);
        end;
      end;
      Break;
    end;
    if not Found then raise Exception.Create('未知计算参数：'+Key);
  end;
end;
procedure Configure(Ship:TFreeShip;Request:TJSONObject;const Output:string);
var Dir,Method:string;
begin
  Method:=Request.Get('calculator','');
  if (Ship.ProjectSettings.ProjectUnits=fuImperial) and (Method<>'delft') and (Method<>'kaper') then
    raise Exception.Create('该方法仅支持公制。请先在工程设置中转换单位。');
  // Caller output is an owned request directory. Cache is private to this Qt
  // session; the legacy key includes geometry, units, draft and density.
  Ship.Preferences.TempDirectory:=IncludeTrailingPathDelimiter(ExtractFileDir(ExcludeTrailingPathDelimiter(Output)))+'performance-cache';
  ForceDirectories(Ship.Preferences.TempDirectory);
  PerformanceCacheKey:='doc-'+StringReplace(Request.Get('expected_document_id',''),'-','',[rfReplaceAll])+'-r'+IntToStr(Request.Get('expected_revision',Int64(0)));
  Dir:=Request.Get('solver_directory','');
  if Dir<>'' then begin
    if not DirectoryExists(Dir) then raise Exception.Create('计算程序目录不存在：'+Dir);
    Ship.Preferences.ExecDirectory:=ExpandFileName(Dir);
  end;
end;
procedure ExtractInputs(Form:TForm);
var C:TComponent;
begin
  C:=Form.FindComponent('CheckBox2');
  if not (C is TCheckBox) or not Assigned(TCheckBox(C).OnClick) then raise Exception.Create('此方法没有参数提取入口。');
  TCheckBox(C).Checked:=True;
  try TCheckBox(C).OnClick(C) finally
    TCheckBox(C).Checked:=False;TCheckBox(C).OnClick(C);
  end;
  if PerformanceWarnings.Count>0 then raise Exception.Create(PerformanceWarnings.Text);
end;
procedure ValidateWork(Form:TForm;const Method:string);
var A,B,C:TComponent;Start,Finish,Step:Double;Choice:string;
begin
  if (Method='propeller5') and (TComboBox(Form.FindComponent('ComboBox')).ItemIndex>0) then begin
    A:=Form.FindComponent('ComboBox1');
    Choice:='unknown';if TComboBox(A).ItemIndex>=0 then Choice:=TComboBox(A).Items[TComboBox(A).ItemIndex];
    if (Pos('reserved',LowerCase(Choice))>0) or (Pos('unknown',LowerCase(Choice))>0) or
       (Pos('未知',Choice)>0) or (Pos('保留',Choice)>0) then raise Exception.Create('请选择已提供的螺旋桨系列。');
  end;
  if (Method='planing') and (TComboBox(Form.FindComponent('ComboBox')).ItemIndex=9) then
    raise Exception.Create('原版仅列出 Almeter 名称，未提供该方法的计算实现。');
  A:=Form.FindComponent('StartSpeedBox');B:=Form.FindComponent('EndSpeedBox');C:=Form.FindComponent('StepSpeedBox');
  if (A is TFloatSpinEdit) and (B is TFloatSpinEdit) and (C is TFloatSpinEdit) then begin
    Start:=TFloatSpinEdit(A).Value;Finish:=TFloatSpinEdit(B).Value;Step:=TFloatSpinEdit(C).Value;
    if Method='delft' then begin
      if (Start<0) or (Finish<=Start) or (Step<=0) or ((Finish-Start)/Step>2000) then
        raise Exception.Create('航速范围无效：终止航速须大于起始航速，间隔须大于零，最多 2001 个点。');
    end else if (Start<=0) or (Finish<=Start) or (Step<=Finish) then
      raise Exception.Create('航速须满足：0 < 起始航速 < 设计航速 < 最大航速。');
  end;
end;
function Collect(Form:TForm;const Method:string):TJSONObject;
var I,J,K,Points:Integer;C:TComponent;Chart:TChart;S:TChartSeries;Series,XY,Notes:TJSONArray;O:TJSONObject;X,Y:Double;Text,Help,Axis,RUnit,PUnit:string;Power:Boolean;
begin
  Result:=TJSONObject.Create;Series:=TJSONArray.Create;Result.Add('series',Series);Points:=0;Text:='';Help:='';
  try
    for I:=0 to Form.ComponentCount-1 do begin
      C:=Form.Components[I];
      if C is TMemo then begin
        if SameText(C.Name,'ResultsMemo') then Text:=TMemo(C).Lines.Text
        else if SameText(C.Name,'ResultsMemo2') then Help:=TMemo(C).Lines.Text;
      end;
      if not (C is TChart) then Continue;Chart:=TChart(C);
      for J:=0 to Chart.SeriesCount-1 do begin
        if not (Chart.Series[J] is TChartSeries) then Continue;S:=TChartSeries(Chart.Series[J]);
        if S.Count=0 then Continue;
        O:=TJSONObject.Create;Series.Add(O);O.Add('name',S.Title);O.Add('source',S.Name);
        O.Add('x_title',Chart.BottomAxis.Title.Caption);
        if S.AxisIndexY>=0 then O.Add('y_title',Chart.AxisList[S.AxisIndexY].Title.Caption)
        else O.Add('y_title',Chart.LeftAxis.Title.Caption);
        O.Add('source_x_title',O.Strings['x_title']);O.Add('source_y_title',O.Strings['y_title']);
        // Original combined charts sometimes bind power to the resistance axis.
        // Retain every raw coordinate, label each selected curve by its equation.
        if Copy(Method,1,9)<>'propeller' then O.Strings['x_title']:='航速 (kn)';
        Axis:=Chart.LeftAxis.Title.Caption;RUnit:='kN';PUnit:='kW';
        if ((Method='holtrop') or (Method='ost') or (Method='rbhs') or (Method='mh')) and (Pos('kN',Axis)=0) then begin RUnit:='N';PUnit:='W' end;
        Power:=(S.Name='Series3') or (S.Name='Series4');
        if (Method='holtrop') or (Method='hollenbach') or (Method='oortmerssen') then begin
          if Power then O.Strings['y_title']:='有效功率 ('+PUnit+')' else O.Strings['y_title']:='阻力 ('+RUnit+')';
        end;
        if (Method='fung') or (Method='ost') or (Method='rbhs') or (Method='mh') then begin
          if Power then O.Strings['y_title']:='有效功率 ('+PUnit+')' else O.Strings['y_title']:='10 × 阻力 ('+RUnit+')';
        end;
        if Method='planing' then begin
          if S.Name='Series4' then O.Strings['y_title']:='有效功率 (kW)'
          else if S.Name='Series5' then O.Strings['y_title']:=Trim(S.Title)+' (°)'
          else O.Strings['y_title']:='10 × 阻力 (kN)';
        end;
        if (Method='propeller4') or (Method='propeller5') then begin
          if S.Name='Series1' then O.Strings['y_title']:='推力系数 KT'
          else if S.Name='Series2' then O.Strings['y_title']:='10 × 转矩系数 KQ'
          else if S.Name='Series3' then O.Strings['y_title']:='敞水效率 η₀'
          else O.Strings['y_title']:='工作点参考线（无量纲）';
        end;
        XY:=TJSONArray.Create;O.Add('points',XY);
        for K:=0 to S.Count-1 do begin
          X:=S.XValue[K];Y:=S.YValue[K];
          if IsNan(X) or IsInfinite(X) or IsNan(Y) or IsInfinite(Y) then raise Exception.Create('计算产生非有限曲线数据。');
          XY.Add(TJSONArray.Create([X,Y]));Inc(Points);
        end;
      end;
    end;
    if (Trim(Text)='') or (Points=0) then raise Exception.Create('未得到有效计算曲线。请检查参数及方法适用范围。'+LineEnding+PerformanceWarnings.Text+Copy(Text,Max(1,Length(Text)-1200),1201));
    Result.Add('text',Text);Result.Add('help',Help);Notes:=TJSONArray.Create;Result.Add('warnings',Notes);
    for I:=0 to PerformanceWarnings.Count-1 do Notes.Add(PerformanceWarnings[I]);
  except Result.Free;raise end;
end;
function Performance(Ship:TFreeShip;Request:TJSONObject;const Output:string;Run:Boolean):TJSONObject;
var Form:TForm;Before,After:TJSONObject;Method:string;
begin
  Result:=nil;Form:=nil;Before:=nil;After:=nil;Method:=Request.Get('calculator','');
  PerformancePreparing:=True;PerformanceCancelFile:=IncludeTrailingPathDelimiter(Output)+'cancel.flag';PerformanceStarted:=GetTickCount64;
  PerformanceOutput:=Output;
  PerformanceWarnings:=TStringList.Create;
  try
    PerformanceCheck;Configure(Ship,Request,Output);Form:=CreatePerformanceForm(Ship,Method);Before:=Schema(Form,Method);ApplyInputs(Form,Before,Request);
    if Request.Get('extract',False) then ExtractInputs(Form);
    After:=Schema(Form,Method);
    if Run then begin
      ValidateWork(Form,Method);PerformanceCheck;PerformancePreparing:=False;RunPerformanceForm(Form,Method);PerformanceCheck;
      Result:=Collect(Form,Method);After.Free;After:=Schema(Form,Method);Result.Add('parameters',After);After:=nil;
      Result.Add('requested',Request.Clone);
      Result.Add('calculator',Method);Result.Add('units',Ord(Ship.ProjectSettings.ProjectUnits));Result.Add('elapsed_ms',GetTickCount64-PerformanceStarted);
      Result.Add('engine','FREE!ship/Plus calculator; Qt interface');
    end else begin Result:=After;After:=nil;if DirectoryExists(Ship.Preferences.ExecDirectory) then Result.Add('solver_directory',Ship.Preferences.ExecDirectory) else Result.Add('solver_directory','') end;
  finally
    PerformancePreparing:=True;PerformanceCancelFile:='';PerformanceStarted:=0;PerformanceOutput:='';PerformanceCacheKey:='';
    FreeAndNil(PerformanceWarnings);Before.Free;After.Free;Form.Free;
  end;
end;
procedure SavePerformanceSettings(Ship:TFreeShip;Request:TJSONObject;const Output:string);
var Form:TForm;Info:TJSONObject;Method:string;
begin
  Form:=nil;Info:=nil;PerformancePreparing:=True;Method:=Request.Get('calculator','');
  try
    Configure(Ship,Request,Output);Form:=CreatePerformanceForm(Ship,Method);Info:=Schema(Form,Method);ApplyInputs(Form,Info,Request);
    StorePerformanceForm(Ship,Form,Method);
  finally Form.Free;Info.Free;PerformanceCacheKey:='' end;
end;
end.
