unit UiPerformance;
{$mode objfpc}{$H+}
interface
uses Main, fpjson;
procedure RunPerformance(Form: TMainForm; Root: TJSONObject; Cases: TJSONArray);
implementation
uses SysUtils, UiTrace;
procedure RunPerformance(Form: TMainForm; Root: TJSONObject; Cases: TJSONArray);
var Path:string; O:TJSONObject; Exists,Loaded:Boolean;
begin
  Path:=GetEnvironmentVariable('FS_UI_PROBE_MODEL');
  BeginStageTrace; MarkStage('start');
  Form.FormShow(Form); MarkStage('form_shown_handler');
  Exists:=FileExists(Path);
  if Exists then begin
    MarkStage('load_begin'); Form.LoadNamedFile(Path); MarkStage('load_returned');
  end;
  Loaded:=Exists and Form.FreeShip.ModelIsLoaded;
  O:=TJSONObject.Create; O.Add('id','load');
  if Loaded then O.Add('status','pass') else O.Add('status','fail');
  O.Add('expected','copied model exists and loads without a dialog');
  O.Add('actual',Format('file-exists=%s; loaded=%s; control-points=%d; views=%d',
    [BoolToStr(Exists,True),BoolToStr(Loaded,True),Form.FreeShip.Surface.NumberOfControlPoints,Form.MDIChildCount]));
  Cases.Add(O);
  Root.Add('timing_trace',EndStageTrace);
end;
end.
