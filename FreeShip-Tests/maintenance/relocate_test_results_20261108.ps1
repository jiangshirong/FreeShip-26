# Move verified historical test outputs, keeping their original paths usable.
$ErrorActionPreference='Stop'
$root='D:\桌面文件夹\项目\FreeShip_26\FreeShip-Tests\reports'
$destination=Join-Path $env:USERPROFILE 'FreeShip26-TestResults\relocated-20261108'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
$records=@()
foreach($name in @('qt-migration-20261101-c','qt-migration-m2-20261101-c','qt-migration-20261101-d','qt-editing-20261101-d','qt-migration-20261101-b')){
 $source=[IO.Path]::GetFullPath((Join-Path $root $name))
 $target=[IO.Path]::GetFullPath((Join-Path $destination $name))
 $retired=$source+'.relocating'
 if([IO.Path]::GetDirectoryName($source) -ne $root -or [IO.Path]::GetDirectoryName($retired) -ne $root -or [IO.Path]::GetDirectoryName($target) -ne $destination){throw 'Path escaped approved roots'}
 if((Get-Item -LiteralPath $source).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Source is a link'}
 if(Test-Path -LiteralPath $target){throw 'Destination already exists'}
 if(Test-Path -LiteralPath $retired){throw 'Staging name already exists'}
 if(@(Get-ChildItem -LiteralPath $source -Recurse -Force -Attributes ReparsePoint).Count){throw 'Source contains links'}
 & robocopy $source $target /E /COPY:DAT /DCOPY:DAT /R:1 /W:1 /XJ /NFL /NDL /NJH /NJS /NP | Out-Null
 if($LASTEXITCODE -ge 8){throw 'Copy failed'}
 $files=@(Get-ChildItem -LiteralPath $source -Recurse -Force -File);$bytes=0
 foreach($file in $files){
  $relative=$file.FullName.Substring($source.Length+1);$copy=Join-Path $target $relative
  if((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash){throw "Hash mismatch: $relative"}
  $bytes+=$file.Length
 }
 if(@(Get-ChildItem -LiteralPath $target -Recurse -Force -File).Count -ne $files.Count){throw 'File count mismatch'}
 Move-Item -LiteralPath $source -Destination $retired
 try{New-Item -ItemType Junction -Path $source -Target $target | Out-Null}catch{Move-Item -LiteralPath $retired -Destination $source;throw}
 if(-not ((Get-Item -LiteralPath $source).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Link missing'}
 if(@(Get-ChildItem -LiteralPath $source -Recurse -Force -File).Count -ne $files.Count){throw 'Original path inaccessible'}
 # The checked, same-volume retired path contains no links. C retains every
 # file, verified above; only this duplicate tree is removed.
 Remove-Item -LiteralPath $retired -Recurse -Force
 $records+=@{source=$source;destination=$target;files=$files.Count;bytes=$bytes;sha256_verified=$true}
 $records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'test-results-relocated-20261108.json') -Encoding UTF8
 Write-Output ("Moved {0}: {1} GiB" -f $name,[math]::Round($bytes/1GB,2))
}
Get-PSDrive C,D | Select-Object Name,Free
