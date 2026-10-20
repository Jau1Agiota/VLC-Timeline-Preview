# GPL-2.0-or-later. Windows PowerShell 5.1 or PowerShell 7.
# Collect candidates only; visible frame timestamps must be reviewed separately.
param(
    [Parameter(Mandatory=$true)][string]$Helper,
    [Parameter(Mandatory=$true)][string]$Fixtures,
    [Parameter(Mandatory=$true)][string]$Results
)
$ErrorActionPreference = 'Stop'
$Helper = (Resolve-Path $Helper).Path
$Fixtures = (Resolve-Path $Fixtures).Path
if (Test-Path $Results) { throw 'Results directory must be new.' }
$Results = (New-Item -ItemType Directory -Path $Results).FullName
$rows = @()
$manifest = Get-Content (Join-Path $Fixtures 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.duration_s -ne 30) { throw 'This matrix requires default 30-second fixtures.' }
foreach ($fixture in $manifest.fixtures) {
    $inputFile = Join-Path $Fixtures $fixture.file
    $actualHash = (Get-FileHash -Algorithm SHA256 $inputFile).Hash.ToLowerInvariant()
    if ($actualHash -ne $fixture.sha256) { throw "Fixture hash mismatch: $inputFile" }
    foreach ($target in @(0, 80, 1000, 14750, 28800, 29500, 29950)) {
        foreach ($repeat in @(1, 2, 3)) {
            $stem = "$($fixture.file)-$target-$repeat"
            $image = Join-Path $Results "$stem.bmp"
            $stdout = Join-Path $Results "$stem.json"
            $stderr = Join-Path $Results "$stem.stderr.txt"
            $watch = [Diagnostics.Stopwatch]::StartNew()
            # ProcessStartInfo avoids PowerShell/native argument quoting surprises.
            # Windows file paths cannot contain double quotes; these paths are
            # files, not directory arguments ending in a backslash.
            $info = New-Object Diagnostics.ProcessStartInfo
            $info.FileName = $Helper
            $info.Arguments = '"{0}" {1} "{2}" 10000' -f $inputFile, $target, $image
            $info.UseShellExecute = $false
            $info.CreateNoWindow = $true
            $info.RedirectStandardOutput = $true
            $info.RedirectStandardError = $true
            $process = New-Object Diagnostics.Process
            $process.StartInfo = $info
            if (-not $process.Start()) { throw 'Helper launch failed.' }
            $outTask = $process.StandardOutput.ReadToEndAsync()
            $errTask = $process.StandardError.ReadToEndAsync()
            $externalTimeout = -not $process.WaitForExit(15000)
            if ($externalTimeout) { $process.Kill(); $process.WaitForExit() }
            $outTask.Result | Set-Content -Encoding UTF8 $stdout
            $errTask.Result | Set-Content -Encoding UTF8 $stderr
            $watch.Stop()
            $exitCode = $process.ExitCode
            $process.Dispose()
            $record = $null
            if (-not $externalTimeout -and $exitCode -eq 0) {
                try { $record = Get-Content $stdout -Raw | ConvertFrom-Json }
                catch { throw "Invalid helper JSON: $stdout" }
                if ($record.status -ne 'candidate_captured' -or -not (Test-Path $image)) {
                    throw "Candidate output missing: $stem"
                }
            }
            $rows += [pscustomobject]@{
                fixture=$fixture.file; requested_ms=$target; repeat=$repeat
                exit_code=$exitCode; external_timeout=$externalTimeout
                wall_latency_ms=$watch.ElapsedMilliseconds
                capture_latency_ms=$(if ($record) { $record.capture_latency_ms } else { '' })
                image="$stem.bmp"; observed_ms=''; observation_method=''
                visible_window='unreviewed'; audible_output='unreviewed'
                notes=''
            }
            $rows | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $Results 'measurements.csv')
        }
    }
}
Write-Host 'Candidates collected. Fill observed_ms from the burned PTS in each BMP; do not use player clocks.'
