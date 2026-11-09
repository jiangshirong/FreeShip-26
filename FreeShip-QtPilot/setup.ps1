$ErrorActionPreference='Stop'
$pilotRoot=$PSScriptRoot
$pilotAlias=Join-Path $env:TEMP 'FreeShip26QtPilot'
if (!(Test-Path -LiteralPath $pilotAlias)) {New-Item -ItemType Junction -Path $pilotAlias -Target $pilotRoot | Out-Null}
if ((Get-Item -LiteralPath $pilotAlias).Target -ne $pilotRoot) {throw 'Build alias points at another project'}
$pythonExe=Join-Path $pilotRoot '.tools\python\Scripts\python.exe'
if (!(Test-Path -LiteralPath $pythonExe)) {python -m venv "$pilotRoot\.tools\python"; if ($LASTEXITCODE) {throw 'Python environment creation failed'}}
& $pythonExe -m pip install aqtinstall==3.3.0 cmake==4.4.4 ninja==1.13.2
if ($LASTEXITCODE) {throw 'Build helper installation failed'}
$qtDir=Join-Path $pilotAlias '.tools\Qt'
if (!(Test-Path -LiteralPath "$qtDir\6.8.3\mingw_64\bin\qmake.exe")) {
 & $pythonExe -m aqt install-qt windows desktop 6.8.3 win64_mingw -O $qtDir --archives qtbase
 if ($LASTEXITCODE) {throw 'Qt installation failed'}
}
$compilerRoot=Join-Path $env:LOCALAPPDATA 'FreeShip26QtTools'
if (!(Test-Path -LiteralPath "$compilerRoot\mingw1310_64\bin\g++.exe")) {
 & $pythonExe -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O $compilerRoot
 if ($LASTEXITCODE) {throw 'Compiler installation failed'}
 # aqt installs tools below Tools; use the explicit final path expected by build.ps1.
 if (!(Test-Path -LiteralPath "$compilerRoot\mingw1310_64")) {
  Copy-Item -LiteralPath "$compilerRoot\Tools\mingw1310_64" -Destination "$compilerRoot\mingw1310_64" -Recurse
 }
}
& "$qtDir\6.8.3\mingw_64\bin\qmake.exe" -query QT_VERSION
if ($LASTEXITCODE) {throw 'Qt verification failed'}
& "$compilerRoot\mingw1310_64\bin\g++.exe" --version
