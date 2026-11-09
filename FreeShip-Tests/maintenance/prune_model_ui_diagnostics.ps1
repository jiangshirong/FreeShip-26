# One delivery's storage maintenance, not a regression test. Keep reports,
# screenshots and logs; only retire reproducible artifacts from superseded runs.
$ErrorActionPreference='Stop'
$audit=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'model-ui-installed-20261104.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if($audit.counts.fail -ne 0 -or $audit.counts.error -ne 0){throw 'Installation not verified'}
$receipt=Join-Path $PSScriptRoot 'model-ui-retired-diagnostics-20261104.json'
if(Test-Path -LiteralPath $receipt){throw 'Already retired'}
$reportRoot=[IO.Path]::GetFullPath('D:\FreeShip26Tests\reports').TrimEnd('\')
$obsolete=@('model-ui-01','model-ui-final-01','model-ui-final-02','model-ui-publication-01','model-ui-publication-02')
$files=@();$folders=@()
foreach($name in $obsolete){
    $target=[IO.Path]::GetFullPath((Join-Path $reportRoot $name))
    if([IO.Path]::GetDirectoryName($target) -ne $reportRoot){throw 'Unexpected report target'}
    $item=Get-Item -LiteralPath $target
    if(-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Expected plain directory'}
    if(@(Get-ChildItem -LiteralPath $target -Recurse -Force -Attributes ReparsePoint).Count){throw 'Report contains a link'}
    foreach($property in $audit.reports.PSObject.Properties){if(([IO.Path]::GetFullPath($property.Value.path)).StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Final receipt still references this run'}}
    foreach($file in Get-ChildItem -LiteralPath $target -Recurse -File){
        $path=[IO.Path]::GetFullPath($file.FullName)
        if(-not $path.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Artifact escaped report folder'}
        if($file.Name -match '(report|manifest|observations).*\.json$' -or $file.Extension -in @('.png','.log','.md')){continue}
        $files+=@{path=$path;bytes=$file.Length}
    }
    $folders+=$target
}
# Every absolute file path has been checked; no directory, source or baseline is removed.
foreach($file in $files){Remove-Item -LiteralPath $file.path -Force}
foreach($folder in $folders){[IO.File]::WriteAllText((Join-Path $folder 'RETIRED-ARTIFACTS.md'),"Superseded model UI run. Reports, screenshots and logs retained. Reproducible model/scene/process artifacts retired after final installation verification. Final evidence: model-ui-final-03, model-ui-publication-03, model-ui-installed-01. See FreeShip-Tests/maintenance/model-ui-retired-diagnostics-20261104.json.",[Text.UTF8Encoding]::new($false))}
$record=@{created_utc=[DateTime]::UtcNow.ToString('o');retired=$files;kept='All reports, screenshots, logs and final runs';bytes=($files|ForEach-Object{$_.bytes}|Measure-Object -Sum).Sum}
$record|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $receipt -Encoding UTF8
Write-Output ('Reclaimed MiB: '+[math]::Round($record.bytes/1MB,2))
