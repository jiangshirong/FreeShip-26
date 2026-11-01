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

unit FreeHydrostaticsDlg;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

interface

uses
{$IFnDEF FPC}
  Windows,
  RichEdit,
{$ELSE}
  LCLIntf, LCLType,
  PrintersDlgs, Printer4Lazarus, FreePrinter,
{$ENDIF}
  //Messages,
  SysUtils,
  Variants,
  Classes,
  Graphics,
  Controls,
  Forms,
  Dialogs,
  StdCtrls,
  LCLTranslator,
  ComCtrls,
  Buttons,
  Printers,
  ExtCtrls,
  FreeShipUnit,
  FreeStringsUnit;

type

  { TFreeHydrostaticsDialog }

  TFreeHydrostaticsDialog = class(TForm)
    Panel1: TPanel;
    Edit: TMemo;
    Panel22: TPanel;
    ButtonClose: TSpeedButton;
    ButtonPrint: TSpeedButton;
    PrintDialog: TPrintDialog;
    ButtonSave: TSpeedButton;
    SaveDialog: TSaveDialog;
    procedure ButtonCloseClick(Sender: TObject);
    procedure ButtonPrintClick(Sender: TObject);
    procedure ButtonSaveClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure FormShow(Sender: TObject);
  private   { Private declarations }
    FReportView:TScrollBox;
    FAdditionalReports: TStringList;
    FAuxiliaryNotice: string;
  public    { Public declarations }
    destructor Destroy; override;
    function SaveReport(const FileName: string): string;
    property AdditionalReports: TStringList read FAdditionalReports;
    property AuxiliaryNotice: string read FAuxiliaryNotice write FAuxiliaryNotice;
  end;

var
  FreeHydrostaticsDialog: TFreeHydrostaticsDialog;

implementation

uses FreeReportPresentation,FreeReportTables;

resourcestring
  rsHydroReportSaved = 'Report saved. Additional tables (%s):%s%s';
  rsHydroAuxCreateFailed = 'Report saved, but additional tables could not be saved to %s.';
  rsHydroAuxWriteFailed = 'Report saved, but additional tables are incomplete in %s: %s';
  rsHydroSaveFailed = 'Could not save report to %s: %s';
  rsHydroSaveHint = 'Save the report and additional tables in a new <report>-data folder.';

{$IFnDEF FPC}
  {$R *.dfm}

{$ELSE}
  {$R *.lfm}
{$ENDIF}

procedure TFreeHydrostaticsDialog.FormCreate(Sender: TObject);
var TextStorage:TPanel;
begin
  FAdditionalReports := TStringList.Create;
  EnlargeHydroReport(Self);
  ButtonClose.Visible:=False;
  FReportView:=THydroReportView.Create(Self);
  FReportView.Parent:=Panel1;FReportView.Align:=alClient;
  TextStorage:=TPanel.Create(Self);
  TextStorage.Parent:=Panel1;TextStorage.Visible:=False;
  Edit.Parent:=TextStorage;Edit.Visible:=False;
  {$IFDEF WINDOWS}
  Edit.Font.Name := 'Consolas';
  if Assigned(GlobalFreeship) and
     SameText(GlobalFreeship.Preferences.Language, 'Chinese') then
    Edit.Font.Name := 'SimHei';
  Edit.Font.Size := 11;
  Edit.Font.Quality := fqCleartypeNatural;
  {$ENDIF}
end;

destructor TFreeHydrostaticsDialog.Destroy;
var I: integer;
begin
  if Assigned(FAdditionalReports) then
    for I := 0 to FAdditionalReports.Count - 1 do
      FAdditionalReports.Objects[I].Free;
  FreeAndNil(FAdditionalReports);
  inherited Destroy;
end;

procedure TFreeHydrostaticsDialog.ButtonCloseClick(Sender: TObject);
begin
  Close;
end;{TFreeHydrostaticsDialog.Button1Click}

procedure TFreeHydrostaticsDialog.ButtonPrintClick(Sender: TObject);
var
  PrintText: TextFile;
  Str: ansistring;
  I: integer;
  TextCopy:TStringList;
begin
  if PrintDialog.Execute then
  begin

    AssignPrn(PrintText);
    Rewrite(PrintText);
    Printer.Canvas.Font.Assign(Edit.Font);
    TextCopy:=ReportPlainText(Edit.Lines);
    try
      for I := 0 to TextCopy.Count-1 do Writeln(PrintText,TextCopy[I]);
    finally TextCopy.Free end;
    CloseFile(PrintText);
  end;
end;{TFreeHydrostaticsDialog.SpeedButton1Click}

procedure TFreeHydrostaticsDialog.ButtonSaveClick(Sender: TObject);
var DirectoryName, Notice: string;
begin
  if not SaveDialog.Execute then Exit;
  try
    DirectoryName := SaveReport(ChangeFileExt(SaveDialog.FileName, '.txt'));
    if DirectoryName <> '' then
    begin
      Notice := Format(rsHydroReportSaved, [FAdditionalReports.CommaText, LineEnding, DirectoryName]);
      if FAuxiliaryNotice <> '' then Notice := Notice + LineEnding + FAuxiliaryNotice;
      MessageDlg(Notice, mtInformation, [mbOK], 0);
    end
    else if FAuxiliaryNotice <> '' then
      MessageDlg(FAuxiliaryNotice, mtWarning, [mbOK], 0);
  except
    on E: Exception do MessageDlg(E.Message, mtError, [mbOK], 0);
  end;
end;{TFreeHydrostaticsDialog.SpeedButton2Click}

function TFreeHydrostaticsDialog.SaveReport(const FileName: string): string;
var BaseName, Candidate: string; I, Suffix: integer; TextCopy:TStringList;
begin
  Result := '';
  try
    TextCopy:=ReportPlainText(Edit.Lines);
    try TextCopy.SaveToFile(FileName) finally TextCopy.Free end;
  except
    on E: Exception do raise EInOutError.CreateFmt(rsHydroSaveFailed, [ExpandFileName(FileName), E.Message]);
  end;
  if FAdditionalReports.Count = 0 then Exit;
  BaseName := ChangeFileExt(ExpandFileName(FileName), '') + '-data';
  Candidate := BaseName;
  Suffix := 1;
  // CreateDir claims a new directory; an existing directory is never reused.
  while not CreateDir(Candidate) do
  begin
    if not (DirectoryExists(Candidate) or FileExists(Candidate)) then
      raise EInOutError.CreateFmt(rsHydroAuxCreateFailed, [Candidate]);
    Inc(Suffix);
    Candidate := BaseName + '-' + IntToStr(Suffix);
  end;
  try
    for I := 0 to FAdditionalReports.Count - 1 do
      TStrings(FAdditionalReports.Objects[I]).SaveToFile(
        IncludeTrailingPathDelimiter(Candidate) + FAdditionalReports[I]);
  except
    on E: Exception do
      raise EInOutError.CreateFmt(rsHydroAuxWriteFailed, [Candidate, E.Message]);
  end;
  Result := Candidate;
end;

procedure TFreeHydrostaticsDialog.FormShow(Sender: TObject);
var
  I: integer;
  S: string;
begin
  PrepareReportMemo(Edit);
  THydroReportView(FReportView).SetReport(Edit.Lines);
  I := Edit.Lines.Count;
  S := Edit.Lines.CommaText;
  // The memo is export storage, never a visible or focused report surface.
  Edit.Visible:=False;
  I := Edit.Lines.Count;
  S := Edit.Lines.CommaText;

  GlobalFreeship.Preferences.LoadImageIntoBitmap(ButtonPrint.Glyph, 'Print');
  GlobalFreeship.Preferences.LoadImageIntoBitmap(ButtonSave.Glyph, 'Save');
  GlobalFreeship.Preferences.LoadImageIntoBitmap(ButtonClose.Glyph, 'Ok');
  if FAdditionalReports.Count > 0 then
  begin
    ButtonSave.Hint := rsHydroSaveHint;
    ButtonSave.ShowHint := True;
  end;
  //ShowTranslatedValues(Self);

end;{TFreeHydrostaticsDialog.FormShow}

end.
