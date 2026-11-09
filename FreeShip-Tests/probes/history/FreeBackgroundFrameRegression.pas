program FreeBackgroundFrameRegression;
{$mode objfpc}{$H+}
uses Interfaces, Forms, SysUtils, Classes, Graphics, Controls, Types,
  FreeGeometry, FreeShipUnit;
{$R ../FreeShip.res}
type
  TProbeViewport = class(TFreeViewport)
    procedure MovePointer;
  end;
procedure TProbeViewport.MovePointer;
begin
  MouseMove([], 20, 20);
end;
procedure Check(Condition: Boolean; const Detail: string);
begin
  if not Condition then raise Exception.Create(Detail);
  WriteLn('PASS: ', Detail);
end;
var Host: TForm; View: TProbeViewport; Image: TBitmap; R: TRect;
begin
  try
  Application.Initialize;
  Host := TForm.Create(nil);
  try
    View := TProbeViewport.Create(Host);
    View.Parent := Host;
    View.SetBounds(0, 0, 320, 240);
    View.ViewType := fvBodyPlan;
    R := View.BackgroundImage.TargetRect;
    Check((R.Right = R.Left) and (R.Bottom = R.Top), 'empty image has empty rectangle');
    View.BackgroundMode := emSetFrame;
    Check(View.BackgroundMode = emNormal, 'cannot enter frame mode without image');
    View.MovePointer;
    Image := TBitmap.Create;
    try
      Image.SetSize(64, 32);
      View.BackgroundImage.AssignData(Image, fvBodyPlan, Point(0,0), 1,
        False, clWhite, 255, 100, 5, True);
    finally
      Image.Free;
    end;
    View.BackgroundMode := emSetFrame;
    Check(View.BackgroundMode = emSetFrame, 'valid image allows frame mode');
    R := View.BackgroundImage.TargetRect;
    Check((R.Right > R.Left) and (R.Bottom > R.Top), 'valid image retains rectangle');
    View.MovePointer;
    View.BackgroundImage.Clear;
    View.MovePointer;
    Check(View.BackgroundMode = emNormal, 'clearing image leaves edit mode safely');
    WriteLn('PASS: pointer remains usable after image removal');
  finally
    Host.Free;
  end;
  except
    on E: Exception do
    begin
      WriteLn('FAIL: ', E.ClassName, ': ', E.Message);
      Halt(1);
    end;
  end;
end.
