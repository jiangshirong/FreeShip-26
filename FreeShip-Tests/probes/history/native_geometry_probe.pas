program NativeGeometryProbe;
{$mode objfpc}{$H+}
{$APPTYPE CONSOLE}
{$R ../FreeShip.res}
uses Interfaces,Forms,Controls,Windows,SysUtils;
var F:TForm;R:TRect;
procedure ShowState(const S:string);
begin
 Windows.GetWindowRect(F.Handle,R);
 WriteLn(S,' bounds=',F.Width,',',F.Height,' native_outer=',R.Right-R.Left,',',R.Bottom-R.Top);
 Windows.GetClientRect(F.Handle,R);
 WriteLn('cached=',F.CachedClientWidth,',',F.CachedClientHeight,' native_client=',R.Right-R.Left,',',R.Bottom-R.Top);
end;
begin
 Application.Initialize;F:=TForm.Create(nil);F.Position:=poDesigned;F.SetBounds(0,0,800,600);F.HandleNeeded;
 ShowState('initial');F.InvalidateClientRectCache(False);R:=F.ClientRect;ShowState('refreshed');F.Free;
end.
