program EmfDeviceProbe;
{$mode objfpc}{$H+}{$APPTYPE CONSOLE}
{$R K:\FreeShip.res}
uses Interfaces,Forms,Graphics,Windows,SysUtils;
var B:Graphics.TBitmap;D:HDC;M:HENHMETAFILE;R:TRect;Header:ENHMETAHEADER;
begin
 Application.Initialize;B:=Graphics.TBitmap.Create;B.SetSize(64,64);
 D:=B.Canvas.Handle;
 WriteLn('dpi ',GetDeviceCaps(D,LOGPIXELSX),',',GetDeviceCaps(D,LOGPIXELSY),
 ' pixels ',GetDeviceCaps(D,HORZRES),',',GetDeviceCaps(D,VERTRES),
 ' mm ',GetDeviceCaps(D,HORZSIZE),',',GetDeviceCaps(D,VERTSIZE));
 R.Left:=0;R.Top:=0;R.Right:=Round(1000000*2540.0/GetDeviceCaps(D,LOGPIXELSX));R.Bottom:=R.Right;
 D:=CreateEnhMetaFileW(D,nil,@R,nil);M:=CloseEnhMetaFile(D);
 GetEnhMetaFileHeader(M,SizeOf(Header),@Header);
 WriteLn('emf px ',Header.szlDevice.cx,',',Header.szlDevice.cy,' mm ',Header.szlMillimeters.cx,',',Header.szlMillimeters.cy,' frame ',Header.rclFrame.Right,',',Header.rclFrame.Bottom);
 DeleteEnhMetaFile(M);B.Free;
end.
