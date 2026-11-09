param([switch]$Deploy,[string]$DeployDir)
$ErrorActionPreference='Stop'
# MinGW tools do not consistently handle non-ASCII build paths. The junction
# points at this project only; it is not an additional source copy.
$pilotRoot=$PSScriptRoot
$pilotAlias=Join-Path $env:TEMP 'FreeShip26QtPilot'
if (!(Test-Path -LiteralPath $pilotAlias)) {New-Item -ItemType Junction -Path $pilotAlias -Target $pilotRoot | Out-Null}
if ((Get-Item -LiteralPath $pilotAlias).Target -ne $pilotRoot) {throw 'Build alias points at a different directory'}
$qtPrefix=Join-Path $pilotAlias '.tools\Qt\6.8.3\mingw_64'
$compilerBin=Join-Path $env:LOCALAPPDATA 'FreeShip26QtTools\mingw1310_64\bin'
$testsAlias=Join-Path $env:TEMP 'FreeShip26QtTests'
$testsRoot=Join-Path (Split-Path $pilotRoot -Parent) 'FreeShip-Tests'
if (!(Test-Path -LiteralPath $testsAlias)) {New-Item -ItemType Junction -Path $testsAlias -Target $testsRoot | Out-Null}
if ((Get-Item -LiteralPath $testsAlias).Target -ne $testsRoot) {throw 'Test alias points at a different directory'}
$pythonBin=Join-Path $pilotAlias '.tools\python\Scripts'
$savedPath=$env:PATH
try {
 $env:PATH="$compilerBin;$qtPrefix\bin;$pythonBin;$savedPath"
 & "$pythonBin\cmake.exe" -S $pilotAlias -B "$pilotAlias\build\qt" -G Ninja "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_PREFIX_PATH=$qtPrefix" "-DCMAKE_CXX_COMPILER=$compilerBin\g++.exe" "-DPILOT_TEST_SOURCE=$testsAlias/qt/linesplan_tests.cpp"
 if ($LASTEXITCODE) {throw 'CMake configure failed'}
 & "$pythonBin\cmake.exe" --build "$pilotAlias\build\qt" --parallel 4
 if ($LASTEXITCODE) {throw 'C++ compilation failed'}
 if ($Deploy) {
  if (!$DeployDir) {$DeployDir=Join-Path $pilotAlias 'dist'}
  $deployDir=[IO.Path]::GetFullPath($DeployDir)
  New-Item -ItemType Directory -Force -Path $deployDir | Out-Null
  Copy-Item -LiteralPath "$pilotAlias\build\qt\FreeShipQtPilot.exe" -Destination $deployDir
  Copy-Item -LiteralPath "$pilotAlias\build\qt\qt_lines_tests.exe" -Destination $deployDir
  & "$qtPrefix\bin\windeployqt.exe" --release --no-translations --no-opengl-sw --compiler-runtime "$deployDir\FreeShipQtPilot.exe"
  if ($LASTEXITCODE) {throw 'Qt runtime deployment failed'}
  Copy-Item -LiteralPath "$qtPrefix\plugins\platforms\qoffscreen.dll" -Destination "$deployDir\platforms"
 }
} finally {$env:PATH=$savedPath}
