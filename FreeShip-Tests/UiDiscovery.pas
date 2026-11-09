unit UiDiscovery;
{$mode objfpc}{$H+}
interface
uses Classes, ActnList, Menus, StdCtrls;
procedure FindComponentsByEvent(Root:TComponent; const EventName:string; Handler:TMethod; Results:TFPList);
function FindEditByHandler(Root:TComponent; Handler:TNotifyEvent; out Matches:Integer):TEdit;
function FindActionByHandler(Root:TComponent; Handler:TNotifyEvent; out Matches:Integer):TAction;
function FindMenuItemByHandler(Root:TComponent; Handler:TNotifyEvent; Tag:PtrInt; out Matches:Integer):TMenuItem;
implementation
uses TypInfo;
procedure FindComponentsByEvent(Root:TComponent; const EventName:string; Handler:TMethod; Results:TFPList);
var Seen:TFPList;
  procedure Walk(C:TComponent);
  var I:Integer; Prop:PPropInfo; E:TMethod;
  begin
    if (C=nil) or (Seen.IndexOf(C)>=0) then Exit;
    Seen.Add(C);
    Prop:=GetPropInfo(C,EventName);
    if Assigned(Prop) and (Prop^.PropType^.Kind=tkMethod) then begin
      E:=GetMethodProp(C,Prop);
      if Assigned(E.Code) and (E.Code=Handler.Code) and (E.Data=Handler.Data) then Results.Add(C);
    end;
    for I:=0 to C.ComponentCount-1 do Walk(C.Components[I]);
  end;
begin
  Seen:=TFPList.Create;
  try if Assigned(Handler.Code) then Walk(Root) finally Seen.Free end;
end;
function FindEditByHandler(Root:TComponent; Handler:TNotifyEvent; out Matches:Integer):TEdit;
var Items:TFPList; I:Integer;
begin
  Result:=nil; Matches:=0; Items:=TFPList.Create;
  try
    FindComponentsByEvent(Root,'OnEditingDone',TMethod(Handler),Items);
    for I:=0 to Items.Count-1 do if TObject(Items[I]) is TEdit then begin
      Inc(Matches); Result:=TEdit(Items[I]);
    end;
    if Matches<>1 then Result:=nil;
  finally Items.Free end;
end;
function FindMenuItemByHandler(Root:TComponent; Handler:TNotifyEvent; Tag:PtrInt; out Matches:Integer):TMenuItem;
var Seen:TFPList; Found:TMenuItem;
  procedure Walk(C:TComponent);
  var I:Integer; M:TMenuItem; E:TNotifyEvent;
  begin
    if (C=nil) or (Seen.IndexOf(C)>=0) then Exit;
    Seen.Add(C);
    if C is TMenuItem then begin
      M:=TMenuItem(C); E:=M.OnClick;
      if Assigned(E) and (M.Tag=Tag) and (TMethod(E).Code=TMethod(Handler).Code) and
        (TMethod(E).Data=TMethod(Handler).Data) then begin Inc(Matches); Found:=M end;
    end;
    for I:=0 to C.ComponentCount-1 do Walk(C.Components[I]);
  end;
begin
  Matches:=0; Found:=nil; Seen:=TFPList.Create;
  try if Assigned(Handler) then Walk(Root) finally Seen.Free end;
  if Matches=1 then Result:=Found else Result:=nil;
end;
function FindActionByHandler(Root:TComponent; Handler:TNotifyEvent; out Matches:Integer):TAction;
var Seen:TFPList; Found:TAction;
  procedure Walk(C:TComponent);
  var I:Integer; A:TAction; E:TNotifyEvent;
  begin
    if (C=nil) or (Seen.IndexOf(C)>=0) then Exit;
    Seen.Add(C);
    if C is TAction then begin
      A:=TAction(C); E:=A.OnExecute;
      if Assigned(E) and (TMethod(E).Code=TMethod(Handler).Code) and
        (TMethod(E).Data=TMethod(Handler).Data) then begin Inc(Matches); Found:=A end;
    end;
    for I:=0 to C.ComponentCount-1 do Walk(C.Components[I]);
  end;
begin
  Matches:=0; Found:=nil; Seen:=TFPList.Create;
  try if Assigned(Handler) then Walk(Root) finally Seen.Free end;
  if Matches=1 then Result:=Found else Result:=nil;
end;
end.
