# GPL-2.0-or-later. Launch this isolated VLC only; environment changes are child-scoped.
param([ValidateSet('libvlc','libmpv')][string]$Backend='libvlc',
      [string]$Runtime, [string]$Media, [switch]$Diagnostics)
$ErrorActionPreference='Stop'
$app=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$info=New-Object System.Diagnostics.ProcessStartInfo
$info.FileName=Join-Path $app 'vlc.exe'
$info.WorkingDirectory=$app
$info.UseShellExecute=$false
$info.EnvironmentVariables['VLC_THUMBNAIL_BACKEND']=$Backend
if($Backend -eq 'libmpv') {
    if(-not $Runtime){throw 'Specify a checksum-verified private -Runtime folder (see Setup-Runtime.ps1).'}
    $runtimePath=(Resolve-Path -LiteralPath $Runtime).Path
    $lock=Get-Content (Join-Path $PSScriptRoot 'dependencies.json') -Raw | ConvertFrom-Json
    $dll=Join-Path $runtimePath 'libmpv-2.dll'
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $dll).Hash.ToLowerInvariant() -ne $lock.dll_sha256){throw 'libmpv checksum mismatch'}
    $worker=Join-Path $runtimePath 'mpv-thumbnail-worker.exe'
    $bundled=Join-Path $PSScriptRoot 'mpv-thumbnail-worker.exe'
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $worker).Hash -ne (Get-FileHash -Algorithm SHA256 -LiteralPath $bundled).Hash){throw 'Worker differs from this package; use a new runtime folder.'}
    $info.EnvironmentVariables['VLC_MPV_HELPER']=$worker
}
$info.EnvironmentVariables['VLC_TIMELINE_HOVER_DEBUG']=$(if($Diagnostics){'1'}else{'0'})
# Quoting Windows argv: media is a file, never switches or shell code.
$info.Arguments='--no-one-instance --verbose=2'
if($Diagnostics){$info.Arguments+=' --file-logging --logfile="'+(Join-Path $env:TEMP ('vlc246B-'+[guid]::NewGuid().ToString()+'.log'))+'"'}
if($Media){
    $path=(Resolve-Path -LiteralPath $Media).Path
    if($path.Contains('"')){throw 'Invalid media path'}
    $info.Arguments+=' -- "'+$path+'"'
}
$child=[System.Diagnostics.Process]::Start($info)
Write-Host "Started isolated VLC PID $($child.Id), backend $Backend. GUI/accuracy acceptance remains pending."
