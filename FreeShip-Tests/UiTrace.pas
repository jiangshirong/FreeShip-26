unit UiTrace;
{$mode objfpc}{$H+}
interface
uses fpjson, Dialogs;
procedure BeginTextTrace;
procedure RecordText(X, Y, W, H: Integer; const S: String);
function EndTextTrace: TJSONArray;
procedure RecordPlacement(const Category, Key, S: String; AX, AY, X, Y, W, H: Integer;
  Visible: Boolean; Index: Integer; Selected: Boolean);
procedure RecordPlacementClip(Left,Top,Right,Bottom:Integer);
procedure RecordLeader(AX, AY, X, Y: Integer);
procedure RecordSegment(X1, Y1, X2, Y2: Integer);
procedure SetLayoutReason(const Reason: string);
procedure SetLayoutDebug(TargetX,TargetY,FinalX,FinalY,ShiftX,ShiftY:Integer; HasOld:Boolean);
function EndPlacementTrace: TJSONArray;
function EndLeaderTrace: TJSONArray;
function EndSegmentTrace: TJSONArray;
procedure BeginStageTrace;
procedure MarkStage(const Name: string);
function EndStageTrace: TJSONArray;
function SafeMessageDlg(const Msg: string; DlgType: TMsgDlgType;
  Buttons: TMsgDlgButtons; HelpCtx: Longint): Integer;
implementation
uses SysUtils;
var Entries, Placements, Leaders, Segments, Stages: TJSONArray; StageStart: QWord; LayoutReason:string;
    HasLayoutDebug, LayoutHasOld:Boolean; LayoutTargetX,LayoutTargetY,LayoutFinalX,LayoutFinalY,LayoutShiftX,LayoutShiftY:Integer;
procedure SetLayoutReason(const Reason:string);
begin LayoutReason:=Reason; if Reason='unclassified' then HasLayoutDebug:=False end;
procedure SetLayoutDebug(TargetX,TargetY,FinalX,FinalY,ShiftX,ShiftY:Integer; HasOld:Boolean);
begin
  HasLayoutDebug:=True; LayoutTargetX:=TargetX; LayoutTargetY:=TargetY; LayoutFinalX:=FinalX; LayoutFinalY:=FinalY;
  LayoutShiftX:=ShiftX; LayoutShiftY:=ShiftY; LayoutHasOld:=HasOld;
end;
function SafeMessageDlg(const Msg: string; DlgType: TMsgDlgType;
  Buttons: TMsgDlgButtons; HelpCtx: Longint): Integer;
begin
  raise Exception.Create('Test prevented a modal dialog: '+Msg);
end;
procedure BeginStageTrace;
begin
  FreeAndNil(Stages); Stages:=TJSONArray.Create; StageStart:=GetTickCount64;
end;
procedure MarkStage(const Name: string);
var O:TJSONObject;
begin
  if Stages=nil then Exit;
  O:=TJSONObject.Create; O.Add('stage',Name);
  O.Add('elapsed_ms',Int64(GetTickCount64-StageStart)); Stages.Add(O);
end;
function EndStageTrace:TJSONArray;
begin Result:=Stages; Stages:=nil end;
procedure BeginTextTrace;
begin
  FreeAndNil(Entries);
  FreeAndNil(Placements); FreeAndNil(Leaders); FreeAndNil(Segments);
  Entries := TJSONArray.Create;
  Placements := TJSONArray.Create; Leaders := TJSONArray.Create; Segments:=TJSONArray.Create;
end;
procedure RecordPlacement(const Category, Key, S: String; AX, AY, X, Y, W, H: Integer;
  Visible: Boolean; Index: Integer; Selected: Boolean);
var O,D:TJSONObject;
begin
  if Placements=nil then Exit;
  O:=TJSONObject.Create; O.Add('category',Category); O.Add('key',Key); O.Add('text',S);
  O.Add('anchor_x',AX); O.Add('anchor_y',AY); O.Add('x',X); O.Add('y',Y);
  O.Add('width',W); O.Add('height',H); O.Add('visible',Visible);
  O.Add('point_index',Index); O.Add('selected',Selected);
  if not Visible then begin
    O.Add('omission_reason',LayoutReason);
    if HasLayoutDebug then begin
      D:=TJSONObject.Create; D.Add('target_x',LayoutTargetX); D.Add('target_y',LayoutTargetY);
      D.Add('final_x',LayoutFinalX); D.Add('final_y',LayoutFinalY);
      D.Add('edge_shift_x',LayoutShiftX); D.Add('edge_shift_y',LayoutShiftY); D.Add('has_previous',LayoutHasOld);
      O.Add('layout_debug',D);
    end;
  end;
  Placements.Add(O);
end;
procedure RecordPlacementClip(Left,Top,Right,Bottom:Integer);
var O:TJSONObject;
begin
  if (Placements=nil) or (Placements.Count=0) then Exit;
  O:=TJSONObject.Create; O.Add('left',Left); O.Add('top',Top); O.Add('right',Right); O.Add('bottom',Bottom);
  Placements.Objects[Placements.Count-1].Add('clip_rect',O);
end;
procedure RecordLeader(AX, AY, X, Y: Integer);
var O:TJSONObject;
begin
  if Leaders=nil then Exit;
  O:=TJSONObject.Create; O.Add('anchor_x',AX); O.Add('anchor_y',AY);
  O.Add('end_x',X); O.Add('end_y',Y); Leaders.Add(O);
end;
procedure RecordSegment(X1,Y1,X2,Y2: Integer);
var O:TJSONObject;
begin
  if Segments=nil then Exit;
  O:=TJSONObject.Create; O.Add('x1',X1); O.Add('y1',Y1);
  O.Add('x2',X2); O.Add('y2',Y2); Segments.Add(O);
end;
function EndPlacementTrace:TJSONArray;
begin Result:=Placements; Placements:=nil end;
function EndLeaderTrace:TJSONArray;
begin Result:=Leaders; Leaders:=nil end;
function EndSegmentTrace:TJSONArray;
begin Result:=Segments; Segments:=nil end;
procedure RecordText(X, Y, W, H: Integer; const S: String);
var O: TJSONObject;
begin
  if Entries = nil then Exit;
  O := TJSONObject.Create;
  O.Add('text', S); O.Add('x', X); O.Add('y', Y);
  O.Add('width', W); O.Add('height', H);
  Entries.Add(O);
end;
function EndTextTrace: TJSONArray;
begin
  Result := Entries;
  Entries := nil;
end;
end.
