# Restore the physical ASCII compiler path used by existing CMake caches.
# MinGW resolves directory junctions before linking; the Chinese source path
# therefore cannot serve as the compiler prefix even through a short junction.
$ErrorActionPreference = 'Stop'
$taskWorkspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskTarget = (Resolve-Path -LiteralPath (Join-Path $taskWorkspace 'FreeShip-QtPilot/.tools/Qt/Tools')).Path
$taskAlias = Join-Path $env:LOCALAPPDATA 'FreeShip26QtTools'
if (!(Test-Path -LiteralPath (Join-Path $taskTarget 'mingw1310_64/bin/g++.exe') -PathType Leaf)) {
    throw 'The preserved MinGW installation is missing.'
}
if (Test-Path -LiteralPath $taskAlias) {
    $taskExisting = Get-Item -LiteralPath $taskAlias -Force
    if ($taskExisting.LinkType -ne 'Junction' -or [IO.Path]::GetFullPath([string]$taskExisting.Target).TrimEnd('\') -ne $taskTarget.TrimEnd('\')) {
        throw "Existing compiler path is not the known alias; inspect without replacing: $taskAlias"
    }
    # Verified junction only. No recursion: preserve its target and all files.
    Remove-Item -LiteralPath $taskAlias -Force
}
New-Item -ItemType Directory -Path $taskAlias | Out-Null
Copy-Item -LiteralPath (Join-Path $taskTarget 'mingw1310_64') -Destination $taskAlias -Recurse
Get-Item -LiteralPath $taskAlias | Select-Object FullName,Target
& (Join-Path $taskAlias 'mingw1310_64/bin/g++.exe') --version
if ($LASTEXITCODE) { throw 'Compiler verification failed' }
