# Deletes only reviewed generated files. No source, report, model or screenshot
# removal. Checks all absolute targets and inventories before deletion.
param([Parameter(Mandatory=$true)][string]$Plan,[switch]$Apply)
$ErrorActionPreference='Stop'
$planPath=(Resolve-Path -LiteralPath $Plan).Path
$data=Get-Content -LiteralPath $planPath -Raw -Encoding UTF8 | ConvertFrom-Json
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..')).TrimEnd('\')
$expected=@('C:\FreeShip26Tests\reports','D:\FreeShip26Tests\reports',(Join-Path $workspace 'FreeShip-Tests\reports'),'C:\FreeShip26Tests\build','D:\FreeShip26Tests\build',(Join-Path $workspace 'FreeShip-Qt\build'),'C:\FreeShip26-HydrodynamicsTests','C:\FreeShip26-ShadingTests','C:\FreeShip26-ViewportTests')
if($data.format -ne 'FreeShipStorageCleanup' -or $data.version -ne 1 -or @(Compare-Object ($data.roots|Sort-Object) ($expected|Sort-Object)).Count){throw 'Unexpected cleanup root or schema'}
if((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'plan_storage_cleanup.py') -Algorithm SHA256).Hash -ne $data.planner_sha256){throw 'Planner changed since inventory'}
$receipt=$planPath+'.result.json'
if(Test-Path -LiteralPath $receipt){throw 'Plan already applied'}
$checked=@{};$targets=[Collections.Generic.List[object]]::new();$protected=@($data.protected)+@((Join-Path $workspace 'FreeShip-Qt\dist'))
$processes=@(Get-CimInstance Win32_Process | Where-Object {$_.Name -match 'FreeShip|Qt.*Fixture|PerformanceOracle|fpc|ppcx64|mingw|g\+\+|cmake|ninja'})
foreach($process in $processes){
    foreach($root in $expected){if($process.ExecutablePath -and $process.ExecutablePath.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'A generated program is still running'}}
}
foreach($entry in $data.files){
    $path=[IO.Path]::GetFullPath($entry.path)
    $roots=@($expected | Where-Object {$path.StartsWith($_+'\',[StringComparison]::OrdinalIgnoreCase)})
    if($roots.Count -ne 1){throw 'Target outside expected output roots'}
    foreach($keep in $protected){if($path -eq $keep -or $path.StartsWith($keep+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Protected target'}}
    if($entry.reason -eq 'rebuildable-native-lines-cache'){
        if(-not $path.Substring($roots[0].Length).Contains('\native-lines-cache\')){throw 'Not a cache file'}
    }elseif($entry.reason -eq 'superseded-compiled-build'){
        if([IO.Path]::GetFileName($roots[0]) -ne 'build' -or [IO.Path]::GetExtension($path).ToLowerInvariant() -notin @('.exe','.dll','.o','.obj','.a','.ppu','.pdb','.ilk','.lib')){throw 'Not a compiled build file'}
    }else{throw 'Unknown retirement reason'}
    $parent=[IO.Path]::GetDirectoryName($path)
    while($parent -and -not $checked.ContainsKey($parent)){
        if((Get-Item -LiteralPath $parent).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked parent'}
        $checked[$parent]=$true
        if($parent -eq $roots[0]){break}
        $parent=[IO.Path]::GetDirectoryName($parent)
    }
    $file=Get-Item -LiteralPath $path
    if($file.PSIsContainer -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Not a plain generated file'}
    $nano=([decimal]$file.LastWriteTimeUtc.Ticks-621355968000000000)*100
    if($file.Length -ne $entry.bytes -or $nano -ne [decimal]$entry.mtime_ns){throw "File changed since inventory: $path"}
    $targets.Add($entry)
}
if(-not $Apply){[PSCustomObject]@{Validated=$targets.Count;LogicalGiB=[Math]::Round($data.bytes/1GB,3)} | ConvertTo-Json;return}
$before=@{C=(Get-PSDrive C).Free;D=(Get-PSDrive D).Free}
$deleted=0L;$bytes=0L
try{
    foreach($entry in $targets){Remove-Item -LiteralPath $entry.path -Force;$deleted++;$bytes+=$entry.bytes}
}finally{
    $after=@{C=(Get-PSDrive C).Free;D=(Get-PSDrive D).Free}
    $record=@{created_utc=[DateTime]::UtcNow.ToString('o');plan=$planPath;plan_sha256=(Get-FileHash -LiteralPath $planPath -Algorithm SHA256).Hash;status=if($deleted -eq $targets.Count){'complete'}else{'partial'};deleted_files=$deleted;logical_bytes=$bytes;free_before=$before;free_after=$after;actual_free_delta=@{C=$after.C-$before.C;D=$after.D-$before.D};kept='All source, reports, screenshots, models, published scenes/exports, manifests, current runtime, protected baselines, current build and private tools. Historical compiled runtimes require rebuild; their manifests remain.'}
    $record|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $receipt -Encoding UTF8
}
$record|ConvertTo-Json -Depth 4
