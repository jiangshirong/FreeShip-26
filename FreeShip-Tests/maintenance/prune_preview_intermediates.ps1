# One delivery's housekeeping, not a regression. Test sources, reports,
# screenshots and final evidence survive; only reproducible intermediate data
# from this delivery's named runs is retired after installation verification.
$ErrorActionPreference='Stop'
$auditPath=Join-Path $PSScriptRoot 'preview-image-installed-20261105.json'
$audit=Get-Content -Raw -LiteralPath $auditPath | ConvertFrom-Json
if($audit.counts.fail -ne 0 -or $audit.counts.error -ne 0){throw 'Installation is not verified'}
$receipt=Join-Path $PSScriptRoot 'preview-retired-intermediates-20261105.json'
if(Test-Path -LiteralPath $receipt){throw 'Cleanup already recorded'}
$plans=@{
    'C:\FreeShip26-PreviewTests'=@('candidate','candidate-02','candidate-03','final','publication-final','saving','saving-02','saving-final','profile')
    'D:\FreeShip26Tests\reports'=@('preview-final-20261105')
}
$targets=@();$files=@()
foreach($parent in $plans.Keys){
    $root=[IO.Path]::GetFullPath($parent).TrimEnd('\')
    $rootItem=Get-Item -LiteralPath $root
    if($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Report root is a link'}
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
            if($file.Name -match '(report|manifest|observations|request|result|retention).*\.json$' -or
               $file.Extension -in @('.png','.jpg','.jpeg','.log','.md','.txt','.py','.cpp','.h','.ps1')){continue}
            $files+=@{path=$path;bytes=$file.Length}
        }
        $targets+=$target
    }
}
# Every absolute target has been checked before deletion; no directory move or
# recursive removal is used. Active/final runs and both runtime packages remain.
foreach($file in $files){Remove-Item -LiteralPath $file.path -Force}
foreach($target in $targets){
    [IO.File]::WriteAllText((Join-Path $target 'RETIRED-ARTIFACTS.md'),
        'Superseded preview run. Reports, JPEG/PNG images, logs and test sources are retained. Reproducible model/scene/process files were retired after installed validation. See FreeShip-Tests/maintenance/preview-image-installed-20261105.json and preview-retired-intermediates-20261105.json.',[Text.UTF8Encoding]::new($false))
}
$result=@{created_utc=[DateTime]::UtcNow.ToString('o');retired=$files;bytes=($files | ForEach-Object {$_.bytes} | Measure-Object -Sum).Sum;kept='All reports, images, logs, source, final runs and installed/previous packages'}
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receipt -Encoding UTF8
Write-Output ('Reclaimed MiB: '+[math]::Round($result.bytes/1MB,2))
