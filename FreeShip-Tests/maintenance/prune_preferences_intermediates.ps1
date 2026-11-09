# One-time retirement of this migration's superseded output, not a test.
# User authorized removal of duplicate/obsolete output while retaining final
# reports, necessary baselines and every reusable test source.
param([Parameter(Mandatory=$true)][string]$ValidatedReport)
$ErrorActionPreference='Stop'
$base='D:\FreeShip26Tests'
$validated=(Resolve-Path -LiteralPath $ValidatedReport).Path
if($validated -ne (Join-Path $base 'reports\preferences-document-02\report.json')){throw 'Expected final document report'}
$final=Get-Content -LiteralPath $validated -Raw -Encoding UTF8 | ConvertFrom-Json
if($final.counts.fail -ne 0 -or $final.counts.error -ne 0){throw 'Final document checks did not pass'}
$receipt=Join-Path $PSScriptRoot 'preferences-retired-output-20261103.json'
if(Test-Path -LiteralPath $receipt){throw 'Retirement already recorded; inspect it before rerunning'}
$relative=@('reports\preferences-core-01','reports\preferences-core-02','reports\preferences-core-03',
    'reports\preferences-document-01','reports\preferences-publication-01',
    'build\preferences-native-package-02','build\preferences-native-package-04')
$targets=@()
$links=@()
$toolchain=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\FreeShip-MCP\.toolchain\lazarus'))
foreach($name in $relative){
    $path=[IO.Path]::GetFullPath((Join-Path $base $name))
    $parent=[IO.Path]::GetDirectoryName($path)
    if($parent -notin @((Join-Path $base 'reports'),(Join-Path $base 'build'))){throw 'Target outside task output'}
    $item=Get-Item -LiteralPath $path
    if(-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Expected plain task directory'}
    if((Resolve-Path -LiteralPath $path).Path -ne $path){throw 'Unexpected resolved target'}
    foreach($link in Get-ChildItem -LiteralPath $path -Force -Recurse -Attributes ReparsePoint){
        # These two known test aliases point at the preserved compiler. Unlink
        # only the junction itself; never recursively enter its target.
        if($link.LinkType -ne 'Junction' -or $link.Name -notmatch '^fs-ui-tools-[0-9a-f]+$' -or
           @($link.Target).Count -ne 1 -or [string]$link.Target -ne $toolchain -or
           -not $link.FullName.StartsWith($path+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unknown linked descendant is not eligible'}
        $links+=@([ordered]@{path=$link.FullName;target=[string]$link.Target})
    }
    $targets+=@($path)
}
$record=[ordered]@{status='running';started=(Get-Date).ToString('o');validated_report=$validated;
    validated_report_sha256=(Get-FileHash -LiteralPath $validated -Algorithm SHA256).Hash;
    final_runtime=(Join-Path $base 'build\preferences-native-package-05');
    retained_final_reports=@('preferences-core-04','preferences-document-02','preferences-publication-02');
    free_bytes_before=(Get-PSDrive D).Free;unlinked_toolchain_aliases=$links;items=@();script_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash}
function Save-Record {[IO.File]::WriteAllText($receipt,($record | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))}
Save-Record
foreach($link in $links){
    $current=Get-Item -LiteralPath $link.path
    if($current.LinkType -ne 'Junction' -or [string]$current.Target -ne $link.target){throw 'Compiler alias changed after validation'}
    [IO.Directory]::Delete($link.path)
}
foreach($path in $targets){
    if(@(Get-ChildItem -LiteralPath $path -Force -Recurse -Attributes ReparsePoint).Count){throw 'Remaining linked descendant'}
    $entry=[ordered]@{path=$path;retained=@();logical_bytes_removed=0;status='running'}
    foreach($name in @('report.json','build-manifest.json')){
        $file=Join-Path $path $name
        if(Test-Path -LiteralPath $file){$entry.retained+=@([ordered]@{name=$name;sha256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash})}
    }
    foreach($child in Get-ChildItem -LiteralPath $path -Force){
        if($child.Name -in @('report.json','build-manifest.json')){continue}
        $target=(Resolve-Path -LiteralPath $child.FullName).Path
        if([IO.Path]::GetDirectoryName($target) -ne $path -or ($child.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Unexpected child target'}
        if($child.PSIsContainer){$entry.logical_bytes_removed+=(Get-ChildItem -LiteralPath $target -Recurse -File -Force | Measure-Object Length -Sum).Sum}
        else{$entry.logical_bytes_removed+=$child.Length}
        # Resolved absolute target and all descendants were checked above.
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    [IO.File]::WriteAllText((Join-Path $path 'RETIRED.txt'),
        "Superseded preference-migration output. Top-level report/manifest retained. Raw artifacts and private runtime retired under the user's output-retention rule. See $receipt. Final package: $($record.final_runtime).",[Text.UTF8Encoding]::new($false))
    $entry.status='retired';$record.items+=@($entry);Save-Record
}
$record.status='complete';$record.completed=(Get-Date).ToString('o');$record.free_bytes_after=(Get-PSDrive D).Free;Save-Record
Write-Output "Retained final evidence; retirement record: $receipt"
