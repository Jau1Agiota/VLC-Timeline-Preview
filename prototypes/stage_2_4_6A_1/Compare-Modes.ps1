# GPL-2.0-or-later. Same-file paced comparison; creates only new result folders.
param(
    [Parameter(Mandatory=$true)][string]$Media,
    [Parameter(Mandatory=$true)][string]$Runtime,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [ValidateSet('gl','sw')][string]$Renderer='gl',
    [ValidateSet('no','auto-copy')][string]$Hardware='no',
    [ValidateSet('keyframes','exact')][string]$Seek='keyframes',
    [string]$LegacyExe=''
)
$ErrorActionPreference='Stop'
if (Test-Path -LiteralPath $OutputRoot) { throw 'OutputRoot must be new.' }
$root=(New-Item -ItemType Directory $OutputRoot).FullName
$gpu=$null
$vendor=Get-Command nvidia-smi -ErrorAction SilentlyContinue
try {
    if ($vendor) {
        $gpu=Start-Process -FilePath $vendor.Source -ArgumentList @('--query-gpu=timestamp,utilization.gpu,utilization.decoder,memory.used','--format=csv','-l','1') -PassThru -NoNewWindow -RedirectStandardOutput "$root/gpu.csv" -RedirectStandardError "$root/gpu-stderr.txt"
    }
    foreach ($mode in @('visual','timing','capture')) {
        py (Join-Path $PSScriptRoot 'replay.py') --exe (Join-Path $PSScriptRoot 'mpv-preview.exe') --runtime $Runtime --media $Media --output "$root/$mode" --renderer $Renderer --hardware $Hardware --seek $Seek --mode $mode
        if ($LASTEXITCODE -ne 0) { throw "Replay failed in $mode; preserve its logs." }
    }
    if ($LegacyExe) {
        py (Join-Path $PSScriptRoot 'replay.py') --exe $LegacyExe --runtime $Runtime --media $Media --output "$root/legacy" --renderer $Renderer --hardware $Hardware --seek $Seek --mode capture --legacy
        if ($LASTEXITCODE -ne 0) { throw 'Legacy replay failed; preserve its logs.' }
    }
} finally {
    if ($gpu) {
        $cleanupError=$null
        try { if (-not $gpu.HasExited) { $gpu.Kill(); if (-not $gpu.WaitForExit(2000)) { $cleanupError='Sampler did not exit within 2 seconds' } } }
        catch { $cleanupError=$_.Exception.Message; Write-Warning "GPU sampler cleanup: $cleanupError" }
        $exitCode=$null
        if ($gpu.HasExited) { $exitCode=$gpu.ExitCode }
        @{available=$true;exit_code=$exitCode;cleanup_error=$cleanupError;scope='Whole GPU, not process-specific; unsupported fields may be N/A/error. Collection stopped by controller.'} | ConvertTo-Json | Set-Content "$root/gpu-status.json"
        $gpu.Dispose()
    } else {
        @{available=$false;reason='nvidia-smi not available; GPU usage unmeasured'} | ConvertTo-Json | Set-Content "$root/gpu-status.json"
    }
}
