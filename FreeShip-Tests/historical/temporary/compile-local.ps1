param([string]$Name)
$t="$env:TEMP\FreeShipLabelIntegration"
$args=@('-B','-dLCL','-dLCLwin32','-FiK:\Units','-FiK:\Forms','-FuK:\Units','-FuK:\Forms','-FuK:\',"-FU$t\about-units","-FE$t")
foreach($d in @('tachart','printers','ideintf','lazcontrols','cairocanvas')){$sub=if($d -eq 'ideintf'){'units'}else{'lib'};$args+='-FuL:\components\'+$d+'\'+$sub+'\x86_64-win64\win32'}
$args+=@('-FuL:\lcl\units\x86_64-win64\win32','-FuL:\lcl\units\x86_64-win64','-FuL:\components\freetype\lib\x86_64-win64','-FuL:\components\buildintf\units\x86_64-win64','-FuL:\components\lazutils\lib\x86_64-win64','-FuL:\packager\units\x86_64-win64')
$args+=Get-ChildItem 'L:\fpc\3.2.2\units\x86_64-win64' -Directory | Where-Object Name -ne 'fv' | ForEach-Object {'-Fu'+$_.FullName}
& L:\fpc\3.2.2\bin\x86_64-win64\fpc.exe @args "$t\$Name.pas" *> "$t\$Name-build.log"
if($LASTEXITCODE -ne 0){Get-Content "$t\$Name-build.log" -Tail 6;exit 1}
& "$t\$Name.exe"
