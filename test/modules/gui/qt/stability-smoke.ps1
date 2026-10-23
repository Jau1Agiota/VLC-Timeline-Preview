# GPL-2.0-or-later. Windows runner Qt/dummy-output launch, NOT physical GUI acceptance.
param([Parameter(Mandatory=$true)][string]$Application,
      [Parameter(Mandatory=$true)][string]$Media,
      [Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Smoke output must be new'}
$exe=(Resolve-Path -LiteralPath $Application).Path
$inputFile=(Resolve-Path -LiteralPath $Media).Path
if($exe.Contains('"') -or $inputFile.Contains('"') -or $Output.Contains('"')){throw 'Invalid path'}
$directory=(New-Item -ItemType Directory -Path $Output).FullName
$rows=@()
foreach($enabled in @($false,$true)) {
    $name=if($enabled){'diagnostics-on'}else{'diagnostics-off'}
    $log=Join-Path $directory ($name+'.log')
    $info=New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName=$exe
    $info.WorkingDirectory=Split-Path $exe
    $info.UseShellExecute=$false
    $info.EnvironmentVariables['VLC_PLAYBACK_STABILITY_DEBUG']=$(if($enabled){'1'}else{'0'})
    $info.EnvironmentVariables['VLC_TIMELINE_HOVER_DEBUG']='0'
    $info.EnvironmentVariables['VLC_THUMBNAIL_BACKEND']='libvlc'
    # All options verified in VLC3.0.24; dummy A/V excludes hardware acceptance.
    $info.Arguments='--no-one-instance --intf=qt --qt-start-minimized --no-qt-system-tray --no-qt-privacy-ask --no-qt-updates-notif --vout=dummy --aout=dummy --stats --play-and-exit --verbose=2 --file-logging --logfile="'+$log+'" -- "'+$inputFile+'"'
    $child=[System.Diagnostics.Process]::Start($info)
    try {
        if(-not $child.WaitForExit(45000)){throw "$name Qt launch timeout"}
        if($child.ExitCode -ne 0){throw "$name Qt exit $($child.ExitCode)"}
        if(-not (Test-Path -LiteralPath $log)){throw "$name missing log"}
        $text=Get-Content -LiteralPath $log -Raw
        $heartbeats=@([regex]::Matches($text,'playback-stability heartbeat')).Count
        if($enabled -and ($heartbeats -lt 2 -or $text -notmatch 'playback-stability enabled')){throw 'Modified Qt diagnostic heartbeat not observed'}
        if(-not $enabled -and $text -match 'playback-stability'){throw 'Foreground diagnostics were not quiet by default'}
        $rows+=@{mode=$name;exit_code=$child.ExitCode;heartbeats=$heartbeats;physical_gui_verified=$false;dummy_outputs=$true}
    }
    finally {
        if(-not $child.HasExited){$child.Kill();[void]$child.WaitForExit(5000)}
        $child.Dispose()
    }
}
$summary=$rows | ConvertTo-Json -Depth 4
$summary | Set-Content -LiteralPath (Join-Path $directory 'results.json') -Encoding utf8
Write-Output ('::notice title=Stage 2.4.6D Qt launch::'+($rows | ConvertTo-Json -Compress -Depth 4))
