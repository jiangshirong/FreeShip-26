# Explicit storage maintenance after installed verification, not a regression.
# Keep source, final evidence, prior installation, diagnostics and report outcomes.
$ErrorActionPreference='Stop'
$audit=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'hydrodynamics-installed-20261104.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if($audit.counts.fail -ne 0 -or $audit.counts.error -ne 0){throw 'Installation not verified'}
$receipt=Join-Path $PSScriptRoot 'hydrodynamics-retired-intermediates-20261104.json'
if(Test-Path -LiteralPath $receipt){throw 'Already retired'}
$root=[IO.Path]::GetFullPath('C:\FreeShip26-HydrodynamicsTests').TrimEnd('\')
if((Get-Item -LiteralPath $root).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Output root is a link'}
if((Get-FileHash -LiteralPath (Join-Path $audit.installed 'build-manifest.json') -Algorithm SHA256).Hash -ne $audit.package_manifest_sha256){throw 'Installation changed'}
function Plain-Child([string]$name){
    $path=[IO.Path]::GetFullPath((Join-Path $root $name))
    if([IO.Path]::GetDirectoryName($path) -ne $root){throw 'Target escaped output root'}
    $item=Get-Item -LiteralPath $path
    if(-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Expected plain directory'}
    if(@(Get-ChildItem -LiteralPath $path -Force -Recurse -Attributes ReparsePoint).Count){throw 'Target contains a link'}
    return $path
}
$packages=@();$files=@();$reports=@()
$processes=Get-CimInstance Win32_Process
foreach($name in 'package-01','package-02','package-03','package-04','package-05'){
    $path=Plain-Child $name
    if(-not(Test-Path -LiteralPath (Join-Path $path 'build-manifest.json'))){throw 'Not a package'}
    foreach($process in $processes){if($process.ExecutablePath -and $process.ExecutablePath.StartsWith($path+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Package still running'}}
    $packages+=@{path=$path;name=$name;bytes=(Get-ChildItem -LiteralPath $path -Recurse -File | Measure-Object Length -Sum).Sum}
}
foreach($name in 'service-01','service-02','reports-01','reports-02','reports-03','reports-04','reports-05'){
    $path=Plain-Child $name
    foreach($property in $audit.reports.PSObject.Properties){if(([IO.Path]::GetFullPath($property.Value.path)).StartsWith($path+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Final receipt references this run'}}
    foreach($process in $processes){if($process.CommandLine -and $process.CommandLine.IndexOf($name,[StringComparison]::OrdinalIgnoreCase) -ge 0 -and $process.CommandLine.IndexOf('HydrodynamicsTests',[StringComparison]::OrdinalIgnoreCase) -ge 0){throw 'Report run may still be active'}}
    foreach($file in Get-ChildItem -LiteralPath $path -Recurse -File){
        $target=[IO.Path]::GetFullPath($file.FullName)
        if(-not $target.StartsWith($path+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'File escaped report folder'}
        if($file.Name -match '(report|manifest|observations).*\.json$' -or $file.Name -eq 'process-result.json' -or $file.Extension -in @('.png','.log','.md','.py','.cpp','.h','.ps1') -or $target.Contains('\solver-files\')){continue}
        $files+=@{path=$target;bytes=$file.Length}
    }
    $reports+=$path
}
$manifests=Join-Path $root 'retired-manifests'
if(Test-Path -LiteralPath $manifests){throw 'Manifest archive already exists'}
New-Item -ItemType Directory -Path $manifests | Out-Null
foreach($entry in $packages){Copy-Item -LiteralPath (Join-Path $entry.path 'build-manifest.json') -Destination (Join-Path $manifests ($entry.name+'.json'))}
# Resolved absolute targets above stay inside this task's output root.
foreach($entry in $packages){Remove-Item -LiteralPath $entry.path -Recurse -Force}
foreach($entry in $files){Remove-Item -LiteralPath $entry.path -Force}
foreach($path in $reports){[IO.File]::WriteAllText((Join-Path $path 'RETIRED-ARTIFACTS.md'),'Superseded Hydrodyn diagnostic run. Source, reports, exit records, original solver protocol files, screenshots and logs retained. Reproducible model/scene/large JSON/CSV intermediates retired after installed verification. Full final evidence: reports-final, publication-01 and installed-01. See FreeShip-Tests/maintenance/hydrodynamics-retired-intermediates-20261104.json.',[Text.UTF8Encoding]::new($false))}
$record=@{created_utc=[DateTime]::UtcNow.ToString('o');packages=$packages;files=$files;kept='All source, reports, original solver protocol files, screenshots, logs, manifests, diagnostics, final runs, package-06, package-final and installed backup';bytes=(@($packages)+@($files)|ForEach-Object{$_.bytes}|Measure-Object -Sum).Sum}
$record|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $receipt -Encoding UTF8
Write-Output ('Reclaimed MiB: '+[math]::Round($record.bytes/1MB,2))
