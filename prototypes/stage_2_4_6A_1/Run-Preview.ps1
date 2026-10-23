# GPL-2.0-or-later. Private runtime selected explicitly; no system installation.
param(
    [Parameter(Mandatory=$true)][string]$Media,
    [Parameter(Mandatory=$true)][string]$Results,
    [Parameter(Mandatory=$true)][string]$Runtime,
    [ValidateSet('gl','sw')][string]$Renderer='gl',
    [ValidateSet('no','auto-copy')][string]$Hardware='no',
    [ValidateSet('keyframes','exact')][string]$Seek='keyframes',
    [ValidateSet('visual','timing','capture')][string]$Mode='visual'
)
$ErrorActionPreference='Stop'
$runtimeDirectory=(Resolve-Path -LiteralPath $Runtime).Path
$mediaFile=(Resolve-Path -LiteralPath $Media).Path
$lock=Get-Content (Join-Path $PSScriptRoot 'dependencies.json') -Raw | ConvertFrom-Json
$dll=Join-Path $runtimeDirectory 'libmpv-2.dll'
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $dll).Hash.ToLowerInvariant() -ne $lock.dll_sha256) { throw 'Wrong runtime DLL.' }
if (Test-Path -LiteralPath $Results) { throw 'Results directory must be new.' }
$resultsPath=[IO.Path]::GetFullPath($Results)
$oldPath=$env:PATH
Push-Location $PSScriptRoot
try {
    $env:PATH="$runtimeDirectory;$oldPath"
    & (Join-Path $PSScriptRoot 'mpv-preview.exe') $mediaFile $resultsPath $Renderer $Hardware $Seek $Mode
    if ($LASTEXITCODE -ne 0) { throw "Preview exited $LASTEXITCODE; inspect results and dependency errors." }
} finally { $env:PATH=$oldPath; Pop-Location }
