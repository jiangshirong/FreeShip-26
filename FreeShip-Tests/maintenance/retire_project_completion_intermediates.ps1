# Retire only regenerated geometry/images in this delivery's superseded runs.
# Keep reports, logs, inputs and the final/installed evidence. No link traversal.
param([string]$Root='C:\Users\12968\FreeShip26-TestResults\project-completion-20261109')
$ErrorActionPreference='Stop'
$taskRoot=(Resolve-Path -LiteralPath $Root).Path.TrimEnd('\')
if ($taskRoot -ne 'C:\Users\12968\FreeShip26-TestResults\project-completion-20261109') { throw 'Unexpected output root' }
$taskRetired=@(); $taskBytes=[long]0
foreach ($taskName in @('focused','focused-02','focused-03','final')) {
    $taskTarget=[IO.Path]::GetFullPath((Join-Path $taskRoot $taskName))
    if ([IO.Path]::GetDirectoryName($taskTarget) -ne $taskRoot) { throw 'Target outside run root' }
    $taskPending=[Collections.Generic.Queue[string]]::new();$taskPending.Enqueue($taskTarget)
    while ($taskPending.Count) {
        $taskDirectory=$taskPending.Dequeue();$taskNode=Get-Item -LiteralPath $taskDirectory -Force
        if ($taskNode.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Refusing to traverse a link' }
        foreach ($taskFile in Get-ChildItem -LiteralPath $taskDirectory -Force) {
            if ($taskFile.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
            if (!$taskFile.FullName.StartsWith($taskTarget+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Path escaped target' }
            if ($taskFile.PSIsContainer) { $taskPending.Enqueue($taskFile.FullName);continue }
            if ($taskFile.Extension -notin @('.png','.bmp','.fslp','.dxf') -and $taskFile.Name -notin @('surface.json','native-input.json')) { continue }
            $taskBytes+=$taskFile.Length
            $taskRetired+=@{path=$taskFile.FullName;bytes=$taskFile.Length}
            Remove-Item -LiteralPath $taskFile.FullName -Force
        }
    }
}
$taskReceipt=@{reason='Superseded local reproduction runs; retained reports/logs/inputs and complete final-02, regression, installed evidence';logical_bytes_removed=$taskBytes;files=$taskRetired}
$taskReceipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'project-completion-intermediates-retired-20261109.json') -Encoding utf8
Write-Output "Removed $($taskRetired.Count) regenerable files, $([math]::Round($taskBytes/1MB,2)) MiB."
