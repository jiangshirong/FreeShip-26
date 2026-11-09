param(
    [Parameter(Mandatory = $true)][string]$BridgeBuild,
    [string]$BuildDir,
    [string]$DeployDir,
    [string]$WorkspaceRoot,
    [string]$LegacySource,
    [string]$SolverDirectory
)
$ErrorActionPreference = 'Stop'
function Get-Sha256([string]$Path) {
    $hasher = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try { return ([BitConverter]::ToString($hasher.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $stream.Dispose(); $hasher.Dispose() }
}
$qtPackageRoot = [IO.Path]::GetFullPath($PSScriptRoot)
if (!$WorkspaceRoot) { $WorkspaceRoot = Split-Path $qtPackageRoot -Parent }
$WorkspaceRoot = [IO.Path]::GetFullPath($WorkspaceRoot)
if (!$LegacySource) { $LegacySource = Join-Path $WorkspaceRoot 'FreeShip-Plus-Lazarus' }
$LegacySource = [IO.Path]::GetFullPath($LegacySource)
$BridgeBuild = [IO.Path]::GetFullPath($BridgeBuild)
if (!$DeployDir) { $DeployDir = Join-Path $qtPackageRoot 'dist' }
$DeployDir = [IO.Path]::GetFullPath($DeployDir).TrimEnd('\')
foreach ($protected in @($LegacySource, (Join-Path $WorkspaceRoot 'FreeShip-QtPilot'))) {
    if ($DeployDir -eq $protected -or $DeployDir.StartsWith($protected.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "The old application and accepted pilot cannot be deployment targets: $DeployDir"
    }
}
if (Test-Path -LiteralPath $DeployDir) {
    throw "Use a new deployment directory, then explicitly promote it after verification: $DeployDir"
}
$bridgeReportPath = Join-Path $BridgeBuild 'report.json'
$bridgeReport = Get-Content -LiteralPath $bridgeReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
function Assert-BridgeBuild {
    if ($bridgeReport.status -ne 'pass' -or $bridgeReport.reference) { throw 'A successful model-service build is required.' }
    $bridgeExe = Join-Path $BridgeBuild 'FreeShipGeometryBridge.exe'
    if ((Get-Sha256 $bridgeExe) -ne $bridgeReport.build.executable_sha256) {
        throw 'Bridge executable no longer matches its build report.'
    }
    foreach ($property in $bridgeReport.adapter_sha256.PSObject.Properties) {
        $path = Join-Path $WorkspaceRoot $property.Name
        if ((Get-Sha256 $path) -ne $property.Value) {
            throw "Bridge adapter changed since compilation: $($property.Name)"
        }
    }
    $sourceHash = & python -c 'import sys; from pathlib import Path; sys.path.insert(0,sys.argv[1]); import freeship_ui_test as c; print(c.fingerprint(Path(sys.argv[2]))[0])' (Join-Path $WorkspaceRoot 'FreeShip-Tests') $LegacySource
    if ($LASTEXITCODE -or $sourceHash.Trim() -ne $bridgeReport.source_sha256) { throw 'Legacy source does not match the bridge build.' }
    foreach ($required in @('Themes', 'Languages', 'locale', 'legacy-source.zip', 'source-id.txt')) {
        if (!(Test-Path -LiteralPath (Join-Path $BridgeBuild $required))) { throw "Bridge resource missing: $required" }
    }
}
Assert-BridgeBuild
$destinationParent = Split-Path $DeployDir -Parent
if (!(Test-Path -LiteralPath $destinationParent)) { New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null }
$stage = Join-Path $destinationParent ('.package-' + [Guid]::NewGuid().ToString('N'))
& (Join-Path $qtPackageRoot 'build.ps1') -Deploy -BuildDir $BuildDir -DeployDir $stage -WorkspaceRoot $WorkspaceRoot
Assert-BridgeBuild
New-Item -ItemType Directory -Path (Join-Path $stage 'bridge') | Out-Null
foreach ($name in @('FreeShipGeometryBridge.exe', 'Themes', 'Languages', 'locale', 'legacy-source.zip', 'source-id.txt', 'report.json')) {
    Copy-Item -LiteralPath (Join-Path $BridgeBuild $name) -Destination (Join-Path $stage 'bridge') -Recurse
}
foreach ($name in @('COPYING', 'NOTICE.md', 'README.md')) {
    Copy-Item -LiteralPath (Join-Path $qtPackageRoot $name) -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $qtPackageRoot 'docs') -Destination $stage -Recurse
Copy-Item -LiteralPath (Join-Path $qtPackageRoot 'third_party') -Destination $stage -Recurse
$manifestFile = Join-Path $stage 'build-manifest.json'
if ($SolverDirectory) {
    $solverPath = (Resolve-Path -LiteralPath $SolverDirectory).Path
    if (!(Test-Path -LiteralPath $solverPath -PathType Container)) { throw 'Calculation program directory is missing.' }
    # A local reference only. Third-party solver binaries are not redistributed.
    $solverConfig = [ordered]@{ directory = $solverPath; scope = 'Existing local calculation tools; binaries and their licenses are separate.' }
    [IO.File]::WriteAllText((Join-Path $stage 'calculation-tools.json'), ($solverConfig | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
}
$manifest = Get-Content -LiteralPath $manifestFile -Raw -Encoding UTF8 | ConvertFrom-Json
$binaryHashes = [ordered]@{}
foreach ($file in (Get-ChildItem -LiteralPath $stage -File -Recurse | Where-Object { $_.Name -ne 'build-manifest.json' } | Sort-Object FullName)) {
    $relative = $file.FullName.Substring($stage.Length + 1).Replace('\', '/')
    $binaryHashes[$relative] = Get-Sha256 $file.FullName
}
$manifest.binaries_sha256 = $binaryHashes
$manifest | Add-Member -NotePropertyName legacy_source_sha256 -NotePropertyValue $bridgeReport.source_sha256
$manifest | Add-Member -NotePropertyName bridge_build -NotePropertyValue $BridgeBuild
$manifest | Add-Member -NotePropertyName package_script_sha256 -NotePropertyValue (Get-Sha256 $PSCommandPath)
$manifest | Add-Member -NotePropertyName package_scope -NotePropertyValue 'Local development build; public release compliance and native desktop acceptance remain pending.'
[IO.File]::WriteAllText($manifestFile, ($manifest | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
# Both absolute paths are fixed and verified before the directory move. Neither
# staging is a sibling of the explicitly selected destination (including C).
$resolvedStage = (Resolve-Path -LiteralPath $stage).Path
if ([IO.Path]::GetDirectoryName($resolvedStage) -ne $destinationParent -or (Get-Item -LiteralPath $resolvedStage).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Unexpected staging directory.' }
Move-Item -LiteralPath $resolvedStage -Destination $DeployDir
Write-Output "Complete local runtime: $DeployDir"
