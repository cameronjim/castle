# Creates the stage 1-2 starter content (input assets, blueprints, placeholder textures,
# mission/flashback data assets, sandbox + Cell Block D maps) by running
# Tools\Editor\create_all.py inside a headless editor.
#
# Idempotent: every script checks for the asset before creating it, so re-running is safe.
# Do not run this while another editor instance is open - they collide on the module DLL.
#
#   .\Tools\create-content.ps1
#   .\Tools\create-content.ps1 -Bright   # brighter room exposure, for playtesting layout and AI

[CmdletBinding()]
param(
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.8",
    [string]$Project = "",
    [switch]$Bright
)

$ErrorActionPreference = "Stop"

# $PSScriptRoot is empty inside a param() default when invoked via -File, so resolve here.
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Resolve-Path (Join-Path $ScriptDir "..")
if (-not $Project) { $Project = Join-Path $ProjectRoot "Castle.uproject" }
$Project = (Resolve-Path $Project).Path
$EditorCmd = Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Script = Join-Path $ProjectRoot "Tools\Editor\create_all.py"
$LogDir = Join-Path $ProjectRoot "Saved\Logs"
$LogFile = Join-Path $LogDir "CastleContent.log"

if (-not (Test-Path $EditorCmd)) { throw "Headless editor not found: $EditorCmd" }
if (-not (Test-Path $Script))    { throw "Python entry point not found: $Script" }
if (-not (Test-Path $LogDir))    { New-Item -ItemType Directory -Path $LogDir | Out-Null }
if (Test-Path $LogFile)          { Remove-Item $LogFile -Force }

Write-Host "Running $Script through $EditorCmd ..."
if ($Bright) { Write-Host "Bright preset requested (CASTLE_BRIGHT=1)." -ForegroundColor Yellow }

if ($Bright) { $env:CASTLE_BRIGHT = "1" }
try {
    & $EditorCmd $Project `
        -run=pythonscript `
        "-script=$Script" `
        -unattended `
        -nullrhi `
        -nosplash `
        -nop4 `
        -stdout `
        -FullStdOutLogOutput `
        -NoLogTimes `
        "-abslog=$LogFile"

    $editorExit = $LASTEXITCODE
} finally {
    if ($Bright) { Remove-Item Env:\CASTLE_BRIGHT -ErrorAction SilentlyContinue }
}

if (-not (Test-Path $LogFile)) {
    Write-Host "No log at $LogFile - the editor produced nothing to inspect." -ForegroundColor Red
    exit 1
}

$pythonLines = Select-String -Path $LogFile -Pattern "LogPython" | ForEach-Object { $_.Line }

Write-Host ""
Write-Host "---- LogPython ----"
if ($pythonLines) {
    $pythonLines | ForEach-Object { Write-Host $_ }
} else {
    Write-Host "(no LogPython output)"
}
Write-Host "-------------------"

$bad = $pythonLines | Where-Object { $_ -match "Error|Traceback" }

if ($bad) {
    Write-Host ""
    Write-Host "Content creation reported errors:" -ForegroundColor Red
    $bad | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    Write-Host "Full log: $LogFile"
    exit 1
}

if ($editorExit -ne 0) {
    Write-Host "Editor exited with code $editorExit. Full log: $LogFile" -ForegroundColor Red
    exit $editorExit
}

Write-Host ""
Write-Host "Content creation finished clean. Full log: $LogFile" -ForegroundColor Green
exit 0
