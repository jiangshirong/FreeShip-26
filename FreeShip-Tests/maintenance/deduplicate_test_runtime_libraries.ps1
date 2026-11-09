# Storage maintenance only. Keep every report path and byte; share identical
# third-party runtime libraries/source archives inside the C-drive test area.
# Never link application executables, user data, settings or mutable build files.
param([string]$Output='C:/FreeShip26Tests/maintenance/runtime-library-dedup-20261103.json')
$ErrorActionPreference='Stop'
$base=[IO.Path]::GetFullPath('C:/FreeShip26Tests').TrimEnd('\')
$seen=@{}; $rows=@(); $bytes=0L
$pending=[Collections.Generic.Stack[string]]::new()
foreach($name in @('build','reports')){$pending.Push((Join-Path $base $name))}
while($pending.Count){
    $dir=$pending.Pop()
    foreach($item in Get-ChildItem -LiteralPath $dir -Force){
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){continue}
        if($item.PSIsContainer){
            if($item.FullName -eq (Join-Path $base 'build\cad-qt')){continue}
            $pending.Push($item.FullName);continue
        }
        if($item.Extension -ne '.dll' -and $item.Name -ne 'legacy-source.zip'){continue}
        $path=[IO.Path]::GetFullPath($item.FullName)
        if(-not $path.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'File outside test root'}
        $hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        $key=$item.Name.ToLowerInvariant()+':'+$hash
        if(-not $seen.ContainsKey($key)){$seen[$key]=$path;continue}
        $source=$seen[$key]
        if(-not $source.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Source outside test root'}
        $backup=$path+'.dedupe-'+[Guid]::NewGuid().ToString('N')
        if([IO.Path]::GetDirectoryName($backup) -ne $item.DirectoryName){throw 'Unexpected backup path'}
        # All files stay on C. Restore the original if hard-link creation fails.
        Move-Item -LiteralPath $path -Destination $backup
        try{
            New-Item -ItemType HardLink -Path $path -Target $source | Out-Null
            if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $hash){throw 'Linked data mismatch'}
        }catch{
            if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}
            Move-Item -LiteralPath $backup -Destination $path
            throw
        }
        Remove-Item -LiteralPath $backup -Force
        $bytes+=$item.Length
        $rows+=@([ordered]@{path=$path;shared_with=$source;sha256=$hash;bytes=$item.Length})
    }
}
$outPath=[IO.Path]::GetFullPath($Output)
if(-not $outPath.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Receipt outside test root'}
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outPath)) | Out-Null
$record=[ordered]@{finished=(Get-Date).ToString('o');status='complete';kind='identical-library-hardlinks';linked_files=$rows.Count;logical_duplicate_bytes=$bytes;files=$rows;free_bytes_after=(Get-PSDrive C).Free}
[IO.File]::WriteAllText($outPath,($record | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
Write-Output ([pscustomobject]$record | Select-Object status,linked_files,logical_duplicate_bytes,free_bytes_after | ConvertTo-Json -Compress)
