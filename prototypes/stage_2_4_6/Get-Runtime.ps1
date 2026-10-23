# GPL-2.0-or-later. Fetch the identified SDK for private use; never overwrite.
param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath $Destination) { throw 'Destination must be new.' }
$lock = Get-Content (Join-Path $PSScriptRoot 'dependencies.json') -Raw | ConvertFrom-Json
$seven = Get-Command 7z -ErrorAction SilentlyContinue
if ($seven) { $extractor = $seven.Source }
else { $extractor = Join-Path $env:ProgramFiles '7-Zip\7z.exe' }
if (-not (Test-Path -LiteralPath $extractor)) { throw 'Install official 7-Zip (7-zip.org), or put 7z on PATH.' }
$directory = (New-Item -ItemType Directory -Path $Destination).FullName
$archive = Join-Path $directory $lock.asset
Invoke-WebRequest -Uri $lock.url -OutFile $archive
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant() -ne $lock.sha256) { throw 'SDK archive checksum mismatch.' }
& $extractor x -y "-o$directory" $archive
if ($LASTEXITCODE -ne 0) { throw 'SDK extraction failed.' }
$dll = Join-Path $directory 'libmpv-2.dll'
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $dll).Hash.ToLowerInvariant() -ne $lock.dll_sha256) { throw 'libmpv DLL checksum mismatch.' }
Write-Host "Verified private runtime: $dll"
Write-Host 'This runtime is not part of the experiment ZIP. Do not redistribute it without resolving corresponding-source obligations.'
