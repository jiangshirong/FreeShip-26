{ Test-only observation of original LCL calculators, without the Qt adapter. }
unit PerformanceObserver;
{$mode objfpc}{$H+}
interface
uses Classes,SysUtils,Forms,fpjson;
var OracleRequest:TJSONObject;OraclePreparing:Boolean=True;
procedure LoadOracleRequest(const Path:string);
procedure OracleInputs(Form:TForm);
procedure OracleCapture(Form:TForm);
implementation
uses jsonparser,StdCtrls,Spin,ExtCtrls,TAGraph,TACustomSeries,Math;
procedure LoadOracleRequest(const Path:string);
var L:TStringList;
begin L:=TStringList.Create;try L.LoadFromFile(Path);OracleRequest:=TJSONObject(GetJSON(L.Text)) finally L.Free end end;
procedure OracleInputs(Form:TForm);
var Values:TJSONObject;I:Integer;C:TComponent;
begin
  WriteLn('OracleInputs: '+Form.ClassName);
  Values:=OracleRequest.Objects['values'];
  for I:=0 to Values.Count-1 do begin
    C:=Form.FindComponent(Values.Names[I]);if C=nil then raise Exception.Create('Unknown oracle input '+Values.Names[I]);
    if C is TFloatSpinEdit then TFloatSpinEdit(C).Value:=Values.Items[I].AsFloat
    else if C is TCheckBox then TCheckBox(C).Checked:=Values.Items[I].AsBoolean
    else if C is TComboBox then begin TComboBox(C).ItemIndex:=Values.Items[I].AsInteger;if Assigned(TComboBox(C).OnClick) then TComboBox(C).OnClick(C) end
    else raise Exception.Create('Unsupported oracle input '+C.Name);
  end;
  OraclePreparing:=False;
  WriteLn('Inputs applied');
end;
procedure OracleCapture(Form:TForm);
var Root,Curve:TJSONObject;Series,Points:TJSONArray;I,J,K:Integer;C:TComponent;S:TChartSeries;L:TStringList;
begin
  Root:=TJSONObject.Create;L:=TStringList.Create;
  try
    Series:=TJSONArray.Create;Root.Add('series',Series);
    for I:=0 to Form.ComponentCount-1 do begin
      C:=Form.Components[I];if not(C is TChart) then Continue;
      for J:=0 to TChart(C).SeriesCount-1 do begin
        if not(TChart(C).Series[J] is TChartSeries) then Continue;S:=TChartSeries(TChart(C).Series[J]);if S.Count=0 then Continue;
        Curve:=TJSONObject.Create;Series.Add(Curve);Curve.Add('source',S.Name);Points:=TJSONArray.Create;Curve.Add('points',Points);
        for K:=0 to S.Count-1 do Points.Add(TJSONArray.Create([S.XValue[K],S.YValue[K]]));
      end;
    end;
    L.Text:=Root.AsJSON;L.SaveToFile(ParamStr(3)+'/oracle.json');
  finally Root.Free;L.Free end;
end;
end.
