param(
    [string]$Map,

    [string]$Plan,

    [string]$Trace,

    [int]$Agents = 0,
    [int]$Start = 0,
    [int]$End = 0,
    [switch]$Wait
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $root "build\planviz-qt.exe"
if (!(Test-Path $exe)) {
    $exe = Join-Path $root "build-vs2022\Release\planviz-qt.exe"
}
if (!(Test-Path $exe)) {
    $exe = Join-Path $root "build-vs18-release\planviz-qt.exe"
}
if (!(Test-Path $exe)) {
    $exe = Join-Path $root "build-vs18-release\Release\planviz-qt.exe"
}
if (!(Test-Path $exe)) {
    $exe = Join-Path $root "build-vs18-safe\planviz-qt.exe"
}
if (!(Test-Path $exe)) {
    $exe = Join-Path $root "build\Release\planviz-qt.exe"
}

if (!(Test-Path $exe)) {
    throw "Executable not found: $exe"
}

Get-Process planviz-qt -ErrorAction SilentlyContinue | Stop-Process -Force

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

$args = if ($Trace) {
    @("--trace", $Trace, "--start", "$Start", "--end", "$End")
} else {
    if (!$Map -or !$Plan) {
        throw "Specify -Trace, or both -Map and -Plan."
    }
    @("--map", $Map, "--plan", $Plan, "--start", "$Start", "--end", "$End")
}
if ($Agents -gt 0) {
    $args += @("--n", "$Agents")
}

$process = Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $exeDir -WindowStyle Normal -PassThru
if ($Wait) {
    $process.WaitForExit()
    Write-Host "planviz-qt exited with code $($process.ExitCode)"
} else {
    Start-Sleep -Seconds 2
    $process.Refresh()
    if ($process.HasExited) {
        Write-Host "planviz-qt exited immediately with code $($process.ExitCode)"
    } else {
        Write-Host "planviz-qt running pid=$($process.Id)"
    }
}
