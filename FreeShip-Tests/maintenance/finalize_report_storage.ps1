# One-time continuation of relocate_reports.ps1, 2026-11-02.
# Codex redirects LocalAppData into its package directory. Move these verified
# reports to a plain C path, and retain both historical path families as links.
# Storage maintenance only: not an application regression or scheduled cleanup.
$ErrorActionPreference='Stop'
$oldBase='C:\Users\12968\AppData\Local\Packages\OpenAI.Codex_2p2nqsd0c76g0\LocalCache\Local\FreeShip26Tests'
$oldRoot=Join-Path $oldBase 'reports'
$finalBase='C:\FreeShip26Tests'
$finalRoot=Join-Path $finalBase 'reports'
$dRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\reports')).TrimEnd('\')
$original=Get-Content -LiteralPath (Join-Path $oldBase 'relocation-20261102.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if($original.status -ne 'complete' -or $original.source_root -ne $dRoot){throw 'Unfinished or unrelated initial copy'}
foreach($root in @($oldRoot,$finalRoot,$dRoot)){
    $item=Get-Item -LiteralPath $root
    if(-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Root must be a plain directory: $root"}
}
if([IO.Path]::GetPathRoot($oldRoot) -ne [IO.Path]::GetPathRoot($finalRoot)){throw 'Final relocation must stay on one volume'}
$children=@(Get-ChildItem -LiteralPath $oldRoot -Force)
$expected=@($original.items.name)+@('qt-exchange-package-final')
if(@(Compare-Object ($children.Name | Sort-Object) ($expected | Sort-Object)).Count){throw 'Unexpected source children'}
$evidence=Join-Path $finalBase 'maintenance\relocation-20261102'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$record=[ordered]@{started=(Get-Date).ToString('o');status='running';source_root=$oldRoot;destination_root=$finalRoot;original_copy_receipt=(Join-Path $evidence 'relocation-20261102.json');d_bytes_relocated=($original.items | Measure-Object bytes -Sum).Sum;items=@()}
$receipt=Join-Path $evidence 'final-paths.json'
function Save-Receipt {[IO.File]::WriteAllText($receipt,($record | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))}
Save-Receipt
foreach($child in $children){
    $source=(Resolve-Path -LiteralPath $child.FullName).Path
    $target=Join-Path $finalRoot $child.Name
    if([IO.Path]::GetDirectoryName($source) -ne $oldRoot -or -not $child.PSIsContainer -or ($child.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Unsafe source directory'}
    if(Test-Path -LiteralPath $target){throw "Target exists: $target"}
    if(@(Get-ChildItem -LiteralPath $source -Recurse -Force -Attributes ReparsePoint).Count){throw 'Source contains a link'}
    $reports=@(Get-ChildItem -LiteralPath $source -Filter report.json -Recurse -File | ForEach-Object { [ordered]@{relative=$_.FullName.Substring($source.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash} })
    $files=@(Get-ChildItem -LiteralPath $source -Recurse -File -Force).Count
    $copied=@($original.items | Where-Object name -eq $child.Name)
    if($copied.Count -and $files -ne $copied[0].files){throw 'Initial copy file count changed'}
    # Absolute direct-child paths above were checked before this same-volume move.
    Move-Item -LiteralPath $source -Destination $target
    if(@(Get-ChildItem -LiteralPath $target -Recurse -File -Force).Count -ne $files){throw 'Moved file count mismatch'}
    foreach($report in $reports){if((Get-FileHash -LiteralPath (Join-Path $target $report.relative) -Algorithm SHA256).Hash -ne $report.sha256){throw 'Moved report hash mismatch'}}
    $dPath=Join-Path $dRoot $child.Name
    if($copied.Count){
        $link=Get-Item -LiteralPath $dPath
        if($link.LinkType -ne 'Junction' -or [string]$link.Target -ne [string]$copied[0].destination){throw 'Unexpected historical junction'}
        # Nonrecursive removal of the junction itself; never its target.
        [IO.Directory]::Delete($dPath)
        try{New-Item -ItemType Junction -Path $dPath -Target $target | Out-Null}
        catch{
            $linkError=$_
            if(-not(Test-Path -LiteralPath $dPath)){
                Move-Item -LiteralPath $target -Destination $source
                New-Item -ItemType Junction -Path $dPath -Target $copied[0].destination | Out-Null
            }
            throw $linkError
        }
        if([string](Get-Item -LiteralPath $dPath).Target -ne $target){throw 'New junction target mismatch'}
        foreach($report in $reports){if((Get-FileHash -LiteralPath (Join-Path $dPath $report.relative) -Algorithm SHA256).Hash -ne $report.sha256){throw 'Historical report path mismatch'}}
    }
    $record.items+=@([ordered]@{name=$child.Name;files=$files;destination=$target;d_junction=if($copied.Count){$dPath}else{$null};reports=$reports})
    Save-Receipt
    Write-Output "Relocated and checked $($child.Name)"
}
if(@(Get-ChildItem -LiteralPath $oldRoot -Force).Count){throw 'Old root is not empty'}
# Only remove the empty, checked directory, and preserve paths inside old JSON.
[IO.Directory]::Delete($oldRoot)
New-Item -ItemType Junction -Path $oldRoot -Target $finalRoot | Out-Null
foreach($file in Get-ChildItem -LiteralPath $oldBase -File -Force){Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $evidence $file.Name)}
$record.status='complete';$record.completed=(Get-Date).ToString('o')
$record.legacy_c_junction=$oldRoot
$record.free_bytes_after=@{C=(Get-PSDrive C).Free;D=(Get-PSDrive D).Free}
Save-Receipt
