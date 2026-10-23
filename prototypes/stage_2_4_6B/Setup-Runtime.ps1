# GPL-2.0-or-later. Private checksum-verified runtime; output must be new.
param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'Get-Runtime.ps1') -Destination $Destination
if(-not (Test-Path -LiteralPath (Join-Path $Destination 'libmpv-2.dll'))){throw 'Runtime unavailable'}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'mpv-thumbnail-worker.exe') -Destination (Join-Path $Destination 'mpv-thumbnail-worker.exe') -ErrorAction Stop
