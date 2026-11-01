unit FreeReportTables;
{$mode objfpc}{$H+}
interface
uses Classes,SysUtils,Types,Forms,Controls,StdCtrls,Grids,Graphics;
type
 THydroReportView=class(TScrollBox)
 private
  FBuilding:Boolean;
  FLayoutBusy:Boolean;
  FMeasure:TBitmap;
  procedure LayoutReport;
  procedure DrawCell(Sender:TObject;Col,Row:Integer;R:TRect;State:TGridDrawState);
  procedure GridKey(Sender:TObject;var Key:Word;Shift:TShiftState);
  function NewGrid(Columns:Integer;Properties:Boolean):TStringGrid;
  procedure AddText(const AText:string;Heading:Boolean);
 protected
  procedure Resize;override;
 public
  constructor Create(AOwner:TComponent);override;
  destructor Destroy;override;
  procedure SetReport(Lines:TStrings);
 end;
procedure EnlargeHydroReport(Form:TForm);
implementation
uses Math,FreeStringsUnit,FreeReportPresentation;
procedure EnlargeHydroReport(Form:TForm);
begin
 Form.BorderStyle:=bsSizeable;Form.DoubleBuffered:=True;
 Form.ClientWidth:=Min(1280,Screen.WorkAreaWidth-64);
 Form.ClientHeight:=Min(900,Screen.WorkAreaHeight-64);
 Form.Constraints.MinWidth:=Min(840,Form.Width);
 Form.Constraints.MinHeight:=Min(560,Form.Height);
end;
constructor THydroReportView.Create(AOwner:TComponent);
begin
 inherited Create(AOwner);FMeasure:=TBitmap.Create;FMeasure.SetSize(1,1);BorderStyle:=bsNone;Color:=RGBToColor(245,248,245);
 ParentColor:=False;Font.Name:='Microsoft YaHei UI';Font.Size:=14;
 Font.Color:=RGBToColor(24,53,54);Font.Quality:=fqCleartypeNatural;
 HorzScrollBar.Visible:=False;VertScrollBar.Tracking:=True;DoubleBuffered:=True;
end;
destructor THydroReportView.Destroy;
begin FreeAndNil(FMeasure);inherited Destroy end;
function THydroReportView.NewGrid(Columns:Integer;Properties:Boolean):TStringGrid;
begin
 Result:=TStringGrid.Create(Self);Result.Parent:=Self;Result.ColCount:=Columns;
 Result.FixedCols:=0;Result.FixedRows:=0;Result.RowCount:=1;
 Result.Tag:=Ord(Properties);Result.BorderStyle:=bsNone;
 Result.Font.Assign(Font);Result.DefaultDrawing:=False;
 Result.Color:=clWhite;Result.GridLineColor:=RGBToColor(220,230,224);
 Result.Options:=[goHorzLine,goVertLine,goRangeSelect,goColSizing];
 Result.OnDrawCell:=@DrawCell;Result.OnKeyDown:=@GridKey;
 Result.DoubleBuffered:=True;Result.ScrollBars:=ssHorizontal;
end;
procedure THydroReportView.AddText(const AText:string;Heading:Boolean);
var L:TStaticText;
begin
 L:=TStaticText.Create(Self);L.Parent:=Self;L.Caption:=AText;L.AutoSize:=False;
 L.Transparent:=False;L.Color:=Color;L.ShowAccelChar:=False;L.Font.Assign(Font);
 if Heading and (Hint<>'') then begin L.Hint:=Hint;L.ShowHint:=True end;
 if Heading then begin L.Tag:=1;L.Font.Size:=16;L.Font.Style:=[fsBold] end;
end;
procedure THydroReportView.GridKey(Sender:TObject;var Key:Word;Shift:TShiftState);
var G:TStringGrid;
begin
 G:=TStringGrid(Sender);
 if ssCtrl in Shift then begin
  if Key=Ord('C') then begin G.CopyToClipboard;Key:=0 end;
  if Key=Ord('A') then begin G.Selection:=Rect(0,0,G.ColCount-1,G.RowCount-1);Key:=0 end;
 end;
end;
procedure THydroReportView.DrawCell(Sender:TObject;Col,Row:Integer;R:TRect;State:TGridDrawState);
var G:TStringGrid;Style:TTextStyle;
begin
 G:=TStringGrid(Sender);G.Canvas.Font.Assign(G.Font);
 G.Canvas.Brush.Color:=clWhite;
 if (Row mod 2)=1 then G.Canvas.Brush.Color:=RGBToColor(246,250,247);
 if (G.Tag=0) and (Row<2) then begin
  G.Canvas.Brush.Color:=RGBToColor(229,240,234);G.Canvas.Font.Style:=[fsBold];
 end;
 if gdSelected in State then G.Canvas.Brush.Color:=RGBToColor(211,234,228);
 G.Canvas.FillRect(R);Style:=G.Canvas.TextStyle;
 Style.SingleLine:=False;Style.Wordbreak:=True;Style.Layout:=tlCenter;
 Style.Clipping:=True;Style.Alignment:=taLeftJustify;
 InflateRect(R,-12,-5);G.Canvas.TextRect(R,R.Left,R.Top,G.Cells[Col,Row],Style);
end;
procedure THydroReportView.LayoutReport;
var I,J,K,Y,W,H,Total,CW,RH,TH,Extra:Integer;G:TStringGrid;L:TStaticText;
begin
 if FBuilding or FLayoutBusy or (FMeasure=nil) then Exit;
 FLayoutBusy:=True;
 try
 Y:=24-VertScrollBar.Position;W:=Max(300,ClientWidth-48);
 for I:=0 to ControlCount-1 do begin
  if Controls[I] is TStringGrid then begin
   G:=TStringGrid(Controls[I]);G.Canvas.Font.Assign(G.Font);
   TH:=G.Canvas.TextHeight('Hg');Total:=0;
   for J:=0 to G.ColCount-1 do begin
    if G.Tag=1 then begin if J=0 then CW:=Round((W-2)*0.60) else CW:=W-2-Round((W-2)*0.60) end
    else begin
     CW:=96;
     for K:=0 to G.RowCount-1 do CW:=Max(CW,G.Canvas.TextWidth(G.Cells[J,K])+32);
     CW:=Min(CW,360);
    end;
    G.ColWidths[J]:=CW;Inc(Total,CW+1);
   end;
   if (G.Tag=0) and (Total<W) then begin
    Extra:=W-Total;
    for J:=0 to G.ColCount-1 do G.ColWidths[J]:=G.ColWidths[J]+Extra div G.ColCount+Ord(J<(Extra mod G.ColCount));
    Total:=W;
   end;
   if Total<=W then G.ScrollBars:=ssNone else G.ScrollBars:=ssHorizontal;
   H:=2;
   for J:=0 to G.RowCount-1 do begin
    RH:=TH+20;
    for K:=0 to G.ColCount-1 do RH:=Max(RH,
      Ceil(G.Canvas.TextWidth(G.Cells[K,J])/Max(1,G.ColWidths[K]-24))*(TH+2)+16);
    G.RowHeights[J]:=RH;Inc(H,RH+1);
   end;
   if Total>W then Inc(H,24);
   G.SetBounds(24,Y,W,H);Inc(Y,H+24);
  end else if Controls[I] is TStaticText then begin
   L:=TStaticText(Controls[I]);FMeasure.Canvas.Font.Assign(L.Font);
   H:=(Ceil(FMeasure.Canvas.TextWidth(L.Caption)/Max(1,W))+1)*(FMeasure.Canvas.TextHeight('Hg')+3);
   if L.Tag=1 then H:=FMeasure.Canvas.TextHeight('Hg')+16;
   L.SetBounds(24,Y,W,H);Inc(Y,H+8);
  end;
 end;
 finally FLayoutBusy:=False end;
end;
procedure THydroReportView.Resize;
begin inherited Resize;LayoutReport end;
procedure THydroReportView.SetReport(Lines:TStrings);
var I,J,P,R:Integer;S,Key,Value:string;G:TStringGrid;Cells:TStringList;
begin
 FBuilding:=True;DisableAlign;
 try
  while ControlCount>0 do Controls[0].Free;
  VertScrollBar.Position:=0;G:=nil;Cells:=TStringList.Create;Hint:='';
  for I:=0 to Lines.Count-1 do begin
   S:=Trim(Lines[I]);
   if (S=rs_NOTE_1__Draft__and_all_other_vertical_heights__is_measured_from_point_of_the_hull_Z_0_) or
      (S=rs_NOTE_2__All_calculated_coefficients_based_on_actual_dimensions_of_submerged_body_) or
      (S=rs_NOTE_2__All_calculated_coefficients_based_on_project_length___draft_and_beam_) then begin
    if Hint<>'' then Hint:=Hint+LineEnding;Hint:=Hint+S;
   end;
  end;
  try
   for I:=0 to Lines.Count-1 do begin
    S:=Trim(ReportUnitPowers(Lines[I]));if S='' then begin G:=nil;Continue end;
    if (S=rs_NOTE_1__Draft__and_all_other_vertical_heights__is_measured_from_point_of_the_hull_Z_0_) or
       (S=rs_NOTE_2__All_calculated_coefficients_based_on_actual_dimensions_of_submerged_body_) or
       (S=rs_NOTE_2__All_calculated_coefficients_based_on_project_length___draft_and_beam_) then Continue;
    if S[1]='+' then Continue;
    if S[1]='|' then begin
     Cells.Clear;Cells.StrictDelimiter:=True;Cells.Delimiter:='|';Cells.QuoteChar:=#0;
     Cells.DelimitedText:=S;
     if Cells.Count>0 then Cells.Delete(0);
     if (Cells.Count>0) and (Trim(Cells[Cells.Count-1])='') then Cells.Delete(Cells.Count-1);
     if Cells.Count=0 then Continue;
     if (G=nil) or (G.Tag<>0) or (G.ColCount<>Cells.Count) then begin
      G:=NewGrid(Cells.Count,False);R:=0;
     end else begin R:=G.RowCount;G.RowCount:=R+1 end;
     for J:=0 to Cells.Count-1 do G.Cells[J,R]:=Trim(Cells[J]);
     Continue;
    end;
    P:=Pos(#9+': ',S);if P=0 then P:=Pos(' : ',S);
    if P>0 then begin
     Key:=Trim(Copy(S,1,P-1));Value:=Trim(Copy(S,P+3,Length(S)));
     if (G=nil) or (G.Tag<>1) then begin G:=NewGrid(2,True);R:=0 end
     else begin R:=G.RowCount;G.RowCount:=R+1 end;
     G.Cells[0,R]:=Key;G.Cells[1,R]:=Value;
    end else begin
     G:=nil;P:=Length(S);
     if (P>0) and (S[P]=':') then AddText(Copy(S,1,P-1),True)
     else AddText(S,False);
    end;
   end;
  finally Cells.Free end;
 finally EnableAlign;FBuilding:=False end;
 LayoutReport;
end;
end.
