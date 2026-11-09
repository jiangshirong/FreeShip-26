unit UiPersistence;
{$mode objfpc}{$H+}
interface
uses Main, fpjson;
procedure RunPersistence(Form: TMainForm; const Scope: string; Cases: TJSONArray);
implementation
uses SysUtils, Graphics, ActnList, FreeTypes, FreeGeometry, UiDiscovery;
procedure Add(Cases: TJSONArray; const Id,Status,Expected,Actual:string);
var O:TJSONObject;
begin
  O:=TJSONObject.Create; O.Add('id',Id); O.Add('status',Status);
  O.Add('expected',Expected); O.Add('actual',Actual); Cases.Add(O);
end;
procedure Check(Cases:TJSONArray; const Id:string; Ok:Boolean; const Expected,Actual:string);
begin
  if Ok then Add(Cases,Id,'pass',Expected,Actual) else Add(Cases,Id,'fail',Expected,Actual);
end;
procedure RunPersistence(Form:TMainForm; const Scope:string; Cases:TJSONArray);
var Path,Details:string; P,Named:TFreeSubdivisionControlPoint; C:T3DCoordinate; ColorBefore:TColor;
    I,NameMatches,SaveMatches:Integer; Ready,Loaded:Boolean; SaveAction:TAction;
begin
  if (Scope<>'all') and (Scope<>'persistence') then Exit;
  Path:=SysUtils.GetEnvironmentVariable('FS_UI_PROBE_MODEL');
  Ready:=SameText(ExpandFileName(Form.FreeShip.Filename),ExpandFileName(Path)) and
    FileExists(Path) and Form.FreeShip.FilenameSet and not Form.FreeShip.FileIsReadOnly;
  Check(Cases,'persistence.isolated-target',Ready,
    'loaded writable fixture copy is the only save target',Form.FreeShip.Filename);
  if not Ready then begin
    Add(Cases,'persistence.save-action','not_run','save fixture copy','writable isolated target unavailable');
    Add(Cases,'persistence.reopen-geometry','not_run','coordinates and point name preserved','save did not run');
    Add(Cases,'persistence.reopen-layer-color','not_run','layer color preserved','save did not run');
    Add(Cases,'persistence.editor','not_run','user edits coordinates in active editor','real focus not exercised');
    Exit;
  end;
  P:=Form.FreeShip.Surface.ControlPoint[5];
  C:=P.Coordinate; C.X:=18.75; C.Y:=2.25; C.Z:=7.5; P.Coordinate:=C;
  P.Name:='Roundtrip-Point-5';
  ColorBefore:=$00C17529; Form.FreeShip.Layer[0].Color:=ColorBefore;
  Form.FreeShip.FileVersion:=Form.FreeShip.DetectMinFileVersion;
  Form.FreeShip.FileChanged:=True;
  SaveAction:=FindActionByHandler(Form,@Form.FileSaveExecute,SaveMatches);
  if GetEnvironmentVariable('FS_UI_DYNAMIC_MAIN')<>'1' then SaveAction:=nil;
  if (SaveAction=nil) or not SaveAction.Enabled then begin
    Add(Cases,'persistence.save-action','not_run','unique enabled save action','missing, ambiguous, disabled or unapproved; matches='+IntToStr(SaveMatches));
    Add(Cases,'persistence.reopen-geometry','not_run','coordinates and name preserved','save did not run');
    Add(Cases,'persistence.reopen-layer-color','not_run','layer color preserved','save did not run');
    Add(Cases,'persistence.editor','not_run','real editor input','no physical focus or input');
    Exit;
  end;
  SaveAction.Execute;
  Check(Cases,'persistence.save-action',not Form.FreeShip.FileChanged and FileExists(Path),
    'save action writes the isolated fixture and clears modified state',
    'modified='+SysUtils.BoolToStr(Form.FreeShip.FileChanged,True));
  Form.FreeShip.ControlpointForm.ActiveControlPoint:=nil;
  Form.FreeShip.Edit.File_Load(Path);
  Loaded:=Form.FreeShip.ModelIsLoaded and (Form.FreeShip.Surface.NumberOfControlPoints=8);
  if Loaded then begin
    Named:=nil; NameMatches:=0; Details:='';
    for I:=0 to Form.FreeShip.Surface.NumberOfControlPoints-1 do begin
      P:=Form.FreeShip.Surface.ControlPoint[I]; C:=P.Coordinate;
      if P.Name='Roundtrip-Point-5' then begin Inc(NameMatches); Named:=P end;
      Details:=Details+Format('[%d] (%.3f, %.3f, %.3f) %s; ',[I,C.X,C.Y,C.Z,P.Name]);
    end;
    if Assigned(Named) then C:=Named.Coordinate;
    Check(Cases,'persistence.reopen-geometry',(NameMatches=1) and Assigned(Named) and
      (Abs(C.X-18.75)<0.0001) and (Abs(C.Y-2.25)<0.0001) and (Abs(C.Z-7.5)<0.0001),
      'one saved point named Roundtrip-Point-5 has coordinates (18.75, 2.25, 7.5); ordering may change',
      Details);
    Check(Cases,'persistence.reopen-layer-color',Form.FreeShip.Layer[0].Color=ColorBefore,
      'first layer color survives save and reload',IntToHex(Form.FreeShip.Layer[0].Color,8));
  end else begin
    Check(Cases,'persistence.reopen-geometry',False,'saved model reloads with 8 points','reload failed');
    Check(Cases,'persistence.reopen-layer-color',False,'saved model reloads with layer','reload failed');
  end;
  Add(Cases,'persistence.editor','not_run','user edits coordinates in active editor',
    'point and layer properties set in-process; real focus and typing are not exercised');
end;
end.
