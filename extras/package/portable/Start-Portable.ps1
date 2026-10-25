# SPDX-License-Identifier: GPL-2.0-or-later
# Invoked by the double-click launcher. No machine configuration or elevation.
param([ValidateSet('exact','fast')][string]$SeekMode='exact',
      [switch]$Diagnostics, [switch]$PlaybackDiagnostics, [switch]$ValidateOnly, [switch]$SetupOnly)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
$app=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$gate=$null
$temp=$null
try {
    $data=Join-Path $app 'portable-data'
    # Fail before download when the extracted folder is not writable.
    [IO.Directory]::CreateDirectory($data) | Out-Null
    $gate=[IO.File]::Open((Join-Path $data 'launch.lock'),'OpenOrCreate','ReadWrite','None')
    $mpv=Join-Path $app 'thumbnail-mpv'
    foreach($file in @('vlc.exe','libvlc.dll','libvlccore.dll','thumbnail-mpv\mpv-thumbnail-worker.exe','thumbnail-mpv\mpv-thumbnail-accurate-worker.exe','thumbnail-mpv\dependencies.json')) {
        if(-not [IO.File]::Exists((Join-Path $app $file))){throw "Missing package dependency: $file. Extract the complete ZIP again into a new folder."}
    }
    $lock=Get-Content -LiteralPath (Join-Path $mpv 'dependencies.json') -Raw | ConvertFrom-Json
    $runtime=Join-Path $data 'runtime'
    $dll=Join-Path $runtime 'libmpv-2.dll'
    if(-not [IO.File]::Exists($dll)) {
        if($ValidateOnly){throw 'Verified runtime is not installed.'}
        if(-not $SetupOnly){[Windows.Forms.MessageBox]::Show('First launch downloads the pinned libmpv runtime (internet required). Please wait after clicking OK. It is verified and kept inside this portable folder. No administrator access is needed.','VLC Timeline Preview setup','OK','Information') | Out-Null}
        [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
        $temp=Join-Path $data ('setup-'+[guid]::NewGuid().ToString('N'))
        [IO.Directory]::CreateDirectory($temp) | Out-Null
        $archive=Join-Path $temp 'runtime.7z'
        $web=New-Object Net.WebClient
        try {
            $download=$web.DownloadFileTaskAsync([uri]$lock.url,$archive)
            if(-not $download.Wait(300000)){$web.CancelAsync(); throw 'Runtime download timed out after five minutes. Please retry later.'}
        } finally {$web.Dispose()}
        if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $lock.sha256){throw 'Downloaded runtime archive checksum mismatch. No runtime was installed.'}
        # Windows 10 1803+ inbox bsdtar/libarchive supports 7z; no extra codec pack.
        $tar=Join-Path $env:SystemRoot 'System32\tar.exe'
        if(-not [IO.File]::Exists($tar)){throw 'Windows inbox tar.exe is missing. Use Windows 10 1803 or newer / Windows 11.'}
        & $tar -xf $archive -C $temp libmpv-2.dll 2> (Join-Path $temp 'extract.log')
        if($LASTEXITCODE -ne 0){throw 'Windows tar could not extract the pinned runtime. Runtime was not installed.'}
        $candidate=Join-Path $temp 'libmpv-2.dll'
        if((Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash.ToLowerInvariant() -ne $lock.dll_sha256){throw 'Extracted runtime checksum mismatch.'}
        [IO.Directory]::CreateDirectory($runtime) | Out-Null
        # No replacement of an existing DLL. A concurrent launcher cannot hold launch.lock.
        [IO.File]::Move($candidate,$dll)
    }
    if((Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant() -ne $lock.dll_sha256){throw 'Cached libmpv checksum mismatch. Preserve the folder for investigation; use a fresh extraction.'}
    foreach($name in @('mpv-thumbnail-worker.exe','mpv-thumbnail-accurate-worker.exe')) {
        $source=Join-Path $mpv $name
        $target=Join-Path $runtime $name
        if(-not [IO.File]::Exists($target)){[IO.File]::Copy($source,$target,$false)}
        if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash){throw 'Cached worker differs from this package. Use a fresh extraction.'}
    }
    # Detect missing transitive runtime imports (notably the GPU driver's Vulkan loader).
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class PortableLibrary {
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 public static extern IntPtr LoadLibraryEx(string path, IntPtr file, uint flags);
 [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr module);
}
'@
    $module=[PortableLibrary]::LoadLibraryEx($dll,[IntPtr]::Zero,0x00001100)
    if($module -eq [IntPtr]::Zero){throw "Cannot load verified libmpv (Windows error $([Runtime.InteropServices.Marshal]::GetLastWin32Error())). Install/update the official GPU driver if vulkan-1.dll is unavailable. Do not download loose DLLs."}
    [PortableLibrary]::FreeLibrary($module) | Out-Null
    if($ValidateOnly -or $SetupOnly){return}
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName=Join-Path $app 'vlc.exe'
    $info.WorkingDirectory=$app
    $info.UseShellExecute=$false
    $info.EnvironmentVariables['VLC_THUMBNAIL_BACKEND']='libmpv'
    $info.EnvironmentVariables['VLC_MPV_SEEK']=$SeekMode
    $worker=if($SeekMode -eq 'exact'){'mpv-thumbnail-accurate-worker.exe'}else{'mpv-thumbnail-worker.exe'}
    $info.EnvironmentVariables['VLC_MPV_HELPER']=Join-Path $runtime $worker
    $info.EnvironmentVariables['VLC_TIMELINE_HOVER_DEBUG']=$(if($Diagnostics){'1'}else{'0'})
    $info.EnvironmentVariables['VLC_PLAYBACK_STABILITY_DEBUG']=$(if($PlaybackDiagnostics){'1'}else{'0'})
    # Child-only profile/cache isolation; never alter the user's normal VLC profile.
    $info.EnvironmentVariables['APPDATA']=Join-Path $data 'config'
    $info.EnvironmentVariables['LOCALAPPDATA']=Join-Path $data 'cache'
    $info.EnvironmentVariables['TEMP']=$data
    $info.EnvironmentVariables['TMP']=$data
    foreach($folder in @('config','cache')){[IO.Directory]::CreateDirectory((Join-Path $data $folder)) | Out-Null}
    $info.Arguments='--no-one-instance --no-qt-privacy-ask --no-qt-updates-notif --verbose=0'
    if($Diagnostics -or $PlaybackDiagnostics){$info.Arguments+=' --verbose=2 --file-logging --logfile="'+(Join-Path $data ('diagnostic-'+[guid]::NewGuid().ToString('N')+'.log'))+'"'}
    [Diagnostics.Process]::Start($info) | Out-Null
} catch {
    if($ValidateOnly -or $SetupOnly){throw}
    [Windows.Forms.MessageBox]::Show($_.Exception.Message,'VLC Timeline Preview could not start','OK','Error') | Out-Null
    exit 1
} finally {
    if($temp -and [IO.Directory]::Exists($temp)){Remove-Item -LiteralPath $temp -Recurse -Force}
    if($gate){$gate.Dispose()}
}
