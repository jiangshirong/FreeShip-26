# Move a completed, private test directory between the named test roots.
# Preserve absolute evidence paths with a directory junction after byte checks.
param(
    [Parameter(Mandatory=$true)][string]$Source,
    [Parameter(Mandatory=$true)][string]$Destination
)
$ErrorActionPreference='Stop'
$src=(Resolve-Path -LiteralPath $Source).Path.TrimEnd('\')
$dst=[IO.Path]::GetFullPath($Destination).TrimEnd('\')
function In-TestRoot([string]$path){
    foreach($root in @('C:\FreeShip26Tests','D:\FreeShip26Tests')){
        if($path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){return $true}
    }
    return $false
}
if(-not(In-TestRoot $src) -or -not(In-TestRoot $dst) -or $src -eq $dst){throw 'Expected two private test paths'}
if($dst.StartsWith($src+'\',[StringComparison]::OrdinalIgnoreCase) -or $src.StartsWith($dst+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Paths overlap'}
$item=Get-Item -LiteralPath $src
if(-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Source must be a plain directory'}
if(Test-Path -LiteralPath $dst){throw 'Destination already exists'}
if(@(Get-ChildItem -LiteralPath $src -Recurse -Force -Attributes ReparsePoint).Count){throw 'Source contains a link; inspect it separately'}
$files=@(Get-ChildItem -LiteralPath $src -Recurse -File -Force)
$hashes=@{}; $bytes=0L
foreach($file in $files){$relative=$file.FullName.Substring($src.Length+1);$hashes[$relative]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash;$bytes+=$file.Length}
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dst)) | Out-Null
Copy-Item -LiteralPath $src -Destination $dst -Recurse
if(@(Get-ChildItem -LiteralPath $dst -Recurse -File -Force).Count -ne $files.Count){throw 'Copied file count differs'}
foreach($relative in $hashes.Keys){
    if((Get-FileHash -LiteralPath (Join-Path $dst $relative) -Algorithm SHA256).Hash -ne $hashes[$relative]){throw "Copy differs: $relative"}
}
# Check the resolved deletion target again. Only the verified original copy is
# removed; the destination already contains every file with matching bytes.
if((Resolve-Path -LiteralPath $src).Path.TrimEnd('\') -ne $src -or -not(In-TestRoot $src) -or ((Get-Item -LiteralPath $src).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Source changed before cleanup'}
Remove-Item -LiteralPath $src -Recurse -Force
New-Item -ItemType Junction -Path $src -Target $dst | Out-Null
if([string](Get-Item -LiteralPath $src).Target -ne $dst){throw 'Historical path did not link to the destination'}
$receiptRoot='D:\FreeShip26Tests\maintenance\relocations'
[IO.Directory]::CreateDirectory($receiptRoot) | Out-Null
$receipt=Join-Path $receiptRoot ((Get-Date -Format 'yyyyMMdd-HHmmssfff')+'-'+$item.Name+'.json')
$record=[ordered]@{status='complete';source_junction=$src;destination=$dst;files=$files.Count;bytes=$bytes;sha256=$hashes;finished=(Get-Date).ToString('o')}
[IO.File]::WriteAllText($receipt,($record | ConvertTo-Json -Depth 4),[Text.UTF8Encoding]::new($false))
Write-Output ([ordered]@{source=$src;destination=$dst;files=$files.Count;bytes=$bytes;receipt=$receipt} | ConvertTo-Json -Compress)
