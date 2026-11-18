param(
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [Parameter(Mandatory=$true)][string]$RuntimeDir,
    [string]$WorkspaceRoot,
    [ValidateSet('native_geometry_tests', 'native_subdivision_tests', 'native_sections_tests')]
    [string]$Target = 'native_geometry_tests'
)
$ErrorActionPreference = 'Stop'
$nativeSourceRoot = [IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\')
if (!$WorkspaceRoot) { $WorkspaceRoot = Split-Path $nativeSourceRoot -Parent }
$WorkspaceRoot = [IO.Path]::GetFullPath($WorkspaceRoot).TrimEnd('\')
$BuildDir = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\')
$RuntimeDir = [IO.Path]::GetFullPath($RuntimeDir).TrimEnd('\')

function Get-NativeHash([string]$Path) {
    $hasher = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try { return ([BitConverter]::ToString($hasher.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $stream.Dispose(); $hasher.Dispose() }
}
function Get-NativeSourceHashes {
    $hashes = [ordered]@{}
    $files = @(Get-ChildItem -LiteralPath (Join-Path $nativeSourceRoot 'src\native') -File -Recurse)
    $files += Get-Item -LiteralPath (Join-Path $nativeSourceRoot 'CMakeLists.txt'), (Join-Path $nativeSourceRoot 'build-native.ps1')
    foreach ($file in ($files | Sort-Object FullName)) {
        $hashes[$file.FullName.Substring($nativeSourceRoot.Length + 1).Replace('\', '/')] = Get-NativeHash $file.FullName
    }
    return $hashes
}
function Get-NativeAlias([string]$Target, [string]$Label) {
    $resolvedTarget = [IO.Path]::GetFullPath($Target).TrimEnd('\')
    if (!(Test-Path -LiteralPath $resolvedTarget -PathType Container)) { throw "Missing $Label directory: $resolvedTarget" }
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { $key = ([BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($resolvedTarget.ToLowerInvariant())))).Replace('-', '').Substring(0, 12) }
    finally { $hasher.Dispose() }
    $alias = Join-Path $env:TEMP "FreeShip26Native-$Label-$key"
    if (!(Test-Path -LiteralPath $alias)) { New-Item -ItemType Junction -Path $alias -Target $resolvedTarget | Out-Null }
    $item = Get-Item -LiteralPath $alias
    if ($item.LinkType -ne 'Junction' -or @($item.Target).Count -ne 1 -or
        [IO.Path]::GetFullPath([string]$item.Target).TrimEnd('\') -ne $resolvedTarget) { throw "Unexpected build alias: $alias" }
    return $alias
}

# No runtime is ever installed by this validation builder. Use private new
# directories so a failed compile cannot replace an accepted application.
foreach ($candidate in @($BuildDir, $RuntimeDir)) {
    foreach ($protected in @((Join-Path $WorkspaceRoot 'FreeShip-Plus-Lazarus'), (Join-Path $WorkspaceRoot 'FreeShip-QtPilot'),
                              (Join-Path $nativeSourceRoot 'dist'), (Join-Path $nativeSourceRoot 'src'), (Join-Path $nativeSourceRoot 'bridge'),
                              (Join-Path $WorkspaceRoot 'References'))) {
        if ($candidate -eq $protected -or $candidate.StartsWith($protected + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Protected output location: $candidate"
        }
    }
    if ($candidate -eq $nativeSourceRoot -or $candidate -eq $WorkspaceRoot) { throw 'Output must be a private child directory' }
}
if ($BuildDir -eq $RuntimeDir) { throw 'Build and runtime directories must be distinct' }
if (Test-Path -LiteralPath $RuntimeDir) { throw "Use a new runtime directory: $RuntimeDir" }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$sourceAlias = Get-NativeAlias $nativeSourceRoot 'source'
$buildAlias = Get-NativeAlias $BuildDir 'build'
$toolsAlias = Get-NativeAlias (Join-Path $WorkspaceRoot 'FreeShip-QtPilot\.tools') 'tools'
$testsRoot = Join-Path $WorkspaceRoot 'FreeShip-Tests'
$testsAlias = Get-NativeAlias $testsRoot 'tests'
$qtPrefix = Join-Path $toolsAlias 'Qt\6.8.3\mingw_64'
$compilerBin = Join-Path $env:LOCALAPPDATA 'FreeShip26QtTools\mingw1310_64\bin'
$pythonBin = Join-Path $toolsAlias 'python\Scripts'
$cmake = Join-Path $pythonBin 'cmake.exe'
if (!(Test-Path -LiteralPath "$compilerBin\g++.exe")) { throw 'Existing MinGW 13.1 toolchain is missing' }
$before = Get-NativeSourceHashes
$testRelative = "qt/$Target.cpp"
$testBefore = Get-NativeHash (Join-Path $testsRoot $testRelative)
$savedNativePath = $env:PATH
try {
    $env:PATH = "$compilerBin;$qtPrefix\bin;$pythonBin;$savedNativePath"
    $configureArgs = @('-S', $sourceAlias, '-B', $buildAlias, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
        "-DCMAKE_PREFIX_PATH=$qtPrefix", "-DCMAKE_CXX_COMPILER=$compilerBin\g++.exe",
        "-DPILOT_TEST_SOURCE=$testsAlias/qt/linesplan_tests.cpp",
        "-DMIGRATION_TEST_SOURCE=$testsAlias/qt/migration_tests.cpp",
        "-DSESSION_TEST_SOURCE=$testsAlias/qt/session_tests.cpp",
        "-DEDITING_TEST_SOURCE=$testsAlias/qt/editing_tests.cpp",
        "-DNATIVE_GEOMETRY_TEST_SOURCE=$testsAlias/qt/native_geometry_tests.cpp",
        "-DNATIVE_SUBDIVISION_TEST_SOURCE=$testsAlias/qt/native_subdivision_tests.cpp",
        "-DNATIVE_SECTIONS_TEST_SOURCE=$testsAlias/qt/native_sections_tests.cpp")
    & $cmake @configureArgs
    if ($LASTEXITCODE) { throw 'Native geometry configure failed' }
    & $cmake --build $buildAlias --target $Target --parallel 4
    if ($LASTEXITCODE) { throw 'Native geometry compilation failed' }
    $after = Get-NativeSourceHashes
    $testAfter = Get-NativeHash (Join-Path $testsRoot $testRelative)
    if (($before | ConvertTo-Json -Compress) -ne ($after | ConvertTo-Json -Compress) -or $testBefore -ne $testAfter) {
        throw 'Calculation or probe source changed while compiling; rebuild in a stable snapshot'
    }
    New-Item -ItemType Directory -Path $RuntimeDir | Out-Null
    Copy-Item -LiteralPath (Join-Path $BuildDir "$Target.exe") -Destination $RuntimeDir
    Copy-Item -LiteralPath "$qtPrefix\bin\Qt6Core.dll" -Destination $RuntimeDir
    foreach ($name in @('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
        Copy-Item -LiteralPath (Join-Path $compilerBin $name) -Destination $RuntimeDir
    }
    $binaries = [ordered]@{}
    foreach ($file in (Get-ChildItem -LiteralPath $RuntimeDir -File | Sort-Object Name)) { $binaries[$file.Name] = Get-NativeHash $file.FullName }
    $testSources = [ordered]@{}; $testSources[$testRelative] = $testAfter
    $manifest = [ordered]@{
        scope = 'Standalone calculation validation; not linked into FreeShipQt and not installed'
        target = $Target
        source_root = $nativeSourceRoot
        build_dir = $BuildDir
        configuration = 'Release'
        created_utc = [DateTime]::UtcNow.ToString('o')
        source_sha256 = $after
        test_source_sha256 = $testSources
        binaries_sha256 = $binaries
        compiler = (& "$compilerBin\g++.exe" -dumpfullversion)
        floating_point_flags = @('-fno-fast-math', '-ffp-contract=off')
    }
    [IO.File]::WriteAllText((Join-Path $RuntimeDir 'native-build-manifest.json'), ($manifest | ConvertTo-Json -Depth 7), [Text.UTF8Encoding]::new($false))
    Write-Output "Private native geometry runtime: $RuntimeDir"
} finally { $env:PATH = $savedNativePath }
