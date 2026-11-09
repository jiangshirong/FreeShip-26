program cached_drag_test;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,ExtCtrls,Graphics,FreeGeometry;
type TV=class(TFreeViewport) public procedure PaintFrame;end;
 TProbe=class Count:Integer;procedure Render(Sender:TObject);end;
procedure TV.PaintFrame;begin Paint end;
procedure TProbe.Render(Sender:TObject);begin Inc(Count);TFreeViewport(Sender).DrawingCanvas.Rectangle(5,5,80,60);end;
var f:TForm;v:TV;p:TProbe;i:integer;t:QWord;
begin
 Application.Initialize;f:=TForm.CreateNew(nil);v:=TV.Create(f);v.Parent:=f;v.SetBounds(0,0,400,300);v.DoubleBuffer:=True;p:=TProbe.Create;v.OnRedraw:=@p.Render;
 v.HandleNeeded;v.PaintFrame;if p.Count<>1 then Halt(1);
 v.PresentationFrozen:=True;t:=GetTickCount64;
 for i:=1 to 100 do begin v.SetBounds(i,i,400,300);v.PaintFrame end;
 if p.Count<>1 then Halt(2);
 v.SetBounds(1,1,500,320);v.PaintFrame;if p.Count<>1 then Halt(3);
 WriteLn('PASS 101 cached drag/resize paints; geometry redraw count remains 1; elapsed_ms=',GetTickCount64-t);
 v.PresentationFrozen:=False;v.PaintFrame;if p.Count<>2 then Halt(4);
 WriteLn('PASS release redraws geometry once');f.Free;p.Free;
end.
