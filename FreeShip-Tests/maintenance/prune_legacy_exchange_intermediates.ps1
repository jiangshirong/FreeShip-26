# One-time cleanup of the unused build accidentally created in this delivery.
# Not a regression test, scheduled cleanup, or general directory remover.
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..')).TrimEnd('\')
$qt=Join-Path $workspace 'FreeShip-Qt'
$target=[IO.Path]::GetFullPath((Join-Path $qt 'build\qt'))
$receipt=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'legacy-exchange-installed-20261104.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$installed=Join-Path $qt 'dist'
$manifestPath=Join-Path $installed 'build-manifest.json'
if($receipt.installed -ne $installed -or (Get-FileHash -LiteralPath $manifestPath).Hash -ne $receipt.package_manifest_sha256){throw 'Installation changed'}
$manifest=Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
if([IO.Path]::GetFullPath($manifest.build_dir) -eq $target){throw 'Active build must be retained'}
if([IO.Path]::GetDirectoryName($target) -ne (Join-Path $qt 'build')){throw 'Unexpected target'}
$node=Get-Item -LiteralPath $target
if($node.CreationTime.ToString('yyyy-MM-dd HH:mm') -ne '2026-11-04 19:14'){throw 'Not this delivery build'}
$entries=@($node)+@(Get-ChildItem -LiteralPath $target -Recurse -Force)
if(@($entries | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count){throw 'Linked target'}
$cache=Get-Content -LiteralPath (Join-Path $target 'CMakeCache.txt') -Raw
if($cache -notmatch 'CMAKE_PROJECT_NAME:STATIC=FreeShipQt'){throw 'Unexpected project'}
$files=@($entries | Where-Object {-not $_.PSIsContainer})
foreach($file in $files){
    $absolute=[IO.Path]::GetFullPath($file.FullName)
    if(-not $absolute.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'File outside target'}
    if($file.Extension -notin @('.a','.bin','.check_cache','.cmake','.cpp','.exe','.json','.ninja','.ninja_deps','.ninja_log','.obj','.txt','.yaml')){throw 'Unexpected file type'}
    if($file.Extension -eq '.cpp' -and $file.Name -ne 'CMakeCXXCompilerId.cpp'){throw 'Unexpected source'}
}
foreach($process in Get-CimInstance Win32_Process){
    if($process.ExecutablePath -and $process.ExecutablePath.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Build program is still running'}
}
$inventory=@($files | ForEach-Object {[ordered]@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}})
$record=Join-Path $PSScriptRoot 'legacy-exchange-build-cleanup-20261104.json'
$result=[ordered]@{status='prepared';target=$target;logical_bytes=($files|Measure-Object Length -Sum).Sum;files=$inventory;retained='Active build, source, reports, installed runtime, tested candidate and rollback'}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $record -Encoding UTF8
# All absolute targets were checked. Delete files then only empty directories.
foreach($file in $files){Remove-Item -LiteralPath $file.FullName}
foreach($directory in ($entries | Where-Object {$_.PSIsContainer} | Sort-Object {$_.FullName.Length} -Descending)){[IO.Directory]::Delete($directory.FullName,$false)}
$result.status='complete'
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $record -Encoding UTF8
[ordered]@{status=$result.status;files=$files.Count;logical_bytes=$result.logical_bytes} | ConvertTo-Json -Compress
