# Delivery housekeeping, not a regression test. Retains test sources, reports,
# images, final runs, bridge builds and the pre-installation runtime backup.
$ErrorActionPreference='Stop'
$audit=Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'project-hydro-installed-20261105.json') | ConvertFrom-Json
if($audit.counts.fail -ne 0 -or $audit.counts.error -ne 0 -or $audit.installed_route_counts.fail -ne 0 -or $audit.installed_route_counts.error -ne 0){throw 'Installation is not verified'}
$receipt=Join-Path $PSScriptRoot 'project-hydro-retired-intermediates-20261105.json'
if(Test-Path -LiteralPath $receipt){throw 'Cleanup already recorded'}
$plans=@{
    # The old 'final' diagnostic includes a junction to the compiler toolchain;
    # keep that entire run rather than traverse or remove its linked subtree.
    'C:\FreeShip26-ProjectHydroTests'=@('candidate','complete')
    'D:\FreeShip26Tests\reports'=@('project-hydro-dialogs-20261105','project-hydro-regression-20261105','project-hydro-final-20261105')
}
$files=@();$targets=@()
foreach($parent in $plans.Keys){
    $root=[IO.Path]::GetFullPath($parent).TrimEnd('\')
    if((Get-Item -LiteralPath $root).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Report root is a link'}
    foreach($name in $plans[$parent]){
        $target=[IO.Path]::GetFullPath((Join-Path $root $name))
        if([IO.Path]::GetDirectoryName($target) -ne $root){throw 'Target escaped report root'}
        $node=Get-Item -LiteralPath $target
        if(-not $node.PSIsContainer -or ($node.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Expected plain report directory'}
        if(@(Get-ChildItem -LiteralPath $target -Recurse -Force -Attributes ReparsePoint).Count){throw 'Report contains a link'}
        foreach($property in $audit.reports.PSObject.Properties){
            if(([IO.Path]::GetFullPath($property.Value.path)).StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Final receipt references this run'}
        }
        foreach($file in Get-ChildItem -LiteralPath $target -Recurse -File){
            $path=[IO.Path]::GetFullPath($file.FullName)
            if(-not $path.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'File escaped report folder'}
            if($file.Name -match '(report|manifest|observations|request|result|retention).*\.json$' -or $file.Extension -in @('.png','.jpg','.jpeg','.log','.md','.txt','.py','.cpp','.h','.pas','.ps1')){continue}
            $files+=@{path=$path;bytes=$file.Length}
        }
        $targets+=$target
    }
}
# All absolute file targets above were checked before removal. No recursive
# directory deletion or move; no active/final output or user model is selected.
foreach($file in $files){Remove-Item -LiteralPath $file.path -Force}
foreach($target in $targets){[IO.File]::WriteAllText((Join-Path $target 'RETIRED-ARTIFACTS.md'),'Superseded project hydro run. Reports, images and logs retained; reproducible private model/scene/process files retired after installed verification. See FreeShip-Tests/maintenance/project-hydro-installed-20261105.json and project-hydro-retired-intermediates-20261105.json.',[Text.UTF8Encoding]::new($false))}
$result=@{created_utc=[DateTime]::UtcNow.ToString('o');retired=$files;bytes=($files | ForEach-Object {$_.bytes} | Measure-Object -Sum).Sum;kept='Test source, reports, images, final runs, bridge builds, previous package and old diagnostic containing a toolchain junction'}
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receipt -Encoding UTF8
Write-Output ('Retired MiB: '+[math]::Round($result.bytes/1MB,2))
