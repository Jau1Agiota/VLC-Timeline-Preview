# GPL-2.0-or-later. Private verified runtime, strictly new output.
param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'Get-Runtime.ps1') -Destination $Destination
foreach($name in @('mpv-thumbnail-worker.exe','mpv-thumbnail-accurate-worker.exe')){
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $Destination $name) -ErrorAction Stop
}
