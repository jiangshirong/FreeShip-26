{#############################################################################################}
{    This code is distributed as part of the FREE!ship project. FREE!ship is an               }
{    open source surface-modelling program based on subdivision surfaces and intended for     }
{    designing ships.                                                                         }
{                                                                                             }
{    Copyright © 2005, by Martijn van Engeland                                                }
{    e-mail                  : Info@FREEship.org                                              }
{    FREE!ship project page  : https://sourceforge.net/projects/freeship                      }
{    FREE!ship homepage      : www.FREEship.org                                               }
{                                                                                             }
{    This program is free software; you can redistribute it and/or modify it under            }
{    the terms of the GNU General Public License as published by the                          }
{    Free Software Foundation; either version 2 of the License, or (at your option)           }
{    any later version.                                                                       }
{                                                                                             }
{    This program is distributed in the hope that it will be useful, but WITHOUT ANY          }
{    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A          }
{    PARTICULAR PURPOSE. See the GNU General Public License for more details.                 }
{                                                                                             }
{    You should have received a copy of the GNU General Public License along with             }
{    this program; if not, write to the Free Software Foundation, Inc.,                       }
{    59 Temple Place, Suite 330, Boston, MA 02111-1307 USA                                    }
{                                                                                             }
{#############################################################################################}
unit FreeLinesplanFrm;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

interface

uses
{$IFnDEF FPC}
  Windows,
{$ELSE}
  LCLIntf, LCLType, LMessages,

{$ENDIF}
  //Messages,
   SysUtils,
   Variants,
   Classes,
   Graphics,
   Controls,
   Forms,
   Dialogs, ComCtrls,
   LCLTranslator,
   FreeGeometry, FreeShipUnit,
   FreeLinesplanFrme;

type

{ TFreeLinesplanForm }

 TFreeLinesplanForm  = class(TForm)
    LinesplanFrame: TFreeLinesplanFrame;
    Viewport: TFreeViewport;
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
    procedure LinesplanFrameClick(Sender: TObject);
    procedure SpinEdit1Change(Sender: TObject);
 protected
    procedure CreateWnd;override;
    procedure DestroyWnd;override;
    procedure WndProc(var Message:TLMessage);override;
    procedure EraseBackground(DC:HDC);override;
    procedure Notification(AComponent:TComponent; Operation:TOperation);override;
 private   { Private declarations }
    FPrepareNativeStateChange:Boolean;
    FCommitNativeStateChange:Boolean;
    FOpeningShip:TFreeShip;
    FReadyCaption:string;
    procedure FinishOpening(Data:PtrInt);
 public    { Public declarations }
    constructor Create(Owner: TComponent); override;
    destructor Destroy;override;
    procedure OpenForShip(Ship:TFreeShip);
    function DisplaysShip(Ship:TFreeShip):Boolean;
    procedure PrepareForDisplay;
 end;

var FreeLinesplanForm: TFreeLinesplanForm;

implementation

{$IFnDEF FPC}
  {$R *.dfm}
{$ELSE}
  {$R *.lfm}
{$ENDIF}

{
inherited Viewport: TFreeViewport
  Width = 820
  Height = 570
end
}

type TLinesplanViewportPainter=class(TFreeViewport);

{$IFDEF MSWINDOWS}
function LinesplanSetSubclass(W:HWND;Proc:Pointer;ID,Data:PtrUInt):LongBool;stdcall;external 'comctl32.dll' name 'SetWindowSubclass';
function LinesplanRemoveSubclass(W:HWND;Proc:Pointer;ID:PtrUInt):LongBool;stdcall;external 'comctl32.dll' name 'RemoveWindowSubclass';
function LinesplanDefaultSubclass(W:HWND;M:UINT;WP:WPARAM;LP:LPARAM):LRESULT;stdcall;external 'comctl32.dll' name 'DefSubclassProc';
function LinesplanDwmFlush:LongInt;stdcall;external 'dwmapi.dll' name 'DwmFlush';
function LinesplanSetClassBackground(W:HWND;Index:LongInt;Value:PtrInt):PtrInt;stdcall;
  external 'user32.dll' name {$IFDEF CPU64}'SetClassLongPtrW'{$ELSE}'SetClassLongW'{$ENDIF};
function LinesplanGetClassBackground(W:HWND;Index:LongInt):PtrUInt;stdcall;
  external 'user32.dll' name {$IFDEF CPU64}'GetClassLongPtrW'{$ELSE}'GetClassLongW'{$ENDIF};
function LinesplanNativeSubclass(W:HWND;M:UINT;WP:WPARAM;LP:LPARAM;ID,Data:PtrUInt):LRESULT;stdcall;
var Host:TFreeLinesplanForm;SavedBrush:PtrUInt;ResizeCommand:Boolean;
begin
  ResizeCommand:=(M=$0112) and (((WP and $FFF0)=$F030) or ((WP and $FFF0)=$F120));
  SavedBrush:=0;
  if ResizeCommand then begin
    // LCL's shared native class has no background brush. During the native
    // maximize/restore transition DWM can expose its black, unpainted growth
    // area even when every child uses double buffering. Provide the chart's
    // white background for this synchronous native operation only. Restore
    // the shared class immediately, including exceptions and nested commands.
    SavedBrush:=LinesplanGetClassBackground(W,-10); // GCLP_HBRBACKGROUND
    LinesplanSetClassBackground(W,-10,PtrInt(LCLIntf.GetStockObject(0))); // WHITE_BRUSH
  end;
  try
    Result:=LinesplanDefaultSubclass(W,M,WP,LP);
  finally
    if ResizeCommand then LinesplanSetClassBackground(W,-10,PtrInt(SavedBrush));
  end;
  Host:=TFreeLinesplanForm(Data);
  if (M=$0047) and (LP<>0) and (Host<>nil) and Host.FCommitNativeStateChange and
    ((LMessages.PWindowPos(LP)^.flags and $0001)=0) and
    LCLIntf.IsWindowVisible(W) and not LCLIntf.IsIconic(W) then begin
    // DefSubclassProc has now committed the native control tree. Present the
    // complete target-size tree before DWM takes its animation surface, rather
    // than leaving individual child WM_PAINT messages in the queue.
    Host.FCommitNativeStateChange:=False;
    LCLIntf.RedrawWindow(W,nil,0,RDW_INVALIDATE or RDW_ERASE or RDW_ALLCHILDREN or RDW_UPDATENOW);
    LinesplanDwmFlush;
    if Host.LinesplanFrame<>nil then Host.LinesplanFrame.TracePresentation('new-surface.committed');
  end;
  if (M=$0083) and (WP<>0) then begin
    // Retaining the old upper-left client rectangle during a maximize leaves
    // stale child-window pixels in DWM's animation surface. The ready frame
    // covers the complete new client; no old client bits need preservation.
    Result:=$0300; // WVR_HREDRAW | WVR_VREDRAW, no WVR_VALIDRECTS
  end;
end;
{$ENDIF}

procedure TFreeLinesplanForm.CreateWnd;
begin
  inherited CreateWnd;
  {$IFDEF MSWINDOWS}
  if not LinesplanSetSubclass(Handle,@LinesplanNativeSubclass,1,PtrUInt(Self)) then
    raise Exception.Create('Unable to install linesplan resize handler');
  {$ENDIF}
end;

procedure TFreeLinesplanForm.DestroyWnd;
begin
  {$IFDEF MSWINDOWS}
  if HandleAllocated then LinesplanRemoveSubclass(Handle,@LinesplanNativeSubclass,1);
  {$ENDIF}
  inherited DestroyWnd;
end;

procedure TFreeLinesplanForm.WndProc(var Message:TLMessage);
var StateCommand:Boolean;P:LMessages.PWindowPos;WR,CR:TRect;W,H:Integer;
begin
  if (LinesplanFrame<>nil) and not (csDestroying in ComponentState) then
    LinesplanFrame.TraceNativeMessage('form','before',Message);
  StateCommand:=(Message.Msg=LM_SYSCOMMAND) and
    (((Message.WParam and $FFF0)=$F030) or ((Message.WParam and $FFF0)=$F120));
  {$IFDEF MSWINDOWS}
  if StateCommand and (WindowState<>wsMinimized) and HandleAllocated and
    (LinesplanFrame<>nil) and (LinesplanFrame.FreeShip<>nil) then begin
    // DWM snapshots the old window before SetWindowPos. Finish its complete
    // control tree first, and release any cached DC before native animation.
    Realign;LinesplanFrame.Realign;
    LCLIntf.RedrawWindow(Handle,nil,0,RDW_INVALIDATE or RDW_ERASE or RDW_ALLCHILDREN or RDW_UPDATENOW);
    if Viewport.Canvas.HandleAllocated then TControlCanvas(Viewport.Canvas).FreeHandle;
    if Canvas.HandleAllocated then TControlCanvas(Canvas).FreeHandle;
    LinesplanDwmFlush;
    LinesplanFrame.TracePresentation('old-surface.committed');
  end;
  {$ENDIF}
  // Win32 invokes DefWindowProc after this LCL callback returns. Remember the
  // command until its subsequent WINDOWPOSCHANGING, rather than a call depth.
  if StateCommand then FPrepareNativeStateChange:=WindowState<>wsMinimized;
  if StateCommand then FCommitNativeStateChange:=True;
  if FPrepareNativeStateChange and (Message.Msg=LM_WINDOWPOSCHANGING) and
    (Viewport is TLinesplanViewport) and (LinesplanFrame<>nil) and
    (LinesplanFrame.FreeShip<>nil) and HandleAllocated then begin
    P:=TLMWindowPosMsg(Message).WindowPos;
    if (P<>nil) and ((P^.flags and $0001)=0) then begin
      FPrepareNativeStateChange:=False;
      LCLIntf.GetWindowRect(Handle,WR);LCLIntf.GetClientRect(Handle,CR);
      if (CR.Right>CR.Left) and (CR.Bottom>CR.Top) then begin
        W:=P^.cx-((WR.Right-WR.Left)-(CR.Right-CR.Left));
        H:=P^.cy-((WR.Bottom-WR.Top)-(CR.Bottom-CR.Top));
        LinesplanFrame.TracePresentation('precompose.before-native-resize');
        TLinesplanViewport(Viewport).PrepareNativeResize(W-Viewport.Left,H-Viewport.Top);
      end;
    end;
  end;
  inherited WndProc(Message);
  if (LinesplanFrame<>nil) and not (csDestroying in ComponentState) then
    LinesplanFrame.TraceNativeMessage('form','after',Message);
end;

procedure TFreeLinesplanForm.PrepareForDisplay;
begin
  // Complete layout and the first double-buffered frame before making the form visible.
  HandleNeeded;
  LinesplanFrame.HandleNeeded;
  Viewport.HandleNeeded;
  Realign;
  LinesplanFrame.Realign;
  Viewport.ZoomExtents;
  if Viewport is TLinesplanViewport then TLinesplanViewport(Viewport).PrepareFrame
  else TLinesplanViewportPainter(Viewport).Paint;
end;

resourcestring rsPreparingLinesplan='Preparing lines plan...';

function TFreeLinesplanForm.DisplaysShip(Ship:TFreeShip):Boolean;
begin
  Result:=(FOpeningShip=Ship) or (LinesplanFrame.FreeShip=Ship);
end;

procedure TFreeLinesplanForm.OpenForShip(Ship:TFreeShip);
begin
  FOpeningShip:=Ship;
  Ship.FreeNotification(Self);
  FReadyCaption:=Caption;
  Caption:=rsPreparingLinesplan;
  LinesplanFrame.Enabled:=False;
  // Show the inexpensive shell before generating the complete chart. Rendering
  // then uses the final native client dimensions, not a pre-show surface.
  Show;
  Application.QueueAsyncCall(FinishOpening,0);
end;

procedure TFreeLinesplanForm.FinishOpening(Data:PtrInt);
var Ship:TFreeShip;
begin
  Ship:=FOpeningShip;
  if Ship=nil then Exit;
  FOpeningShip:=nil;
  Ship.RemoveFreeNotification(Self);
  try
    LinesplanFrame.FreeShip:=Ship;
    PrepareForDisplay;
  finally
    Caption:=FReadyCaption;
    LinesplanFrame.Enabled:=True;
    Invalidate;
  end;
end;

procedure TFreeLinesplanForm.Notification(AComponent:TComponent; Operation:TOperation);
begin
  inherited Notification(AComponent,Operation);
  if (Operation=opRemove) and (AComponent=FOpeningShip) then begin
    Application.RemoveAsyncCalls(Self);
    FOpeningShip:=nil;
    Caption:=FReadyCaption;
    LinesplanFrame.Enabled:=True;
  end;
end;

destructor TFreeLinesplanForm.Destroy;
begin
  Application.RemoveAsyncCalls(Self);
  if FOpeningShip<>nil then FOpeningShip.RemoveFreeNotification(Self);
  inherited Destroy;
end;

procedure TFreeLinesplanForm.EraseBackground(DC:HDC);
var Target:TCanvas;
begin
  if (DC=0) or (LinesplanFrame=nil) or (LinesplanFrame.FreeShip=nil) or
    (csDestroying in ComponentState) then begin inherited EraseBackground(DC);Exit end;
  if not LinesplanNativeLayoutReady(Self,DC) then begin
    LinesplanFrame.TracePresentation('form-background.deferred',DC);Invalidate;Exit;
  end;
  Realign;
  LinesplanFrame.SetBounds(0,0,ClientWidth,ClientHeight);
  Target:=TCanvas.Create;
  try
    Target.Handle:=DC;
    LinesplanFrame.PresentClientBackground(Target);
  finally Target.Handle:=0;Target.Free end;
end;

constructor TFreeLinesplanForm.Create(Owner: TComponent);
begin
  inherited Create(Owner);
  ParentDoubleBuffered:=False;DoubleBuffered:=True;
  Viewport:=LinesplanFrame.Viewport;
  LinesplanFrame.FontSize := 7;
  LinesplanFrame.SpinEdit1.Value:=7;
end;

procedure TFreeLinesplanForm.FormClose(Sender: TObject;var Action: TCloseAction);
begin
   // Disconnect from FreeShip component;
   LinesplanFrame.FreeShip:=nil;
   Action:=caFree;
end;{TFreeLinesplanForm.FormClose}

procedure TFreeLinesplanForm.LinesplanFrameClick(Sender: TObject);
begin

end;

procedure TFreeLinesplanForm.SpinEdit1Change(Sender: TObject);
begin
  LinesplanFrame.SpinEdit1Change(Sender);
end;

end.
