# One delivery's housekeeping, not a regression. Keep all source, reports,
# screenshots, verified package-03, final package and installation backup.
$ErrorActionPreference='Stop'
$audit=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'viewport-installed-20261104.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if($audit.counts.fail -ne 0 -or $audit.counts.error -ne 0){throw 'Delivery checks have not passed'}
$record=Join-Path $PSScriptRoot 'viewport-retired-packages-20261104.json'
if(Test-Path -LiteralPath $record){throw 'Cleanup already recorded'}
$root=[IO.Path]::GetFullPath('C:\FreeShip26-ViewportTests').TrimEnd('\')
if((Get-Item -LiteralPath $root).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Output root is a link'}
foreach($name in 'package-03','package-final'){
    if(-not(Test-Path -LiteralPath (Join-Path $root ($name+'\build-manifest.json')))){throw 'Verified package missing'}
}
if((Get-FileHash -LiteralPath (Join-Path $audit.installed 'build-manifest.json') -Algorithm SHA256).Hash -ne $audit.package_manifest_sha256){throw 'Installation changed'}
$targets=@()
foreach($name in 'package-01','package-02'){
    $path=[IO.Path]::GetFullPath((Join-Path $root $name))
    if([IO.Path]::GetDirectoryName($path) -ne $root){throw 'Target outside output root'}
    $item=Get-Item -LiteralPath $path
    if(-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Expected plain package directory'}
    if(@(Get-ChildItem -LiteralPath $path -Force -Recurse -Attributes ReparsePoint).Count){throw 'Package contains a link'}
    if(-not(Test-Path -LiteralPath (Join-Path $path 'build-manifest.json'))){throw 'Not a built package'}
    $targets+=@{path=$path;bytes=(Get-ChildItem -LiteralPath $path -Recurse -File | Measure-Object Length -Sum).Sum}
}
# Every absolute target above is restricted to this delivery's two old packages.
foreach($entry in $targets){Remove-Item -LiteralPath $entry.path -Recurse -Force}
$result=@{created_utc=[DateTime]::UtcNow.ToString('o');retired=$targets;kept=@('package-03','package-final','all reports and screenshots','installed runtime and backup');bytes=($targets | ForEach-Object {$_.bytes} | Measure-Object -Sum).Sum}
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $record -Encoding UTF8
Write-Output ($result | ConvertTo-Json -Depth 6)
