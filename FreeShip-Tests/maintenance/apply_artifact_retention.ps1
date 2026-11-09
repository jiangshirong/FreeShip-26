param([Parameter(Mandatory=$true)][string]$Plan,[switch]$Apply)
$ErrorActionPreference='Stop'
$planPath=(Resolve-Path -LiteralPath $Plan).Path
$data=Get-Content -LiteralPath $planPath -Raw -Encoding UTF8 | ConvertFrom-Json
$expected=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\reports')).TrimEnd('\')
if($data.format -ne 'FreeShipArtifactRetentionPlan' -or $data.version -ne 1 -or $data.root -ne $expected){throw 'Unexpected plan root or format'}
$protected=@{};foreach($item in $data.kept){$protected[$item.path]=$true}
$targets=@()
foreach($item in $data.candidates){
    $target=(Resolve-Path -LiteralPath $item.path).Path.TrimEnd('\')
    if([IO.Path]::GetDirectoryName($target) -ne $expected -or $protected.ContainsKey($target)){throw "Unsafe deletion target: $target"}
    $info=Get-Item -LiteralPath $target
    if($info.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Linked target: $target"}
    if(@(Get-ChildItem -LiteralPath $target -Recurse -Force -Attributes ReparsePoint).Count){throw "Linked child: $target"}
    $report=Join-Path $target 'report.json'
    if((Get-FileHash -LiteralPath $report -Algorithm SHA256).Hash.ToLowerInvariant() -ne $item.report_sha256){throw "Report changed since plan: $target"}
    $inventory=Get-ChildItem -LiteralPath $target -Recurse -Force -File | Measure-Object Length -Sum
    if($inventory.Count -ne $item.files -or $inventory.Sum -ne $item.bytes){throw "Directory changed since plan: $target"}
    $targets+= $target
}
if(!$Apply){[pscustomobject]@{Validated=$targets.Count;ReclaimGiB=[Math]::Round($data.reclaim_bytes/1GB,2);Applied=$false}|ConvertTo-Json;return}
$before=(Get-PSDrive -Name D).Free
$removed=@()
try{
    foreach($target in $targets){Remove-Item -LiteralPath $target -Recurse -Force;$removed+=$target}
}finally{
    $after=(Get-PSDrive -Name D).Free
    [pscustomobject]@{Plan=$planPath;Removed=$removed;RemovedCount=$removed.Count;FreeBefore=$before;FreeAfter=$after;FreedGiB=[Math]::Round(($after-$before)/1GB,2)} |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath ($planPath+'.result.json') -Encoding UTF8
}
Get-Content -LiteralPath ($planPath+'.result.json') -Raw | ConvertFrom-Json | Select-Object RemovedCount,FreedGiB,FreeAfter | ConvertTo-Json
