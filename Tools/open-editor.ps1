# Opens the Castle project in Unreal Editor 5.8 directly, bypassing the .uproject file
# association (which needs UnrealVersionSelector and a verb, and breaks easily on Windows 11).
# Usage: .\Tools\open-editor.ps1 [-Map /Game/Maps/L_M01_CellBlockD] [-Game]
#   -Map   open the editor on that map (or, with -Game, play it standalone)
#   -Game  launch as a standalone game window instead of the editor
param(
    [string]$Map = "",
    [switch]$Game,
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

$ArgList = @("`"$Proj`"")
if ($Map) { $ArgList += $Map }
if ($Game) { $ArgList += @("-game", "-windowed", "-ResX=1600", "-ResY=900", "-log") }

Start-Process -FilePath $Exe -ArgumentList $ArgList -WorkingDirectory $Root
Write-Host "Launching Unreal Editor 5.8 on $Proj $(if ($Map) { "($Map)" })"
