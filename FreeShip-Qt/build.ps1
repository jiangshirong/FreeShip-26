param(
    [switch]$Deploy,
    [string]$BuildDir,
    [string]$DeployDir,
    [string]$WorkspaceRoot
)
$ErrorActionPreference = 'Stop'
$qtSourceRoot = [IO.Path]::GetFullPath($PSScriptRoot)
if (!$WorkspaceRoot) { $WorkspaceRoot = Split-Path $qtSourceRoot -Parent }
$WorkspaceRoot = [IO.Path]::GetFullPath($WorkspaceRoot)
if (!$BuildDir) { $BuildDir = Join-Path $qtSourceRoot 'build\qt' }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)

function Get-Sha256([string]$Path) {
    $hasher = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try { return ([BitConverter]::ToString($hasher.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $stream.Dispose(); $hasher.Dispose() }
}
function Get-SourceHashes {
    $hashes = [ordered]@{}
    $files = @(Get-ChildItem -LiteralPath (Join-Path $qtSourceRoot 'src'), (Join-Path $qtSourceRoot 'bridge'), (Join-Path $qtSourceRoot 'third_party') -File -Recurse |
        Where-Object { $_.Extension -in @('.h', '.cpp', '.pas', '.inc', '.py', '.json') -and $_.FullName -notmatch '[\\/]__pycache__[\\/]' })
    $files += Get-Item -LiteralPath (Join-Path $qtSourceRoot 'CMakeLists.txt'), (Join-Path $qtSourceRoot 'build.ps1')
    foreach ($file in ($files | Sort-Object FullName)) {
        $relative = $file.FullName.Substring($qtSourceRoot.Length + 1).Replace('\', '/')
        $hashes[$relative] = Get-Sha256 $file.FullName
    }
    return $hashes
}
function Get-TestHashes {
    $hashes = [ordered]@{}
    foreach ($name in @('qt/linesplan_tests.cpp', 'qt/migration_tests.cpp', 'qt/session_tests.cpp', 'qt/editing_tests.cpp', 'qt/overlay_tests.cpp', 'qt/background_tests.cpp', 'qt/display_tests.cpp', 'qt/auxiliary_tests.cpp','qt/exchange_tests.cpp','qt/fbm_codec_tests.cpp','qt/face_tools_probe.cpp','qt/project_edit_probe.cpp','qt/document_create_probe.cpp','qt/definition_edit_probe.cpp','qt/auxiliary_edit_probe.cpp','qt/background_edit_probe.cpp','qt/analysis_edit_probe.cpp','qt/flowline_probe.cpp','qt/model_ui_tests.cpp','qt/camera_lighting_tests.cpp','qt/shading_tests.cpp','qt/added_mass_probe.cpp','qt/grid_preview_tests.cpp','qt/recovery_tests.cpp','qt/save_protection_tests.cpp','qt/dialog_layout_tests.cpp','qt/preview_image_tests.cpp','qt/underwater_tests.cpp','qt/calculation_chart_tests.cpp')) {
        $hashes[$name] = Get-Sha256 (Join-Path $testsAlias $name)
    }
    $hashes['qt/native_subdivision_tests.cpp'] = Get-Sha256 (Join-Path $testsAlias 'qt/native_subdivision_tests.cpp')
    return $hashes
}

function Get-BuildAlias([string]$Target, [string]$Label) {
    $resolvedTarget = [IO.Path]::GetFullPath($Target).TrimEnd('\')
    if (!(Test-Path -LiteralPath $resolvedTarget -PathType Container)) {
        throw "Missing $Label directory: $resolvedTarget"
    }
    $hasher = [Security.Cryptography.SHA256]::Create()
    try {
        $hash = ([BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($resolvedTarget.ToLowerInvariant())))).Replace('-', '').Substring(0, 12)
    } finally { $hasher.Dispose() }
    $alias = Join-Path $env:TEMP "FreeShip26Qt-$Label-$hash"
    if (!(Test-Path -LiteralPath $alias)) {
        New-Item -ItemType Junction -Path $alias -Target $resolvedTarget | Out-Null
    }
    $actual = Get-Item -LiteralPath $alias
    if ($actual.LinkType -ne 'Junction' -or @($actual.Target).Count -ne 1 -or
        [IO.Path]::GetFullPath([string]$actual.Target).TrimEnd('\') -ne $resolvedTarget) {
        throw "Build alias has an unexpected target: $alias"
    }
    return $alias
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$sourceAlias = Get-BuildAlias $qtSourceRoot 'source'
$buildAlias = Get-BuildAlias $BuildDir 'build'
$toolsAlias = Get-BuildAlias (Join-Path $WorkspaceRoot 'FreeShip-QtPilot\.tools') 'tools'
$testsAlias = Get-BuildAlias (Join-Path $WorkspaceRoot 'FreeShip-Tests') 'tests'
$qtPrefix = Join-Path $toolsAlias 'Qt\6.8.3\mingw_64'
$compilerBin = Join-Path $env:LOCALAPPDATA 'FreeShip26QtTools\mingw1310_64\bin'
$pythonBin = Join-Path $toolsAlias 'python\Scripts'
$cmake = Join-Path $pythonBin 'cmake.exe'
if (!(Test-Path -LiteralPath "$compilerBin\g++.exe")) { throw 'MinGW 13.1 is not installed in the existing toolchain directory.' }
$savedPath = $env:PATH
$sourcesBefore = Get-SourceHashes
$testsBefore = Get-TestHashes
try {
    $env:PATH = "$compilerBin;$qtPrefix\bin;$pythonBin;$savedPath"
    & $cmake -S $sourceAlias -B $buildAlias -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_PREFIX_PATH=$qtPrefix" "-DCMAKE_CXX_COMPILER=$compilerBin\g++.exe" "-DPILOT_TEST_SOURCE=$testsAlias/qt/linesplan_tests.cpp" "-DMIGRATION_TEST_SOURCE=$testsAlias/qt/migration_tests.cpp" "-DSESSION_TEST_SOURCE=$testsAlias/qt/session_tests.cpp" "-DEDITING_TEST_SOURCE=$testsAlias/qt/editing_tests.cpp" "-DDIALOG_LAYOUT_TEST_SOURCE=$testsAlias/qt/dialog_layout_tests.cpp" "-DOVERLAY_TEST_SOURCE=$testsAlias/qt/overlay_tests.cpp" "-DBACKGROUND_TEST_SOURCE=$testsAlias/qt/background_tests.cpp" "-DDISPLAY_TEST_SOURCE=$testsAlias/qt/display_tests.cpp" "-DAUXILIARY_TEST_SOURCE=$testsAlias/qt/auxiliary_tests.cpp" "-DEXCHANGE_TEST_SOURCE=$testsAlias/qt/exchange_tests.cpp" "-DDOCUMENT_CODEC_TEST_SOURCE=$testsAlias/qt/fbm_codec_tests.cpp" "-DNATIVE_GEOMETRY_TEST_SOURCE=$testsAlias/qt/native_geometry_tests.cpp" "-DNATIVE_SUBDIVISION_TEST_SOURCE=$testsAlias/qt/native_subdivision_tests.cpp" "-DNATIVE_SECTIONS_TEST_SOURCE=$testsAlias/qt/native_sections_tests.cpp" "-DLACKENBY_PROBE_SOURCE=$testsAlias/qt/lackenby_probe.cpp" "-DFACE_TOOLS_PROBE_SOURCE=$testsAlias/qt/face_tools_probe.cpp" "-DPROJECT_EDIT_PROBE_SOURCE=$testsAlias/qt/project_edit_probe.cpp" "-DDOCUMENT_CREATE_PROBE_SOURCE=$testsAlias/qt/document_create_probe.cpp" "-DDEFINITION_EDIT_PROBE_SOURCE=$testsAlias/qt/definition_edit_probe.cpp" "-DAUXILIARY_EDIT_PROBE_SOURCE=$testsAlias/qt/auxiliary_edit_probe.cpp" "-DBACKGROUND_EDIT_PROBE_SOURCE=$testsAlias/qt/background_edit_probe.cpp" "-DANALYSIS_EDIT_PROBE_SOURCE=$testsAlias/qt/analysis_edit_probe.cpp" "-DFLOWLINE_PROBE_SOURCE=$testsAlias/qt/flowline_probe.cpp" "-DMODEL_UI_TEST_SOURCE=$testsAlias/qt/model_ui_tests.cpp" "-DCAMERA_LIGHTING_TEST_SOURCE=$testsAlias/qt/camera_lighting_tests.cpp" "-DSHADING_TEST_SOURCE=$testsAlias/qt/shading_tests.cpp"
    if ($LASTEXITCODE) { throw 'CMake configure failed' }
    & $cmake --build $buildAlias --parallel 4
    if ($LASTEXITCODE) { throw 'C++ compilation failed' }
    $sources = Get-SourceHashes
    $testSources = Get-TestHashes
    if (($sourcesBefore | ConvertTo-Json -Compress) -ne ($sources | ConvertTo-Json -Compress) -or
        ($testsBefore | ConvertTo-Json -Compress) -ne ($testSources | ConvertTo-Json -Compress)) {
        throw 'Source changed during compilation; rebuild before using or deploying these binaries.'
    }
    if ($Deploy) {
        if (!$DeployDir) { $DeployDir = Join-Path $qtSourceRoot 'dist' }
        $DeployDir = [IO.Path]::GetFullPath($DeployDir)
        New-Item -ItemType Directory -Force -Path $DeployDir | Out-Null
        foreach ($name in @('FreeShipQt.exe', 'FreeShipNativeLinesplan.exe', 'FreeShipGeometryExport.exe', 'FreeShipDocumentService.exe', 'qt_document_codec_tests.exe', 'native_face_tools_probe.exe', 'native_project_edit_probe.exe', 'native_document_create_probe.exe', 'native_definition_edit_probe.exe', 'native_auxiliary_edit_probe.exe', 'native_background_edit_probe.exe', 'native_analysis_edit_probe.exe', 'native_flowline_probe.exe', 'qt_lines_tests.exe', 'qt_migration_tests.exe', 'qt_session_tests.exe', 'qt_editing_tests.exe', 'qt_overlay_tests.exe', 'qt_background_tests.exe', 'qt_display_tests.exe', 'qt_auxiliary_tests.exe','qt_exchange_tests.exe','qt_model_ui_tests.exe','qt_camera_lighting_tests.exe','qt_shading_tests.exe','qt_added_mass_probe.exe','qt_grid_preview_tests.exe','qt_recovery_tests.exe','qt_save_protection_tests.exe','qt_dialog_layout_tests.exe','qt_preview_image_tests.exe','qt_underwater_tests.exe','qt_calculation_chart_tests.exe')) {
            Copy-Item -LiteralPath (Join-Path $BuildDir $name) -Destination $DeployDir
        }
        Copy-Item -LiteralPath (Join-Path $BuildDir 'native_subdivision_tests.exe') -Destination $DeployDir
        & "$qtPrefix\bin\windeployqt.exe" --release --no-translations --no-opengl-sw --compiler-runtime "$DeployDir\FreeShipQt.exe"
        if ($LASTEXITCODE) { throw 'Qt runtime deployment failed' }
        Copy-Item -LiteralPath "$qtPrefix\plugins\platforms\qoffscreen.dll" -Destination "$DeployDir\platforms"
        $binaries = [ordered]@{}
        foreach ($file in (Get-ChildItem -LiteralPath $DeployDir -File -Recurse | Where-Object { $_.Name -ne 'build-manifest.json' } | Sort-Object FullName)) {
            $relative = $file.FullName.Substring($DeployDir.Length + 1).Replace('\', '/')
            $binaries[$relative] = Get-Sha256 $file.FullName
        }
        $manifest = [ordered]@{
            source_root = $qtSourceRoot
            build_dir = $BuildDir
            configuration = 'Release'
            created_utc = [DateTime]::UtcNow.ToString('o')
            source_sha256 = $sources
            test_source_sha256 = $testSources
            binaries_sha256 = $binaries
        }
        $json = $manifest | ConvertTo-Json -Depth 6
        [IO.File]::WriteAllText((Join-Path $DeployDir 'build-manifest.json'), $json, [Text.UTF8Encoding]::new($false))
    }
} finally { $env:PATH = $savedPath }
