# SPDX-License-Identifier: GPL-2.0-or-later
# Real Windows inbox PowerShell/tar setup + native bootstrap from path with spaces.
param([Parameter(Mandatory=$true)][string]$Application)
$ErrorActionPreference='Stop'
$app=(Resolve-Path -LiteralPath $Application).Path
$ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$script=Join-Path $app 'portable\Start-Portable.ps1'
function Run-Setup([string]$Mode){
    Write-Output "::notice title=Portable setup phase::$Mode"
    $details=(& $ps -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $script $Mode 2>&1 | Out-String)
    if($LASTEXITCODE -ne 0){
        $safe=$details.Replace('%','%25').Replace("`r",'%0D').Replace("`n",'%0A')
        Write-Output "::error title=Portable setup failure::$safe"
        throw "Portable setup failed: $Mode"
    }
}
Run-Setup '-SetupOnly'
$dll=Join-Path $app 'portable-data\runtime\libmpv-2.dll'
$before=(Get-Item -LiteralPath $dll).LastWriteTimeUtc
Run-Setup '-ValidateOnly'
if((Get-Item -LiteralPath $dll).LastWriteTimeUtc -ne $before){throw 'Cached runtime was replaced'}
# Deliberately broken cache fails closed, without setup/download or messagebox.
$worker=Join-Path $app 'portable-data\runtime\mpv-thumbnail-accurate-worker.exe'
$backup=[IO.File]::ReadAllBytes($worker)
try {
    [IO.File]::WriteAllText($worker,'invalid test worker')
    & $ps -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $script -ValidateOnly 2>$null
    if($LASTEXITCODE -eq 0){throw 'Mismatched worker accepted'}
} finally {[IO.File]::WriteAllBytes($worker,$backup)}
Run-Setup '-ValidateOnly'
& $ps -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $script -ValidateOnly -StageWorkerLocally
if($LASTEXITCODE -ne 0){throw 'Fixed-drive staging validation failed'}
Write-Output '::notice title=Portable setup phase::Native launcher cached startup'
$expected=Join-Path $app 'vlc.exe'
$existing=@(Get-Process vlc -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
$bootstrap=Start-Process -FilePath (Join-Path $app 'Start VLC Timeline Preview.exe') -PassThru
$child=$null
try {
    for($i=0;$i -lt 60;$i++) {
        $child=Get-Process vlc -ErrorAction SilentlyContinue | Where-Object {$_.Id -notin $existing -and $_.Path -eq $expected} | Select-Object -First 1
        if($child){break}
        Start-Sleep -Milliseconds 500
    }
    if(-not $child){
        $state=if($bootstrap.HasExited){"exit=$($bootstrap.ExitCode)"}else{"still-running"}
        Write-Output "::error title=Native bootstrap state::pid=$($bootstrap.Id) $state"
        # Close only this bootstrap's PowerShell child, which may be awaiting an
        # error dialog and holding setup.lock. Never target other VLC/PS instances.
        Get-CimInstance Win32_Process -Filter "ParentProcessId=$($bootstrap.Id)" | Where-Object {
            $_.Name -eq 'powershell.exe' -and $_.CommandLine.Contains($script)
        } | ForEach-Object {Stop-Process -Id $_.ProcessId -Force}
        foreach($log in @(Get-ChildItem -LiteralPath (Join-Path $app 'portable-data') -Filter 'startup-error-*.log')){
            $safe=([IO.File]::ReadAllText($log.FullName)).Replace('%','%25').Replace("`r",'%0D').Replace("`n",'%0A')
            Write-Output "::error title=Portable startup error::$safe"
        }
        foreach($v in @(Get-Process vlc -ErrorAction SilentlyContinue)){
            Write-Output "::notice title=VLC process observation::pid=$($v.Id) path=$($v.Path)"
        }
        throw 'Native portable launcher did not start packaged VLC'
    }
    Start-Sleep -Seconds 3
    if($child.HasExited){throw 'Packaged VLC exited unexpectedly'}
    $command=(Get-CimInstance Win32_Process -Filter "ProcessId=$($child.Id)").CommandLine
    if($command -notmatch '--no-one-instance'){throw 'Isolation argument missing'}
    Write-Output '::notice title=Stage F portable smoke::Inbox PowerShell/tar setup, cached validation, rejected corrupt worker and native launcher in spaced path passed. GUI/thumbnail acceptance not established.'
} finally {
    if($child -and -not $child.HasExited){$child.CloseMainWindow() | Out-Null; if(-not $child.WaitForExit(5000)){$child.Kill();$child.WaitForExit(5000) | Out-Null}}
}
