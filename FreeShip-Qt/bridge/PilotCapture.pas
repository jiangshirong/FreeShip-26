unit PilotCapture;
{$mode objfpc}{$H+}
interface
uses Classes,SysUtils,Graphics,Math;
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
 TotalPoints:Int64=0;PrimitiveCount:Integer=0;
procedure CheckCoordinate(V:Double);
begin
 if IsNan(V) or IsInfinite(V) or (Abs(V)>=1e12) then
  raise Exception.Create('Invalid scene coordinate');
end;
procedure AddPrimitive(N:Integer);
begin
 if TotalPoints+N>12000000 then raise Exception.Create('Scene exceeds 12 million points');
 Inc(TotalPoints,N);Inc(PrimitiveCount);
end;
procedure U(V:LongWord);begin Data.WriteBuffer(V,4) end;
procedure D(V:Double);begin Data.WriteBuffer(V,8) end;
function PilotActive:Boolean;begin Result:=Data<>nil end;
procedure PilotBegin(const Filename:string;X0,Y0,X1,Y1:Double);
const Magic:array[0..7]of Char='FSLP0001';
begin
 if Data<>nil then raise Exception.Create('Capture already active');
 CheckCoordinate(X0);CheckCoordinate(Y0);CheckCoordinate(X1);CheckCoordinate(Y1);
 if (X1<=X0) or (Y1<=Y0) then raise Exception.Create('Invalid scene bounds');
 Target:=Filename;Data:=TMemoryStream.Create;Count:=0;
 TotalPoints:=0;PrimitiveCount:=0;
 Data.WriteBuffer(Magic,8);D(X0);D(Y0);D(X1);D(Y1);
end;
procedure PilotEnd;
begin
 if Data=nil then Exit;
 try
  if Count<>0 then raise Exception.Create('Unterminated capture path');
  if PrimitiveCount=0 then raise Exception.Create('Empty scene');
  if Data.Size>256*1024*1024 then raise Exception.Create('Scene exceeds 256 MiB');
  Data.SaveToFile(Target);
 finally FreeAndNil(Data) end;
end;
procedure PilotCancel;
begin FreeAndNil(Data);Count:=0 end;
procedure PilotPoint(X,Y:Double);
begin
 if Data=nil then Exit;
 CheckCoordinate(X);CheckCoordinate(Y);
 if Count>=2000000 then raise Exception.Create('Scene path exceeds one million points');
 if Count+2>Length(Points) then SetLength(Points,Length(Points)+2048);
 Points[Count]:=X;Points[Count+1]:=Y;Inc(Count,2);
end;
procedure PilotFlush(Kind,Color:LongWord);
begin
 if Data=nil then Exit;
 if not (((Kind=1) and (Count=6)) or ((Kind=2) and (Count>=4) and (Count<=2000000))) then
  raise Exception.Create('Invalid scene primitive point count');
 AddPrimitive(Count div 2);
 U(Kind);U(ColorToRGB(Color));U(Count div 2);
 Data.WriteBuffer(Points[0],Count*8);Count:=0;
end;
procedure PilotLine(X0,Y0,X1,Y1:Double;Color:LongWord);
begin PilotPoint(X0,Y0);PilotPoint(X1,Y1);PilotFlush(2,Color) end;
procedure PilotLabel(X,Y:Double;Color:LongWord;Pixels:Integer;const Text:string);
var Encoded:UTF8String;
begin
 if Data=nil then Exit;
 CheckCoordinate(X);CheckCoordinate(Y);
 if (Pixels<1) or (Pixels>512) or (Length(Text)>65536) then raise Exception.Create('Invalid scene label');
 Encoded:=UTF8Encode(UTF8Decode(Text));
 if Length(Encoded)<>Length(Text) then raise Exception.Create('Invalid UTF-8 label');
 if Length(Text)>0 then
  if CompareByte(Encoded[1],Text[1],Length(Text))<>0 then raise Exception.Create('Invalid UTF-8 label');
 AddPrimitive(1);
 U(3);U(ColorToRGB(Color));U(1);D(X);D(Y);U(Pixels);U(Length(Text));
 if Length(Text)>0 then Data.WriteBuffer(Text[1],Length(Text));
end;
end.
