# One-time local CAD delivery. Not a regression test or an automatic updater.
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..')).TrimEnd('\')
$qt=Join-Path $workspace 'FreeShip-Qt'
$source='C:\FreeShip26Tests\build\cad-package-01'
$dest=Join-Path $qt 'dist'
$backup=Join-Path $qt 'build\dist-before-cad-20261102'
$stage=Join-Path $qt 'build\cad-install-stage-20261102'
foreach($root in @($qt,(Join-Path $qt 'build'),$source,$dest)){
    $node=Get-Item -LiteralPath $root
    if(-not $node.PSIsContainer -or ($node.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Expected plain installation directory'}
}
foreach($target in @($stage,$backup)){
    if([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($target)) -ne (Join-Path $qt 'build') -or (Test-Path -LiteralPath $target)){throw 'Unexpected or existing staging/backup path'}
}
function Check-Idle {
    foreach($process in Get-CimInstance Win32_Process -Filter "Name = 'FreeShipQt.exe'"){
        if($process.ExecutablePath -and $process.ExecutablePath.StartsWith($qt+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Save work and close the running Qt application before promotion'}
    }
}
Check-Idle
if(@(Get-ChildItem -LiteralPath $source -Recurse -Force -Attributes ReparsePoint).Count){throw 'Candidate contains a link'}
Copy-Item -LiteralPath $source -Destination $stage -Recurse
$manifest=Get-Content -LiteralPath (Join-Path $stage 'build-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
foreach($property in $manifest.binaries_sha256.PSObject.Properties){
    $file=[IO.Path]::GetFullPath((Join-Path $stage $property.Name))
    if(-not $file.StartsWith($stage+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Manifest file outside stage'}
    if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $property.Value){throw "Installed staging hash mismatch: $($property.Name)"}
}
Check-Idle
# All resolved absolute source/target directories were checked above. These two
# final moves stay on D, with an intact candidate and a reversible old package.
Move-Item -LiteralPath $dest -Destination $backup
try{Move-Item -LiteralPath $stage -Destination $dest}
catch{if(-not(Test-Path -LiteralPath $dest)){Move-Item -LiteralPath $backup -Destination $dest};throw}
Write-Output "Installed: $dest"
Write-Output "Previous package: $backup"
