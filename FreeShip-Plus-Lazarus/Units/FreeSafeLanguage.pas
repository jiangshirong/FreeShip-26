unit FreeSafeLanguage;

{$mode objfpc}{$H+}

interface

// Change translated UI text without overwriting values being edited by the user.
function ApplyLanguageSafely(const Code, Directory: string): Boolean;

implementation

uses Classes, SysUtils, TypInfo, StdCtrls, Forms, LResources, LCLTranslator, MDIPanel;

type
  TLiveTranslator = class(TUpdateTranslator)
  private
    FDelegate: TAbstractTranslator;
  public
    constructor Create(Delegate: TAbstractTranslator);
    procedure TranslateStringProperty(Sender: TObject; const Instance: TPersistent;
      PropInfo: PPropInfo; var Content: string); override;
  end;

constructor TLiveTranslator.Create(Delegate: TAbstractTranslator);
begin
  inherited Create;
  FDelegate := Delegate;
end;

procedure TLiveTranslator.TranslateStringProperty(Sender: TObject;
  const Instance: TPersistent; PropInfo: PPropInfo; var Content: string);
begin
  // LCL stores Text as a translatable string; old catalogues contain translations
  // for edit placeholders, which must never replace the user's live input.
  if (PropInfo <> nil) and SameText(PropInfo^.Name, 'Text') and
    ((Instance is TCustomEdit) or (Instance is TCustomComboBox)) then Exit;
  FDelegate.TranslateStringProperty(Sender, Instance, PropInfo, Content);
end;

function ApplyLanguageSafely(const Code, Directory: string): Boolean;
var Delegate: TAbstractTranslator; Live: TLiveTranslator; I: Integer;
  procedure UpdateOwnedPanels(Owner: TComponent);
  var J: Integer; Child: TComponent; SavedCaption: string;
  begin
    for J := 0 to Owner.ComponentCount - 1 do
    begin
      Child := Owner.Components[J];
      // Lazarus only treats forms/frames as resource roots. Our embedded MDI
      // panels own their own actions and menu items, which otherwise remain in
      // the previous language. Reset the identifier path to the panel's class.
      if Child is TCustomMDIPanel then
      begin
        SavedCaption := TCustomMDIPanel(Child).Caption;
        Live.UpdateTranslation(Child);
        // This caption describes a live view; its caller refreshes it separately.
        TCustomMDIPanel(Child).Caption := SavedCaption;
      end;
      UpdateOwnedPanels(Child);
    end;
  end;
begin
  Result := False;
  if SetDefaultLang(Code, Directory, 'FreeShip', False) = '' then Exit;
  Delegate := LRSTranslator;
  if Delegate = nil then Exit;
  Live := TLiveTranslator.Create(Delegate);
  LRSTranslator := Live;
  try
    for I := 0 to Screen.CustomFormCount - 1 do
    begin
      Live.UpdateTranslation(Screen.CustomForms[I]);
      UpdateOwnedPanels(Screen.CustomForms[I]);
    end;
    for I := 0 to Screen.DataModuleCount - 1 do
    begin
      Live.UpdateTranslation(Screen.DataModules[I]);
      UpdateOwnedPanels(Screen.DataModules[I]);
    end;
  finally
    LRSTranslator := Delegate;
    Live.Free;
  end;
  Result := True;
end;
end.
