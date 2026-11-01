unit FreePathResolution;

{$mode objfpc}{$H+}

interface

// Resolve an address-bar entry without requiring a trailing path delimiter.
function ResolveFileDialogLocation(const Location: string; out Dir, FileName: string): Boolean;

implementation

uses SysUtils, LazFileUtils;

function ResolveFileDialogLocation(const Location: string; out Dir, FileName: string): Boolean;
begin
  Dir := '';
  FileName := '';
  if DirectoryExistsUTF8(Location) then
  begin
    Dir := CleanAndExpandDirectory(Location);
    Exit(True);
  end;
  if FileExistsUTF8(Location) then
  begin
    Dir := CleanAndExpandDirectory(ExtractFilePath(Location));
    FileName := ExtractFileName(Location);
    Exit(True);
  end;
  Result := False;
end;

end.
