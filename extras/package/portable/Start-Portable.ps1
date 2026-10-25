# SPDX-License-Identifier: GPL-2.0-or-later
# Invoked by the double-click launcher. No machine configuration or elevation.
param([ValidateSet('exact','fast')][string]$SeekMode='exact',
      [switch]$Diagnostics, [switch]$PlaybackDiagnostics, [switch]$ValidateOnly, [switch]$SetupOnly, [switch]$StageWorkerLocally)
$ErrorActionPreference='Stop'
# Explorer, cmd, and PowerShell 7 supply different PSModulePath values.
# Get-FileHash is a script function in Windows PowerShell's built-in utility
# module; import the OS copy explicitly rather than discover a caller's module.
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
Add-Type -AssemblyName System.Windows.Forms
$app=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
# Startup-only loader probe: no GPU enumeration, Vulkan calls, installation or network.
function Initialize-PortableLibrary {
    if('PortableLibrary' -as [type]){return}
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class PortableLibrary {
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 public static extern IntPtr LoadLibraryEx(string path, IntPtr file, uint flags);
 public static int LastError() { return Marshal.GetLastWin32Error(); }
 [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr module);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern uint GetDriveType(string root);
}
'@
}
function Assert-PortableVulkanLoader([string]$RuntimeDirectory) {
    Initialize-PortableLibrary
    # Match the existing libmpv LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | DEFAULT_DIRS
    # policy, including an already present runtime-local loader. Never search CWD/PATH.
    $localLoader=Join-Path $RuntimeDirectory 'vulkan-1.dll'
    $path='vulkan-1.dll'
    $flags=0x00001000
    if([IO.File]::Exists($localLoader)){$path=[IO.Path]::GetFullPath($localLoader);$flags=0x00001100}
    $loader=[PortableLibrary]::LoadLibraryEx($path,[IntPtr]::Zero,$flags)
    $loadError=[PortableLibrary]::LastError()
    if($loader -eq [IntPtr]::Zero){
        $detail=if($loadError -eq 193){'The loader must be compatible with a Windows x64 process.'}else{'The loader is unavailable or one of its own dependencies could not be loaded.'}
        throw "The original preview runtime requires the Windows x64 Vulkan loader (vulkan-1.dll). $detail Windows error $loadError. Compatible, up-to-date graphics drivers commonly provide it, but availability is not guaranteed. Obtain drivers only from your PC or graphics vendor; do not download loose DLLs. This check does not require a Vulkan-capable GPU and does not install or download anything."
    }
    [PortableLibrary]::FreeLibrary($loader) | Out-Null
}

$gate=$null
$temp=$null
$localWorker=$null
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
    # Probe only after cached runtime and worker integrity checks.
    Assert-PortableVulkanLoader $runtime
    $module=[PortableLibrary]::LoadLibraryEx($dll,[IntPtr]::Zero,0x00001100)
    if($module -eq [IntPtr]::Zero){throw "Cannot load verified libmpv (Windows error $([Runtime.InteropServices.Marshal]::GetLastWin32Error())). The Vulkan prerequisite check passed, but libmpv or another runtime dependency failed to load. Use a complete, checksum-verified package and report this error; do not download loose DLLs."}
    [PortableLibrary]::FreeLibrary($module) | Out-Null
    $workerRoot=$runtime
    if($StageWorkerLocally -or [PortableLibrary]::GetDriveType([IO.Path]::GetPathRoot($runtime)) -ne 3){
        # Existing controller permits helpers only on DRIVE_FIXED. Do not bypass it.
        $localTemp=[IO.Path]::GetTempPath()
        if([PortableLibrary]::GetDriveType([IO.Path]::GetPathRoot($localTemp)) -ne 3){throw 'Preview helper requires a writable local fixed-drive TEMP folder when running from removable storage.'}
        $localWorker=Join-Path $localTemp ('vlc-preview-portable-'+[guid]::NewGuid().ToString('N'))
        [IO.Directory]::CreateDirectory($localWorker) | Out-Null
        foreach($file in @('libmpv-2.dll','mpv-thumbnail-worker.exe','mpv-thumbnail-accurate-worker.exe')){
            [IO.File]::Copy((Join-Path $runtime $file),(Join-Path $localWorker $file),$false)
            if((Get-FileHash -LiteralPath (Join-Path $localWorker $file) -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath (Join-Path $runtime $file) -Algorithm SHA256).Hash){throw 'Temporary worker copy checksum mismatch.'}
        }
        $workerRoot=$localWorker
    }
    if($ValidateOnly -or $SetupOnly){return}
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName=Join-Path $app 'vlc.exe'
    $info.WorkingDirectory=$app
    $info.UseShellExecute=$false
    $info.EnvironmentVariables['VLC_THUMBNAIL_BACKEND']='libmpv'
    $info.EnvironmentVariables['VLC_MPV_SEEK']=$SeekMode
    $worker=if($SeekMode -eq 'exact'){'mpv-thumbnail-accurate-worker.exe'}else{'mpv-thumbnail-worker.exe'}
    $info.EnvironmentVariables['VLC_MPV_HELPER']=Join-Path $workerRoot $worker
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
    $child=[Diagnostics.Process]::Start($info)
    # For removable storage only, retain the temporary worker while this VLC lives.
    if($gate){$gate.Dispose();$gate=$null}
    if($localWorker){$child.WaitForExit()}
} catch {
    $problem=$_.Exception.GetBaseException()
    try {
        [IO.File]::WriteAllText((Join-Path $data ('startup-error-'+[guid]::NewGuid().ToString('N')+'.log')),$problem.ToString())
    } catch {} # Read-only/missing storage still gets a dialog.
    if($ValidateOnly -or $SetupOnly){throw $problem}
    [Windows.Forms.MessageBox]::Show($problem.Message,'VLC Timeline Preview could not start','OK','Error') | Out-Null
    exit 1
} finally {
    if($temp -and [IO.Directory]::Exists($temp)){Remove-Item -LiteralPath $temp -Recurse -Force}
    if($localWorker -and [IO.Directory]::Exists($localWorker)){
        # The controller may still be reaping a worker at VLC shutdown; bounded retry.
        for($attempt=0;$attempt -lt 10;$attempt++){
            try {Remove-Item -LiteralPath $localWorker -Recurse -Force; break} catch {Start-Sleep -Milliseconds 200}
        }
    }
    if($gate){$gate.Dispose()}
}
