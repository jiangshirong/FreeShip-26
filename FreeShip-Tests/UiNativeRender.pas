unit UiNativeRender;
{$mode objfpc}{$H+}
interface
uses Controls,Graphics;
procedure RenderNativeControl(Control:TWinControl;Target:Graphics.TBitmap);
implementation
uses Windows,Forms,ExtCtrls,LMessages;
procedure RenderNativeControl(Control:TWinControl;Target:Graphics.TBitmap);
 procedure PaintTree(C:TWinControl;Surface:Graphics.TBitmap);
 var Saved,I:Integer;Child:TControl;Part:Graphics.TBitmap;
 begin
  C.HandleNeeded;Surface.SetSize(C.ClientWidth,C.ClientHeight);
  Surface.Canvas.Brush.Color:=clWhite;Surface.Canvas.FillRect(0,0,Surface.Width,Surface.Height);
  Saved:=Windows.SaveDC(Surface.Canvas.Handle);
  try Windows.SendMessage(C.Handle,WM_PRINTCLIENT,WPARAM(Surface.Canvas.Handle),PRF_CLIENT)
  finally Windows.RestoreDC(Surface.Canvas.Handle,Saved) end;
  if C is TScrollingWinControl then begin
   if C is TScrollBox then Surface.Canvas.Brush.Color:=TScrollBox(C).Color
   else if C is TForm then Surface.Canvas.Brush.Color:=TForm(C).Color;
   Surface.Canvas.FillRect(0,0,Surface.Width,Surface.Height);
  end;
  if C is TPanel then Surface.Canvas.Brush.Color:=TPanel(C).Color;
  for I:=0 to C.ControlCount-1 do begin
   Child:=C.Controls[I];if not Child.Visible then Continue;
   Part:=Graphics.TBitmap.Create;
   try
    if Child is TWinControl then PaintTree(TWinControl(Child),Part)
    else if Child is TGraphicControl then begin
     Part.SetSize(Child.Width,Child.Height);
     Part.Canvas.Brush.Color:=Surface.Canvas.Brush.Color;
     Part.Canvas.FillRect(0,0,Part.Width,Part.Height);
     Saved:=Windows.SaveDC(Part.Canvas.Handle);
     try Child.Perform(LM_PAINT,WPARAM(Part.Canvas.Handle),0)
     finally Windows.RestoreDC(Part.Canvas.Handle,Saved) end;
    end;
    if (Part.Width>0) and (Part.Height>0) then Surface.Canvas.Draw(Child.Left,Child.Top,Part);
   finally Part.Free end;
  end;
 end;
begin PaintTree(Control,Target) end;
end.
