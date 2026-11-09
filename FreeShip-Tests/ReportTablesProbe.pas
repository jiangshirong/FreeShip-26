program ReportTablesProbe;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Classes,SysUtils,Types,Graphics,Controls,Grids,StdCtrls,FreeStringsUnit,LCLTranslator,FreeShipUnit,FreeGeometry,FreeTypes,FPJSON,UiNativeRender,FreeReportTables,FreeHydrostaticsDlg,FreeHydrostaticsResultsDlg;
var F:TForm;V:THydroReportView;L:TStringList;B:TBitmap;I,J,Tables:Integer;G:TStringGrid;D:TFreeHydrostaticsDialog;M:TFreeHydrostaticsResultsDialog;Overlap:TRect;H:string;Lang:Integer;Ship:TFreeShip;Calc:TFreeHydrostaticCalc;Hints:TJSONObject;HintFile:TStringList;
procedure Check(OK:Boolean;const Name:string);
begin if not OK then begin WriteLn('FAIL ',Name);Halt(1) end;WriteLn('PASS ',Name) end;
begin
 Application.Initialize;Hints:=TJSONObject.Create;Ship:=TFreeShip.Create(nil);Calc:=TFreeHydrostaticCalc.Create(Ship);F:=TForm.CreateNew(nil);F.ClientWidth:=1280;F.ClientHeight:=900;
 V:=THydroReportView.Create(F);V.Parent:=F;V.Align:=alClient;L:=TStringList.Create;
 L.Add('工程 : T2');L.Add('文件名 : Женя.fbm');L.Add('设计船长 : 189.13 米');
 L.Add(rs_NOTE_1__Draft__and_all_other_vertical_heights__is_measured_from_point_of_the_hull_Z_0_);
 L.Add(rs_NOTE_2__All_calculated_coefficients_based_on_project_length___draft_and_beam_);
 L.Add('体积属性:');L.Add('排水体积 : 42958 m^3');L.Add('排水量 : 44032 吨');L.Add('湿表面积 : 7725.9 m^2');
 L.Add('水线面属性:');L.Add('水线面面积 : 4331.7 m^2');L.Add('纵向惯性矩 : 10111632 m^4');
 L.Add('图层属性:');L.Add('+-----+------+-----+');L.Add('| 图层 | 面积 | 重量 |');L.Add('| | m^2 | 吨 |');L.Add('| 图层 0 | 9546.9 | 0.000 |');L.Add('+-----+------+-----+');
 V.SetReport(L);F.HandleNeeded;V.HandleNeeded;Tables:=0;
 for I:=0 to V.ControlCount-1 do if V.Controls[I] is TStringGrid then begin
  G:=TStringGrid(V.Controls[I]);Inc(Tables);
  WriteLn('OBS table_font_points ',G.Font.Size);Check(G.RowCount>0,'nonempty table');
  if G.Tag=0 then begin Check(G.ColCount=3,'embedded layer columns');Check(G.Cells[1,1]='m²','embedded unit superscript');Check(G.Cells[1,2]='9546.9','table numeric value preserved') end;
 end;
 Check(Tables=4,'sectioned native tables');
 Check(Pos(rs_NOTE_1__Draft__and_all_other_vertical_heights__is_measured_from_point_of_the_hull_Z_0_,V.Hint)>0,'datum note retained in hover explanation');
 F.ShowInTaskBar:=stNever;F.Position:=poDesigned;F.Left:=-20000;F.Top:=-20000;F.Show;Application.ProcessMessages;
 F.Realign;V.SetBounds(0,0,F.ClientWidth,F.ClientHeight);V.SetReport(L);
 Check(V.ClientWidth>=1200,'native report viewport has requested width');
 for I:=0 to V.ControlCount-1 do WriteLn('BOUNDS ',V.Controls[I].ClassName,' ',V.Controls[I].Left,' ',V.Controls[I].Top,' ',V.Controls[I].Width,' ',V.Controls[I].Height);
 for I:=0 to V.ControlCount-1 do for J:=I+1 to V.ControlCount-1 do
 Check(not IntersectRect(Overlap,V.Controls[I].BoundsRect,V.Controls[J].BoundsRect),'report sections do not overlap');
 B:=TBitmap.Create;RenderNativeControl(F,B);B.SaveToFile(ParamStr(1));B.Free;F.Hide;
 F.ClientWidth:=900;V.SetReport(L);Check(V.ControlCount>0,'narrow layout rebuild');
 D:=TFreeHydrostaticsDialog.Create(nil);WriteLn('OBS report_client ',D.ClientWidth,' ',D.ClientHeight);Check(D.BorderStyle=bsSizeable,'report can be resized');Check(not D.Edit.Visible,'plain text not the visible report');D.Free;
 M:=TFreeHydrostaticsResultsDialog.Create(nil);WriteLn('OBS multiple_draft_font_points ',M.Grid.Font.Size);
 for Lang:=0 to 1 do begin
  if Lang=0 then SetDefaultLang('cn',ExtractFilePath(ParamStr(0))+'locale','FreeShip',True)
  else SetDefaultLang('ru',ExtractFilePath(ParamStr(0))+'locale','FreeShip',True);
  M.Grid.Cells[12,0]:='Cw';M.Grid.Cells[12,1]:='[-]';
  M.Grid.Cells[6,0]:='LCB';M.Grid.Cells[7,0]:='VCB';
  M.Header.Lines.Clear;
  M.Header.Lines.Add(rs_NOTE_1__Draft__and_all_other_vertical_heights__is_measured_from_point_of_the_hull_Z_0_);
  M.Header.Lines.Add(rs_NOTE_2__All_calculated_coefficients_based_on_actual_dimensions_of_submerged_body_);
  M.Header.Lines.Add('Cw : '+rs_Waterplane_coefficient);
  M.Header.Lines.Add('LCB : '+rs_Longitudinal_center_of_buoyancy+', '+rs_measured_from_the_aft_perpendicular_at_X_0_0);
  M.Header.Lines.Add('VCB : '+rs_Vertical_center_of_buoyancy+', '+rs_measured_from_the_keel_line_of_the_hull);
  M.PrepareHeaderView;H:='';M.Grid.OnGetCellHint(M.Grid,12,0,H);
  if Lang=0 then Hints.Add('cn',H) else Hints.Add('ru',H);
  WriteLn('OBS Cw hover ',H);
  Check(Pos(rs_Waterplane_coefficient,H)>0,'localized header callback meaning');
  if Lang=0 then Check(Pos('水线面系数',H)>0,'Chinese Cw meaning');
  Check(Pos(rs_NOTE_2__All_calculated_coefficients_based_on_actual_dimensions_of_submerged_body_,M.HeaderMeaning(12,1))>0,'actual coefficient basis retained on unit row');
  Check(Pos(rs_NOTE_2__All_calculated_coefficients_based_on_actual_dimensions_of_submerged_body_,H)=0,'symbol hover stays concise');
  Check(Pos(rs_measured_from_the_aft_perpendicular_at_X_0_0,M.HeaderMeaning(6,0))>0,'LCB origin retained');
  Check(Pos(rs_measured_from_the_keel_line_of_the_hull,M.HeaderMeaning(7,0))>0,'VCB origin retained');
  Check(M.HeaderMeaning(12,2)='','data rows do not show header definitions');
  for I:=0 to M.ComponentCount-1 do if M.Components[I] is THydroReportView then
   Check(THydroReportView(M.Components[I]).ControlCount=0,'glossary and notes removed from visible header');
  Check(M.Header.Lines.Count=5,'export source keeps explanations');
 end;
 // Use the actual calculation footer to check every glossary entry, not just Cw.
 M.Grid.ColCount:=18;
 M.Grid.Cells[2,0]:='Lwl';M.Grid.Cells[3,0]:='Bwl';M.Grid.Cells[4,0]:='排水体积';M.Grid.Cells[5,0]:='排水量';
 M.Grid.Cells[8,0]:='Cb';M.Grid.Cells[9,0]:='Am';M.Grid.Cells[10,0]:='Cm';M.Grid.Cells[11,0]:='Aw';
 M.Grid.Cells[13,0]:='LCF';M.Grid.Cells[14,0]:='Cp';M.Grid.Cells[15,0]:='S';M.Grid.Cells[16,0]:='KMt';M.Grid.Cells[17,0]:='KMl';
 L.Clear;Calc.AddFooter(L,fhMultipleCalculations);M.Header.Lines.Assign(L);M.PrepareHeaderView;
 for I:=2 to 17 do Check(M.HeaderMeaning(I,0)<>M.Grid.Cells[I,0],'calculation footer meaning '+IntToStr(I));
 for I:=0 to M.ComponentCount-1 do if M.Components[I] is THydroReportView then
  Check(THydroReportView(M.Components[I]).ControlCount=0,'all actual footer glossary entries hidden');
 HintFile:=TStringList.Create;
 try HintFile.Text:=Hints.FormatJSON;HintFile.SaveToFile(ChangeFileExt(ParamStr(1),'.hints.json')) finally HintFile.Free end;
 Hints.Free;Calc.Free;Ship.Free;M.Free;
 L.Free;F.Free;
end.

