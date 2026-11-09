program HistoricalIconsProbe;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Classes,SysUtils,Graphics,LResources,FreeShip_26Icons;
var I,J,N,P,Count,Best:Integer; Name,Key,S:string; Pic:TPicture; Keys:TStringList;
begin
 Application.Initialize;Pic:=TPicture.Create;Keys:=TStringList.Create;Count:=0;
 try
  for I:=0 to LazarusResources.Count-1 do begin
   Name:=LazarusResources.Items[I].Name;
   if Pos('FreeShip_26/',Name)<>1 then Continue;
   { Strip the prefix by its actual length; resource keys are discovered. }
   S:=Copy(Name,Length('FreeShip_26/')+1,Length(Name));P:=LastDelimiter('/',S);
   Key:=Copy(S,1,P-1);N:=StrToInt(Copy(S,P+1,Length(S)));
   Pic.LoadFromLazarusResource(Name);
   LoadFreeShip_26Icon(Pic.Graphic,Key,N);
   if (Pic.Width<>N) or (Pic.Height<>N) then raise Exception.Create('Wrong icon size: '+Name);
   if Keys.IndexOf(Key)<0 then Keys.Add(Key);
   Inc(Count);
  end;
  if Count=0 then raise Exception.Create('No icon resources discovered');
  for I:=0 to Keys.Count-1 do begin
   Key:=Keys[I];Best:=MaxInt;
   for J:=0 to LazarusResources.Count-1 do begin
    Name:=LazarusResources.Items[J].Name;
    if Pos('FreeShip_26/'+Key+'/',Name)<>1 then Continue;
    N:=StrToInt(Copy(Name,LastDelimiter('/',Name)+1,Length(Name)));
    if Abs(N-35)<Best then Best:=Abs(N-35);
   end;
   LoadFreeShip_26Icon(Pic.Graphic,Key,35);
   if (Abs(Pic.Width-35)<>Best) or (Pic.Width<>Pic.Height) then
     raise Exception.Create('Incorrect nearest-size fallback: '+Key);
  end;
  WriteLn('PASS resources=',Count,' fallback_keys=',Keys.Count);
 finally Keys.Free;Pic.Free end;
end.
