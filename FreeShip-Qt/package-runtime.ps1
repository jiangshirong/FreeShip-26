# Assemble a verified native runtime without compiling or touching the old app.
param(
    [Parameter(Mandatory=$true)][string]$FromRuntime,
    [Parameter(Mandatory=$true)][string]$DeployDir,
    [switch]$KeepLocalSolverReference
)
$ErrorActionPreference='Stop'
$runtimePackageArgs=@((Join-Path $PSScriptRoot 'tools/package_runtime.py'),'--source',[IO.Path]::GetFullPath($FromRuntime),'--output',[IO.Path]::GetFullPath($DeployDir))
if($KeepLocalSolverReference){$runtimePackageArgs+='--local-solvers'}
& python @runtimePackageArgs
if($LASTEXITCODE){throw 'Native runtime packaging failed; the source runtime was not modified.'}
