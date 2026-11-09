unit UiInventory;
{$mode objfpc}{$H+}
interface
uses Main, fpjson;
procedure CaptureInventory(Form:TMainForm; Root:TJSONObject; const State:string);
implementation
uses Classes, SysUtils, Controls, Forms, StdCtrls, Menus, ActnList, Buttons, ComCtrls, TypInfo, LCLProc, Graphics, Types, Math;

function EffectiveVisible(C:TControl):Boolean;
begin
  Result:=True;
  while Assigned(C) do begin
    if not C.Visible then Exit(False);
    C:=C.Parent;
  end;
end;

function ShortcutName(Value:TShortCut):string;
begin
  if Value=0 then Exit('');
  Result:=ShortCutToTextRaw(Value);
end;

procedure CaptureInventory(Form:TMainForm; Root:TJSONObject; const State:string);
var Snapshot:TJSONObject; Nodes:TJSONArray; Seen:TFPList;
  function RelativeOrigin(C:TControl):TPoint;
  begin Result:=Point(0,0); while Assigned(C) and Assigned(C.Parent) do begin Inc(Result.X,C.Left); Inc(Result.Y,C.Top); C:=C.Parent end end;
  function LogicalVisible(C:TControl):Boolean;
  begin Result:=True; while Assigned(C) and Assigned(C.Parent) do begin if not C.Visible then Exit(False); C:=C.Parent end end;
  procedure CaptureText(C:TControl; const Path:string);
  var Prop:PPropInfo; S,Field:string; O,Geom,ClipObj,FontObj:TJSONObject; B:TBitmap;
      L:TStringList; I,W,H,MaxW:Integer; P,AP:TPoint; Clip:TRect; Parent:TWinControl; F:TObject; Opts:TJSONArray;
  begin
    Field:='caption'; Prop:=GetPropInfo(C,'Caption'); S:='';
    if Assigned(Prop) then S:=GetStrProp(C,Prop);
    if (S='') and (C is TCustomEdit) then begin Field:='text'; Prop:=GetPropInfo(C,'Text'); if Assigned(Prop) then S:=GetStrProp(C,Prop) end;
    if (C is TComboBox) then begin Field:='text'; S:=TComboBox(C).Text end;
    if (S='') and not (C is TComboBox) then Exit;
    O:=TJSONObject.Create; O.Add('kind','text_control'); O.Add('class',C.ClassName); O.Add('name',C.Name);
    O.Add('component_path',Path); O.Add('text_field',Field); O.Add('text',S);
    O.Add('visible_local',C.Visible); O.Add('visible_excluding_hidden_form',LogicalVisible(C)); O.Add('enabled',C.Enabled);
    O.Add('hint',C.Hint); O.Add('show_hint',C.ShowHint);
    P:=RelativeOrigin(C); Clip:=Rect(P.X,P.Y,P.X+C.Width,P.Y+C.Height); Parent:=C.Parent;
    while Assigned(Parent) do begin
      AP:=RelativeOrigin(Parent); Clip.Left:=Max(Clip.Left,AP.X); Clip.Top:=Max(Clip.Top,AP.Y);
      Clip.Right:=Min(Clip.Right,AP.X+Parent.ClientWidth); Clip.Bottom:=Min(Clip.Bottom,AP.Y+Parent.ClientHeight); Parent:=Parent.Parent;
    end;
    Clip.Right:=Max(Clip.Left,Clip.Right); Clip.Bottom:=Max(Clip.Top,Clip.Bottom);
    Geom:=TJSONObject.Create; Geom.Add('x',P.X); Geom.Add('y',P.Y); Geom.Add('width',C.Width); Geom.Add('height',C.Height); O.Add('bounds',Geom);
    ClipObj:=TJSONObject.Create; ClipObj.Add('left',Clip.Left); ClipObj.Add('top',Clip.Top); ClipObj.Add('right',Clip.Right); ClipObj.Add('bottom',Clip.Bottom); O.Add('ancestor_clip',ClipObj);
    Prop:=GetPropInfo(C,'WordWrap'); if Assigned(Prop) then O.Add('word_wrap',GetOrdProp(C,Prop)<>0) else O.Add('word_wrap_known',False);
    B:=TBitmap.Create; L:=TStringList.Create;
    try
      Prop:=GetPropInfo(C,'Font'); F:=nil; if Assigned(Prop) then F:=GetObjectProp(C,Prop);
      if F is TFont then begin
        B.Canvas.Font.Assign(TFont(F)); FontObj:=TJSONObject.Create; FontObj.Add('name',TFont(F).Name); FontObj.Add('size',TFont(F).Size); FontObj.Add('height',TFont(F).Height); O.Add('font',FontObj);
        L.Text:=S; MaxW:=0; H:=0;
        for I:=0 to L.Count-1 do begin W:=B.Canvas.TextWidth(L[I]); MaxW:=Max(MaxW,W); Inc(H,B.Canvas.TextHeight('Ag')) end;
        O.Add('measured_width',MaxW); O.Add('measured_height',H); O.Add('line_count',L.Count);
        O.Add('measurement','LCL bitmap canvas using declared control font; native padding, icons, wrapping and font fallback are not measured');
        if C is TComboBox then begin
          Opts:=TJSONArray.Create;
          for I:=0 to TComboBox(C).Items.Count-1 do begin
            Geom:=TJSONObject.Create; Geom.Add('index',I); Geom.Add('text',TComboBox(C).Items[I]);
            Geom.Add('measured_width',B.Canvas.TextWidth(TComboBox(C).Items[I])); Opts.Add(Geom);
          end; O.Add('option_measurements',Opts); O.Add('dropdown_width_known',False);
        end;
      end else O.Add('measurement_unavailable','control has no accessible TFont property');
    finally L.Free; B.Free end;
    Nodes.Add(O);
  end;
  procedure WalkMenu(Item:TMenuItem; const Path,OwnerName,ContainerId,IdPath:string; ParentVisible:Boolean);
  var I:Integer; O:TJSONObject; S:string; V:Boolean;
  begin
    if Item=nil then Exit;
    V:=ParentVisible and Item.Visible;
    S:=Path; if Item.Caption<>'' then begin if S<>'' then S:=S+' / '; S:=S+Item.Caption end;
    if Item.Parent<>nil then begin
      O:=TJSONObject.Create; O.Add('kind','menu_item'); O.Add('name',Item.Name);
      O.Add('class',Item.ClassName); O.Add('owner',OwnerName); O.Add('path',S);
      O.Add('container_id',ContainerId); O.Add('item_index_path',IdPath);
      O.Add('caption',Item.Caption); O.Add('visible_local',Item.Visible);
      O.Add('visible_effective',V); O.Add('enabled',Item.Enabled); O.Add('checked',Item.Checked);
      O.Add('shortcut_code',Item.ShortCut); O.Add('shortcut',ShortcutName(Item.ShortCut));
      if Assigned(Item.Action) then O.Add('action',Item.Action.Name);
      O.Add('has_click_handler',Assigned(Item.OnClick)); O.Add('children_count',Item.Count); Nodes.Add(O);
    end;
    for I:=0 to Item.Count-1 do WalkMenu(Item.Items[I],S,OwnerName,ContainerId,IdPath+'/'+IntToStr(I),V);
  end;
  procedure Walk(Component:TComponent; const ComponentPath:string);
  var I:Integer; O:TJSONObject; C:TControl; A:TAction; M:TMenu; CB:TComboBox; Items:TJSONArray; Prop:PPropInfo; Child:TComponent; ChildId:string;
  begin
    if (Component=nil) or (Seen.IndexOf(Component)>=0) then Exit;
    Seen.Add(Component);
    if Component is TMenu then begin
      M:=TMenu(Component); O:=TJSONObject.Create; O.Add('kind','menu'); O.Add('name',M.Name);
      O.Add('class',M.ClassName); O.Add('component_path',ComponentPath);
      if Assigned(Component.Owner) then O.Add('owner',Component.Owner.Name);
      if M is TPopupMenu then O.Add('dynamic_popup_handler',Assigned(TPopupMenu(M).OnPopup));
      Nodes.Add(O); WalkMenu(M.Items,'',M.Name,ComponentPath,'',True);
    end;
    if Component is TAction then begin
      A:=TAction(Component); O:=TJSONObject.Create; O.Add('kind','action'); O.Add('name',A.Name);
      O.Add('class',A.ClassName); O.Add('component_path',ComponentPath); O.Add('caption',A.Caption); O.Add('hint',A.Hint);
      O.Add('category',A.Category); O.Add('enabled',A.Enabled); O.Add('visible',A.Visible);
      O.Add('checked',A.Checked); O.Add('shortcut_code',A.ShortCut); O.Add('shortcut',ShortcutName(A.ShortCut));
      O.Add('has_execute_handler',Assigned(A.OnExecute)); Nodes.Add(O);
    end;
    if Component is TControl then begin
      C:=TControl(Component);
      CaptureText(C,ComponentPath);
      if (C is TSpeedButton) or (C is TToolButton) or (C is TComboBox) then begin
        O:=TJSONObject.Create;
        if C is TComboBox then O.Add('kind','combo_box') else O.Add('kind','button');
        O.Add('name',C.Name); O.Add('class',C.ClassName); O.Add('component_path',ComponentPath);
        if Assigned(C.Parent) then O.Add('parent',C.Parent.Name);
        O.Add('visible_local',C.Visible); O.Add('visible_effective',EffectiveVisible(C));
        O.Add('enabled',C.Enabled); O.Add('hint',C.Hint);
        if Assigned(C.Action) then O.Add('action',C.Action.Name);
        Prop:=GetPropInfo(C,'Caption'); if Assigned(Prop) then O.Add('caption',GetStrProp(C,Prop));
        if C is TSpeedButton then O.Add('down',TSpeedButton(C).Down);
        if C is TComboBox then begin
          CB:=TComboBox(C); O.Add('text',CB.Text); O.Add('selected_index',CB.ItemIndex);
          Items:=TJSONArray.Create; for I:=0 to CB.Items.Count-1 do Items.Add(CB.Items[I]); O.Add('items',Items);
          O.Add('dynamic_dropdown_handler',Assigned(CB.OnDropDown));
        end;
        Nodes.Add(O);
      end;
    end;
    for I:=0 to Component.ComponentCount-1 do begin
      Child:=Component.Components[I]; ChildId:=Child.Name;
      if ChildId='' then ChildId:=Child.ClassName+'#'+IntToStr(I);
      Walk(Child,ComponentPath+'/'+ChildId);
    end;
  end;
begin
  if Root.Find('inventory')=nil then Root.Add('inventory',TJSONArray.Create);
  Snapshot:=TJSONObject.Create; Snapshot.Add('state',State);
  Snapshot.Add('language',Form.FreeShip.Preferences.Language);
  Snapshot.Add('model_loaded',Form.FreeShip.ModelIsLoaded);
  Snapshot.Add('selected_points',Form.FreeShip.NumberOfSelectedControlPoints);
  Nodes:=TJSONArray.Create; Snapshot.Add('nodes',Nodes);
  Snapshot.Add('limitations','static component snapshot; popup/dropdown handlers and real focus not invoked');
  Seen:=TFPList.Create;
  try Walk(Form,Form.Name) finally Seen.Free end;
  Root.Arrays['inventory'].Add(Snapshot);
end;
end.
