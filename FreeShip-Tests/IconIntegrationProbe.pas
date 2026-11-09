program IconIntegrationProbe;
{$mode objfpc}{$H+}
uses Interfaces,Forms,Classes,SysUtils,Graphics,Types,Controls,ActnList,ImgList,
 LResources,FreeShip_26Icons,FreeShipUnit,FreeLinesplanFrme,FreeIntersectionDlg,FreeLayerDlg,UiNativeRender;
var Ship:TFreeShip;ImageList:TImageList;Actions:TActionList;A:TAction;Pic:TPicture;
 Keys:TStringList;I,J,Size,Count,BeforeCount:Integer;Name,Key,Rest:string;Host:TForm;
 Frame:TFreeLinesplanFrame;Section:TFreeIntersectionDialog;Layer:TFreeLayerDialog;Bitmap:TBitmap;
const Names:array[0..10]of string=('ZoomIn','ZoomOut','ZoomExtents','SaveBitmap','Print','UseLights','ShowFillColor','ShowMonochrome','MirrorPlanView','DeleteAll','DeleteSelected');
 Sizes:array[0..8]of Integer=(16,20,24,32,40,48,64,96,128);
procedure Check(OK:Boolean;const Message:string);
begin if not OK then begin WriteLn('FAIL ',Message);Halt(1) end;WriteLn('PASS ',Message) end;
procedure Snapshot(Control:TWinControl;const Name:string);
begin RenderNativeControl(Control,Bitmap);Bitmap.SaveToFile(IncludeTrailingPathDelimiter(ParamStr(1))+Name+'.bmp') end;
begin
 Application.Initialize;Pic:=TPicture.Create;Keys:=TStringList.Create;Count:=0;
 for I:=0 to LazarusResources.Count-1 do begin
  Name:=LazarusResources.Items[I].Name;if Pos('FreeShip_26/',Name)<>1 then Continue;
  Rest:=Copy(Name,Length('FreeShip_26/')+1,MaxInt);J:=LastDelimiter('/',Rest);
  Key:=Copy(Rest,1,J-1);Size:=StrToInt(Copy(Rest,J+1,MaxInt));
  Pic.LoadFromLazarusResource(Name);LoadFreeShip_26Icon(Pic.Graphic,Key,Size);
  Check((Pic.Width=Size) and (Pic.Height=Size),'embedded size '+Rest);
  if Keys.IndexOf(Key)<0 then Keys.Add(Key);Inc(Count);
 end;
 Check(Count>0,'embedded icons discovered');WriteLn('RESOURCE_COUNT=',Count,' KEYS=',Keys.Count);
 for I:=0 to Keys.Count-1 do begin LoadFreeShip_26Icon(Pic.Graphic,Keys[I],35);Check(Pic.Width=32,'nearest-size fallback '+Keys[I]) end;
 Ship:=TFreeShip.Create(nil);Bitmap:=TBitmap.Create;Host:=TForm.CreateNew(nil);Host.SetBounds(0,0,1200,800);
 for J:=0 to High(Sizes) do begin
  Ship.Preferences.ToolIconSize:=Sizes[J];ImageList:=TImageList.Create(nil);Actions:=TActionList.Create(nil);
  ImageList.Width:=Sizes[J];ImageList.Height:=Sizes[J];Bitmap.SetSize(Sizes[J],Sizes[J]);ImageList.Add(Bitmap,nil);
  for I:=0 to High(Names) do begin A:=TAction.Create(Actions);A.ActionList:=Actions;A.Name:=Names[I];A.ImageIndex:=0 end;
  Ship.Preferences.LoadImageListByActions(ImageList,Actions);
  Check(ImageList.Count=Length(Names),'shared legacy slots split '+IntToStr(Sizes[J]));
  for I:=0 to Actions.ActionCount-1 do begin
   Check(TAction(Actions.Actions[I]).ImageIndex<ImageList.Count,'valid image index');
   for Size:=I+1 to Actions.ActionCount-1 do Check(TAction(Actions.Actions[I]).ImageIndex<>TAction(Actions.Actions[Size]).ImageIndex,'different semantics use different slots');
  end;
  BeforeCount:=ImageList.Count;
  for I:=0 to 5 do Ship.Preferences.LoadImageListByActions(ImageList,Actions);
  Check(ImageList.Count=BeforeCount,'repeat opening does not grow image list');
  A:=TAction.Create(Actions);A.ActionList:=Actions;A.Name:='ScaleModel';A.ImageIndex:=-1;
  Ship.Preferences.LoadImageListByActions(ImageList,Actions);
  Check((A.ImageIndex>=0) and (A.ImageIndex<ImageList.Count),'existing action without artwork receives new icon');
  Actions.Free;ImageList.Free;
 end;
 Ship.Preferences.ToolIconSize:=24;
 Frame:=TFreeLinesplanFrame.Create(Host);Frame.Parent:=Host;Frame.SetBounds(0,0,1200,700);Frame.FreeShip:=Ship;
 Snapshot(Frame.ToolBar1,'linesplan-toolbar-24');Frame.FreeShip:=nil;Frame.Free;
 Section:=TFreeIntersectionDialog.Create(nil);Ship.Preferences.LoadImageListByActions(Section.MenuImages,Section.ActionList1);
 Snapshot(Section.ToolBar1,'sections-toolbar-24');Section.Free;
 Layer:=TFreeLayerDialog.Create(nil);
 Ship.Preferences.LoadImageIntoList(Layer.MenuImages,1,'NewLayer');
 Ship.Preferences.LoadImageIntoList(Layer.MenuImages,2,'DeleteEmptyLayers');
 Ship.Preferences.LoadImageIntoList(Layer.MenuImages,3,'MoveUp');
 Ship.Preferences.LoadImageIntoList(Layer.MenuImages,4,'MoveDown');
 Snapshot(Layer.ToolBar1,'layers-toolbar-24');Layer.Free;
 Bitmap.Free;Host.Free;Ship.Free;Pic.Free;Keys.Free;
end.
