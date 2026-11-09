program HistoricalProgressProbe;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Main,FreeShipUnit;
var Ship:TFreeShip; I,W,H:Integer;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);
 try
  Ship:=MainForm.FreeShip;
  MainForm.StatusBar.Visible:=False;
  W:=MainForm.MainClientPanel.Width;H:=MainForm.MainClientPanel.Height;
  Ship.Edit.ProgressBar:=MainForm.ProgressBarMain;
  try
   for I:=1 to 100 do begin
    Ship.Edit.OnFaceRebuilt(Ship,I,100);
    if MainForm.StatusBar.Visible or (MainForm.MainClientPanel.Width<>W) or
       (MainForm.MainClientPanel.Height<>H) then
      raise Exception.Create('Progress callback changed hidden progress visibility or client bounds');
   end;
  finally Ship.Edit.ProgressBar:=nil end;
  WriteLn('PASS hidden progress callback preserves client bounds');
 finally MainForm.Free end;
end.
