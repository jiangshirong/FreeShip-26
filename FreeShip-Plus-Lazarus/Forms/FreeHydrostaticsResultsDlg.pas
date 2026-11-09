{#############################################################################################}
{    This code is distributed as part of the FREE!ship project. FREE!ship is an               }
{    open source surface-modelling program based on subdivision surfaces and intended for     }
{    designing ships.                                                                         }
{                                                                                             }
{    Copyright © 2005, by Martijn van Engeland                                                }
{    e-mail                  : Info@FREEship.org                                              }
{    FREE!ship project page  : https://sourceforge.net/projects/freeship                      }
{    FREE!ship homepage      : www.FREEship.org                                               }
{                                                                                             }
{    This program is free software; you can redistribute it and/or modify it under            }
{    the terms of the GNU General Public License as published by the                          }
{    Free Software Foundation; either version 2 of the License, or (at your option)           }
{    any later version.                                                                       }
{                                                                                             }
{    This program is distributed in the hope that it will be useful, but WITHOUT ANY          }
{    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A          }
{    PARTICULAR PURPOSE. See the GNU General Public License for more details.                 }
{                                                                                             }
{    You should have received a copy of the GNU General Public License along with             }
{    this program; if not, write to the Free Software Foundation, Inc.,                       }
{    59 Temple Place, Suite 330, Boston, MA 02111-1307 USA                                    }
{                                                                                             }
{#############################################################################################}

unit FreeHydrostaticsResultsDlg;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

interface

uses
     {$ifdef Windows}
     Windows,
     {$endif}
     {$ifdef LCL}
     LCLIntf, LCLType, //
     LResources,
     PrintersDlgs, Printer4Lazarus, FreePrinter,
     {$endif}
     //Messages,
     SysUtils,
     Variants,
     Classes,
     Graphics,
     Controls,
     Forms,
     Dialogs,
     Grids,
     ExtCtrls,
     StdCtrls,
     Printers,
     Buttons,
     LCLTranslator,
     FreeShipUnit,
     FreeStringsUnit;

type

{ TFreeHydrostaticsResultsDialog }

 TFreeHydrostaticsResultsDialog  = class(TForm)
                                             Panel1: TPanel;
                                             Panel2: TPanel;
                                             Grid: TStringGrid;
                                             Header: TMemo;
                                             Splitter1: TSplitter;
                                             ButtonPrint: TSpeedButton;
                                             ButtonSave: TSpeedButton;
                                             ButtonClose: TSpeedButton;
    PrintDialog: TPrintDialog;
    SaveDialog: TSaveDialog;
    procedure FormCreate(Sender: TObject);
                                             procedure GridDrawCell(Sender: TObject; ACol, ARow: Integer;Rect: TRect; State: TGridDrawState);
                                             procedure FormResize(Sender: TObject);
                                             procedure ButtonCloseClick(Sender: TObject);
    procedure ButtonSaveClick(Sender: TObject);
    procedure ButtonPrintClick(Sender: TObject);
                                           private { Private declarations }
                                              ColWidths : array of Integer;
                                              FHeaderView:TScrollBox;
                                              FColumnHints:TStringList;
                                              FCoefficientNote:string;
                                              FDatumNote:string;
    procedure GridCellHint(Sender:TObject;ACol,ARow:Integer;var HintText:string);
                                           public  { Public declarations }
    destructor Destroy;override;
    procedure PrepareHeaderView;
    function HeaderMeaning(ACol,ARow:Integer):string;
                                              function Execute:Boolean;
                                        end;

var FreeHydrostaticsResultsDialog: TFreeHydrostaticsResultsDialog;

implementation

uses Math,FreeReportPresentation,FreeReportTables;

{$IFnDEF FPC}
  {$R *.dfm}
{$ELSE}
  {$R *.lfm}
{$ENDIF}

procedure TFreeHydrostaticsResultsDialog.FormCreate(Sender: TObject);
begin
  FColumnHints:=TStringList.Create;
  Grid.ShowHint:=True;Grid.Options:=Grid.Options+[goCellHints];
  Grid.CellHintPriority:=chpAllNoDefault;Grid.OnGetCellHint:=GridCellHint;
  EnlargeHydroReport(Self);
  ButtonClose.Visible:=False;
  Grid.Font.Name:='Microsoft YaHei UI';Grid.Font.Size:=14;
  Grid.FixedColor:=RGBToColor(229,240,234);
  Grid.GridLineColor:=RGBToColor(220,230,224);
  Grid.DefaultRowHeight:=40;
  FHeaderView:=THydroReportView.Create(Self);
  FHeaderView.Parent:=Header.Parent;FHeaderView.Align:=alTop;FHeaderView.Height:=280;
  Header.Visible:=False;
  Header.Font.Size:=14;
  {$IFDEF WINDOWS}
  // The report is padded with spaces, so use a font with fixed-width digits.
  // SimHei also provides full-width Chinese glyphs without bitmap strikes.
  Header.Font.Name := 'Consolas';
  if Assigned(GlobalFreeship) and
     SameText(GlobalFreeship.Preferences.Language, 'Chinese') then
    Header.Font.Name := 'SimHei';
  Header.Font.Size := 14;
  Header.Font.Quality := fqCleartypeNatural;
  {$ENDIF}
end;

destructor TFreeHydrostaticsResultsDialog.Destroy;
begin FreeAndNil(FColumnHints);inherited Destroy end;

procedure TFreeHydrostaticsResultsDialog.PrepareHeaderView;
var VisibleHeader:TStringList;I,J,P,Column:Integer;S,Symbol,Meaning:string;
begin
 FColumnHints.Clear;FCoefficientNote:='';FDatumNote:='';
 VisibleHeader:=TStringList.Create;
 try
  VisibleHeader.Assign(Header.Lines);
  for I:=VisibleHeader.Count-1 downto 0 do begin
   S:=Trim(VisibleHeader[I]);
   if S=rs_NOTE_1__Draft__and_all_other_vertical_heights__is_measured_from_point_of_the_hull_Z_0_ then FDatumNote:=S;
   if (S=rs_NOTE_2__All_calculated_coefficients_based_on_actual_dimensions_of_submerged_body_) or
      (S=rs_NOTE_2__All_calculated_coefficients_based_on_project_length___draft_and_beam_) then FCoefficientNote:=S;
   P:=Pos(#9+': ',S);if P=0 then P:=Pos(' : ',S);if P=0 then Continue;
   Symbol:=Trim(Copy(S,1,P-1));Column:=-1;
   for J:=0 to Grid.ColCount-1 do if SameText(Symbol,Grid.Cells[J,0]) then Column:=J;
   // These two column titles are translated; their legacy glossary keys are not.
   if (Symbol='Volume') and (Grid.ColCount>4) then Column:=4;
   if (Symbol='Displ.') and (Grid.ColCount>5) then Column:=5;
   if Column<0 then Continue;
   Meaning:=Trim(Copy(S,P+3,Length(S)));
   FColumnHints.Values[IntToStr(Column)]:=Meaning;
   VisibleHeader.Delete(I);
  end;
  THydroReportView(FHeaderView).SetReport(VisibleHeader);
 finally VisibleHeader.Free end;
end;

function TFreeHydrostaticsResultsDialog.HeaderMeaning(ACol,ARow:Integer):string;
begin
 Result:='';if (ARow<0) or (ARow>1) or (ACol<0) or (ACol>=Grid.ColCount) then Exit;
 Result:=FColumnHints.Values[IntToStr(ACol)];if Result='' then Result:=Grid.Cells[ACol,0];
 if (ARow=1) and (Grid.Cells[ACol,1]='[-]') and (FCoefficientNote<>'') then Result:=Result+LineEnding+FCoefficientNote;
 if (ACol=0) and (FDatumNote<>'') then Result:=Result+LineEnding+FDatumNote;
end;

procedure TFreeHydrostaticsResultsDialog.GridCellHint(Sender:TObject;ACol,ARow:Integer;var HintText:string);
begin HintText:=HeaderMeaning(ACol,ARow) end;

procedure TFreeHydrostaticsResultsDialog.GridDrawCell(Sender: TObject;ACol, ARow: Integer; Rect: TRect; State: TGridDrawState);
var W    : Integer;
    Str  : string;
    Back : TColor;
begin
   if (ACol=0) or (ARow in [0,1]) then
   begin
      if Grid.Canvas.Font.Style=[] then Grid.Canvas.Font.Style:=[fsBold];
   end else
   begin
      if Grid.Canvas.Font.Style=[fsBold] then Grid.Canvas.Font.Style:=[];
   end;
   if (ARow>1) and (ACol>0) then
   begin
      if gdSelected in state then
      begin
         //Back:=clWhite;
         //Grid.Canvas.Font.Color:=clBlack;
      end else if Odd(ARow) then
      begin
         Back:=RGB(235,235,235);
         if Grid.Canvas.Brush.Color<>Back then Grid.Canvas.Brush.Color:=Back;
      end else
      begin
         if Grid.Canvas.Brush.Color<>clWindow then Grid.Canvas.Brush.Color:=clWindow;
      end;
      Grid.Canvas.Rectangle(Rect);
   end;
   Str:=Grid.Cells[ACol,ARow];
   W:=Grid.Canvas.TextWidth(Str);

   if (ARow in [0,1]) or (ACol = 0)
     then
      begin
        Grid.Canvas.FillRect(Rect);
        Grid.Canvas.TextRect(Rect,(Rect.Left+Rect.Right-W) div 2,Rect.Top+3,Str);
      end
     else Grid.Canvas.TextRect(Rect,Rect.Right-W-5,Rect.Top+3,Str);

end;{TFreeHydrostaticsResultsDialog.GridDrawCell}

function TFreeHydrostaticsResultsDialog.Execute:Boolean;
var I,J,W : Integer;
begin
   // Store the column width for resizing purposes
   setlength(ColWidths,Grid.ColCount);
   Grid.Canvas.Font.Assign(Grid.Font);
   for I:=0 to Grid.ColCount-1 do begin
     W:=90;for J:=0 to Grid.RowCount-1 do W:=Max(W,Grid.Canvas.TextWidth(Grid.Cells[I,J])+24);
     ColWidths[I]:=W;Grid.ColWidths[I]:=W;
   end;

   GlobalFreeship.Preferences.LoadImageIntoBitmap(ButtonPrint.Glyph,'Print');
   GlobalFreeship.Preferences.LoadImageIntoBitmap(ButtonSave.Glyph,'Save');
   GlobalFreeship.Preferences.LoadImageIntoBitmap(ButtonClose.Glyph,'Ok');
   //ShowTranslatedValues(Self);

   PrepareReportMemo(Header);
   PrepareHeaderView;
   ShowModal;
   Result:=modalresult=mrOK;
end;{TFreeHydrostaticsResultsDialog.Execute}

procedure TFreeHydrostaticsResultsDialog.FormResize(Sender: TObject);
var I,Total : Integer;
    remain  : Integer;
    NewW    : Integer;
    Fraction: Double;
begin
   ButtonClose.Left:=Panel1.Width-ButtonClose.Width-5;
   if Length(ColWidths)<>Grid.ColCount then Exit;
   if FHeaderView<>nil then FHeaderView.Height:=Min(300,ClientHeight div 3);
   Total:=0;
   for I:=1 to Grid.ColCount do Inc(Total,ColWidths[I-1]);
   if Total<>0 then
   begin
      Remain:=Grid.ClientWidth-10;
      for I:=1 to Grid.ColCount do
      begin
         Fraction:=ColWidths[I-1]/Total;// percentage of total width when shown for the first time
         NewW:=Max(ColWidths[I-1],trunc(Fraction*remain));
         Grid.ColWidths[I-1]:=NewW;
      end;
   end;
end;{TFreeHydrostaticsResultsDialog.FormResize}

procedure TFreeHydrostaticsResultsDialog.ButtonCloseClick(Sender: TObject);
begin
   ModalResult:=mrOK;
end;{TFreeHydrostaticsResultsDialog.Button1Click}

procedure TFreeHydrostaticsResultsDialog.ButtonSaveClick(Sender: TObject);
var Strings    : TStringList;
    MaxWidth   : array of integer;
    I,J,L,N    : Integer;
    Str,Tmp    : Ansistring;
begin
   if SaveDialog.Execute then
   begin
      strings:=TStringList.Create;
      Strings.Assign(Header.Lines);
      Setlength(MaxWidth,Grid.ColCount);
      for I:=1 to Grid.ColCount do
      begin
         MaxWidth[I-1]:=0;
         for J:=1 to Grid.RowCount do
         begin
            L:=Length(Grid.Cells[I-1,J-1]);
            if L>MaxWidth[I-1] then MaxWidth[I-1]:=L;
         end;
      end;
      Strings.Add('');
      Strings.Add('');
      for I:=1 to Grid.RowCount do
      begin
         Str:='';
         for J:=1 to Grid.ColCount do
         begin
            Tmp:=Grid.Cells[J-1,I-1];
            L:=Length(Tmp);
            for N:=L+1 to MaxWidth[J-1] do Tmp:=#32+Tmp;
            if J=1 then Str:=Tmp
                   else Str:=Str+#32+Tmp;
         end;
         Strings.Add(Str);
      end;
      // Skip translation
      Strings.SaveToFile(ChangeFileExt(SaveDialog.Filename,'.txt'));
      // End Skip translation
      FreeAndNil(Strings);
   end;
end;{TFreeHydrostaticsResultsDialog.SpeedButton2Click}

procedure TFreeHydrostaticsResultsDialog.ButtonPrintClick(Sender: TObject);
var PrintText : TextFile;
    MaxWidth  : array of integer;
    I,J,L,N   : Integer;
    Str,Tmp   : Ansistring;
begin
   if PrintDialog.Execute then
   begin
      AssignPrn(PrintText);
      Rewrite(PrintText);
      Printer.Canvas.Font.Assign(Header.Font);
      for I:=1 to Header.Lines.Count do Writeln(PrintText,#32,Header.Lines[I-1]);

      Setlength(MaxWidth,Grid.ColCount);
      for I:=1 to Grid.ColCount do
      begin
         MaxWidth[I-1]:=0;
         for J:=1 to Grid.RowCount do
         begin
            L:=Length(Grid.Cells[I-1,J-1]);
            if L>MaxWidth[I-1] then MaxWidth[I-1]:=L;
         end;
      end;
      Writeln(PrintText);
      Writeln(PrintText);
      Printer.Canvas.Font.Size:=Printer.Canvas.Font.Size-1;
      for I:=1 to Grid.RowCount do
      begin
         Str:='';
         for J:=1 to Grid.ColCount do
         begin
            Tmp:=Grid.Cells[J-1,I-1];
            L:=Length(Tmp);
            for N:=L+1 to MaxWidth[J-1] do Tmp:=#32+Tmp;
            if J=1 then Str:=Tmp
                   else Str:=Str+#32+Tmp;
         end;
         Writeln(PrintText,#32#32,Str);
      end;
      CloseFile(PrintText);
   end;
end;{TFreeHydrostaticsResultsDialog.SpeedButton1Click}

end.

