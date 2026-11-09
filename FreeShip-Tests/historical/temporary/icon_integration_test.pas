program icon_integration_test;
{$mode objfpc}{$H+}{$codepage utf8}
uses Interfaces,Forms,SysUtils,Graphics,Types,ImgList,Buttons,Controls,FreeShipUnit,Main,FreeAboutDlg,RongShipIcons;
const Keys:array[0..14]of string=('Menu','Edge','Face','Point','ControlNet','Layers','Sections','LinesPlan','Calculate','Info','More','Minimize','Maximize','Restore','Legal');
Sizes:array[0..3]of Integer=(20,25,30,40);
var Png:TPortableNetworkGraphic;B,G:TBitmap;I,J,N:Integer;Images:TImageList;About:TFreeAboutDlg;
procedure Check(Ok:Boolean;const Msg:string);begin if not Ok then begin WriteLn('FAIL ',Msg);Halt(1) end;WriteLn('PASS ',Msg) end;
begin
 Application.Initialize;Application.CreateForm(TMainForm,MainForm);MainForm.FormShow(MainForm);
 Png:=TPortableNetworkGraphic.Create;B:=TBitmap.Create;G:=TBitmap.Create;B.SetSize(900,440);B.Canvas.Brush.Color:=RGBToColor(250,252,250);B.Canvas.FillRect(0,0,900,440);B.Canvas.Font.Name:='Segoe UI';B.Canvas.Font.Size:=9;
 for J:=0 to 3 do for I:=0 to 14 do begin LoadRongShipIcon(Png,Keys[I],Sizes[J]);Check((Png.Width=Sizes[J]) and (Png.Height=Sizes[J]),Keys[I]+' '+IntToStr(Sizes[J]));B.Canvas.Draw(I*58+10,J*90+10,Png);B.Canvas.TextOut(I*58+2,J*90+57,Keys[I]);end;
 for I:=11 to 13 do for N in [16,22,32] do begin LoadRongShipIcon(G,Keys[I],N);Check((G.Width=N) and (G.Height=N),'window glyph '+Keys[I]+' '+IntToStr(N)) end;
 LoadRongShipIcon(Png,'Info',35);Check((Png.Width>0),'nonstandard DPI fallback');
 Images:=TImageList(MainForm.FindComponent('StudioToolbarImages'));Check(Assigned(Images),'actual toolbar uses icon list');WriteLn('toolbar loaded icon count=',Images.Count);Check(Images.Count>=11,'all eleven new toolbar icons available');
 for N in Sizes do begin Images.Resolution[N].GetBitmap(0,G);Check((G.Width=N) and (G.Height=N),'actual toolbar resolution '+IntToStr(N)) end;
 About:=TFreeAboutDlg.Create(nil);J:=0;for I:=0 to About.ComponentCount-1 do if About.Components[I] is TBitBtn then begin Inc(J);Check(TBitBtn(About.Components[I]).Glyph.Width>0,'about footer loaded glyph');end;Check(J=2,'about retains exactly two footer controls');About.Free;
 B.SaveToFile(GetTempDir+'FreeShipLabelIntegration\icons-integrated.bmp');Png.Free;B.Free;G.Free;MainForm.Free;WriteLn('PASS integration completed');
end.
