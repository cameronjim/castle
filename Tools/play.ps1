# Launches the Castle game standalone (no editor UI) for fast iteration on feel.
# Refuses to start if an editor instance is already running - this machine has 16 GB RAM
# and editor + standalone game at once will thrash.
# Usage: .\Tools\play.ps1 [-Map /Game/Maps/L_M01_CellBlockD] [-Fullscreen]
#   -Map         play a different map instead of the mission default (L_M01_CellBlockD)
#   -Fullscreen  launch fullscreen instead of windowed at 1600x900
param(
    [string]$Map = "/Game/Maps/L_M01_CellBlockD",
    [switch]$Fullscreen,
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.8"
)

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Proj = Join-Path $Root "Castle.uproject"
$Exe = Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor.exe"

if (-not (Test-Path $Exe)) { Write-Error "UnrealEditor.exe not found at $Exe"; exit 2 }
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    Write-Warning "An editor instance is already running. This machine has 16 GB RAM; not starting a second one."
    exit 1
}

$ArgList = @("`"$Proj`"", $Map, "-game")
if ($Fullscreen) {
    $ArgList += @("-fullscreen", "-ResX=1920", "-ResY=1080", "-log")
} else {
    $ArgList += @("-windowed", "-ResX=1600", "-ResY=900", "-log")
}

Start-Process -FilePath $Exe -ArgumentList $ArgList -WorkingDirectory $Root
Write-Host "Launching Castle standalone on $Proj ($Map)"
