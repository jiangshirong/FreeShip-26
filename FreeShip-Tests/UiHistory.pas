unit UiHistory;
{$mode objfpc}{$H+}
interface
uses Main, fpjson;
procedure RunHistory(Form:TMainForm; const Scope:string; Cases:TJSONArray);
implementation
uses SysUtils, Graphics, ActnList, FreeGeometry, FreeTypes, UiDiscovery;
procedure Check(Cases:TJSONArray; const Id:string; Ok:Boolean; const Expected,Actual:string);
var O:TJSONObject;
begin O:=TJSONObject.Create; O.Add('id',Id); if Ok then O.Add('status','pass') else O.Add('status','fail');
  O.Add('expected',Expected); O.Add('actual',Actual); Cases.Add(O) end;
procedure RunHistory(Form:TMainForm; const Scope:string; Cases:TJSONArray);
var P:TFreeSubdivisionControlPoint; C:T3DCoordinate; OrigX,NewX:Double;
    OrigColor,NewColor:TColor; Found,UndoDone:Boolean; UndoAction,RedoAction:TAction; UndoMatches,RedoMatches:Integer;
  function StateMatches(X:Double; Color:TColor):Boolean;
  var I:Integer;
  begin
    Result:=False;
    for I:=0 to Form.FreeShip.Surface.NumberOfControlPoints-1 do begin
      P:=Form.FreeShip.Surface.ControlPoint[I];
      if P.Name='HistoryProbe' then
        Exit((Abs(P.Coordinate.X-X)<0.0001) and (Form.FreeShip.Layer[0].Color=Color));
    end;
  end;
begin
  if (Scope<>'all') and (Scope<>'history') then Exit;
  P:=Form.FreeShip.Surface.ControlPoint[0]; P.Name:='HistoryProbe'; C:=P.Coordinate;
  OrigX:=C.X; NewX:=OrigX+0.125; OrigColor:=Form.FreeShip.Layer[0].Color; NewColor:=$00543210;
  if OrigColor=NewColor then NewColor:=$00123456;
  Form.FreeShip.Edit.Undo_Clear;
  Form.FreeShip.Edit.CreateUndoObject('history test',True);
  C.X:=NewX; P.Coordinate:=C; Form.FreeShip.Layer[0].Color:=NewColor;
  Form.FreeShip.FileChanged:=True; Form.UpdateMenu;
  UndoAction:=FindActionByHandler(Form,@Form.UndoExecute,UndoMatches);
  RedoAction:=FindActionByHandler(Form,@Form.RedoExecute,RedoMatches);
  if GetEnvironmentVariable('FS_UI_DYNAMIC_MAIN')<>'1' then begin UndoAction:=nil; RedoAction:=nil end;
  UndoDone:=False;
  if Assigned(UndoAction) then begin
    Found:=UndoAction.Enabled;
    Check(Cases,'history.undo-enabled',Found,'discovered undo action enabled after edit',SysUtils.BoolToStr(Found,True));
    if Found then begin UndoAction.Execute; UndoDone:=StateMatches(OrigX,OrigColor);
      Check(Cases,'history.undo',UndoDone,'undo restores point coordinate and layer color together','observed model after discovered action')
    end else begin
      Cases.Add(TJSONObject.Create(['id','history.undo','status','not_run','expected','undo model state','actual','action disabled']));
    end;
  end else begin
    Cases.Add(TJSONObject.Create(['id','history.undo-enabled','status','not_run','expected','unique undo action','actual','missing, ambiguous or source unapproved']));
    Cases.Add(TJSONObject.Create(['id','history.undo','status','not_run','expected','undo model state','actual','undo action unavailable']));
  end;
  Form.UpdateMenu;
  if Assigned(RedoAction) and UndoDone then begin
    Found:=RedoAction.Enabled;
    Check(Cases,'history.redo-enabled',Found,'discovered redo action enabled after undo',SysUtils.BoolToStr(Found,True));
    if Found then begin RedoAction.Execute;
      Check(Cases,'history.redo',StateMatches(NewX,NewColor),'redo restores edited point coordinate and layer color together','observed model after discovered action')
    end else Cases.Add(TJSONObject.Create(['id','history.redo','status','not_run','expected','redo model state','actual','action disabled']));
  end else begin
    Cases.Add(TJSONObject.Create(['id','history.redo-enabled','status','not_run','expected','unique redo after successful undo','actual','missing, ambiguous, unapproved or undo did not complete']));
    Cases.Add(TJSONObject.Create(['id','history.redo','status','not_run','expected','redo model state','actual','redo prerequisites unavailable']));
  end;
end;
end.
