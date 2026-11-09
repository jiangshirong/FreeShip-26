# Explicit delivery maintenance; not a regression and never a scheduled cleanup.
# Retire this task's obsolete candidates after installation QA. Reports, source,
# final package, installed app and its rollback remain available.
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$receipt=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'curve-import-installed-20261104.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$installed=Join-Path $workspace 'FreeShip-Qt\dist'
if($receipt.installed -ne $installed){throw 'Unexpected installation'}
if((Get-FileHash -LiteralPath (Join-Path $installed 'build-manifest.json')).Hash -ne $receipt.package_manifest_sha256){throw 'Installation changed'}
foreach($report in $receipt.reports.PSObject.Properties.Value){
    if((Get-FileHash -LiteralPath $report.path).Hash -ne $report.sha256 -or $report.counts.fail -ne 0 -or $report.counts.error -ne 0){throw 'Report verification failed'}
}
$root='C:\FreeShip26-CurveImportTests'
if((Get-Item -LiteralPath $root).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked private root'}
$targets=@('package-02','package-03','package-04') | ForEach-Object {[IO.Path]::GetFullPath((Join-Path $root $_))}
foreach($process in Get-CimInstance Win32_Process){
    if($process.ExecutablePath){foreach($target in $targets){if($process.ExecutablePath.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Candidate is still running'}}}
}
$files=@();$directories=@();[long]$bytes=0
foreach($target in $targets){
    if([IO.Path]::GetDirectoryName($target) -ne $root){throw 'Target escaped private output'}
    $node=Get-Item -LiteralPath $target
    if(-not $node.PSIsContainer -or ($node.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Expected plain candidate'}
    $entries=@(Get-ChildItem -LiteralPath $target -Recurse -Force)
    if(@($entries | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count){throw 'Candidate contains a link'}
    $manifestPath=Join-Path $target 'build-manifest.json'
    $manifest=Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $expected=@{}
    foreach($item in $manifest.binaries_sha256.PSObject.Properties){
        $file=[IO.Path]::GetFullPath((Join-Path $target $item.Name))
        if(-not $file.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Manifest path escaped target'}
        if((Get-FileHash -LiteralPath $file).Hash -ne $item.Value){throw 'Candidate file modified'}
        $expected[$file]=$true
    }
    foreach($file in @($entries | Where-Object {-not $_.PSIsContainer})){
        if($file.FullName -ne $manifestPath -and -not $expected.ContainsKey($file.FullName)){throw 'Unrecognized candidate file'}
        $files+=$file.FullName;$bytes+=$file.Length
    }
    Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $root ((Split-Path $target -Leaf)+'-retired-manifest.json'))
    $directories+=@($entries | Where-Object {$_.PSIsContainer} | ForEach-Object {$_.FullName})
    $directories+=$target
}
# Every absolute target and file was validated first. Delete individual files,
# then only empty directories; never recursively delete a computed directory.
foreach($file in $files){Remove-Item -LiteralPath $file}
foreach($directory in ($directories | Sort-Object Length -Descending)){[IO.Directory]::Delete($directory,$false)}
$result=[ordered]@{status='complete';removed_files=$files.Count;logical_bytes=$bytes;targets=$targets;retained='Reports, input fixtures, final package, installed app and rollback'}
$result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $root 'intermediate-cleanup.json') -Encoding UTF8
$result | ConvertTo-Json -Compress
