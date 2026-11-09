# Retire reproducible intermediate outputs after installed verification.
# Keeps source, reports, images, logs, the final runs, bridge and previous package.
$ErrorActionPreference='Stop'
$audit=Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'underwater-installed-20261105.json') | ConvertFrom-Json
if($audit.counts.fail -ne 0 -or $audit.counts.error -ne 0 -or $audit.installed_route_counts.fail -ne 0 -or $audit.installed_route_counts.error -ne 0){throw 'Installation not verified'}
$receipt=Join-Path $PSScriptRoot 'underwater-retired-intermediates-20261105.json'
if(Test-Path -LiteralPath $receipt){throw 'Cleanup already recorded'}
$root=[IO.Path]::GetFullPath('C:\FreeShip26-UnderwaterTests').TrimEnd('\')
if((Get-Item -LiteralPath $root).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Report root is a link'}
$files=@();$targets=@()
foreach($name in @('primary','primary-02','ready','ready-02')){
    $target=[IO.Path]::GetFullPath((Join-Path $root $name))
    if([IO.Path]::GetDirectoryName($target) -ne $root){throw 'Target escaped report root'}
    $node=Get-Item -LiteralPath $target
    if(-not $node.PSIsContainer -or ($node.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Expected plain report directory'}
    if(@(Get-ChildItem -LiteralPath $target -Recurse -Force -Attributes ReparsePoint).Count){throw 'Report contains a link'}
    foreach($report in $audit.reports){
        if(([IO.Path]::GetFullPath($report.path)).StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Final report references this run'}
    }
    foreach($file in Get-ChildItem -LiteralPath $target -Recurse -File){
        $path=[IO.Path]::GetFullPath($file.FullName)
        if(-not $path.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'File escaped report folder'}
        if($file.Name -match '(report|manifest|observations|request|result|retention).*\.json$' -or $file.Extension -in @('.png','.jpg','.jpeg','.log','.md','.txt','.py','.cpp','.h','.pas','.ps1')){continue}
        $files+=@{path=$path;bytes=$file.Length}
    }
    $targets+=$target
}
# Remove only enumerated, contained files, without directory recursion or links.
foreach($file in $files){Remove-Item -LiteralPath $file.path -Force}
foreach($target in $targets){
    [IO.File]::WriteAllText((Join-Path $target 'RETIRED-ARTIFACTS.md'),'Superseded water-opacity diagnostic. Reports, images and logs retained; reproducible private model/scene outputs retired after installed verification. See FreeShip-Tests/maintenance/underwater-installed-20261105.json and underwater-retired-intermediates-20261105.json.',[Text.UTF8Encoding]::new($false))
}
$result=@{created_utc=[DateTime]::UtcNow.ToString('o');retired=$files;bytes=($files | ForEach-Object {$_.bytes} | Measure-Object -Sum).Sum;kept='Test source, reports, images, logs, final runs, bridge build and previous package'}
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receipt -Encoding UTF8
Write-Output ('Retired MiB: '+[math]::Round($result.bytes/1MB,2))
