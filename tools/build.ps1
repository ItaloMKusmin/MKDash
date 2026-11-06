param(
    [string]$IdfPath = 'C:\esp\v5.5.5\esp-idf',
    [string]$ToolsPath = 'C:\Espressif\tools',
    [switch]$CleanConfig,
    [switch]$DeviceTests
)

$ErrorActionPreference = 'Stop'
$mkProject = Split-Path $PSScriptRoot -Parent
$env:IDF_PATH = $IdfPath
$env:IDF_TOOLS_PATH = $ToolsPath
$env:IDF_PYTHON_ENV_PATH = Join-Path $ToolsPath 'python\v5.5.5\venv'
$env:ESP_ROM_ELF_DIR = Join-Path $ToolsPath 'esp-rom-elfs\20241011'
$mkToolDirs = @(
    (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts'),
    (Join-Path $ToolsPath 'cmake\3.30.2\bin'),
    (Join-Path $ToolsPath 'ninja\1.12.1'),
    (Join-Path $ToolsPath 'riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin')
)
foreach ($mkDir in $mkToolDirs) {
    if (-not (Test-Path -LiteralPath $mkDir)) {
        throw "Required tool directory not found: $mkDir"
    }
}
$env:PATH = ($mkToolDirs -join ';') + ';' + $env:PATH
$mkPython = Join-Path $mkToolDirs[0] 'python.exe'
$mkIdf = Join-Path $IdfPath 'tools\idf.py'
$mkBuild = Join-Path $mkProject 'build'
$mkOptions = @('-C', $mkProject, '-DIDF_TARGET=esp32p4', '-DMK_BUILD_TESTS=OFF')
if ($DeviceTests) {
    $mkBuild = Join-Path $mkProject 'build-device-tests'
    $mkOptions = @('-C', $mkProject, '-DIDF_TARGET=esp32p4', '-DMK_BUILD_TESTS=ON')
}
if ($CleanConfig) {
    # A new directory preserves the user's existing build and sdkconfig.
    $mkBuild = Join-Path $mkProject ('build-verify-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
    $mkOptions += '-DSDKCONFIG=' + (Join-Path $mkBuild 'sdkconfig')
}
elseif ($DeviceTests) {
    $mkOptions += '-DSDKCONFIG=' + (Join-Path $mkBuild 'sdkconfig')
}
$mkOptions += @('-B', $mkBuild)
& $mkPython $mkIdf @mkOptions reconfigure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $mkPython $mkIdf @mkOptions build size-components
exit $LASTEXITCODE
