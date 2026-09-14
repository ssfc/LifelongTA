param(
    [string]$Map = "",
    [string]$Json = "",
    [switch]$Wait
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $root "build\highway-editor-qt.exe"
if (!(Test-Path $exe)) {
    $exe = Join-Path $root "build-vs2022\Release\highway-editor-qt.exe"
}
if (!(Test-Path $exe)) {
    $exe = Join-Path $root "build-vs18-release\highway-editor-qt.exe"
}
if (!(Test-Path $exe)) {
    $exe = Join-Path $root "build-vs18-release\Release\highway-editor-qt.exe"
}

if (!(Test-Path $exe)) {
    throw "Executable not found: $exe"
}

$exeDir = Split-Path -Parent $exe
$pluginDir = $exeDir
if (!(Test-Path (Join-Path $pluginDir "platforms"))) {
    $parentDir = Split-Path -Parent $exeDir
    if (Test-Path (Join-Path $parentDir "platforms")) {
        $pluginDir = $parentDir
    }
}
if (!(Test-Path (Join-Path $pluginDir "platforms"))) {
    $vcpkgPluginDir = "C:\vcpkg\installed\x64-windows\Qt6\plugins"
    if (Test-Path (Join-Path $vcpkgPluginDir "platforms")) {
        $pluginDir = $vcpkgPluginDir
    }
}

$env:PATH = "$exeDir;$pluginDir;C:\vcpkg\installed\x64-windows\bin;$env:PATH"
$env:QT_PLUGIN_PATH = $pluginDir
$env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $pluginDir "platforms"
$env:QT_QPA_PLATFORM = "windows"

$args = @()
if ($Map) {
    $args += @("--map", $Map)
}
if ($Json) {
    $args += @("--json", $Json)
}

$process = Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $exeDir -WindowStyle Normal -PassThru
if ($Wait) {
    $process.WaitForExit()
    Write-Host "highway-editor-qt exited with code $($process.ExitCode)"
} else {
    Start-Sleep -Seconds 2
    $process.Refresh()
    if ($process.HasExited) {
        Write-Host "highway-editor-qt exited immediately with code $($process.ExitCode)"
    } else {
        Write-Host "highway-editor-qt running pid=$($process.Id)"
    }
}
