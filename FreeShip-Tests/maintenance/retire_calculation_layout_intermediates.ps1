# One-delivery maintenance, not a regression test. Only retire regenerated
# pictures/geometry and calculator exports from superseded private runs.
# Keep inputs, test summary reports and logs; final calculator exports remain.
param([string]$Root='C:\Users\12968\FreeShip26-TestResults\calculation-layout-20261109')
$ErrorActionPreference='Stop'
$taskRoot=(Resolve-Path -LiteralPath $Root).Path.TrimEnd('\')
if($taskRoot -ne 'C:\Users\12968\FreeShip26-TestResults\calculation-layout-20261109'){throw 'Unexpected output root'}
$taskFinal=Get-Content -LiteralPath (Join-Path $taskRoot 'final-04\qt-calculation-layout\report.json') -Raw -Encoding utf8 | ConvertFrom-Json
$taskInstalled=Get-Content -LiteralPath (Join-Path $taskRoot 'installed\qt-calculation-layout\report.json') -Raw -Encoding utf8 | ConvertFrom-Json
if($taskFinal.counts.fail -or $taskFinal.counts.error -or $taskInstalled.counts.fail -or $taskInstalled.counts.error){throw 'Final regression is not complete'}
$taskRetired=@();$taskBytes=[long]0
$taskReceiptPath=Join-Path $PSScriptRoot 'calculation-layout-intermediates-retired-20261109.json'
if(Test-Path -LiteralPath $taskReceiptPath){
    $taskPrevious=Get-Content -LiteralPath $taskReceiptPath -Raw -Encoding utf8 | ConvertFrom-Json
    $taskRetired=@($taskPrevious.files);$taskBytes=[long]$taskPrevious.logical_bytes_removed
}
foreach($taskName in @('layout-01','final-01','final-02','final-03')){
    $taskTarget=[IO.Path]::GetFullPath((Join-Path $taskRoot $taskName))
    if([IO.Path]::GetDirectoryName($taskTarget) -ne $taskRoot){throw 'Target outside run root'}
    foreach($taskProcess in Get-CimInstance Win32_Process){
        if($taskProcess.CommandLine -and $taskProcess.CommandLine.Contains($taskTarget)){throw 'Superseded run is still in use'}
    }
    $taskPending=[Collections.Generic.Queue[string]]::new();$taskPending.Enqueue($taskTarget)
    while($taskPending.Count){
        $taskDirectory=$taskPending.Dequeue();$taskNode=Get-Item -LiteralPath $taskDirectory -Force
        if($taskNode.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Refusing to traverse a link'}
        foreach($taskFile in Get-ChildItem -LiteralPath $taskDirectory -Force){
            if($taskFile.Attributes -band [IO.FileAttributes]::ReparsePoint){continue}
            if(!$taskFile.FullName.StartsWith($taskTarget+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Path escaped target'}
            if($taskFile.PSIsContainer){$taskPending.Enqueue($taskFile.FullName);continue}
            if($taskFile.Extension -notin @('.png','.bmp','.fslp','.dxf') -and $taskFile.Name -notin @('surface.json','native-input.json','performance-report.json','performance-report.csv','performance-report.pdf','performance-report.txt')){continue}
            $taskBytes+=$taskFile.Length;$taskRetired+=@{path=$taskFile.FullName;bytes=$taskFile.Length}
            Remove-Item -LiteralPath $taskFile.FullName -Force
        }
    }
}
$taskReceipt=@{reason='Superseded calculation-layout runs only; complete final-04 and installed evidence retained, test summaries/logs/inputs retained; previous calculator exports replaced by final evidence';logical_bytes_removed=$taskBytes;files=$taskRetired}
$taskReceipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $taskReceiptPath -Encoding utf8
Write-Output "Retired $($taskRetired.Count) generated files, $([math]::Round($taskBytes/1MB,2)) MiB."
