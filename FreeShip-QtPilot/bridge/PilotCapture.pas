unit PilotCapture;
{$mode objfpc}{$H+}
interface
uses Classes,SysUtils,Graphics;
procedure PilotBegin(const Filename:string;X0,Y0,X1,Y1:Double);
procedure PilotEnd;
procedure PilotCancel;
procedure PilotPoint(X,Y:Double);
procedure PilotFlush(Kind,Color:LongWord);
procedure PilotLine(X0,Y0,X1,Y1:Double;Color:LongWord);
procedure PilotLabel(X,Y:Double;Color:LongWord;Pixels:Integer;const Text:string);
function PilotActive:Boolean;
implementation
var Data:TMemoryStream=nil;Target:string;Points:array of Double;Count:Integer=0;
procedure U(V:LongWord);begin Data.WriteBuffer(V,4) end;
procedure D(V:Double);begin Data.WriteBuffer(V,8) end;
function PilotActive:Boolean;begin Result:=Data<>nil end;
procedure PilotBegin(const Filename:string;X0,Y0,X1,Y1:Double);
const Magic:array[0..7]of Char='FSLP0001';
begin
 if Data<>nil then raise Exception.Create('Capture already active');
 Target:=Filename;Data:=TMemoryStream.Create;Count:=0;
 Data.WriteBuffer(Magic,8);D(X0);D(Y0);D(X1);D(Y1);
end;
procedure PilotEnd;
begin
 if Data=nil then Exit;
 try
  if Count<>0 then raise Exception.Create('Unterminated capture path');
  Data.SaveToFile(Target);
 finally FreeAndNil(Data) end;
end;
procedure PilotCancel;
begin FreeAndNil(Data);Count:=0 end;
procedure PilotPoint(X,Y:Double);
begin
 if Data=nil then Exit;
 if Count+2>Length(Points) then SetLength(Points,Length(Points)+2048);
 Points[Count]:=X;Points[Count+1]:=Y;Inc(Count,2);
end;
procedure PilotFlush(Kind,Color:LongWord);
begin
 if Data=nil then Exit;
 if Count<4 then raise Exception.Create('Incomplete capture path');
 U(Kind);U(ColorToRGB(Color));U(Count div 2);
 Data.WriteBuffer(Points[0],Count*8);Count:=0;
end;
procedure PilotLine(X0,Y0,X1,Y1:Double;Color:LongWord);
begin PilotPoint(X0,Y0);PilotPoint(X1,Y1);PilotFlush(2,Color) end;
procedure PilotLabel(X,Y:Double;Color:LongWord;Pixels:Integer;const Text:string);
begin
 if Data=nil then Exit;
 U(3);U(ColorToRGB(Color));U(1);D(X);D(Y);U(Pixels);U(Length(Text));
 if Length(Text)>0 then Data.WriteBuffer(Text[1],Length(Text));
end;
end.
