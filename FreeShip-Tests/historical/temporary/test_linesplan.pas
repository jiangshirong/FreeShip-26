program test_linesplan;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces, Forms, SysUtils, Classes, Graphics, Types, FreeTypes,
  FreeGeometry, FreeShipUnit, FreeLinesplanFrme, Main, LCLTranslator, FreeStringsUnit;
type
  TRecorded = record r: TRect; text: string end;
  TCheckedViewport = class(TFreeViewport)
    procedure TextOut(x,y:integer; val:string); override;
  end;
var entries:array of TRecorded;
procedure TCheckedViewport.TextOut(x,y:integer; val:string);
var n:integer;
begin
 n:=Length(entries); SetLength(entries,n+1);
 entries[n].r:=Rect(x,y,x+TextWidth(val),y+TextHeight(val)); entries[n].text:=val;
 inherited TextOut(x,y,val);
end;
var frame:TFreeLinesplanFrame; vp:TCheckedViewport; ship:TFreeShip; bm:TBitmap;
  mn,mx:T3DCoordinate; i,j,n,intersections,w,h,mode,language:integer; lang:string; r1,r2:TRect;
begin
 Application.Initialize; Application.CreateForm(TMainForm, MainForm); ship:=MainForm.FreeShip;
 ship.Edit.File_Load(GetTempDir+'FreeShipPathCheck\current-validation\验证-Женя\输入-Женя.fbm');
 frame:=TFreeLinesplanFrame.Create(nil); bm:=TBitmap.Create;
 try
  frame.FreeShip:=ship; frame.FontSize:=9; frame.Viewport.Free;
  vp:=TCheckedViewport.Create(frame); frame.Viewport:=vp;
  vp.ViewType:=fvPlan; vp.Angle:=90; vp.Elevation:=90; vp.Margin:=1; vp.ViewportMode:=vmWireframe;
  for language:=0 to 2 do for mode:=0 to 2 do begin
   case language of 0:begin lang:='cn';ship.Preferences.Language:='Chinese' end;
     1:begin lang:='ru';ship.Preferences.Language:='Russian' end;
     2:begin lang:='en';ship.Preferences.Language:='English' end end;
   SetDefaultLang(lang,GetTempDir+'FreeShipPathCheck\current-validation\locale','FreeShip',True); SetUserStringsLang; WriteLn('translation ',lang,' baseline=',rs_Base,' dwl=',rs_DWL);
   case mode of 0:begin w:=800;h:=500 end; 1:begin w:=1280;h:=720 end; 2:begin w:=1600;h:=900 end end;
   vp.SetBounds(0,0,w,h); vp.DestinationWidth:=w; vp.DestinationHeight:=h; bm.SetSize(w,h); vp.DrawingCanvas:=bm.Canvas;
   bm.Canvas.Brush.Color:=clWhite; bm.Canvas.FillRect(0,0,w,h);
   frame.ViewportRequestExtents(vp,mn,mx); vp.InitializeViewport(mn,mx);
   SetLength(entries,0); frame.ViewportRedraw(vp); intersections:=0;
   for i:=0 to High(entries) do for j:=i+1 to High(entries) do begin
    r1:=entries[i].r;r2:=entries[j].r;
    if (r1.Left<r2.Right) and (r1.Right>r2.Left) and (r1.Top<r2.Bottom) and (r1.Bottom>r2.Top) then begin Inc(intersections); WriteLn('collision: ',entries[i].text,' / ',entries[j].text) end;
   end;
   WriteLn('linesplan ',lang,' ',w,'x',h,' labels=',Length(entries),' overlaps=',intersections);
   if Length(entries)<4 then begin WriteLn('FAIL too few labels'); Halt(1) end;
   if intersections>0 then begin WriteLn('FAIL overlapping labels'); Halt(2) end;
   bm.SaveToFile(GetTempDir+'FreeShipLabelIntegration\linesplan-'+lang+'-'+IntToStr(w)+'.bmp');
  end;
 finally frame.FreeShip:=nil;frame.Free;bm.Free;MainForm.Free end;
end.
