# One delivery's housekeeping, not a regression. Retains all test sources,
# reports, screenshots, the final candidate and the pre-installation backup.
# Only the two superseded packages built for this model UI delivery match.
$ErrorActionPreference='Stop'
$receipt=Join-Path $PSScriptRoot 'model-ui-installed-20261104.json'
$audit=Get-Content -LiteralPath $receipt -Raw -Encoding UTF8 | ConvertFrom-Json
if($audit.counts.fail -ne 0 -or $audit.counts.error -ne 0){throw 'Delivery checks have not passed'}
$record=Join-Path $PSScriptRoot 'model-ui-retired-packages-20261104.json'
if(Test-Path -LiteralPath $record){throw 'This cleanup has already been recorded'}
$buildRoot=[IO.Path]::GetFullPath('D:\FreeShip26Tests\build').TrimEnd('\')
$final=Join-Path $buildRoot 'model-ui-package-03'
if(-not(Test-Path -LiteralPath (Join-Path $final 'build-manifest.json'))){throw 'Final package is missing'}
if((Get-FileHash -LiteralPath (Join-Path $audit.installed 'build-manifest.json') -Algorithm SHA256).Hash -ne $audit.package_manifest_sha256){throw 'Installation has changed'}
$targets=@()
foreach($index in 1..2){
    $path=[IO.Path]::GetFullPath((Join-Path $buildRoot ('model-ui-package-{0:d2}' -f $index)))
    if([IO.Path]::GetDirectoryName($path) -ne $buildRoot){throw 'Cleanup target escaped its parent'}
    $item=Get-Item -LiteralPath $path
    if(-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Expected a plain package directory'}
    if(@(Get-ChildItem -LiteralPath $path -Force -Recurse -Attributes ReparsePoint).Count){throw 'Package contains a link'}
    if(-not(Test-Path -LiteralPath (Join-Path $path 'build-manifest.json'))){throw 'Not a built package'}
    $size=(Get-ChildItem -LiteralPath $path -Recurse -File | Measure-Object Length -Sum).Sum
    $targets+=@{path=$path;bytes=$size}
}
# All resolved absolute targets were checked before any recursive removal.
foreach($entry in $targets){Remove-Item -LiteralPath $entry.path -Recurse -Force}
$result=@{created_utc=[DateTime]::UtcNow.ToString('o');retired=$targets;kept=$final;reports='All reports retained';bytes=($targets | ForEach-Object { $_.bytes } | Measure-Object -Sum).Sum}
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $record -Encoding UTF8
Write-Output ($result | ConvertTo-Json -Depth 6)
