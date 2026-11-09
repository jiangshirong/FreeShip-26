unit UiLayers;
{$mode objfpc}{$H+}
interface
uses Main, fpjson;
procedure RunLayers(Form:TMainForm; Cases:TJSONArray; Root:TJSONObject);
implementation
uses SysUtils, Classes, StdCtrls, Grids, FreeGeometry, FreeLayerVisibilityDlg, UiDiscovery;

procedure CaseResult(Cases:TJSONArray; const Id,Status,Expected,Actual:string);
var O:TJSONObject;
begin O:=TJSONObject.Create; O.Add('id',Id); O.Add('status',Status); O.Add('expected',Expected); O.Add('actual',Actual); Cases.Add(O) end;
procedure Check(Cases:TJSONArray; const Id:string; Ok:Boolean; const Expected,Actual:string);
begin if Ok then CaseResult(Cases,Id,'pass',Expected,Actual) else CaseResult(Cases,Id,'fail',Expected,Actual) end;
procedure Missing(Cases:TJSONArray; const Id,Why:string);
begin CaseResult(Cases,Id,'not_run','discover and exercise a unique safe layer control',Why) end;
procedure MissingAll(Cases:TJSONArray; const Why:string);
const Ids:array[0..16] of string=('rows','hide-surface','refill-state','restore-surface','hide-control-net','restore-control-net','other-surface-unchanged','other-net-unchanged','active-selection','bulk.hide-surfaces','bulk.surface-keeps-nets','bulk.show-surfaces','bulk.hide-nets','bulk.net-keeps-surfaces','bulk.show-nets','sorted-row-target','actual-layout');
var I:Integer;
begin for I:=0 to High(Ids) do Missing(Cases,'layers.'+Ids[I],Why) end;

procedure RunLayers(Form:TMainForm; Cases:TJSONArray; Root:TJSONObject);
var Dlg:TFreeLayerVisibilityDialog; A,B:TFreeSubdivisionLayer; Grid:TStringGrid;
    Items:TFPList; GridEvent:TToggledCheckboxEvent; Event:TNotifyEvent; I,J,RowA,RowB,ObjCol,SurfaceCol,NetCol,Idx,GridCol:Integer;
    Info,Entry:TJSONObject; Columns:TJSONArray; SurfaceBefore,NetBefore:Boolean;
    Bulk1,Bulk2:TCheckBox; Selector:TComboBox; SurfaceColumn:TGridColumn;
  function UniqueCheck(Handler:TNotifyEvent):TCheckBox;
  var K,N:Integer; Found:TCheckBox;
  begin
    Items.Clear; FindComponentsByEvent(Dlg,'OnClick',TMethod(Handler),Items); N:=0; Found:=nil;
    for K:=0 to Items.Count-1 do if TObject(Items[K]) is TCheckBox then begin Inc(N); Found:=TCheckBox(Items[K]) end;
    if N=1 then Result:=Found else Result:=nil;
  end;
  procedure LocateRows;
  var C,R,Hits,BCol:Integer;
  begin
    RowA:=-1; RowB:=-1; ObjCol:=-1; BCol:=-1; Hits:=0;
    for C:=0 to Grid.ColCount-1 do for R:=Grid.FixedRows to Grid.RowCount-1 do begin
      if Grid.Objects[C,R]=A then begin RowA:=R; ObjCol:=C; Inc(Hits) end;
      if Grid.Objects[C,R]=B then begin RowB:=R; BCol:=C; Inc(Hits) end;
    end;
    if (Hits<>2) or (BCol<>ObjCol) then begin RowA:=-1; RowB:=-1; ObjCol:=-1 end;
  end;
  procedure Toggle(Col,Row:Integer; State:TCheckboxState);
  begin Grid.OnCheckboxToggled(Grid,Col,Row,State) end;
  procedure Bulk(C:TCheckBox; State:Boolean);
  begin C.Checked:=State; if Assigned(C.OnClick) then C.OnClick(C) end;
begin
  if GetEnvironmentVariable('FS_UI_LAYER_DIALOG')<>'1' then begin MissingAll(Cases,'layer dialog source fingerprint not approved'); Exit end;
  A:=Form.FreeShip.Layer[0]; B:=Form.FreeShip.Surface.AddNewLayer; B.Name:='UIProbeLayer';
  A.SurfaceVisible:=True; A.ControlNetVisible:=True; B.SurfaceVisible:=True; B.ControlNetVisible:=True;
  Dlg:=TFreeLayerVisibilityDialog.Create(Form); Items:=TFPList.Create;
  try
    Dlg.FreeShip:=Form.FreeShip; Dlg.FillLayers;
    GridEvent:=@Dlg.sgLayersCheckboxToggled;
    FindComponentsByEvent(Dlg,'OnCheckboxToggled',TMethod(GridEvent),Items); Grid:=nil;
    for I:=0 to Items.Count-1 do if TObject(Items[I]) is TStringGrid then begin if Grid=nil then Grid:=TStringGrid(Items[I]) else begin Grid:=nil; Break end end;
    if Grid=nil then begin MissingAll(Cases,'grid callback absent or ambiguous'); Exit end;
    LocateRows;
    if (RowA<0) or (RowB<0) then begin MissingAll(Cases,'layer object rows absent or ambiguous'); Exit end;
    Check(Cases,'layers.rows',True,'two uniquely identified model layer objects in grid',Format('rows=%d,%d; object column=%d',[RowA,RowB,ObjCol]));
    SurfaceCol:=-1; NetCol:=-1; SurfaceColumn:=nil; Columns:=TJSONArray.Create;
    for I:=0 to Grid.Columns.Count-1 do if (Grid.Columns[I].ButtonStyle=cbsCheckboxColumn) and (Grid.Columns.VisibleIndex(I)>=0) then begin
      GridCol:=Grid.FixedCols+Grid.Columns.VisibleIndex(I);
      A.SurfaceVisible:=True; A.ControlNetVisible:=True; B.SurfaceVisible:=True; B.ControlNetVisible:=True;
      Toggle(GridCol,RowA,cbUnchecked);
      Entry:=TJSONObject.Create; Entry.Add('index',GridCol); Entry.Add('title',Grid.Columns[I].Title.Caption);
      Entry.Add('surface_after',A.SurfaceVisible); Entry.Add('net_after',A.ControlNetVisible);
      Entry.Add('other_surface_after',B.SurfaceVisible); Entry.Add('other_net_after',B.ControlNetVisible); Columns.Add(Entry);
      if not A.SurfaceVisible and A.ControlNetVisible then begin if SurfaceCol=-1 then begin SurfaceCol:=GridCol; SurfaceColumn:=Grid.Columns[I] end else SurfaceCol:=-2 end;
      if A.SurfaceVisible and not A.ControlNetVisible then begin if NetCol=-1 then NetCol:=GridCol else NetCol:=-2 end;
      Toggle(GridCol,RowA,cbChecked);
    end;
    Info:=TJSONObject.Create; Info.Add('grid_name',Grid.Name); Info.Add('object_column',ObjCol);
    Info.Add('row_a',RowA); Info.Add('row_b',RowB); Info.Add('column_effects',Columns); Root.Add('layer_discovery',Info);
    if SurfaceCol>=0 then begin
      A.SurfaceVisible:=True; B.SurfaceVisible:=True; Toggle(SurfaceCol,RowA,cbUnchecked);
      Check(Cases,'layers.hide-surface',not A.SurfaceVisible,'discovered surface control hides target',SysUtils.BoolToStr(A.SurfaceVisible,True));
      Check(Cases,'layers.other-surface-unchanged',B.SurfaceVisible,'other layer unchanged',SysUtils.BoolToStr(B.SurfaceVisible,True));
      Dlg.FillLayers; LocateRows;
      Check(Cases,'layers.refill-state',Grid.Cells[SurfaceCol,RowA]=SurfaceColumn.ValueUnchecked,
        'refilled checkbox reflects its configured unchecked value',Grid.Cells[SurfaceCol,RowA]);
      Toggle(SurfaceCol,RowA,cbChecked); Check(Cases,'layers.restore-surface',A.SurfaceVisible,'surface can be restored',SysUtils.BoolToStr(A.SurfaceVisible,True));
    end else begin Missing(Cases,'layers.hide-surface','no unique observed surface effect'); Missing(Cases,'layers.other-surface-unchanged','no unique surface control'); Missing(Cases,'layers.refill-state','no unique surface control'); Missing(Cases,'layers.restore-surface','no unique surface control') end;
    if NetCol>=0 then begin
      A.ControlNetVisible:=True; B.ControlNetVisible:=True; Toggle(NetCol,RowA,cbUnchecked);
      Check(Cases,'layers.hide-control-net',not A.ControlNetVisible,'discovered net control hides target',SysUtils.BoolToStr(A.ControlNetVisible,True));
      Check(Cases,'layers.other-net-unchanged',B.ControlNetVisible,'other layer net unchanged',SysUtils.BoolToStr(B.ControlNetVisible,True));
      Toggle(NetCol,RowA,cbChecked); Check(Cases,'layers.restore-control-net',A.ControlNetVisible,'net can be restored',SysUtils.BoolToStr(A.ControlNetVisible,True));
    end else begin Missing(Cases,'layers.hide-control-net','no unique observed net effect'); Missing(Cases,'layers.other-net-unchanged','no unique net control'); Missing(Cases,'layers.restore-control-net','no unique net control') end;
    Bulk1:=UniqueCheck(@Dlg.CheckBox1Click); Bulk2:=UniqueCheck(@Dlg.CheckBox2Click);
    A.SurfaceVisible:=True; B.SurfaceVisible:=True; A.ControlNetVisible:=True; B.ControlNetVisible:=True; Dlg.FillLayers;
    if Assigned(Bulk1) then begin
      Bulk(Bulk1,False); Check(Cases,'layers.bulk.hide-surfaces',not A.SurfaceVisible and not B.SurfaceVisible,'bulk surface control hides both layers',SysUtils.BoolToStr(A.SurfaceVisible or B.SurfaceVisible,True));
      Check(Cases,'layers.bulk.surface-keeps-nets',A.ControlNetVisible and B.ControlNetVisible,'bulk surface control preserves nets','observed both layers');
      Bulk(Bulk1,True); Check(Cases,'layers.bulk.show-surfaces',A.SurfaceVisible and B.SurfaceVisible,'bulk surface control restores both','observed both layers');
    end else begin Missing(Cases,'layers.bulk.hide-surfaces','no unique callback-bound checkbox'); Missing(Cases,'layers.bulk.surface-keeps-nets','no unique checkbox'); Missing(Cases,'layers.bulk.show-surfaces','no unique checkbox') end;
    if Assigned(Bulk2) then begin
      Bulk(Bulk2,False); Check(Cases,'layers.bulk.hide-nets',not A.ControlNetVisible and not B.ControlNetVisible,'bulk net control hides both nets','observed both layers');
      Check(Cases,'layers.bulk.net-keeps-surfaces',A.SurfaceVisible and B.SurfaceVisible,'bulk net control preserves surfaces','observed both layers');
      Bulk(Bulk2,True); Check(Cases,'layers.bulk.show-nets',A.ControlNetVisible and B.ControlNetVisible,'bulk net control restores both','observed both layers');
    end else begin Missing(Cases,'layers.bulk.hide-nets','no unique callback-bound checkbox'); Missing(Cases,'layers.bulk.net-keeps-surfaces','no unique checkbox'); Missing(Cases,'layers.bulk.show-nets','no unique checkbox') end;
    if (SurfaceCol>=0) and (ObjCol>=0) then begin
      Dlg.FillLayers; Grid.SortColRow(True,ObjCol); LocateRows;
      if (RowA>=0) and (RowB>=0) then begin A.SurfaceVisible:=True; B.SurfaceVisible:=True; Toggle(SurfaceCol,RowB,cbUnchecked);
        Check(Cases,'layers.sorted-row-target',A.SurfaceVisible and not B.SurfaceVisible,'sorted row changes referenced layer only',Format('first=%s; second=%s',[SysUtils.BoolToStr(A.SurfaceVisible,True),SysUtils.BoolToStr(B.SurfaceVisible,True)])); Toggle(SurfaceCol,RowB,cbChecked)
      end else Missing(Cases,'layers.sorted-row-target','layer row identity ambiguous after sort');
    end else Missing(Cases,'layers.sorted-row-target','surface or object column ambiguous');
    Items.Clear; Event:=@Form.LayerBoxChange; FindComponentsByEvent(Form,'OnChange',TMethod(Event),Items); Selector:=nil; Idx:=0;
    for I:=0 to Items.Count-1 do if TObject(Items[I]) is TComboBox then begin Inc(Idx); Selector:=TComboBox(Items[I]) end;
    if (Idx=1) and Assigned(Selector) then begin
      J:=Selector.Items.IndexOfObject(B);
      if J>=0 then begin Selector.ItemIndex:=J; Selector.OnChange(Selector); Check(Cases,'layers.active-selection',Form.FreeShip.ActiveLayer=B,'discovered layer selector activates selected layer',SysUtils.BoolToStr(Form.FreeShip.ActiveLayer=B,True)) end
      else Missing(Cases,'layers.active-selection','fixture layer absent from selector');
    end else Missing(Cases,'layers.active-selection','selector callback absent or ambiguous');
    Missing(Cases,'layers.actual-layout','native display and desktop input not exercised');
  finally Items.Free; Dlg.Free end;
end;
end.

