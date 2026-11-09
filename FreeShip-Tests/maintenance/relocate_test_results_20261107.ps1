# Move verified historical test outputs, keeping their original paths usable.
$ErrorActionPreference='Stop'
$root='D:\FreeShip26Tests\reports'
$destination=Join-Path $env:USERPROFILE 'FreeShip26-TestResults\relocated-20261107'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
$records=@()
foreach($name in @('resistance-native-final-02','cad-native-core-01','preview-complete-20261105','publication-migration-02','cad-native-migration-01','plates-native-migration-03','plates-native-migration-02','legacy-exchange-delivery-publication','legacy-exchange-publication-final','legacy-exchange-publication','curve-publication-01','publication-installed-02','preferences-ui-publication-02','preferences-ui-publication-01','preferences-publication-02','model-ui-publication-03','plates-native-final-01','plates-native-final-03','plates-native-final-02','plates-native-workflow-03','resistance-native-hydro-01','preview-safety-20261105','plates-native-workflow-02','publication-core-02','publication-core-03','model-ui-final-03','publication-installed-01','model-ui-installed-01','publication-core-01','plates-native-workflow-01')){
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
 $records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'test-results-relocated-20261107.json') -Encoding UTF8
 Write-Output ("Moved {0}: {1} GiB" -f $name,[math]::Round($bytes/1GB,2))
}
Get-PSDrive C,D | Select-Object Name,Free
