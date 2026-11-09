# One-off crash diagnosis. Not a regression: requires local GDB and may change
# thread scheduling. qt-calculation-chart-speed owns the actual exit assertion.
param([Parameter(Mandatory=$true)][string]$Output,
      [string]$Runtime='C:/FreeShip26-MultihullTests/runtime')
$ErrorActionPreference='Stop'
$debugOut=[IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath $debugOut){throw 'Use a new isolated output directory'}
New-Item -ItemType Directory -Path $debugOut | Out-Null
$debugger=Join-Path $env:LOCALAPPDATA 'FreeShip26QtTools/mingw1310_64/bin/gdb.exe'
$commands=Join-Path $debugOut 'commands.gdb'
[IO.File]::WriteAllText($commands,"set pagination off`nset print thread-events off`nrun`nthread apply all bt`n",[Text.UTF8Encoding]::new($false))
& $debugger --batch "--command=$commands" --args (Join-Path $Runtime 'qt_calculation_chart_tests.exe') --warmup-exit $debugOut *> (Join-Path $debugOut 'gdb.log')
Get-Content -LiteralPath (Join-Path $debugOut 'gdb.log') -Tail 70
