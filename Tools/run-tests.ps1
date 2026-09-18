# Runs Castle automation tests headless and exits non-zero on any failure.
# Usage: .\Tools\run-tests.ps1 [-Filter Castle.Health] [-Engine "C:\Program Files\Epic Games\UE_5.8"]
param(
    [string]$Filter = "Castle",
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.8"
)

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Proj = Join-Path $Root "Castle.uproject"
$Report = Join-Path $Root "Saved\Automation"
$Cmd = Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if (-not (Test-Path $Cmd)) { Write-Error "UnrealEditor-Cmd.exe not found at $Cmd"; exit 2 }
if (Test-Path $Report) { Remove-Item -Recurse -Force $Report }

$LogFile = Join-Path $Root "Saved\Logs\CastleTests.log"
& $Cmd $Proj -ExecCmds="Automation RunTests $Filter; Quit" -unattended -nullrhi -nosplash -nop4 -stdout -FullStdOutLogOutput -NoLogTimes -ReportExportPath="$Report" -abslog="$LogFile" | Out-Null

if (-not (Test-Path $LogFile)) { Write-Error "No log produced at $LogFile"; exit 2 }

$Lines = Get-Content $LogFile
# UE 5.8 writes Result={Success} / Result={Fail} / Result={Skipped}, not Passed/Failed.
$Passed = @($Lines | Select-String -Pattern "Test Completed\. Result=\{(Success|Passed)\}").Count
$Failed = @($Lines | Select-String -Pattern "Test Completed\. Result=\{(Fail|Failed)\}")
$Skipped = @($Lines | Select-String -Pattern "Test Completed\. Result=\{(Skipped|NotRun)\}").Count

Write-Host ""
Write-Host "Castle tests ($Filter): $Passed passed, $($Failed.Count) failed, $Skipped skipped"
if ($Failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Failures:"
    $Failed | ForEach-Object { Write-Host "  $($_.Line.Trim())" }
    Write-Host ""
    Write-Host "Errors:"
    $Lines | Select-String -Pattern "LogAutomationController: Error|Error: .*Castle" | ForEach-Object { Write-Host "  $($_.Line.Trim())" }
    exit 1
}
if ($Passed -eq 0) {
    Write-Warning "No tests ran. Check the filter '$Filter' and that the Castle module loaded. Log: $LogFile"
    exit 1
}
exit 0
