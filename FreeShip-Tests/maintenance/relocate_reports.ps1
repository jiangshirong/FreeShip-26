param(
    [Parameter(Mandatory=$true)][string]$Plan,
    [Parameter(Mandatory=$true)][string]$Receipt
)
# Local storage maintenance, not an application regression test. Copies and
# hashes every file before replacing a report directory with a junction.
# Retains the source path used by old evidence and binary manifests.
$ErrorActionPreference='Stop'
$planData=Get-Content -LiteralPath $Plan -Raw -Encoding UTF8 | ConvertFrom-Json
$sourceRoot=(Resolve-Path -LiteralPath $planData.source_root).Path.TrimEnd('\')
$destinationRoot=[IO.Path]::GetFullPath($planData.destination_root).TrimEnd('\')
$expectedSource=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\reports')).TrimEnd('\')
if($sourceRoot -ne $expectedSource -or (Get-Item -LiteralPath $sourceRoot).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Unexpected source report root'}
if($destinationRoot -ne 'C:\Users\12968\AppData\Local\FreeShip26Tests\reports'){throw 'Unexpected destination root'}
New-Item -ItemType Directory -Path $destinationRoot -Force | Out-Null
$record=[ordered]@{started=(Get-Date).ToString('o');source_root=$sourceRoot;destination_root=$destinationRoot;status='running';items=@()}
function Save-Receipt {[IO.File]::WriteAllText([IO.Path]::GetFullPath($Receipt),($record | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))}
Save-Receipt
foreach($name in $planData.names){
    if([IO.Path]::GetFileName($name) -ne $name -or $name -match '[\\/]'){throw 'Invalid report basename'}
    $source=Join-Path $sourceRoot $name;$target=Join-Path $destinationRoot $name
    $resolved=(Resolve-Path -LiteralPath $source).Path
    if([IO.Path]::GetDirectoryName($resolved) -ne $sourceRoot){throw 'Source outside report root'}
    if(Test-Path -LiteralPath $target){throw "Destination already exists: $target"}
    $node=Get-Item -LiteralPath $source
    if(-not $node.PSIsContainer -or ($node.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Source is not a plain directory'}
    $links=@(Get-ChildItem -LiteralPath $source -Recurse -Force -Attributes ReparsePoint)
    if($links.Count){throw 'Do not relocate directories containing links'}
    $files=@(Get-ChildItem -LiteralPath $source -Recurse -File -Force)
    $bytes=($files | Measure-Object Length -Sum).Sum
    if((Get-PSDrive C).Free -lt $bytes+20GB){throw 'Reserve at least 20 GiB on C'}
    Write-Output "Copying $name ($([math]::Round($bytes/1GB,2)) GiB)"
    Copy-Item -LiteralPath $source -Destination $target -Recurse
    $hashes=@{}
    foreach($file in $files){
        $relative=$file.FullName.Substring($source.Length+1);$copy=Join-Path $target $relative
        if(-not(Test-Path -LiteralPath $copy -PathType Leaf) -or (Get-Item -LiteralPath $copy).Length -ne $file.Length){throw "Copy size mismatch: $relative"}
        $hash=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        if((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -ne $hash){throw "Copy hash mismatch: $relative"}
        $hashes[$relative]=$hash
    }
    if(@(Get-ChildItem -LiteralPath $source -Recurse -File -Force).Count -ne $files.Count -or @(Get-ChildItem -LiteralPath $target -Recurse -File -Force).Count -ne $files.Count){throw 'File count changed during copy'}
    $hashPath=Join-Path (Split-Path -Parent $Receipt) ($name+'-relocation-hashes.json')
    [IO.File]::WriteAllText($hashPath,($hashes | ConvertTo-Json -Depth 3),[Text.UTF8Encoding]::new($false))
    $holding=Join-Path $sourceRoot ('.relocating-'+$name+'-'+[Guid]::NewGuid().ToString('N'))
    if([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($holding)) -ne $sourceRoot){throw 'Invalid holding directory'}
    Move-Item -LiteralPath $source -Destination $holding
    try {
        New-Item -ItemType Junction -Path $source -Target $target | Out-Null
        $junction=Get-Item -LiteralPath $source
        if($junction.LinkType -ne 'Junction' -or [IO.Path]::GetFullPath([string]$junction.Target).TrimEnd('\') -ne $target){throw 'Junction target mismatch'}
    }catch{
        if(-not(Test-Path -LiteralPath $source)){Move-Item -LiteralPath $holding -Destination $source}
        throw
    }
    # The sole recursive removal is the verified duplicate, now under a checked
    # direct child of the report root. Never follow a link or remove the junction.
    $checked=(Resolve-Path -LiteralPath $holding).Path
    if([IO.Path]::GetDirectoryName($checked) -ne $sourceRoot -or [IO.Path]::GetFileName($checked) -notlike '.relocating-*' -or ((Get-Item -LiteralPath $checked).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Unsafe duplicate cleanup path'}
    Remove-Item -LiteralPath $checked -Recurse -Force
    $record.items+=@([ordered]@{name=$name;files=$files.Count;bytes=$bytes;destination=$target;hash_manifest=$hashPath;old_path_junction=$source;status='verified'})
    Save-Receipt;Write-Output "Verified and relocated $name"
}
$record.status='complete';$record.completed=(Get-Date).ToString('o');Save-Receipt
