unit ProcessCallerProbe;
{$mode objfpc}{$H+}
interface
uses Main;
procedure RunProcessCallerProbe(Form: TMainForm);
implementation
uses Classes, SysUtils, Forms, Controls, Windows, fpjson, FreeProcess,
  FreePropeller_Task1Dlg, FreePropeller_Task4Dlg, FreeHydrodyn_ManeuvDlg;
procedure RunProcessCallerProbe(Form: TMainForm);
var R,O:TJSONObject; C:TJSONArray; Text:TStringList;
    First:TFreePropeller_Task1; Fourth:TFreePropeller_Task4;
    Maneuv:TFreeHydrodyn_Maneuv; Started:QWord; Seed:TStringList;
    OriginalDirectory,Key,ErrorText,Mode,Helper:string;
    BeforeCursor:TCursor; I,J:Integer; Caught:Boolean;
  procedure Check(const Suffix,Expected:string; Passed:Boolean; const Actual:string='');
  begin
    O:=TJSONObject.Create; O.Add('id',Key+'.'+Suffix); O.Add('expected',Expected);
    if Passed then O.Add('status','pass') else O.Add('status','fail');
    O.Add('actual',Actual); C.Add(O);
  end;
begin
  R:=TJSONObject.Create; C:=TJSONArray.Create; R.Add('cases',C);
  First:=nil; Fourth:=nil; Maneuv:=nil;
  try
    try
      OriginalDirectory:=GetCurrentDir;
      First:=TFreePropeller_Task1.Create(nil);
      Fourth:=TFreePropeller_Task4.Create(nil);
      First.ProbeShip:=Form.FreeShip; Fourth.ProbeShip:=Form.FreeShip;
      for J:=0 to 1 do begin
        if J=0 then begin Mode:='nonzero'; Helper:='process-fail.exe' end
        else begin Mode:='timeout'; Helper:='process-wait.exe' end;
        Windows.SetEnvironmentVariable('FS_PROCESS_CALLER_FAULT',PChar(Helper));
        for I:=0 to 2 do begin
          Key:=Format('11.caller.%d.%s',[I,Mode]); BeforeCursor:=Screen.Cursor;
          Caught:=False; ErrorText:='';
          try
            case I of 0:First.Calculate; 1:Fourth.Calculate; 2:Form.FreeShip.Edit.File_Execute_AddMass end;
          except on E:EFreePluginError do begin
            Caught:=((J=0) and (E.State=fpsExitError) and (E.ExitCode=7)) or
              ((J=1) and (E.State=fpsTimedOut)); ErrorText:=E.Message;
          end end;
          Check('error','real calculation method propagates the process failure',Caught,ErrorText);
          Check('directory','outer finally restores the original directory',GetCurrentDir=OriginalDirectory,GetCurrentDir);
          Check('cursor','outer finally restores the original cursor',Screen.Cursor=BeforeCursor);
        end;
      end;
      Windows.SetEnvironmentVariable('FS_PROCESS_CALLER_FAULT',nil);
      Maneuv:=TFreeHydrodyn_Maneuv.Create(nil);
      // This property is exposed only in the disposable source copy.
      Maneuv.ProbeShip:=Form.FreeShip;
      Form.FreeShip.Preferences.ExecDirectory:=SysUtils.GetEnvironmentVariable('FS_PROCESS_CALLER_DIRECTORY');
      Form.FreeShip.Preferences.TempDirectory:=Form.FreeShip.Preferences.ExecDirectory;
      Maneuv.CheckBox2.Checked:=False;
      Maneuv.Dat2:=111; Maneuv.Dat3:=20; Maneuv.Dat4:=2; Maneuv.Dat5:=1;
      Maneuv.Dat6:=0.6; Maneuv.Dat7:=2; Maneuv.Dat8:=49; Maneuv.Dat9:=10;
      Maneuv.Dat10:=1025.87; Maneuv.Dat11:=1.18831; Maneuv.Dat12:=0.25;
      Maneuv.Dat13:=50; Maneuv.Dat14:=2.5; Maneuv.Dat15:=0.02;
      Maneuv.Dat16:=0.001; Maneuv.Dat17:=1; Maneuv.Dat18:=1;
      Maneuv.Dat19:=1; Maneuv.Dat20:=10; Maneuv.I3:=1; Maneuv.I4:=1; Maneuv.I5:=1;
      Seed:=TStringList.Create;
      try Seed.Text:='stale result'; Seed.SaveToFile(IncludeTrailingPathDelimiter(Form.FreeShip.Preferences.TempDirectory)+'OUT.') finally Seed.Free end;
      Key:='11.caller.maneuv.zero-exit-missing-output'; BeforeCursor:=Screen.Cursor;
      Caught:=False; ErrorText:=''; Started:=GetTickCount64;
      try Maneuv.Calculate;
      except on E:EInOutError do begin
        Caught:=(Pos('OUT.',E.Message)>0) and (Pos('ManeuvPP.EXE',E.Message)>0); ErrorText:=E.Message;
      end end;
      Check('error','real Calculate rejects zero-exit helper that produces no OUT.',Caught,ErrorText);
      Check('bounded','returns without waiting for an already exited process',GetTickCount64-Started<2000,IntToStr(GetTickCount64-Started));
      Check('directory','outer finally restores the original directory',GetCurrentDir=OriginalDirectory,GetCurrentDir);
      Check('cursor','outer finally restores the original cursor',Screen.Cursor=BeforeCursor);
      Check('freshness','isolated export cannot consume or delete an unrelated shared OUT.',FileExists(IncludeTrailingPathDelimiter(Form.FreeShip.Preferences.TempDirectory)+'OUT.'));
    except on E:Exception do R.Add('error',E.ClassName+': '+E.Message) end;
    Text:=TStringList.Create;
    try Text.Text:=R.FormatJSON; Text.SaveToFile(SysUtils.GetEnvironmentVariable('FS_PROCESS_CALLER_REPORT')) finally Text.Free end;
  finally
    Windows.SetEnvironmentVariable('FS_PROCESS_CALLER_FAULT',nil);
    First.Free; Fourth.Free; Maneuv.Free; R.Free;
  end;
end;
end.
