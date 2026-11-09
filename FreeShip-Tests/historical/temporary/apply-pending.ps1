param([string]$Target64,[string]$Stage64,[string]$Backup64,[string]$Log64,[string]$OriginalHash,[string]$StageHash)
$ErrorActionPreference='Stop'
function Decode([string]$s) { [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($s)) }
$target=Decode $Target64; $stage=Decode $Stage64; $backup=Decode $Backup64; $log=Decode $Log64
function HashFile([string]$path) { $stream=[IO.File]::OpenRead($path); try { $h=[Security.Cryptography.SHA256]::Create(); try { ([BitConverter]::ToString($h.ComputeHash($stream))).Replace('-','') } finally { $h.Dispose() } } finally { $stream.Dispose() } }
function Log([string]$m) { Add-Content -LiteralPath $log -Value ((Get-Date -Format o)+' '+$m) -Encoding UTF8 }
try {
  if (-not [IO.File]::Exists($target) -or -not [IO.File]::Exists($stage)) { throw 'missing target or candidate' }
  if ((HashFile $stage) -ne $StageHash) { throw 'candidate hash mismatch' }
  $procName=[IO.Path]::GetFileNameWithoutExtension($target)
  Log 'waiting for program to exit'
  while ($true) {
    $busy=$false
    foreach ($proc in [Diagnostics.Process]::GetProcessesByName($procName)) {
      try { if (-not $proc.MainModule.FileName -or ([string]::Equals($proc.MainModule.FileName,$target,[StringComparison]::OrdinalIgnoreCase))) { $busy=$true } } catch { $busy=$true }
    }
    if ($busy) { Start-Sleep -Seconds 2; continue }
    if (-not [IO.File]::Exists($target) -or (HashFile $target) -ne $OriginalHash) { throw 'target changed while update was pending; replacement cancelled' }
    if (-not [IO.File]::Exists($stage) -or (HashFile $stage) -ne $StageHash) { throw 'candidate changed while update was pending; replacement cancelled' }
    try { [IO.File]::Replace($stage,$target,$backup,$true); break }
    catch [IO.IOException] { Log 'target busy; will retry'; Start-Sleep -Seconds 2 }
    catch [UnauthorizedAccessException] { Log 'target busy or inaccessible; will retry'; Start-Sleep -Seconds 2 }
  }
  if ((HashFile $target) -ne $StageHash) {
    try { [IO.File]::Replace($backup,$target,$null,$true); Log 'verification failed; restored previous executable' }
    catch { Log ('verification failed; backup retained at '+$backup+'; restore failed: '+$_.Exception.Message) }
    exit 1
  }
  Log 'update applied and verified; previous executable retained as backup'
} catch { Log ('update stopped: '+$_.Exception.Message); exit 1 }
