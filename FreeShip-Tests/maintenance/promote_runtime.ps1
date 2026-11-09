# Local reversible package promotion. Not a regression test or automatic updater.
param(
    [Parameter(Mandatory=$true)][string]$Candidate,
    [Parameter(Mandatory=$true)][string]$DeliveryName
)
$ErrorActionPreference='Stop'
if($DeliveryName -notmatch '^[a-z0-9-]+$'){throw 'Delivery name must be a simple directory suffix'}
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..')).TrimEnd('\')
$qt=Join-Path $workspace 'FreeShip-Qt'
$source=(Resolve-Path -LiteralPath $Candidate).Path.TrimEnd('\')
$dest=Join-Path $qt 'dist'
$backup=Join-Path $qt ('build\dist-before-'+$DeliveryName)
$stage=Join-Path $qt ('build\install-stage-'+$DeliveryName)
foreach($root in @($qt,(Join-Path $qt 'build'),$source,$dest)){
    $node=Get-Item -LiteralPath $root
    if(-not $node.PSIsContainer -or ($node.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Expected plain installation directory'}
}
foreach($target in @($stage,$backup)){
    $resolved=[IO.Path]::GetFullPath($target)
    if([IO.Path]::GetDirectoryName($resolved) -ne (Join-Path $qt 'build') -or (Test-Path -LiteralPath $resolved)){throw 'Unexpected or occupied staging/backup path'}
}
if([IO.Path]::GetFullPath($dest) -ne (Join-Path $qt 'dist')){throw 'Unexpected installation target'}
function Check-Idle {
    foreach($process in Get-CimInstance Win32_Process -Filter "Name = 'FreeShipQt.exe'"){
        if($process.ExecutablePath -and $process.ExecutablePath.StartsWith($qt+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Save work and close the Qt application before promotion'}
    }
}
Check-Idle
if(@(Get-ChildItem -LiteralPath $source -Recurse -Force -Attributes ReparsePoint).Count){throw 'Candidate contains a link'}
Copy-Item -LiteralPath $source -Destination $stage -Recurse
$manifest=Get-Content -LiteralPath (Join-Path $stage 'build-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$expected=@{}
foreach($property in $manifest.binaries_sha256.PSObject.Properties){
    $file=[IO.Path]::GetFullPath((Join-Path $stage $property.Name))
    if(-not $file.StartsWith($stage+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Manifest file outside stage'}
    if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $property.Value){throw "Staging hash mismatch: $($property.Name)"}
    $expected[$file.ToLowerInvariant()]=$true
}
foreach($file in Get-ChildItem -LiteralPath $stage -Recurse -File){
    if($file.FullName -ne (Join-Path $stage 'build-manifest.json') -and -not $expected.ContainsKey($file.FullName.ToLowerInvariant())){throw 'Unmanifested staging file'}
}
Check-Idle
# Absolute source/destination targets above are restricted to the Qt installation
# and its build directory. Both moves stay on D; the previous package is retained.
Move-Item -LiteralPath $dest -Destination $backup
try{Move-Item -LiteralPath $stage -Destination $dest}
catch{if(-not(Test-Path -LiteralPath $dest)){Move-Item -LiteralPath $backup -Destination $dest};throw}
Write-Output "Installed: $dest"
Write-Output "Previous package: $backup"
