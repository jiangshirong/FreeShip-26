{#############################################################################################}
{    This code is distributed as part of the FREE!ship project. FREE!ship is an               }
{    open source surface-modelling program based on subdivision surfaces and intended for     }
{    designing ships.                                                                         }
{                                                                                             }
{    Copyright © 2015, by Mark Malakanov                                                      }
{    FREE!ship Plus for Lazarus project page: https://code.google.com/p/freeship-plus-in-lazarus/  }
{    FREE!ship Plus homepage      : http://www.hydronship.net                                      }
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

unit FreeAboutDlg;

{$mode delphi}

interface

uses
  Classes, SysUtils,      {$IFDEF VER3}
      LazUTF8,
      LazFileUtils,
     {$ELSE}
      FileUtil, //deprecated
     {$ENDIF}
 Forms, Controls, Graphics, Dialogs, ExtCtrls,
  LCLIntf, LCLType, 
  LCLTranslator, LResources, Types, Math,
  StdCtrls, Grids, Buttons,
  FreeVersionUnit, FreeShip_26Icons;

type

{ TFreeAboutDlg }

  TFreeAboutDlg = class(TForm)
    Image1: TImage;
    Label1: TLabel;
    Label2: TLabel;
    Label3: TLabel;
    Label4: TLabel;
    LabelProductVersion: TLabel;
    LabelTitle: TLabel;
    Panel1: TPanel;
    Panel2: TPanel;
    Panel5: TPanel;
    StringGridCopyrights: TStringGrid;
    StringGridVersionInfo: TStringGrid;
    StringGridCredits: TStringGrid;
    procedure FormCreate(Sender: TObject);
    procedure StringGridCopyrightsClick(Sender: TObject);
    procedure StringGridCopyrightsSelectCell(Sender: TObject; aCol,
      aRow: Integer; var CanSelect: Boolean);
  private
    FCopyrightToggle: TBitBtn;
    FVersionToggle: TBitBtn;
    FFooter, FHeader: TPanel;
    FCreditsBody: TPanel;
    FLayoutBusy: Boolean;
    procedure AboutKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
    procedure AboutResize(Sender: TObject);
    procedure DrawInfoCell(Sender: TObject; ACol, ARow: Integer; ARect: TRect; AState: TGridDrawState);
    procedure BuildAboutLayout;
    procedure ToggleVersion(Sender: TObject);
    procedure ToggleCopyrights(Sender: TObject);
    { private declarations }
    procedure AddVersionInfo(sName,sValue:String);
    procedure AddCredits(const Year, Contribution, Contributor: String);
    procedure AddCopyrights(sLine,sContact:String);
  public
    function CreateInformationWindow(IsVersion: Boolean): TForm;
  end;

var
  FreeAboutDlg: TFreeAboutDlg;

implementation

{$R *.lfm}

  resourcestring
  rsCreditYear = 'Year';
  rsCreditContribution = 'Contribution';
  rsCreditContributor = 'Contributor';
  rsAboutVersionButton = 'Version information';
  rsAboutLicenseButton = 'License information';
  rsAboutLegalDetails = 'Copyright and license details';
  rsAboutVersionDetails = 'Version details';
  rsAboutProductVersion = 'Product version';
  rsAboutProgramVersion = 'Program version';
  rsAboutGitRevision = 'Git Revision';
  rsAboutGitCommit = 'Git Commit';
  rsAboutBuildDate = 'Build date';
  rsAboutCompilerVersion = 'Compiler version';
  rsAboutTargetCPU = 'Target CPU';
  rsAboutTargetOS = 'Target OS';
  rsAboutInitialAuthor = 'Initial Author';
  rsAboutAuthor = 'Author';
  rsAboutTranslation = 'Translation';
  rsAboutConversion = 'Conversion to FPC';
  rsAboutIcons = 'New scalable icons';
  rsAboutImprovements = 'Improvements, fixing issues';
  rsAboutCredits = 'Credits';
  rsAboutCopyrightHeading = 'Copyright';
  rsFreeShip_26Copyright = 'Copyright © 2026 蒋世融 (Jiang Shirong) — FreeShip_26 modifications';
  rsAboutCopyrightLine = 'Copyright © %s, by %s';
  rsAboutLicense = 'License: GPL v3+';
  rsContributorJiangName = '蒋世融 (Jiang Shirong)';
  rsFreeShip_26Contribution = 'UI beautification | Massive performance optimization';

procedure TFreeAboutDlg.AboutKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
begin
  if (Key=VK_ESCAPE) and (Shift=[]) then begin
    Key:=0;
    TForm(Sender).Close;
  end;
end;

function TFreeAboutDlg.CreateInformationWindow(IsVersion: Boolean): TForm;
var G,Source:TStringGrid; Heading,LicenseLabel:TLabel; I,J,W,H:Integer;
begin
  Result:=TForm.CreateNew(Self);
  Result.Name:='AboutInformationWindow'; Result.BorderStyle:=bsDialog;
  Result.PopupMode:=pmExplicit; Result.PopupParent:=Self; Result.Position:=poOwnerFormCenter;
  Result.Font.Assign(Font); Result.Color:=Color; Result.KeyPreview:=True;
  Result.OnKeyDown:=AboutKeyDown;
  if IsVersion then begin Source:=StringGridVersionInfo; Result.Caption:=rsAboutVersionButton end
  else begin Source:=StringGridCopyrights; Result.Caption:=rsAboutLicenseButton end;
  W:=Scale96ToForm(960); H:=Scale96ToForm(56);
  Result.ClientWidth:=W;
  Result.ClientHeight:=Scale96ToForm(100)+Source.RowCount*H;
  if not IsVersion then Result.ClientHeight:=Result.ClientHeight+Scale96ToForm(40);
  Heading:=TLabel.Create(Result); Heading.Parent:=Result; Heading.Caption:=Result.Caption;
  Heading.Font.Size:=16; Heading.Font.Style:=[fsBold]; Heading.Font.Color:=RGBToColor(30,83,73);
  Heading.SetBounds(Scale96ToForm(24),Scale96ToForm(24),W-Scale96ToForm(48),Scale96ToForm(32));
  G:=TStringGrid.Create(Result); G.Name:='InformationGrid'; G.Parent:=Result;
  G.FixedRows:=0; G.FixedCols:=0; G.ColCount:=2; G.RowCount:=Source.RowCount;
  G.ScrollBars:=ssNone; G.BorderStyle:=bsNone; G.Options:=[];
  G.DefaultRowHeight:=H; G.DefaultDrawing:=False; G.OnDrawCell:=DrawInfoCell;
  G.SetBounds(Scale96ToForm(24),Scale96ToForm(72),W-Scale96ToForm(48),Source.RowCount*(H+1)+2);
  G.ColWidths[0]:=Round(G.Width*0.52); G.ColWidths[1]:=G.Width-G.ColWidths[0]-2;
  for I:=0 to Source.RowCount-1 do for J:=0 to 1 do G.Cells[J,I]:=Source.Cells[J,I];
  if not IsVersion then begin
    LicenseLabel:=TLabel.Create(Result); LicenseLabel.Parent:=Result; LicenseLabel.Caption:=rsAboutLicense;
    LicenseLabel.SetBounds(Scale96ToForm(24),G.Top+G.Height+Scale96ToForm(12),G.Width,Scale96ToForm(30));
  end;
end;

procedure TFreeAboutDlg.ToggleVersion(Sender: TObject);
var Info:TForm;
begin
  Info:=CreateInformationWindow(True);
  try Info.ShowModal finally Info.Free end;
end;

procedure TFreeAboutDlg.ToggleCopyrights(Sender: TObject);
var Info:TForm;
begin
  Info:=CreateInformationWindow(False);
  try Info.ShowModal finally Info.Free end;
end;

procedure TFreeAboutDlg.AboutResize(Sender: TObject);
var BottomY, RowHeight, I, GroupGap, GapCount, TotalHeight:Integer;
begin
  if (FFooter=nil) or FLayoutBusy then Exit;
  FLayoutBusy:=True;
  try
  FFooter.SetBounds(0,ClientHeight-FFooter.Height,ClientWidth,FFooter.Height);
  FHeader.SetBounds(0,0,ClientWidth,FHeader.Height);
  BottomY:=FFooter.Top;
  Panel5.SetBounds(0,FHeader.Height,ClientWidth,BottomY-FHeader.Height);
  LabelTitle.SetBounds(Scale96ToForm(32),0,FHeader.ClientWidth-Scale96ToForm(64),FHeader.ClientHeight);
  FCreditsBody.SetBounds(Scale96ToForm(32),0,Panel5.ClientWidth-Scale96ToForm(64),Panel5.ClientHeight-Scale96ToForm(20));
  Panel2.SetBounds(0,0,FCreditsBody.ClientWidth,FCreditsBody.ClientHeight);
  Label2.SetBounds(0,0,Panel2.ClientWidth,Scale96ToForm(40));
  GroupGap:=Scale96ToForm(20); GapCount:=0;
  for I:=2 to StringGridCredits.RowCount-1 do
    if StringGridCredits.Cells[1,I]='' then Inc(GapCount);
  RowHeight:=Min(Scale96ToForm(54),(Panel2.ClientHeight-Label2.Height-Scale96ToForm(12)-GroupGap*GapCount) div Max(1,StringGridCredits.RowCount-GapCount)-1);
  StringGridCredits.DefaultRowHeight:=RowHeight;
  TotalHeight:=2;
  for I:=0 to StringGridCredits.RowCount-1 do begin
    StringGridCredits.RowHeights[I]:=RowHeight;
    if (I>0) and (StringGridCredits.Cells[1,I]='') then
      StringGridCredits.RowHeights[I]:=GroupGap;
    Inc(TotalHeight,StringGridCredits.RowHeights[I]+1);
  end;
  StringGridCredits.SetBounds(0,Label2.Height,Panel2.ClientWidth,TotalHeight);
  StringGridCredits.ColWidths[0]:=Scale96ToForm(88);
  StringGridCredits.ColWidths[1]:=Round((StringGridCredits.ClientWidth-StringGridCredits.ColWidths[0])*0.44);
  StringGridCredits.ColWidths[2]:=StringGridCredits.ClientWidth-StringGridCredits.ColWidths[0]-StringGridCredits.ColWidths[1]-3;
  FCopyrightToggle.Left:=FFooter.ClientWidth-FCopyrightToggle.Width-Scale96ToForm(24);
  finally FLayoutBusy:=False end;
end;

procedure TFreeAboutDlg.DrawInfoCell(Sender: TObject; ACol, ARow: Integer; ARect: TRect; AState: TGridDrawState);
var G:TStringGrid; Style:TTextStyle; R:TRect;
begin
  G:=TStringGrid(Sender);
  G.Canvas.Font.Assign(G.Font);
  if (G=StringGridCredits) and (ARow>0) and (G.Cells[1,ARow]='') then begin
    G.Canvas.Brush.Color:=Color; G.Canvas.FillRect(ARect);
    Exit; // No row border crosses the blank band between year groups.
  end;
  G.Canvas.Brush.Color:=clWhite;
  if (G=StringGridCredits) and (ARow=G.RowCount-1) then
    G.Canvas.Brush.Color:=RGBToColor(232,243,238);
  if (G=StringGridCredits) and (ARow=0) then begin
    G.Canvas.Brush.Color:=RGBToColor(235,240,233);
    G.Canvas.Font.Style:=[fsBold];
  end;
  G.Canvas.FillRect(ARect);
  G.Canvas.Font.Color:=RGBToColor(33,59,54);
  if ACol=0 then G.Canvas.Font.Color:=RGBToColor(69,93,85);
  R:=ARect; InflateRect(R,-Scale96ToForm(12),-Scale96ToForm(4));
  Style:=G.Canvas.TextStyle; Style.SingleLine:=False; Style.Wordbreak:=True;
  Style.Layout:=tlCenter; Style.Clipping:=True; Style.ShowPrefix:=False;
  G.Canvas.TextRect(R,R.Left,R.Top,G.Cells[ACol,ARow],Style);
  G.Canvas.Pen.Color:=RGBToColor(225,232,227);
  G.Canvas.Line(ARect.Left,ARect.Bottom-1,ARect.Right,ARect.Bottom-1);
end;

procedure TFreeAboutDlg.BuildAboutLayout;
var I:Integer; G:TStringGrid;
  procedure SetupButton(B:TBitBtn; const ResourceName:string);
  begin
    B.Align:=alNone; B.Parent:=FFooter; B.SetBounds(Scale96ToForm(24),Scale96ToForm(16),Scale96ToForm(220),Scale96ToForm(40));
    B.Layout:=blGlyphLeft; B.Spacing:=Scale96ToForm(10);
    LoadFreeShip_26Icon(B.Glyph,ResourceName,Scale96ToForm(20));
  end;
begin
  DisableAlign;
  try
    Color:=RGBToColor(247,248,244); Font.Color:=RGBToColor(33,59,54);
    Width:=Scale96ToForm(1040); Height:=Scale96ToForm(760);
    Constraints.MinWidth:=Scale96ToForm(840); Constraints.MinHeight:=Scale96ToForm(620);
    Panel5.AutoSize:=False; Panel5.Caption:=''; Panel5.BevelOuter:=bvNone;
    Panel5.Color:=Color; Panel1.Visible:=False; Image1.Visible:=False;
    FHeader:=TPanel.Create(Self); FHeader.Parent:=Self; FHeader.Align:=alTop;
    FHeader.Height:=Scale96ToForm(112); FHeader.BevelOuter:=bvNone; FHeader.Color:=Color;
    LabelTitle.AutoSize:=False; LabelTitle.Parent:=FHeader; LabelTitle.Align:=alClient; LabelTitle.Alignment:=taLeftJustify;
    LabelTitle.Layout:=tlCenter; LabelTitle.BorderSpacing.Left:=Scale96ToForm(32);
    LabelTitle.Font.Color:=RGBToColor(30,83,73); LabelTitle.Font.Size:=30;
    LabelProductVersion.Visible:=False; Label4.Visible:=False;
    FFooter:=TPanel.Create(Self); FFooter.Name:='AboutFooter'; FFooter.Caption:=''; FFooter.Parent:=Self;
    FFooter.Align:=alBottom; FFooter.Top:=ClientHeight; FFooter.Height:=Scale96ToForm(72);
    FFooter.BevelOuter:=bvNone; FFooter.Color:=RGBToColor(235,240,233);
    SetupButton(FVersionToggle,'Info'); SetupButton(FCopyrightToggle,'Legal');
    FVersionToggle.Caption:=rsAboutVersionButton; FCopyrightToggle.Caption:=rsAboutLicenseButton;
    Label1.Parent:=Panel1; Label1.Visible:=False;
    Label3.Parent:=Panel1; Label3.Visible:=False;
    StringGridVersionInfo.Parent:=Panel1; StringGridCopyrights.Parent:=Panel1;
    FCreditsBody.Color:=Color; FCreditsBody.BevelOuter:=bvNone;
    FCreditsBody.BorderSpacing.Left:=Scale96ToForm(32);
    FCreditsBody.BorderSpacing.Right:=Scale96ToForm(32); FCreditsBody.BorderSpacing.Bottom:=Scale96ToForm(20);
    Panel2.AutoSize:=False; Panel2.Color:=Color; Panel2.Caption:=''; Panel2.BevelOuter:=bvNone;
    Label2.AutoSize:=False; Label2.Align:=alTop; Label2.Alignment:=taLeftJustify; Label2.Top:=0; Label2.Height:=Scale96ToForm(40);
    Label2.BorderSpacing.Top:=0; Label2.Font.Size:=13; Label2.Font.Style:=[fsBold];
    StringGridCredits.Align:=alTop; StringGridCredits.Top:=Scale96ToForm(40);
    StringGridCredits.Height:=Scale96ToForm(54)*StringGridCredits.RowCount+2;
    StringGridCredits.DefaultRowHeight:=Scale96ToForm(54);
    Panel2.Height:=StringGridCredits.Height+Label2.Height+Scale96ToForm(12);
    for I:=0 to 2 do begin
      case I of 0:G:=StringGridCredits; 1:G:=StringGridVersionInfo; 2:G:=StringGridCopyrights end;
      G.AutoFillColumns:=False; G.BorderStyle:=bsNone; G.Color:=clWhite;
      G.BorderSpacing.Left:=0; G.BorderSpacing.Right:=0;
      G.ScrollBars:=ssNone; G.Options:=[]; G.DefaultDrawing:=False;
      G.OnDrawCell:=DrawInfoCell;
      G.OnResize:=AboutResize;
      if I>0 then G.DefaultRowHeight:=Scale96ToForm(38);
    end;
    StringGridVersionInfo.Visible:=False; StringGridCopyrights.Visible:=False;
      OnResize:=AboutResize;
  finally EnableAlign end;
  AboutResize(Self);
end;

{ TFreeAboutDlg }

procedure TFreeAboutDlg.AddVersionInfo(sName,sValue:String);
begin
   with StringGridVersionInfo do
   begin
   RowCount := RowCount + 1;
   Cells[0,RowCount-1]:=sName;
   Cells[1,RowCount-1]:=sValue;
   end;
end;

procedure TFreeAboutDlg.AddCredits(const Year, Contribution, Contributor: String);
var R, Previous:Integer;
begin
  R:=StringGridCredits.RowCount;
  Previous:=R-1;
  while (Previous>0) and (StringGridCredits.Cells[0,Previous]='') do Dec(Previous);
  if (Previous>0) and (StringGridCredits.Cells[0,Previous]<>Year) then begin
    StringGridCredits.RowCount:=R+1;
    Inc(R); // A real blank row separates complete year groups.
  end;
  StringGridCredits.RowCount:=R+1;
  if (Previous>0) and (StringGridCredits.Cells[0,Previous]=Year) then
    StringGridCredits.Cells[0,R]:=''
  else StringGridCredits.Cells[0,R]:=Year;
  StringGridCredits.Cells[1,R]:=Contribution;
  StringGridCredits.Cells[2,R]:=Contributor;
end;

procedure TFreeAboutDlg.AddCopyrights(sLine,sContact:String);
begin
   with StringGridCopyrights do
   begin
   RowCount := RowCount + 1;
   Cells[0,RowCount-1]:=sLine;
   Cells[1,RowCount-1]:=sContact;
   end;
end;

procedure TFreeAboutDlg.FormCreate(Sender: TObject);
begin
  Caption := 'FreeShip_26';
  KeyPreview:=True; OnKeyDown:=AboutKeyDown;
  Width := Scale96ToForm(1100); Height := Scale96ToForm(800);
  Constraints.MinWidth := Scale96ToForm(850); Constraints.MinHeight := Scale96ToForm(650);
  Font.Name := 'Microsoft YaHei UI'; Font.Size := 12;
  LabelTitle.Caption := 'FreeShip_26'; LabelTitle.Font.Name := 'Microsoft YaHei UI';
  LabelTitle.Font.Size := 26; LabelTitle.Height := Scale96ToForm(58);
  LabelProductVersion.Font.Size := 14; LabelProductVersion.Height := Scale96ToForm(34);
  Label3.Height := Scale96ToForm(32);
  StringGridVersionInfo.Font.Size := 12; StringGridVersionInfo.DefaultRowHeight := Scale96ToForm(28);
  StringGridCredits.Font.Size := 12; StringGridCredits.DefaultRowHeight := Scale96ToForm(28);
  StringGridCredits.Height := Scale96ToForm(185);
  StringGridCopyrights.Font.Size := 12; StringGridCopyrights.DefaultRowHeight := Scale96ToForm(28);
  StringGridCopyrights.Height := Scale96ToForm(130);
  StringGridCopyrights.Visible := False; Label4.Visible := False;
  FCreditsBody:=TPanel.Create(Self); FCreditsBody.Parent:=Panel5;
  FCreditsBody.Align:=alClient; FCreditsBody.BevelOuter:=bvNone;
  Panel2.Parent:=FCreditsBody; Panel2.Align:=alTop; Panel2.Height:=Scale96ToForm(490);
  FCopyrightToggle := TBitBtn.Create(Self); FCopyrightToggle.Name := 'AboutCopyrightToggle'; FCopyrightToggle.Parent := Panel2;
  FCopyrightToggle.Caption := rsAboutLegalDetails; FCopyrightToggle.Align := alTop;
  FCopyrightToggle.Top := Label4.Top; FCopyrightToggle.Height := Scale96ToForm(38);
  FCopyrightToggle.OnClick := ToggleCopyrights;
  Label1.Visible:=False; StringGridVersionInfo.Visible:=False;
  FVersionToggle:=TBitBtn.Create(Self); FVersionToggle.Name:='AboutVersionToggle';
  FVersionToggle.Parent:=Panel2; FVersionToggle.Caption:=rsAboutVersionDetails;
  FVersionToggle.Align:=alTop; FVersionToggle.Top:=0;
  FVersionToggle.Height:=Scale96ToForm(38); FVersionToggle.OnClick:=ToggleVersion;
  LabelProductVersion.Caption := rsAboutProductVersion+': '+ResourceVersionInfo;
  Label1.Caption:=rsAboutVersionDetails; Label2.Caption:=rsAboutCredits;
  Label3.Caption:=rsAboutLicense; Label4.Caption:=rsAboutCopyrightHeading;

  //StringGridVersionInfo.ColCount:=2;
  //StringGridVersionInfo.RowCount:=0;
  AddVersionInfo(rsAboutProductVersion,FREESHIP_VERSION);
  AddVersionInfo(rsAboutProgramVersion,ResourceVersionInfo);
  AddVersionInfo(rsAboutGitRevision,IntToStr(GITVERSION_REVISION));
  AddVersionInfo(rsAboutGitCommit,GITCOMMIT);
  AddVersionInfo(rsAboutBuildDate,COMPILE_DATE+' '+COMPILE_TIME);
  AddVersionInfo(rsAboutCompilerVersion,{$I %FPCVERSION%});
  AddVersionInfo(rsAboutTargetCPU,TARGET_CPU);
  AddVersionInfo(rsAboutTargetOS,TARGET_OS);

  StringGridCredits.Columns.Clear;
  StringGridCredits.ColCount:=3; StringGridCredits.RowCount:=1; StringGridCredits.FixedRows:=1;
  StringGridCredits.Cells[0,0]:=rsCreditYear;
  StringGridCredits.Cells[1,0]:=rsCreditContribution;
  StringGridCredits.Cells[2,0]:=rsCreditContributor;
  AddCredits('2005',rsAboutInitialAuthor,'Martijn van Engeland');
  AddCredits('2007',rsAboutAuthor,'Timoshenko Victor F');
  AddCredits('2007',rsAboutTranslation,'Martijn van Engeland');
  AddCredits('2015',rsAboutConversion,'Mark Malakanov');
  AddCredits('2015',rsAboutIcons,'Tom (https://github.com/tvld)');
  AddCredits('2015',rsAboutImprovements,'Sönke J. Peters');
  AddCredits('2026',rsFreeShip_26Contribution,rsContributorJiangName);
  StringGridCredits.Height:=StringGridCredits.RowCount*StringGridCredits.DefaultRowHeight+Scale96ToForm(8);

  AddCopyrights(Format(rsAboutCopyrightLine,['2005','Martijn van Engeland']),'');
  AddCopyrights(Format(rsAboutCopyrightLine,['2007-2012','Timoshenko Victor F.']),'vftim@rambler.ru');
  AddCopyrights(Format(rsAboutCopyrightLine,['2015','Mark Malakanov']),'markmal@github.com');
  AddCopyrights(rsFreeShip_26Copyright,'');
  BuildAboutLayout;

end;



var SelectedCellColumn,SelectedCellRow: integer;
procedure TFreeAboutDlg.StringGridCopyrightsClick(Sender: TObject);
var email:string;
begin
  if SelectedCellColumn = 1 then
  begin
    email:=StringGridCopyrights.Cells[1,SelectedCellRow];
    if email = 'vftim@rambler.ru' then
      OpenDocument('mailto:'+email+'?subject=FREE!ship+');
    if email = 'markmal@github.com' then
      OpenDocument('mailto:'+email+'?subject=FreeShip in Lazarus');
  end;
end;

procedure TFreeAboutDlg.StringGridCopyrightsSelectCell(Sender: TObject; aCol,
  aRow: Integer; var CanSelect: Boolean);
begin
  SelectedCellRow:=aRow; SelectedCellColumn:=aCol;
end;

end.

