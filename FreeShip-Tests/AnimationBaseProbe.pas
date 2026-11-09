program AnimationBaseProbe;
{$mode objfpc}{$H+}
uses Interfaces,Forms,SysUtils,Graphics,Types,FreeGeometry;
var V:TFreeViewport;B:TBitmap;
procedure Check(OK:Boolean;const M:string);
begin if not OK then begin WriteLn('FAIL ',M);Halt(1) end;WriteLn('PASS ',M) end;
procedure Clear;
begin B.Canvas.Brush.Color:=clWhite;B.Canvas.FillRect(0,0,400,220) end;
begin
 Application.Initialize;V:=TFreeViewport.Create(nil);B:=TBitmap.Create;
 B.SetSize(400,220);V.SetBounds(0,0,400,220);V.DrawingCanvas:=B.Canvas;
 V.FinishProgressiveDisplay(False);V.BeginOverlayFrame;
 Check(not V.RestoreAnimationBase,'first frame prepares base');
 Clear;B.Canvas.Pen.Color:=clBlack;B.Canvas.Line(20,30,200,30);
 V.PenColor:=clRed;V.Line(20,100,200,100);V.SaveAnimationBase;V.EndOverlayFrame;
 Clear;V.BeginOverlayFrame;
 Check(V.RestoreAnimationBase,'next frame reuses base');
 Check(ColorToRGB(B.Canvas.Pixels[100,30])=ColorToRGB(clBlack),'static geometry restored');
 Check(ColorToRGB(B.Canvas.Pixels[100,100])=ColorToRGB(clWhite),'animated lines not baked into base');
 V.EndOverlayFrame;
 V.ZoomIn;V.BeginOverlayFrame;Check(not V.RestoreAnimationBase,'zoom invalidates cache key');
 V.SaveAnimationBase;V.InvalidateAnimationBase;Check(not V.RestoreAnimationBase,'explicit model invalidation discards base');
 V.SaveAnimationBase;V.SetBounds(0,0,420,220);V.BeginOverlayFrame;Check(not V.RestoreAnimationBase,'resize invalidates base');
 V.SaveAnimationBase;V.FinishProgressiveDisplay(True);V.BeginOverlayFrame;
 Check(not V.RestoreAnimationBase,'steady view bypasses animation cache');
 V.Free;B.Free;
end.
