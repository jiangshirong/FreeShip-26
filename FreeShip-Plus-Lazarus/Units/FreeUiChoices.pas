unit FreeUiChoices;

{$mode objfpc}{$H+}

interface

uses Classes, StdCtrls, ExtCtrls;

// Translate labels only for lists whose application logic uses ItemIndex.
procedure SetChoiceLabels(Combo: TComboBox; const Labels: array of string);
procedure SetRadioChoiceLabels(Group: TRadioGroup; const Labels: array of string);
procedure SetResistanceShipTypeChoices(Combo: TComboBox);
// Some controls also contribute to generated model names; keep those stable.
function OriginalChoiceLabel(Combo: TComboBox; const Labels: array of string): string;
procedure SetLanguageChoices(Combo: TComboBox; DisplayMap: TStrings;
  const CurrentLanguage: string);
function LanguageChoiceKey(DisplayMap: TStrings; const DisplayText: string): string;

implementation

uses SysUtils, FreeStringsUnit;

procedure SetChoiceLabels(Combo: TComboBox; const Labels: array of string);
var I, Selected: Integer;
    PreviousText: string;
    PreviousChange, PreviousSelect, PreviousClick: TNotifyEvent;
begin
  if (Combo = nil) or Combo.Sorted or (Combo.Items.Count <> Length(Labels)) then Exit;
  Selected := Combo.ItemIndex;
  PreviousText := Combo.Text;
  PreviousChange := Combo.OnChange;
  PreviousSelect := Combo.OnSelect;
  PreviousClick := Combo.OnClick;
  Combo.OnChange := nil;
  Combo.OnSelect := nil;
  Combo.OnClick := nil;
  Combo.Items.BeginUpdate;
  try
    for I := 0 to High(Labels) do Combo.Items[I] := Labels[I];
  finally
    Combo.Items.EndUpdate;
    if Selected >= 0 then Combo.ItemIndex := Selected
    else Combo.Text := PreviousText;
    Combo.OnChange := PreviousChange;
    Combo.OnSelect := PreviousSelect;
    Combo.OnClick := PreviousClick;
  end;
end;

procedure SetRadioChoiceLabels(Group: TRadioGroup; const Labels: array of string);
var I, Selected: Integer;
    PreviousClick: TNotifyEvent;
begin
  if (Group = nil) or (Group.Items.Count <> Length(Labels)) then Exit;
  Selected := Group.ItemIndex;
  PreviousClick := Group.OnClick;
  Group.OnClick := nil;
  Group.Items.BeginUpdate;
  try
    for I := 0 to High(Labels) do Group.Items[I] := Labels[I];
  finally
    Group.Items.EndUpdate;
    Group.ItemIndex := Selected;
    Group.OnClick := PreviousClick;
  end;
end;

procedure SetResistanceShipTypeChoices(Combo: TComboBox);
begin
  SetChoiceLabels(Combo, [rsChoicePassengerFerry, rsChoiceCargoMidFull,
    rsChoiceCargoMidBallast, rsChoiceCargoAftFull, rsChoiceCargoAftBallast,
    rsChoiceTankerMidFull, rsChoiceTankerMidBallast, rsChoiceTankerAftFull,
    rsChoiceTankerAftBallast, rsChoiceTrawler, rsChoiceTug]);
end;

function OriginalChoiceLabel(Combo: TComboBox; const Labels: array of string): string;
begin
  Result := '';
  if Combo = nil then Exit;
  if (Combo.ItemIndex >= 0) and (Combo.ItemIndex <= High(Labels)) then
    Result := Labels[Combo.ItemIndex]
  else
    Result := Combo.Text;
end;

procedure SetLanguageChoices(Combo: TComboBox; DisplayMap: TStrings;
  const CurrentLanguage: string);
var
  Names: TStringList;
  I, Selected: Integer;
  DisplayName, CurrentKey: string;
  PreviousChange, PreviousSelect: TNotifyEvent;
begin
  if (Combo = nil) or (DisplayMap = nil) then Exit;
  Names := TStringList.Create;
  try
    Names.Assign(Combo.Items);
    CurrentKey := LanguageName(LanguageCode(CurrentLanguage));
    if CurrentKey = '' then CurrentKey := CurrentLanguage;
    PreviousChange := Combo.OnChange;
    PreviousSelect := Combo.OnSelect;
    Combo.OnChange := nil;
    Combo.OnSelect := nil;
    Combo.Items.BeginUpdate;
    try
      DisplayMap.Clear;
      Combo.Items.Clear;
      for I := 0 to Names.Count - 1 do
      begin
        if not IsVisibleInterfaceLanguage(Names[I]) then Continue;
        DisplayName := LanguageDisplayName(Names[I]);
        if DisplayMap.IndexOfName(DisplayName) >= 0 then
          DisplayName := DisplayName + ' (' + Names[I] + ')';
        DisplayMap.Values[DisplayName] := Names[I];
        Combo.Items.Add(DisplayName);
      end;
      Selected := -1;
      for I := 0 to Combo.Items.Count - 1 do
        if SameText(DisplayMap.Values[Combo.Items[I]], CurrentKey) then
        begin
          Selected := I;
          Break;
        end;
      if Selected < 0 then
        for I := 0 to Combo.Items.Count - 1 do
          if SameText(DisplayMap.Values[Combo.Items[I]], 'English') then
          begin
            Selected := I;
            Break;
          end;
      Combo.ItemIndex := Selected;
    finally
      Combo.Items.EndUpdate;
      Combo.OnChange := PreviousChange;
      Combo.OnSelect := PreviousSelect;
    end;
  finally
    Names.Free;
  end;
end;

function LanguageChoiceKey(DisplayMap: TStrings; const DisplayText: string): string;
begin
  Result := '';
  if DisplayMap <> nil then Result := DisplayMap.Values[DisplayText];
  if Result = '' then Result := DisplayText;
end;

end.
