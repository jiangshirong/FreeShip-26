param(
    [string]$FromRuntime=(Join-Path $PSScriptRoot 'dist'),
    [Parameter(Mandatory=$true)][string]$OutputDir,
    [string]$Version='0.1.2',
    [string]$WixDirectory=(Join-Path $env:LOCALAPPDATA 'FreeShip26QtTools\wix314')
)
$ErrorActionPreference='Stop'
& python -B (Join-Path $PSScriptRoot 'tools/build_installer.py') --runtime $FromRuntime --output $OutputDir --version $Version --wix $WixDirectory
if($LASTEXITCODE){throw 'Installer build failed. The source runtime was not modified.'}
