param(
    [int]$Timesteps = 1000,
    [int]$Trials = 5,
    [string]$OutputRoot = "results/dormitory"
)

$ErrorActionPreference = "Stop"
$sharedRunner = Join-Path $PSScriptRoot "run_w400_w500_k100_validation_windows.ps1"
& $sharedRunner -Timesteps $Timesteps -Trials $Trials -Warehouses @(200, 300) -OutputRoot $OutputRoot
