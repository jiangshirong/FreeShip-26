{#############################################################################################}
{    This code is distributed as part of the FREE!ship project. FREE!ship is an               }
{    open source surface-modelling program based on subdivision surfaces and intended for     }
{    designing ships.                                                                         }

{    Copyright © 2005, by Martijn van Engeland                                                }
{    e-mail                  : Info@FREEship.org                                              }
{    FREE!ship project page  : https://sourceforge.net/projects/freeship                      }
{    FREE!ship homepage      : www.FREEship.org                                               }

{    This program is free software; you can redistribute it and/or modify it under            }
{    the terms of the GNU General Public License as published by the                          }
{    Free Software Foundation; either version 2 of the License, or (at your option)           }
{    any later version.                                                                       }

{    This program is distributed in the hope that it will be useful, but WITHOUT ANY          }
{    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A          }
{    PARTICULAR PURPOSE. See the GNU General Public License for more details.                 }

{    You should have received a copy of the GNU General Public License along with             }
{    this program; if not, write to the Free Software Foundation, Inc.,                       }
{    59 Temple Place, Suite 330, Boston, MA 02111-1307 USA                                    }

{#############################################################################################}
unit FreeHydrostaticsFrm;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

interface

uses
{$IFnDEF FPC}
  Windows,
{$ELSE}
  LCLIntf, LCLType, //LMessages,
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
  FreeShipUnit,
  FreeTypes,
  FreeGeometry,
  ExtCtrls,
  FreeHydrostaticsResultsDlg,
  Buttons, Spin;

type

  TFreeDraftSequence = array of Single;

  { TFreeHydrostaticsForm }

  TFreeHydrostaticsForm = class(TForm)
    FloatSpinEdit1: TFloatSpinEdit;
    FloatSpinEdit2: TFloatSpinEdit;
    FloatSpinEdit3: TFloatSpinEdit;
    FloatSpinEdit4: TFloatSpinEdit;
    Label1: TLabel;
    Label2: TLabel;
    Label3: TLabel;
    Label4: TLabel;
    Label9: TLabel;
    Panel2: TPanel;
    Button1: TSpeedButton;
    Panel1: TPanel;
    Button2: TSpeedButton;
    _Label10: TLabel;
    _Label11: TLabel;
    _Label5: TLabel;
    procedure Edit1KeyPress(Sender: TObject;
      var Key: char);
    procedure Button1Click(Sender: TObject);
    procedure CloseFromTitleBar(Sender:TObject; var CloseAction:TCloseAction);
    procedure Edit1Exit(Sender: TObject);
    procedure Edit3Exit(Sender: TObject);
    procedure Edit4Exit(Sender: TObject);
    procedure Edit2Exit(Sender: TObject);
    procedure Button2Click(Sender: TObject);
  private   { Private declarations }
    FFreeShip: TFreeShip;
    function FGetStartDraft: single;
    procedure FSetStartDraft(Val: single);
    function FGetEndDraft: single;
    procedure FSetEndDraft(Val: single);
    function FGetDraftStep: single;
    procedure FSetDraftStep(Val: single);
    function FGetTrim: single;
    procedure FSetTrim(Val: single);
  public    { Public declarations }
    function Execute(FreeShip: TFreeShip): boolean;
    property DraftStep: single
      read FGetDraftStep write FSetDraftStep;
    property EndDraft: single
      read FGetEndDraft write FSetEndDraft;
    property StartDraft: single
      read FGetStartDraft write FSetStartDraft;
    property Trim: single
      read FGetTrim write FSetTrim;
  end;

const
  MaxHydrostaticDraftRows = 10000;

function BuildHydrostaticDraftSequence(StartValue, EndValue, StepValue: Single): TFreeDraftSequence;

var
  FreeHydrostaticsForm: TFreeHydrostaticsForm;

implementation

{$IFnDEF FPC}
  {$R *.dfm}

{$ELSE}
  {$R *.lfm}
{$ENDIF}

uses FreeLanguageSupport, Math;

resourcestring
  rsHydroDraft = 'Draft';
  rsHydroTrim = 'Trim';
  rsHydroVolume = 'Volume';
  rsHydroDisplacement = 'Displ';
  rsHydroDraftFinite = 'Draft values and step must be finite numbers.';
  rsHydroDraftRange = 'The end draft must not be below the start draft, and the step must be positive.';
  rsHydroDraftRows = 'Too many draft rows. Increase the step (maximum %d rows).';
  rsHydroDraftPrecision = 'The draft step is too small for the precision of these draft values.';

function BuildHydrostaticDraftSequence(StartValue, EndValue, StepValue: Single): TFreeDraftSequence;
var
  Ratio, StartWide, EndWide, StepWide: Double;
  WholeSteps, I, Count: Integer;
  Value: Single;
begin
  SetLength(Result, 0);
  if IsNan(StartValue) or IsInfinite(StartValue) or IsNan(EndValue) or
    IsInfinite(EndValue) or IsNan(StepValue) or IsInfinite(StepValue) then
    raise EArgumentException.Create(rsHydroDraftFinite);
  if (EndValue < StartValue) or (StepValue <= 0) then
    raise EArgumentException.Create(rsHydroDraftRange);
  if EndValue = StartValue then
  begin
    SetLength(Result, 1);
    Result[0] := StartValue;
    Exit;
  end;
  StartWide := StartValue;
  EndWide := EndValue;
  StepWide := StepValue;
  Ratio := (EndWide - StartWide) / StepWide;
  if Ratio > MaxHydrostaticDraftRows then
    raise EArgumentException.CreateFmt(rsHydroDraftRows, [MaxHydrostaticDraftRows]);
  WholeSteps := Math.Min(Trunc(Ratio), MaxHydrostaticDraftRows - 1);
  SetLength(Result, WholeSteps + 2);
  Count := 0;
  for I := 0 to WholeSteps do
  begin
    // Compute each value from its index, without accumulating rounding error.
    Value := StartWide + I * StepWide;
    if Value >= EndValue then Break;
    if (Count > 0) and (Value <= Result[Count - 1]) then
      raise EArgumentException.Create(rsHydroDraftPrecision);
    Result[Count] := Value;
    Inc(Count);
  end;
  if Count >= MaxHydrostaticDraftRows then
    raise EArgumentException.CreateFmt(rsHydroDraftRows, [MaxHydrostaticDraftRows]);
  Result[Count] := EndValue;
  SetLength(Result, Count + 1);
end;

function TFreeHydrostaticsForm.FGetStartDraft: single;
begin
  Result := FloatSpinEdit1.Value;
end;{TFreeHydrostaticsForm.FGetStartDraft}

procedure TFreeHydrostaticsForm.FSetStartDraft(Val: single);
begin
  FloatSpinEdit1.Value := Val;
end;{TFreeHydrostaticsForm.FSetStartDraft}

function TFreeHydrostaticsForm.FGetTrim: single;
begin
  Result := FloatSpinEdit4.Value;
end;{TFreeHydrostaticsForm.FGetTrim}

procedure TFreeHydrostaticsForm.FSetTrim(Val: single);
begin
  FloatSpinEdit4.Value := Val;
end;{TFreeHydrostaticsForm.FSetTrim}

function TFreeHydrostaticsForm.FGetEndDraft: single;
begin
  Result := FloatSpinEdit2.Value;
end;{TFreeHydrostaticsForm.FGetEndDraft}

procedure TFreeHydrostaticsForm.FSetEndDraft(Val: single);
begin
  FloatSpinEdit2.Value := Val;
end;{TFreeHydrostaticsForm.FSetEndDraft}

function TFreeHydrostaticsForm.FGetDraftStep: single;
begin
  Result := FloatSpinEdit3.Value;
end;{TFreeHydrostaticsForm.FGetDraftStep}

procedure TFreeHydrostaticsForm.FSetDraftStep(Val: single);
begin
  FloatSpinEdit3.Value := Val;
end;{TFreeHydrostaticsForm.FSetDraftStep}

function TFreeHydrostaticsForm.Execute(FreeShip: TFreeShip): boolean;
var
  Str: string;
begin
  FFreeShip := Freeship;
  Str := LengthStr(FreeShip.ProjectSettings.ProjectUnits);
  _Label5.Caption := Str;
  Label9.Caption := Str;
  _Label10.Caption := Str;
  _Label11.Caption := Str;
  ShowModal;
  Result := Modalresult = mrOk;
end;{TFreeHydrostaticsForm.Execute}

procedure TFreeHydrostaticsForm.Edit1KeyPress(Sender: TObject; var Key: char);
begin
  if (Key in [#8, '1'..'9', '0', '-', #13]) or (Key = FormatSettings.DecimalSeparator) then
  else
    key := #0;
end;{TFreeHydrostaticsForm.Edit1KeyPress}

procedure TFreeHydrostaticsForm.CloseFromTitleBar(Sender:TObject; var CloseAction:TCloseAction);
begin
  ModalResult:=mrOk;
end;

procedure TFreeHydrostaticsForm.Button1Click(Sender: TObject);
begin
  ModalResult := mrOk;
end;{TFreeHydrostaticsForm.Button1Click}

procedure TFreeHydrostaticsForm.Edit1Exit(Sender: TObject);
begin
  StartDraft := StartDraft;
end;{TFreeHydrostaticsForm.Edit1Exit}

procedure TFreeHydrostaticsForm.Edit3Exit(Sender: TObject);
begin
  DraftStep := DraftStep;
end;{TFreeHydrostaticsForm.Edit3Exit}

procedure TFreeHydrostaticsForm.Edit4Exit(Sender: TObject);
begin
  Trim := Trim;
end;{TFreeHydrostaticsForm.Edit4Exit}

procedure TFreeHydrostaticsForm.Edit2Exit(Sender: TObject);
begin
  EndDraft := Enddraft;
end;{TFreeHydrostaticsForm.Edit2Exit}

procedure TFreeHydrostaticsForm.Button2Click(Sender: TObject);
var
  Value: single;
  HydObject: TFreeHydrostaticCalc;
  I, DraftIndex: integer;
  Drafts: TFreeDraftSequence;
  PrevCursor: TCursor;
  ResultsDlg: TFreeHydrostaticsResultsDialog;
  Units: TFreeUnitType;
  Strings: TStringList;
  Cb, Cm, Cp, Zmin: single;
begin
  try
    Drafts := BuildHydrostaticDraftSequence(StartDraft, EndDraft, DraftStep);
  except
    on E: EArgumentException do
    begin
      MessageDlg(E.Message, mtError, [mbOK], 0);
      Exit;
    end;
  end;
  begin
    PrevCursor := Screen.Cursor;
    Screen.Cursor := crHourglass;
    Strings := nil;
    ResultsDlg := nil;
    HydObject := nil;
    try
      Strings := TStringList.Create;
      ResultsDlg := TFreeHydrostaticsResultsDialog.Create(self);
      ShowTranslatedValues(ResultsDlg);
      // Quietly test for inconsistencies
      if not FFreeShip.ProjectSettings.DisableModelCheck then
        FFreeShip.Edit.Model_Check(False);
      HydObject := TFreeHydrostaticCalc.Create(FFreeShip);
      ResultsDlg.Grid.RowCount := Length(Drafts) + 2;
      Units := FFreeship.ProjectSettings.ProjectUnits;

      ResultsDlg.Grid.Colcount := 18;
      ResultsDlg.Grid.Cells[0, 0] := rsHydroDraft;
      ResultsDlg.Grid.Cells[0, 1] := LengthStr(Units);
      ResultsDlg.Grid.Cells[1, 0] := rsHydroTrim;
      ResultsDlg.Grid.Cells[1, 1] := LengthStr(Units);
      ResultsDlg.Grid.Cells[2, 0] := 'Lwl';
      ResultsDlg.Grid.Cells[2, 1] := LengthStr(Units);
      ResultsDlg.Grid.Cells[3, 0] := 'Bwl';
      ResultsDlg.Grid.Cells[3, 1] := LengthStr(Units);
      ResultsDlg.Grid.Cells[4, 0] := rsHydroVolume;
      ResultsDlg.Grid.Cells[4, 1] := VolStr(Units);
      ResultsDlg.Grid.Cells[5, 0] := rsHydroDisplacement;
      ResultsDlg.Grid.Cells[5, 1] := WeightStr(Units);
      ResultsDlg.Grid.Cells[6, 0] := 'LCB';
      ResultsDlg.Grid.Cells[6, 1] := LengthStr(Units);
      ResultsDlg.Grid.Cells[7, 0] := 'VCB';
      ResultsDlg.Grid.Cells[7, 1] := LengthStr(Units);
      ResultsDlg.Grid.Cells[8, 0] := 'Cb';
      ResultsDlg.Grid.Cells[8, 1] := '[-]';
      ResultsDlg.Grid.Cells[9, 0] := 'Am';
      ResultsDlg.Grid.Cells[9, 1] := AreaStr(Units);
      ResultsDlg.Grid.Cells[10, 0] := 'Cm';
      ResultsDlg.Grid.Cells[10, 1] := '[-]';
      ResultsDlg.Grid.Cells[11, 0] := 'Aw';
      ResultsDlg.Grid.Cells[11, 1] := AreaStr(Units);
      ResultsDlg.Grid.Cells[12, 0] := 'Cw';
      ResultsDlg.Grid.Cells[12, 1] := '[-]';
      ResultsDlg.Grid.Cells[13, 0] := 'LCF';
      ResultsDlg.Grid.Cells[13, 1] := LengthStr(Units);
      ResultsDlg.Grid.Cells[14, 0] := 'Cp';
      ResultsDlg.Grid.Cells[14, 1] := '[-]';
      ResultsDlg.Grid.Cells[15, 0] := 'S';
      ResultsDlg.Grid.Cells[15, 1] := AreaStr(Units);
      ResultsDlg.Grid.Cells[16, 0] := 'KMt';
      ResultsDlg.Grid.Cells[16, 1] := LengthStr(Units);
      ResultsDlg.Grid.Cells[17, 0] := 'KMl';
      ResultsDlg.Grid.Cells[17, 1] := LengthStr(Units);

      for I := 0 to 16 do
        if I in [0, 1, 2, 3, 6, 7, 8, 10, 12, 14] then
          ResultsDlg.Grid.ColWidths[I] := 47
        else
        if I in [4, 5, 15] then
          ResultsDlg.Grid.ColWidths[I] := 55;
      Zmin := 0;
      I := 2;
      for DraftIndex := 0 to High(Drafts) do
      begin
        Value := Drafts[DraftIndex];
        HydObject.Clear;
        HydObject.HeelingAngle := 0.0;
        HydObject.Trim := Trim;
        HydObject.Draft := Value - Zmin;
        HydObject.Calculate;
        // Пересчет коэфФициентов полноты, если базовая линия проходит через линию киля
        if abs(HydObject.Data.ModelMin.Z) > 0.001 then
        begin
          Zmin := HydObject.Data.ModelMin.Z;
          Cb := HydObject.Data.BlockCoefficient * HydObject.Draft /
            (HydObject.Draft + HydObject.Data.ModelMin.Z);
          Cm := HydObject.Data.MidshipCoeff * HydObject.Draft /
            (HydObject.Draft + HydObject.Data.ModelMin.Z);
        end
        else
        begin
          Cb := HydObject.Data.BlockCoefficient;
          Cm := HydObject.Data.MidshipCoeff;
        end;
        if Cm > 0 then
        begin
          Cp := Cb / Cm;
          ResultsDlg.Grid.RowCount := I + 1;
          ResultsDlg.Grid.Cells[0, I] := FloatToStrF(HydObject.Draft + Zmin, ffFixed, 7, 3);
          ResultsDlg.Grid.Cells[1, I] := FloatToStrF(HydObject.Trim, ffFixed, 7, 3);
          ResultsDlg.Grid.Cells[2, I] :=
            FloatToStrF(HydObject.Data.LengthWaterline, ffFixed, 7, 3);
          ResultsDlg.Grid.Cells[3, I] :=
            FloatToStrF(HydObject.Data.BeamWaterline, ffFixed, 7, 3);
          ResultsDlg.Grid.Cells[4, I] :=
            FloatToStrF(HydObject.Data.Volume, ffFixed, 8, NumberOfDecimals(HydObject.Data.Volume));
          ResultsDlg.Grid.Cells[5, I] :=
            FloatToStrF(HydObject.Data.Displacement, ffFixed, 8, NumberOfDecimals(
            HydObject.Data.Displacement));
          ResultsDlg.Grid.Cells[6, I] :=
            FloatToStrF(HydObject.Data.CenterOfBuoyancy.X, ffFixed, 7, 3);
          ResultsDlg.Grid.Cells[7, I] :=
            FloatToStrF(HydObject.Data.CenterOfBuoyancy.Z + Zmin, ffFixed, 7, 3);
          ResultsDlg.Grid.Cells[8, I] := FloatToStrF(Cb, ffFixed, 7, 4);
          ResultsDlg.Grid.Cells[9, I] :=
            FloatToStrF(HydObject.Data.MidshipArea, ffFixed, 7, 3);
          ResultsDlg.Grid.Cells[10, I] := FloatToStrF(Cm, ffFixed, 7, 4);
          ResultsDlg.Grid.Cells[11, I] :=
            FloatToStrF(HydObject.Data.Waterplanearea, ffFixed, 7, NumberOfDecimals(
            HydObject.Data.Waterplanearea));
          ResultsDlg.Grid.Cells[12, I] :=
            FloatToStrF(HydObject.Data.WaterplaneCoeff, ffFixed, 7, 4);
          ResultsDlg.Grid.Cells[13, I] :=
            FloatToStrF(HydObject.Data.WaterplaneCOG.X, ffFixed, 7, 3);
          ResultsDlg.Grid.Cells[14, I] := FloatToStrF(Cp, ffFixed, 7, 4);
          ResultsDlg.Grid.Cells[15, I] :=
            FloatToStrF(HydObject.Data.WettedSurface, ffFixed, 7, NumberOfDecimals(
            HydObject.Data.WettedSurface));
          ResultsDlg.Grid.Cells[16, I] :=
            FloatToStrF(HydObject.Data.KMtransverse + Zmin, ffFixed, 7, 3);
          ResultsDlg.Grid.Cells[17, I] :=
            FloatToStrF(HydObject.Data.KMlongitudinal + Zmin, ffFixed, 7, NumberOfDecimals(
            HydObject.Data.KMlongitudinal));
          Inc(I);
        end;
        if (feMakingWater in HydObject.Errors) then
          break; // stop if the vessel is making water
      end;
      HydObject.AddHeader(Strings);
      Strings.add('');
      Strings.add('');
      HydObject.AddFooter(Strings, fhMultipleCalculations);
      ResultsDlg.Header.Lines.AddStrings(Strings);
      ResultsDlg.Grid.RowCount := Math.Max(3, I);
      Screen.Cursor := PrevCursor;
      ResultsDlg.Execute;
    finally
      FreeAndNil(HydObject);
      FreeAndNil(ResultsDlg);
      FreeAndNil(Strings);
      Screen.Cursor := PrevCursor;
    end;
  end;
end;{TFreeHydrostaticsForm.Button2Click}


end.
